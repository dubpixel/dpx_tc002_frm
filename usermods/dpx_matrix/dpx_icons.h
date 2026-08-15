// ================================================================================
// dpx_icons.h — 8×8 Icon Loading + Rendering
// ================================================================================
// Original work — dubpixel / dpx_tc002 (EUPL v1.2)
// ================================================================================
// PROJECT: dpx_tc002_frm
// ================================================================================
//
// File: dpx_icons.h
// Purpose: Load raw RGB888 8x8 icons from LittleFS /ICONS/<name>.raw and blit
//          them to the matrix. Browser-side <canvas> does PNG→raw conversion at
//          install time (dpx_html.h) so no PNG decoder is needed on-device.
//
//          Icon files are exactly 192 bytes: 64 pixels, row-major top-to-bottom,
//          3 bytes/pixel (R,G,B). A pure-black pixel (0,0,0) is treated as
//          transparent so the app background shows through.
//
// ================================================================================

#pragma once
#include "dpx_text.h"

static const int DPX_ICON_W       = 8;
static const int DPX_ICON_H       = 8;
static const int DPX_ICON_PIXELS  = DPX_ICON_W * DPX_ICON_H;   // 64
static const int DPX_ICON_BYTES   = DPX_ICON_PIXELS * 3;       // 192 (raw RGB888)

// Reads /ICONS/<name>.raw into `out` (must hold DPX_ICON_PIXELS uint32_t 0xRRGGBB
// colors). Returns false if the file is missing or not exactly DPX_ICON_BYTES.
static bool dpxLoadIcon(const String& name, uint32_t* out) {
    if (!name.length()) return false;
    String path = "/ICONS/" + name + ".raw";
    if (!LittleFS.exists(path)) return false;
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    if (f.size() != (size_t)DPX_ICON_BYTES) { f.close(); return false; }
    uint8_t buf[DPX_ICON_BYTES];
    f.read(buf, DPX_ICON_BYTES);
    f.close();
    for (int i = 0; i < DPX_ICON_PIXELS; i++) {
        out[i] = ((uint32_t)buf[i * 3] << 16) | ((uint32_t)buf[i * 3 + 1] << 8) | buf[i * 3 + 2];
    }
    return true;
}

// Blits an 8×8 icon (row-major, pixels[0] = top-left) to the matrix at (x,y).
// Black pixels (0x000000) are skipped so background/app content shows through.
static void dpxRenderIcon(const uint32_t* pixels, int x, int y) {
    for (int row = 0; row < DPX_ICON_H; row++) {
        for (int col = 0; col < DPX_ICON_W; col++) {
            uint32_t c = pixels[row * DPX_ICON_W + col];
            if (c) dpxSetPixel(x + col, y + row, c);
        }
    }
}

// ── Single-slot cache — avoids re-reading flash every frame while the same
// icon stays on screen. Reloads automatically when the requested name changes.
static struct {
    String   name;
    uint32_t pixels[DPX_ICON_PIXELS];
    bool     loaded = false;
} dpxIconCache;

// Returns a pointer to the cached pixel buffer for `name`, loading it from
// flash if not already cached. Returns nullptr if name is empty or load fails.
static const uint32_t* dpxGetIcon(const String& name) {
    if (!name.length()) return nullptr;
    if (dpxIconCache.loaded && dpxIconCache.name == name) return dpxIconCache.pixels;
    if (dpxLoadIcon(name, dpxIconCache.pixels)) {
        dpxIconCache.name   = name;
        dpxIconCache.loaded = true;
        return dpxIconCache.pixels;
    }
    dpxIconCache.loaded = false;
    return nullptr;
}
