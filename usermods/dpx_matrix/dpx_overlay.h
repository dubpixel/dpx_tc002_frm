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
//      Effects: "sparkle", "strobe", "rain", "twinkle", "blink", "none"
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
    uint8_t  rain[DPX_MATRIX_W] = {}; // rain column heights
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
    dpxPixelEffect.lastMs    = 0;
    dpxPixelEffect.strobeOn  = true;
    memset(dpxPixelEffect.rain, 0, sizeof(dpxPixelEffect.rain));
    return true;
}

static void dpxClearPixelEffect() { dpxPixelEffect.active = false; }

// Render one frame of the active pixel effect on top of current LED content.
static void dpxRenderPixelEffect() {
    if (!dpxPixelEffect.active) return;

    unsigned long now = millis();
    uint32_t col = dpxPixelEffect.color;
    uint8_t  iv  = dpxPixelEffect.intensity;

    // ── Sparkle ────────────────────────────────────────────────────────────
    if (dpxPixelEffect.name == "sparkle") {
        // Each frame, light up ~intensity/4 random pixels white then fade
        int count = max(1, (int)(iv / 4));
        for (int i = 0; i < count; i++) {
            int px = (int)(random(256));
            strip.setPixelColor(px, col);
        }
    }

    // ── Twinkle ────────────────────────────────────────────────────────────────
    else if (dpxPixelEffect.name == "twinkle") {
        int count = max(1, (int)(iv / 8));
        for (int i = 0; i < count; i++) {
            int px = (int)(random(256));
            // Blend with existing pixel
            uint32_t existing = strip.getPixelColor(px);
            strip.setPixelColor(px, color_blend(existing, col, 180));
        }
    }

    // ── Strobe ────────────────────────────────────────────────────────────
    else if (dpxPixelEffect.name == "strobe") {
        // Flash on/off at speed derived from intensity
        int intervalMs = map(iv, 0, 100, 400, 40);
        if (now - dpxPixelEffect.lastMs >= (unsigned long)intervalMs) {
            dpxPixelEffect.lastMs  = now;
            dpxPixelEffect.strobeOn = !dpxPixelEffect.strobeOn;
        }
        if (dpxPixelEffect.strobeOn) {
            for (int i = 0; i < 256; i++)
                strip.setPixelColor(i, color_blend(strip.getPixelColor(i), col, 200));
        }
    }

    // ── Blink ─────────────────────────────────────────────────────────────
    else if (dpxPixelEffect.name == "blink") {
        int intervalMs = map(iv, 0, 100, 1000, 200);
        if (now - dpxPixelEffect.lastMs >= (unsigned long)intervalMs) {
            dpxPixelEffect.lastMs  = now;
            dpxPixelEffect.strobeOn = !dpxPixelEffect.strobeOn;
        }
        if (!dpxPixelEffect.strobeOn) {
            for (int i = 0; i < 256; i++) strip.setPixelColor(i, 0);
        }
    }

    // ── Rain ──────────────────────────────────────────────────────────────
    else if (dpxPixelEffect.name == "rain") {
        int intervalMs = map(iv, 0, 100, 200, 20);
        if (now - dpxPixelEffect.lastMs >= (unsigned long)intervalMs) {
            dpxPixelEffect.lastMs = now;
            // Advance each column's raindrop
            for (int x = 0; x < DPX_MATRIX_W; x++) {
                if (dpxPixelEffect.rain[x] > 0) {
                    // Erase old head, draw new
                    int y = dpxPixelEffect.rain[x] - 1;
                    if (y < DPX_MATRIX_H) dpxSetPixel(x, y, 0);
                    dpxPixelEffect.rain[x]++;
                    if ((int)dpxPixelEffect.rain[x] - 1 < DPX_MATRIX_H)
                        dpxSetPixel(x, dpxPixelEffect.rain[x] - 1, col);
                    if ((int)dpxPixelEffect.rain[x] > DPX_MATRIX_H + 1)
                        dpxPixelEffect.rain[x] = 0; // reset column
                }
                // Randomly start new drops
                if (dpxPixelEffect.rain[x] == 0 && (random(256) < (iv * 2)))
                    dpxPixelEffect.rain[x] = 1;
            }
        }
    }
}

// ── Master overlay render — call at end of handleOverlayDraw() ────────────────
static void dpxRenderOverlays() {
    dpxRenderPixelEffect();  // pixel effect first (under text)
    dpxRenderTextOverlay();  // text on top
}
