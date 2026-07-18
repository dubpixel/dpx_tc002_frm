// ================================================================================
// dpx_apps.h — App Loop System
// ================================================================================
// Original work — dubpixel / dpx_tc002 (EUPL v1.2)
// NOT derived from MatrixDisplayUi.cpp (AWTRIX CC BY-NC-SA 4.0).
// Written from scratch using SPEC.md §3-4 as behavioral reference.
// ================================================================================
// PROJECT: dpx_tc002_frm
// ================================================================================
//
// File: dpx_apps.h
// Purpose: CustomApp struct, app loop vector, rotation timer, app management.
//          Native apps (Time, Date, Temp) render their own content in render().
//
// ================================================================================

#pragma once
#include <vector>
#include <map>
#include <set>
#include <Arduino.h>
#include "dpx_text.h"
#include "dpx_persist.h"

// ── Draw instruction (subset of SPEC.md §4.3) ────────────────────────────────
struct DpxDrawCmd {
    String cmd;       // "dp","dl","dr","df","dc","dfc","dt","db"
    // Numeric args (up to 5)
    int    n[5];
    String s;         // string arg for "dt" command
    uint32_t color;
};

// ── Custom App data (SPEC.md §4.1 key fields) ────────────────────────────────
struct DpxCustomApp {
    String   text;
    uint32_t color       = 0xFFFFFF;
    uint32_t background  = 0x000000;
    bool     rainbow     = false;
    bool     center      = false;
    bool     noScroll    = false;
    int      scrollSpeed = 100;      // % of base speed
    bool     topText     = false;
    int      duration    = 0;        // seconds; 0 = use DPX_ATIME
    int16_t  repeat      = -1;
    int      progress    = -1;       // progress bar 0-100, -1=off
    uint32_t pColor      = 0x00FF00;
    uint32_t pbColor     = 0x1a1a1a;
    uint64_t lifetime    = 0;        // auto-remove after N seconds (0=off)
    unsigned long addedMs = 0;       // millis() when app was added
    bool     save        = false;
    std::vector<DpxDrawCmd> drawCmds;
    String   overlay   = "";   // per-app pixel effect name (e.g. "rain", "snow")
    String   icon      = "";   // icon name (no extension, no path)
    int      pushIcon  = 0;    // 0=fixed left, 1=scroll+gone, 2=scroll+loop

    bool valid = false;  // false = slot unused

    // Effective display duration in milliseconds
    unsigned long durationMs() const {
        int secs = (duration > 0) ? duration : DPX_ATIME;
        return (unsigned long)secs * 1000UL;
    }

    // True if this app has expired by lifetime
    bool isExpired() const {
        if (lifetime == 0 || addedMs == 0) return false;
        return (millis() - addedMs) > (lifetime * 1000UL);
    }
};

// ── App loop entry ────────────────────────────────────────────────────────────
struct DpxApp {
    String        name;
    DpxCustomApp  data;
    bool          isNative = false; // Time, Date, WLED etc.
    bool          muted    = false; // if true, skipped in rotation
};

// ── OSC Listener Registry ─────────────────────────────────────────────────────
struct DpxOscListener {
    String path;
    String channel;
    String label;
};

// ── Global app state ──────────────────────────────────────────────────────────
// Effect ID assigned by strip.addEffect() in DpxMatrix::setup().
static uint8_t _dpxEffectId = 255;

// Switch the main segment to dpx Matrix effect if not already active.
static inline void dpxActivateEffect() {
    if (_dpxEffectId != 255 && strip.getMainSegment().mode != _dpxEffectId) {
        strip.getMainSegment().setMode(_dpxEffectId);
        stateUpdated(CALL_MODE_DIRECT_CHANGE);
    }
}

static std::vector<DpxApp>            dpxApps;         // ordered app loop
static std::map<String, DpxCustomApp> dpxCustom;       // named custom apps
static std::set<String>               dpxHiddenApps;   // removed from rotation (incl. natives)
static std::vector<DpxOscListener>    dpxOscListeners;

