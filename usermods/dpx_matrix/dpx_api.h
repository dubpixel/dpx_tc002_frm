// ================================================================================
// dpx_api.h — HTTP Routes: pages + REST API
// ================================================================================
// Original work — dubpixel / dpx_tc002 (EUPL v1.2)
// ================================================================================
// PROJECT: dpx_tc002_frm
// ================================================================================
//
// File: dpx_api.h
// Purpose: Register all HTTP routes for the dpx_matrix usermod.
//
// Pages:
//   GET /ctrl      → Control panel (dpx_html.h → ctrl_html)
//   GET /browse    → Icon browser (dpx_html.h → browse_html)
//   GET /api-ref   → API reference (dpx_html.h → apiref_html)
//   GET /screen    → Live matrix view (dpx_html.h → screenfull_html)
//
// API:
//   GET/POST /api/stats            device stats
//   GET      /api/apps             app list
//   GET      /api/loop             app loop map
//   POST     /api/notify           push notification
//   POST     /api/notify/dismiss   dismiss held notification
//   GET/POST /api/custom           get or push custom app (?name=)
//   POST     /api/switch           switch to named app
//   POST     /api/nextapp          advance loop
//   POST     /api/previousapp      go back in loop
//   POST     /api/power            {"power":true/false}
//   POST     /api/indicator1|2|3   {"color":[r,g,b],"blink":ms}
//   GET/POST /api/time             get/set device time
//   POST     /api/syncntp          re-trigger NTP sync
//   GET/POST /api/settings         DPX + WLED settings
//   GET/POST /api/dev              raw dev.json
//   GET/POST/DELETE /api/osc/listeners   OSC listener registry
//   POST     /api/moodlight        enable WLED effects (disable matrix overlay)
//   POST     /api/rtttl            play RTTTL melody on TC001 piezo buzzer (GPIO 15)
//   POST     /api/sound            stub
//   POST     /api/rename           LittleFS rename
//   GET      /api/effects          WLED effect names
//   GET      /api/transitions      empty list (not implemented)
//   GET/POST /api/reboot           restart device
//   GET      /dpx                  legacy status (compat)
//   GET      /dpx/screen           legacy pixel dump (compat)
//
// ================================================================================

#pragma once
#include "dpx_html.h"

// dpxIndicator is declared extern in dpx_osc.h; defined in dpx_matrix.cpp.
extern uint32_t dpxIndicator[3];
extern uint32_t dpxIndicatorBlink[3];
extern uint32_t dpxIndicatorFade[3];

// ── Helpers ───────────────────────────────────────────────────────────────────

// Get plain-text POST body from AsyncWebServerRequest.
static inline String dpxBody(AsyncWebServerRequest* req) {
    // req->arg("plain") is the ESPAsyncWebServer convention for raw POST body
    // when Content-Type is not multipart/form-data (e.g. application/json).
    // Using arg() instead of getParam("plain", true) matches how WLED itself
    // reads POST bodies in wled/ui.cpp and avoids param-type mismatch.
    return req->arg("plain");
}

// 256-pixel screen dump as JSON array.
static String dpxScreenJson() {
    DynamicJsonDocument doc(4096);
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < 256; i++)
        arr.add((uint32_t)(strip.getPixelColor(i) & 0x00FFFFFF));
    String s; serializeJson(doc, s); return s;
}

// Device stats JSON (shared by /dpx and /api/stats).
static String dpxStatsJson() {
    DynamicJsonDocument doc(512);
    doc[F("version")]   = F("dpx_tc002");
    doc[F("build")]     = F(__DATE__ " " __TIME__);  // unique per compile
#ifndef DPX_BUILD_ID
#define DPX_BUILD_ID "dev"
#endif
    doc[F("build_id")]  = F(DPX_BUILD_ID);  // injected by pio-scripts/write_build_ts.py
    doc[F("uptime")]    = millis() / 1000;
    doc[F("ram")]       = ESP.getFreeHeap();
    doc[F("ip")]        = WiFi.localIP().toString();
    doc[F("rssi")]      = WiFi.RSSI();
    doc[F("hostname")]  = WiFi.getHostname();
    doc[F("app")]       = (dpxCurrentApp < (int)dpxApps.size())
                          ? dpxApps[dpxCurrentApp].name : String();
    doc[F("notif")]     = (int)dpxNotifQueue.size();
    doc[F("autoTrans")] = dpxAutoTrans;
    doc[F("enabled")]   = dpxEnabled;
    String s; serializeJson(doc, s); return s;
}

