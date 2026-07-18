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
//   .../dpx/indicator/<1-3>  → set indicator pixel {"color":[r,g,b]} or "" to clear
//   .../dpx/power            → {"power":true/false}
//   .../dpx/brightness       → {"bri":128}  (0–255)
//   .../dpx/rtttl            → raw RTTTL string or JSON {"rtttl":"..."} — "stop" silences
//
// ================================================================================

#pragma once
#include "dpx_apps.h"
#include "dpx_notifications.h"
#include "dpx_tc.h"
#include "dpx_overlay.h"

// dpxIndicator is defined in dpx_matrix.cpp; declared extern in dpx_osc.h
extern uint32_t dpxIndicator[3];

// Relative sub-path we attach to mqttDeviceTopic. Must match what senders use.
static const char DPX_MQTT_SUB[] PROGMEM = "/dpx/#";
// Short alias accepted regardless of device topic (for cross-device compatibility)
static const char DPX_MQTT_ALIAS[] PROGMEM = "dpx/#";

// ── Internal helpers ──────────────────────────────────────────────────────────

// Strip the leading topic prefix and return the command portion ("dpx/cmd").
// Returns empty String if topic doesn't look like ours.
static String dpxMqttCmd(const char* topic) {
    String t(topic);
    // Match either "{mqttDeviceTopic}/dpx/..." or "dpx/..."
    String dev = String(mqttDeviceTopic) + F("/dpx/");
    if (t.startsWith(dev)) return t.substring(dev.length());
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
    // topic: .../dpx/indicator/1  (or 2, 3)
    if (cmd.startsWith(F("indicator/"))) {
        int num = cmd.charAt(10) - '0';
        if (num >= 1 && num <= 3) {
            String p(payload); p.trim();
            if (!p.length() || p == "0" || p == "off") {
                dpxIndicator[num - 1] = 0;
            } else {
                StaticJsonDocument<128> doc;
                if (!deserializeJson(doc, p) && doc.containsKey("color")) {
                    JsonArray a = doc["color"].as<JsonArray>();
                    if (a.size() >= 3)
                        dpxIndicator[num - 1] = ((uint32_t)(uint8_t)a[0] << 16)
                                              | ((uint32_t)(uint8_t)a[1] <<  8)
                                              |  (uint32_t)(uint8_t)a[2];
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

    return false; // not our topic
}
