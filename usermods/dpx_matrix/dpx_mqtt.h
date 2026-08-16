// ================================================================================
// dpx_mqtt.h — MQTT Integration
// ================================================================================
// Original work — dubpixel / dpx_tc002 (EUPL v1.2)
// ================================================================================
// PROJECT: dpx_tc002_frm
// ================================================================================
//
// File: dpx_mqtt.h
// Purpose: Subscribe to dpx/# topics and dispatch to dpx_matrix modules.
//          Uses WLED's existing AsyncMqttClient connection (global `mqtt`).
//
// Topic convention — subscribe root is: {mqttDeviceTopic}/dpx/#
// (e.g. "wled/AABBCCDDEEFF/dpx/#")
// Also accepts the short alias "dpx/#" for compatibility with dpx_tc001 senders.
//
// Supported topics (payload is JSON unless noted):
//   .../dpx/notify           → push notification  {"text":"...","duration":5}
//   .../dpx/notify/dismiss   → dismiss active notification (payload ignored)
//   .../dpx/tc               → timecode string "HH:MM:SS:FF" (plain text or JSON)
//   .../dpx/switch           → switch to app {"name":"clock"} or plain app name
//   .../dpx/nextapp          → advance app loop (payload ignored)
//   .../dpx/previousapp      → step back in app loop (payload ignored)
//   .../dpx/app/<name>       → create/update custom app by name
//   .../dpx/indicator/<1-3>  → set indicator pixel {"color":[r,g,b]|"#rrggbb",
//                               "blink":ms,"fade":ms} or "" to clear (same fields
//                               as POST /api/indicator1|2|3)
//   .../dpx/power            → {"power":true/false}
//   .../dpx/brightness       → {"bri":128}  (0–255)
//   .../dpx/rtttl            → raw RTTTL string or JSON {"rtttl":"..."} — "stop" silences
//   .../dpx/pair             → device-claim PIN display {"pin":"482913","duration":60,"scale":1}
//                               (duration secs default 60; scale 1=default/safe or 2=large/
//                               may clip) or "" to clear. Full-screen, highest render
//                               priority (even over notifications). See dpx_pair.h.
//   .../dpx/icon/list         → request the installed-icon listing (payload ignored).
//                               Response published to .../dpx/icon/list/result.
//   .../dpx/icon/get/<name>   → request one icon's raw bytes (<name> without .raw
//                               extension). Response published to .../dpx/icon/data/<name>.
//
// Published (not subscribed) on connect, retained:
//   .../dpx/info              → {"name","ip","mac","build"} — device registry metadata.
//                                Online/offline presence itself is WLED core's existing
//                                LWT at {mqttDeviceTopic}/status ("online"/"offline"),
//                                not duplicated here.
//
// Published (not subscribed) on request, not retained:
//   .../dpx/icon/list/result  → JSON array, same shape as GET /api/list?dir=/ICONS/
//   .../dpx/icon/data/<name>  → raw 192-byte RGB888 buffer (8x8, row-major) — the exact
//                                same format as GET /ICONS/<name>.raw over HTTP
//
// NOTE for the friendster/cuemaster server (see dpx_tc002_server.md): device topics are
// nested as "wled/<mac>/..." (mqttDeviceTopic), not a flat "<name>/...". A single-level
// "+/presence" subscription won't match — use "wled/+/status" and "wled/+/dpx/info".
//
// ================================================================================

#pragma once
#include "dpx_apps.h"
#include "dpx_notifications.h"
#include "dpx_tc.h"
#include "dpx_overlay.h"
#include "dpx_icons.h"
#include "../../wled00/dpx_build_id.h"

// dpxIndicator is defined in dpx_matrix.cpp; declared extern in dpx_osc.h
extern uint32_t dpxIndicator[3];
extern uint32_t dpxIndicatorBlink[3];
extern uint32_t dpxIndicatorFade[3];