// Settings JSON: DPX runtime + WLED globals.
// GET /api/settings returns this; POST /api/settings applies supported keys.
static String dpxSettingsJson() {
    DynamicJsonDocument doc(768);
    doc[F("BRI")]        = (int)bri;
    doc[F("ATIME")]      = DPX_ATIME;
    doc[F("ATRANS")]     = DPX_ATRANS;
    doc[F("SSPEED")]     = DPX_SSPEED;
    doc[F("UPPERCASE")]  = DPX_UPPERCASE;
    doc[F("Timezone")]   = DPX_TIMEZONE;
    doc[F("MQTT_PREFIX")] = String(mqttDeviceTopic);
    doc[F("SOUND")]      = DPX_SOUND_ENABLED;
    // VOL omitted — passive piezo has no volume control
    doc[F("TIM")]        = DPX_SHOW_TIME;
    doc[F("DAT")]        = DPX_SHOW_DATE;
    doc[F("TC_MUTE")]    = DPX_TC_MUTE;
    String s; serializeJson(doc, s); return s;
}

// Apply settings from JSON body to DPX runtime + WLED globals.
static void dpxApplySettings(const String& body) {
    DynamicJsonDocument doc(512);
    if (deserializeJson(doc, body)) return;

    if (doc.containsKey("BRI")) {
        bri = (uint8_t)constrain(doc["BRI"].as<int>(), 0, 255);
        stateUpdated(CALL_MODE_DIRECT_CHANGE);
    }
    if (doc.containsKey("ATIME"))    { DPX_ATIME    = doc["ATIME"].as<int>(); }
    if (doc.containsKey("ATRANS"))   { DPX_ATRANS   = doc["ATRANS"].as<bool>(); dpxAutoTrans = DPX_ATRANS; }
    if (doc.containsKey("SSPEED"))   { DPX_SSPEED   = doc["SSPEED"].as<int>(); }
    if (doc.containsKey("UPPERCASE")){ DPX_UPPERCASE = doc["UPPERCASE"].as<bool>(); }
    // Timezone is handled via /api/syncntp for full resync
    if (doc.containsKey("Timezone")) {
        DPX_TIMEZONE = doc["Timezone"].as<String>();
        setenv("TZ", DPX_TIMEZONE.c_str(), 1);
        tzset();
        dpxMergeDev(("{\"timezone_posix\":\"" + DPX_TIMEZONE + "\"}").c_str());
    }
    if (doc.containsKey("TIM")) {
        DPX_SHOW_TIME = doc["TIM"].as<bool>();
        if (DPX_SHOW_TIME) dpxHiddenApps.erase(String(F("Time")));
        else               dpxHiddenApps.insert(String(F("Time")));
        dpxRebuildLoop();
        dpxMergeDev(((String)F("{\"show_time\":") + (DPX_SHOW_TIME ? F("true}") : F("false}"))).c_str());
    }
    if (doc.containsKey("DAT")) {
        DPX_SHOW_DATE = doc["DAT"].as<bool>();
        if (DPX_SHOW_DATE) dpxHiddenApps.erase(String(F("Date")));
        else               dpxHiddenApps.insert(String(F("Date")));
        dpxRebuildLoop();
        dpxMergeDev(((String)F("{\"show_date\":") + (DPX_SHOW_DATE ? F("true}") : F("false}"))).c_str());
    }
    if (doc.containsKey("SOUND")) {
        DPX_SOUND_ENABLED = doc["SOUND"].as<bool>();
        dpxMergeDev(((String)F("{\"sound_enabled\":") + (DPX_SOUND_ENABLED ? F("true}") : F("false}"))).c_str());
    }
    if (doc.containsKey("TC_MUTE")) {
        DPX_TC_MUTE = doc["TC_MUTE"].as<bool>();
        dpxMergeDev(((String)F("{\"tc_mute\":") + (DPX_TC_MUTE ? F("true}") : F("false}"))).c_str());
    }
}

