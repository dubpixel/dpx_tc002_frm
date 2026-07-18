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
<h3 align="center"><i>a sassy project tag line here</i></h3>
  <p align="center">
    ...a short description to tease interest
        <br />
     »  
     <a href="https://github.com/dubpixel/dpx_tc002_frm/tree/main/"><strong>Project Here!</strong></a>
     »  
    <a href="https://github.com/dubpixel/dpx_tc002_frm/tree/main/hardware/src/dpx_tc002_frm/bom"><strong>BOM Here!</strong></a>
     » 
    <a href="https://dubpixel.github.io/dpx_tc002_frm/ibom/index.html"><strong>Interactive BOM Here!</strong></a>
     <br />
    <a href="https://github.com/dubpixel/dpx_tc002_frm/issues/new?labels=bug&template=bug-report---.md">Report Bug</a>
    ·
    <a href="https://github.com/dubpixel/dpx_tc002_frm/issues/new?labels=enhancement&template=feature-request---.md">Request Feature</a>
    </p>
</div>
   <br />
<!-- TABLE OF CONTENTS -->
<details>
  <summary><h3>Table of Contents</h3></summary>
<ol>
    <li><a href="#about-the-project">About The Project</a></li>
    <li><a href="#getting-started">Getting Started</a></li>
    <li><a href="#first-boot-defaults">First Boot Defaults</a></li>
    <li><a href="#usage">Usage</a></li>
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

