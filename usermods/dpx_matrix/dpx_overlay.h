// ================================================================================
// dpx_overlay.h — Text Overlay + Pixel Effects (run on top of app content)
// ================================================================================
// Original work — dubpixel / dpx_tc002 (EUPL v1.2)
// ================================================================================
// PROJECT: dpx_tc002_frm
// ================================================================================
//
// File: dpx_overlay.h
// Purpose: Two stacked layers rendered after dpxRenderCurrentApp():
//
//   1. TEXT OVERLAY — static or scrolling text drawn at a specific position
//      over the current app content. Controlled via MQTT dpx/overlay or
//      JSON state {"dpx":{"overlay":{...}}}.
//
//      Position options:
//        "top"    → baseline row 2 (glyph rows 1–5, occupies top of display)
//        "bottom" → baseline row 7 (glyph rows 3–7, occupies bottom 5 rows)
//        "center" → baseline DPX_FONT_BASELINE (vertically centered)
//        N (int)  → explicit baseline row number
//
//      JSON schema:
//        {"text":"LIVE","color":"#FF0000","pos":"top","scroll":false,"speed":80}
//        Clear: {"text":""}
//
//   2. PIXEL EFFECT — lightweight per-pixel effect applied on top of everything.
//      Controlled via MQTT dpx/effect or JSON state {"dpx":{"effect":{...}}}.
//
//      Overlay Effects: "sparkle", "strobe", "rain", "twinkle", "blink", "drizzle",
//                      "snow", "storm", "thunder", "frost", "none"
//      Background Effects: "colorwaves", "plasma", "twinklingstars", "theatrechase", "pacifica"
//
//      JSON schema:
//        {"name":"sparkle","color":"#FFFFFF","intensity":50}
//        Clear: {"name":"none"} or {"name":""}
//
// ================================================================================

#pragma once
#include "dpx_text.h"
#include "dpx_apps.h"  // for dpxParseColor

// ── Text overlay state ────────────────────────────────────────────────────────
static struct {
    bool     active   = false;
    String   text;
    uint32_t color    = 0xFFFFFF;
    bool     rainbow  = false;
    bool     scroll   = false;
    int      baseline = DPX_FONT_BASELINE; // cursor_y
    int      speed    = 80;                // scroll speed %
    // Scroll state (re-used from DpxScrollState internals inline):
    int      scrollX  = DPX_MATRIX_W;
    int      textWidth = 0;
    unsigned long lastMs = 0;
    int      speedMs  = 50;
} dpxTextOverlay;

static struct {
    String   name;                // "sparkle", "strobe", "rain", "twinkle", "blink", "none"
    uint32_t color    = 0xFFFFFF;
    uint8_t  intensity = 50;      // 0–100
    bool     active   = false;
    // per-effect private state
    unsigned long lastMs = 0;
    bool     strobeOn = true;
    uint8_t  rain[DPX_MATRIX_W] = {}; // rain/drizzle/storm drop positions per column
    uint8_t  snow[DPX_MATRIX_W] = {}; // snow floor accumulation (rows from bottom)
    bool     frostInit = false;        // frost: static pattern generated?
    uint8_t  frost[256] = {};          // frost: which pixels are frosted (1=yes)
    // Persistent brightness buffer for sparkle/twinkle — updated at rate-limit
    // interval, drawn every frame so glow doesn't flicker with FX framerate (#59)
    uint8_t  pixbuf[256] = {};
    // New effect state
    uint8_t  pulsePhase = 0;      // pulse: 0–255, maps to brightness
    uint8_t  rainbowHue = 0;      // rainbow: current hue (0–255)
    uint8_t  fireState[256] = {}; // fire: heat per pixel
    uint8_t  matrix[DPX_MATRIX_W] = {}; // matrix: drop position per column
    uint8_t  scanPos = 0;         // scan: beam X position
} dpxPixelEffect;

// ── Text overlay control ──────────────────────────────────────────────────────

