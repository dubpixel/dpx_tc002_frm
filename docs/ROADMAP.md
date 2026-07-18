# dpx_tc002_frm — Master Roadmap & TODO

Last updated: 2026-07-18

Reference files (read before working on this project):
- `⊘ dpx_reference/SPEC.md` — full behavioral spec, API contract, all JSON keys
- `⊘ dpx_reference/dpx_tc002.md` — firmware build plan, phase order, GPIO map
- `⊘ dpx_reference/dpx_tc002_server.md` — Friendster/CueMaster server plan

---

## Quick Reference

**MQTT topic for text:** `[PREFIX]/notify` → `{"text":"hello","rainbow":true,"duration":8}`
PREFIX = device hostname. Read from `GET /api/settings` → `MQTT_PREFIX`.

**Server project (Friendster/CueMaster):** separate Python FastAPI repo, build after firmware is stable.
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
- [x] **1.9** Per-app overlay activation — added hook in `handleOverlayDraw()` in `dpx_matrix.h`; overlay activates automatically when app/notification is shown, clears on transition

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

### Phase 2 — Icon Rendering

> **Key insight:** pre-convert PNG→raw in-browser using a Canvas at download time. No PNGdec library needed on device.

- [ ] **2.1** Browser-side PNG→raw conversion `dpx_html.h`
  - At download time in the icon browser, convert PNG to raw RGB888 using `<canvas>`
  - Upload as `<name>.raw` to `/ICONS/` instead of PNG
  - Update the "installed icons" dropdown loader to look for `.raw` files

- [ ] **2.2** Icon load + render — new `dpx_icons.h`
  - `dpxLoadIcon(name)` — reads `/ICONS/<name>.raw`, returns 64-byte (8×8 RGB888) array
  - `dpxRenderIcon(pixels, x, y)` — blits 8×8 to matrix at column x

- [ ] **2.3** Text layout with icon `dpx_apps.h` + `dpx_text.h`
  - When `icon` is set: render icon in cols 0–7, scroll/render text in cols 8–31 (24px wide)
  - `pushIcon` modes: 0=icon fixed, 1=icon scrolls with text and disappears, 2=icon scrolls and loops
  - Apply to both custom apps and notifications

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

### Phase 4 — Font Scaling / Larger Font

> Check `wled00/src/fonts/` for existing GFX-format fonts before writing a new one.

- [ ] **4.1** Second font `dpx_font.h`
  - Add a 6×8 or 8×8 large font for close-viewing / big clock mode
  - Look for GFX-format bitmap fonts already in the repo first

- [ ] **4.2** Font selector `dpx_apps.h`
  - New JSON field: `"font":"small"` (default 3×5) | `"font":"large"` (new 6×8)
  - Or: `"textScale":2` for 2× pixel-doubling of existing font (simpler, no new font data)

- [ ] **4.3** UI control `dpx_html.h`
  - Add font size selector to Notification and Custom App cards

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

## Part 2 — Server (friendster / CueMaster)

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

### Phase S2 — CueMaster

- [ ] Saved cues stored in YAML/JSON config file
  - Schema: `{name, text, color/palette, targets[], duration}`
- [ ] Target groups (ALL, stage, foh, etc.)
- [ ] `POST /api/cue` — create/save named cue
- [ ] `GET /api/cues` — list saved cues
- [ ] `POST /api/cue/:name/fire` — fire a named cue
- [ ] **CueMaster UI** — device grid, large tap targets, quick-fire buttons, log panel
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

## 3 — LaMetric Icons on Matrix*

**Status:** 🟡 Partial — icon browser + LittleFS download works; no display rendering

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

## 5 — Text Overlay Effects*

**Status:** 🟡 Partial — 5 effects implemented, 5 others unimplemented, 1 name-mismatch bug

**What it is:**
Pixel-level effects rendered on top of (or composited with) displayed text.
Defined in the spec as 7 weather-themed overlays plus additional effects.

**Currently implemented** (`usermods/dpx_matrix/dpx_overlay.h`):
- ✅ `rain` — column-based falling pixels
- ✅ `sparkle` — random pixel lighting
- ✅ `twinkle` — blended random pixels
- ✅ `strobe` — full-matrix flash
- ✅ `blink` — full matrix on/off

**Not implemented (spec-required):**
- ❌ `snow` — slow drifting white pixels, accumulate at bottom
- ❌ `drizzle` — lighter version of rain (fewer, slower drops)
- ❌ `storm` — heavy rain + occasional flash
- ❌ `thunder` — periodic full-screen white flash
- ❌ `frost` — static blue-white pixel scatter over text

**Bug — name mismatch:**
The effect name strings in `dpxRenderPixelEffect()` (API parser) do not match the
names documented in the web UI JSON schema (`dpx_html.h`). Fix both sides to agree.

**Files to modify:**
- `usermods/dpx_matrix/dpx_overlay.h` — add 5 missing effects + fix name constants

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

1. **#5 Overlay effects** — lowest effort, high spec compliance impact; fix the name bug first
2. **#3 LaMetric icons** — browser-side PNG→raw conversion avoids on-device decoder complexity
3. **#2 Animated GIFs** — depends on choosing + integrating a GIF decoder library
4. **#1 cpt-city gradients** — standalone, no dependencies; start with offline bundle approach
5. **#4 Font pixel effects** — most architectural complexity; design the bbox layer carefully