static int     dpxCurrentApp   = 0;               // index into dpxApps
static bool    dpxAutoTrans    = true;             // auto-advance enabled
static unsigned long dpxAppStartMs = 0;            // when current app was shown

// Active scroll state — one per display
static DpxScrollState dpxScroll;

// ── Parse a color from JSON value (string "#RRGGBB" or array [r,g,b]) ─────────
static uint32_t dpxParseColor(JsonVariant v, uint32_t def = 0xFFFFFF) {
    if (v.is<JsonArray>()) {
        JsonArray a = v.as<JsonArray>();
        if (a.size() >= 3) {
            return ((uint32_t)(a[0].as<uint8_t>()) << 16)
                 | ((uint32_t)(a[1].as<uint8_t>()) << 8)
                 |  (uint32_t)(a[2].as<uint8_t>());
        }
    }
    if (v.is<const char*>()) {
        const char* s = v.as<const char*>();
        if (s && s[0] == '#') {
            return (uint32_t)strtol(s + 1, nullptr, 16);
        }
    }
    return def;
}

// ── Parse a CustomApp from JSON body ─────────────────────────────────────────
static DpxCustomApp dpxParseApp(const char* json) {
    DpxCustomApp app;
    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, json)) return app;

    app.valid = true;

    if (doc.containsKey("text"))        app.text        = doc["text"].as<String>();
    if (doc.containsKey("color"))       app.color       = dpxParseColor(doc["color"]);
    if (doc.containsKey("background"))  app.background  = dpxParseColor(doc["background"], 0x000000);
    if (doc.containsKey("rainbow"))     app.rainbow     = doc["rainbow"].as<bool>();
    if (doc.containsKey("center"))      app.center      = doc["center"].as<bool>();
    if (doc.containsKey("noScroll"))    app.noScroll    = doc["noScroll"].as<bool>();
    if (doc.containsKey("scrollSpeed")) app.scrollSpeed = doc["scrollSpeed"].as<int>();
    if (doc.containsKey("topText"))     app.topText     = doc["topText"].as<bool>();
    if (doc.containsKey("duration"))    app.duration    = doc["duration"].as<int>();
    if (doc.containsKey("repeat"))      app.repeat      = doc["repeat"].as<int>();
    if (doc.containsKey("progress"))    app.progress    = doc["progress"].as<int>();
    if (doc.containsKey("progressC"))   app.pColor      = dpxParseColor(doc["progressC"], 0x00FF00);
    if (doc.containsKey("progressBC"))  app.pbColor     = dpxParseColor(doc["progressBC"], 0x1a1a1a);
    if (doc.containsKey("lifetime"))    app.lifetime    = doc["lifetime"].as<unsigned long>();
    if (doc.containsKey("save"))        app.save        = doc["save"].as<bool>();
    if (doc.containsKey("overlay"))  { app.overlay = doc["overlay"].as<String>(); app.overlay.toLowerCase(); }
    if (doc.containsKey("icon"))       app.icon     = doc["icon"].as<String>();
    if (doc.containsKey("pushIcon"))   app.pushIcon = doc["pushIcon"].as<int>();
    app.addedMs = millis();

    // Draw commands
    if (doc.containsKey("draw") && doc["draw"].is<JsonArray>()) {
        for (JsonObject o : doc["draw"].as<JsonArray>()) {
            DpxDrawCmd cmd;
            if (o.containsKey("dp")) { cmd.cmd = "dp"; auto a = o["dp"].as<JsonArray>(); cmd.n[0]=a[0];cmd.n[1]=a[1]; cmd.color=dpxParseColor(a[2]); app.drawCmds.push_back(cmd); }
            else if (o.containsKey("df")) { cmd.cmd = "df"; auto a = o["df"].as<JsonArray>(); cmd.n[0]=a[0];cmd.n[1]=a[1];cmd.n[2]=a[2];cmd.n[3]=a[3]; cmd.color=dpxParseColor(a[4]); app.drawCmds.push_back(cmd); }
            else if (o.containsKey("dr")) { cmd.cmd = "dr"; auto a = o["dr"].as<JsonArray>(); cmd.n[0]=a[0];cmd.n[1]=a[1];cmd.n[2]=a[2];cmd.n[3]=a[3]; cmd.color=dpxParseColor(a[4]); app.drawCmds.push_back(cmd); }
            else if (o.containsKey("dl")) { cmd.cmd = "dl"; auto a = o["dl"].as<JsonArray>(); cmd.n[0]=a[0];cmd.n[1]=a[1];cmd.n[2]=a[2];cmd.n[3]=a[3]; cmd.color=dpxParseColor(a[4]); app.drawCmds.push_back(cmd); }
            else if (o.containsKey("dt")) { cmd.cmd = "dt"; auto a = o["dt"].as<JsonArray>(); cmd.n[0]=a[0];cmd.n[1]=a[1]; cmd.s=a[2].as<String>(); cmd.color=dpxParseColor(a[3]); app.drawCmds.push_back(cmd); }
        }
    }

    return app;
}

