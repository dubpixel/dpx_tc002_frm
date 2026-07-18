// ================================================================================
// dpx_osc.h — OSC Receiver (UDP port 4210)
// ================================================================================
// Original work — dubpixel / dpx_tc002 (EUPL v1.2)
// Ported/adapted from dpx_tc001 ServerManager.cpp::handleOSC()
// (original work — OSC protocol handling, not AWTRIX-derived)
// ================================================================================
// PROJECT: dpx_tc002_frm
// ================================================================================
//
// File: dpx_osc.h
// Purpose: Receives OSC packets on UDP port 4210. Handles built-in addresses
//          and routes unknown addresses through the OSC Listener Registry
//          (d3 disguise integration). FIND_AWTRIX discovery also handled here.
//
// Supported addresses (both bare and /dpx_tc002/ or /awtrix/ prefixed):
//   /notify, /text, /tc, /custom/<name>, /switch,
//   /nextapp, /previousapp, /power, /brightness,
//   /indicator/1, /indicator/2, /indicator/3
//
// ================================================================================

#pragma once
#include <WiFiUdp.h>
#include <LittleFS.h>
#include "dpx_tc.h"
#include "dpx_notifications.h"

static WiFiUDP dpxUdp;
static const uint16_t DPX_OSC_PORT = 4210;
static bool dpxUdpStarted = false;

// ── OSC binary helpers ────────────────────────────────────────────────────────
static String dpxOscStr(const uint8_t* b, int& p, int len) {
    String s;
    while (p < len && b[p]) s += (char)b[p++];
    p++;                    // consume null terminator
    p = (p + 3) & ~3;       // pad to 4-byte boundary
    return s;
}

static int32_t dpxOscI32(const uint8_t* b, int& p) {
    int32_t v = ((int32_t)b[p] << 24) | ((int32_t)b[p+1] << 16)
              | ((int32_t)b[p+2] << 8)  |  (int32_t)b[p+3];
    p += 4;
    return v;
}

static float dpxOscF32(const uint8_t* b, int& p) {
    uint32_t raw = ((uint32_t)b[p] << 24) | ((uint32_t)b[p+1] << 16)
                 | ((uint32_t)b[p+2] << 8)  |  (uint32_t)b[p+3];
    p += 4;
    float f;
    memcpy(&f, &raw, sizeof f);
    return f;
}

static int dpxOscNum(const uint8_t* b, int& p, char tag) {
    if (tag == 'f') return (int)dpxOscF32(b, p);
    return (int)dpxOscI32(b, p);
}

// ── OSC Listener Registry — persist ──────────────────────────────────────────
static void dpxSaveOscListeners() {
    DynamicJsonDocument doc(2048);
    JsonArray arr = doc.to<JsonArray>();
    for (auto& lsr : dpxOscListeners) {
        JsonObject o = arr.createNestedObject();
        o["path"] = lsr.path; o["channel"] = lsr.channel; o["label"] = lsr.label;
    }
    File f = LittleFS.open("/osc_listeners.json", "w");
    if (f) { serializeJson(doc, f); f.close(); }
}

static void dpxLoadOscListeners() {
    if (!LittleFS.exists("/osc_listeners.json")) return;
    File f = LittleFS.open("/osc_listeners.json", "r");
    if (!f) return;
    DynamicJsonDocument doc(2048);
    if (deserializeJson(doc, f)) { f.close(); return; }
    f.close();
    dpxOscListeners.clear();
    for (JsonObject o : doc.as<JsonArray>()) {
        DpxOscListener lsr;
        lsr.path    = o["path"].as<String>();
        lsr.channel = o["channel"].as<String>();
        lsr.label   = o.containsKey("label") ? o["label"].as<String>() : lsr.channel;
        if (!lsr.path.isEmpty() && !lsr.channel.isEmpty())
            dpxOscListeners.push_back(lsr);
    }
}

static String dpxOscListenersJson() {
    DynamicJsonDocument doc(2048);
    JsonArray arr = doc.to<JsonArray>();
    for (auto& lsr : dpxOscListeners) {
        JsonObject o = arr.createNestedObject();
        o["path"] = lsr.path; o["channel"] = lsr.channel; o["label"] = lsr.label;
    }
    String s; serializeJson(doc, s); return s;
}

