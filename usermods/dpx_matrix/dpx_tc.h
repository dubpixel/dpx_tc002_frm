// ================================================================================
// dpx_tc.h — Timecode Display Helper
// ================================================================================
// Original work — dubpixel / dpx_tc002 (EUPL v1.2)
// Ported/adapted from dpx_tc001 ServerManager.cpp::pushTCDisplay()
// (original work, not AWTRIX-derived)
// ================================================================================
// PROJECT: dpx_tc002_frm
// ================================================================================
//
// File: dpx_tc.h
// Purpose: Parses HH:MM:SS:FF timecode strings, renders them on the matrix,
//          and manages the TC display lock (auto-switches to tc app, disables
//          auto-transition, auto-restores after DPX_TC_DWELL_MS).
//
// Mode A (DPX_TC_SHOW_FRAMES=false, default):
//   Renders HH:MM:SS as text. Bottom 2 rows (6-7) show a frame-progress bar
//   that fills left-to-right green→amber as frames advance within the second.
//
// Mode B (DPX_TC_SHOW_FRAMES=true):
//   Renders compact MM:SS.FF — fits 32px wide, no bar.
//
// ================================================================================

#pragma once
#include "dpx_apps.h"

static unsigned long dpxTcLastMs   = 0;    // millis() of last TC packet
static bool          dpxTcLocked   = false; // display locked to TC app
static uint8_t       _tcSavedEffect = 255;  // WLED effect mode before TC takeover (1.15)

// Parse "HH:MM:SS:FF" or "HH:MM:SS,FF" into components.
// Returns false if parse failed.
static bool dpxParseTc(const String& tc, int& h, int& m, int& s, int& f) {
    h = m = s = f = 0;
    return sscanf(tc.c_str(), "%d:%d:%d%*c%d", &h, &m, &s, &f) >= 3;
}

// Push a timecode packet — builds a custom app JSON and sets "tc" app.
static void dpxPushTC(const String& tc, int fps = 30) {
    if (DPX_TC_MUTE) return;  // 1.14: mute suppresses all TC display
    int h, m, s, f;
    if (!dpxParseTc(tc, h, m, s, f)) return;
    if (f >= fps) f = fps - 1;

    DpxCustomApp app;
    app.valid    = true;
    app.noScroll = true;
    app.color    = 0xFFFFFF;
    app.topText  = true;

    if (DPX_TC_SHOW_FRAMES) {
        // Mode B: MM:SS.FF
        char buf[12];
        snprintf(buf, sizeof(buf), "%02d:%02d.%02d", m, s, f);
        app.text = buf;
    } else {
        // Mode A: HH:MM:SS + frame progress bar
        char buf[12];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
        app.text = buf;

        // Build progress bar as draw commands
        int barW = (f * DPX_MATRIX_W) / fps;
        // Background bar — rows 6-7
        DpxDrawCmd bg;
        bg.cmd = "df"; bg.n[0]=0; bg.n[1]=6; bg.n[2]=DPX_MATRIX_W; bg.n[3]=2; bg.color=0x1a1a1a;
        app.drawCmds.push_back(bg);
        // Progress fill — green→amber
        if (barW > 0) {
            DpxDrawCmd bar;
            bar.cmd = "df"; bar.n[0]=0; bar.n[1]=6; bar.n[2]=barW; bar.n[3]=2;
            uint8_t r = (uint8_t)((barW * 255) / DPX_MATRIX_W);
            uint8_t g = (uint8_t)(200 - (barW * 100) / DPX_MATRIX_W);
            bar.color = ((uint32_t)r << 16) | ((uint32_t)g << 8);
            app.drawCmds.push_back(bar);
        }
    }

    // Install as custom app "tc"
    dpxCustom["tc"] = app;
    dpxRebuildLoop();

    // Lock display to tc app
    dpxTcLastMs = millis();
    if (!dpxTcLocked) {
        // Switch to tc app and disable auto-transition
        for (int i = 0; i < (int)dpxApps.size(); i++) {
            if (dpxApps[i].name == "tc") {
                dpxCurrentApp = i;
                dpxAppStartMs = millis();
                dpxScroll.stop();
                break;
            }
        }
        dpxAutoTrans = false;
        dpxTcLocked  = true;
        // 1.15: save current effect and force dpx_matrix
        _tcSavedEffect = strip.getMainSegment().mode;
        dpxActivateEffect();  // ensure dpx Matrix effect is showing
    }
}

// TC dwell tick — call from loop(). Restores auto-transition after dwell.
static void dpxTcDwellTick() {
    if (!dpxTcLocked) return;
    if (DPX_TC_HOLD) return; // never auto-dismiss in hold mode
    if (millis() - dpxTcLastMs > DPX_TC_DWELL_MS) {
        // Remove tc app and restore auto-transition
        dpxCustom.erase("tc");
        dpxRebuildLoop();
        dpxAutoTrans = true;
        dpxTcLocked  = false;
        dpxNextApp();
        // 1.15: restore the effect that was active before TC took over
        if (_tcSavedEffect != 255 && _tcSavedEffect != strip.getMainSegment().mode) {
            strip.getMainSegment().setMode(_tcSavedEffect);
            stateUpdated(CALL_MODE_DIRECT_CHANGE);
            _tcSavedEffect = 255;
        }
        if (DPX_TC_STOP_BEEP) dpxBuzzerPlay("TC:d=32,o=5,b=200:c,p,c");
    }
}
