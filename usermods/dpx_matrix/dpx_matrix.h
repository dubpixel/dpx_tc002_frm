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
#include "dpx_font.h"
#include "dpx_text.h"
#include "dpx_persist.h"
#include "dpx_buzzer.h"    // after dpx_persist.h so DPX_SOUND_ENABLED is in scope
#include "dpx_apps.h"
#include "dpx_notifications.h"
#include "dpx_tc.h"
#include "dpx_osc.h"
#include "dpx_overlay.h"
#include "dpx_mqtt.h"
#include "dpx_api.h"

// ── dpx Matrix WLED effect ────────────────────────────────────────────────────
// Registered as a proper WLED effect so brightness, transitions, and power-fade
// all work natively. Replaces the old handleOverlayDraw() hack.
// _dpxEffectId is declared in dpx_apps.h and assigned in DpxMatrix::setup().

static void mode_dpx_matrix() {
    // Notifications take priority
    if (dpxNotifTick()) {
        dpxRenderNotification();
    } else {
        dpxRenderCurrentApp();
    }
    // Text overlay + pixel effects on top
    dpxRenderOverlays();
    // Corner indicator pixels (topmost layer) — L-shape at each corner, AWTRIX3 style
    // ind[0]=top-left (0,0)(1,0)(0,1)  ind[1]=top-right (31,0)(30,0)(31,1)  ind[2]=bottom-left (0,7)(1,7)(0,6)
    {
        static const int8_t IND_PX[3][3] = {{0,1,0}, {31,30,31}, {0,1,0}};
        static const int8_t IND_PY[3][3] = {{0,0,1}, { 0, 0, 1}, {7,7,6}};
        extern uint32_t dpxIndicatorBlink[3];
        extern uint32_t dpxIndicatorFade[3];
        unsigned long now = millis();
        for (int i = 0; i < 3; i++) {
            if (!dpxIndicator[i]) continue;
            if (dpxIndicatorBlink[i] > 0 && (now / dpxIndicatorBlink[i]) % 2) continue;
            uint32_t col = dpxIndicator[i];
            if (dpxIndicatorFade[i] > 0) {
                // Triangle wave: ramp up 0→1 then down 1→0 over fade period
                float t = (float)(now % dpxIndicatorFade[i]) / (float)dpxIndicatorFade[i];
                float bright = (t < 0.5f) ? (2.0f * t) : (2.0f - 2.0f * t);
                uint8_t r = ((col >> 16) & 0xFF) * bright;
                uint8_t g = ((col >>  8) & 0xFF) * bright;
                uint8_t b = ( col        & 0xFF) * bright;
                col = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            }
            for (int j = 0; j < 3; j++)
                SEGMENT.setPixelColorXY(IND_PX[i][j], IND_PY[i][j], col);
        }
    }
}

class DpxMatrix : public Usermod {

private:
    bool _initDone  = false;
    bool dpxEnabled = true;

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

        // Initialise TC001 buzzer pin; drives LOW to prevent float noise.
        dpxBuzzerInit();

        // NOTE: dpxOscBegin() is called in connected() once WiFi is up.
        // Opening a UDP socket before lwip is ready crashes the device.

        // Register dpx Matrix as a WLED effect — this replaces handleOverlayDraw.
        // Using SEGMENT APIs means brightness/transitions/power-fade work natively.
        _dpxEffectId = strip.addEffect(255, &mode_dpx_matrix,
            "dpx Matrix;!;2");  // 255 = auto-assign next available ID
        // Switch the main segment to our effect on startup
        strip.getMainSegment().setMode(_dpxEffectId);
        stateUpdated(CALL_MODE_INIT);

        // Register HTTP routes
        dpxRegisterRoutes();

        _initDone = true;
        DEBUG_PRINTF("DpxMatrix: setup complete, effect id=%d\n", _dpxEffectId);
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

