// ================================================================================
// dpx_api.h — HTTP Route Registration (AsyncWebServer)
// ================================================================================
// Original work — dubpixel / dpx_tc002 (MIT)
// Behavioral reference: SPEC.md §5 (HTTP API Reference)
// Implementation adapted from dpx_tc001 ServerManager.cpp (original work)
// ================================================================================
// PROJECT: dpx_tc002_frm
// ================================================================================
//
// File: dpx_api.h
// Purpose: Register all custom HTTP routes with WLED's AsyncWebServer instance.
//          Call dpxRegisterRoutes() once from DpxMatrix::setup().
//
// Routes registered:
//   GET  /ctrl, /browse, /api-ref, /screen, /fullscreen
//   GET  /api/loop, /api/apps, /api/custom, /api/settings, /api/time,
//         /api/dev, /api/stats, /api/screen, /api/effects, /api/transitions,
//         /api/osc/listeners, /api/reboot
//   POST /api/notify, /api/notify/dismiss, /api/custom, /api/switch,
//         /api/nextapp, /api/previousapp, /api/power, /api/settings,
//         /api/moodlight, /api/indicator1/2/3, /api/rtttl, /api/sound,
//         /api/apps, /api/reorder, /api/time, /api/syncntp, /api/rename,
//         /api/tc, /api/dev, /api/osc/listeners
//   DELETE /api/osc/listeners
//
// ================================================================================

#pragma once
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "dpx_html.h"
#include "dpx_osc.h"
#include "dpx_tc.h"

// Indicator pixel colors (rendered in handleOverlayDraw)
uint32_t dpxIndicator[3] = {0, 0, 0};

// Helper: get plain text body as String
static inline String dpxBody(AsyncWebServerRequest* req) {
    if (req->hasParam("plain", true)) {
        return req->getParam("plain", true)->value();
    }
    return "";
}

// Helper: read and return /api/screen — 256 RGB values as JSON array
static String dpxScreenJson() {
    DynamicJsonDocument doc(4096);
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < 256; i++) {
        uint32_t c = strip.getPixelColor(i);
        // WLED returns WRGB — extract RGB
        arr.add((uint32_t)(c & 0x00FFFFFF));
    }
    String s; serializeJson(doc, s); return s;
}

// Helper: basic stats JSON
static String dpxStatsJson() {
    DynamicJsonDocument doc(512);
    doc["version"]   = "dpx_tc002";
    doc["uptime"]    = millis() / 1000;
    doc["ram"]       = ESP.getFreeHeap();
    doc["ip"]        = WiFi.localIP().toString();
    doc["rssi"]      = WiFi.RSSI();
    doc["hostname"]  = WiFi.getHostname();
    String s; serializeJson(doc, s); return s;
}