// ── Main OSC dispatch ─────────────────────────────────────────────────────────
static void dpxHandleOSC(const uint8_t* buf, int len) {
    if (len < 4 || buf[0] != '/') return;
    int pos = 0;
    String addr = dpxOscStr(buf, pos, len);
    if (pos >= len || buf[pos] != ',') return;
    String tags = dpxOscStr(buf, pos, len); // tags[0] == ','

    // Accept /dpx_tc002/... as primary; /dpx_tc001/ and /awtrix/ for compatibility
    if      (addr.startsWith("/dpx_tc002")) addr = addr.substring(10);
    else if (addr.startsWith("/dpx_tc001")) addr = addr.substring(10);
    else if (addr.startsWith("/awtrix"))    addr = addr.substring(7);
    if (addr.isEmpty()) addr = "/";

    if (addr == "/tc") {
        if (tags.length() >= 2 && tags[1] == 's') {
            String tc = dpxOscStr(buf, pos, len);
            dpxPushTC(tc);
        }
    } else if (addr == "/notify" || addr == "/text") {
        if (tags.length() >= 2 && tags[1] == 's') {
            String text = dpxOscStr(buf, pos, len);
            StaticJsonDocument<256> doc;
            doc["text"] = text;
            String json; serializeJson(doc, json);
            dpxPushNotification(json.c_str());
        }
    } else if (addr.startsWith("/custom/")) {
        String name = addr.substring(8);
        if (!name.isEmpty() && tags.length() >= 2 && tags[1] == 's') {
            String text = dpxOscStr(buf, pos, len);
            if (name == "tc") {
                dpxPushTC(text);
            } else {
                StaticJsonDocument<256> doc;
                doc["text"] = text;
                String json; serializeJson(doc, json);
                dpxSetCustomApp(name, json.c_str());
            }
        }
    } else if (addr == "/switch") {
        if (tags.length() >= 2 && tags[1] == 's') {
            String name = dpxOscStr(buf, pos, len);
            StaticJsonDocument<128> doc;
            doc["name"] = name;
            String json; serializeJson(doc, json);
            dpxSwitchToApp(json.c_str());
        }
    } else if (addr == "/nextapp")     { dpxNextApp(); }
    else if (addr == "/previousapp")   { dpxPrevApp(); }
    else if (addr == "/power") {
        if (tags.length() >= 2) {
            bool on = (dpxOscNum(buf, pos, tags[1]) != 0);
            if (!on) {
                // Turn WLED off
                bri = 0;
                stateUpdated(CALL_MODE_DIRECT_CHANGE);
            } else {
                bri = briLast > 0 ? briLast : 128;
                stateUpdated(CALL_MODE_DIRECT_CHANGE);
            }
        }
    } else if (addr == "/brightness") {
        if (tags.length() >= 2) {
            int b = constrain(dpxOscNum(buf, pos, tags[1]), 0, 255);
            bri = b;
            stateUpdated(CALL_MODE_DIRECT_CHANGE);
        }
    } else if (addr.startsWith("/indicator/")) {
        // Indicator dots — stored as corner pixels
        int num = addr.substring(11).toInt();
        if (num >= 1 && num <= 3 && tags.length() >= 4) {
            int r = constrain(dpxOscNum(buf, pos, tags[1]), 0, 255);
            int g = constrain(dpxOscNum(buf, pos, tags[2]), 0, 255);
            int b = constrain(dpxOscNum(buf, pos, tags[3]), 0, 255);
            // Store indicator colors (rendered in handleOverlayDraw)
            // Indicator 1 = pixel 0 (top-left), 2 = pixel 31, 3 = pixel 224
            uint32_t col = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            extern uint32_t dpxIndicator[3];
            if (num >= 1 && num <= 3) dpxIndicator[num - 1] = col;
        }
    } else {
        // Dynamic OSC listener registry (d3 monitoring paths)
        for (auto& lsr : dpxOscListeners) {
            if (addr == lsr.path && tags.length() >= 2) {
                String val;
                char t = tags[1];
                if      (t == 's') val = dpxOscStr(buf, pos, len);
                else if (t == 'f') val = String(dpxOscF32(buf, pos), 4);
                else if (t == 'i') val = String(dpxOscI32(buf, pos));
                if (lsr.channel == "tc") {
                    dpxPushTC(val);
                } else {
                    StaticJsonDocument<256> doc;
                    doc["text"] = val;
                    String json; serializeJson(doc, json);
                    dpxSetCustomApp(lsr.channel, json.c_str());
                }
                break;
            }
        }
    }
}

// ── UDP receive tick — call from loop() ───────────────────────────────────────
#define DPX_UDP_BUF 512
static char dpxUdpBuf[DPX_UDP_BUF];

static void dpxOscTick() {
    if (!dpxUdpStarted) return;
    int pkt = dpxUdp.parsePacket();
    if (!pkt) return;
    int len = dpxUdp.read(dpxUdpBuf, DPX_UDP_BUF - 1);
    if (len <= 0) return;
    dpxUdpBuf[len] = 0;

    if (dpxUdpBuf[0] == '/') {
        // OSC packet
        dpxHandleOSC((const uint8_t*)dpxUdpBuf, len);
    } else if (strcmp(dpxUdpBuf, "FIND_AWTRIX") == 0) {
        // Discovery response — reply on port 4211 with hostname:port
        char resp[64];
        snprintf(resp, sizeof(resp), "%s", WiFi.getHostname());
        dpxUdp.beginPacket(dpxUdp.remoteIP(), 4211);
        dpxUdp.write((const uint8_t*)resp, strlen(resp));
        dpxUdp.endPacket();
    }
}

// ── Start UDP listener ────────────────────────────────────────────────────────
static void dpxOscBegin() {
    if (dpxUdpStarted) return;
    dpxUdp.begin(DPX_OSC_PORT);
    dpxUdpStarted = true;
}