// ── Render draw commands ──────────────────────────────────────────────────────
static void dpxExecDraw(const std::vector<DpxDrawCmd>& cmds) {
    for (const auto& c : cmds) {
        if (c.cmd == "dp") { dpxSetPixel(c.n[0], c.n[1], c.color); }
        else if (c.cmd == "df") { dpxFillRect(c.n[0], c.n[1], c.n[2], c.n[3], c.color); }
        else if (c.cmd == "dr") { dpxDrawRect(c.n[0], c.n[1], c.n[2], c.n[3], c.color); }
        else if (c.cmd == "dl") {
            // Simple line using Bresenham
            int x0=c.n[0],y0=c.n[1],x1=c.n[2],y1=c.n[3];
            int dx=abs(x1-x0), dy=abs(y1-y0), sx=x0<x1?1:-1, sy=y0<y1?1:-1, err=dx-dy;
            while(true){ dpxSetPixel(x0,y0,c.color); if(x0==x1&&y0==y1)break; int e2=2*err; if(e2>-dy){err-=dy;x0+=sx;} if(e2<dx){err+=dx;y0+=sy;} }
        }
        else if (c.cmd == "dt") { dpxRenderText(c.n[0], c.n[1], c.s.c_str(), c.color); }
    }
}

// ── Native app rendering ──────────────────────────────────────────────────────
static void dpxRenderNativeTime() {
    // localTime is WLED's timezone-adjusted Unix epoch. Respect WLED's useAMPM setting.
    // Wrong time → set timezone in WLED: Config → Time & NTP → select timezone.
    char buf[9];
    if (localTime < 100000UL) {
        strncpy(buf, "--:--", sizeof(buf));  // NTP not synced yet
    } else if (useAMPM) {
        // 12h format with AM/PM indicator
        uint8_t h = hourFormat12(localTime);
        snprintf(buf, sizeof(buf), "%d:%02d%s", h, minute(localTime),
                 (hour(localTime) < 12) ? "a" : "p");
    } else {
        snprintf(buf, sizeof(buf), "%02d:%02d", hour(localTime), minute(localTime));
    }
    int w = dpxTextPixelWidth(buf);
    int x = (DPX_MATRIX_W - w) / 2;
    dpxRenderText(x, DPX_FONT_BASELINE, buf, 0xFFFFFF);
}

static void dpxRenderNativeDate() {
    char buf[9];
    if (localTime < 100000UL) {
        strncpy(buf, "--.--.--", sizeof(buf));
    } else {
        snprintf(buf, sizeof(buf), "%02d.%02d.%02d",
                 day(localTime), month(localTime), year(localTime) % 100);
    }
    int w = dpxTextPixelWidth(buf);
    int x = (DPX_MATRIX_W - w) / 2;
    dpxRenderText(x, DPX_FONT_BASELINE, buf, 0x8888FF);
}

