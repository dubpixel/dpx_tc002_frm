// ================================================================================
// dpx_text.h — Text Rendering for 32×8 LED Matrix
// ================================================================================
// Original work — dubpixel / dpx_tc002 (EUPL v1.2)
// ================================================================================
// PROJECT: dpx_tc002_frm
// ================================================================================
//
// File: dpx_text.h
// Purpose: Pixel-level text rendering using dpx_font.h onto WLED's LED buffer.
//          Functions operate on a 32-pixel-wide, 8-pixel-tall matrix.
//          Pixel (x, y) = strip index y*32 + x.
//          Caller must hold the strip lock / call from handleOverlayDraw().
//
// ================================================================================

#pragma once
#include "dpx_font.h"

// Matrix dimensions — Ulanzi TC001
static const int DPX_MATRIX_W = 32;
static const int DPX_MATRIX_H = 8;

// ── Pixel write via SEGMENT ────────────────────────────────────────────────────
// Using SEGMENT.setPixelColorXY() instead of strip.setPixelColor() means WLED
// handles brightness, transitions, power-fade, and 2D panel mapping (incl.
// serpentine) automatically. Only valid from within an effect function context.
static inline void dpxSetPixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= DPX_MATRIX_W || y < 0 || y >= DPX_MATRIX_H) return;
    SEGMENT.setPixelColorXY(x, y, color);
}

static inline uint32_t dpxGetPixel(int x, int y) {
    if (x < 0 || x >= DPX_MATRIX_W || y < 0 || y >= DPX_MATRIX_H) return 0;
    return SEGMENT.getPixelColorXY(x, y);
}

// ── Fill / clear ───────────────────────────────────────────────────────────────
static inline void dpxFillRect(int x, int y, int w, int h, uint32_t color) {
    for (int row = y; row < y + h; row++)
        for (int col = x; col < x + w; col++)
            SEGMENT.setPixelColorXY(col, row, color);
}

static inline void dpxClear() { SEGMENT.fill(0); }

// ── Rainbow colour for a character index ──────────────────────────────────────
static inline uint32_t dpxRainbowColor(int charIdx, int totalChars) {
    // Evenly distribute hue across the string.
    // WLED's CRGBW(CHSV) constructor applies the rainbow-method conversion.
    uint8_t hue = (totalChars > 1)
        ? (uint8_t)((uint32_t)charIdx * 255 / totalChars)
        : 0;
    CRGBW rgb = CHSV(hue, 255, 255);
    return (uint32_t)rgb.r << 16 | (uint32_t)rgb.g << 8 | rgb.b;
}

