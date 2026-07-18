# LTC → OSC Bridge — Agent Handoff

**Task:** Build a small cross-platform Python app that listens for LTC (Linear Timecode) on a system audio input, decodes it, and sends the decoded timecode string via OSC to a dpx_tc001 pixel clock device (or any OSC receiver).

**Status:** Not started. Directory created. No code exists yet.

---

## What It Needs To Do

1. List available audio input devices so user can pick the right one
2. Capture audio from a selected input channel (LTC arrives as audio — typically on one channel of a stereo or multi-channel interface)
3. Decode the LTC biphase mark signal in real time
4. Format the timecode as `HH:MM:SS:FF` (or `HH:MM:SS,FF` for drop frame)
5. Send via OSC UDP to a configurable target IP/port/address
6. Print current TC to terminal so user can confirm it's working

---

## Target OSC Receiver (dpx_tc001 firmware)

The receiving device is an ESP32 pixel clock running custom firmware. OSC is received on **UDP port 4210**.

Relevant OSC addresses:

| Address | Args | Effect |
|---------|------|--------|
| `/dpx_tc001/custom/tc` | `(s)` string | Creates/updates a **persistent custom app** named `tc` that stays in the display rotation — best for ongoing TC display |
| `/dpx_tc001/notify` | `(s)` string | **One-shot notification** — pops up, scrolls, disappears |
| `/notify` | `(s)` string | Same as above, no namespace prefix |

**Recommendation:** Use `/dpx_tc001/custom/tc` as the default address. The custom app persists and updates live. The display name is "tc" and it will scroll the timecode string continuously.

The device also accepts `/awtrix/custom/tc` as a compatibility alias.

**OSC packet format:** Standard OSC 1.0 over UDP. No TCP, no bundles needed.

---

## LTC Technical Spec

**LTC = Linear Timecode (SMPTE 12M)**

LTC encodes SMPTE timecode as an audio signal using **biphase mark code (BMC)**:
- The signal is an audio waveform — looks like a square wave with varying pulse widths
- Bit encoding:
  - There is ALWAYS a transition at the start of every bit period
  - Bit = **1**: additional transition at the MID-point of the bit period
  - Bit = **0**: NO mid-point transition
- Result: measuring zero-crossing gaps:
  - **Short gap** (≈ T/2): part of a '1' bit (two shorts in a row = one '1')
  - **Long gap** (≈ T): a '0' bit

**Frame structure (80 bits, transmitted LSB-first within each field):**

```
Bits  0- 3: Frame units    (BCD, 0-9)
Bits  4- 7: User bits 1    (4 bits, ignore)
Bits  8- 9: Frame tens     (BCD, 0-2)
Bit  10:    Drop frame flag
Bit  11:    Color frame flag
Bits 12-15: User bits 2
Bits 16-19: Seconds units  (BCD, 0-9)
Bits 20-23: User bits 3
Bits 24-26: Seconds tens   (BCD, 0-5)
Bit  27:    Unused
Bits 28-31: User bits 4
Bits 32-35: Minutes units  (BCD, 0-9)
Bits 36-39: User bits 5
Bits 40-42: Minutes tens   (BCD, 0-5)
Bit  43:    Biphase correction bit
Bits 44-47: User bits 6
Bits 48-51: Hours units    (BCD, 0-9)
Bits 52-55: User bits 7
Bits 56-57: Hours tens     (BCD, 0-2)
Bits 58-63: Flags + user bits 8
Bits 64-79: SYNC WORD = 0011 1111 1111 1101  (fixed pattern, MSB first)
```

**Sync word check:**
```python
SYNC_WORD = [0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,1]  # bits[64:80]
# or as integer (bit64=MSB): int(''.join(str(b) for b in bits[64:80]), 2) == 0x3FFD
```

**BCD decode helper:**
```python
def bcd(bits_slice):
    return sum(b << i for i, b in enumerate(bits_slice))
```

**Frame decode:**
```python
frames = bcd(bits[0:4]) + bcd(bits[8:10]) * 10
drop   = bool(bits[10])
secs   = bcd(bits[16:20]) + bcd(bits[24:27]) * 10
mins   = bcd(bits[32:36]) + bcd(bits[40:43]) * 10
hours  = bcd(bits[48:52]) + bcd(bits[56:58]) * 10

tc_str = f"{hours:02d}:{mins:02d}:{secs:02d}{',' if drop else ':'}{frames:02d}"
```

**Common frame rates:** 24, 25, 29.97 (DF), 30 fps. Bit rate = fps × 80 bits/frame.
At 30fps on 48kHz audio: T ≈ 1600 samples/bit, T/2 ≈ 800 samples.

