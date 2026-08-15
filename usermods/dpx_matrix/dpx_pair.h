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
//   POST /api/pair          {"pin":"482913","duration":60}   (duration secs, default 60)
//   MQTT .../dpx/pair        same JSON payload
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
    unsigned long expiresMs = 0;
} dpxPairState;

// Starts showing `pin` full-screen for `durationSec` seconds (default 60).
static void dpxStartPair(const String& pin, int durationSec = 60) {
    if (!pin.length()) { dpxPairState.active = false; return; }
    dpxPairState.pin       = pin;
    dpxPairState.expiresMs = millis() + (unsigned long)max(1, durationSec) * 1000UL;
    dpxPairState.active    = true;
}

static void dpxStopPair() { dpxPairState.active = false; }

// True while the pairing display should be shown; auto-clears on expiry.
static bool dpxPairActive() {
    if (dpxPairState.active && millis() >= dpxPairState.expiresMs) dpxPairState.active = false;
    return dpxPairState.active;
}

// Full-screen, large (2x), centered PIN. No scroll — pairing PINs are short
// enough (4-6 digits) to always fit even at 2x scale.
static void dpxRenderPair() {
    dpxClear();
    const int scale = 2;
    int w = dpxTextPixelWidth(dpxPairState.pin.c_str(), scale);
    int x = (DPX_MATRIX_W - w) / 2;
    if (x < 0) x = 0;
    dpxRenderText(x, DPX_FONT_BASELINE, dpxPairState.pin.c_str(), 0x00FFAA, false, 0, scale);
}

// Parse {"pin":"...", "duration":N} (or empty body / {} to clear) from JSON.
static bool dpxSetPair(const char* json) {
    if (!json || strlen(json) <= 2) { dpxStopPair(); return true; }
    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, json)) return false;
    String pin = doc["pin"] | String();
    if (!pin.length()) { dpxStopPair(); return true; }
    int duration = doc["duration"] | 60;
    dpxStartPair(pin, duration);
    return true;
}
