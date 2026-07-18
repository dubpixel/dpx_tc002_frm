#!/usr/bin/env python3
"""
LTC → OSC Bridge — Web GUI  v2.0
==================================
Two-tab web interface:
  Tab 1: LTC → OSC  — live audio decode | TC generator | WAV file playback
  Tab 2: d3 Control — disguise Designer showcontrol OSC panel

Opens http://localhost:8765 automatically.
Usage:
    python ltc_osc_bridge_gui.py
    python ltc_osc_bridge_gui.py --port 9000
"""

from __future__ import annotations

import json
import os
import queue
import sys
import tempfile
import threading
import time
import wave
import webbrowser
from typing import Optional

# ── Dep checks ─────────────────────────────────────────────────────────────────

def _die(msg: str) -> None:
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)

try:
    from flask import Flask, Response, jsonify, request, stream_with_context
except ImportError:
    _die("Flask not installed.\n  Run: pip install flask")

try:
    import numpy as np
except ImportError:
    _die("NumPy not installed.\n  Run: pip install numpy")

try:
    from ltc_osc_bridge import (
        LTCDecoder, OSCSender, _tc_to_frames, _frames_to_tc,
        _HAS_SD, _HAS_OSC,
    )
except ImportError:
    _die("ltc_osc_bridge.py not found in the same directory.")

if _HAS_SD:
    import sounddevice as sd

if _HAS_OSC:
    from pythonosc.udp_client import SimpleUDPClient as _UDPClient

# ── Flask app ───────────────────────────────────────────────────────────────────

app = Flask(__name__)
app.config["SECRET_KEY"] = "ltc-osc-bridge-gui-v2-local"

# ── Shared state ────────────────────────────────────────────────────────────────

_lock         = threading.Lock()
_subscribers: list[queue.Queue] = []

_running       = False
_audio_stream  = None           # sounddevice InputStream (live mode)
_bg_thread     = None           # Thread (generate / wav mode)
_decoder       = None           # LTCDecoder
_osc_sender    = None           # OSCSender
_wav_tmp_path  = None           # path to uploaded WAV file
_stats         = {"tc": "--:--:--:--", "fps": None, "frames": 0}


def _broadcast(payload: dict) -> None:
    msg = json.dumps(payload)
    with _lock:
        dead = []
        for q in _subscribers:
            try:
                q.put_nowait(msg)
            except queue.Full:
                dead.append(q)
        for q in dead:
            _subscribers.remove(q)


def _make_on_frame(osc):
    """Return an on_frame callback that broadcasts TC and optionally sends OSC."""
    def on_frame(tc: str, df: bool, fps) -> None:
        _stats["tc"]     = tc
        _stats["fps"]    = fps
        _stats["frames"] = _stats["frames"] + 1
        _broadcast({"tc": tc, "fps": fps, "frames": _stats["frames"]})
        if osc:
            osc.send(tc)
    return on_frame


# ── Background threads ──────────────────────────────────────────────────────────

def _thread_generate(start_frame: int, fps: float, osc) -> None:
    global _running
    on_frame = _make_on_frame(osc)
    f = start_frame
    interval = 1.0 / fps
    try:
        while _running:
            on_frame(_frames_to_tc(f, fps), False, fps)
            f += 1
            time.sleep(interval)
    except Exception as exc:
        _broadcast({"error": str(exc)})
    _running = False
    _broadcast({"stopped": True})


def _thread_wav(path: str, channel: int, osc) -> None:
    global _running
    try:
        wf = wave.open(path, "rb")
        n_ch  = wf.getnchannels()
        sampw = wf.getsampwidth()
        rate  = wf.getframerate()

        dtype_map = {1: np.int8, 2: np.int16, 4: np.int32}
        if sampw not in dtype_map or channel >= n_ch:
            _broadcast({"error": f"WAV: bad format or channel out of range (has {n_ch}ch)"})
            _running = False
            return

        dtype   = dtype_map[sampw]
        max_val = float(2 ** (sampw * 8 - 1))
        dec     = LTCDecoder(sample_rate=rate, on_frame=_make_on_frame(osc))

        CHUNK      = 4096
        chunk_dur  = CHUNK / rate   # seconds per chunk (for real-time pacing)

        while _running:
            raw = wf.readframes(CHUNK)
            if not raw:
                break
            t0          = time.monotonic()
            interleaved = np.frombuffer(raw, dtype=dtype)
            mono        = interleaved[channel::n_ch].astype(np.float32) / max_val
            dec.feed(mono)
            sleep = chunk_dur - (time.monotonic() - t0)
            if sleep > 0:
                time.sleep(sleep)

        wf.close()
    except Exception as exc:
        _broadcast({"error": str(exc)})
    _running = False
    _broadcast({"stopped": True, "reason": "WAV playback complete"})


# ── Routes ──────────────────────────────────────────────────────────────────────

@app.route("/")
def index():
    return _HTML, 200, {"Content-Type": "text/html; charset=utf-8"}


@app.route("/api/devices")
def api_devices():
    if not _HAS_SD:
        return jsonify({"error": "sounddevice not installed — pip install sounddevice"}), 500
    out = []
    for i, d in enumerate(sd.query_devices()):
        if d["max_input_channels"] > 0:
            out.append({
                "index":    i,
                "name":     d["name"],
                "channels": int(d["max_input_channels"]),
                "rate":     int(d["default_samplerate"]),
                "default":  (i == sd.default.device[0]),
            })
    return jsonify(out)


