# dpx_tc002_frm — Master Roadmap & TODO

Last updated: 2026-08-20

> **Since the last update (v0.2.0):** icon rendering, WLED pattern slots (real
> WLED FX in the app rotation), multi-instance custom apps, customizable clock
> instances, 2x font scale, device-claim PIN pairing, MQTT icon fetch, and
> browser-flashable firmware (ESP Web Tools, auto-deployed to GitHub Pages) all
> shipped. This pass updates the checkboxes below to match — several items in
> Phase 1/2/4 were marked incomplete but are actually done.

Reference files (read before working on this project):
- `⊘ dpx_reference/SPEC.md` — full behavioral spec, API contract, all JSON keys
- `⊘ dpx_reference/dpx_tc002.md` — firmware build plan, phase order, GPIO map
- `dpx_friendster/docs/dpx_tc002_server.md` — Friendster/cueMaestro server plan (moved
  out of `dpx_reference/` into the sibling `dpx_friendster` repo — clone it alongside
  this one for the path to resolve)

---

## Quick Reference

**MQTT topic for text:** `[PREFIX]/notify` → `{"text":"hello","rainbow":true,"duration":8}`
PREFIX = device hostname. Read from `GET /api/settings` → `MQTT_PREFIX`.

**Server project (Friendster/cueMaestro):** separate Python FastAPI repo, build after firmware is stable.
See Part 2 of this file.

---

## Part 1 — Firmware (dpx_tc002_frm)