// Relative sub-path we attach to mqttDeviceTopic. Must match what senders use.
static const char DPX_MQTT_SUB[] PROGMEM = "/dpx/#";
// Short alias accepted regardless of device topic (for cross-device compatibility)
static const char DPX_MQTT_ALIAS[] PROGMEM = "dpx/#";

// ── Internal helpers ──────────────────────────────────────────────────────────

// Strip the leading topic prefix and return the command portion ("dpx/cmd").
// Returns empty String if topic doesn't look like ours.
static String dpxMqttCmd(const char* topic) {
    String t(topic);
    // WLED core (mqtt.cpp) strips the mqttDeviceTopic prefix before forwarding
    // to usermods when the incoming topic starts with it, leaving a leading
    // slash: "wled/AABBCC/dpx/notify" arrives here as "/dpx/notify", not the
    // full topic. Check that stripped form first.
    if (t.startsWith(F("/dpx/"))) return t.substring(5);
    // In case a caller ever passes the full, unstripped topic directly.
    String dev = String(mqttDeviceTopic) + F("/dpx/");
    if (t.startsWith(dev)) return t.substring(dev.length());
    // Bare alias: "dpx/..." (device topic didn't match, core left it untouched)
    if (t.startsWith(F("dpx/"))) return t.substring(4);
    return "";
}

// ── Public API ────────────────────────────────────────────────────────────────

// Call from DpxMatrix::onMqttConnect()
static void dpxMqttConnect() {
    if (!WLED_MQTT_CONNECTED) return;
    // Per-device topic: wled/MAC/dpx/#
    String sub = String(mqttDeviceTopic) + "/dpx/#";
    mqtt->subscribe(sub.c_str(), 0);
    // Alias: dpx/#  (compatible with dpx_tc001 senders)
    mqtt->subscribe("dpx/#", 0);
    DEBUG_PRINTF("DpxMatrix: MQTT subscribed to %s and dpx/#\n", sub.c_str());

    // Retained device-registry metadata for the friendster/cuemaster server.
    // Presence (online/offline) is WLED core's own LWT at {mqttDeviceTopic}/status.
    StaticJsonDocument<192> info;
    info["name"]  = serverDescription;
    info["ip"]    = WiFi.localIP().toString();
    info["mac"]   = escapedMac;
    info["build"] = DPX_BUILD_ID;
    String infoStr;
    serializeJson(info, infoStr);
    String infoTopic = String(mqttDeviceTopic) + "/dpx/info";
    mqtt->publish(infoTopic.c_str(), 0, true, infoStr.c_str());
}