@app.route("/api/wav", methods=["POST"])
def api_wav():
    """Upload a WAV file for playback. Returns basic file info."""
    global _wav_tmp_path
    if "wav" not in request.files:
        return jsonify({"error": "No file provided"}), 400
    f = request.files["wav"]
    try:
        tmp = tempfile.NamedTemporaryFile(delete=False, suffix=".wav")
        f.save(tmp.name)
        tmp.close()
        if _wav_tmp_path and os.path.exists(_wav_tmp_path):
            try:
                os.unlink(_wav_tmp_path)
            except OSError:
                pass
        _wav_tmp_path = tmp.name
        wf = wave.open(tmp.name, "rb")
        info = {
            "ok":       True,
            "channels": wf.getnchannels(),
            "rate":     wf.getframerate(),
            "bits":     wf.getsampwidth() * 8,
            "duration": round(wf.getnframes() / wf.getframerate(), 1),
        }
        wf.close()
        return jsonify(info)
    except Exception as exc:
        return jsonify({"error": str(exc)}), 400


@app.route("/api/start", methods=["POST"])
def api_start():
    global _audio_stream, _bg_thread, _decoder, _osc_sender, _running

    if _running:
        return jsonify({"error": "Already running"}), 400

    data    = request.get_json(force=True, silent=True) or {}
    source  = data.get("source", "live")       # live | generate | wav
    channel = int(data.get("channel", 0))
    target  = str(data.get("target",  "192.168.1.100")).strip()
    port    = int(data.get("port",    4210))
    address = str(data.get("address", "/dpx_tc001/custom/tc")).strip()

    # Build OSC sender
    _osc_sender = None
    if _HAS_OSC:
        try:
            _osc_sender = OSCSender(target, port, address)
        except Exception as exc:
            return jsonify({"error": f"OSC init failed: {exc}"}), 500

    _stats.update(tc="--:--:--:--", fps=None, frames=0)

    # ── Live audio ────────────────────────────────────────────────────────────
    if source == "live":
        if not _HAS_SD:
            return jsonify({"error": "sounddevice not installed — pip install sounddevice"}), 500
        dev_idx = data.get("device")
        try:
            dev_info = (
                sd.query_devices(dev_idx, "input") if dev_idx is not None
                else sd.query_devices(sd.default.device[0], "input")
            )
        except Exception as exc:
            return jsonify({"error": f"Cannot open device: {exc}"}), 400

        n_ch = int(dev_info["max_input_channels"])
        if channel >= n_ch:
            return jsonify({"error": f"Device has {n_ch}ch; channel {channel} out of range"}), 400
        rate = int(dev_info["default_samplerate"])

        _decoder = LTCDecoder(sample_rate=rate, on_frame=_make_on_frame(_osc_sender))

        def _audio_cb(indata, frames, time_info, status) -> None:
            if _decoder is not None:
                _decoder.feed(indata[:, channel].copy())

        try:
            _audio_stream = sd.InputStream(
                device=dev_idx, channels=n_ch, samplerate=rate,
                dtype="float32", blocksize=1024, callback=_audio_cb,
            )
            _audio_stream.start()
            _running = True
        except Exception as exc:
            return jsonify({"error": f"Cannot start audio: {exc}"}), 500

        return jsonify({"ok": True, "source": "live",
                        "device": dev_info["name"], "rate": rate, "osc": _HAS_OSC})

    # ── TC generator ──────────────────────────────────────────────────────────
    if source == "generate":
        fps_val   = float(data.get("fps", 30.0))
        start_str = data.get("start_tc", "00:00:00:00")
        try:
            start_frame = _tc_to_frames(start_str, fps_val)
        except ValueError as exc:
            return jsonify({"error": str(exc)}), 400

        _running   = True
        _bg_thread = threading.Thread(
            target=_thread_generate,
            args=(start_frame, fps_val, _osc_sender),
            daemon=True,
        )
        _bg_thread.start()
        return jsonify({"ok": True, "source": "generate",
                        "start_tc": start_str, "fps": fps_val, "osc": _HAS_OSC})

    # ── WAV playback ──────────────────────────────────────────────────────────
    if source == "wav":
        if not _wav_tmp_path or not os.path.exists(_wav_tmp_path):
            return jsonify({"error": "No WAV file uploaded yet"}), 400

        _running   = True
        _bg_thread = threading.Thread(
            target=_thread_wav,
            args=(_wav_tmp_path, channel, _osc_sender),
            daemon=True,
        )
        _bg_thread.start()
        return jsonify({"ok": True, "source": "wav", "osc": _HAS_OSC})

    return jsonify({"error": f"Unknown source: {source!r}"}), 400


@app.route("/api/stop", methods=["POST"])
def api_stop():
    global _audio_stream, _bg_thread, _decoder, _osc_sender, _running
    _running = False
    if _audio_stream is not None:
        try:
            _audio_stream.stop()
            _audio_stream.close()
        except Exception:
            pass
        _audio_stream = None
    _decoder    = None
    _osc_sender = None
    _broadcast({"tc": "--:--:--:--", "fps": None, "frames": 0, "stopped": True})
    return jsonify({"ok": True})


@app.route("/api/d3", methods=["POST"])
def api_d3():
    """Send a disguise d3 showcontrol OSC command."""
    if not _HAS_OSC:
        return jsonify({"error": "python-osc not installed — pip install python-osc"}), 500
    data = request.get_json(force=True, silent=True) or {}
    ip   = str(data.get("ip",   "192.168.1.100")).strip()
    port = int(data.get("port", 7401))
    path = str(data.get("path", "")).strip()
    args = data.get("args", None)   # None | scalar | list
    if not path.startswith("/"):
        return jsonify({"error": "OSC path must start with /"}), 400
    try:
        client = _UDPClient(ip, port)
        client.send_message(path, args)
        return jsonify({"ok": True, "path": path, "args": args})
    except Exception as exc:
        return jsonify({"error": str(exc)}), 500


