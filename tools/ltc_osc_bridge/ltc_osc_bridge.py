#!/usr/bin/env python3
"""
ltc_osc_bridge — SMPTE LTC → OSC Bridge
=========================================
Decodes Linear Timecode (LTC/SMPTE 12M) from a system audio input or a WAV
file and sends the decoded timecode string via OSC UDP.

Usage:
    python ltc_osc_bridge.py --list-devices
    python ltc_osc_bridge.py --target 192.168.1.100 --device 2
    python ltc_osc_bridge.py --wav test.wav --no-osc
    python ltc_osc_bridge.py --test --target 192.168.1.100

Install deps:
    pip install -r requirements.txt
"""

from __future__ import annotations

__version__ = "1.0.0"

import argparse
import json
import sys
import time
import urllib.request
import urllib.error
import wave
import struct
import threading
from collections import deque
from typing import Callable, Optional, List

import numpy as np

# ── Optional deps (gracefully absent) ─────────────────────────────────────────

try:
    from pythonosc.udp_client import SimpleUDPClient as _UDPClient
    _HAS_OSC = True
except ImportError:  # pragma: no cover
    _HAS_OSC = False

try:
    import sounddevice as sd
    _HAS_SD = True
except ImportError:  # pragma: no cover
    _HAS_SD = False

# ── LTC / SMPTE constants ──────────────────────────────────────────────────────

# Sync word: bits 64-79 of each 80-bit LTC frame, transmitted MSB-first.
# Binary: 0011 1111 1111 1101  (0x3FFD)
_SYNC: tuple = (0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1)

# Common SMPTE frame rates for FPS snap detection
_COMMON_FPS = (24.0, 25.0, 29.97, 30.0)

# ── TC string ↔ frame-count helpers ───────────────────────────────────────────

def _tc_to_frames(tc: str, fps: float) -> int:
    """Parse HH:MM:SS:FF (or with ;/,) into an absolute frame count."""
    parts = tc.strip().replace(";", ":").replace(",", ":").split(":")
    if len(parts) != 4:
        raise ValueError(f"Invalid timecode: {tc!r}  (expected HH:MM:SS:FF)")
    h, m, s, f = (int(x) for x in parts)
    fps_r = round(fps)
    return (h * 3600 + m * 60 + s) * fps_r + f


def _frames_to_tc(n: int, fps: float, drop: bool = False) -> str:
    """Convert absolute frame count to HH:MM:SS:FF string."""
    fps_r = round(fps)
    fr = n % fps_r
    ts = n // fps_r
    s  = ts % 60
    tm = ts // 60
    m  = tm % 60
    h  = tm // 60
    sep = ";" if drop else ":"
    return f"{h:02d}:{m:02d}:{s:02d}{sep}{fr:02d}"


# ── BCD helpers ────────────────────────────────────────────────────────────────

def _bcd(bits: List[int], start: int, length: int) -> int:
    """Decode BCD integer from LSB-first bit slice."""
    return sum(bits[start + i] << i for i in range(length))


def _decode_frame(bits: List[int]) -> Optional[tuple]:
    """
    Decode one 80-bit LTC frame.
    Returns (tc_str, drop_frame) or None if invalid.
    """
    if tuple(bits[64:80]) != _SYNC:
        return None
    fr = _bcd(bits, 0, 4) + _bcd(bits,  8, 2) * 10
    df = bool(bits[10])
    sc = _bcd(bits, 16, 4) + _bcd(bits, 24, 3) * 10
    mn = _bcd(bits, 32, 4) + _bcd(bits, 40, 3) * 10
    hr = _bcd(bits, 48, 4) + _bcd(bits, 56, 2) * 10
    # Sanity-check values
    if fr > 29 or sc > 59 or mn > 59 or hr > 23:
        return None
    sep = ";" if df else ":"
    return f"{hr:02d}:{mn:02d}:{sc:02d}{sep}{fr:02d}", df

# ── LTC Decoder ────────────────────────────────────────────────────────────────