// Call from DpxMatrix::onMqttMessage(). Returns true if topic was ours.
static bool dpxMqttMessage(char* topic, char* payload) {
    String cmd = dpxMqttCmd(topic);
    if (!cmd.length()) return false;

    // ── notify ──────────────────────────────────────────────────────────────
    if (cmd == F("notify")) {
        dpxPushNotification(payload);
        return true;
    }
    if (cmd == F("notify/dismiss")) {
        dpxDismissNotification();
        return true;
    }

    // ── timecode ─────────────────────────────────────────────────────────────
    if (cmd == F("tc")) {
        String tc(payload);
        tc.trim();
        // Accept plain "HH:MM:SS:FF" or JSON {"tc":"HH:MM:SS:FF"}
        if (tc.startsWith("{")) {
            StaticJsonDocument<64> doc;
            if (!deserializeJson(doc, tc) && doc.containsKey("tc"))
                tc = doc["tc"].as<String>();
        }
        if (tc.length() >= 8) dpxPushTC(tc);
        return true;
    }

    // ── app loop control ──────────────────────────────────────────────────────
    if (cmd == F("nextapp"))     { dpxNextApp(); return true; }
    if (cmd == F("previousapp")) { dpxPrevApp(); return true; }

    if (cmd == F("switch")) {
        String p(payload); p.trim();
        // Accept plain name or {"name":"..."}
        if (!p.startsWith("{")) {
            StaticJsonDocument<64> doc;
            doc["name"] = p;
            String s; serializeJson(doc, s);
            dpxSwitchToApp(s.c_str());
        } else {
            dpxSwitchToApp(p.c_str());
        }
        return true;
    }

    // ── custom app upsert ─────────────────────────────────────────────────────
    // topic: .../dpx/app/<name>
    if (cmd.startsWith(F("app/"))) {
        String name = cmd.substring(4);
        if (name.length()) dpxSetCustomApp(name, payload);
        return true;
    }

    // ── indicator pixels ─────────────────────────────────────────────────────
    // topic: .../dpx/indicator/1  (or 2, 3)  payload: {"color":[r,g,b]|"#rrggbb",
    // "blink":ms,"fade":ms} — same fields as POST /api/indicator1|2|3, previously
    // MQTT only supported a static "color" while HTTP also had blink/fade.
    if (cmd.startsWith(F("indicator/"))) {
        int num = cmd.charAt(10) - '0';
        if (num >= 1 && num <= 3) {
            int idx = num - 1;
            String p(payload); p.trim();
            if (!p.length() || p == "0" || p == "off") {
                dpxIndicator[idx] = 0;
                dpxIndicatorBlink[idx] = 0;
                dpxIndicatorFade[idx] = 0;
            } else {
                StaticJsonDocument<128> doc;
                if (!deserializeJson(doc, p)) {
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
                            const char* s = cv.as<const char*>();
                            dpxIndicator[idx] = (s && s[0] == '#') ? (uint32_t)strtol(s + 1, nullptr, 16) : 0;
                        } else {
                            dpxIndicator[idx] = 0;
                        }
                    }
                    if (doc.containsKey("blink"))
                        dpxIndicatorBlink[idx] = (uint32_t)max(0, doc["blink"].as<int>());
                    if (doc.containsKey("fade"))
                        dpxIndicatorFade[idx] = (uint32_t)max(0, doc["fade"].as<int>());
                }
            }
        }
        return true;
    }

    // ── power / brightness ────────────────────────────────────────────────────
    if (cmd == F("power")) {
        StaticJsonDocument<64> doc;
        if (!deserializeJson(doc, payload) && doc.containsKey("power")) {
            bool on = doc["power"].as<bool>();
            bri = on ? (briLast > 0 ? briLast : 128) : 0;
            stateUpdated(CALL_MODE_DIRECT_CHANGE);
        }
        return true;
    }
    if (cmd == F("brightness")) {
        StaticJsonDocument<64> doc;
        String p(payload); p.trim();
        if (p.length() && isDigit(p[0])) {
            bri = (uint8_t)p.toInt();
        } else if (!deserializeJson(doc, p) && doc.containsKey("bri")) {
            bri = doc["bri"].as<uint8_t>();
        }
        stateUpdated(CALL_MODE_DIRECT_CHANGE);
        return true;
    }

    // ── RTTTL / buzzer ────────────────────────────────────────────────────────
    // topic: .../dpx/rtttl  payload: raw RTTTL string or JSON {"rtttl":"..."}
    if (cmd == F("rtttl")) {
        String p(payload); p.trim();
        if (!p.length() || p == "stop") {
            dpxBuzzerStop();
        } else if (p.startsWith("{")) {
            StaticJsonDocument<256> doc;
            if (!deserializeJson(doc, p) && doc.containsKey("rtttl"))
                dpxBuzzerPlay(doc["rtttl"].as<const char*>());
        } else {
            dpxBuzzerPlay(p.c_str());
        }
        return true;
    }

    // ── Channel mute ──────────────────────────────────────────────────────────
    // topic: .../dpx/mute/<AppName>  payload: 1/0/true/false/on/off
    if (cmd.startsWith(F("mute/"))) {
        String name = cmd.substring(5);
        String p(payload); p.trim(); p.toLowerCase();
        bool mute = (p == "1" || p == "true" || p == "on");
        dpxMuteApp(name, mute);
        return true;
    }

    // ── Text overlay ──────────────────────────────────────────────────────────
    if (cmd == F("overlay")) {
        String p(payload); p.trim();
        if (!p.length() || p == "0" || p == "off" || p == "clear")
            dpxClearOverlay();
        else
            dpxSetOverlay(p.c_str());
        return true;
    }

    // ── Pixel effect ──────────────────────────────────────────────────────────
    if (cmd == F("effect")) {
        String p(payload); p.trim();
        if (!p.length() || p == "none" || p == "off" || p == "clear")
            dpxClearPixelEffect();
        else
            dpxSetPixelEffect(p.c_str());
        return true;
    }

    // ── Pairing PIN (device-claim flow, see dpx_pair.h) ────────────────────────
    // topic: .../dpx/pair  payload: {"pin":"482913","duration":60} or "" to clear
    if (cmd == F("pair")) {
        dpxSetPair(payload);
        return true;
    }

    // ── Icon list / fetch over MQTT ────────────────────────────────────────────
    // The friendster/cuemaster server may be cloud-hosted while this device
    // sits on a venue/home LAN — HTTP can't reach it, but MQTT always can
    // (same reasoning as everything else in this file). Request/response uses
    // distinct topic names (not the request topic itself) so the device's own
    // `dpx/#` subscription doesn't re-trigger on its own published reply.
    //
    // topic: .../dpx/icon/list  → publishes JSON array (same shape as
    // GET /api/list?dir=/ICONS/) to .../dpx/icon/list/result
    if (cmd == F("icon/list")) {
        DynamicJsonDocument doc(4096);
        JsonArray arr = doc.to<JsonArray>();
        File d = LittleFS.open("/ICONS/", "r");
        if (d && d.isDirectory()) {
            File f = d.openNextFile();
            while (f) {
                JsonObject o = arr.createNestedObject();
                String name = f.name();
                int slash = name.lastIndexOf('/');
                if (slash >= 0) name = name.substring(slash + 1); // basename only
                o["name"] = name;
                o["type"] = f.isDirectory() ? "dir" : "file";
                o["size"] = f.size();
                f = d.openNextFile();
            }
        }
        String s; serializeJson(doc, s);
        String resultTopic = String(mqttDeviceTopic) + "/dpx/icon/list/result";
        mqtt->publish(resultTopic.c_str(), 0, false, s.c_str());
        return true;
    }

    // topic: .../dpx/icon/get/<name>  (name WITHOUT the .raw extension, same
    // convention as the CustomApp "icon" field elsewhere) → publishes the raw
    // 192-byte RGB888 buffer to .../dpx/icon/data/<name>
    if (cmd.startsWith(F("icon/get/"))) {
        String name = cmd.substring(9);
        if (name.length()) {
            // Load directly via dpxLoadIcon() into a local buffer instead of
            // dpxGetIcon()'s shared single-slot cache. AsyncMqttClient message
            // callbacks run on the async_tcp task, not the main loop task that
            // owns the render path (dpx_apps.h also calls dpxGetIcon() every
            // frame) — two tasks mutating that shared static String/cache with
            // no locking is a heap-corruption race, not just a redundant load.
            uint32_t pixels[DPX_ICON_PIXELS];
            if (dpxLoadIcon(name, pixels)) {
                uint8_t buf[DPX_ICON_BYTES];
                for (int i = 0; i < DPX_ICON_PIXELS; i++) {
                    buf[i * 3]     = (pixels[i] >> 16) & 0xFF;
                    buf[i * 3 + 1] = (pixels[i] >> 8)  & 0xFF;
                    buf[i * 3 + 2] =  pixels[i]        & 0xFF;
                }
                String dataTopic = String(mqttDeviceTopic) + "/dpx/icon/data/" + name;
                mqtt->publish(dataTopic.c_str(), 0, false, (const char*)buf, DPX_ICON_BYTES);
            }
        }
        return true;
    }

    return false; // not our topic
}