static bool dpxSetOverlay(const char* json) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, json)) return false;

    String text = doc["text"] | String();
    if (!text.length()) {
        dpxTextOverlay.active = false;
        return true;
    }

    dpxTextOverlay.text     = text;
    dpxTextOverlay.color    = dpxParseColor(doc["color"], 0xFFFFFF);
    dpxTextOverlay.rainbow  = doc["rainbow"] | false;
    dpxTextOverlay.scroll   = doc["scroll"]  | false;
    dpxTextOverlay.speed    = doc["speed"]   | 80;
    dpxTextOverlay.speedMs  = max(10, (int)(50 * 100 / max(1, dpxTextOverlay.speed)));
    dpxTextOverlay.textWidth = dpxTextPixelWidth(text.c_str());
    dpxTextOverlay.scrollX  = DPX_MATRIX_W;
    dpxTextOverlay.lastMs   = 0;

    // Resolve position string → baseline row
    if (doc.containsKey("pos")) {
        JsonVariant pos = doc["pos"];
        if (pos.is<int>()) {
            dpxTextOverlay.baseline = pos.as<int>();
        } else {
            String p = pos.as<String>();
            if      (p == "top")    dpxTextOverlay.baseline = 2;
            else if (p == "bottom") dpxTextOverlay.baseline = 7;
            else if (p == "center") dpxTextOverlay.baseline = DPX_FONT_BASELINE;
            else                    dpxTextOverlay.baseline = DPX_FONT_BASELINE;
        }
    } else {
        dpxTextOverlay.baseline = DPX_FONT_BASELINE;
    }

    dpxTextOverlay.active = true;
    return true;
}

static void dpxClearOverlay() { dpxTextOverlay.active = false; }

// Render text overlay — called after app/notification content is drawn.
static void dpxRenderTextOverlay() {
    if (!dpxTextOverlay.active) return;

    if (!dpxTextOverlay.scroll || dpxTextOverlay.textWidth <= DPX_MATRIX_W) {
        // Static: centre or left-align
        int x = 0;
        if (dpxTextOverlay.textWidth < DPX_MATRIX_W)
            x = (DPX_MATRIX_W - dpxTextOverlay.textWidth) / 2;
        dpxRenderText(x, dpxTextOverlay.baseline,
                      dpxTextOverlay.text.c_str(),
                      dpxTextOverlay.color, dpxTextOverlay.rainbow);
    } else {
        // Scrolling
        unsigned long now = millis();
        if (now - dpxTextOverlay.lastMs >= (unsigned long)dpxTextOverlay.speedMs) {
            dpxTextOverlay.lastMs = now;
            if (--dpxTextOverlay.scrollX < -(dpxTextOverlay.textWidth))
                dpxTextOverlay.scrollX = DPX_MATRIX_W;
        }
        dpxRenderText(dpxTextOverlay.scrollX, dpxTextOverlay.baseline,
                      dpxTextOverlay.text.c_str(),
                      dpxTextOverlay.color, dpxTextOverlay.rainbow);
    }
}

// ── Pixel effect control ──────────────────────────────────────────────────────

static bool dpxSetPixelEffect(const char* json) {
    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, json)) return false;

    String name = doc["name"] | String("none");
    name.toLowerCase();
    dpxPixelEffect.name      = name;
    dpxPixelEffect.color     = dpxParseColor(doc["color"], 0xFFFFFF);
    dpxPixelEffect.intensity = doc["intensity"] | 50;
    dpxPixelEffect.active    = (name != "none" && name.length() > 0);
    dpxPixelEffect.lastMs    = millis(); // init to now so first toggle doesn't fire immediately
    dpxPixelEffect.strobeOn  = true;
    memset(dpxPixelEffect.rain,   0, sizeof(dpxPixelEffect.rain));
    memset(dpxPixelEffect.snow,   0, sizeof(dpxPixelEffect.snow));
    memset(dpxPixelEffect.pixbuf, 0, sizeof(dpxPixelEffect.pixbuf));
    memset(dpxPixelEffect.fireState, 0, sizeof(dpxPixelEffect.fireState));
    memset(dpxPixelEffect.matrix, 0, sizeof(dpxPixelEffect.matrix));
    dpxPixelEffect.frostInit = false;
    dpxPixelEffect.pulsePhase = 0;
    dpxPixelEffect.rainbowHue = 0;
    dpxPixelEffect.scanPos = 0;
    return true;
}

