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

// ── Helpers ───────────────────────────────────────────────────────────────────

// Get plain-text POST body from AsyncWebServerRequest.
static inline String dpxBody(AsyncWebServerRequest* req) {
    if (req->hasParam("plain", true))
        return req->getParam("plain", true)->value();
    return "";
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
        dpxSetCustomApp(name, body.c_str());
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
    // {"color":[r,g,b],"blink":ms}  — blink is ignored (no HW PWM), color stored
    auto handleIndicator = [](AsyncWebServerRequest* r, int idx) {
        String body = dpxBody(r);
        DynamicJsonDocument doc(128);
        if (!deserializeJson(doc, body)) {
            if (doc.containsKey("color")) {
                JsonArray a = doc["color"].as<JsonArray>();
                if (a.size() >= 3)
                    dpxIndicator[idx] = ((uint32_t)(uint8_t)a[0].as<int>() << 16)
                                      | ((uint32_t)(uint8_t)a[1].as<int>() <<  8)
                                      |  (uint32_t)(uint8_t)a[2].as<int>();
                else
                    dpxIndicator[idx] = 0;
            }
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
        // GET: return local time string + UTC epoch
        time_t now; struct tm ti;
        time(&now); localtime_r(&now, &ti);
        char buf[24];
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &ti);
        DynamicJsonDocument doc(128);
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
        String body = dpxBody(r);
        DynamicJsonDocument doc(256);
        if (deserializeJson(doc, body)) { r->send(400, F("text/plain"), F("bad JSON")); return; }

        if (r->method() == HTTP_DELETE) {
            String path = doc["path"] | String();
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
        // POST: add listener
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

    // ── RTTTL / Sound — TC001 piezo buzzer on GPIO 15 ────────────────────────
    // POST /api/rtttl  body: raw RTTTL string (e.g. "Beep:d=4,o=5,b=200:c,e,g")
    server.on("/api/rtttl", HTTP_POST, [](AsyncWebServerRequest* r) {
        String body = dpxBody(r);
        body.trim();
        if (body.length()) {
            dpxBuzzerPlay(body.c_str());
            r->send(200, F("application/json"), F("{\"ok\":true}"));
        } else {
            r->send(400, F("text/plain"), F("RTTTL string required"));
        }
    });
    // POST /api/sound  body: JSON {"rtttl":"..."} — compatible with old senders
    server.on("/api/sound", HTTP_POST, [](AsyncWebServerRequest* r) {
        String body = dpxBody(r);
        body.trim();
        StaticJsonDocument<256> doc;
        if (!deserializeJson(doc, body) && doc.containsKey("rtttl")) {
            String s = doc["rtttl"].as<String>();
            dpxBuzzerPlay(s.c_str());
            r->send(200, F("application/json"), F("{\"ok\":true}"));
        } else if (body.startsWith("Beep") || body.indexOf(':') > 0) {
            // Plain RTTTL string passed to /api/sound as well
            dpxBuzzerPlay(body.c_str());
            r->send(200, F("application/json"), F("{\"ok\":true}"));
        } else {
            r->send(400, F("text/plain"), F("rtttl field required"));
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
        r->send(200, F("application/json"), F("[]"));
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
}