        // ── Serial debug command handler ────────────────────────────────
        // Send a character to get status info. Commands:
        //   ? / h  — help
        //   s      — status (IP, app, time, RSSI, heap)
        //   r      — reboot
        if (Serial.available()) {
            char cmd = Serial.read();
            while (Serial.available()) Serial.read();  // flush
            switch (cmd) {
                case '?': case 'h':
                    Serial.println(F("[dpx] commands: s=status  r=reboot  h=help"));
                    break;
                case 's': {
                    Serial.printf("[dpx] IP      : %s\n", WiFi.localIP().toString().c_str());
                    Serial.printf("[dpx] AP SSID : %s (%s) clients=%d\n",
                        apSSID,
                        WiFi.softAPIP().toString().c_str(),
                        WiFi.softAPgetStationNum());
                    Serial.printf("[dpx] WiFi    : %s (RSSI %d dBm)\n",
                        WiFi.SSID().c_str(), WiFi.RSSI());
                    Serial.printf("[dpx] Heap    : %u free / %u total\n",
                        ESP.getFreeHeap(), ESP.getHeapSize());
                    Serial.printf("[dpx] App     : %s (#%u of %u)\n",
                        dpxCurrentApp < dpxApps.size() ? dpxApps[dpxCurrentApp].name.c_str() : "?",
                        (unsigned)dpxCurrentApp, (unsigned)dpxApps.size());
                    Serial.printf("[dpx] Notifs  : %u queued\n", (unsigned)dpxNotifQueue.size());
                    Serial.printf("[dpx] Time    : %02d:%02d:%02d (localTime=%lu)\n",
                        hour(localTime), minute(localTime), second(localTime), (unsigned long)localTime);
                    Serial.printf("[dpx] Uptime  : %lus\n", millis() / 1000);
                    Serial.printf("[dpx] MQTT    : %s\n", WLED_MQTT_CONNECTED ? "connected" : "disconnected");
                    Serial.printf("[dpx] OSC UDP : %s port %d\n", dpxUdpStarted ? "started" : "stopped", DPX_OSC_PORT);
                    break;
                }
                case 'r':
                    Serial.println(F("[dpx] rebooting..."));
                    delay(100); ESP.restart();
                    break;
                default:
                    Serial.printf("[dpx] unknown command '%c' — send h for help\n", cmd);
            }
        }
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
    }

    // ── Button handling ────────────────────────────────────────────────────
    // Called by WLED's button loop on every tick for each configured button.
    // Returning true consumes the event — WLED will not act on it.
    // We use WLED's APIs (toggleOnOff, stateUpdated) for WLED-level actions.
    //
    // Button layout (TC001 front, left→right):
    //   b=0  GPIO DPX_BTN_LEFT   short=prev app    long=dismiss notification
    //   b=1  GPIO DPX_BTN_MID    short=next app    long=cycle WLED effect
    //   b=2  GPIO DPX_BTN_RIGHT  short=power tog   long=show IP
    bool handleButton(uint8_t b) override {
        if (!_initDone || b > 2) return false;

        static const uint8_t PINS[3] = {DPX_BTN_LEFT, DPX_BTN_MID, DPX_BTN_RIGHT};
        static unsigned long pressStart[3] = {0, 0, 0};
        static bool          wasPressed[3] = {false, false, false};

        bool pressed = (digitalRead(PINS[b]) == LOW);  // active-low
        unsigned long now = millis();

        if (pressed && !wasPressed[b]) {
            pressStart[b] = now;  // rising edge
        } else if (!pressed && wasPressed[b]) {
            // falling edge — determine action
            unsigned long dur = now - pressStart[b];
            if (dur >= 30) {  // debounce threshold
                bool lng = (dur > 800);
                switch (b) {
                    case 0:  // LEFT
                        lng ? dpxDismissNotification() : dpxPrevApp();
                        break;
                    case 1:  // MIDDLE — next app / long=cycle effect
                        if (lng) {
                            effectCurrent = (effectCurrent + 1) % strip.getModeCount();
                            stateChanged = true;
                            colorUpdated(CALL_MODE_BUTTON);
                        } else {
                            dpxNextApp();
                        }
                        break;
                    case 2:  // RIGHT — power toggle / long=show IP
                        if (lng) {
                            String ip = WiFi.localIP().toString();
                            StaticJsonDocument<64> doc;
                            doc["text"] = ip; doc["color"] = "#00FF88";
                            String s; serializeJson(doc, s);
                            dpxPushNotification(s.c_str());
                        } else {
                            toggleOnOff();
                            stateUpdated(CALL_MODE_BUTTON);
                        }
                        break;
                }
            }
        }
        wasPressed[b] = pressed;
        return true;  // always consume — prevent WLED double-acting on these pins
    }

    // handleOverlayDraw() removed — rendering now happens in mode_dpx_matrix()
    // which is a proper WLED effect registered in setup(). This gives correct
    // brightness scaling, transitions, and power-fade for free.

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

        // Mute/unmute channels: {"mute": {"Time": true, "WLED": false}}
        if (dpx.containsKey(F("mute"))) {
            JsonObject mutes = dpx[F("mute")].as<JsonObject>();
            for (JsonPair kv : mutes)
                dpxMuteApp(String(kv.key().c_str()), kv.value().as<bool>());
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