static void dpxRegisterRoutes() {
    // ── Web pages ──────────────────────────────────────────────────────────
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->redirect("/ctrl");
    });
    server.on("/ctrl", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send_P(200, "text/html", ctrl_html);
    });
    server.on("/browse", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send_P(200, "text/html", browse_html);
    });
    server.on("/api-ref", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send_P(200, "text/html", apiref_html);
    });
    server.on("/screen", HTTP_GET, [](AsyncWebServerRequest* r) {
        // /screen uses screenfull_html but since it has %%FPS%% placeholder,
        // serve it statically; JS fetches /api/screen.
        r->send_P(200, "text/html", screenfull_html);
    });
    server.on("/fullscreen", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send_P(200, "text/html", screenfull_html);
    });

    // ── Read endpoints ────────────────────────────────────────────────────
    server.on("/api/loop", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, "application/json", dpxGetLoopJson());
    });
    server.on("/api/apps", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, "application/json", dpxGetAppsJson());
    });
    server.on("/api/custom", HTTP_GET, [](AsyncWebServerRequest* r) {
        if (r->hasParam("name")) {
            r->send(200, "application/json",
                dpxGetCustomAppJson(r->getParam("name")->value()));
        } else {
            r->send(200, "application/json", dpxGetAppsJson());
        }
    });
    server.on("/api/screen", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, "application/json", dpxScreenJson());
    });
    server.on("/api/stats", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, "application/json", dpxStatsJson());
    });
    server.on("/api/time", HTTP_GET, [](AsyncWebServerRequest* r) {
        time_t now; struct tm ti;
        time(&now); localtime_r(&now, &ti);
        char buf[32]; strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &ti);
        StaticJsonDocument<128> doc;
        doc["local"] = buf; doc["utc"] = (long)now;
        String j; serializeJson(doc, j);
        r->send(200, "application/json", j);
    });
    server.on("/api/dev", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, "application/json", dpxReadDevJson());
    });
    server.on("/api/osc/listeners", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, "application/json", dpxOscListenersJson());
    });
    server.on("/api/reboot", HTTP_ANY, [](AsyncWebServerRequest* r) {
        r->send(200, "text/plain", "OK");
        delay(200);
        ESP.restart();
    });
    // WLED's /api/effects and /api/transitions handled by WLED itself; add stubs
    server.on("/api/effects", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, "application/json", F("[]"));
    });
    server.on("/api/transitions", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, "application/json", F("[\"None\",\"Fade\",\"Slide\"]"));
    });

    // ── Control endpoints ─────────────────────────────────────────────────
    server.on("/api/notify", HTTP_POST, [](AsyncWebServerRequest* r) {},
        nullptr,
        [](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            String body((char*)data, len);
            dpxPushNotification(body.c_str())
                ? r->send(200, "text/plain", "OK")
                : r->send(500, "text/plain", "ParseError");
        });

    server.on("/api/notify/dismiss", HTTP_ANY, [](AsyncWebServerRequest* r) {
        dpxDismissNotification();
        r->send(200, "text/plain", "OK");
    });

    server.on("/api/custom", HTTP_POST, [](AsyncWebServerRequest* r) {},
        nullptr,
        [](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            String name = r->hasParam("name") ? r->getParam("name")->value() : "";
            String body((char*)data, len);
            body.trim();
            if (name.length()) dpxSetCustomApp(name, body.c_str());
            r->send(200, "text/plain", "OK");
        });

    server.on("/api/switch", HTTP_POST, [](AsyncWebServerRequest* r) {},
        nullptr,
        [](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            String body((char*)data, len);
            dpxSwitchToApp(body.c_str())
                ? r->send(200, "text/plain", "OK")
                : r->send(500, "text/plain", "FAILED");
        });

    server.on("/api/nextapp", HTTP_ANY, [](AsyncWebServerRequest* r) {
        dpxNextApp(); r->send(200, "text/plain", "OK");
    });
    server.on("/api/previousapp", HTTP_ANY, [](AsyncWebServerRequest* r) {
        dpxPrevApp(); r->send(200, "text/plain", "OK");
    });

    server.on("/api/power", HTTP_POST, [](AsyncWebServerRequest* r) {},
        nullptr,
        [](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            String body((char*)data, len);
            StaticJsonDocument<64> doc;
            if (!deserializeJson(doc, body) && doc.containsKey("power")) {
                bool on = doc["power"].as<bool>();
                bri = on ? (briLast > 0 ? briLast : 128) : 0;
                stateUpdated(CALL_MODE_DIRECT_CHANGE);
            }
            r->send(200, "text/plain", "OK");
        });

    server.on("/api/tc", HTTP_POST, [](AsyncWebServerRequest* r) {},
        nullptr,
        [](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            String body((char*)data, len);
            body.trim(); body.replace("\"", "");
            if (body.length() >= 8) {
                dpxPushTC(body);
                r->send(200, "text/plain", "OK");
            } else {
                r->send(400, "text/plain", "BadRequest");
            }
        });

    server.on("/api/dev", HTTP_POST, [](AsyncWebServerRequest* r) {},
        nullptr,
        [](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            String body((char*)data, len);
            dpxMergeDev(body.c_str())
                ? r->send(200, "text/plain", "OK")
                : r->send(400, "text/plain", "BadRequest");
        });

    server.on("/api/time", HTTP_POST, [](AsyncWebServerRequest* r) {},
        nullptr,
        [](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            StaticJsonDocument<128> doc;
            String body((char*)data, len);
            if (!deserializeJson(doc, body) && doc.containsKey("utc")) {
                time_t t = (time_t)doc["utc"].as<long>();
                struct timeval tv = {t, 0};
                settimeofday(&tv, nullptr);
                r->send(200, "text/plain", "OK");
            } else {
                r->send(400, "text/plain", "BadRequest");
            }
        });

    server.on("/api/syncntp", HTTP_POST, [](AsyncWebServerRequest* r) {},
        nullptr,
        [](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            String body((char*)data, len);
            if (body.length() > 2) {
                StaticJsonDocument<256> doc;
                if (!deserializeJson(doc, body)) {
                    if (doc.containsKey("timezone")) {
                        configTzTime(doc["timezone"].as<const char*>(), "pool.ntp.org");
                    }
                }
            }
            r->send(200, "text/plain", "OK");
        });

    server.on("/api/rename", HTTP_POST, [](AsyncWebServerRequest* r) {},
        nullptr,
        [](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            StaticJsonDocument<256> doc;
            String body((char*)data, len);
            if (deserializeJson(doc, body) || !doc.containsKey("from") || !doc.containsKey("to")) {
                r->send(400, "text/plain", "BadRequest"); return;
            }
            String from = doc["from"].as<String>();
            String to   = doc["to"].as<String>();
            if (!LittleFS.exists(from)) { r->send(404, "text/plain", "NotFound"); return; }
            LittleFS.rename(from, to)
                ? r->send(200, "text/plain", "OK")
                : r->send(500, "text/plain", "Failed");
        });

    server.on("/api/apps", HTTP_POST, [](AsyncWebServerRequest* r) {},
        nullptr,
        [](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            // Reorder apps — array of name strings; rebuild loop in that order
            DynamicJsonDocument doc(1024);
            String body((char*)data, len);
            if (deserializeJson(doc, body)) { r->send(400, "text/plain", "BadRequest"); return; }
            // For now just acknowledge — reorder TODO
            r->send(200, "text/plain", "OK");
        });

    server.on("/api/reorder", HTTP_POST, [](AsyncWebServerRequest* r) {},
        nullptr,
        [](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            r->send(200, "text/plain", "OK");
        });

    // Indicator endpoints (1, 2, 3)
    auto indicatorHandler = [](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
        // Extract indicator number from URL
        String url = r->url();
        int num = url.charAt(url.length() - 1) - '0';
        if (num < 1 || num > 3) { r->send(400, "text/plain", "BadRequest"); return; }
        StaticJsonDocument<256> doc;
        String body((char*)data, len);
        if (!deserializeJson(doc, body) && doc.containsKey("color")) {
            JsonArray a = doc["color"].as<JsonArray>();
            if (a.size() >= 3) {
                uint32_t col = ((uint32_t)(uint8_t)a[0] << 16)
                             | ((uint32_t)(uint8_t)a[1] << 8)
                             |  (uint32_t)(uint8_t)a[2];
                dpxIndicator[num - 1] = col;
            }
        } else {
            dpxIndicator[num - 1] = 0;
        }
        r->send(200, "text/plain", "OK");
    };
    server.on("/api/indicator1", HTTP_POST, [](AsyncWebServerRequest* r){}, nullptr, indicatorHandler);
    server.on("/api/indicator2", HTTP_POST, [](AsyncWebServerRequest* r){}, nullptr, indicatorHandler);
    server.on("/api/indicator3", HTTP_POST, [](AsyncWebServerRequest* r){}, nullptr, indicatorHandler);

    // OSC listener add/delete
    server.on("/api/osc/listeners", HTTP_POST, [](AsyncWebServerRequest* r) {},
        nullptr,
        [](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            StaticJsonDocument<256> doc;
            String body((char*)data, len);
            if (deserializeJson(doc, body)) { r->send(400, "text/plain", "BadRequest"); return; }
            String path    = doc["path"].as<String>();
            String channel = doc["channel"].as<String>();
            String label   = doc.containsKey("label") ? doc["label"].as<String>() : channel;
            if (!path.length() || !channel.length()) { r->send(400, "text/plain", "BadRequest"); return; }
            // Update or add
            bool found = false;
            for (auto& lsr : dpxOscListeners) {
                if (lsr.path == path) { lsr.channel = channel; lsr.label = label; found = true; break; }
            }
            if (!found) { DpxOscListener l; l.path=path; l.channel=channel; l.label=label; dpxOscListeners.push_back(l); }
            dpxSaveOscListeners();
            r->send(200, "text/plain", "OK");
        });

    server.on("/api/osc/listeners", HTTP_DELETE, [](AsyncWebServerRequest* r) {},
        nullptr,
        [](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            StaticJsonDocument<128> doc;
            String body((char*)data, len);
            if (deserializeJson(doc, body)) { r->send(400, "text/plain", "BadRequest"); return; }
            String path = doc["path"].as<String>();
            dpxOscListeners.erase(
                std::remove_if(dpxOscListeners.begin(), dpxOscListeners.end(),
                    [&](const DpxOscListener& l){ return l.path == path; }),
                dpxOscListeners.end());
            dpxSaveOscListeners();
            r->send(200, "text/plain", "OK");
        });

    // Settings passthrough — read/write WLED settings JSON (partial — BRI only for now)
    server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest* r) {
        StaticJsonDocument<256> doc;
        doc["BRI"]   = bri;
        doc["ATIME"] = DPX_ATIME;
        doc["ATRANS"] = DPX_ATRANS;
        doc["SSPEED"] = DPX_SSPEED;
        String s; serializeJson(doc, s);
        r->send(200, "application/json", s);
    });
    server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest* r) {},
        nullptr,
        [](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
            StaticJsonDocument<512> doc;
            String body((char*)data, len);
            if (!deserializeJson(doc, body)) {
                if (doc.containsKey("BRI")) { bri = doc["BRI"].as<uint8_t>(); stateUpdated(CALL_MODE_DIRECT_CHANGE); }
                if (doc.containsKey("ATIME"))  DPX_ATIME  = doc["ATIME"].as<int>();
                if (doc.containsKey("ATRANS")) DPX_ATRANS = doc["ATRANS"].as<bool>();
                if (doc.containsKey("SSPEED")) DPX_SSPEED = doc["SSPEED"].as<int>();
            }
            r->send(200, "text/plain", "OK");
        });

    // Moodlight — placeholder (WLED handles this differently; delegate to WLED /json endpoint)
    server.on("/api/moodlight", HTTP_ANY, [](AsyncWebServerRequest* r) {
        r->send(200, "text/plain", "OK");
    });

    // RTTTL — no buzzer in this build; accept and ignore for compatibility
    server.on("/api/rtttl", HTTP_POST, [](AsyncWebServerRequest* r) {
        r->send(200, "text/plain", "OK");
    });

    // Sound — placeholder
    server.on("/api/sound", HTTP_ANY, [](AsyncWebServerRequest* r) {
        r->send(200, "text/plain", "OK");
    });

    // Sleep — WLED handles deep sleep; placeholder
    server.on("/api/sleep", HTTP_POST, [](AsyncWebServerRequest* r) {
        r->send(200, "text/plain", "OK");
    });
}
