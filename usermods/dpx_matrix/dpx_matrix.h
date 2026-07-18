// ================================================================================
// dpx_matrix.h — Main Usermod Class
// ================================================================================
// Original work — dubpixel / dpx_tc002 (EUPL v1.2)
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
#include "dpx_firstboot.h"
#include "dpx_buzzer.h"
#include "dpx_font.h"
#include "dpx_text.h"
#include "dpx_persist.h"
#include "dpx_apps.h"
#include "dpx_notifications.h"
#include "dpx_tc.h"
#include "dpx_osc.h"
#include "dpx_overlay.h"
#include "dpx_mqtt.h"
#include "dpx_api.h"

class DpxMatrix : public Usermod {

private:
    bool _initDone  = false;

    // ── Button debounce state ───────────────────────────────────────────────
    // Buttons are active-LOW (internal pull-up). We fire on falling edge.
    struct BtnState {
        uint8_t  pin;
        bool     lastRaw   = true;    // last raw read (true = not pressed)
        bool     debounced = true;    // debounced state
        uint32_t lastMs    = 0;
    };
    BtnState _btn[3]; // index 0=left, 1=mid, 2=right

    // Poll one button; returns true on falling-edge (press event).
    bool _pollBtn(BtnState& b) {
        if (b.pin == 255) return false;
        bool raw = digitalRead(b.pin);
        uint32_t now = millis();
        if (raw != b.lastRaw) b.lastMs = now;   // reset timer on any change
        b.lastRaw = raw;
        if ((now - b.lastMs) >= 50) {            // 50 ms debounce
            bool prev = b.debounced;
            b.debounced = raw;
            if (prev && !raw) return true;        // falling edge = press
        }
        return false;
    }

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
        // Write default cfg.json on first boot if LittleFS has none.
        // Must run before dpxLoadDev() so WLED picks it up on next reboot.
        dpxFirstBoot();

        // Load persistent settings
        dpxLoadDev();
        dpxLoadOscListeners();

        // Build the initial app loop (Time, Date only to start)
        dpxRebuildLoop();
        dpxAppStartMs = millis();

        // Initialise button pins (active-low, internal pull-up)
        const uint8_t btnPins[3] = { DPX_BTN_LEFT, DPX_BTN_MID, DPX_BTN_RIGHT };
        for (int i = 0; i < 3; i++) {
            _btn[i].pin       = btnPins[i];
            _btn[i].lastRaw   = true;
            _btn[i].debounced = true;
            _btn[i].lastMs    = 0;
            if (_btn[i].pin != 255) pinMode(_btn[i].pin, INPUT_PULLUP);
        }

        // Initialise TC001 buzzer pin; drives LOW to prevent float noise.
        dpxBuzzerInit();

        // NOTE: dpxOscBegin() is called in connected() once WiFi is up.
        // Opening a UDP socket before lwip is ready crashes the device.

        // Register HTTP routes
        dpxRegisterRoutes();

