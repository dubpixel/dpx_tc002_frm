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

// ── Palette helpers (WLED has no FastLED RainbowColors_p/OceanColors_p/sin8) ──
static inline uint32_t dpxHueColor(uint8_t hue) {
    CRGBW rgb = CHSV(hue, 255, 255);
    return (uint32_t)rgb.r << 16 | (uint32_t)rgb.g << 8 | rgb.b;
}
static inline uint32_t dpxOceanColor(uint8_t v) {
    // Blue/cyan band only (FastLED hue ~130-170) for a watery feel.
    CRGBW rgb = CHSV(130 + (v % 40), 200, 255);
    return (uint32_t)rgb.r << 16 | (uint32_t)rgb.g << 8 | rgb.b;
}

// Maps a linear position (0..perim) onto the DPX_MATRIX_W x DPX_MATRIX_H border ring,
// walking clockwise from (0,0) — used for marquee/border-chase style effects.
static inline void dpxPerimXY(int p, int &x, int &y) {
    const int W = DPX_MATRIX_W, H = DPX_MATRIX_H;
    if (p < W) { x = p; y = 0; return; }
    p -= W;
    if (p < H - 1) { x = W - 1; y = p + 1; return; }
    p -= (H - 1);
    if (p < W - 1) { x = W - 2 - p; y = H - 1; return; }
    p -= (W - 1);
    x = 0; y = H - 2 - p;
}

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
    uint8_t  snow[DPX_MATRIX_W] = {}; // snow floor accumulation (rows from bottom)
    // Persistent brightness buffer for sparkle/twinkle/frost/twinklingstars — updated
    // at rate-limit interval, drawn every frame so glow doesn't flicker with FX framerate (#59)
    uint8_t  pixbuf[256] = {};
    uint8_t  pulsePhase = 0;      // reserved for future use
    // Persistent full-matrix color buffer for rain/drizzle/storm/thunder (#13/#18/#19).
    // Shifted down (and right for storm/thunder wind) each tick with fadeToBlack-style
    // trail dimming applied every shift, matching AWTRIX's CRGB leds[32][8] approach.
    uint32_t ovBuf[DPX_MATRIX_W][DPX_MATRIX_H] = {};
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
    memset(dpxPixelEffect.snow,   0, sizeof(dpxPixelEffect.snow));
    memset(dpxPixelEffect.pixbuf, 0, sizeof(dpxPixelEffect.pixbuf));
    memset(dpxPixelEffect.ovBuf,  0, sizeof(dpxPixelEffect.ovBuf));
    dpxPixelEffect.pulsePhase = 0;
    return true;
}