// ── Render one custom app frame (static or scroll) ───────────────────────────
// Returns false if app has completed its scroll cycles.
static bool dpxRenderApp(DpxCustomApp& app) {
    if (!app.valid) return false;

    // Background fill
    dpxFillRect(0, 0, DPX_MATRIX_W, DPX_MATRIX_H, app.background);

    // Draw commands (behind text)
    dpxExecDraw(app.drawCmds);

    // Progress bar (rows 6-7)
    if (app.progress >= 0) {
        dpxDrawProgressBar(app.progress, app.pColor, app.pbColor);
    }

    int textY = app.topText ? (DPX_FONT_BASELINE - 1) : DPX_FONT_BASELINE; // proper AwtrixFont baselines
    int textW = dpxTextPixelWidth(app.text.c_str());

    if (app.noScroll || textW <= DPX_MATRIX_W) {
        // Static: center or left-align
        int x = 0;
        if (app.center && textW < DPX_MATRIX_W) x = (DPX_MATRIX_W - textW) / 2;
        dpxRenderText(x, textY, app.text.c_str(), app.color, app.rainbow);
        return true; // static apps never "complete"
    }

    // Scrolling
    if (!dpxScroll.active || dpxScroll.text != app.text) {
        dpxScroll.start(app.text, app.color, app.rainbow, textY, app.scrollSpeed, app.repeat);
    }
    bool done = dpxScroll.tick();
    dpxScroll.render();
    return !done;
}

// ── App loop management ───────────────────────────────────────────────────────

// Rebuild dpxApps from dpxCustom. Any app in dpxHiddenApps is skipped.
static void dpxRebuildLoop() {
    std::vector<DpxApp> newList;
    // Native apps — included unless user deleted them from the rotation
    const char* natives[] = {"Time", "Date", "WLED"};
    for (auto n : natives) {
        if (dpxHiddenApps.find(String(n)) == dpxHiddenApps.end()) {
            DpxApp a; a.name = n; a.isNative = true;
            newList.push_back(a);
        }
    }
    // Custom apps in insertion order
    for (auto& kv : dpxCustom) {
        if (kv.second.valid) {
            DpxApp a;
            a.name = kv.first;
            a.data = kv.second;
            a.isNative = false;
            newList.push_back(a);
        }
    }
    dpxApps = newList;
    if (dpxCurrentApp >= (int)dpxApps.size()) dpxCurrentApp = 0;
}

// Add or update a custom app. Empty body = remove from rotation.
// Native apps (Time, Date, WLED) are hidden (not deleted); custom apps are erased.
static void dpxSetCustomApp(const String& name, const char* json) {
    if (!json || strlen(json) <= 2) {
        // Remove from rotation — custom erased, natives hidden
        dpxCustom.erase(name);
        dpxHiddenApps.insert(name);
        LittleFS.remove("/CUSTOMAPPS/" + name + ".json");
    } else {
        DpxCustomApp app = dpxParseApp(json);
        if (app.valid) {
            dpxCustom[name] = app;
            dpxHiddenApps.erase(name);
            dpxActivateEffect();  // incoming app — switch display to dpx Matrix
            if (app.save) {
                LittleFS.mkdir("/CUSTOMAPPS");
                File f = LittleFS.open("/CUSTOMAPPS/" + name + ".json", "w");
                if (f) { f.print(json); f.close(); }
            }
        }
    }
    dpxRebuildLoop();
}

// Advance to next unmuted app
static void dpxNextApp() {
    if (dpxApps.empty()) return;
    int start = dpxCurrentApp;
    do {
        dpxCurrentApp = (dpxCurrentApp + 1) % dpxApps.size();
    } while (dpxApps[dpxCurrentApp].muted && dpxCurrentApp != start);
    dpxAppStartMs = millis();
    dpxScroll.stop();
}

// Go to previous unmuted app
static void dpxPrevApp() {
    if (dpxApps.empty()) return;
    int start = dpxCurrentApp;
    do {
        dpxCurrentApp = (dpxCurrentApp + (int)dpxApps.size() - 1) % dpxApps.size();
    } while (dpxApps[dpxCurrentApp].muted && dpxCurrentApp != start);
    dpxAppStartMs = millis();
    dpxScroll.stop();
}

// Switch to a named app; JSON body: {"name":"AppName"}
static bool dpxSwitchToApp(const char* json) {
    DynamicJsonDocument doc(128);
    if (deserializeJson(doc, json)) return false;
    String name = doc["name"].as<String>();
    for (int i = 0; i < (int)dpxApps.size(); i++) {
        if (dpxApps[i].name == name) {
            dpxCurrentApp = i;
            dpxAppStartMs = millis();
            dpxScroll.stop();
            dpxActivateEffect();  // switch display to dpx Matrix
            return true;
        }
    }
    return false;
}