class LTCDecoder:
    """
    Streaming biphase-mark LTC decoder.

    Feed mono float32 audio via .feed(). Each decoded frame is delivered to
    the on_frame(tc_str: str, drop_frame: bool, fps: float | None) callback
    from the calling thread.

    Thread-safety: not thread-safe by itself; use a single producer thread.
    """

    #: Number of zero-crossing gaps to collect before bootstrapping half-period.
    #: 300 gaps ≈ 2+ full LTC frames, guaranteeing we capture the sync word's
    #: 26 short gaps even in worst-case (mostly-zero timecode) data.
    _BOOT_N: int = 300

    #: EMA alpha for adaptive half-period tracking (smaller = slower/stabler).
    _HP_ALPHA: float = 0.08

    #: Maximum search window when hunting for sync (bits). Larger = more robust
    #: initial lock, slightly more CPU when misaligned.
    _SYNC_SEARCH_WINDOW: int = 32

    def __init__(self, sample_rate: int = 48000,
                 on_frame: Optional[Callable] = None) -> None:
        self.sample_rate = sample_rate
        self.on_frame = on_frame or (lambda *_: None)

        # Adaptive half-period estimator (samples per half-bit-period)
        self._hp: float = 0.0
        self._boot_gaps: List[float] = []

        # Biphase-mark state machine: 0=IDLE, 1=HALF
        self._bmc_state: int = 0

        # Bit accumulator (plain list for fast appends + slicing)
        self._buf: List[int] = []

        # Absolute sample position of the last detected zero-crossing
        self._last_cross: int = -1

        # Sign of the last sample from the previous feed() call.
        # Used to detect zero-crossings at chunk boundaries.
        self._last_sign: int = 0

        # Total samples consumed (used for FPS timing)
        self._n: int = 0

        # FPS estimation (ring buffer of frame arrival times in samples)
        self._frame_times: deque = deque(maxlen=12)

        # Absolute position of the most-recently processed crossing.
        # Updated in feed() before each _process_gap() call so that
        # _record_frame() can use a precise timestamp instead of self._n.
        self._cur_abs_pos: int = 0

        # Public stats
        self.fps: Optional[float] = None
        self.frame_count: int = 0

    # ── Half-period estimation ─────────────────────────────────────────────────

    def _bootstrap_hp(self) -> None:
        """
        Bootstrap half-period from initial gap collection.

        Gaps form two clusters: short (≈T/2) and long (≈T = 2×T/2).
        We find the minimum observed gap (≈T/2) and treat everything within
        60% of that as the short cluster, then take the median.

        This correctly handles worst-case LTC content (mostly-zero timecodes)
        where short gaps may be a minority of the bootstrap window.
        """
        arr = sorted(g for g in self._boot_gaps if g > 3)
        if not arr:
            return
        # The minimum gap is T/2 (or very close).  T = 2×T/2 so the two
        # clusters are well-separated: short < 1.6×min, long ≈ 2×min.
        min_g = arr[0]
        shorts = [g for g in arr if g <= min_g * 1.6]
        self._hp = float(np.median(shorts)) if shorts else float(min_g)
        self._boot_gaps = []

    def _update_hp(self, short_gap: float) -> None:
        """Exponential moving-average update with a measured short gap."""
        self._hp = (1.0 - self._HP_ALPHA) * self._hp + self._HP_ALPHA * short_gap

    # ── Bit / frame processing ─────────────────────────────────────────────────

    def _push_bit(self, b: int) -> None:
        self._buf.append(b)
        if len(self._buf) >= 80:
            self._search_sync()

    def _search_sync(self) -> None:
        """
        Search the bit buffer for a valid LTC frame.
        Scans up to _SYNC_SEARCH_WINDOW positions from the oldest end to
        lock on quickly while staying O(1) in the steady state.
        """
        buf = self._buf
        blen = len(buf)
        limit = blen - 79  # number of possible frame start positions

        # In steady state (aligned), the frame starts at position 0.
        # We allow a small window for cases where a few extra bits accumulated
        # before the previous decode (noise, startup misalignment).
        for i in range(min(limit, self._SYNC_SEARCH_WINDOW)):
            if tuple(buf[i + 64 : i + 80]) == _SYNC:
                result = _decode_frame(buf[i : i + 80])
                if result is not None:
                    tc, df = result
                    self._on_valid_frame(tc, df)
                    del buf[: i + 80]
                    return

        # No sync found in search window; trim to keep last 79 bits so the
        # next incoming bit can complete a frame without carrying stale data.
        if blen > 200:
            del buf[:-79]

    def _on_valid_frame(self, tc: str, df: bool) -> None:
        self.frame_count += 1
        self._frame_times.append(self._cur_abs_pos)  # precise crossing position
        self._estimate_fps()
        self.on_frame(tc, df, self.fps)

    def _estimate_fps(self) -> None:
        ft = self._frame_times
        if len(ft) < 3:
            return
        intervals = [ft[k] - ft[k - 1] for k in range(1, len(ft))]
        avg = float(np.mean(intervals))
        if avg <= 0:
            return
        raw = self.sample_rate / avg
        best = min(_COMMON_FPS, key=lambda f: abs(raw - f))
        if abs(raw - best) < 1.5:
            self.fps = best

    # ── Gap / BMC logic ────────────────────────────────────────────────────────

    def _process_gap(self, gap: int) -> None:
        """Handle one measured inter-crossing gap (in samples)."""
        # ── Bootstrapping ──────────────────────────────────────────────────────
        if self._hp == 0.0:
            if 2 < gap < 8000:
                self._boot_gaps.append(gap)
            if len(self._boot_gaps) >= self._BOOT_N:
                self._bootstrap_hp()
            return

        # ── Classify gap ───────────────────────────────────────────────────────
        threshold = self._hp * 1.5  # boundary between T/2 and T

        if gap < threshold:
            # ── Short gap (≈ T/2): part of a '1' bit ──────────────────────────
            self._update_hp(gap)
            if self._bmc_state == 0:    # IDLE → HALF (first half of '1')
                self._bmc_state = 1
            else:                       # HALF → emit '1', back to IDLE
                self._push_bit(1)
                self._bmc_state = 0

        elif gap < threshold * 2.5:
            # ── Long gap (≈ T): a '0' bit ─────────────────────────────────────
            if self._bmc_state == 1:    # unexpected long after half → resync
                self._bmc_state = 0
            else:
                self._push_bit(0)

        else:
            # ── Very long gap: dropout / silence ──────────────────────────────
            self._bmc_state = 0

    # ── Public interface ───────────────────────────────────────────────────────

    def feed(self, samples: np.ndarray) -> None:
        """
        Process a 1-D float32 mono numpy array.
        May be called from an audio callback or any single thread.
        """
        if samples.ndim != 1:
            raise ValueError("LTCDecoder.feed() expects a 1-D mono array")

        # Sign array: +1 / -1; treat exact-zero samples as positive (rare in
        # real audio; a 1-sample timing offset at T/2=10 samples is harmless).
        signs = np.sign(samples).astype(np.int8)
        signs[signs == 0] = 1

        # ── Boundary crossing detection ──────────────────────────────────────
        # np.diff() cannot see across successive feed() calls.  If the LTC
        # signal has a zero-crossing exactly at the chunk boundary (i.e., the
        # last sample of the previous call and the first sample of this call
        # have opposite signs), we must detect it here explicitly.
        if self._last_sign != 0 and signs[0] != self._last_sign:
            abs_pos = self._n  # first sample of this chunk = boundary position
            self._cur_abs_pos = abs_pos
            if self._last_cross < 0:
                self._last_cross = abs_pos
            else:
                self._process_gap(abs_pos - self._last_cross)
                self._last_cross = abs_pos

        # Store last sign before we overwrite it below
        self._last_sign = int(signs[-1])

        # ── Internal zero-crossings ──────────────────────────────────────────
        crossings = np.where(np.diff(signs) != 0)[0]
        for idx in crossings:
            abs_pos = self._n + int(idx) + 1
            self._cur_abs_pos = abs_pos
            if self._last_cross < 0:
                self._last_cross = abs_pos
            else:
                self._process_gap(abs_pos - self._last_cross)
                self._last_cross = abs_pos

        self._n += len(samples)

    def reset(self) -> None:
        """Reset all decoder state (e.g., after detected signal loss)."""
        self._hp = 0.0
        self._boot_gaps = []
        self._bmc_state = 0
        self._buf = []
        self._last_cross = -1
        self._last_sign = 0
        self.fps = None

