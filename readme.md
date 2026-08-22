<!-- Improved compatibility of back to top link: See: https://github.com/othneildrew/Best-README-Template/pull/73 -->
<a id="readme-top"></a>

<!--  *** Thanks for checking out the Best-README-Template. If you have a suggestion that would make this better, please fork the repo and create a pull request or simply open an issue with the tag "enhancement". Don't forget to give the project a star! Thanks again! Now go create something AMAZING! :D -->



<!-- /// d   u   b   p   i   x   e   l  ---  f   o   r   k   ////--v0.5.6 -->
<!--this has additionally been modifed by @dubpixel for hardware use -->
<!--search dpx_tc002_frm.. search & replace is COMMAND OPTION F -->

<!--this is the version for HARDWARE and SOFTWARE -->
<!--todo add small product image thats not in a details tag -->
<!--igure out how to get the details tag to properly render in jekyll for gihub pages.-->



<!-- PROJECT SHIELDS -->
<!--
*** I'm using markdown "reference style" links for readability.
*** Reference links are enclosed in brackets [ ] instead of parentheses ( ).
*** See the bottom of this document for the declaration of the reference variables
*** for contributors-url, forks-url, etc. This is an optional, concise syntax you may use.
*** https://www.markdownguide.org/basic-syntax/#reference-style-links
***
-->
<div align="center">

[![Contributors][contributors-shield]][contributors-url]
[![Forks][forks-shield]][forks-url]
[![Stargazers][stars-shield]][stars-url]
[![Issues][issues-shield]][issues-url]
[![License][license-shield]][license-url]
[![LinkedIn][linkedin-shield]][linkedin-url]
</div>
<!-- PROJECT LOGO -->
<div align="center">
  <a href="https://github.com/dubpixel/dpx_tc002_frm">
    <img src="images/logo.png" alt="Logo" height="120">
  </a>
<h1 align="center">dpx_tc002_frm</h1>
<h3 align="center"><i>A matrix-focused toolkit built on WLED — flash it from a browser, drive it from OSC/MQTT/HTTP</i></h3>
  <p align="center">
    Turns the Ulanzi TC001 into a networked 32×8 status display with a fully documented open
    API — and it's not just a clock: notifications, timecode, custom clocks, and 200+ WLED
    effects, all controllable without touching a soldering iron.
        <br />
     »  
     <a href="https://github.com/dubpixel/dpx_tc002_frm/tree/main/"><strong>Project Here!</strong></a>
     »
    <a href="https://dubpixel.github.io/dpx_tc002_frm/flash/"><strong>Flash a Device Here!</strong></a>
     »
    <a href="https://dubpixel.github.io/dpx_tc002_frm/api/"><strong>API Docs Here!</strong></a>
     <br />
    <a href="https://github.com/dubpixel/dpx_tc002_frm/issues/new?labels=bug&template=bug-report---.md">Report Bug</a>
    ·
    <a href="https://github.com/dubpixel/dpx_tc002_frm/issues/new?labels=enhancement&template=feature-request---.md">Request Feature</a>
    </p>
</div>

<div align="center">
  <a href="https://friendster.dubpixel.tv/about">
    <img alt="matrix_blaster companion server" src="https://img.shields.io/badge/companion%20server-matrix__blaster-7ee787?style=for-the-badge&labelColor=0a0a0d">
  </a>
</div>
   <br />

### Screenshots