        _initDone = true;
        DEBUG_PRINTLN(F("DpxMatrix: setup complete"));
    }

    void connected() override {
        // Safe to open UDP socket now that WiFi/lwip is ready
        dpxOscBegin();

        // Re-apply POSIX timezone AFTER WLED's configTime() call (which runs on
        // WiFi connect and overwrites TZ with a dumb UTC-offset string).
        // configTzTime sets TZ env + kicks off an NTP sync in one shot.
        if (DPX_TIMEZONE.length()) {
            configTzTime(DPX_TIMEZONE.c_str(), ntpServerName);
            DEBUG_PRINTF("DpxMatrix: TZ applied: %s\n", DPX_TIMEZONE.c_str());
        }

        DEBUG_PRINTLN(F("DpxMatrix: WiFi connected, OSC UDP started"));

        // Print IP prominently — visible any time serial monitor is open
        String ip = WiFi.localIP().toString();
        Serial.println();
        Serial.println(F("┌─────────────────────────────┐"));
        Serial.print  (F("│  dpx_tc002  IP: "));
        Serial.print  (ip);
        Serial.println(F("  │"));
        Serial.println(F("└─────────────────────────────┘"));
        Serial.println();
        Serial.printf("[dpx] IP: %s\n", ip.c_str());
        StaticJsonDocument<128> doc;
        doc["text"]   = ip;
        doc["color"]  = "#00FF88";
        doc["repeat"] = 1;
        String s; serializeJson(doc, s);
        dpxPushNotification(s.c_str());
    }

    void loop() override {
        if (!_initDone) return;

        // Rate-limit to ~50Hz max. Constant UDP/timer polling on every Arduino
        // loop tick (which runs at 300-1000Hz) hammers the lwip socket layer
        // and slows down HTTP/WebSocket responses.
        static unsigned long _lastLoopMs = 0;
        unsigned long now = millis();
        if (now - _lastLoopMs < 20) return;
        _lastLoopMs = now;

        // Reprint IP whenever serial is freshly connected
        static bool _serialWasConnected = false;
        bool serialNow = (bool)Serial;
        if (serialNow && !_serialWasConnected && WiFi.localIP()[0] != 0) {
            String ip = WiFi.localIP().toString();
            Serial.println();
            Serial.println(F("┌─────────────────────────────┐"));
            Serial.print  (F("│  dpx_tc002  IP: "));
            Serial.print  (ip);
            Serial.println(F("  │"));
            Serial.println(F("└─────────────────────────────┘"));
            Serial.println();
        }
        _serialWasConnected = serialNow;

        // Advance app pointer when duration expires
        dpxAppLoopTick();

        // TC dwell timeout — restore auto-transition after TC signal stops
        dpxTcDwellTick();

        // Receive OSC packets
        dpxOscTick();

        // Advance RTTTL note sequencer
        dpxBuzzerTick();

        // Physical buttons
        // LEFT → previous app, RIGHT → next app, MIDDLE → toggle dpxEnabled
        if (_pollBtn(_btn[0])) dpxPrevApp();
        if (_pollBtn(_btn[2])) dpxNextApp();
        if (_pollBtn(_btn[1])) {
            dpxEnabled = !dpxEnabled;
            DEBUG_PRINTF("DpxMatrix: button toggle enabled=%d\n", (int)dpxEnabled);
        }
    }

    void handleOverlayDraw() override {
        if (!_initDone || !dpxEnabled) return;

        // Notifications take priority over the app loop
        if (dpxNotifTick()) {
            dpxRenderNotification();
        } else {
            dpxRenderCurrentApp();
        }

        // Pixel effects + text overlay on top of app content
        dpxRenderOverlays();

        // Indicator corner dots (topmost layer)
        renderIndicators();
    }

    // ── MQTT integration ──────────────────────────────────────────────────
    void onMqttConnect(bool /*sessionPresent*/) override {
        dpxMqttConnect();
    }

    bool onMqttMessage(char* topic, char* payload) override {
        // dpx_mqtt.h handles dpx/* and {deviceTopic}/dpx/* topics
        if (dpxMqttMessage(topic, payload)) return true;
        return false;
    }

    // ── JSON state integration (POST /json {"dpx":{...}}) ─────────────────
    void readFromJsonState(JsonObject& obj) override {
        JsonObject dpx = obj[F("dpx")];
        if (dpx.isNull()) return;

        // Notifications
        if (dpx.containsKey(F("notify"))) {
            String s; serializeJson(dpx[F("notify")], s);
            dpxPushNotification(s.c_str());
        }
        if (dpx.containsKey(F("notify_dismiss"))) dpxDismissNotification();

        // Timecode
        if (dpx.containsKey(F("tc"))) {
            String tc = dpx[F("tc")].as<String>(); tc.trim();
            if (tc.length() >= 8) dpxPushTC(tc);
        }

        // Enable/disable matrix overlay
        if (dpx.containsKey(F("enabled"))) dpxEnabled = dpx[F("enabled")].as<bool>();

        // App loop control
        if (dpx.containsKey(F("nextapp")))     dpxNextApp();
        if (dpx.containsKey(F("previousapp"))) dpxPrevApp();
        if (dpx.containsKey(F("switch"))) {
            String s; serializeJson(dpx[F("switch")], s);
            dpxSwitchToApp(s.c_str());
        }

        // Custom app upsert: {"app":{"name":{...}}}
        if (dpx.containsKey(F("app"))) {
            JsonObject apps = dpx[F("app")].as<JsonObject>();
            for (JsonPair kv : apps) {
                String v; serializeJson(kv.value(), v);
                dpxSetCustomApp(String(kv.key().c_str()), v.c_str());
            }
        }

        // Text overlay
        if (dpx.containsKey(F("overlay"))) {
            String s; serializeJson(dpx[F("overlay")], s);
            dpxSetOverlay(s.c_str());
        }

        // Pixel effect
        if (dpx.containsKey(F("effect"))) {
            String s; serializeJson(dpx[F("effect")], s);
            dpxSetPixelEffect(s.c_str());
        }

        // Indicators
        for (int i = 1; i <= 3; i++) {
            String key = String(F("indicator")) + i;
            if (dpx.containsKey(key)) {
                JsonArray a = dpx[key].as<JsonArray>();
                if (a.size() >= 3)
                    dpxIndicator[i-1] = ((uint32_t)(uint8_t)a[0] << 16)
                                      | ((uint32_t)(uint8_t)a[1] <<  8)
                                      |  (uint32_t)(uint8_t)a[2];
                else
                    dpxIndicator[i-1] = 0;
            }
        }
    }

    void addToJsonState(JsonObject& obj) override {
        JsonObject dpx = obj.createNestedObject(F("dpx"));
        dpx[F("enabled")]   = dpxEnabled;
        dpx[F("app")]       = (dpxCurrentApp < dpxApps.size())
                              ? dpxApps[dpxCurrentApp].name : String();
        dpx[F("notif")]     = (int)dpxNotifQueue.size();
        dpx[F("autoTrans")] = dpxAutoTrans;
        dpx[F("overlay")]   = dpxTextOverlay.active ? dpxTextOverlay.text : String();
        dpx[F("effect")]    = dpxPixelEffect.active  ? dpxPixelEffect.name  : String();
    }

    uint16_t getId() override { return USERMOD_ID_DPX_MATRIX; }

    void addToJsonInfo(JsonObject& root) override {
        JsonObject user = root["u"].isNull() ? root.createNestedObject("u") : root["u"];
        user[FPSTR(_name)] = F("dpx_tc002 matrix active");
    }

    // ── Usermod settings persist (WLED /cfg.json) ─────────────────────────
    void addToConfig(JsonObject& root) override {
        JsonObject top = root.createNestedObject(FPSTR(_name));
        top["enabled"] = dpxEnabled;
        top["atime"]   = DPX_ATIME;
        top["atrans"]  = DPX_ATRANS;
        top["sspeed"]  = DPX_SSPEED;
    }

    bool readFromConfig(JsonObject& root) override {
        JsonObject top = root[FPSTR(_name)];
        if (top.isNull()) return false;
        if (top.containsKey("enabled")) dpxEnabled = top["enabled"].as<bool>();
        if (top.containsKey("atime"))   DPX_ATIME  = top["atime"];
        if (top.containsKey("atrans"))  DPX_ATRANS = top["atrans"];
        if (top.containsKey("sspeed"))  DPX_SSPEED = top["sspeed"];
        return true;
    }
};

const char DpxMatrix::_name[] PROGMEM = "DpxMatrix";
