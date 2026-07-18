// ================================================================================
// dpx_matrix.h — Main Usermod Class
// ================================================================================
// Original work — dubpixel / dpx_tc002 (MIT)
// ================================================================================
// PROJECT: dpx_tc002_frm
// ================================================================================
//
// File: dpx_matrix.h
// Purpose: WLED Usermod class that takes over the 32×8 LED matrix to display
//          text apps, scrolling text, timecode, and notifications. Also registers
//          custom HTTP API routes and an OSC UDP receiver.
//
// Activate in platformio_override.ini:
//   custom_usermods = dpx_matrix
//
// WLED setup required (via WLED web UI or startup config):
//   - LED Type:  WS2812B (RGB)
//   - GPIO:      32
//   - LED Count: 256
//   - 2D Matrix: width=32, height=8, serpentine=no
//
// ================================================================================

#pragma once

// Include all module headers in dependency order
#include "dpx_font.h"
#include "dpx_text.h"
#include "dpx_persist.h"
#include "dpx_apps.h"
#include "dpx_notifications.h"
#include "dpx_tc.h"
#include "dpx_osc.h"
#include "dpx_api.h"

class DpxMatrix : public Usermod {

private:
    bool _initDone = false;

    // Render the 3 indicator pixels (corner dots).
    // Indicator 1 = top-left (pixel 0)
    // Indicator 2 = top-right (pixel 31)
    // Indicator 3 = bottom-left (pixel 224)
    void renderIndicators() {
        if (dpxIndicator[0]) strip.setPixelColor(0,   dpxIndicator[0]);
        if (dpxIndicator[1]) strip.setPixelColor(31,  dpxIndicator[1]);
        if (dpxIndicator[2]) strip.setPixelColor(224, dpxIndicator[2]);
    }

public:
    static const char _name[];

    void setup() override {
        // Load persistent settings
        dpxLoadDev();
        dpxLoadOscListeners();

        // Build the initial app loop (Time, Date only to start)
        dpxRebuildLoop();
        dpxAppStartMs = millis();

        // Start UDP for OSC
        dpxOscBegin();

        // Register HTTP routes
        dpxRegisterRoutes();

        _initDone = true;
        DEBUG_PRINTLN(F("DpxMatrix: setup complete"));
    }

    void loop() override {
        if (!_initDone) return;

        // Advance app pointer when duration expires
        dpxAppLoopTick();

        // TC dwell timeout — restore auto-transition after TC signal stops
        dpxTcDwellTick();

        // Receive OSC packets
        dpxOscTick();
    }

    void handleOverlayDraw() override {
        if (!_initDone) return;

        // Notifications take priority over the app loop
        if (dpxNotifTick()) {
            dpxRenderNotification();
        } else {
            dpxRenderCurrentApp();
        }

        // Overlay indicator dots on top of everything
        renderIndicators();
    }

    uint16_t getId() override { return USERMOD_ID_DPX_MATRIX; }

    void addToJsonInfo(JsonObject& root) override {
        JsonObject user = root["u"].isNull() ? root.createNestedObject("u") : root["u"];
        user[FPSTR(_name)] = F("dpx_tc002 matrix active");
    }

    // ── Usermod settings persist (WLED /cfg.json) ─────────────────────────
    void addToConfig(JsonObject& root) override {
        JsonObject top = root.createNestedObject(FPSTR(_name));
        top["atime"]  = DPX_ATIME;
        top["atrans"] = DPX_ATRANS;
        top["sspeed"] = DPX_SSPEED;
    }

    bool readFromConfig(JsonObject& root) override {
        JsonObject top = root[FPSTR(_name)];
        if (top.isNull()) return false;
        if (top.containsKey("atime"))  DPX_ATIME  = top["atime"];
        if (top.containsKey("atrans")) DPX_ATRANS = top["atrans"];
        if (top.containsKey("sspeed")) DPX_SSPEED = top["sspeed"];
        return true;
    }
};

const char DpxMatrix::_name[] PROGMEM = "DpxMatrix";