### Phase 1 — Bug Fixes ✅ DONE (2026-07-18)
- [x] **1.1** Overlay name case mismatch — overlay dropdowns in `/ctrl` sent uppercase (`"RAIN"`) but firmware matched lowercase. Fixed: JS option values now lowercase; firmware already does `toLowerCase()`.
- [x] **1.2** 5 missing overlay effects — `snow`, `drizzle`, `storm`, `thunder`, `frost` all implemented in `dpx_overlay.h`
- [x] **1.3** `icon` and `pushIcon` JSON fields not parsed — added to `DpxCustomApp` struct and `dpxParseApp()` in `dpx_apps.h`. Rendering is Phase 2.
- [x] **1.4** `deleteCustomApp()` JS bug — was calling `.filter()` on `{name:index}` object. Removed broken reorder logic; now just refreshes the loop list.
- [x] **1.5** Native app toggles (`TIM`/`DAT`) — `POST /api/settings` now handles `TIM` and `DAT` keys; `DPX_SHOW_TIME` / `DPX_SHOW_DATE` flags control `dpxRebuildLoop()`.
- [x] **1.6** `/api/transitions` stub — was returning `[]`; now returns `["fade","slide"]`
- [x] **1.7** `/api/sleep` not implemented — added ESP32 deep sleep with optional timer wakeup (`{"sleep":N}`)
- [x] **1.8** `save` flag — `save: true` in custom app JSON now persists to `/CUSTOMAPPS/<name>.json` on LittleFS; delete also removes the file
- [x] **1.9** Per-app overlay activation — originally added to `handleOverlayDraw()`; **see 1.11 below — needs re-port to `mode_dpx_matrix()`**
- [x] **1.10** Serial config dump `dpx_matrix.h` — done (GH #10). Not yet visually
  verified over a real USB serial connection this cycle (no cable available), only
  build-verified — flag for a hardware check next time a cable's on hand.

- [x] **1.11** Port per-app overlay activation to `mode_dpx_matrix()` `dpx_matrix.h`
  - **Context:** the 1.9 activation hook was written for the old `handleOverlayDraw()` which no longer exists.
    dpx_matrix is now a proper WLED effect — the effect function `mode_dpx_matrix()` is the only render path.
  - Add a static `_prevAppEffect` tracker inside `mode_dpx_matrix()` (same logic as the old hook):
    check `dpxCurrentNotif.data.overlay` or `dpxApps[dpxCurrentApp].data.overlay`, and call
    `dpxSetPixelEffect()` / `dpxClearPixelEffect()` when the app changes.

- [x] **1.12** Fix overlay effects using `strip.setPixelColor()` → `SEGMENT` `dpx_overlay.h`
  - **Context:** dpx_matrix is now a WLED effect. All pixel writes must go through `SEGMENT.setPixelColorXY()`
    so brightness, power-fade, and transitions work correctly. `dpx_text.h` already does this.
    `dpx_overlay.h` was not updated — sparkle, twinkle, strobe, blink, storm, thunder, frost all call
    `strip.setPixelColor()` / `strip.getPixelColor()` directly, bypassing the brightness pipeline.
  - Replace all `strip.setPixelColor(i, ...)` → `SEGMENT.setPixelColorXY(i % 32, i / 32, ...)`
  - Replace all `strip.getPixelColor(i)` → `SEGMENT.getPixelColorXY(i % 32, i / 32)`
  - Full-matrix blends (strobe, storm flash, frost, thunder) must iterate via XY coordinates.

- [x] **1.13** Audit `DPX_SHOW_TIME` / `DPX_SHOW_DATE` vs `dpxHiddenApps` `dpx_persist.h` + `dpx_api.h`

- [ ] **1.14** TC mute toggle ✅ *implemented in feature/overlay-tc-fixes*

- [ ] **1.15** TC takeover forces dpx_matrix effect ✅ *implemented in feature/overlay-tc-fixes*

- [x] **1.16** WLED pattern slots in app rotation `dpx_apps.h` + `dpx_html.h` — done
  (GH #11). A real WLED FX effect can occupy a rotation slot; JSON schema
  `{"type":"wled_fx","effect":53,"palette":0,"speed":128,"intensity":128,"dur":10}`
  matches the original design exactly. (A regression where the effect never
  actually activated on create/update was found and fixed post-ship — GH #67.)

- [x] **1.17** Overlay additive compositing (text shows through) `dpx_overlay.h` — done (GH #12, hardware-verified).

- [x] **1.18** Persistent pixel state buffer for rain/snow/storm `dpx_overlay.h` — done.

- [x] **1.19** Storm wind effect `dpx_overlay.h` — done.

- [ ] **1.20** Background effects system (full-screen animated canvases) `dpx_overlay.h`
  - **Status:** Partially resolved without new code — 1.16 (pattern slots) lets any WLED
    FX (Matrix, Fireworks, Ripple, Plasma, Pacifica, etc.) occupy a rotation slot, matching
    the "routing through WLED's FX engine" option below. What's still genuinely unbuilt is
    a *literal* WLED effect rendering simultaneously with text on the same frame — WLED only
    calls one mode function per segment per frame, so that needs real work (tracked as GH #14,
    explicitly deferred: "we can do 14 later"). The existing hand-written overlay effects
    (colorwaves/plasma/twinklingstars/theatrechase/pacifica in `dpx_overlay.h`) already
    achieve the visual goal via additive compositing and don't need this.
  - **Context:** AWTRIX3 has a separate `effect` JSON field (distinct from `overlay`) for full-screen
    animated effects that render *behind* text: Fireworks, TwinklingStars, Matrix code rain, Ripple,
    Pacifica ocean, PlasmaCloud, PingPong, Radar, ColorWaves, etc.
  - **Do NOT port AWTRIX3 code** — their license is non-commercial. Write equivalent effects from
    scratch in `dpx_overlay.h` using the SEGMENT API, using different implementations/algorithms.
  - First, audit WLED's built-in effects (`GET /json/effects`) — WLED already has Matrix, Fireworks,
    Ripple, Plasma, Pacifica, and many others as full FX. Consider whether routing through WLED's
    FX engine (via 1.16 pattern slots) is sufficient before writing parallel implementations.
  - If WLED FX are used, the `effect` JSON field on a custom app could just trigger a brief WLED FX
    slot during that app's dwell, then return to dpx_matrix. May make 1.20 mostly free via 1.16.
  - If custom effects are warranted (e.g., effects that need to composite with text), add a new
    `DpxBgEffect` struct and `dpxBgEffect` state alongside `dpxPixelEffect`.
  - Candidate effects to implement if needed: TwinklingStars, Fireworks, ColorWaves, Radar.

---

### Architecture Note — dpx_matrix as WLED Effect (2026-07-18)

> dpx_matrix is **not** a handleOverlayDraw hack anymore. It is registered as a proper WLED effect
> via `strip.addEffect(255, &mode_dpx_matrix, "dpx Matrix;!;2")` and the main segment is set to
> it on startup. This means:
>
> - Brightness slider, power-fade, and segment transitions all work natively — no manual BRI mapping
> - `mode_dpx_matrix()` is the single render entry point (replaces `handleOverlayDraw()`)
> - All pixel writes must use `SEGMENT.setPixelColorXY(x, y, color)` not `strip.setPixelColor()`
> - PixelForge (FX #53, FX #122) runs on the same segment when the user switches effects via WLED UI;
>   `dpxActivateEffect()` re-engages dpx_matrix when an app is pushed via the API
> - The "WLED passthrough" app slot is gone — not needed since dpx_matrix IS the segment effect
>
> **dpx_matrix should remain a full display manager, not a thin wrapper around FX #122 (Scrolling Text).**
> FX #122 only handles scrolling text; dpx_matrix does the app loop, MQTT dispatch, OSC routing,
> notification queue, TC display, and icon rendering. The two coexist — FX #122 is available to the
> user via PixelForge when they want it.

---

### Phase 1.5 — `/ctrl` UI Refactor `feature/ctrl-ui-refactor`

> The current `/ctrl` page (`dpx_html.h`) is ~90 KB of ported AWTRIX HTML/JS that references
> AWTRIX endpoints which no longer exist. Nothing connects to the actual dpx_matrix API.
> Needs a complete replacement.

**What to remove:** all AWTRIX-derived HTML in `dpx_html.h`; dead routes in `dpx_api.h`.

**New `/ctrl` (vanilla JS, no framework, target <50 KB uncompressed):**

- [ ] **1.5.1** App channel strip — `GET /dpx/apps` list; Go / Mute / Remove per row; add-back hidden natives; add custom app form
- [ ] **1.5.2** Notification panel — one-shot send + dismiss
- [ ] **1.5.3** Overlay / effects panel — text overlay fields; pixel effect picker + intensity
- [ ] **1.5.4** Device status strip — IP, SSID, RSSI, heap, uptime from `GET /dpx`
- [ ] **1.5.5** API reference — regenerated from actual routes/JSON keys; replaces AWTRIX stub

---

### Phase 1.8 — Hardware Sensor Reads `feature/sensors`

> TC001 hardware that needs firmware support before native Temp / Humidity / Battery / ABRI apps can be enabled.
> Hardware refs: `SPEC.md` §2 GPIO table.

**Hardware available:**
- **SHT3x** — I²C addr 0x44, GPIO21 SDA / GPIO22 SCL — temperature + humidity
  - WLED already ships a `BME280` usermod; SHT3x can follow the same pattern via Wire.h directly or hook the existing usermod
- **GPIO34** — battery ADC (read-only voltage divider, 3.3 V ref) → map voltage to 0–100%
- **GPIO35** — GL5516 LDR → auto-brightness (ABRI)

- [ ] **1.8.1** SHT3x sensor read — new `dpx_sensors.h`
  - Init I²C on GPIO21/22; read SHT3x at addr 0x44 every 10 s
  - Globals: `float DPX_TEMP`, `float DPX_HUM`; guarded by `#ifdef ARDUINO_ARCH_ESP32`
  - Expose as `"temp"` and `"hum"` in `dpxStatsJson()` (via `GET /dpx` and `GET /api/stats`)
  - Apply `DPX_TEMP_OFFSET` / `DPX_HUM_OFFSET` from `dev.json`
  - Add `CEL` to `dpxApplySettings()` and `dpxSettingsJson()`
  - Add temp_offset / hum_offset to `/api/dev` handling

- [ ] **1.8.2** Battery ADC — `dpx_sensors.h`
  - `analogRead(34)` → voltage divider → battery % (map 3.0–4.2 V → 0–100%)
  - Expose as `"bat"` (int 0–100) in `dpxStatsJson()`
  - **Alt: use upstream `Battery` usermod** — add `Battery` to `custom_usermods` and set
    `-D USERMOD_BATTERY_MEASUREMENT_PIN=34` in build_flags (default is GPIO35 = LDR, must override).
    Gives auto-off threshold, low-power indicator, and WLED info page integration for free.
    Trade-off: less control over `"bat"` JSON key in `dpxStatsJson()` without extra glue code.

- [ ] **1.8.3** ABRI — auto-brightness via LDR
  - `analogRead(35)` → lux estimate → `bri` target; ramp smoothly via `stateUpdated()`
  - Add `ABRI` bool to `dpxApplySettings()` and `dpxSettingsJson()`
  - Expose raw LDR value as `"lux"` in `dpxStatsJson()`

- [ ] **1.8.4** Wire up native apps
  - Add `Temperature`, `Humidity`, `Battery` to `natives[]` in `dpx_apps.h`
  - Render via `dpxRenderNativeTemp()`, `dpxRenderNativeHum()`, `dpxRenderNativeBat()`
  - Toggle via `TEMP`, `HUM`, `BAT` settings keys in `dpxApplySettings()`

- [ ] **1.8.5** UI — restore Sensors card in `/ctrl`
  - Re-add Sensors card (C/F toggle, temp offset slider, humidity offset slider)
  - Enable Temp / Humidity / Battery native app toggles (remove disabled state)
  - Show `temp`, `hum`, `bat`, `lux` in the status strip from `GET /dpx`
  - Add `ABRI` checkbox to Display card

---

### Phase 1.9 — WLED Pattern Slots in App Rotation ✅ DONE (duplicate of 1.16 above, GH #11)

---

### Phase 2 — Icon Rendering ✅ DONE (GH #16)

> Shipped via browser-side PNG→raw conversion (no on-device PNG decoder needed),
> exactly per the original plan below. `dpx_icons.h` exists and does 2.2/2.3 as speced.

- [x] **2.1** Browser-side PNG→raw conversion `dpx_html.h`
- [x] **2.2** Icon load + render — `dpx_icons.h`
- [x] **2.3** Text layout with icon `dpx_apps.h` + `dpx_text.h`

---

### Phase 3 — Animated GIF Playback

> **First:** check if WLED's existing GIF engine (`wled00/image_loader.cpp`, `AnimatedGIF` library already linked via `-D WLED_ENABLE_GIF`) can be hooked directly — avoid writing a second GIF decoder.

- [ ] **3.1** Investigate WLED GIF hook
  - Can `image_loader.cpp` render a GIF to an arbitrary pixel buffer (not just WLED segments)?
  - If yes, use it. If no, use `AnimatedGIF` library directly in `dpx_gif.h`.

- [ ] **3.2** Frame extraction `dpx_apps.h` or new `dpx_gif.h`
  - AWTRIX convention: 32×8 GIF = N frames of 8×8, laid out horizontally
  - Slice frames by x-offset using GIF frame metadata

- [ ] **3.3** Playback state machine `dpx_apps.h`
  - Frame index, loop control, GIF frame delay timing
  - Trigger: if `icon` field ends in `.gif`, enter GIF playback mode

- [ ] **3.4** Render path
  - GIF frame renders to cols 0–7 (same position as static icon)
  - Text continues in cols 8–31 alongside

---

### Phase 4 — Font Scaling / Larger Font ✅ DONE (GH #19, #63)

> Shipped via the 4.2 "textScale:2 pixel-doubling" option (simpler, no new font
> bitmap needed) rather than 4.1's second-font approach. `fontScale`/`"font":"large"`
> JSON field, threaded through `dpxDrawChar`/`dpxRenderText`/`dpxTextPixelWidth`.
> **Known risk, not yet visually confirmed:** most `AwtrixFont` glyphs are 5px
> tall, so 2x scale (10px) clips against the 8-row matrix — defaults to 1x
> (safe) with 2x opt-in until checked on hardware.

- [x] **4.2** Font selector `dpx_apps.h` — `fontScale` JSON field
- [x] **4.3** UI control `dpx_html.h` — font size selector on Notification and Custom App cards

---

### Phase 5 — Font Pixels as WLED Effects

> Most architecturally complex item. Design the bounding-box layer carefully before building.

- [ ] **5.1** Bounding box tracking `dpx_text.h`
  - During glyph render, record `{x, y, w, h}` per character into a `CharBBox[]` out-param
  - Only computed when `textEffect` field is present (no perf cost on normal renders)

- [ ] **5.2** Per-region effect application `dpx_overlay.h`
  - `dpxApplyEffectToRegion(effect, x, y, w, h)` — applies an effect only to pixels within bbox
  - Use existing WLED effect functions on a local pixel sub-region

- [ ] **5.3** JSON field + dispatch `dpx_apps.h`
  - Add `"textEffect":"rainbow"` (global) or per-fragment in colored text arrays
  - Map effect name strings to implementations

---

### Phase 6 — cpt-city Gradient Integration

> WLED already has 58+ cpt-city-sourced gradients in `wled00/palettes.cpp`. The custom palette system in `wled00/colors.cpp` → `loadCustomPalettes()` loads JSON from LittleFS. Palette editor at `/cpal/cpal.htm` already exists.

- [ ] **6.1** Curated offline bundle
  - Pick 20–30 popular cpt-city gradients NOT already in `wled00/palettes.cpp`
  - Convert `.cpt` format → WLED gradient byte arrays via a Node.js build script in `tools/`
  - Append to `wled00/palettes.cpp` alongside existing set

- [ ] **6.2** Palette editor label `wled00/data/cpal/cpal.htm` (optional)
  - Label the new cpt-city additions in a distinct group in the palette picker

---

### Phase 7 — LaMetric Icon Generator Tool

- [ ] **7.1** 8×8 pixel editor — new tab in `/browse` or standalone page
  - Click-to-paint 8×8 grid canvas
  - Color picker, fill, clear, eyedropper
  - Export: saves as `.raw` to `/ICONS/` on device via `POST /edit`

- [ ] **7.2** Generative / rule-based icons (stretch)
  - Simple JS generators for common shapes: arrows, checkmarks, letters, numerals
  - No external API needed for basic set

---

### Phase 8 — Hardware Abstraction (Other Pixel Clocks)

- [ ] **8.1** Hardware config header `dpx_hw.h`
  - Pull GPIO pins, matrix W×H, button layout out of hardcoded values into one place
  - Currently Ulanzi TC001 config is scattered across `platformio_override.ini` build flags and inline defines

- [ ] **8.2** Alternative hardware profiles
  - Ulanzi TC001: 32×8, GPIO32 data, 3 buttons (current — keep as default)
  - Ulanzi TC004: different dimensions — define profile
  - Generic ESP32 + WS2812B matrix: user-configurable size via `dev.json`

---

## Part 2 — Server (friendster / cueMaestro)

Full spec: `⊘ dpx_reference/dpx_tc002_server.md`
Predecessor: `matrix-blast` service in `dpx_showsite_ops` (port 8090)

**Build order:** firmware Phase 1 must be done first (already is). Server is a separate Python repo.

**Tech stack:** Python 3.11+ · FastAPI · paho-mqtt · influxdb-client · Docker Compose
**Deployment:** new service in `dpx_showsite_ops/docker-compose.yml` on port 8091

### Phase S1 — Core Messaging

- [ ] Create `friendster` repo + Docker service skeleton
- [ ] MQTT client subscribing to `+/presence` and `+/info` wildcards
- [ ] In-memory device registry + InfluxDB `friendster` measurement for persistence
- [ ] `GET /api/devices` — list with online/offline status
- [ ] `POST /api/send {"to":"name","text":"...","palette":"rainbow"}` — publishes to `[PREFIX]/notify`
- [ ] `POST /api/broadcast {"text":"..."}` — sends to all online device prefixes
- [ ] `GET /api/messages` — recent message history from InfluxDB
- [ ] `GET /sse/devices` — Server-Sent Events for live presence changes
- [ ] `GET /sse/messages` — SSE for live message feed
- [ ] **Friendster UI** — dark-themed buddy list + send form + SSE-driven updates

### Phase S2 — cueMaestro

- [ ] Saved cues stored in YAML/JSON config file
  - Schema: `{name, text, color/palette, targets[], duration}`
- [ ] Target groups (ALL, stage, foh, etc.)
- [ ] `POST /api/cue` — create/save named cue
- [ ] `GET /api/cues` — list saved cues
- [ ] `POST /api/cue/:name/fire` — fire a named cue
- [ ] **cueMaestro UI** — device grid, large tap targets, quick-fire buttons, log panel
- [ ] Keyboard shortcuts (1-9 for quick cues, tablet-optimized)

### Phase S3 — Integration

- [ ] OSC receive (UDP) — `/friendster/cue/<name>` fires named cue; allows QLab trigger
- [ ] Set Schedule hook — auto-fire cue on scene advance events from `dpx_showsite_ops`
- [ ] HTTP callback API — external systems can fire cues via REST
- [ ] Multi-site support — multiple broker connections

### Phase S4 — Social Polish

- [ ] Mobile-optimized Friendster UI
- [ ] PWA manifest + offline support
- [ ] Message reactions
- [ ] Presence icons / avatars per device

---

## /ctrl Page Status (audit 2026-07-18)

| Card | Status | Notes |
|------|--------|-------|
| Notification | ✅ Works | icon/pushIcon parsed but not rendered (Phase 2); overlays now functional |
| Custom App | ✅ Works | same icon note; deleteCustomApp fixed |
| Indicators | 🟡 Partial | `fade` field still ignored in backend handler |
| Moodlight | ✅ Works | — |
| Display | ✅ Works | — |
| App Channels | ✅ Works | — |
| OSC Listeners | ✅ Works | — |
| TC Settings | ✅ Works | — |
| Native Apps | 🟡 Partial | TIM/DAT toggle implemented; TEMP/HUM/BAT rendering not built yet |
| Time | ✅ Works | — |
| Sensors | ✅ Works | — |
| Sound | 🟡 Partial | SOUND/VOL settings keys are cosmetic (passive buzzer = no volume) |

---

## 1 — cpt-city Gradient Integration

**Status:** ❌ Not started

**What it is:**
Load gradient palettes from the cpt-city archive (http://soliton.vm.bytemark.co.uk/pub/cpt-city/).
cpt-city `.cpt` files use a simple `x R G B` text format and cover hundreds of named
color gradients (terrain, scientific, artistic).

**Current state:**
- WLED already has 59 hardcoded gradient palettes in `wled00/palettes.cpp`
- No references to cpt-city anywhere in the codebase
- `dpx_matrix` supports two-color gradient text (`gradient: [c1, c2]` JSON param) but
  only for custom app text rendering, not WLED-wide palettes

**Integration points:**
- `wled00/palettes.cpp` — add parsed cpt-city palettes alongside existing ones
- `wled00/data/cpal/cpal.htm` — existing palette editor; extend to preview cpt-city entries
- `usermods/dpx_matrix/dpx_text.h` — gradient text rendering already accepts color arrays

**Approach options (choose one):**
- A) Bundle a curated offline set of popular `.cpt` files, converted to WLED palette format at build time
- B) Device fetches `.cpt` files from cpt-city CDN at runtime (requires HTTP client on device)

---

## 2 — Bigtime Animated GIF Playback*

**Status:** ❌ Stub — web UI tooling only, zero firmware implementation

**What it is:**
Display 32×8 animated GIFs on the matrix. The AWTRIX convention stores animations as
a horizontal strip of 8×8 frames in a single GIF (e.g., 4 frames = 32×8 px image).

**Current state:**
- `usermods/dpx_matrix/dpx_html.h` — web UI tab "Bigtime GIFs" (~220 lines):
  - Fetches `.gif` filenames from the Blueforcer/AWTRIX3 GitHub repo
  - Converts GIF → frame images in-browser
  - Uploads to device LittleFS `/ANIMS/` folder
- **Zero firmware code**: no GIF decoder, no frame extraction, no playback loop, no render mode

**What needs to be built:**
1. GIF decoder library for ESP32/Arduino (e.g., `AnimatedGIF` by Larry Bank — MIT license)
2. Frame extraction: slice 32×8 GIF into 8×8 frames by x-offset
3. Playback engine: frame timing, loop control, frame index state
4. New render mode `TMODE_GIF` in `usermods/dpx_matrix/dpx_apps.h`
5. Integration with custom app system (`icon` field accepting a `.gif` filename)

**Files to modify:**
- `usermods/dpx_matrix/dpx_apps.h` — add GIF app type + playback state
- `usermods/dpx_matrix/dpx_text.h` — add GIF frame render path
- `usermods/dpx_matrix/dpx_html.h` — already handles upload side

---

## 3 — LaMetric Icons on Matrix* ✅ DONE (GH #16)

**Status:** ✅ Done — browser-side PNG→raw conversion, `dpx_icons.h` load + render, wired
into the app draw loop. Shipped via the pre-convert-at-download-time approach from
"What needs to be built" below (no on-device PNG decoder needed).

<details>
<summary>Original planning notes (kept for history)</summary>

**What it is:**
Fetch a named LaMetric icon (8×8 PNG from `developer.lametric.com`), decode it to
RGB pixels, and render it on the left 8 columns of the matrix alongside scrolling text.

**Current state:**
- `usermods/dpx_matrix/dpx_html.h` — icon browser (~120 lines):
  - Fetches thumbnails from `developer.lametric.com/content/apps/icon_thumbs/{id}_icon_thumb.png`
  - Downloads full PNG to device `/ICONS/` LittleFS folder
- App JSON `"icon": "87"` field is parsed and stored
- **Zero firmware rendering**: no PNG decoder, no pixel extraction, icons never appear on display

**What needs to be built:**
1. PNG decoder for ESP32 — options:
   - `PNGdec` library by Larry Bank (MIT, ESP32-compatible)
   - Pre-convert PNGs to raw RGB565/RGB888 at download time (browser-side, avoids on-device decode)
2. Icon render function: 8×8 RGB888 → left 8 columns of matrix
3. Text layout adjustment: when icon present, scroll text in columns 9–32 only
4. Wire into `usermods/dpx_matrix/dpx_apps.h` render loop

**Files to modify:**
- `usermods/dpx_matrix/dpx_apps.h` — icon render call in app draw loop
- `usermods/dpx_matrix/dpx_html.h` — optionally pre-convert PNG→raw at download time
- New file: `usermods/dpx_matrix/dpx_icons.h` — icon load + render functions

</details>

---

## 4 — Font Pixels as WLED LED Effects

**Status:** ❌ Not started — requires architectural change to text renderer

**What it is:**
Every pixel of every rendered font character behaves like a pixel in a WLED LED strip.
WLED effects (rainbow, sparkle, pulse, fire, etc.) can be applied per-character or
per-glyph, so text itself animates with the full WLED effect engine rather than just
being a static or single-color block.

**Current architecture problem:**
`usermods/dpx_matrix/dpx_text.h` rasterizes glyphs directly into the final LED pixel
buffer as RGB888 values. There is no intermediate representation — no per-character
bounding boxes, no "which pixels belong to which glyph" metadata.

**What needs to be built:**
1. **Bounding box tracking** in `dpx_text.h`: during render, record `{x, y, w, h}` for
   each character drawn into a `CharBBox[]` array
2. **Per-character effect mask** in `dpx_overlay.h`: given a bounding box, run a WLED
   effect only on those pixels (using a local pixel buffer slice)
3. **JSON field** in the custom app schema: e.g., `"textEffect": "rainbow"` per character
   fragment in multi-color text arrays, or `"textEffect": "sparkle"` globally
4. **Effect hookup**: map effect names to existing WLED FX functions or re-implement
   lightweight versions that work on arbitrary pixel sub-regions

**Files to modify:**
- `usermods/dpx_matrix/dpx_text.h` — add bbox tracking output
- `usermods/dpx_matrix/dpx_overlay.h` — add per-region effect application
- `usermods/dpx_matrix/dpx_apps.h` — add `textEffect` JSON field + per-char effect dispatch

---

## 5 — Text Overlay Effects* ✅ DONE

**Status:** ✅ Done — all 10 originally-planned effects implemented, plus 5 more not
in the original spec, with additive compositing (text shows through, GH #12) and a
persistent pixel buffer with wind drift for the weather effects.

**Implemented** (`usermods/dpx_matrix/dpx_overlay.h`):
`rain` · `snow` · `drizzle` · `storm` · `thunder` · `frost` · `sparkle` · `twinkle` ·
`strobe` · `blink` · `pulse` · `rainbow` · `fire` · `matrix` · `scan`

The original name-mismatch bug (API parser vs. web UI JSON schema) was fixed as part
of this work.

---

## 6 — Progress Bar* ✅ DONE

**Status:** ✅ Fully implemented

**Implementation:**
- `usermods/dpx_matrix/dpx_text.h` — `dpxDrawProgressBar()` function
- `usermods/dpx_matrix/dpx_apps.h` — JSON parsing + render call in app loop
- `usermods/dpx_matrix/dpx_tc.h` — reuses bar for timecode frame-progress display

**JSON fields:** `progress` (int 0–100, -1 = hidden), `progressC` (fill color), `progressBC` (background color)

**Behavior:** Renders in the bottom 2 rows of the 8-row matrix, fills left-to-right proportionally.

---

## Implementation Priority (suggested order)

1. ~~**#5 Overlay effects**~~ — ✅ done
2. ~~**#3 LaMetric icons**~~ — ✅ done
3. **#2 Animated GIFs** — depends on choosing + integrating a GIF decoder library
4. **#1 cpt-city gradients** — standalone, no dependencies; start with offline bundle approach
5. **#4 Font pixel effects** — most architectural complexity; design the bbox layer carefully