@app.route("/api/stream")
def api_stream():
    q: queue.Queue = queue.Queue(maxsize=60)
    with _lock:
        _subscribers.append(q)

    def generate():
        yield f"data: {json.dumps(_stats)}\n\n"
        try:
            while True:
                try:
                    msg = q.get(timeout=12)
                    yield f"data: {msg}\n\n"
                except queue.Empty:
                    yield 'data: {"ping":true}\n\n'
        finally:
            with _lock:
                if q in _subscribers:
                    _subscribers.remove(q)

    return Response(
        stream_with_context(generate()),
        mimetype="text/event-stream",
        headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no",
                 "Connection": "keep-alive"},
    )


# ── Embedded HTML/CSS/JS ────────────────────────────────────────────────────────

_HTML = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>LTC → OSC Bridge</title>
<style>
:root{
  --bg:#111214;--surf:#1a1c1f;--surf2:#222428;--surf3:#2a2d32;
  --border:#2e3035;--border2:#42454d;
  --accent:#00e676;--accent-d:#00b359;
  --danger:#ef5350;--warn:#ffa726;--info:#42a5f5;
  --text:#dde1e7;--muted:#767b84;--label:#9aa0aa;
  --r:8px;--rl:14px;
}
*{box-sizing:border-box;margin:0;padding:0}
html,body{height:100%}
body{background:var(--bg);color:var(--text);
  font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,'Helvetica Neue',Arial,sans-serif;
  font-size:14px;line-height:1.5;
  display:flex;align-items:flex-start;justify-content:center;
  min-height:100vh;padding:24px 16px 40px}

/* ── Panel ── */
.panel{width:100%;max-width:520px;
  background:var(--surf);border:1px solid var(--border);
  border-radius:var(--rl);overflow:hidden;
  box-shadow:0 12px 48px rgba(0,0,0,.6)}

/* ── Tab bar ── */
.tab-bar{display:flex;border-bottom:1px solid var(--border);background:#141618}
.tab-btn{flex:1;padding:13px 8px;border:none;background:none;
  color:var(--muted);font-size:12px;font-weight:700;letter-spacing:.1em;
  text-transform:uppercase;cursor:pointer;
  border-bottom:2px solid transparent;transition:color .15s,border-color .15s}
.tab-btn.active{color:var(--accent);border-bottom-color:var(--accent)}
.tab-btn:hover:not(.active){color:var(--label)}

/* ── Header (inside panel body) ── */
.ph{padding:18px 24px 0;display:flex;align-items:center;gap:10px}
.ph svg{flex-shrink:0}
.ph-title{font-size:11px;font-weight:700;letter-spacing:.14em;
  text-transform:uppercase;color:var(--muted)}
.pb{padding:20px 24px 24px}

/* ── TC display ── */
.tc-card{background:#0d0f11;border:1px solid var(--border);border-radius:var(--r);
  padding:24px 20px 18px;margin-bottom:20px;text-align:center;
  transition:border-color .3s}
.tc-card.active{border-color:#1e3a2a}
.tc-value{font-family:'SF Mono','Fira Code','Cascadia Code',Menlo,monospace;
  font-size:50px;font-weight:700;letter-spacing:3px;color:#1e2a22;
  line-height:1;transition:color .25s;user-select:all}
.tc-value.live{color:var(--accent);text-shadow:0 0 24px rgba(0,230,118,.22)}
.tc-footer{display:flex;align-items:center;justify-content:space-between;
  margin-top:12px;font-size:12px;color:var(--muted)}
.dot{width:7px;height:7px;border-radius:50%;background:#333;
  flex-shrink:0;transition:background .2s,box-shadow .2s;display:inline-block;margin-right:6px}
.dot.on{background:var(--accent);box-shadow:0 0 8px var(--accent-d)}
.dot.wait{background:var(--warn);animation:pulse 1.1s ease-in-out infinite}
.dot.err{background:var(--danger)}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.35}}

/* ── Source selector ── */
.seg{display:flex;background:var(--surf2);border:1px solid var(--border);
  border-radius:var(--r);overflow:hidden;margin-bottom:16px}
.seg label{flex:1;text-align:center;padding:8px 4px;
  font-size:12px;font-weight:600;color:var(--muted);cursor:pointer;
  transition:background .15s,color .15s}
.seg input{display:none}
.seg input:checked+label{background:var(--surf3);color:var(--text)}
.seg label:hover{color:var(--label)}

/* ── Source panels ── */
.src-panel{display:none}
.src-panel.active{display:block}

/* ── Form ── */
.section{margin-bottom:18px}
.sec-label{font-size:10.5px;font-weight:700;letter-spacing:.12em;
  text-transform:uppercase;color:var(--muted);margin-bottom:9px}
.row{display:flex;gap:8px}
.field{display:flex;flex-direction:column;gap:5px;flex:1}
.field>label{font-size:12px;color:var(--label)}
select,input[type=text],input[type=number]{
  background:var(--surf2);border:1px solid var(--border);border-radius:var(--r);
  color:var(--text);padding:8px 10px;font-size:13px;font-family:inherit;
  width:100%;outline:none;transition:border-color .15s}
select{
  background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='10' height='6' viewBox='0 0 10 6'%3E%3Cpath d='M0 0l5 6 5-6z' fill='%23666'/%3E%3C/svg%3E");
  background-repeat:no-repeat;background-position:right 10px center;padding-right:28px;cursor:pointer}