**dpx_tc002_frm** is a custom WLED firmware build for the [Ulanzi TC001](https://www.ulanzi.com/products/ulanzi-pixel-smart-clock-2882) pixel clock — a battery-powered ESP32 device with a 32×8 WS2812B LED matrix. It replaces the stock Awtrix firmware with a WLED base plus the `dpx_matrix` usermod, adding timecode display, OSC/MQTT control, scrolling text apps, pixel overlay effects, and first-boot hardware configuration.

Built on top of [WLED](https://github.com/wled/WLED) (EUPL v1.2) — all WLED features remain fully functional.

### What WLED brings

- **200+ built-in LED effects** including 2D/matrix effects, audio-reactive, and palettes
- **Segments** — independent effects/colors on parts of the strip simultaneously
- **Up to 250 presets** with playlist support
- **JSON + HTTP APIs**, MQTT with Home Assistant discovery, E1.31/Art-Net/DDP
- **Multi-WiFi** with automatic AP fallback, OTA firmware updates
- **NTP time sync** with full timezone + DST support
- Full [WLED documentation at kno.wled.ge](https://kno.wled.ge)
- [WLED mobile app](https://kno.wled.ge/basics/getting-started/) for Android and iOS

### What dpx_matrix adds

- Scrolling/static **text app loop** (Time, Date, custom apps) with AwtrixFont
- **Timecode display** (LTC via OSC) with frame progress bar
- **Notifications** — one-shot priority messages
- **Text overlay** + pixel effects (sparkle, strobe, rain, twinkle, blink) on top of any WLED effect
- **OSC receiver** (UDP 4210) — compatible with dpx_tc001 and AWTRIX OSC senders
- **MQTT** via WLED's broker connection
- **First-boot config** — device is usable out of the box, no wizard required

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
</details>
<p align="right">(<a href="#readme-top">back to top</a>)</p>

### Built With

* [![WLED][WLED-badge]][WLED-url]
* [![PlatformIO][PlatformIO-badge]][PlatformIO-url]
* [![ESP32][ESP32-badge]][ESP32-url]
<p align="right">(<a href="#readme-top">back to top</a>)</p>
<!-- GETTING STARTED -->

## Getting Started

  ### Prerequisites
  * PlatformIO (VS Code extension or CLI)
  * Node.js 20+ (`npm ci` before first build)

  ### Installation

  1. Clone the repo and open in VS Code
  2. `npm ci && npm run build` — generate web UI headers
  3. Hit **Upload** (PlatformIO sidebar → `ulanzi_tc001`) or run `pio run -t upload -e ulanzi_tc001`
  4. On first boot the device writes its own config — see [First Boot Defaults](#first-boot-defaults) below

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- FIRST BOOT DEFAULTS -->
## First Boot Defaults

When you flash this firmware the device is **not stock WLED** — it boots into a pre-configured state. Here's what to expect:

### Connecting for the first time

1. Power the device
2. Look for WiFi network **`dpx-tc002`** (not `WLED-AP`)
3. Password: **`dubpixel1`** (not `wled1234`)
4. Connect and go to **`http://4.3.2.1`** in a browser
5. Set your WiFi network under **WiFi Setup**
6. The device joins your network and is accessible at **`http://dpx-tc002.local`**

> This AP stays open indefinitely whenever the device is not connected to WiFi — no 5-minute timeout.

### Default hardware config (written once on first boot)

| Setting | Value | Notes |
|---|---|---|
| **AP SSID** | `dpx-tc002` | |
| **AP Password** | `dubpixel1` | |
| **mDNS** | `dpx-tc002.local` | |
| **LED GPIO** | 32 | |
| **LED Count** | 256 | 32×8 matrix |
| **LED Type** | WS2812B GRB | |
| **2D Panel** | 32×8, non-serpentine | Toggle serpentine in WLED → LED Preferences |
| **Buttons** | GPIO 26 / 14 / 27 | Configurable macros in WLED |
| **Transitions** | 0 ms | Instant — better for text |
| **WiFi** | Not saved | Configure on first connect |

> Reflashing **preserves** your saved WiFi credentials. To reset to factory defaults, delete `/cfg.json` via WLED → File Manager, then reboot.

### Control interfaces

| Interface | How |
|---|---|
| **Web UI** | `http://dpx-tc002.local` |
| **OSC** | UDP 4210 — `/dpx/notify`, `/dpx/tc`, `/dpx/app/<name>`, `/dpx/overlay`, `/dpx/effect` |
| **MQTT** | `{deviceTopic}/dpx/#` — same structure as OSC |
| **JSON** | `POST /json {"dpx":{...}}` — WLED standard JSON API |
| **Serial** | 115200 baud — prints IP on connect |

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

<!-- ROADMAP -->
## Roadmap

- [ ] Button macros (prev/next app, dismiss notification, show IP)
- [ ] LDR-based auto brightness
- [ ] Temperature/humidity sensor display (I²C)
- [ ] Preset integration — trigger dpx apps from WLED presets
- [ ] 2D effect groups in web UI

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
This firmware is distributed under the **EUPL v1.2** (inherited from WLED). See `LICENSE` for details.

The `dpx_matrix` usermod (original work) is additionally available under MIT.

AwtrixFont used in dpx_font.h is BSD 3-Clause — see header for full attribution.
<!-- CONTACT -->
## Contact

  ### Joshua Fleitell — i@dubpixel.tv

  Project Link: [https://github.com/dubpixel/dpx_tc002_frm](https://github.com/dubpixel/dpx_tc002_frm)

<!-- ACKNOWLEDGMENTS -->
## Acknowledgments

* [WLED](https://github.com/wled/WLED) — the firmware base. Originally by [Aircoookie](https://github.com/Aircoookie), now community-maintained. EUPL v1.2.
* [AwtrixFont](https://github.com/Blueforcer/awtrix3) — TomThumb-derived 3×5 pixel font by Blueforcer et al. BSD 3-Clause.
* [Ulanzi TC001](https://www.ulanzi.com/products/ulanzi-pixel-smart-clock-2882) — the hardware platform.

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
[WLED-badge]: https://img.shields.io/badge/WLED-17.0.0--dev-blue?style=flat-square
[WLED-url]: https://github.com/wled/WLED
[PlatformIO-badge]: https://img.shields.io/badge/PlatformIO-ESP32-orange?style=flat-square
[PlatformIO-url]: https://platformio.org
[ESP32-badge]: https://img.shields.io/badge/ESP32--WROOM--32D-240MHz-green?style=flat-square
[ESP32-url]: https://www.espressif.com/en/products/socs/esp32