Live off the device — `/ctrl` control panel and `/api-ref`. (See [Icons & Media Browser](#icons--media-browser)
below for the `/browse` icon picker screenshot.)

<details>
<summary>Click to expand</summary>

![Control panel][ui-shot-ctrl]
![API reference][ui-shot-apiref]

</details>

> 🚧 Real action shots of the device running are still placeholders for now — see the
> [About](#about-the-project) section below.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- TABLE OF CONTENTS -->
<details>
  <summary><h3>Table of Contents</h3></summary>
<ol>
    <li><a href="#screenshots">Screenshots</a></li>
    <li><a href="#about-the-project">About The Project</a></li>
    <li><a href="#getting-started">Getting Started</a></li>
    <li><a href="#first-boot-defaults">First Boot Defaults</a></li>
    <li><a href="#usage">Usage</a></li>
    <li><a href="#icons--media-browser">Icons &amp; Media Browser</a></li>
    <li><a href="#osc-control">OSC Control</a></li>
    <li><a href="#roadmap">Roadmap</a></li>
    <li><a href="#contributing">Contributing</a></li>
    <li><a href="#license">License</a></li>
    <li><a href="#contact">Contact</a></li>
    <li><a href="#acknowledgments">Acknowledgments</a></li>
</ol>
</details>
<!-- ABOUT THE PROJECT -->
<details>
<summary><h3>About The Project</h3></summary>

**dpx_tc002_frm** is a custom WLED firmware build, currently targeting the [Ulanzi TC001](https://www.ulanzi.com/products/ulanzi-pixel-smart-clock-2882) — a desktop-sized ESP32 device with a 32×8 WS2812B LED matrix (not wearable). It replaces the stock Awtrix firmware with a WLED base plus the `dpx_matrix` usermod, adding timecode display, OSC/MQTT control, scrolling text apps, pixel overlay effects, first-boot hardware configuration, and a fully documented HTTP/OSC/MQTT API — it's not limited to being a clock, it's a general-purpose networked pixel display you can drive from anything. Because it's WLED underneath, the same firmware isn't tied to this one device long-term: it extends to other addressable-LED matrix layouts, and a dedicated `wled-matrix` board is planned down the line.

Built on top of [WLED](https://github.com/wled/WLED) (EUPL v1.2) — all WLED features remain fully functional.

### What WLED brings

All standard WLED features are intact in this build. See full docs at [kno.wled.ge](https://kno.wled.ge).

**Effects & Visuals**
- [**200+ built-in effects**](https://kno.wled.ge/features/effects/) — classic animations, audio-reactive, and 2D/matrix effects
- [50+ color palettes](https://kno.wled.ge/features/palettes/) plus a built-in **custom palette editor** (PixelForge)
- [**2D LED matrix support**](https://kno.wled.ge/advanced/mapping/) with dedicated 2D effects and flexible panel mapping
- [**HUB75 RGB matrix panel support**](https://kno.wled.ge/advanced/HUB75/) (ESP32)
- [**AudioReactive**](https://kno.wled.ge/advanced/audio-reactive/) effects responding to sound via microphone, line-in, or network audio source
- **Effect blending** for smooth transitions between animations
- Antialiased drawing functions for smooth graphics

**Segments & Control**
- [**Segments**](https://kno.wled.ge/features/segments/) — apply different effects, colors, and palettes to independent parts of your LED setup simultaneously
- Up to **250 presets** to save and recall colors, effects, and segment configurations — supports [playlists](https://kno.wled.ge/features/presets/) for automated cycling
- **Nightlight** function with configurable dimming curve
- **Auto Brightness Limiter** (per output) for safe operation

**Hardware Support**
- [**Up to 17 LED outputs**](https://kno.wled.ge/features/multi-strip/) on ESP32 using parallel I2S + RMT
- [Addressable LED support](https://kno.wled.ge/basics/compatible-led-strips/): WS2812B, WS2811, WS2815, SK6812, WS2805, TM1914, APA102, WS2801, LPD8806, and many more
- RGBW, [RGB+CCT](https://kno.wled.ge/features/cct/), and white-only strips
- PWM outputs for analog LEDs and dimmers
- [**Ethernet** support](https://kno.wled.ge/features/ethernet-lan/) for a wide range of boards
- Filesystem-based config — easy backup and restore of presets and settings
- Full OTA firmware updates (HTTP + ArduinoOTA), password-protectable

**Connectivity & Integrations**
- **WLED app** for [Android](https://play.google.com/store/apps/details?id=ca.cgagnier.wlednativeandroid) and [iOS](https://apps.apple.com/gb/app/wled-native/id6446207239)
- [JSON](https://kno.wled.ge/interfaces/json-api/) and [HTTP request](https://kno.wled.ge/interfaces/http-api/) APIs
- **Multi-WiFi** — connect to up to 3 networks with automatic AP fallback
- **ESP-NOW** wireless sync between devices (no WiFi router required)
- [**MQTT**](https://kno.wled.ge/interfaces/mqtt/) with Home Assistant discovery
- [**E1.31, Art-Net**](https://kno.wled.ge/interfaces/e1.31-dmx/), [DDP](https://kno.wled.ge/interfaces/ddp/), and [TPM2.net](https://kno.wled.ge/interfaces/udp-realtime/) for DMX / professional lighting control
- [UDP realtime sync](https://kno.wled.ge/interfaces/udp-notifier/) across multiple WLED devices
- Alexa voice control (on/off, brightness, color)
- [Philips Hue sync](https://kno.wled.ge/interfaces/philips-hue/)
- [diyHue](https://github.com/diyhue/diyHue) and [Hyperion](https://github.com/hyperion-project/hyperion.ng) integration
- [Adalight / TPM2](https://kno.wled.ge/interfaces/serial/) — PC ambilight via serial
- [Infrared remote control](https://kno.wled.ge/interfaces/infrared/) (24-key RGB, receiver required)
- NTP time sync with full **timezone + DST support**, timers and schedules

**Developer**
- **Usermod system** — extend WLED with community or custom modules without modifying core code
- [Community usermod library](https://kno.wled.ge/advanced/community-usermods/) — AudioReactive, temperature sensors, rotary encoders, displays, and more
- Well-documented [JSON API](https://kno.wled.ge/interfaces/json-api/) and [HTTP API](https://kno.wled.ge/interfaces/http-api/)
- Licensed under the **EUPL v1.2**

> 💬 Community: [Discord](https://discord.gg/QAh7wJHrRM) · [Forum](https://wled.discourse.group) · [Compatible hardware](https://kno.wled.ge/basics/compatible-hardware)

### What dpx_matrix adds

- **OSC receiver** (UDP 4210) — compatible with dpx_tc001 and AWTRIX OSC senders
- **MQTT** via WLED's broker connection — commands, device presence, and icon fetch over MQTT
- **Timecode display** (LTC via OSC) with frame progress bar
- **Notifications** — one-shot priority messages
- **8×8 icon rendering** — LaMetric PNG→raw pipeline, icon browser built into `/ctrl`
- Scrolling/static **text app loop** (Time, Date, custom apps, WLED pattern slots) with AwtrixFont — including multiple instances of the same app
- **World clock** — customizable per-instance clocks with their own timezone offset, color, and icon; add as many as you want for multiple timezones at a glance
- **OTA updates** — inherited from the WLED base: HTTP + ArduinoOTA, password-protectable, no cable needed after the first flash
- **First-boot config** — device is usable out of the box, no wizard required
- **Text overlay** + 15 dpx-native pixel effects (weather, decorative, and full-screen) composited live with text. 13 of the 15 dodge lit text pixels so it stays readable; `colorwaves` and `pacifica` still blend over text uniformly ([GH #69](https://github.com/dubpixel/dpx_tc002_frm/issues/69)). This is separate from WLED's own 200+ built-in effects — those run via "pattern slot" apps (`type:"wled_fx"`), a full-screen dwell with no text/icon shown during it, not simultaneous compositing; true simultaneous text-over-any-WLED-effect compositing is tracked separately as unimplemented ([GH #14](https://github.com/dubpixel/dpx_tc002_frm/issues/14))
- **Device-claim PIN pairing** — full-screen PIN display for [matrix_blaster](https://friendster.dubpixel.tv/about) (working title) companion server's device-claim flow
- **Browser-flashable firmware** — install directly over USB from Chrome/Edge, no PlatformIO needed (see [Getting Started](#getting-started))

*author: [Joshua Fleitell](https://www.dubpixel.tv) — i@dubpixel.tv*

> ⚠️ If you are prone to photosensitive epilepsy, avoid strobe/lightning effects and high speed settings.

### Images

### FRONT
![FRONT][product-front]
### REAR
![REAR][product-rear]
### FRONT Rendering
![FRONT][product-front-rendering]
### REAR Rendering
![REAR][product-rear-rendering]

### Action Shots

> 🚧 Placeholders below — real photos coming later. Swap `images/action_shot_placeholder*.svg`
> for actual shots of the device running (a notification scrolling, a custom clock, etc.).

![Action shot 1][action-shot-1]
![Action shot 2][action-shot-2]

</details>
<p align="right">(<a href="#readme-top">back to top</a>)</p>

### Built With

* [![WLED][WLED-badge]][WLED-url]
* [![PlatformIO][PlatformIO-badge]][PlatformIO-url]
* [![ESP32][ESP32-badge]][ESP32-url]
<p align="right">(<a href="#readme-top">back to top</a>)</p>
<!-- GETTING STARTED -->

## Getting Started

  ### Just want to flash a device? No dev setup needed.
  Flash the latest firmware directly from your browser over USB — no PlatformIO, no
  Node, no drivers: **[dubpixel.github.io/dpx_tc002_frm/flash](https://dubpixel.github.io/dpx_tc002_frm/flash/)**
  (Chrome or Edge only — Web Serial API isn't supported elsewhere). Pick the **nightly**
  channel (latest `main`, rebuilt on every push) or **release** (pinned to the latest
  tagged version). Prefer screenshots over text? There's also a
  **[visual, step-by-step guide](https://dubpixel.github.io/dpx_tc002_frm/flash/visual/)**
  covering the same install with real screenshots at every step.

  ### Want to talk to a device? Read the API docs.
  Full HTTP/OSC/MQTT reference, generated straight from the firmware source (can't drift
  out of sync): **[dubpixel.github.io/dpx_tc002_frm/api](https://dubpixel.github.io/dpx_tc002_frm/api/)**.
  Building an agent/integration instead of reading in a browser? Grab the plain-markdown
  version: **[api/llms.txt](https://dubpixel.github.io/dpx_tc002_frm/api/llms.txt)**.

  ### Prerequisites (for firmware development)
  * PlatformIO (VS Code extension or CLI)
  * Node.js 20+ (`npm ci` before first build)

  ### Installation

  1. Clone the repo and open in VS Code
  2. `npm ci && npm run build` — generate web UI headers
  3. Hit **Upload** (PlatformIO sidebar → `ulanzi_tc001`) or run `pio run -t upload -e ulanzi_tc001`
  4. On first boot the device writes its own config — see [First Boot Defaults](#first-boot-defaults) below

### Testing

A regression test suite lives in `tools/dpx_test.sh`. It runs API checks against a live device over HTTP and reports pass/fail. Visual display checks require a human watching the device.

```bash
# Fast automated-only (CI-safe, ~10s, no display pauses)
bash tools/dpx_test.sh --auto --fast 192.168.2.33

# Full suite with display checks (~2 min, watch the device)
bash tools/dpx_test.sh 192.168.2.33

# Single suite (iterate while working on a feature)
bash tools/dpx_test.sh --auto --fast --suite=overlay 192.168.2.33

# With reboot persistence test (~12 min)
bash tools/dpx_test.sh --reboot 192.168.2.33
```

Available suites: `connectivity` · `apps` · `notify` · `overlay` · `indicators` · `tc` · `sound` · `settings` · `icons` · `lint` · `persist`

`persist` is manual-only (gated behind `--reboot`, skipped by `--auto`/CI runs) — run it before a release, not routinely.

Requires: `curl`, `jq`, a device on the network.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- FIRST BOOT DEFAULTS -->
## First Boot Defaults

When you flash this firmware the device is **not stock WLED** — it boots into a pre-configured state. Here's what to expect:

### Connecting for the first time

1. Power the device
2. Look for WiFi network **`dpx-tc002-XXXXXX`** (not `WLED-AP`) — `XXXXXX` is unique per device (last 3 bytes of its MAC), so multiple unclaimed devices never collide
3. Password: **`dubpixel1`** (not `wled1234`)
4. Connect and go to **`http://4.3.2.1`** in a browser — the welcome page there also prints this device's exact name
5. Set your WiFi network under **WiFi Setup**
6. The device joins your network and is accessible at **`http://dpx-tc002-XXXXXX.local`** — same `XXXXXX` as the AP name you just joined

> This AP stays open indefinitely whenever the device is not connected to WiFi — no 5-minute timeout.

> **Note:** After powering on, allow **10–15 seconds** before scanning for the `dpx-tc002-XXXXXX` network. The AP comes up after the initial WiFi connection attempt completes. If the network doesn't appear immediately, wait a moment and scan again.

### Default hardware config (written once on first boot)

| Setting | Value | Notes |
|---|---|---|
| **AP SSID** | `dpx-tc002-XXXXXX` | `XXXXXX` = last 3 MAC bytes, lowercase hex — unique per device |
| **AP Password** | `dubpixel1` | |
| **mDNS** | `dpx-tc002-XXXXXX.local` | Same suffix as the AP SSID |
| **LED GPIO** | 32 | |
| **LED Count** | 256 | 32×8 matrix |
| **LED Type** | WS2812B GRB | |
| **2D Panel** | 32×8, non-serpentine | Toggle serpentine in WLED → LED Preferences |
| **Buttons** | GPIO 26 / 14 / 27 | Configurable macros in WLED |
| **Transitions** | 0 ms | Instant — better for text |
| **WiFi** | Not saved | Configure on first connect |
| **MQTT** | `mb.dubpixel.tv:1883`, no auth | Default broker — makes a fresh device claimable via [matrix_blaster](https://friendster.dubpixel.tv/about) out of the box; change in WLED → Sync → MQTT if not using it |

> Reflashing **preserves** your saved WiFi credentials. To reset to factory defaults, delete `/cfg.json` via WLED → File Manager, then reboot.

### Control interfaces

| Interface | How |
|---|---|
| **Web UI** | `http://dpx-tc002-XXXXXX.local` (device-specific, shown on `/welcome`) |
| **OSC** | UDP 4210 — `/dpx/notify`, `/dpx/tc`, `/dpx/app/<name>`, `/dpx/overlay`, `/dpx/effect` |
| **MQTT** | `{deviceTopic}/dpx/#` — same structure as OSC |
| **JSON** | `POST /json {"dpx":{...}}` — WLED standard JSON API |
| **Serial** | 115200 baud — prints IP on connect; send `s` for a status dump (IP, hostname, AP, WiFi, heap, current app, uptime, firmware version, build, MQTT/OSC state) — WLED's own `v` command only reports its internal build number, not our version |

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- USAGE EXAMPLES -->
## Usage

Send a notification via OSC:
```
/dpx/notify  {"text":"HELLO","color":"#FF0000","duration":5}
```

Send timecode via OSC (compatible with dpx_tc001 and AWTRIX senders):
```
/dpx/tc  01:23:45:12
```

Set a custom scrolling app via MQTT:
```
{deviceTopic}/dpx/app/mylabel  {"text":"LIVE","color":"#FF4400","duration":10}
```

Via WLED JSON API:
```json
POST /json
{"dpx": {"notify": {"text": "HELLO", "color": "#FF0000"}}}
```

See [First Boot Defaults](#first-boot-defaults) for all control interfaces.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- ICONS -->
## Icons & Media Browser

Not just an 8×8 pixel renderer — `/browse` on the device is a built-in browser across three icon/media sources, no external tools needed:

- **LaMetric Icons** — thousands of free icons from LaMetric's public gallery, searchable by ID, paginated thumbnail grid. Click **Get** to install one to `/ICONS/`, click **Use** to copy its ID straight into an `icon` field.
- **Bigtime GIFs** — Blueforcer/awtrix3's animated GIF collection, one-click install from GitHub.
- **On-Device Files** — a small file manager for `/ICONS/`, `/MELODIES/`, and device root (list, delete).

<details>
<summary>Screenshot</summary>

![Icon browser][ui-shot-browse]

</details>

Once installed, reference an icon by name in any notify/custom-app call:
```
curl -X POST http://[IP]/api/notify \
  -H "Content-Type: application/x-www-form-urlencoded" \
  --data-urlencode 'plain={"text":"Hi!","icon":"87","pushIcon":0}'
```

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- OSC CONTROL -->
## OSC Control

The firmware's OSC receiver (UDP 4210) accepts timecode, notifications, app switching, brightness, and indicator colors — see the [full OSC address list](https://dubpixel.github.io/dpx_tc002_frm/api/#osc) in the API docs. Both bare addresses (`/notify`, `/tc`) and namespaced ones (`/dpx_tc002/notify`, `/awtrix/notify`) are accepted.

### `tools/ltc_osc_bridge/` — LTC timecode & show-control bridge

A standalone Python tool (CLI + local web GUI) that goes well beyond just feeding this device timecode:

- **LTC → OSC** — decodes real SMPTE linear timecode from a system audio input (or a WAV file) and forwards it to any OSC receiver, this device included
- **Test/generator mode** — sends a fake incrementing timecode with no LTC hardware needed (`--test`), for trying things out before you have real audio gear connected
- **Arbitrary OSC sender** — `--send "01:00:00:00" --address /dpx_tc001/notify` fires any string to any address, useful for scripting/testing without touching the LTC decoder at all
- **disguise (d3) show-control** — a second feature area, separate from LTC: play/stop, cue triggering, volume/brightness, track name/id, and a listener registry for d3's own OSC feedback — `--d3-play`, `--d3-cue 1 2 5`, `--d3-volume 0.8`, etc.
- **Web GUI** — `python ltc_osc_bridge_gui.py` opens a two-tab local web app at `http://localhost:8765` (LTC→OSC / d3 Control) — no CLI flags to remember. Mac/Windows launcher scripts included (`Launch LTC Bridge.command` / `.bat`) for double-click use.

```bash
cd tools/ltc_osc_bridge
pip install -r requirements.txt

python ltc_osc_bridge.py --list-devices                                    # find your audio input
python ltc_osc_bridge.py --target 192.168.1.100 --device 2                 # decode real LTC audio
python ltc_osc_bridge.py --test --start-tc 01:00:00:00 --fps 25 --target 192.168.1.100
python ltc_osc_bridge_gui.py                                               # or just use the GUI
```

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- ROADMAP -->
## Roadmap

- [x] Button macros (prev/next app, dismiss notification, show IP)
- [x] LDR-based auto brightness (ABRI, v0.5.1)
- [x] Temperature/humidity sensor display (I²C, v0.5.1) — see [#86](https://github.com/dubpixel/dpx_tc002_frm/issues/86) for calibration follow-up
- [x] Optional PIN lock for the live-control API (`CTRL_LOCK`, v0.6.0) — see [#88](https://github.com/dubpixel/dpx_tc002_frm/issues/88)
- [ ] Preset integration — trigger dpx apps from WLED presets
- [ ] 2D effect groups in web UI
- [ ] Social/community features via the companion server (likes, reactions, that kind of thing) — speculative, low priority

See the [open issues](https://github.com/dubpixel/dpx_tc002_frm/issues) for a full list of proposed features and known issues.

<!-- CONTRIBUTING -->
## Contributing

_Contributions are what make the open source community such an amazing place to learn, inspire, and create. Any contributions you make are **greatly appreciated**._

If you have a suggestion that would make this better, please fork the repo and create a pull request. You can also simply open an issue with the tag "enhancement".
Don't forget to give the project a star! Thanks again!

1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/AmazingFeature`)
3. Commit your Changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the Branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

### Top contributors:
<a href="https://github.com/dubpixel/dpx_tc002_frm/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=dubpixel/dpx_tc002_frm" alt="contrib.rocks image" />
</a>

<!-- LICENSE -->
## License

This project is a fork of [WLED](https://github.com/wled/WLED) and is distributed under the **[EUPL v1.2](https://github.com/dubpixel/dpx_tc002_frm/blob/main/LICENSE)** (European Union Public Licence).

Key points:
- **Source must be provided** with any distribution — this GitHub repo fulfils that requirement
- Derivative works must be licensed under EUPL or a [compatible license](https://joinup.ec.europa.eu/collection/eupl/matrix-eupl-compatible-open-source-licences) (GPL, LGPL, AGPL, MPL, etc.)
- Attribution to WLED contributors is required — see [kno.wled.ge/about/contributors](https://kno.wled.ge/about/contributors/)

The `dpx_matrix` usermod is original work by dubpixel, but as part of this compiled firmware distribution it is covered by EUPL v1.2.

AwtrixFont (`dpx_font.h`) is BSD 3-Clause — see file header for full attribution.
<!-- CONTACT -->
## Contact

  ### Joshua Fleitell — i@dubpixel.tv

  Project Link: [https://github.com/dubpixel/dpx_tc002_frm](https://github.com/dubpixel/dpx_tc002_frm)

<!-- ACKNOWLEDGMENTS -->
## Acknowledgments

* [WLED](https://github.com/wled/WLED) — the firmware base. Originally created by [Aircoookie](https://github.com/Aircoookie), now community-maintained. EUPL v1.2. Full [contributor credits at kno.wled.ge](https://kno.wled.ge/about/contributors/).
* [AwtrixFont](https://github.com/Blueforcer/awtrix3) — TomThumb-derived 3×5 pixel font by Blueforcer et al. BSD 3-Clause.
* [Ulanzi TC001](https://www.ulanzi.com/products/ulanzi-pixel-smart-clock-2882) — the hardware platform.
* [NeoPixelBus](https://github.com/Makuna/NeoPixelBus) — addressable LED driver library by Makuna.
* [ESPAsyncWebServer](https://github.com/Aircoookie/ESPAsyncWebServer) — async HTTP server for ESP32/ESP8266.
* [AsyncMqttClient](https://github.com/marvinroger/async-mqtt-client) — MQTT client by marvinroger.
* [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266) — infrared remote library.
* [ESP-DMX](https://github.com/someweisguy/esp_dmx) — DMX input/output library by someweisguy.
* [CORS proxy](https://corsfix.com/) by Corsfix — used by WLED's web UI for external API calls.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- MARKDOWN LINKS & IMAGES -->
[contributors-shield]: https://img.shields.io/github/contributors/dubpixel/dpx_tc002_frm.svg?style=flat-square
[contributors-url]: https://github.com/dubpixel/dpx_tc002_frm/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/dubpixel/dpx_tc002_frm.svg?style=flat-square
[forks-url]: https://github.com/dubpixel/dpx_tc002_frm/network/members
[stars-shield]: https://img.shields.io/github/stars/dubpixel/dpx_tc002_frm.svg?style=flat-square
[stars-url]: https://github.com/dubpixel/dpx_tc002_frm/stargazers
[issues-shield]: https://img.shields.io/github/issues/dubpixel/dpx_tc002_frm.svg?style=flat-square
[issues-url]: https://github.com/dubpixel/dpx_tc002_frm/issues
[license-shield]: https://img.shields.io/github/license/dubpixel/dpx_tc002_frm.svg?style=flat-square
[license-url]: https://github.com/dubpixel/dpx_tc002_frm/blob/main/LICENSE
[linkedin-shield]: https://img.shields.io/badge/-LinkedIn-black.svg?style=flat-square&logo=linkedin&colorB=555
[linkedin-url]: https://linkedin.com/in/jfleitell
[product-front]: images/front.png
[product-rear]: images/rear.png
[product-front-rendering]: images/front_render.png
[product-rear-rendering]: images/rear_render.png
[action-shot-1]: images/action_shot_placeholder.svg
[action-shot-2]: images/action_shot_placeholder_2.svg
[ui-shot-ctrl]: images/ui_shot_ctrl.png
[ui-shot-apiref]: images/ui_shot_apiref.png
[ui-shot-browse]: images/ui_shot_browse.png
[WLED-badge]: https://img.shields.io/badge/WLED-17.0.0--dev-blue?style=flat-square
[WLED-url]: https://github.com/wled/WLED
[PlatformIO-badge]: https://img.shields.io/badge/PlatformIO-ESP32-orange?style=flat-square
[PlatformIO-url]: https://platformio.org
[ESP32-badge]: https://img.shields.io/badge/ESP32--WROOM--32D-240MHz-green?style=flat-square
[ESP32-url]: https://www.espressif.com/en/products/socs/esp32