static void dpxClearPixelEffect() { dpxPixelEffect.active = false; }

// Render one frame of the active pixel effect on top of current LED content.
// Effects are split into advance (rate-limited state update) and draw (every
// frame from persistent state) so effects don't flicker at FX framerate. (#59)
static void dpxRenderPixelEffect() {
    if (!dpxPixelEffect.active) return;

    unsigned long now = millis();
    uint32_t col = dpxPixelEffect.color;
    uint8_t  iv  = dpxPixelEffect.intensity;

    // ── Sparkle ──────────────────────────────────────────────────────────
    // pixbuf stores per-pixel glow brightness. Advance seeds/decays; draw renders
    // pixbuf every frame so glow persists between rate-limit intervals.
    if (dpxPixelEffect.name == "sparkle") {
        int intervalMs = map(iv, 0, 100, 120, 30);
        if (now - dpxPixelEffect.lastMs >= (unsigned long)intervalMs) {
            dpxPixelEffect.lastMs = now;
            for (int i = 0; i < 256; i++)
                dpxPixelEffect.pixbuf[i] = (dpxPixelEffect.pixbuf[i] > 30) ? dpxPixelEffect.pixbuf[i] - 30 : 0;
            int count = max(1, (int)(iv / 8)); // sparser
            for (int i = 0; i < count; i++)
                dpxPixelEffect.pixbuf[(int)random(256)] = 255;
        }
        uint8_t r = (col >> 16) & 0xFF, g = (col >> 8) & 0xFF, b = col & 0xFF;
        for (int i = 0; i < 256; i++) {
            if (dpxPixelEffect.pixbuf[i] > 0) {
                uint8_t bri = dpxPixelEffect.pixbuf[i];
                uint32_t c = ((uint32_t)(r * bri / 255) << 16) | ((uint32_t)(g * bri / 255) << 8) | (b * bri / 255);
                SEGMENT.setPixelColorXY(i % DPX_MATRIX_W, i / DPX_MATRIX_W,
                    color_blend(SEGMENT.getPixelColorXY(i % DPX_MATRIX_W, i / DPX_MATRIX_W), c, 180)); // brighter
            }
        }
    }

    // ── Twinkle ──────────────────────────────────────────────────────────
    // Pops pixels bright — random pixels flash above their base color. (#58)
    else if (dpxPixelEffect.name == "twinkle") {
        int intervalMs = map(iv, 0, 100, 200, 50);
        if (now - dpxPixelEffect.lastMs >= (unsigned long)intervalMs) {
            dpxPixelEffect.lastMs = now;
            for (int i = 0; i < 256; i++)
                dpxPixelEffect.pixbuf[i] = (dpxPixelEffect.pixbuf[i] > 25) ? dpxPixelEffect.pixbuf[i] - 25 : 0;
            int count = max(1, (int)(iv / 6));
            for (int i = 0; i < count; i++)
                dpxPixelEffect.pixbuf[(int)random(256)] = 200 + (int)(random(55));
        }
        uint8_t r = (col >> 16) & 0xFF, g = (col >> 8) & 0xFF, b = col & 0xFF;
        for (int i = 0; i < 256; i++) {
            if (dpxPixelEffect.pixbuf[i] > 0) {
                uint8_t bri = dpxPixelEffect.pixbuf[i];
                uint32_t c = ((uint32_t)(r * bri / 255) << 16) | ((uint32_t)(g * bri / 255) << 8) | (b * bri / 255);
                SEGMENT.setPixelColorXY(i % DPX_MATRIX_W, i / DPX_MATRIX_W,
                    color_blend(SEGMENT.getPixelColorXY(i % DPX_MATRIX_W, i / DPX_MATRIX_W), c, 160));
            }
        }
    }

    // ── Strobe — inverts the display on flash frames (#60) ───────────────────
    else if (dpxPixelEffect.name == "strobe") {
        int intervalMs = map(iv, 0, 100, 400, 40);
        if (now - dpxPixelEffect.lastMs >= (unsigned long)intervalMs) {
            dpxPixelEffect.lastMs  = now;
            dpxPixelEffect.strobeOn = !dpxPixelEffect.strobeOn;
        }
        if (dpxPixelEffect.strobeOn) {
            for (int y = 0; y < DPX_MATRIX_H; y++)
                for (int x = 0; x < DPX_MATRIX_W; x++) {
                    uint32_t p = SEGMENT.getPixelColorXY(x, y);
                    // Invert: each channel flips around mid-grey
                    uint32_t inv = (((255 - ((p >> 16) & 0xFF)) << 16)
                                  | ((255 - ((p >>  8) & 0xFF)) <<  8)
                                  |  (255 - ( p        & 0xFF)));
                    SEGMENT.setPixelColorXY(x, y, inv);
                }
        }
    }

    // ── Blink ────────────────────────────────────────────────────────────────
    else if (dpxPixelEffect.name == "blink") {
        int intervalMs = map(iv, 0, 100, 1000, 200);
        if (now - dpxPixelEffect.lastMs >= (unsigned long)intervalMs) {
            dpxPixelEffect.lastMs  = now;
            dpxPixelEffect.strobeOn = !dpxPixelEffect.strobeOn;
        }
        if (!dpxPixelEffect.strobeOn) {
            for (int y = 0; y < DPX_MATRIX_H; y++)
                for (int x = 0; x < DPX_MATRIX_W; x++)
                    SEGMENT.setPixelColorXY(x, y, 0);
        }
    }

    // ── Rain — advance rate-limited, draw current state every frame ───────────────
    else if (dpxPixelEffect.name == "rain") {
        int intervalMs = map(iv, 0, 100, 200, 20);
        if (now - dpxPixelEffect.lastMs >= (unsigned long)intervalMs) {
            dpxPixelEffect.lastMs = now;
            for (int x = 0; x < DPX_MATRIX_W; x++) {
                if (dpxPixelEffect.rain[x] > 0) {
                    dpxPixelEffect.rain[x]++;
                    if ((int)dpxPixelEffect.rain[x] > DPX_MATRIX_H + 1) dpxPixelEffect.rain[x] = 0;
                }
                if (dpxPixelEffect.rain[x] == 0 && (random(256) < (iv * 2))) dpxPixelEffect.rain[x] = 1;
            }
        }
        for (int x = 0; x < DPX_MATRIX_W; x++)
            if (dpxPixelEffect.rain[x] > 0 && (int)dpxPixelEffect.rain[x] - 1 < DPX_MATRIX_H)
                dpxSetPixel(x, dpxPixelEffect.rain[x] - 1, 0x4466AA); // blue like drizzle
    }

    // ── Drizzle ──────────────────────────────────────────────────────────────
    else if (dpxPixelEffect.name == "drizzle") {
        int intervalMs = map(iv, 0, 100, 400, 80);
        if (now - dpxPixelEffect.lastMs >= (unsigned long)intervalMs) {
            dpxPixelEffect.lastMs = now;
            for (int x = 0; x < DPX_MATRIX_W; x++) {
                if (dpxPixelEffect.rain[x] > 0) {
                    dpxPixelEffect.rain[x]++;
                    if ((int)dpxPixelEffect.rain[x] > DPX_MATRIX_H + 1) dpxPixelEffect.rain[x] = 0;
                }
                if (dpxPixelEffect.rain[x] == 0 && (random(512) < (uint32_t)(iv + 5))) dpxPixelEffect.rain[x] = 1;
            }
        }
        for (int x = 0; x < DPX_MATRIX_W; x++)
            if (dpxPixelEffect.rain[x] > 0 && (int)dpxPixelEffect.rain[x] - 1 < DPX_MATRIX_H)
                dpxSetPixel(x, dpxPixelEffect.rain[x] - 1, 0x4466AA);
    }

    // ── Storm — diagonal rain (slants right as it falls) ─────────────────────
    else if (dpxPixelEffect.name == "storm") {
        int intervalMs = map(iv, 0, 100, 100, 10);
        if (now - dpxPixelEffect.lastMs >= (unsigned long)intervalMs) {
            dpxPixelEffect.lastMs = now;
            for (int x = 0; x < DPX_MATRIX_W; x++) {
                if (dpxPixelEffect.rain[x] > 0) {
                    dpxPixelEffect.rain[x]++;
                    if ((int)dpxPixelEffect.rain[x] > DPX_MATRIX_H + 1) dpxPixelEffect.rain[x] = 0;
                }
                if (dpxPixelEffect.rain[x] == 0 && (random(128) < (uint32_t)(iv * 3 + 20))) dpxPixelEffect.rain[x] = 1;
            }
        }
        // Draw diagonally: x offset = row/2 so drop slants right as it falls
        for (int x = 0; x < DPX_MATRIX_W; x++) {
            if (dpxPixelEffect.rain[x] > 0) {
                int row = (int)dpxPixelEffect.rain[x] - 1;
                if (row < DPX_MATRIX_H) {
                    int dx = (x + row / 2) % DPX_MATRIX_W;
                    dpxSetPixel(dx, row, 0x6688FF);
                }
            }
        }
        static unsigned long _stormFlashMs = 0;
        static int _stormFlashFrames = 0;
        if (_stormFlashFrames > 0) {
            for (int y = 0; y < DPX_MATRIX_H; y++)
                for (int x = 0; x < DPX_MATRIX_W; x++)
                    SEGMENT.setPixelColorXY(x, y, color_blend(SEGMENT.getPixelColorXY(x, y), 0xFFFFFF, 180));
            _stormFlashFrames--;
        } else if (now - _stormFlashMs > 2000 && random(200) < 5) {
            _stormFlashMs = now;
            _stormFlashFrames = 2;
        }
    }

    // ── Thunder — bright flash with ~400ms crash-and-decay using pixbuf ─────
    else if (dpxPixelEffect.name == "thunder") {
        int intervalMs = map(iv, 0, 100, 5000, 800);
        if (now - dpxPixelEffect.lastMs >= (unsigned long)intervalMs) {
            dpxPixelEffect.lastMs = now;
            // Trigger flash: set all pixbuf to 255
            memset(dpxPixelEffect.pixbuf, 255, sizeof(dpxPixelEffect.pixbuf));
            dpxPixelEffect.strobeOn = true;
        }
        // Decay pixbuf each frame (~15 per frame at 50fps = 400ms to zero)
        if (dpxPixelEffect.strobeOn) {
            bool anyActive = false;
            for (int i = 0; i < 256; i++) {
                if (dpxPixelEffect.pixbuf[i] > 0) {
                    dpxPixelEffect.pixbuf[i] = (dpxPixelEffect.pixbuf[i] > 15) ? dpxPixelEffect.pixbuf[i] - 15 : 0;
                    anyActive = true;
                }
            }
            if (!anyActive) dpxPixelEffect.strobeOn = false;
            for (int i = 0; i < 256; i++) {
                if (dpxPixelEffect.pixbuf[i] > 0) {
                    SEGMENT.setPixelColorXY(i % DPX_MATRIX_W, i / DPX_MATRIX_W,
                        color_blend(SEGMENT.getPixelColorXY(i % DPX_MATRIX_W, i / DPX_MATRIX_W),
                                    0xFFFFFF, dpxPixelEffect.pixbuf[i]));
                }
            }
        }
    }

    // ── Snow ────────────────────────────────────────────────────────────────
    else if (dpxPixelEffect.name == "snow") {
        int intervalMs = map(iv, 0, 100, 350, 70);
        if (now - dpxPixelEffect.lastMs >= (unsigned long)intervalMs) {
            dpxPixelEffect.lastMs = now;
            for (int x = 0; x < DPX_MATRIX_W; x++) {
                if (dpxPixelEffect.rain[x] > 0) {
                    int floorY = DPX_MATRIX_H - 1 - (int)dpxPixelEffect.snow[x];
                    int y = dpxPixelEffect.rain[x] - 1;
                    if (y >= floorY) {
                        if (dpxPixelEffect.snow[x] < DPX_MATRIX_H) dpxPixelEffect.snow[x]++;
                        dpxPixelEffect.rain[x] = 0;
                    } else {
                        dpxPixelEffect.rain[x]++;
                    }
                } else if (random(1024) < (uint32_t)(iv + 5)) {
                    dpxPixelEffect.rain[x] = 1;
                }
            }
            // Wind drift: occasionally shift a falling flake left or right
            for (int x = 0; x < DPX_MATRIX_W; x++) {
                if (dpxPixelEffect.rain[x] > 0 && random(8) == 0) {
                    int nx = x + (random(2) == 0 ? 1 : -1);
                    if (nx >= 0 && nx < DPX_MATRIX_W && dpxPixelEffect.rain[nx] == 0) {
                        dpxPixelEffect.rain[nx] = dpxPixelEffect.rain[x];
                        dpxPixelEffect.rain[x] = 0;
                    }
                }
            }
        }
        for (int x = 0; x < DPX_MATRIX_W; x++)
            if (dpxPixelEffect.rain[x] > 0 && (int)dpxPixelEffect.rain[x] - 1 < DPX_MATRIX_H)
                dpxSetPixel(x, dpxPixelEffect.rain[x] - 1, 0xDDDDFF);
        for (int x = 0; x < DPX_MATRIX_W; x++)
            for (uint8_t py = 0; py < dpxPixelEffect.snow[x]; py++)
                dpxSetPixel(x, DPX_MATRIX_H - 1 - py, 0xDDDDFF);
    }

    // ── Frost ──────────────────────────────────────────────────────────────
    else if (dpxPixelEffect.name == "frost") {
        if (!dpxPixelEffect.frostInit) {
            dpxPixelEffect.frostInit = true;
            int count = (256 * iv) / 200;
            memset(dpxPixelEffect.frost, 0, sizeof(dpxPixelEffect.frost));
            for (int i = 0; i < count; i++)
                dpxPixelEffect.frost[(int)random(256)] = 1;
        }
        for (int i = 0; i < 256; i++)
            if (dpxPixelEffect.frost[i])
                SEGMENT.setPixelColorXY(i % DPX_MATRIX_W, i / DPX_MATRIX_W,
                    color_blend(SEGMENT.getPixelColorXY(i % DPX_MATRIX_W, i / DPX_MATRIX_W), 0xAADDFF, 160));
    }

    // ── ColorWaves — horizontal color gradient animation ───────────────────
    else if (dpxPixelEffect.name == "colorwaves") {
        for (int x = 0; x < DPX_MATRIX_W; x++) {
            for (int y = 0; y < DPX_MATRIX_H; y++) {
                uint8_t hue = (uint8_t)((x * 255 / DPX_MATRIX_W) + now / 10);
                uint32_t c = ColorFromPalette(RainbowColors_p, hue, 255, LINEARBLEND);
                SEGMENT.setPixelColorXY(x, y,
                    color_blend(SEGMENT.getPixelColorXY(x, y), c, map(iv, 0, 100, 80, 200)));
            }
        }
    }

    // ── Plasma — overlapping sine patterns ─────────────────────────────────
    else if (dpxPixelEffect.name == "plasma") {
        static double plasmaTime = 0;
        plasmaTime += map(iv, 0, 100, 1, 10) * 0.01;
        for (int x = 0; x < DPX_MATRIX_W; x++) {
            for (int y = 0; y < DPX_MATRIX_H; y++) {
                uint8_t value = sin8(x * 10 + plasmaTime * 50) +
                               sin8(y * 10 + plasmaTime * 50 / 2) +
                               sin8((x + y) * 10 + plasmaTime * 50 / 3);
                value = value / 3;
                uint32_t c = ColorFromPalette(RainbowColors_p, value, 255, LINEARBLEND);
                SEGMENT.setPixelColorXY(x, y,
                    color_blend(SEGMENT.getPixelColorXY(x, y), c, map(iv, 0, 100, 60, 180)));
            }
        }
    }

    // ── TwinklingStars — random fading pixels ──────────────────────────────
    else if (dpxPixelEffect.name == "twinklingstars") {
        int intervalMs = map(iv, 0, 100, 300, 60);
        if (now - dpxPixelEffect.lastMs >= (unsigned long)intervalMs) {
            dpxPixelEffect.lastMs = now;
            int numStars = max(1, (int)(iv / 20));
            for (int i = 0; i < numStars; i++) {
                int x = random(DPX_MATRIX_W);
                int y = random(DPX_MATRIX_H);
                dpxPixelEffect.pixbuf[y * DPX_MATRIX_W + x] = 200 + random(55);
            }
            for (int i = 0; i < 256; i++) {
                if (dpxPixelEffect.pixbuf[i] > 0)
                    dpxPixelEffect.pixbuf[i] = (dpxPixelEffect.pixbuf[i] > 20) ? dpxPixelEffect.pixbuf[i] - 20 : 0;
            }
        }
        for (int i = 0; i < 256; i++) {
            if (dpxPixelEffect.pixbuf[i] > 0) {
                uint8_t bri = dpxPixelEffect.pixbuf[i];
                uint32_t c = ColorFromPalette(OceanColors_p, random8(), 255, LINEARBLEND);
                SEGMENT.setPixelColorXY(i % DPX_MATRIX_W, i / DPX_MATRIX_W,
                    color_blend(SEGMENT.getPixelColorXY(i % DPX_MATRIX_W, i / DPX_MATRIX_W), c, bri));
            }
        }
    }

    // ── TheaterChase — chasing dots pattern ────────────────────────────────
    else if (dpxPixelEffect.name == "theatrechase") {
        int intervalMs = map(iv, 0, 100, 200, 30);
        if (now - dpxPixelEffect.lastMs >= (unsigned long)intervalMs) {
            dpxPixelEffect.lastMs = now;
            dpxPixelEffect.pulsePhase = (dpxPixelEffect.pulsePhase + 1) % 3;
        }
        for (int x = 0; x < DPX_MATRIX_W; x++) {
            for (int y = 0; y < DPX_MATRIX_H; y++) {
                if ((x + dpxPixelEffect.pulsePhase) % 3 == 0) {
                    uint8_t hue = (x * 255 / DPX_MATRIX_W);
                    uint32_t c = ColorFromPalette(RainbowColors_p, hue, 255, LINEARBLEND);
                    SEGMENT.setPixelColorXY(x, y, color_blend(SEGMENT.getPixelColorXY(x, y), c, 200));
                }
            }
        }
    }

    // ── Pacifica — wavy ocean simulation ───────────────────────────────────
    else if (dpxPixelEffect.name == "pacifica") {
        static uint32_t pacificaTime = 0;
        pacificaTime += map(iv, 0, 100, 1, 5);
        for (int x = 0; x < DPX_MATRIX_W; x++) {
            for (int y = 0; y < DPX_MATRIX_H; y++) {
                uint16_t ulx = (pacificaTime / 8) - (x * 16);
                uint16_t uly = (pacificaTime / 4) + (y * 16);
                uint16_t v = 0;
                v += sin16(ulx * 6 + pacificaTime / 2) / 8 + 127;
                v += sin16(uly * 9 + pacificaTime / 2) / 8 + 127;
                v += sin16(ulx * 7 + uly * 2 - pacificaTime) / 16;
                v = v / 3;
                uint32_t c = ColorFromPalette(OceanColors_p, v, 255, LINEARBLEND);
                SEGMENT.setPixelColorXY(x, y,
                    color_blend(SEGMENT.getPixelColorXY(x, y), c, map(iv, 0, 100, 100, 220)));
            }
        }
    }
}

// ── Master overlay render — call at end of handleOverlayDraw() ────────────────
static void dpxRenderOverlays() {
    dpxRenderPixelEffect();  // pixel effect first (under text)
    dpxRenderTextOverlay();  // text on top
}