# ── OSC sender ─────────────────────────────────────────────────────────────────

class OSCSender:
    """Thread-safe OSC UDP sender with frame-rate limiting."""

    def __init__(self, host: str, port: int, address: str) -> None:
        if not _HAS_OSC:
            raise RuntimeError(
                "python-osc not installed. Run: pip install python-osc"
            )
        self._client = _UDPClient(host, port)
        self._address = address
        self._lock = threading.Lock()
        self._last_sent: float = 0.0
        self._min_interval: float = 1.0 / 32  # ≤ 32 sends / sec

    def send(self, tc_str: str) -> None:
        now = time.monotonic()
        with self._lock:
            if now - self._last_sent < self._min_interval:
                return
            self._last_sent = now
        try:
            self._client.send_message(self._address, tc_str)
        except OSError:
            pass  # network unreachable — don't crash the audio thread

# ── Device listing ─────────────────────────────────────────────────────────────

def list_devices() -> None:
    if not _HAS_SD:
        _die("sounddevice not installed. Run: pip install sounddevice")
    devices = sd.query_devices()
    print("\nAvailable audio input devices:\n")
    for i, dev in enumerate(devices):
        if dev["max_input_channels"] < 1:
            continue
        sr = int(dev["default_samplerate"])
        ch = dev["max_input_channels"]
        print(f"  [{i:3d}]  {dev['name']}")
        print(f"         {ch} in · {sr} Hz default")
    print()