// ── Route registration ────────────────────────────────────────────────────────

static void dpxRegisterRoutes() {

    // ── Pages ─────────────────────────────────────────────────────────────────
    server.on("/ctrl",    HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send_P(200, PSTR("text/html"), ctrl_html);
    });
    server.on("/browse",  HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send_P(200, PSTR("text/html"), browse_html);
    });
    server.on("/api-ref", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send_P(200, PSTR("text/html"), apiref_html);
    });
    server.on("/screen",  HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send_P(200, PSTR("text/html"), screenfull_html);
    });

    // ── Stats ─────────────────────────────────────────────────────────────────
    // Legacy endpoint kept for back-compat
    server.on("/dpx", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, F("application/json"), dpxStatsJson());
    });
    server.on("/api/stats", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, F("application/json"), dpxStatsJson());
    });

    // ── Screen dump ───────────────────────────────────────────────────────────
    server.on("/dpx/screen", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, F("application/json"), dpxScreenJson());
    });
    server.on("/api/screen", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, F("application/json"), dpxScreenJson());
    });

    // ── App loop ──────────────────────────────────────────────────────────────
    server.on("/api/apps", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, F("application/json"), dpxGetAppsJson());
    });
    server.on("/api/loop", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, F("application/json"), dpxGetLoopJson());
    });

    // ── Notifications ─────────────────────────────────────────────────────────
    server.on("/api/notify", HTTP_POST, [](AsyncWebServerRequest* r) {
        String body = dpxBody(r);
        if (body.length()) dpxPushNotification(body.c_str());
        r->send(200, F("application/json"), F("{\"ok\":true}"));
    });
    server.on("/api/notify/dismiss", HTTP_POST, [](AsyncWebServerRequest* r) {
        dpxDismissNotification();
        r->send(200, F("application/json"), F("{\"ok\":true}"));
    });
    server.on("/api/notify/clear", HTTP_POST, [](AsyncWebServerRequest* r) {
        // Clear ALL queued notifications at once
        dpxNotifQueue.clear();
        r->send(200, F("application/json"), F("{\"ok\":true}"));
    });

    // ── Custom apps (GET=load, POST=push, name in query) ──────────────────────
    server.on("/api/custom", HTTP_ANY, [](AsyncWebServerRequest* r) {
        String name = r->hasParam("name") ? r->getParam("name")->value() : String();
        if (name.isEmpty()) { r->send(400, F("text/plain"), F("name required")); return; }

        if (r->method() == HTTP_GET) {
            r->send(200, F("application/json"), dpxGetCustomAppJson(name));
            return;
        }
        // POST: push or delete (empty body = delete)
        String body = dpxBody(r);
        bool isDelete = (body.length() <= 2);
        dpxSetCustomApp(name, body.c_str());
        if (isDelete) {
            // Remove any OSC listeners for this channel so D3 doesn't recreate it
            dpxOscListeners.erase(
                std::remove_if(dpxOscListeners.begin(), dpxOscListeners.end(),
                    [&name](const DpxOscListener& l){ return l.channel == name; }),
                dpxOscListeners.end());
            dpxSaveOscListeners();
        }
        r->send(200, F("application/json"), F("{\"ok\":true}"));
    });

    // ── App navigation ────────────────────────────────────────────────────────
    server.on("/api/switch", HTTP_POST, [](AsyncWebServerRequest* r) {
        String body = dpxBody(r);
        bool ok = body.length() ? dpxSwitchToApp(body.c_str()) : false;
        r->send(ok ? 200 : 400, F("application/json"), ok ? F("{\"ok\":true}") : F("{\"error\":\"not found\"}"));
    });
    server.on("/api/nextapp", HTTP_POST, [](AsyncWebServerRequest* r) {
        dpxNextApp();
        r->send(200, F("application/json"), F("{\"ok\":true}"));
    });
    server.on("/api/previousapp", HTTP_POST, [](AsyncWebServerRequest* r) {
        dpxPrevApp();
        r->send(200, F("application/json"), F("{\"ok\":true}"));
    });

    // ── Power on/off ──────────────────────────────────────────────────────────
    // {"power": true/false}  — maps to WLED brightness
    server.on("/api/power", HTTP_POST, [](AsyncWebServerRequest* r) {
        String body = dpxBody(r);
        DynamicJsonDocument doc(64);
        if (!deserializeJson(doc, body) && doc.containsKey("power")) {
            bool on = doc["power"].as<bool>();
            if (on) {
                bri = briLast ? briLast : 128;
            } else {
                briLast = bri;
                bri = 0;
            }
            stateUpdated(CALL_MODE_DIRECT_CHANGE);
        }
        r->send(200, F("application/json"), F("{\"ok\":true}"));
    });

    // ── Indicators ────────────────────────────────────────────────────────────
    // {"color":[r,g,b],"blink":ms}  blink=0 → solid, blink>0 → on/off interval ms
    auto handleIndicator = [](AsyncWebServerRequest* r, int idx) {
        String body = dpxBody(r);
        DynamicJsonDocument doc(128);
        if (!deserializeJson(doc, body)) {
            if (doc.containsKey("color")) {
                JsonVariant cv = doc["color"];
                if (cv.is<JsonArray>()) {
                    JsonArray a = cv.as<JsonArray>();
                    if (a.size() >= 3)
                        dpxIndicator[idx] = ((uint32_t)(uint8_t)a[0].as<int>() << 16)
                                          | ((uint32_t)(uint8_t)a[1].as<int>() <<  8)
                                          |  (uint32_t)(uint8_t)a[2].as<int>();
                    else
                        dpxIndicator[idx] = 0;
                } else if (cv.is<const char*>()) {
                    // Accept "#RRGGBB" hex string
                    const char* s = cv.as<const char*>();
                    dpxIndicator[idx] = (s && s[0]=='#') ? (uint32_t)strtol(s+1, nullptr, 16) : 0;
                } else {
                    dpxIndicator[idx] = 0;
                }
            }
            if (doc.containsKey("blink"))
                dpxIndicatorBlink[idx] = (uint32_t)max(0, doc["blink"].as<int>());
            if (doc.containsKey("fade"))
                dpxIndicatorFade[idx] = (uint32_t)max(0, doc["fade"].as<int>());
        }
        r->send(200, F("application/json"), F("{\"ok\":true}"));
    };
    server.on("/api/indicator1", HTTP_POST, [handleIndicator](AsyncWebServerRequest* r){ handleIndicator(r, 0); });
    server.on("/api/indicator2", HTTP_POST, [handleIndicator](AsyncWebServerRequest* r){ handleIndicator(r, 1); });
    server.on("/api/indicator3", HTTP_POST, [handleIndicator](AsyncWebServerRequest* r){ handleIndicator(r, 2); });

    // ── Time ──────────────────────────────────────────────────────────────────
    server.on("/api/time", HTTP_ANY, [](AsyncWebServerRequest* r) {
        if (r->method() == HTTP_POST) {
            String body = dpxBody(r);
            DynamicJsonDocument doc(64);
            if (!deserializeJson(doc, body) && doc.containsKey("utc")) {
                time_t t = (time_t)doc["utc"].as<uint32_t>();
                struct timeval tv = { t, 0 };
                settimeofday(&tv, nullptr);
            }
            r->send(200, F("application/json"), F("{\"ok\":true}"));
            return;
        }
        // GET: use WLED's localTime (has WLED timezone offset applied) for display.
        // localtime_r uses POSIX TZ env which may not be set, causing UTC display.
        time_t now;
        time(&now);
        DynamicJsonDocument doc(128);
        char buf[24];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
            year(localTime), month(localTime), day(localTime),
            hour(localTime), minute(localTime), second(localTime));
        doc[F("local")] = buf;
        doc[F("utc")]   = (uint32_t)now;
        String s; serializeJson(doc, s);
        r->send(200, F("application/json"), s);
    });

    // ── NTP resync + timezone ─────────────────────────────────────────────────
    // POST body (optional): {"timezone":"PST8PDT,...","server":"pool.ntp.org"}
    server.on("/api/syncntp", HTTP_POST, [](AsyncWebServerRequest* r) {
        String body = dpxBody(r);
        DynamicJsonDocument doc(256);
        if (!deserializeJson(doc, body)) {
            if (doc.containsKey("timezone")) {
                DPX_TIMEZONE = doc["timezone"].as<String>();
                dpxMergeDev(("{\"timezone_posix\":\"" + DPX_TIMEZONE + "\"}").c_str());
            }
        }
        // Apply TZ env variable
        if (DPX_TIMEZONE.length()) {
            setenv("TZ", DPX_TIMEZONE.c_str(), 1);
            tzset();
        }
        // Re-trigger ESP32 SNTP via configTzTime (sets TZ env + syncs)
        String ntpSrv = (!doc.isNull() && doc.containsKey("server"))
            ? doc["server"].as<String>() : String(ntpServerName);
        if (!ntpSrv.length()) ntpSrv = F("pool.ntp.org");

        if (DPX_TIMEZONE.length()) {
            configTzTime(DPX_TIMEZONE.c_str(), ntpSrv.c_str());
        } else {
            configTime(0, 0, ntpSrv.c_str());
        }
        r->send(200, F("application/json"), F("{\"ok\":true}"));
    });

    // ── Settings ──────────────────────────────────────────────────────────────
    server.on("/api/settings", HTTP_ANY, [](AsyncWebServerRequest* r) {
        if (r->method() == HTTP_POST) {
            dpxApplySettings(dpxBody(r));
            r->send(200, F("application/json"), F("{\"ok\":true}"));
            return;
        }
        r->send(200, F("application/json"), dpxSettingsJson());
    });

    // ── dev.json (raw device settings) ────────────────────────────────────────
    server.on("/api/dev", HTTP_ANY, [](AsyncWebServerRequest* r) {
        if (r->method() == HTTP_POST) {
            String body = dpxBody(r);
            if (body.length()) dpxMergeDev(body.c_str());
            r->send(200, F("application/json"), F("{\"ok\":true}"));
            return;
        }
        r->send(200, F("application/json"), dpxReadDevJson());
    });

    // ── OSC Listeners ─────────────────────────────────────────────────────────
    server.on("/api/osc/listeners", HTTP_ANY, [](AsyncWebServerRequest* r) {
        if (r->method() == HTTP_GET) {
            r->send(200, F("application/json"), dpxOscListenersJson());
            return;
        }
        if (r->method() == HTTP_DELETE) {
            // Path passed as ?path=<encoded> query param — DELETE body not reliably parsed
            String path = r->arg("path");
            if (path.length()) {
                dpxOscListeners.erase(
                    std::remove_if(dpxOscListeners.begin(), dpxOscListeners.end(),
                        [&path](const DpxOscListener& l){ return l.path == path; }),
                    dpxOscListeners.end());
                dpxSaveOscListeners();
            }
            r->send(200, F("application/json"), F("{\"ok\":true}"));
            return;
        }
        // POST: parse URL-encoded body — plain=<json> (ESPAsyncWebServer form field)
        String body = dpxBody(r);
        DynamicJsonDocument doc(256);
        if (deserializeJson(doc, body)) { r->send(400, F("text/plain"), F("bad JSON")); return; }
        DpxOscListener lsr;
        lsr.path    = doc["path"]    | String();
        lsr.channel = doc["channel"] | String();
        lsr.label   = doc["label"]   | lsr.channel;
        if (lsr.path.length() && lsr.channel.length()) {
            // Remove any existing entry for this path first
            dpxOscListeners.erase(
                std::remove_if(dpxOscListeners.begin(), dpxOscListeners.end(),
                    [&lsr](const DpxOscListener& l){ return l.path == lsr.path; }),
                dpxOscListeners.end());
            dpxOscListeners.push_back(lsr);
            dpxSaveOscListeners();
            // Ensure target app channel exists
            if (dpxCustom.find(lsr.channel) == dpxCustom.end()) {
                DpxCustomApp a; a.valid = true; a.text = lsr.label;
                dpxCustom[lsr.channel] = a;
                dpxRebuildLoop();
            }
        }
        r->send(200, F("application/json"), F("{\"ok\":true}"));
    });

    // ── Moodlight ─────────────────────────────────────────────────────────────
    // Non-empty body  → disable dpx overlay, WLED effects take over
    // Empty body      → re-enable dpx overlay
    server.on("/api/moodlight", HTTP_POST, [](AsyncWebServerRequest* r) {
        String body = dpxBody(r);
        body.trim();
        if (!body.length() || body == "{}") {
            dpxEnabled = true;
            r->send(200, F("application/json"), F("{\"ok\":true,\"enabled\":true}"));
            return;
        }
        dpxEnabled = false;
        // Apply brightness/color to WLED if provided
        DynamicJsonDocument doc(256);
        if (!deserializeJson(doc, body)) {
            if (doc.containsKey("brightness")) {
                bri = (uint8_t)constrain(doc["brightness"].as<int>(), 0, 255);
                stateUpdated(CALL_MODE_DIRECT_CHANGE);
            }
        }
        r->send(200, F("application/json"), F("{\"ok\":true,\"enabled\":false}"));
    });

    // ── RTTTL — 3-callback form guarantees body collection into _tempObject ───────────
    // POST body: raw RTTTL string (text/plain) or "stop" to silence.
    // Same mechanism as AsyncCallbackJsonWebHandler — works regardless of Content-Type.
    server.on("/api/rtttl", HTTP_POST,
        [](AsyncWebServerRequest* r) {
            String body = r->_tempObject ? String((char*)r->_tempObject) : String();
            body.trim();
            if (body.isEmpty()) {
                r->send(400, F("application/json"), F("{\"error\":\"empty body\"}"));
                return;
            }
            if (body.equalsIgnoreCase("stop")) {
                dpxBuzzerStop();
                r->send(200, F("application/json"), F("{\"ok\":true,\"action\":\"stop\"}"));
                return;
            }
            dpxBuzzerPlay(body.c_str());
            DynamicJsonDocument doc(128);
            doc[F("ok")]      = true;
            doc[F("sound")]   = DPX_SOUND_ENABLED;
            doc[F("notes")]   = (int)_bzCount;
            doc[F("playing")] = _bzActive;
            String s; serializeJson(doc, s);
            r->send(200, F("application/json"), s);
        },
        nullptr,
        [](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index == 0) { free(r->_tempObject); r->_tempObject = malloc(total + 1); }
            if (r->_tempObject) {
                memcpy((uint8_t*)r->_tempObject + index, data, len);
                if (index + len == total) ((uint8_t*)r->_tempObject)[total] = '\0';
            }
        }
    );

    // ── RTTTL parse debug — GET /api/buzzer/parse?q=<rtttl> ─────────────
    // Returns JSON array of {freq, ms} for each parsed note (no playback).
    server.on("/api/buzzer/parse", HTTP_GET, [](AsyncWebServerRequest* r) {
        String q = r->hasParam("q") ? r->arg("q") : "";
        _bzParse(q.c_str());
        String out = "[";
        for (uint8_t i = 0; i < _bzCount; i++) {
            if (i) out += ",";
            out += "{\"f\":" + String(_bzNotes[i].freq) + ",\"ms\":" + String(_bzNotes[i].ms) + "}";
        }
        out += "]";
        _bzCount = 0; // reset so it doesn't accidentally play
        r->send(200, F("application/json"), out);
    });

    // ── Direct beep test ─────────────────────────────────────────────────
    // Bypasses RTTTL parser and DPX_SOUND_ENABLED — hardware-only test.
    // Add ?active=1 to test active-buzzer theory: DC pulse instead of PWM.
    server.on("/api/beeptest", HTTP_POST, [](AsyncWebServerRequest* r) {
        bool testActive = r->hasParam("active") && r->arg("active") == "1";
        if (testActive) {
            // Active buzzer test: simple DC on/off, no PWM
            ledcDetachPin(DPX_BUZZER_PIN);
            pinMode(DPX_BUZZER_PIN, OUTPUT);
            digitalWrite(DPX_BUZZER_PIN, HIGH);
            delay(200); digitalWrite(DPX_BUZZER_PIN, LOW);
            delay(80);  digitalWrite(DPX_BUZZER_PIN, HIGH);
            delay(200); digitalWrite(DPX_BUZZER_PIN, LOW);
            // Re-attach LEDC after test
            ledcAttachPin(DPX_BUZZER_PIN, DPX_BUZZER_LEDC_CH);
            ledcWrite(DPX_BUZZER_LEDC_CH, 0);
        } else {
            _bzNotes[0] = {880, 200};
            _bzNotes[1] = {  0,  80};
            _bzNotes[2] = {880, 200};
            _bzNotes[3] = {  0,   1};
            _bzCount = 4; _bzIdx = 0; _bzActive = true; _bzStart = millis();
            ledcWriteTone(DPX_BUZZER_LEDC_CH, 880);
        }
        DynamicJsonDocument doc(128);
        doc[F("ok")]            = true;
        doc[F("pin")]           = (int)DPX_BUZZER_PIN;
        doc[F("ledc_ch")]       = (int)DPX_BUZZER_LEDC_CH;
        doc[F("sound_enabled")] = DPX_SOUND_ENABLED;
        doc[F("active_test")]   = testActive;
        String s; serializeJson(doc, s);
        r->send(200, F("application/json"), s);
    });
    // POST /api/sound  body: JSON {"rtttl":"..."} or {"sound":"filename"} or empty=stop
    // sound files are RTTTL text stored in /MELODIES/<name>.txt on LittleFS
    server.on("/api/sound", HTTP_POST, [](AsyncWebServerRequest* r) {
        String body = dpxBody(r);
        body.trim();
        // Empty body or {} = stop
        if (!body.length() || body == F("{}")) {
            dpxBuzzerStop();
            r->send(200, F("application/json"), F("{\"ok\":true}"));
            return;
        }
        StaticJsonDocument<256> doc;
        if (!deserializeJson(doc, body)) {
            if (doc.containsKey("rtttl")) {
                dpxBuzzerPlay(doc["rtttl"].as<const char*>());
                r->send(200, F("application/json"), F("{\"ok\":true}"));
                return;
            }
            if (doc.containsKey("sound")) {
                // Load RTTTL from /MELODIES/<name>.txt
                String path = String(F("/MELODIES/")) + doc["sound"].as<String>() + F(".txt");
                File f = LittleFS.open(path, "r");
                if (f) {
                    String rtttl = f.readString();
                    f.close();
                    rtttl.trim();
                    dpxBuzzerPlay(rtttl.c_str());
                    r->send(200, F("application/json"), F("{\"ok\":true}"));
                } else {
                    r->send(404, F("text/plain"), F("melody file not found"));
                }
                return;
            }
        }
        // Fallback: treat raw body as RTTTL string
        if (body.indexOf(':') > 0) {
            dpxBuzzerPlay(body.c_str());
            r->send(200, F("application/json"), F("{\"ok\":true}"));
        } else {
            r->send(400, F("text/plain"), F("rtttl or sound field required"));
        }
    });

    // ── File rename ───────────────────────────────────────────────────────────
    // {"from":"/ICONS/a.jpg","to":"/ICONS/b.jpg"}
    server.on("/api/rename", HTTP_POST, [](AsyncWebServerRequest* r) {
        String body = dpxBody(r);
        DynamicJsonDocument doc(256);
        if (deserializeJson(doc, body)) { r->send(400, F("text/plain"), F("bad JSON")); return; }
        String from = doc["from"] | String();
        String to   = doc["to"]   | String();
        if (from.isEmpty() || to.isEmpty()) { r->send(400, F("text/plain"), F("from/to required")); return; }
        if (!LittleFS.exists(from)) { r->send(404, F("text/plain"), F("not found")); return; }
        if (LittleFS.rename(from, to)) {
            r->send(200, F("application/json"), F("{\"ok\":true}"));
        } else {
            r->send(500, F("text/plain"), F("rename failed"));
        }
    });

    // ── WLED effects list ─────────────────────────────────────────────────────
    server.on("/api/effects", HTTP_GET, [](AsyncWebServerRequest* r) {
        DynamicJsonDocument doc(8192);
        JsonArray arr = doc.to<JsonArray>();
        uint8_t cnt = strip.getModeCount();
        for (uint8_t i = 0; i < cnt; i++) {
            const char* data = strip.getModeData(i);
            if (!data) continue;
            // Mode string format: "Name@param1,param2;..." — extract up to '@' or ';' or '\0'
            String name;
            while (*data && *data != '@' && *data != ';') name += *data++;
            name.trim();
            if (name.length()) arr.add(name);
        }
        String s; serializeJson(doc, s);
        r->send(200, F("application/json"), s);
    });

    // ── Transitions list (stub — not implemented) ─────────────────────────────
    server.on("/api/transitions", HTTP_GET, [](AsyncWebServerRequest* r) {
        // Transition names (cross-fade is the only supported type currently)
        r->send(200, F("application/json"), F("[\"fade\",\"slide\"]"));
    });

    // ── Reboot ────────────────────────────────────────────────────────────────
    server.on("/dpx/reboot", HTTP_ANY, [](AsyncWebServerRequest* r) {
        r->send(200, F("text/plain"), F("OK"));
        delay(200);
        ESP.restart();
    });
    server.on("/api/reboot", HTTP_ANY, [](AsyncWebServerRequest* r) {
        r->send(200, F("application/json"), F("{\"ok\":true}"));
        delay(200);
        ESP.restart();
    });

    // ── Sleep (ESP32 deep sleep) ──────────────────────────────────────────
    // POST body: {"sleep": N}  — sleep N seconds then wake via timer.
    // If N is 0 or omitted, sleeps indefinitely (wake on button press only).
    server.on("/api/sleep", HTTP_POST, [](AsyncWebServerRequest* r) {
        String body = dpxBody(r);
        DynamicJsonDocument doc(64);
        uint64_t secs = 0;
        if (!deserializeJson(doc, body) && doc.containsKey("sleep"))
            secs = (uint64_t)doc["sleep"].as<uint32_t>();
        r->send(200, F("application/json"), F("{\"ok\":true}"));
        delay(200);
        if (secs > 0)
            esp_sleep_enable_timer_wakeup(secs * 1000000ULL);
        esp_deep_sleep_start();
    });

    // ── Mute / unmute an app in the rotation ─────────────────────────────────
    // POST body: {"name":"Time","mute":true}   mute=false to un-mute
    server.on("/api/mute", HTTP_POST, [](AsyncWebServerRequest* r) {
        String body = dpxBody(r);
        DynamicJsonDocument doc(128);
        if (!deserializeJson(doc, body) && doc.containsKey("name")) {
            String name = doc["name"].as<String>();
            bool mute = doc.containsKey("mute") ? doc["mute"].as<bool>() : true;
            bool ok = dpxMuteApp(name, mute);
            r->send(ok ? 200 : 404, F("application/json"),
                    ok ? F("{\"ok\":true}") : F("{\"error\":\"not found\"}"));
            return;
        }
        r->send(400, F("text/plain"), F("name required"));
    });
}