// App loop JSON: {"AppName": index, ...}
static String dpxGetLoopJson() {
    DynamicJsonDocument doc(1024);
    for (int i = 0; i < (int)dpxApps.size(); i++) {
        doc[dpxApps[i].name] = i;
    }
    String s; serializeJson(doc, s); return s;
}

// Apps-with-icon JSON array (SPEC.md §5 /api/apps)
static String dpxGetAppsJson() {
    DynamicJsonDocument doc(2048);
    JsonArray arr = doc.to<JsonArray>();
    for (auto& a : dpxApps) {
        JsonObject o = arr.createNestedObject();
        o["name"]   = a.name;
        o["native"] = a.isNative;
        o["muted"]  = a.muted;
    }
    String s; serializeJson(doc, s); return s;
}

// Mute or unmute an app by name. Returns false if name not found.
static bool dpxMuteApp(const String& name, bool mute) {
    for (auto& a : dpxApps) {
        if (a.name == name) { a.muted = mute; return true; }
    }
    return false;
}

// Return JSON of a specific custom app's current state
static String dpxGetCustomAppJson(const String& name) {
    if (dpxCustom.find(name) == dpxCustom.end()) return F("{}");
    const DpxCustomApp& a = dpxCustom[name];
    DynamicJsonDocument doc(512);
    doc["text"]        = a.text;
    doc["color"]       = a.color;
    doc["background"]  = a.background;
    doc["rainbow"]     = a.rainbow;
    doc["center"]      = a.center;
    doc["noScroll"]    = a.noScroll;
    doc["scrollSpeed"] = a.scrollSpeed;
    doc["duration"]    = a.duration;
    doc["progress"]    = a.progress;
    doc["icon"]        = a.icon;
    doc["pushIcon"]    = a.pushIcon;
    doc["overlay"]     = a.overlay;
    String s; serializeJson(doc, s); return s;
}

// ── App loop tick (call from loop()) ─────────────────────────────────────────
// Advances app pointer when current app's duration expires.
static void dpxAppLoopTick() {
    if (dpxApps.empty()) return;

    // Prune expired lifetime apps
    for (auto it = dpxCustom.begin(); it != dpxCustom.end(); ) {
        if (it->second.isExpired()) { it = dpxCustom.erase(it); dpxRebuildLoop(); }
        else ++it;
    }

    if (!dpxAutoTrans) return;
    if (dpxApps.empty()) return;

    unsigned long now = millis();
    unsigned long dur = dpxApps[dpxCurrentApp].isNative
        ? (unsigned long)DPX_ATIME * 1000UL
        : dpxApps[dpxCurrentApp].data.durationMs();

    if (now - dpxAppStartMs >= dur) {
        dpxNextApp();  // dpxNextApp already skips muted apps
    }
}

// ── Render the current app frame ─────────────────────────────────────────────
// Called from handleOverlayDraw().
static void dpxRenderCurrentApp() {
    if (dpxApps.empty()) {
        dpxClear();
        dpxRenderText(0, DPX_FONT_BASELINE, "NO APP", 0x444444);
        return;
    }

    DpxApp& app = dpxApps[dpxCurrentApp];
    if (app.isNative) {
        if (app.name == "WLED") {
            // Passthrough: switch away from dpx Matrix so WLED effects show.
            // Switches to DNA Spiral (182); user can change via WLED UI.
            if (_dpxEffectId != 255 && strip.getMainSegment().mode == _dpxEffectId) {
                strip.getMainSegment().setMode(FX_MODE_2DDNASPIRAL);
                stateUpdated(CALL_MODE_DIRECT_CHANGE);
            }
            return;
        }
        dpxClear();
        if (app.name == "Time") dpxRenderNativeTime();
        else if (app.name == "Date") dpxRenderNativeDate();
    } else {
        dpxRenderApp(app.data);
    }
}