select:focus,input:focus{border-color:var(--border2)}
select:disabled,input:disabled{opacity:.4;cursor:not-allowed}

/* WAV file input */
.file-btn{display:inline-flex;align-items:center;gap:6px;padding:8px 14px;
  background:var(--surf2);border:1px solid var(--border);border-radius:var(--r);
  color:var(--label);font-size:13px;cursor:pointer;transition:border-color .15s}
.file-btn:hover{border-color:var(--border2);color:var(--text)}
.file-btn input{display:none}
.wav-info{font-size:12px;color:var(--muted);margin-top:6px;min-height:16px}

/* ── Start/Stop ── */
.btn{display:flex;align-items:center;justify-content:center;gap:8px;
  width:100%;padding:12px 20px;border:none;border-radius:var(--r);
  font-size:14px;font-weight:700;letter-spacing:.07em;text-transform:uppercase;
  cursor:pointer;transition:background .15s,transform .1s,box-shadow .15s}
.btn:active:not(:disabled){transform:scale(.98)}
.btn:disabled{opacity:.4;cursor:not-allowed}
.btn-start{background:var(--accent);color:#000;box-shadow:0 4px 16px rgba(0,230,118,.2)}
.btn-start:hover:not(:disabled){background:#1bffb4;box-shadow:0 4px 20px rgba(0,230,118,.35)}
.btn-stop{background:var(--danger);color:#fff;box-shadow:0 4px 16px rgba(239,83,80,.2)}
.btn-stop:hover:not(:disabled){background:#f44}

/* ── Divider ── */
.div{border:none;border-top:1px solid var(--border);margin:18px 0}

/* ── Banners ── */
.banner{display:none;margin-top:12px;padding:10px 14px;border-radius:var(--r);
  font-size:12.5px;line-height:1.5}
.banner.err{background:#2a1010;border:1px solid #5a2020;color:#ff8a80}
.banner.info{background:#1a2010;border:1px solid #3a5020;color:#a5d6a7}

/* ── d3 tab ── */
.d3-grid{display:grid;gap:6px}
.d3-grid.g2{grid-template-columns:1fr 1fr}
.d3-grid.g4{grid-template-columns:1fr 1fr 1fr 1fr}

.d3-btn{padding:9px 6px;border:1px solid var(--border);border-radius:var(--r);
  background:var(--surf2);color:var(--label);font-size:12px;font-weight:600;
  cursor:pointer;transition:background .12s,border-color .12s,color .12s,transform .08s;
  text-align:center;line-height:1.3}
.d3-btn:hover{background:var(--surf3);border-color:var(--border2);color:var(--text)}
.d3-btn:active{transform:scale(.95)}
.d3-btn.flash{background:#1a3a28;border-color:var(--accent);color:var(--accent)}
.d3-btn.danger:hover{border-color:#5a2020;background:#2a1010;color:#ff8a80}

.d3-row{display:flex;gap:8px;align-items:flex-end}
.d3-row .field{flex:1}
.d3-go{flex-shrink:0;padding:8px 14px;border:1px solid var(--border);
  border-radius:var(--r);background:var(--surf2);color:var(--label);
  font-size:12px;font-weight:600;cursor:pointer;transition:all .12s;white-space:nowrap}
.d3-go:hover{border-color:var(--accent);color:var(--accent)}

/* Cue inputs */
.cue-row{display:flex;align-items:center;gap:6px}
.cue-row input{text-align:center}
.cue-dot{color:var(--muted);font-weight:700;font-size:16px;flex-shrink:0}
.cue-fire{padding:8px 16px;border:1px solid var(--border);border-radius:var(--r);
  background:var(--surf2);color:var(--label);font-size:12px;font-weight:600;
  cursor:pointer;transition:all .12s;flex-shrink:0}
.cue-fire:hover{border-color:var(--warn);color:var(--warn)}

/* Sliders */
.slider-row{display:flex;align-items:center;gap:10px;margin-bottom:10px}
.slider-label{font-size:12px;color:var(--label);width:80px;flex-shrink:0}
.slider-val{font-size:12px;color:var(--muted);width:36px;text-align:right;flex-shrink:0}
input[type=range]{flex:1;-webkit-appearance:none;appearance:none;
  height:4px;border-radius:2px;background:var(--border2);outline:none;cursor:pointer}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;
  width:14px;height:14px;border-radius:50%;background:var(--accent);cursor:pointer}
input[type=range]::-moz-range-thumb{width:14px;height:14px;border-radius:50%;
  background:var(--accent);cursor:pointer;border:none}

/* d3 flash feedback */
.d3-feedback{font-size:11px;color:var(--muted);min-height:16px;margin-top:8px;text-align:center}
</style>
</head>
<body>
<div class="panel">

<!-- ── Tab bar ── -->
<div class="tab-bar">
  <button class="tab-btn active" data-tab="ltc" onclick="showTab('ltc')">
    LTC → OSC
  </button>
  <button class="tab-btn" data-tab="d3" onclick="showTab('d3')">
    d3 Control
  </button>
</div>

<!-- ════════════════════════════════════════════════════════ TAB: LTC → OSC -->
<div id="tab-ltc">
<div class="ph">
  <svg width="16" height="16" viewBox="0 0 24 24" fill="none"
       stroke="#00e676" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
    <circle cx="12" cy="12" r="10"/><polyline points="12 6 12 12 16 14"/>
  </svg>
  <span class="ph-title">LTC → OSC Bridge</span>
</div>
<div class="pb">

  <!-- TC display -->
  <div class="tc-card" id="tcCard">
    <div class="tc-value" id="tcVal">--:--:--:--</div>
    <div class="tc-footer">
      <span><span class="dot" id="dot"></span><span id="statusTxt">Stopped</span></span>
      <span id="metaTxt"></span>
    </div>
  </div>

  <!-- Source selector -->
  <div class="sec-label">Source</div>
  <div class="seg" style="margin-bottom:14px">
    <input type="radio" name="src" id="srcLive" value="live" checked onchange="onSrcChange()">
    <label for="srcLive">🎙 Live Audio</label>
    <input type="radio" name="src" id="srcGen"  value="generate" onchange="onSrcChange()">
    <label for="srcGen">⏱ Generate</label>
    <input type="radio" name="src" id="srcWav"  value="wav" onchange="onSrcChange()">
    <label for="srcWav">📁 WAV File</label>
  </div>

  <!-- Live panel -->
  <div id="panelLive" class="src-panel active">
    <div class="section">
      <div class="field" style="margin-bottom:8px">
        <label>Device</label>
        <select id="device"></select>
      </div>
      <div class="row">
        <div class="field" style="max-width:110px">
          <label>Channel</label>
          <input type="number" id="channel" value="0" min="0" max="63">
        </div>
        <div class="field">
          <label>Channels available</label>
          <input type="text" id="chInfo" value="—" disabled style="color:var(--muted)">
        </div>
      </div>
    </div>
  </div>

  <!-- Generate panel -->
  <div id="panelGen" class="src-panel">
    <div class="section">
      <div class="row">
        <div class="field" style="flex:2">
          <label>Start TC (HH:MM:SS:FF)</label>
          <input type="text" id="startTc" value="00:00:00:00" placeholder="01:00:00:00">
        </div>
        <div class="field">
          <label>FPS</label>
          <select id="fps">
            <option value="24">24</option>
            <option value="25">25</option>
            <option value="29.97">29.97</option>
            <option value="30" selected>30</option>
          </select>
        </div>
      </div>
    </div>
  </div>

  <!-- WAV panel -->
  <div id="panelWav" class="src-panel">
    <div class="section">
      <div class="field" style="margin-bottom:6px">
        <label>Channel</label>
        <input type="number" id="wavChannel" value="0" min="0" max="63" style="max-width:110px">
      </div>
      <label class="file-btn">
        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor"
             stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
          <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/>
          <polyline points="17 8 12 3 7 8"/><line x1="12" y1="3" x2="12" y2="15"/>
        </svg>
        Browse WAV file…
        <input type="file" id="wavFile" accept=".wav,.WAV" onchange="onWavSelect(this)">
      </label>
      <div class="wav-info" id="wavInfo">No file selected</div>
    </div>
  </div>

  <hr class="div">

  <!-- OSC target -->
  <div class="section">
    <div class="sec-label">OSC Target (dpx_tc001)</div>
    <div class="row" style="margin-bottom:8px">
      <div class="field" style="flex:3">
        <label>IP Address</label>
        <input type="text" id="target" value="192.168.1.100">
      </div>
      <div class="field" style="flex:1.2">
        <label>Port</label>
        <input type="number" id="port" value="4210">
      </div>
    </div>
    <div class="field">
      <label>OSC Address</label>
      <input type="text" id="address" value="/dpx_tc001/custom/tc">
    </div>
  </div>

  <hr class="div">

  <button class="btn btn-start" id="mainBtn" onclick="toggle()">
    <svg width="13" height="13" viewBox="0 0 24 24" fill="currentColor"><polygon points="5,3 19,12 5,21"/></svg>
    Start
  </button>
  <div class="banner err"  id="errBanner"></div>
  <div class="banner info" id="infoBanner"></div>

</div><!-- /pb -->
</div><!-- /tab-ltc -->


<!-- ════════════════════════════════════════════════════ TAB: d3 Control -->
<div id="tab-d3" hidden>
<div class="ph">
  <svg width="16" height="16" viewBox="0 0 24 24" fill="none"
       stroke="#42a5f5" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
    <polygon points="23 7 16 12 23 17 23 7"/><rect x="1" y="5" width="15" height="14" rx="2"/>
  </svg>
  <span class="ph-title" style="color:var(--info)">disguise d3 Showcontrol</span>
</div>
<div class="pb">

  <!-- d3 target -->
  <div class="section">
    <div class="sec-label">d3 Machine Target</div>
    <div class="row">
      <div class="field" style="flex:3">
        <label>IP Address</label>
        <input type="text" id="d3Ip" value="192.168.1.100">
      </div>
      <div class="field" style="flex:1.2">
        <label>OSC Port</label>
        <input type="number" id="d3Port" value="7401">
      </div>
    </div>
  </div>

  <hr class="div">

  <!-- Playback controls -->
  <div class="section">
    <div class="sec-label">Playback</div>
    <div class="d3-grid g4" style="margin-bottom:6px">
      <button class="d3-btn" onclick="d3send('/d3/showcontrol/play',null,this)">▶<br>Play</button>
      <button class="d3-btn danger" onclick="d3send('/d3/showcontrol/stop',null,this)">⏹<br>Stop</button>
      <button class="d3-btn" onclick="d3send('/d3/showcontrol/loop',null,this)">↺<br>Loop</button>
      <button class="d3-btn" onclick="d3send('/d3/showcontrol/playsection',null,this)">⤷<br>Play §</button>
    </div>
    <div class="d3-grid g4">
      <button class="d3-btn" onclick="d3send('/d3/showcontrol/previoussection',null,this)">⏮<br>Prev §</button>
      <button class="d3-btn" onclick="d3send('/d3/showcontrol/nextsection',null,this)">⏭<br>Next §</button>
      <button class="d3-btn" onclick="d3send('/d3/showcontrol/returntostart',null,this)">↵<br>Return</button>
      <button class="d3-btn" onclick="d3send('/d3/showcontrol/hold',null,this)">⏸<br>Hold</button>
    </div>
  </div>

  <hr class="div">

  <!-- Output / brightness -->
  <div class="section">
    <div class="sec-label">Output</div>
    <div class="d3-grid g2">
      <button class="d3-btn" onclick="d3send('/d3/showcontrol/fadeup',null,this)">▲ Fade Up</button>
      <button class="d3-btn" onclick="d3send('/d3/showcontrol/fadedown',null,this)">▼ Fade Down</button>
    </div>
  </div>

  <hr class="div">

  <!-- Track navigation -->
  <div class="section">
    <div class="sec-label">Tracks</div>
    <div class="d3-grid g2" style="margin-bottom:10px">
      <button class="d3-btn" onclick="d3send('/d3/showcontrol/previoustrack',null,this)">⏮ Prev Track</button>
      <button class="d3-btn" onclick="d3send('/d3/showcontrol/nexttrack',null,this)">⏭ Next Track</button>
    </div>
    <div class="d3-row" style="margin-bottom:8px">
      <div class="field">
        <label>Go to track name</label>
        <input type="text" id="trackName" placeholder="Act 2">
      </div>
      <button class="d3-go" onclick="d3TrackName()">→ Go</button>
    </div>
    <div class="d3-row">
      <div class="field">
        <label>Go to track ID</label>
        <input type="text" id="trackId" placeholder="42">
      </div>
      <button class="d3-go" onclick="d3TrackId()">→ Go</button>
    </div>
  </div>

  <hr class="div">

  <!-- Cues -->
  <div class="section">
    <div class="sec-label">Cues</div>
    <div style="margin-bottom:8px">
      <div class="field" style="margin-bottom:5px"><label>Cue number (up to 3 parts)</label></div>
      <div class="cue-row">
        <input type="number" id="cue1" value="1" min="0" style="max-width:60px">
        <span class="cue-dot">.</span>
        <input type="number" id="cue2" value="" min="0" placeholder="–" style="max-width:60px">
        <span class="cue-dot">.</span>
        <input type="number" id="cue3" value="" min="0" placeholder="–" style="max-width:60px">
        <button class="cue-fire" onclick="d3Cue()">Fire</button>
      </div>
    </div>
    <div>
      <div class="field" style="margin-bottom:5px"><label>Float cue</label></div>
      <div class="cue-row">
        <input type="number" id="floatCue" value="1.1" step="0.01" style="max-width:100px">
        <button class="cue-fire" onclick="d3FloatCue()">Fire</button>
      </div>
    </div>
  </div>

  <hr class="div">

  <!-- Levels -->
  <div class="section">
    <div class="sec-label">Levels</div>
    <div class="slider-row">
      <span class="slider-label">Volume</span>
      <input type="range" id="volSlider" min="0" max="100" value="80"
             oninput="document.getElementById('volVal').textContent=(this.value/100).toFixed(2)"
             onchange="d3send('/d3/showcontrol/volume', parseFloat(this.value)/100)">
      <span class="slider-val" id="volVal">0.80</span>
    </div>
    <div class="slider-row">
      <span class="slider-label">Brightness</span>
      <input type="range" id="brSlider" min="0" max="100" value="80"
             oninput="document.getElementById('brVal').textContent=(this.value/100).toFixed(2)"
             onchange="d3send('/d3/showcontrol/brightness', parseFloat(this.value)/100)">
      <span class="slider-val" id="brVal">0.80</span>
    </div>
  </div>

  <div class="d3-feedback" id="d3Feedback"></div>

</div><!-- /pb -->
</div><!-- /tab-d3 -->

</div><!-- /panel -->

<script>
// ── Tabs ──────────────────────────────────────────────────────────────────────
function showTab(name) {
  ['ltc','d3'].forEach(t => {
    document.getElementById('tab-'+t).hidden = (t !== name);
    document.querySelector(`.tab-btn[data-tab="${t}"]`).classList.toggle('active', t === name);
  });
}

// ── Source selector ───────────────────────────────────────────────────────────
function onSrcChange() {
  const src = document.querySelector('input[name=src]:checked').value;
  ['Live','Gen','Wav'].forEach(s =>
    document.getElementById('panel'+s).classList.toggle('active', s.toLowerCase() === src.slice(0,3).toLowerCase())
  );
  // Normalize: 'generate' → 'Gen', 'wav' → 'Wav', 'live' → 'Live'
  const map = {live:'Live', generate:'Gen', wav:'Wav'};
  ['Live','Gen','Wav'].forEach(s =>
    document.getElementById('panel'+s).classList.remove('active')
  );
  const panel = {live:'panelLive', generate:'panelGen', wav:'panelWav'}[src];
  if (panel) document.getElementById(panel).classList.add('active');
}

// ── Persistence ───────────────────────────────────────────────────────────────
const LS = 'ltc_osc_bridge_v2';
function save() {
  try {
    localStorage.setItem(LS, JSON.stringify({
      target:   document.getElementById('target').value,
      port:     document.getElementById('port').value,
      address:  document.getElementById('address').value,
      channel:  document.getElementById('channel').value,
      startTc:  document.getElementById('startTc').value,
      fps:      document.getElementById('fps').value,
      wavCh:    document.getElementById('wavChannel').value,
      d3Ip:     document.getElementById('d3Ip').value,
      d3Port:   document.getElementById('d3Port').value,
      src:      document.querySelector('input[name=src]:checked').value,
      device:   document.getElementById('device').value,
    }));
  } catch(e) {}
}
function restore() {
  try {
    const s = JSON.parse(localStorage.getItem(LS) || '{}');
    if (s.target)  document.getElementById('target').value  = s.target;
    if (s.port)    document.getElementById('port').value    = s.port;
    if (s.address) document.getElementById('address').value = s.address;
    if (s.channel) document.getElementById('channel').value = s.channel;
    if (s.startTc) document.getElementById('startTc').value = s.startTc;
    if (s.fps)     document.getElementById('fps').value     = s.fps;
    if (s.wavCh)   document.getElementById('wavChannel').value = s.wavCh;
    if (s.d3Ip)    document.getElementById('d3Ip').value    = s.d3Ip;
    if (s.d3Port)  document.getElementById('d3Port').value  = s.d3Port;
    if (s.src) {
      const el = document.querySelector(`input[name=src][value="${s.src}"]`);
      if (el) { el.checked = true; onSrcChange(); }
    }
    return s.device;
  } catch(e) { return null; }
}

// ── Device list ───────────────────────────────────────────────────────────────
let _devices = [];
let _savedDevice = null;

async function loadDevices() {
  const sel = document.getElementById('device');
  try {
    const r = await fetch('/api/devices');
    if (!r.ok) { const d = await r.json(); sel.innerHTML = `<option>⚠ ${d.error||'Error'}</option>`; return; }
    _devices = await r.json();
    sel.innerHTML = _devices.map(d =>
      `<option value="${d.index}"${d.default?' selected':''}>[${d.index}] ${d.name} (${d.channels}ch)</option>`
    ).join('');
    if (_savedDevice !== null) {
      const opt = [...sel.options].find(o => o.value == _savedDevice);
      if (opt) opt.selected = true;
    }
    updateChInfo();
  } catch(e) { sel.innerHTML = '<option>Error loading devices</option>'; }
}
function updateChInfo() {
  const idx = parseInt(document.getElementById('device').value);
  const dev = _devices.find(d => d.index === idx);
  document.getElementById('chInfo').value = dev ? `${dev.channels} ch` : '—';
}
document.getElementById('device').addEventListener('change', updateChInfo);

// ── WAV upload ────────────────────────────────────────────────────────────────
let _wavUploaded = false;
async function onWavSelect(input) {
  if (!input.files[0]) return;
  const info = document.getElementById('wavInfo');
  info.textContent = 'Uploading…';
  const form = new FormData();
  form.append('wav', input.files[0]);
  try {
    const r = await fetch('/api/wav', { method: 'POST', body: form });
    const d = await r.json();
    if (d.error) { info.textContent = '⚠ ' + d.error; _wavUploaded = false; }
    else {
      info.textContent = `${input.files[0].name}  ·  ${d.channels}ch · ${d.rate}Hz · ${d.bits}-bit · ${d.duration}s`;
      _wavUploaded = true;
    }
  } catch(e) { info.textContent = '⚠ Upload error: ' + e.message; _wavUploaded = false; }
}

// ── SSE ───────────────────────────────────────────────────────────────────────
let _running = false;
let _evtSrc  = null;

function connectSSE() {
  if (_evtSrc) _evtSrc.close();
  _evtSrc = new EventSource('/api/stream');
  _evtSrc.onmessage = e => {
    const d = JSON.parse(e.data);
    if (d.ping) return;
    if (d.stopped)  { stopUI(); return; }
    if (d.error)    { showBanner('err', d.error); stopUI(); return; }
    if (d.tc && d.tc !== '--:--:--:--') {
      const el = document.getElementById('tcVal');
      el.textContent = d.tc;
      el.className   = 'tc-value live';
      document.getElementById('tcCard').className = 'tc-card active';
      setDot('on');
    }
    if (d.fps != null) {
      document.getElementById('metaTxt').textContent =
        `${parseFloat(d.fps).toFixed(2)} fps · ${(d.frames||0).toLocaleString()} frames`;
    }
  };
}
function disconnectSSE() { if (_evtSrc) { _evtSrc.close(); _evtSrc = null; } }

// ── Start / Stop ──────────────────────────────────────────────────────────────
const CTRL_IDS = ['device','channel','target','port','address','startTc','fps','wavChannel'];

async function toggle() {
  if (_running) {
    await fetch('/api/stop', { method: 'POST' });
    stopUI();
    return;
  }
  showBanner('err', '');
  showBanner('info', '');
  setBtn('…', false, '');

  const src = document.querySelector('input[name=src]:checked').value;

  if (src === 'wav' && !_wavUploaded) {
    showBanner('err', 'Please select a WAV file first.');
    setBtn('Start', true, 'btn-start');
    return;
  }

  const payload = {
    source:   src,
    target:   document.getElementById('target').value.trim(),
    port:     parseInt(document.getElementById('port').value) || 4210,
    address:  document.getElementById('address').value.trim(),
    channel:  parseInt(document.getElementById('channel').value) || 0,
  };
  if (src === 'live') {
    const dv = document.getElementById('device').value;
    payload.device = dv === '' ? null : parseInt(dv);
  }
  if (src === 'generate') {
    payload.start_tc = document.getElementById('startTc').value.trim();
    payload.fps      = parseFloat(document.getElementById('fps').value) || 30;
  }
  if (src === 'wav') {
    payload.channel = parseInt(document.getElementById('wavChannel').value) || 0;
  }

  try {
    const r = await fetch('/api/start', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify(payload),
    });
    const data = await r.json();
    if (data.error) { showBanner('err', data.error); setBtn('Start', true, 'btn-start'); return; }

    _running = true;
    save();
    setBtn('Stop', true, 'btn-stop');
    setDot('wait');
    document.getElementById('statusTxt').textContent = 'Listening…';
    lockControls(true);
    connectSSE();

    if (!data.osc) showBanner('info', 'python-osc not installed — TC decoded but not sent via OSC.\npip install python-osc');
  } catch(e) {
    showBanner('err', 'Network error: ' + e.message);
    setBtn('Start', true, 'btn-start');
  }
}

function stopUI() {
  _running = false;
  disconnectSSE();
  setBtn('Start', true, 'btn-start');
  setDot('');
  document.getElementById('statusTxt').textContent = 'Stopped';
  document.getElementById('metaTxt').textContent   = '';
  document.getElementById('tcVal').className       = 'tc-value';
  document.getElementById('tcCard').className      = 'tc-card';
  lockControls(false);
}

// ── UI helpers ────────────────────────────────────────────────────────────────
function setBtn(label, enabled, cls) {
  const btn = document.getElementById('mainBtn');
  btn.disabled  = !enabled;
  btn.className = 'btn ' + (cls || '');
  const icons   = {
    'btn-start': '<svg width="13" height="13" viewBox="0 0 24 24" fill="currentColor"><polygon points="5,3 19,12 5,21"/></svg>',
    'btn-stop':  '<svg width="13" height="13" viewBox="0 0 24 24" fill="currentColor"><rect x="6" y="6" width="12" height="12"/></svg>',
  };
  btn.innerHTML = (icons[cls] || '') + ' ' + label;
}
function setDot(state) {
  document.getElementById('dot').className = 'dot' + (state ? ' ' + state : '');
}
function lockControls(locked) {
  CTRL_IDS.forEach(id => {
    const el = document.getElementById(id);
    if (el) el.disabled = locked;
  });
  document.querySelectorAll('input[name=src]').forEach(el => el.disabled = locked);
  const wb = document.querySelector('.file-btn');
  if (wb) wb.style.pointerEvents = locked ? 'none' : '';
}
function showBanner(type, msg) {
  const el = document.getElementById(type === 'err' ? 'errBanner' : 'infoBanner');
  el.textContent = msg;
  el.style.display = msg ? 'block' : 'none';
}

// ── d3 helpers ────────────────────────────────────────────────────────────────
async function d3send(path, args, btn) {
  const ip   = document.getElementById('d3Ip').value.trim();
  const port = parseInt(document.getElementById('d3Port').value) || 7401;
  const fb   = document.getElementById('d3Feedback');
  try {
    const r = await fetch('/api/d3', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({ip, port, path, args: args ?? null}),
    });
    const d = await r.json();
    if (d.error) {
      fb.style.color = 'var(--danger)';
      fb.textContent = '⚠ ' + d.error;
    } else {
      fb.style.color = 'var(--accent)';
      const argStr = args != null ? '  ' + JSON.stringify(args) : '';
      fb.textContent = '✓  ' + path + argStr;
      if (btn) {
        btn.classList.add('flash');
        setTimeout(() => btn.classList.remove('flash'), 300);
      }
    }
  } catch(e) {
    fb.style.color = 'var(--danger)';
    fb.textContent = '⚠ ' + e.message;
  }
}
function d3TrackName() {
  const v = document.getElementById('trackName').value.trim();
  if (!v) return;
  d3send('/d3/showcontrol/trackname', v, null);
}
function d3TrackId() {
  const v = document.getElementById('trackId').value.trim();
  if (!v) return;
  d3send('/d3/showcontrol/trackid', v, null);
}
function d3Cue() {
  const a = parseInt(document.getElementById('cue1').value);
  const b = document.getElementById('cue2').value.trim();
  const c = document.getElementById('cue3').value.trim();
  const args = [a, ...(b !== '' ? [parseInt(b)] : []), ...(c !== '' ? [parseInt(c)] : [])];
  d3send('/d3/showcontrol/cue', args.length === 1 ? args[0] : args, null);
}
function d3FloatCue() {
  const v = parseFloat(document.getElementById('floatCue').value);
  if (isNaN(v)) return;
  d3send('/d3/showcontrol/floatcue', v, null);
}

// ── Init ──────────────────────────────────────────────────────────────────────
_savedDevice = restore();
loadDevices();
connectSSE();
</script>
</body>
</html>
"""

# ── Entry point ─────────────────────────────────────────────────────────────────

DEFAULT_PORT = 8765


def main() -> None:
    import argparse
    p = argparse.ArgumentParser(
        description="LTC → OSC Bridge v2 — web UI with d3 control panel"
    )
    p.add_argument("--port",       type=int, default=DEFAULT_PORT,
                   help=f"Local web server port (default: {DEFAULT_PORT})")
    p.add_argument("--no-browser", action="store_true",
                   help="Don't auto-open the browser")
    args = p.parse_args()

    url = f"http://localhost:{args.port}"
    print(f"\ndpx_tc001  LTC → OSC Bridge  v2  (Web UI)")
    print(f"{'─' * 42}")
    print(f"  Open : {url}")
    print(f"  Stop : Ctrl+C\n")

    if not args.no_browser:
        def _open():
            time.sleep(0.9)
            webbrowser.open(url)
        threading.Thread(target=_open, daemon=True).start()

    app.run(host="127.0.0.1", port=args.port,
            debug=False, threaded=True, use_reloader=False)


if __name__ == "__main__":
    main()