static void dpxClearPixelEffect() {
    dpxPixelEffect.active = false;
    memset(dpxPixelEffect.ovBuf, 0, sizeof(dpxPixelEffect.ovBuf));
}

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
    // Inverse of sparkle: pops random *currently-lit* pixels DARKER instead of
    // adding new bright flecks, so it reads as text/content flickering rather
    // than glitter on top of it. pixbuf holds a per-pixel dim countdown — while
    // >0 that pixel is drawn faded; it recovers back to full brightness as the
    // countdown decays. (#58)
    else if (dpxPixelEffect.name == "twinkle") {
        int intervalMs = map(iv, 0, 100, 200, 50);
        if (now - dpxPixelEffect.lastMs >= (unsigned long)intervalMs) {
            dpxPixelEffect.lastMs = now;
            for (int i = 0; i < 256; i++)
                dpxPixelEffect.pixbuf[i] = (dpxPixelEffect.pixbuf[i] > 25) ? dpxPixelEffect.pixbuf[i] - 25 : 0;
            int count = max(1, (int)(iv / 6));
            for (int n = 0; n < count; n++) {
                int i = (int)random(256);
                // Only dim pixels that currently have real content lit
                if (SEGMENT.getPixelColorXY(i % DPX_MATRIX_W, i / DPX_MATRIX_W) != 0)
                    dpxPixelEffect.pixbuf[i] = 220 + (int)random(36);
            }
        }
        for (int i = 0; i < 256; i++) {
            if (dpxPixelEffect.pixbuf[i] > 0) {
                int x = i % DPX_MATRIX_W, y = i / DPX_MATRIX_W;
                uint32_t p = SEGMENT.getPixelColorXY(x, y);
                if (p) SEGMENT.setPixelColorXY(x, y, color_fade(p, 255 - dpxPixelEffect.pixbuf[i], true));
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

    // ── Rain — persistent buffer, shift-down + trail fade (#13/#18) ───────────
    // ovBuf holds real color per pixel; unaffected pixels stay untouched so
    // text underneath is never erased (#12) — only drawn-on pixels are set.
    else if (dpxPixelEffect.name == "rain") {
        int intervalMs = map(iv, 0, 100, 160, 16);
        if (now - dpxPixelEffect.lastMs >= (unsigned long)intervalMs) {
            dpxPixelEffect.lastMs = now;
            // Per-column speed jitter — not every drop falls in lockstep.
            for (int x = 0; x < DPX_MATRIX_W; x++) {
                if (random(10) < 8) {
                    for (int y = DPX_MATRIX_H - 1; y > 0; y--)
                        dpxPixelEffect.ovBuf[x][y] = dpxPixelEffect.ovBuf[x][y - 1];
                    dpxPixelEffect.ovBuf[x][0] = 0;
                }
            }
            // Wind lean — drops occasionally drift diagonally instead of straight down.
            for (int x = 0; x < DPX_MATRIX_W; x++) {
                for (int y = 0; y < DPX_MATRIX_H; y++) {
                    if (dpxPixelEffect.ovBuf[x][y] && random(14) == 0) {
                        int nx = x + (random(2) == 0 ? 1 : -1);
                        if (nx >= 0 && nx < DPX_MATRIX_W && !dpxPixelEffect.ovBuf[nx][y]) {
                            dpxPixelEffect.ovBuf[nx][y] = dpxPixelEffect.ovBuf[x][y];
                            dpxPixelEffect.ovBuf[x][y] = 0;
                        }
                    }
                }
            }
            for (int x = 0; x < DPX_MATRIX_W; x++)
                for (int y = 0; y < DPX_MATRIX_H; y++)
                    if (dpxPixelEffect.ovBuf[x][y])
                        dpxPixelEffect.ovBuf[x][y] = color_fade(dpxPixelEffect.ovBuf[x][y], 195, true);
            for (int x = 0; x < DPX_MATRIX_W; x++)
                if (!dpxPixelEffect.ovBuf[x][0] && random(256) < (uint32_t)(iv * 2))
                    dpxPixelEffect.ovBuf[x][0] = random(2) ? 0x4466AA : 0x5577CC; // two-tone drops read as depth
        }
        for (int x = 0; x < DPX_MATRIX_W; x++)
            for (int y = 0; y < DPX_MATRIX_H; y++)
                if (dpxPixelEffect.ovBuf[x][y])
                    dpxSetPixel(x, y, dpxPixelEffect.ovBuf[x][y]);
    }

    // ── Drizzle — same buffer mechanics, sparser + slower + shorter trail ────
    else if (dpxPixelEffect.name == "drizzle") {
        int intervalMs = map(iv, 0, 100, 400, 80);
        if (now - dpxPixelEffect.lastMs >= (unsigned long)intervalMs) {
            dpxPixelEffect.lastMs = now;
            for (int x = 0; x < DPX_MATRIX_W; x++) {
                for (int y = DPX_MATRIX_H - 1; y > 0; y--)
                    dpxPixelEffect.ovBuf[x][y] = dpxPixelEffect.ovBuf[x][y - 1];
                dpxPixelEffect.ovBuf[x][0] = 0;
            }
            for (int x = 0; x < DPX_MATRIX_W; x++)
                for (int y = 0; y < DPX_MATRIX_H; y++)
                    if (dpxPixelEffect.ovBuf[x][y])
                        dpxPixelEffect.ovBuf[x][y] = color_fade(dpxPixelEffect.ovBuf[x][y], 140, true);
            for (int x = 0; x < DPX_MATRIX_W; x++)
                if (random(512) < (uint32_t)(iv + 5)) dpxPixelEffect.ovBuf[x][0] = 0x4466AA;
        }
        for (int x = 0; x < DPX_MATRIX_W; x++)
            for (int y = 0; y < DPX_MATRIX_H; y++)
                if (dpxPixelEffect.ovBuf[x][y])
                    dpxSetPixel(x, y, dpxPixelEffect.ovBuf[x][y]);
    }

    // ── Storm — buffer rain + wind drift (shift right every ~3 ticks) + flash (#19) ─
    else if (dpxPixelEffect.name == "storm") {
        int intervalMs = map(iv, 0, 100, 100, 10);
        if (now - dpxPixelEffect.lastMs >= (unsigned long)intervalMs) {
            dpxPixelEffect.lastMs = now;
            for (int x = 0; x < DPX_MATRIX_W; x++) {
                for (int y = DPX_MATRIX_H - 1; y > 0; y--)
                    dpxPixelEffect.ovBuf[x][y] = dpxPixelEffect.ovBuf[x][y - 1];
                dpxPixelEffect.ovBuf[x][0] = 0;
            }
            // Wind: sweep the whole buffer right by 1 column every 3rd tick
            dpxPixelEffect.pulsePhase = (dpxPixelEffect.pulsePhase + 1) % 3;
            if (dpxPixelEffect.pulsePhase == 0) {
                for (int y = 0; y < DPX_MATRIX_H; y++) {
                    uint32_t carry = dpxPixelEffect.ovBuf[DPX_MATRIX_W - 1][y];
                    for (int x = DPX_MATRIX_W - 1; x > 0; x--)
                        dpxPixelEffect.ovBuf[x][y] = dpxPixelEffect.ovBuf[x - 1][y];
                    dpxPixelEffect.ovBuf[0][y] = carry;
                }
            }
            for (int x = 0; x < DPX_MATRIX_W; x++)
                for (int y = 0; y < DPX_MATRIX_H; y++)
                    if (dpxPixelEffect.ovBuf[x][y])
                        dpxPixelEffect.ovBuf[x][y] = color_fade(dpxPixelEffect.ovBuf[x][y], 165, true);
            // was `random(128) < iv*3+20` — at default intensity the threshold (170)
            // exceeded random()'s own range (0-127), so EVERY column spawned a drop
            // EVERY tick: a solid wall that buried the text. Capped + rescaled.
            for (int x = 0; x < DPX_MATRIX_W; x++)
                if (random(256) < (uint32_t)map(iv, 0, 100, 30, 130)) dpxPixelEffect.ovBuf[x][0] = 0x6688FF;
        }
        for (int x = 0; x < DPX_MATRIX_W; x++)
            for (int y = 0; y < DPX_MATRIX_H; y++)
                if (dpxPixelEffect.ovBuf[x][y])
                    dpxSetPixel(x, y, dpxPixelEffect.ovBuf[x][y]);
        static unsigned long _stormFlashMs = 0;
        static int _stormFlashFrames = 0;
        if (_stormFlashFrames > 0) {
            for (int y = 0; y < DPX_MATRIX_H; y++)
                for (int x = 0; x < DPX_MATRIX_W; x++)
                    SEGMENT.setPixelColorXY(x, y, color_blend(SEGMENT.getPixelColorXY(x, y), 0xFFFFFF, 150));
            _stormFlashFrames--;
        } else if (now - _stormFlashMs > 2000 && random(200) < 5) {
            _stormFlashMs = now;
            _stormFlashFrames = 2;
        }
    }

    // ── Thunder — jagged bolt + screen shake + decaying afterglow (redesign) ──
    // Distinct from storm: mostly idle/dark, punctuated by a jagged lightning
    // bolt streaking top-to-bottom, a soft screen-wide afterglow, and a brief
    // shake of everything already on screen (text included) as if it rattled
    // the display. ovBuf holds a snapshot of the pre-shake frame so repeated
    // shifts each frame don't compound the displacement.
    else if (dpxPixelEffect.name == "thunder") {
        static int shakeFramesLeft = 0;
        static unsigned long _boltMs = 0;

        int intervalMs = map(iv, 0, 100, 60, 20);
        if (now - dpxPixelEffect.lastMs >= (unsigned long)intervalMs) {
            dpxPixelEffect.lastMs = now;
            for (int i = 0; i < 256; i++)
                dpxPixelEffect.pixbuf[i] = (dpxPixelEffect.pixbuf[i] > 18) ? dpxPixelEffect.pixbuf[i] - 18 : 0;

            int boltIntervalMs = map(iv, 0, 100, 5000, 1200);
            if (shakeFramesLeft == 0 && now - _boltMs >= (unsigned long)boltIntervalMs) {
                _boltMs = now;
                if (random(100) < 55) {
                    int x = (int)random(DPX_MATRIX_W);
                    for (int y = 0; y < DPX_MATRIX_H; y++) {
                        x += (int)random(3) - 1; // jagged left/right/straight
                        x = constrain(x, 0, DPX_MATRIX_W - 1);
                        dpxPixelEffect.pixbuf[y * DPX_MATRIX_W + x] = 255;
                    }
                    for (int i = 0; i < 256; i++) // faint screen-wide afterglow
                        if (dpxPixelEffect.pixbuf[i] < 50) dpxPixelEffect.pixbuf[i] = 50;
                    for (int x2 = 0; x2 < DPX_MATRIX_W; x2++)
                        for (int y2 = 0; y2 < DPX_MATRIX_H; y2++)
                            dpxPixelEffect.ovBuf[x2][y2] = SEGMENT.getPixelColorXY(x2, y2);
                    shakeFramesLeft = 8;
                }
            }
        }
        if (shakeFramesLeft > 0) {
            int dx = (int)random(3) - 1, dy = (int)random(3) - 1;
            for (int x = 0; x < DPX_MATRIX_W; x++) {
                for (int y = 0; y < DPX_MATRIX_H; y++) {
                    int sx = constrain(x + dx, 0, DPX_MATRIX_W - 1);
                    int sy = constrain(y + dy, 0, DPX_MATRIX_H - 1);
                    SEGMENT.setPixelColorXY(x, y, dpxPixelEffect.ovBuf[sx][sy]);
                }
            }
            shakeFramesLeft--;
        }
        for (int i = 0; i < 256; i++) {
            if (dpxPixelEffect.pixbuf[i] > 0) {
                int x = i % DPX_MATRIX_W, y = i / DPX_MATRIX_W;
                SEGMENT.setPixelColorXY(x, y,
                    color_blend(SEGMENT.getPixelColorXY(x, y), 0xFFFFFF, dpxPixelEffect.pixbuf[i]));
            }
        }
    }

    // ── Snow — buffer fall (no fade, flakes persist) + wind drift + floor pile ──
    else if (dpxPixelEffect.name == "snow") {
        int intervalMs = map(iv, 0, 100, 350, 70);
        if (now - dpxPixelEffect.lastMs >= (unsigned long)intervalMs) {
            dpxPixelEffect.lastMs = now;
            for (int x = 0; x < DPX_MATRIX_W; x++) {
                int floorY = DPX_MATRIX_H - 1 - (int)dpxPixelEffect.snow[x];
                // Per-column pile cap (deterministic, varies 3-6 rows) so the
                // floor builds up unevenly instead of one flat uniform line.
                uint8_t maxPile = 3 + ((x * 41) % 4);
                // Fall: shift occupied cells down one row unless blocked by the floor
                for (int y = DPX_MATRIX_H - 1; y > 0; y--) {
                    if (dpxPixelEffect.ovBuf[x][y - 1] && !dpxPixelEffect.ovBuf[x][y]) {
                        if (y >= floorY) {
                            if (dpxPixelEffect.snow[x] < maxPile) {
                                dpxPixelEffect.snow[x]++;
                            } else if (random(3) == 0) {
                                // Column capped out — spill into a shorter neighbor
                                // so drifts redistribute instead of just stopping.
                                int nx = x + (random(2) ? 1 : -1);
                                if (nx >= 0 && nx < DPX_MATRIX_W) {
                                    uint8_t nMax = 3 + ((nx * 41) % 4);
                                    if (dpxPixelEffect.snow[nx] < nMax) dpxPixelEffect.snow[nx]++;
                                }
                            }
                            dpxPixelEffect.ovBuf[x][y - 1] = 0;
                        } else {
                            dpxPixelEffect.ovBuf[x][y] = dpxPixelEffect.ovBuf[x][y - 1];
                            dpxPixelEffect.ovBuf[x][y - 1] = 0;
                        }
                    }
                }
                if (!dpxPixelEffect.ovBuf[x][0] && random(300) < (uint32_t)(iv + 15))
                    dpxPixelEffect.ovBuf[x][0] = 0xDDDDFF;
            }
            // Wind drift: occasionally shift a falling flake left or right
            for (int x = 0; x < DPX_MATRIX_W; x++) {
                for (int y = 0; y < DPX_MATRIX_H; y++) {
                    if (dpxPixelEffect.ovBuf[x][y] && random(8) == 0) {
                        int nx = x + (random(2) == 0 ? 1 : -1);
                        if (nx >= 0 && nx < DPX_MATRIX_W && !dpxPixelEffect.ovBuf[nx][y]) {
                            dpxPixelEffect.ovBuf[nx][y] = dpxPixelEffect.ovBuf[x][y];
                            dpxPixelEffect.ovBuf[x][y] = 0;
                        }
                    }
                }
            }
        }
        for (int x = 0; x < DPX_MATRIX_W; x++)
            for (int y = 0; y < DPX_MATRIX_H; y++)
                if (dpxPixelEffect.ovBuf[x][y])
                    dpxSetPixel(x, y, dpxPixelEffect.ovBuf[x][y]);
        // Drift pile: where it covers lit text, invert instead of painting flat
        // white over it, so buried text still reads through the drift.
        for (int x = 0; x < DPX_MATRIX_W; x++) {
            for (uint8_t py = 0; py < dpxPixelEffect.snow[x]; py++) {
                int y = DPX_MATRIX_H - 1 - py;
                uint32_t p = SEGMENT.getPixelColorXY(x, y);
                if (p) {
                    uint32_t inv = (((255 - ((p >> 16) & 0xFF)) << 16)
                                  | ((255 - ((p >>  8) & 0xFF)) <<  8)
                                  |  (255 - ( p        & 0xFF)));
                    SEGMENT.setPixelColorXY(x, y, inv);
                } else {
                    dpxSetPixel(x, y, 0xDDDDFF);
                }
            }
        }
    }

    // ── Frost — dendritic crystal growth from edge seeds (redesign #3) ────────
    // Previous versions were either scattered blobs or a flat uniform rim —
    // neither read as "crystal." Each new pixel mostly continues its parent's
    // growth direction (needle-like branches) with occasional turns to branch
    // off, which is what actually makes ice crystals look like ice crystals.
    // ovBuf is reused as scratch: stores each occupied pixel's growth
    // direction (1=+x, 2=-x, 3=+y, 4=-y).
    else if (dpxPixelEffect.name == "frost") {
        int intervalMs = map(iv, 0, 100, 200, 40);
        int target = 20 + (iv * 120) / 100;
        if (now - dpxPixelEffect.lastMs >= (unsigned long)intervalMs) {
            dpxPixelEffect.lastMs = now;
            int active = 0;
            for (int i = 0; i < 256; i++) if (dpxPixelEffect.pixbuf[i] > 0) active++;

            if (active == 0) {
                for (int s = 0; s < 5; s++) {
                    int x, y, dir;
                    switch (random(4)) {
                        case 0: x = 0;                    y = random(DPX_MATRIX_H); dir = 1; break; // grows +x
                        case 1: x = DPX_MATRIX_W - 1;      y = random(DPX_MATRIX_H); dir = 2; break; // grows -x
                        case 2: x = random(DPX_MATRIX_W);  y = 0;                    dir = 3; break; // grows +y
                        default: x = random(DPX_MATRIX_W); y = DPX_MATRIX_H - 1;     dir = 4; break; // grows -y
                    }
                    dpxPixelEffect.pixbuf[y * DPX_MATRIX_W + x] = 255;
                    dpxPixelEffect.ovBuf[x][y] = dir;
                }
            } else if (active < target) {
                static const int ddx[5] = {0, 1, -1, 0, 0};
                static const int ddy[5] = {0, 0, 0, 1, -1};
                for (int attempt = 0; attempt < 8; attempt++) {
                    int i = (int)random(256);
                    if (dpxPixelEffect.pixbuf[i] == 0) continue;
                    int x = i % DPX_MATRIX_W, y = i / DPX_MATRIX_W;
                    int dir = (int)dpxPixelEffect.ovBuf[x][y];
                    if (dir == 0) dir = 1 + (int)random(4);
                    int newDir = (random(4) < 3) ? dir : 1 + (int)random(4); // mostly straight, sometimes branch
                    int nx = x + ddx[newDir], ny = y + ddy[newDir];
                    if (nx >= 0 && nx < DPX_MATRIX_W && ny >= 0 && ny < DPX_MATRIX_H &&
                        dpxPixelEffect.pixbuf[ny * DPX_MATRIX_W + nx] == 0) {
                        dpxPixelEffect.pixbuf[ny * DPX_MATRIX_W + nx] = 200 + random(56);
                        dpxPixelEffect.ovBuf[nx][ny] = newDir;
                        break;
                    }
                }
            } else if (random(4) == 0) {
                int i = (int)random(256);
                if (dpxPixelEffect.pixbuf[i] > 0)
                    dpxPixelEffect.pixbuf[i] = (dpxPixelEffect.pixbuf[i] > 60) ? dpxPixelEffect.pixbuf[i] - 60 : 0;
            }
        }
        for (int i = 0; i < 256; i++) {
            if (dpxPixelEffect.pixbuf[i] > 0) {
                uint32_t c = dpxOceanColor((uint8_t)((i * 37) & 0xFF));
                SEGMENT.setPixelColorXY(i % DPX_MATRIX_W, i / DPX_MATRIX_W,
                    color_blend(SEGMENT.getPixelColorXY(i % DPX_MATRIX_W, i / DPX_MATRIX_W), c, dpxPixelEffect.pixbuf[i]));
            }
        }
    }

    // ── ColorWaves — horizontal color gradient animation ───────────────────
    else if (dpxPixelEffect.name == "colorwaves") {
        for (int x = 0; x < DPX_MATRIX_W; x++) {
            for (int y = 0; y < DPX_MATRIX_H; y++) {
                uint8_t hue = (uint8_t)((x * 255 / DPX_MATRIX_W) + now / 10);
                uint32_t c = dpxHueColor(hue);
                SEGMENT.setPixelColorXY(x, y,
                    color_blend(SEGMENT.getPixelColorXY(x, y), c, map(iv, 0, 100, 80, 200)));
            }
        }
    }

    // ── Plasma — overlapping sine + radial waves, rotating hue (redesign) ──
    // The 3-term average was too flat/muddy. Adds a 4th radial term, contrast-
    // stretches the result, and rotates a global hue offset over time so the
    // palette keeps cycling instead of sitting on one static-looking gradient.
    // Where it crosses lit text it inverts instead of blending, so text stays
    // legible against the noise.
    else if (dpxPixelEffect.name == "plasma") {
        static double plasmaTime = 0;
        plasmaTime += map(iv, 0, 100, 2, 16) * 0.01;
        uint8_t hueShift = (uint8_t)(plasmaTime * 25);
        for (int x = 0; x < DPX_MATRIX_W; x++) {
            for (int y = 0; y < DPX_MATRIX_H; y++) {
                uint32_t existing = SEGMENT.getPixelColorXY(x, y);
                if (existing) {
                    uint32_t inv = (((255 - ((existing >> 16) & 0xFF)) << 16)
                                  | ((255 - ((existing >>  8) & 0xFF)) <<  8)
                                  |  (255 - ( existing        & 0xFF)));
                    SEGMENT.setPixelColorXY(x, y, inv);
                    continue;
                }
                int cx = x - DPX_MATRIX_W / 2, cy = y - DPX_MATRIX_H / 2;
                uint8_t radial = sin8_t((uint8_t)((int)sqrtf((float)(cx * cx + cy * cy)) * 12 + plasmaTime * 60));
                int sum = sin8_t((uint8_t)(x * 14 + plasmaTime * 70))
                        + sin8_t((uint8_t)(y * 14 + plasmaTime * 55))
                        + sin8_t((uint8_t)((x + y) * 9 + plasmaTime * 40))
                        + radial;
                uint8_t value = (uint8_t)(sum / 4);
                int stretched = ((int)value - 128) * 2 + 128; // widen contrast around midpoint
                value = (uint8_t)constrain(stretched, 0, 255);
                uint32_t c = dpxHueColor((uint8_t)(value + hueShift));
                SEGMENT.setPixelColorXY(x, y,
                    color_blend(SEGMENT.getPixelColorXY(x, y), c, map(iv, 0, 100, 120, 220)));
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
                // Fixed warm amber tone (hue ~30) — brightness varies per star, not hue.
                CRGBW rgb = CHSV(30, 230, 255);
                uint32_t c = (uint32_t)rgb.r << 16 | (uint32_t)rgb.g << 8 | rgb.b;
                SEGMENT.setPixelColorXY(i % DPX_MATRIX_W, i / DPX_MATRIX_W,
                    color_blend(SEGMENT.getPixelColorXY(i % DPX_MATRIX_W, i / DPX_MATRIX_W), c, bri));
            }
        }
    }

    // ── TheaterChase — marquee-style chase around the screen border ────────
    // Chases lights around the perimeter ring only, leaving the interior
    // untouched. Color slowly breathes between solid amber and a full rainbow
    // sweep and back (~10s cycle) instead of sitting on one look.
    else if (dpxPixelEffect.name == "theatrechase") {
        int intervalMs = map(iv, 0, 100, 160, 30);
        if (now - dpxPixelEffect.lastMs >= (unsigned long)intervalMs) {
            dpxPixelEffect.lastMs = now;
            dpxPixelEffect.pulsePhase = (dpxPixelEffect.pulsePhase + 1) % 3;
        }
        const int perim = 2 * (DPX_MATRIX_W + DPX_MATRIX_H) - 4;
        uint8_t rainbowMix = sin8_t((uint8_t)(now / 40)); // ~10s amber<->rainbow<->amber
        CRGBW amberRgb = CHSV(30, 230, 255);
        uint32_t amber = (uint32_t)amberRgb.r << 16 | (uint32_t)amberRgb.g << 8 | amberRgb.b;
        for (int p = 0; p < perim; p++) {
            if ((p + dpxPixelEffect.pulsePhase) % 3 == 0) {
                int x, y;
                dpxPerimXY(p, x, y);
                uint32_t rainbow = dpxHueColor((uint8_t)(p * 255 / perim));
                uint32_t c = color_blend(amber, rainbow, rainbowMix);
                SEGMENT.setPixelColorXY(x, y, color_blend(SEGMENT.getPixelColorXY(x, y), c, 220));
            }
        }
    }

    // ── Pacifica — wavy ocean simulation ───────────────────────────────────
    else if (dpxPixelEffect.name == "pacifica") {
        static uint32_t pacificaTime = 0;
        pacificaTime += map(iv, 0, 100, 1, 3); // slower drift than the original 1-5 range
        for (int x = 0; x < DPX_MATRIX_W; x++) {
            for (int y = 0; y < DPX_MATRIX_H; y++) {
                uint16_t ulx = (pacificaTime / 8) - (x * 16);
                uint16_t uly = (pacificaTime / 4) + (y * 16);
                uint16_t v = 0;
                v += sin16_t(ulx * 6 + pacificaTime / 2) / 8 + 127;
                v += sin16_t(uly * 9 + pacificaTime / 2) / 8 + 127;
                v += sin16_t(ulx * 7 + uly * 2 - pacificaTime) / 16;
                v = v / 3;
                uint32_t c = dpxOceanColor((uint8_t)v);
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