**Auto-detection of bit period:** Track recent zero-crossing gaps. The shortest gaps cluster around T/2. A simple approach: keep a running minimum of recent gaps (with outlier rejection) as the T/2 estimate.

---

## Biphase Mark Decoder Algorithm

```
State machine with two states: IDLE and HALF

On each zero crossing, measure gap since last crossing:
  threshold = 1.5 × estimated_half_period

  If gap < threshold (SHORT):
    If state == IDLE:  → set state = HALF  (first half of a '1')
    If state == HALF:  → emit bit=1, set state = IDLE
  
  If gap >= threshold (LONG):
    If state == HALF:  → resync error, reset state = IDLE
    If state == IDLE:  → emit bit=0, stay IDLE

After emitting each bit, check last 80 bits for sync word.
When sync found at position P: decode bits[P:P+80] as frame, clear buffer.
```

**Auto-calibrate half period:** Use the median or filtered minimum of recent short gaps. Update continuously. Handle both short and long gaps gracefully — don't break on occasional noise.

---

## Libraries

```
sounddevice    # cross-platform audio I/O via PortAudio
numpy          # signal processing
python-osc     # OSC UDP client
```

```
pip install sounddevice numpy python-osc
```

`sounddevice` uses PortAudio which ships pre-built for macOS/Windows/Linux — no system dependencies needed.

---

## CLI Interface

```
python ltc_osc_bridge.py --list-devices
python ltc_osc_bridge.py --target 192.168.1.100 --device 2
python ltc_osc_bridge.py --target 192.168.1.100 --device 2 --channel 1 --port 4210
python ltc_osc_bridge.py --target 192.168.1.100 --device 2 --address /custom/tc
```

**Arguments:**
- `--list-devices` / `-l` — print available audio inputs and exit
- `--device` / `-d` — device index from list (default: system default input)
- `--channel` / `-c` — which channel index to read LTC from (default: 0)
- `--target` / `-t` — target IP (default: `192.168.1.100`)
- `--port` / `-p` — OSC port (default: `4210`)
- `--address` / `-a` — OSC address (default: `/dpx_tc001/custom/tc`)
- `--rate` / `-r` — sample rate in Hz (default: `48000`)

---

## Output Format

Terminal should show the current timecode updating in place:
```
dpx_tc001 LTC→OSC Bridge
  Target : 192.168.1.100:4210
  OSC    : /dpx_tc001/custom/tc
  Device : [2] Dante Virtual Soundcard
  Channel: 0

Listening... Press Ctrl+C to stop

  TC: 01:23:45:12    ← updates in place on same line
```

---

## Edge Cases to Handle

1. **No signal / silence** — don't spam OSC. Only send when a valid frame is decoded.
2. **Signal dropout** — after N frames with no valid decode, optionally send a blank or stop sending.
3. **Wrong channel** — user picks the wrong channel. Should fail gracefully with a useful message.
4. **Frame rate detection** — don't require user to specify fps. Auto-detect from bit period measurement. Display detected fps in the header.
5. **Rate limiting** — don't send OSC faster than once per frame (e.g., max 30 sends/sec). The display only updates ~30fps anyway.
6. **Drop frame** — format correctly with `;` or `,` separator when drop flag is set.

---

## Files to Create

```
tools/ltc_osc_bridge/
    ltc_osc_bridge.py   ← main script, single file, no classes required
    requirements.txt    ← sounddevice, numpy, python-osc
```

Keep it a single Python file. No packaging, no setup.py, no complexity. It should run with `python ltc_osc_bridge.py` after `pip install -r requirements.txt`.

---

## Test Without LTC Hardware

To test the OSC sending without an actual LTC signal, add a `--test` flag that sends a fake incrementing timecode at 30fps:

```python
if args.test:
    f = 0
    while True:
        tc = f"{(f//108000)%24:02d}:{(f//1800)%60:02d}:{(f//30)%60:02d}:{f%30:02d}"
        osc.send_message(args.address, tc)
        print(f"\r  TC: {tc}    ", end="", flush=True)
        f += 1
        time.sleep(1/30)
```

---

## Context

- This tool is a companion to the `dpx_tc001` pixel clock firmware (ESP32, Ulanzi TC001)
- OSC receiver is on UDP port 4210, same port as device discovery — coexists fine
- The `/dpx_tc001/custom/tc` app will display the timecode string, scrolling it across the 32×8 LED matrix
- The firmware also accepts `/dpx_tc001/notify` for one-shot pop-up display
- Future: this same tool should work as-is with a WLED-based device that has an OSC receiver on the same port