// ── Draw a single glyph using AwtrixFont (GFX row-major format) ───────────────
// x         = left edge of where the glyph will land
// baseline  = cursor_y (pass DPX_FONT_BASELINE to centre in 8-row display)
// minX      = left clip boundary (pixels before this column are not drawn) —
//             used to keep scrolling text from running under a fixed icon.
// scale     = integer pixel-doubling factor (GH #19/#63 "bigger font"; no new
//             font data — each source pixel becomes a scale x scale block).
//             Most glyphs are 5px tall, so scale=2 (10px) is taller than the
//             8-row matrix and clips.
//
//             IMPORTANT: yOffset is scaled but measured from a SHARED
//             baseline anchor for every glyph (not each glyph's own
//             midpoint) — this is what keeps x-height letters, ascenders,
//             and descenders (p/q/j/y) correctly positioned *relative to
//             each other* on the same line at any scale. An earlier attempt
//             centered each glyph independently on its own bounding box,
//             which looked "fixed" for a single character in isolation but
//             broke cross-glyph alignment badly — descenders no longer hung
//             below the baseline relative to x-height letters, ascenders
//             and descenders drifted to different vertical offsets from
//             each other, and the resulting unevenness read as letters
//             being oddly spaced/"too wide" even though xAdvance itself was
//             untouched and correct (audited: E/W/N/L vs A/H/T all have the
//             same 1px trailing gap past their actual bitmap content —
//             W/N/E are just inherently wider letterforms by design).
//
//             The baseline itself shifts down as scale grows (see
//             effBaseline below) so a scale=2 typical caps-height glyph
//             (yOffset=-5, height=5 unscaled) clips symmetrically ~1px off
//             both top and bottom instead of ~4px off the top only with 2px
//             of unused space at the bottom (the pre-this-fix behavior).
//             dpxSetPixel's own bounds check handles the actual clipping.
// Returns the next cursor X (x + xAdvance*scale).
// Glyphs that land partially off-left are clipped pixel-by-pixel.
static int dpxDrawChar(int x, int baseline, char c, uint32_t color, int minX = 0, int scale = 1) {
    extern const GFXfont AwtrixFont;
    uint8_t ci = (uint8_t)c;
    if (ci < AwtrixFont.first || ci > AwtrixFont.last) return x + 4 * scale;

    GFXglyph g;
    memcpy_P(&g, &AwtrixFont.glyph[ci - AwtrixFont.first], sizeof(GFXglyph));

    // Each row of the glyph is ceil(g.width/8) bytes; here always 1 byte (width=8)
    int bytesPerRow = (g.width + 7) / 8;
    // Shift the baseline down as scale grows so a typical full-height glyph
    // (yOffset=-5, height=5 at scale=1) clips symmetrically top/bottom
    // instead of running off the top. Derived from centering that typical
    // glyph's scaled span in the 8-row matrix: effBaseline(S) = 4 + 2.5*S,
    // expressed as an integer delta from the scale=1 baseline so scale=1
    // is untouched (delta=0). scale=2 -> effBaseline = baseline+3.
    int effBaseline = baseline + (5 * (scale - 1) + 1) / 2;
    int glyphTop    = effBaseline + g.yOffset * scale; // shared-anchor scaling, top pixel row on matrix

    for (int row = 0; row < (int)g.height; row++) {
        for (int b = 0; b < bytesPerRow; b++) {
            uint8_t bits = pgm_read_byte(
                &((uint8_t*)AwtrixFont.bitmap)[g.bitmapOffset + row * bytesPerRow + b]);
            for (int bit = 7; bit >= 0; bit--) {
                int col = (b * 8) + (7 - bit);
                if (col >= (int)g.width) break;
                if (!(bits & (1 << bit))) continue;
                int baseX = x + (g.xOffset + col) * scale;
                int baseY = glyphTop + row * scale;
                for (int sy = 0; sy < scale; sy++) {
                    int py = baseY + sy;
                    if (py < 0 || py >= DPX_MATRIX_H) continue;
                    for (int sx = 0; sx < scale; sx++) {
                        int px = baseX + sx;
                        if (px < minX || px >= DPX_MATRIX_W) continue;
                        dpxSetPixel(px, py, color);
                    }
                }
            }
        }
    }
    return x + g.xAdvance * scale;
}

// ── Render a text string ───────────────────────────────────────────────────────
// x        = left cursor position
// baseline = cursor_y (use DPX_FONT_BASELINE for centered output)
// rainbow  = true overrides color with per-character hue sweep
// minX     = left clip boundary, see dpxDrawChar
// scale    = pixel-doubling factor, see dpxDrawChar
// Returns the x position after the last character.
static int dpxRenderText(int x, int baseline, const char* text, uint32_t color, bool rainbow = false, int minX = 0, int scale = 1) {
    if (!text) return x;
    int n = strlen(text);
    int curX = x;
    for (int i = 0; i < n; i++) {
        uint32_t c = rainbow ? dpxRainbowColor(i, n) : color;
        curX = dpxDrawChar(curX, baseline, text[i], c, minX, scale);
        if (curX >= DPX_MATRIX_W) break;
    }
    return curX;
}