# ── WAV file mode ──────────────────────────────────────────────────────────────

def run_wav(path: str, args: argparse.Namespace,
            osc: Optional[OSCSender]) -> None:
    """Decode LTC from a WAV file and optionally send via OSC."""
    try:
        wf = wave.open(path, "rb")
    except FileNotFoundError:
        _die(f"WAV file not found: {path}")
    except wave.Error as e:
        _die(f"Cannot open WAV file: {e}")

    n_ch   = wf.getnchannels()
    sampw  = wf.getsampwidth()
    rate   = wf.getframerate()
    n_samp = wf.getnframes()
    dur    = n_samp / rate

    if args.channel >= n_ch:
        _die(f"WAV has {n_ch} channel(s); --channel {args.channel} is out of range.")

    dtype_map = {1: np.int8, 2: np.int16, 4: np.int32}
    if sampw not in dtype_map:
        _die(f"Unsupported WAV sample width: {sampw} bytes")

    dtype    = dtype_map[sampw]
    max_val  = float(2 ** (sampw * 8 - 1))

    print(f"\n  File   : {path}")
    print(f"  Rate   : {rate} Hz · {n_ch}ch · {sampw*8}-bit · {dur:.1f} s")
    print(f"  Channel: {args.channel}")
    if osc:
        print(f"  OSC    : {args.target}:{args.port}  {args.address}")
    print()

    last_tc: list = [None]
    unique_count: list = [0]
    start_time = time.monotonic()

    def on_frame(tc: str, df: bool, fps: Optional[float]) -> None:
        if tc != last_tc[0]:
            last_tc[0] = tc
            unique_count[0] += 1
            fps_str = f"  @{fps:.2f} fps" if fps else ""
            print(f"\r  TC: {tc}{fps_str}    ", end="", flush=True)
        if osc:
            osc.send(tc)

    dec = LTCDecoder(sample_rate=rate, on_frame=on_frame)

    CHUNK = 4096
    try:
        while True:
            raw = wf.readframes(CHUNK)
            if not raw:
                break
            interleaved = np.frombuffer(raw, dtype=dtype)
            # De-interleave: select the chosen channel
            mono = interleaved[args.channel::n_ch].astype(np.float32) / max_val
            dec.feed(mono)
    except KeyboardInterrupt:
        pass
    finally:
        wf.close()

    elapsed = time.monotonic() - start_time
    print(f"\n\n  Done in {elapsed:.1f}s. "
          f"{unique_count[0]} unique timecodes decoded, "
          f"{dec.frame_count} frames total.")
    if dec.fps:
        print(f"  Detected FPS: {dec.fps}")

# ── Direct OSC send mode + d3 showcontrol ─────────────────────────────────────

# disguise d3 zero-argument trigger paths
_D3_TRIGGERS: dict = {
    "d3_play":            "/d3/showcontrol/play",
    "d3_stop":            "/d3/showcontrol/stop",
    "d3_loop":            "/d3/showcontrol/loop",
    "d3_playsection":     "/d3/showcontrol/playsection",
    "d3_nextsection":     "/d3/showcontrol/nextsection",
    "d3_previoussection": "/d3/showcontrol/previoussection",
    "d3_nexttrack":       "/d3/showcontrol/nexttrack",
    "d3_previoustrack":   "/d3/showcontrol/previoustrack",
    "d3_returntostart":   "/d3/showcontrol/returntostart",
    "d3_hold":            "/d3/showcontrol/hold",
    "d3_fadeup":          "/d3/showcontrol/fadeup",
    "d3_fadedown":        "/d3/showcontrol/fadedown",
}
_D3_VALUES = ("d3_volume", "d3_brightness", "d3_trackname", "d3_trackid",
              "d3_cue", "d3_floatcue")

