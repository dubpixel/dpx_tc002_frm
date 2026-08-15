// ================================================================================
// dpx_pair.h — Device Pairing PIN Display
// ================================================================================
// Original work — dubpixel / dpx_tc002 (EUPL v1.2)
// ================================================================================
// PROJECT: dpx_tc002_frm
// ================================================================================
//
// File: dpx_pair.h
// Purpose: Full-screen, large-font PIN display used by the friendster/cuemaster
//          server's device-claim flow (see dpx_tc002_server.md — "Accounts,
//          Device Pairing & Admin Roster"). The SERVER picks the PIN and tells
//          the device to show it; the device never generates or verifies it —
//          it's just a display. The user reads the PIN off the physical screen
//          and types it into the claim UI, proving physical possession.
//
// Trigger:
//   POST /api/pair  {"pin":"482913","duration":60,"scale":1}
//     duration: secs, default 60
//     scale:    1 (default) or 2 — see dpxRenderPair() for why 1x is the
//               safer default (most glyphs are 5px tall; 2x clips against
//               the 8-row display and hasn't been visually confirmed to
//               read well). Caller can opt into 2x once that's checked.
//   MQTT .../dpx/pair  same JSON payload
//   Clear early: POST /api/pair {}  or  MQTT .../dpx/pair with empty/"" payload
//
// Takes priority over everything else on screen (even notifications) and
// auto-expires — see dpxPairActive(). Suppresses text overlay + pixel effects
// while active so the PIN stays legible.
//
// ================================================================================

#pragma once
#include "dpx_text.h"

static struct {
    bool     active    = false;
    String   pin;
    int      scale     = 1;
    unsigned long expiresMs = 0;
} dpxPairState;

// Starts showing `pin` full-screen for `durationSec` seconds (default 60).
// scale: 1 (default, safe) or 2 (large, may clip — see file header).
static void dpxStartPair(const String& pin, int durationSec = 60, int scale = 1) {
    if (!pin.length()) { dpxPairState.active = false; return; }
    dpxPairState.pin       = pin;
    dpxPairState.scale     = constrain(scale, 1, 2);
    dpxPairState.expiresMs = millis() + (unsigned long)max(1, durationSec) * 1000UL;
    dpxPairState.active    = true;
}

static void dpxStopPair() { dpxPairState.active = false; }

// True while the pairing display should be shown; auto-clears on expiry.
static bool dpxPairActive() {
    if (dpxPairState.active && millis() >= dpxPairState.expiresMs) dpxPairState.active = false;
    return dpxPairState.active;
}

// Full-screen, centered PIN at dpxPairState.scale. No scroll — pairing PINs
// are short enough (4-6 digits) to always fit even at 2x scale width-wise;
// it's vertical clipping at 2x (10px glyphs on an 8-row display) that's the
// open question, which is why 1x is the default rather than assuming 2x reads fine.
static void dpxRenderPair() {
    dpxClear();
    int scale = dpxPairState.scale;
    int w = dpxTextPixelWidth(dpxPairState.pin.c_str(), scale);
    int x = (DPX_MATRIX_W - w) / 2;
    if (x < 0) x = 0;
    dpxRenderText(x, DPX_FONT_BASELINE, dpxPairState.pin.c_str(), 0x00FFAA, false, 0, scale);
}

// Parse {"pin":"...", "duration":N, "scale":1|2} (or empty body / {} to clear).
static bool dpxSetPair(const char* json) {
    if (!json || strlen(json) <= 2) { dpxStopPair(); return true; }
    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, json)) return false;
    String pin = doc["pin"] | String();
    if (!pin.length()) { dpxStopPair(); return true; }
    int duration = doc["duration"] | 60;
    int scale    = doc["scale"] | 1;
    dpxStartPair(pin, duration, scale);
    return true;
}
