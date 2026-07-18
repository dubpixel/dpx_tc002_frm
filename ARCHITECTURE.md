# dpx_tc002 — Architecture & Decisions

**Date:** 2026-07-17
**Status:** Firmware repo pending setup

---

## Repo Structure

| Repo | What it is |
|------|------------|
| `dpx_tc002` | Product repo — hardware, docs, tools, this file |
| `dpx_tc002_frm` | Firmware — fork of `wled/WLED` (MIT), separate GitHub repo |
| `dpx_tc001/dpx_reference` | Reference only — AWTRIX-derived, do not copy license-unsafe files |

`dpx_tc002_frm` is a clean fork of WLED mainline (`wled/WLED`, `main` branch). No nested git repos, no submodules.

---

## Why WLED Fork (not AWTRIX)

dpx_tc001 was a fork of AWTRIX 3 (CC BY-NC-SA 4.0) — that license blocks commercial use.
dpx_tc002_frm is a WLED fork (MIT) — commercially clean, no share-alike requirement.

### Why mainline WLED and not MoonModules/WLED-MM

MoonModules adds 2D effects and experimental features, but dpx_tc002 writes its own display layer on top. The extra effect catalog adds maintenance burden with no benefit. Mainline 2D matrix support is sufficient for a 32×8 pixel array.

---

## What dpx_tc002_frm Will Contain

Full WLED source with one custom usermod added:

**`usermods/dpx_matrix/`** — original work, MIT:
- 5×7 pixel bitmap font (new — NOT the AWTRIX font)
- Text rendering + scrolling (`renderText`, `renderScroll`)
- App loop system (named display slots, auto-rotation timer)
- Notification queue (interrupts loop, auto-dismisses)
- OSC receiver on UDP port 4210 — d3 disguise integration, `/tc` timecode
- HTTP API matching dpx_tc001 contract (`/api/custom`, `/api/notify`, `/api/tc`, etc.)
- Web UI pages served from PROGMEM: `/ctrl`, `/browse`, `/api-ref`
- TC display with frame-accurate progress bar (two render modes)
- 3 indicator pixels (corner dots, addressable via `/api/indicator1/2/3`)
- dev.json persistence (temp offsets, LDR config, TC dwell settings)
- OSC Listener Registry (d3 monitoring path → display channel mappings)

---

## What Gets Stripped / Disabled

Handled via compile-time flags in `platformio.ini` — no source deletion needed:

```ini
-D WLED_DISABLE_ALEXA
-D WLED_DISABLE_LOXONE
-D WLED_DISABLE_INFRARED
-D WLED_DISABLE_HUESYNC
-D WLED_DISABLE_ADALIGHT
-D WLED_DISABLE_ESPNOW
```

Most of the 100+ WLED effects can be pruned later if flash is tight. The WLED stock web UI (`wled00/data/`) is superseded by the custom pages served from the usermod — keep it in but redirect `/` to `/ctrl`.

---

## Hardware Target — Ulanzi TC001

ESP32-WROOM-32D · 240MHz · 4MB flash (1.75MB app + 256KB LittleFS) · CH340 USB-serial

| GPIO | Function |
|------|----------|
| 32 | LED matrix data (256× WS2812B, 32 cols × 8 rows) |
| 26 | Left button (active low) |
| 27 | Middle button (active low, inverted) |
| 14 | Right button (active low) |
| 15 | Passive buzzer — RTTTL via PWM |
| 34 | Battery ADC (read-only voltage divider) |
| 35 | LDR — auto-brightness (GL5516) |
| 21 / 22 | I²C SDA / SCL — SHT3x temp + humidity (addr 0x44) |
| 23 / 18 | DFPlayer Mini RX / TX (optional) |

**Matrix layout:** row-major, left-to-right, top-to-bottom. Pixel index = `row * 32 + col`.

---

## License Boundaries

**Do NOT copy (AWTRIX-derived, CC BY-NC-SA 4.0):**
- `AwtrixFont.h`
- `MatrixDisplayUi.cpp`
- `MQTTManager.cpp`
- The `/setup` page from `htmls.h`

**Safe to port (original work from dpx_tc001 session):**
- `tools/ltc_osc_bridge/` — copy verbatim, 100% original Python, MIT-clean
- `/ctrl`, `/browse`, `/api-ref` HTML pages from `htmls.h`
- OSC `handleOSC()` logic from `ServerManager.cpp`

---

## Next Steps

1. Fork `wled/WLED` → `dubpixel/dpx_tc002_frm` on GitHub, clone locally
2. Update `dpx_tc002.code-workspace` to include `dpx_tc002_frm` as the firmware folder
3. Verify WLED builds clean: `npm ci && npm run build && pio run -e esp32dev`
4. Create `usermods/dpx_matrix/` skeleton (header files only, empty stubs)
5. Configure `platformio.ini` env for Ulanzi TC001 with `custom_usermods = dpx_matrix`
6. Build Phase 1 — font + text rendering

See `⊘ dpx_reference/dpx_tc002.md` for the full phased build plan (Phases 1–8).