# disguise d3 monitoring output paths (§7.3) — sent FROM d3 TO device
# Tuple: (osc_path, suggested_channel, label)
_D3_MONITORING: dict = {
    "timecodeposition":   ("/d3/showcontrol/timecodeposition",  "tc",       "Timecode"),
    "trackposition":      ("/d3/showcontrol/trackposition",     "d3_pos",   "Track Position"),
    "trackname":          ("/d3/showcontrol/trackname",         "d3_name",  "Track Name"),
    "trackid":            ("/d3/showcontrol/trackid",           "d3_id",    "Track ID"),
    "playmode":           ("/d3/showcontrol/playmode",          "d3_mode",  "Play Mode"),
    "currentsectionname": ("/d3/showcontrol/currentsectionname","d3_sec",   "Current Section"),
    "nextsectionname":    ("/d3/showcontrol/nextsectionname",   "d3_nsec",  "Next Section"),
    "sectionhint":        ("/d3/showcontrol/sectionhint",       "d3_hint",  "Section Hint"),
    "volume":             ("/d3/showcontrol/volume",            "d3_vol",   "Volume"),
    "brightness":         ("/d3/showcontrol/brightness",        "d3_bri",   "Brightness"),
    "bpm":                ("/d3/showcontrol/bpm",               "d3_bpm",   "BPM"),
    "heartbeat":          ("/d3/showcontrol/heartbeat",         "d3_hb",    "Heartbeat"),
}


def _any_d3(args: argparse.Namespace) -> bool:
    return (any(getattr(args, k, False) for k in _D3_TRIGGERS) or
            any(getattr(args, k, None) is not None for k in _D3_VALUES))


def run_d3(args: argparse.Namespace) -> None:
    """Send a disguise Designer showcontrol OSC command and exit."""
    if not _HAS_OSC:
        _die("python-osc not installed. Run: pip install python-osc")
    from pythonosc.udp_client import SimpleUDPClient
    client = SimpleUDPClient(args.target, args.d3_port)

    def _send(path: str, value=None) -> None:
        client.send_message(path, value)
        val_str = f"  {value!r}" if value is not None else ""
        print(f"  d3  → {args.target}:{args.d3_port}")
        print(f"        {path}{val_str}")

    # Zero-argument triggers
    for dest, path in _D3_TRIGGERS.items():
        if getattr(args, dest, False):
            _send(path)
            return

    # Value commands
    if args.d3_volume is not None:
        _send("/d3/showcontrol/volume", max(0.0, min(1.0, float(args.d3_volume))))
    elif args.d3_brightness is not None:
        _send("/d3/showcontrol/brightness", max(0.0, min(1.0, float(args.d3_brightness))))
    elif args.d3_trackname is not None:
        _send("/d3/showcontrol/trackname", args.d3_trackname)
    elif args.d3_trackid is not None:
        _send("/d3/showcontrol/trackid", args.d3_trackid)
    elif args.d3_cue is not None:
        ints = [int(x) for x in args.d3_cue[:3]]
        client.send_message("/d3/showcontrol/cue", ints[0] if len(ints) == 1 else ints)
        print(f"  d3  → {args.target}:{args.d3_port}")
        print(f"        /d3/showcontrol/cue  {'.' .join(str(i) for i in ints)}")
    elif args.d3_floatcue is not None:
        _send("/d3/showcontrol/floatcue", float(args.d3_floatcue))
    else:
        _die("No d3 command given — use --d3-play, --d3-volume, etc. (--help)")


# ── d3 monitoring listener registry (HTTP) ────────────────────────────────────

def _listener_api(args: argparse.Namespace, method: str, body: Optional[dict] = None) -> str:
    """Call the device's /api/osc/listeners HTTP endpoint."""
    url = f"http://{args.target}:{args.http_port}/api/osc/listeners"
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(
        url, data=data,
        headers={"Content-Type": "application/json"} if data else {},
        method=method,
    )
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            return resp.read().decode()
    except urllib.error.URLError as exc:
        _die(f"Device HTTP error ({url}): {exc.reason}")
    return ""