// ── Scroll state (one active scroll per display) ───────────────────────────────
struct DpxScrollState {
    String  text;
    uint32_t color     = 0xFFFFFF;
    bool    rainbow    = false;
    int     y          = DPX_FONT_BASELINE; // baseline row (centred in 8-row display)
    int     scrollX    = DPX_MATRIX_W; // current x offset (starts at right edge)
    int     speedMs    = 50;         // ms between scroll steps (lower = faster)
    unsigned long lastStepMs = 0;
    bool    active     = false;
    bool    completed  = false;      // true when finite repeat cycle finished naturally
    int16_t repeat     = -1;         // scroll repeat count (-1 = infinite)
    int16_t repeatsDone = 0;
    int     textWidth  = 0;         // cached pixel width
    int     scale      = 1;         // GH #19/#63 pixel-doubling factor

    void start(const String& t, uint32_t col, bool rb, int y_, int speedPct, int16_t rep, int scale_ = 1) {
        text       = t;
        color      = col;
        rainbow    = rb;
        y          = (y_ == 0) ? DPX_FONT_BASELINE : y_; // default to centred baseline
        speedMs    = max(10, (int)(50 * 100 / max(1, speedPct)));
        repeat     = rep;
        repeatsDone = 0;
        scale      = scale_;
        textWidth  = dpxTextPixelWidth(t.c_str(), scale);
        scrollX    = DPX_MATRIX_W;
        lastStepMs = 0;
        active     = true;
        completed  = false;
    }

    void stop() { active = false; completed = false; }

    // Returns true when the current scroll cycle is complete (scrolled off left).
    // Caller should call again to advance another repeat or stop.
    bool tick() {
        if (!active) return false;
        unsigned long now = millis();
        if (now - lastStepMs < (unsigned long)speedMs) return false;
        lastStepMs = now;
        scrollX--;
        // Complete when text has fully scrolled off left with a half-screen
        // trailing gap — gives a clean blank before wrap/end (issue #51)
        if (scrollX < -(textWidth + DPX_MATRIX_W / 2)) {
            repeatsDone++;
            if (repeat >= 0 && repeatsDone >= repeat) {
                active    = false;
                completed = true;
                return true; // done
            }
            scrollX = DPX_MATRIX_W; // wrap
        }
        return false; // still running
    }

    // Render current scroll position. Call after tick().
    // minX: left clip boundary, see dpxDrawChar — keeps text from running under a fixed icon.
    void render(int minX = 0) const {
        if (!active) return;
        dpxRenderText(scrollX, y, text.c_str(), color, rainbow, minX, scale);
    }
};

// ── Draw a horizontal line (e.g., progress bar) ───────────────────────────────
static inline void dpxDrawHLine(int x, int y, int w, uint32_t color) {
    for (int i = 0; i < w; i++) dpxSetPixel(x + i, y, color);
}

// ── Draw a filled rectangle outline ──────────────────────────────────────────
static inline void dpxDrawRect(int x, int y, int w, int h, uint32_t color) {
    for (int i = 0; i < w; i++) { dpxSetPixel(x+i, y, color); dpxSetPixel(x+i, y+h-1, color); }
    for (int i = 1; i < h-1; i++) { dpxSetPixel(x, y+i, color); dpxSetPixel(x+w-1, y+i, color); }
}

// ── Progress bar: rows 6-7, full width 32px ───────────────────────────────────
// pct 0-100, barColor = fill, bgColor = background
static void dpxDrawProgressBar(int pct, uint32_t barColor, uint32_t bgColor) {
    int filled = (pct * DPX_MATRIX_W) / 100;
    filled = constrain(filled, 0, DPX_MATRIX_W);
    for (int x = 0; x < DPX_MATRIX_W; x++) {
        uint32_t c = (x < filled) ? barColor : bgColor;
        dpxSetPixel(x, 6, c);
        dpxSetPixel(x, 7, c);
    }
}