def run_listener_cmds(args: argparse.Namespace) -> None:
    """Handle --d3-list-listeners / --d3-add-listener / --d3-remove-listener / --d3-preset."""
    prefix = f"  device  → http://{args.target}:{args.http_port}"

    if args.d3_list_listeners:
        raw = _listener_api(args, "GET")
        listeners: list = json.loads(raw) if raw else []
        if not listeners:
            print(f"{prefix}\n  No listeners registered.")
        else:
            print(f"{prefix}")
            print(f"  {'OSC Path':<47} {'Channel':<15} Label")
            print("  " + "─" * 76)
            for lsr in listeners:
                print(f"  {lsr['path']:<47} {lsr['channel']:<15} {lsr.get('label','')}")
        return

    if args.d3_add_listener:
        parts = args.d3_add_listener
        if len(parts) < 2:
            _die("--d3-add-listener requires PATH CHANNEL [LABEL]")
        path, channel = parts[0], parts[1]
        label = parts[2] if len(parts) > 2 else channel
        _listener_api(args, "POST", {"path": path, "channel": channel, "label": label})
        print(f"{prefix}\n  Added   : {path} → {channel}  ({label})")
        return

    if args.d3_remove_listener:
        _listener_api(args, "DELETE", {"path": args.d3_remove_listener})
        print(f"{prefix}\n  Removed : {args.d3_remove_listener}")
        return

    if args.d3_preset:
        if args.d3_preset == "all":
            items = list(_D3_MONITORING.values())
        elif args.d3_preset in _D3_MONITORING:
            items = [_D3_MONITORING[args.d3_preset]]
        else:
            names = ", ".join(_D3_MONITORING.keys())
            _die(f"Unknown preset '{args.d3_preset}'. Choose: all, {names}")
        print(f"{prefix}")
        for path, channel, label in items:
            _listener_api(args, "POST", {"path": path, "channel": channel, "label": label})
            print(f"  Registered: {path} → {channel}  ({label})")
        return


def _any_listener_cmd(args: argparse.Namespace) -> bool:
    return any([
        getattr(args, "d3_list_listeners", False),
        getattr(args, "d3_add_listener",   None) is not None,
        getattr(args, "d3_remove_listener",None) is not None,
        getattr(args, "d3_preset",         None) is not None,
    ])


def run_send(args: argparse.Namespace, osc: Optional["OSCSender"]) -> None:
    """Send a single OSC string immediately and exit."""
    if osc is None:
        _die("--send requires OSC output. Don't use --no-osc with --send.")
    msg = args.send
    # Bypass rate-limiting: send directly for one-shot use
    try:
        osc._client.send_message(args.address, msg)  # type: ignore[union-attr]
        print(f"  Sent  : {args.address}")
        print(f"  Value : \"{msg}\"")
        print(f"  To    : {args.target}:{args.port}")
    except OSError as exc:
        _die(f"OSC send failed: {exc}")


# ── Test / generate mode ──────────────────────────────────────────────────────

def run_test(args: argparse.Namespace, osc: Optional[OSCSender]) -> None:
    """Generate and send TC from an arbitrary start time at a given frame rate."""
    fps = float(getattr(args, "fps", 30.0))
    start_tc_str = getattr(args, "start_tc", None)
    try:
        start = _tc_to_frames(start_tc_str, fps) if start_tc_str else 0
    except ValueError as exc:
        _die(str(exc))
    print(f"\n  [GENERATE] {fps} fps from {_frames_to_tc(start, fps)}. Ctrl+C to stop.\n")
    f = start
    try:
        while True:
            tc = _frames_to_tc(f, fps)
            print(f"\r  TC: {tc}    ", end="", flush=True)
            if osc:
                osc.send(tc)
            f += 1
            time.sleep(1.0 / fps)
    except KeyboardInterrupt:
        print("\n\n  Stopped.")

# ── Live audio mode ────────────────────────────────────────────────────────────

def run_live(args: argparse.Namespace, osc: Optional[OSCSender]) -> None:
    if not _HAS_SD:
        _die("sounddevice not installed. Run: pip install sounddevice")

    # Resolve device
    try:
        dev_idx  = args.device  # None = system default
        dev_info = sd.query_devices(dev_idx, "input")
    except Exception as exc:
        _die(f"Cannot open audio device: {exc}")

    n_ch = int(dev_info["max_input_channels"])
    if args.channel >= n_ch:
        _die(f"Device has {n_ch} input channel(s); --channel {args.channel} is out of range.")

    rate = args.rate or int(dev_info["default_samplerate"])
    name = dev_info["name"]

    print(f"\n  Target : {args.target}:{args.port}")
    print(f"  OSC    : {args.address}")
    if dev_idx is not None:
        print(f"  Device : [{dev_idx}] {name}")
    else:
        print(f"  Device : [default] {name}")
    print(f"  Channel: {args.channel}")
    print(f"  Rate   : {rate} Hz")
    print("\n  Listening... Ctrl+C to stop\n")

    last_tc: list = [None]
    dec = LTCDecoder(sample_rate=rate, on_frame=lambda *_: None)

    def on_frame(tc: str, df: bool, fps: Optional[float]) -> None:
        if tc != last_tc[0]:
            last_tc[0] = tc
            fps_str = f"  @{fps:.2f}" if fps else ""
            print(f"\r  TC: {tc}{fps_str}    ", end="", flush=True)
        if osc:
            osc.send(tc)

    dec.on_frame = on_frame  # type: ignore[assignment]

    def audio_callback(indata: np.ndarray, frames: int,
                       time_info: object, status: object) -> None:
        if status:
            print(f"\n  [audio] {status}", file=sys.stderr)
        mono = indata[:, args.channel].copy()
        dec.feed(mono)

    try:
        with sd.InputStream(
            device     = dev_idx,
            channels   = n_ch,
            samplerate = rate,
            dtype      = "float32",
            blocksize  = 1024,
            callback   = audio_callback,
        ):
            while True:
                time.sleep(0.1)
    except KeyboardInterrupt:
        print("\n\n  Stopped.")
    except sd.PortAudioError as exc:
        _die(f"PortAudio error: {exc}")
    except Exception as exc:
        _die(f"Audio error: {exc}")

# ── Argument parser ────────────────────────────────────────────────────────────

def _build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="ltc_osc_bridge",
        description=(
            "Decode SMPTE LTC from a system audio input (or WAV file) "
            "and send the timecode via OSC UDP."
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
LTC → OSC examples:
  %(prog)s --list-devices
  %(prog)s --target 192.168.1.100 --device 2
  %(prog)s --wav ltc.wav --no-osc
  %(prog)s --test --start-tc 01:00:00:00 --fps 25 --target 192.168.1.100
  %(prog)s --send "01:00:00:00" --target 192.168.1.100 --address /dpx_tc001/notify

disguise d3 showcontrol examples:
  %(prog)s --target 192.168.1.200 --d3-play
  %(prog)s --target 192.168.1.200 --d3-stop
  %(prog)s --target 192.168.1.200 --d3-cue 1 2 5
  %(prog)s --target 192.168.1.200 --d3-volume 0.8
  %(prog)s --target 192.168.1.200 --d3-brightness 0.5
  %(prog)s --target 192.168.1.200 --d3-trackname "Act 2"

d3 monitoring listener registry examples:
  %(prog)s --target 192.168.1.100 --d3-list-listeners
  %(prog)s --target 192.168.1.100 --d3-preset all
  %(prog)s --target 192.168.1.100 --d3-preset timecodeposition
  %(prog)s --target 192.168.1.100 --d3-add-listener /d3/showcontrol/playmode d3_mode "Play Mode"
  %(prog)s --target 192.168.1.100 --d3-remove-listener /d3/showcontrol/heartbeat
""",
    )
    p.add_argument("-l", "--list-devices", action="store_true",
                   help="List available audio input devices and exit")
    p.add_argument("-d", "--device", type=int, default=None, metavar="INDEX",
                   help="Audio input device index (default: system default)")
    p.add_argument("-c", "--channel", type=int, default=0, metavar="N",
                   help="Audio channel index to read LTC from (default: 0)")
    p.add_argument("-t", "--target", default="192.168.1.100", metavar="IP",
                   help="OSC target IP address (default: 192.168.1.100)")
    p.add_argument("-p", "--port", type=int, default=4210, metavar="PORT",
                   help="OSC UDP port (default: 4210)")
    p.add_argument("-a", "--address", default="/dpx_tc001/custom/tc",
                   metavar="ADDR",
                   help="OSC address (default: /dpx_tc001/custom/tc)")
    p.add_argument("-r", "--rate", type=int, default=None, metavar="HZ",
                   help="Sample rate in Hz (default: device default)")
    p.add_argument("--wav", metavar="FILE",
                   help="Decode LTC from a WAV file instead of live audio")
    p.add_argument("--send", metavar="STRING",
                   help="Send a single OSC string to --address and exit")
    p.add_argument("--no-osc", action="store_true",
                   help="Disable OSC output (decode and print only)")
    p.add_argument("--test", action="store_true",
                   help="Generate TC from --start-tc at --fps (no audio needed)")
    p.add_argument("--start-tc", default=None, metavar="HH:MM:SS:FF",
                   help="Starting timecode for generate mode (default: 00:00:00:00)")
    p.add_argument("--fps", type=float, default=30.0, metavar="FPS",
                   help="Frame rate for generate mode: 24 / 25 / 29.97 / 30 (default: 30)")
    p.add_argument("--version", action="version",
                   version=f"%(prog)s {__version__}")

    # ── disguise d3 showcontrol ────────────────────────────────────────────────
    d3g = p.add_argument_group(
        "disguise d3 showcontrol",
        "Send OSC transport commands to a disguise Designer machine and exit."
    )
    d3g.add_argument("--d3-port", type=int, default=7401, metavar="PORT",
                     help="disguise OSC receive port (default: 7401)")
    # Trigger commands (no args)
    for _flag, _path in [
        ("--d3-play",            "play"),
        ("--d3-stop",            "stop"),
        ("--d3-loop",            "loop section"),
        ("--d3-playsection",     "play to end of section"),
        ("--d3-nextsection",     "jump next section"),
        ("--d3-previoussection", "jump previous section"),
        ("--d3-nexttrack",       "jump next track"),
        ("--d3-previoustrack",   "jump previous track"),
        ("--d3-returntostart",   "return to start"),
        ("--d3-hold",            "hold/freeze outputs"),
        ("--d3-fadeup",          "master fade up"),
        ("--d3-fadedown",        "master fade down"),
    ]:
        d3g.add_argument(_flag, action="store_true", help=f"/d3/showcontrol/{_path}")
    # Value commands
    d3g.add_argument("--d3-volume",     type=float, metavar="0-1",
                     help="Set master volume (float 0.0–1.0)")
    d3g.add_argument("--d3-brightness", type=float, metavar="0-1",
                     help="Set master brightness (float 0.0–1.0)")
    d3g.add_argument("--d3-trackname",  metavar="NAME",
                     help="Jump to named track (string)")
    d3g.add_argument("--d3-trackid",    metavar="ID",
                     help="Jump to track by ID (string/int)")
    d3g.add_argument("--d3-cue",        nargs="+", metavar="N",
                     help="Trigger cue e.g. --d3-cue 1 2 5  → cue 1.2.5")
    d3g.add_argument("--d3-floatcue",   type=float, metavar="FLOAT",
                     help="Trigger float cue (single float, e.g. 1.05)")

    # ── d3 monitoring listener registry (device HTTP API) ─────────────────────
    _preset_names = ", ".join(_D3_MONITORING.keys())
    d3m = p.add_argument_group(
        "d3 monitoring listener registry",
        "Manage which d3 OSC monitoring paths the device listens for.\n"
        "These paths are broadcast FROM disguise TO the device (§7.3).\n"
        "Uses the device HTTP API — --target must be the device IP."
    )
    d3m.add_argument("--http-port", type=int, default=80, metavar="PORT",
                     help="Device HTTP port (default: 80)")
    d3m.add_argument("--d3-list-listeners", action="store_true",
                     help="List listener registry stored on device")
    d3m.add_argument("--d3-add-listener", nargs="+", metavar=("PATH", "CHANNEL"),
                     help="Add listener: PATH CHANNEL [LABEL]")
    d3m.add_argument("--d3-remove-listener", metavar="PATH",
                     help="Remove listener by OSC path")
    d3m.add_argument("--d3-preset", metavar="NAME",
                     help=f"Register well-known preset(s) on device: all, or one of: {_preset_names}")
    return p

# ── Helpers ────────────────────────────────────────────────────────────────────

def _die(msg: str, code: int = 1) -> None:
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(code)

# ── Entry point ────────────────────────────────────────────────────────────────

def main() -> None:
    p    = _build_parser()
    args = p.parse_args()

    print(f"\ndpx_tc001  LTC → OSC Bridge  v{__version__}")
    print("─" * 44)

    if args.list_devices:
        list_devices()
        return

    # Set up OSC sender (unless suppressed)
    osc: Optional[OSCSender] = None
    if not args.no_osc:
        if not _HAS_OSC:
            print(
                "WARNING: python-osc not installed — OSC output disabled.\n"
                "         pip install python-osc\n",
                file=sys.stderr,
            )
        else:
            try:
                osc = OSCSender(args.target, args.port, args.address)
                print(f"  OSC → udp://{args.target}:{args.port}  {args.address}")
            except Exception as exc:
                print(f"WARNING: Could not init OSC sender: {exc}", file=sys.stderr)

    # d3 showcontrol: no OSC sender needed (uses own client directly)
    if _any_d3(args):
        run_d3(args)
        return

    # d3 monitoring listener registry management (device HTTP API)
    if _any_listener_cmd(args):
        run_listener_cmds(args)
        return

    if args.send is not None:
        run_send(args, osc)
    elif args.test:
        run_test(args, osc)
    elif args.wav:
        run_wav(args.wav, args, osc)
    else:
        run_live(args, osc)


if __name__ == "__main__":
    main()
