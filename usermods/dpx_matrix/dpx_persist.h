// ================================================================================
// dpx_persist.h — Persistent Settings (dev.json) + Runtime Globals
// ================================================================================
// Original work — dubpixel / dpx_tc002 (EUPL v1.2)
// ================================================================================
// PROJECT: dpx_tc002_frm
// ================================================================================
//
// File: dpx_persist.h
// Purpose: Runtime configuration variables loaded from /dev.json on LittleFS.
//          Also provides load/save helpers. Included once via dpx_matrix.cpp.
//
// dev.json keys:
//   hostname, temp_offset, hum_offset, tc_dwell, tc_hold, tc_show_frames,
//   min_brightness, max_brightness, ldr_factor, ldr_gamma, rotate_screen,
//   mirror_screen, sensor_reading, matrix, dfplayer, bootsound, web_port,
//   background_effect, ha_prefix, stats_interval
//
// ================================================================================

#pragma once
// ArduinoJson is pulled in via wled.h — do not include it directly here.
#include <LittleFS.h>

// ── Runtime variables (set at boot from dev.json, some apply immediately) ─────
static float    DPX_TEMP_OFFSET    = -9.0f;
static float    DPX_HUM_OFFSET     =  0.0f;
static uint32_t DPX_TC_DWELL_MS    = 2000;   // ms before TC app auto-dismiss
static bool     DPX_TC_HOLD        = false;   // if true, TC never auto-dismisses
static bool     DPX_TC_SHOW_FRAMES = false;   // false=HH:MM:SS+bar, true=MM:SS.FF
static bool     DPX_TC_STOP_BEEP   = false;   // 2x beep when TC signal stops
static bool     DPX_TC_MUTE        = false;   // if true, TC signal is ignored entirely
static int      DPX_MIN_BRI        = 2;
static int      DPX_MAX_BRI        = 180;
static float    DPX_LDR_FACTOR     = 1.0f;
static float    DPX_LDR_GAMMA      = 3.0f;
static bool     DPX_ROTATE_SCREEN  = false;
static bool     DPX_MIRROR_SCREEN  = false;
static bool     DPX_SENSOR_READING = true;
static bool     DPX_SOUND_ENABLED  = true;    // buzzer on/off
static int      DPX_ATIME          = 7;       // app display seconds
static bool     DPX_ATRANS         = true;    // auto-advance apps
static int      DPX_SSPEED         = 100;     // global scroll speed %
static bool     DPX_UPPERCASE      = true;    // force uppercase text
static bool     dpxEnabled         = true;    // false = pass-through to WLED effects
static String   DPX_TIMEZONE;                 // POSIX TZ string (e.g. PST8PDT,...)
// Native app visibility (toggled via POST /api/settings TIM/DAT keys)
static bool     DPX_SHOW_TIME      = true;
static bool     DPX_SHOW_DATE      = false;

// ── Load dev.json from LittleFS ───────────────────────────────────────────────
static void dpxLoadDev() {
    if (!LittleFS.exists("/dev.json")) return;
    File f = LittleFS.open("/dev.json", "r");
    if (!f) return;
    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, f)) { f.close(); return; }
    f.close();

    if (doc.containsKey("temp_offset"))    DPX_TEMP_OFFSET    = doc["temp_offset"].as<float>();
    if (doc.containsKey("hum_offset"))     DPX_HUM_OFFSET     = doc["hum_offset"].as<float>();
    if (doc.containsKey("tc_dwell"))       DPX_TC_DWELL_MS    = (uint32_t)(doc["tc_dwell"].as<int>() * 1000);
    if (doc.containsKey("tc_hold"))        DPX_TC_HOLD        = doc["tc_hold"].as<bool>();
    if (doc.containsKey("tc_show_frames")) DPX_TC_SHOW_FRAMES = doc["tc_show_frames"].as<bool>();
    if (doc.containsKey("tc_stop_beep"))   DPX_TC_STOP_BEEP   = doc["tc_stop_beep"].as<bool>();
    if (doc.containsKey("min_brightness")) DPX_MIN_BRI        = doc["min_brightness"].as<int>();
    if (doc.containsKey("max_brightness")) DPX_MAX_BRI        = doc["max_brightness"].as<int>();
    if (doc.containsKey("ldr_factor"))     DPX_LDR_FACTOR     = doc["ldr_factor"].as<float>();
    if (doc.containsKey("ldr_gamma"))      DPX_LDR_GAMMA      = doc["ldr_gamma"].as<float>();
    if (doc.containsKey("rotate_screen"))  DPX_ROTATE_SCREEN  = doc["rotate_screen"].as<bool>();
    if (doc.containsKey("mirror_screen"))  DPX_MIRROR_SCREEN  = doc["mirror_screen"].as<bool>();
    if (doc.containsKey("sensor_reading")) DPX_SENSOR_READING = doc["sensor_reading"].as<bool>();
    if (doc.containsKey("sound_enabled"))  DPX_SOUND_ENABLED  = doc["sound_enabled"].as<bool>();
    // Native app visibility — flags only; dpxHiddenApps sync happens in setup() after dpx_apps.h is loaded
    if (doc.containsKey("show_time")) DPX_SHOW_TIME = doc["show_time"].as<bool>();
    if (doc.containsKey("show_date")) DPX_SHOW_DATE = doc["show_date"].as<bool>();
    if (doc.containsKey("timezone_posix")) {
        DPX_TIMEZONE = doc["timezone_posix"].as<String>();
        setenv("TZ", DPX_TIMEZONE.c_str(), 1);
        tzset();
    }
}

// ── Merge a JSON object into dev.json and apply immediately ───────────────────
// Returns false on parse error.
static bool dpxMergeDev(const char* json) {
    DynamicJsonDocument incoming(512);
    if (deserializeJson(incoming, json)) return false;

    DynamicJsonDocument merged(1024);
    if (LittleFS.exists("/dev.json")) {
        File f = LittleFS.open("/dev.json", "r");
        if (f) { deserializeJson(merged, f); f.close(); }
    }
    for (JsonPair kv : incoming.as<JsonObject>())
        merged[kv.key()] = kv.value();

    File fw = LittleFS.open("/dev.json", "w");
    if (!fw) return false;
    serializeJson(merged, fw);
    fw.close();

    // Apply changed values immediately
    if (incoming.containsKey("temp_offset"))    DPX_TEMP_OFFSET    = incoming["temp_offset"].as<float>();
    if (incoming.containsKey("hum_offset"))     DPX_HUM_OFFSET     = incoming["hum_offset"].as<float>();
    if (incoming.containsKey("tc_dwell"))       DPX_TC_DWELL_MS    = (uint32_t)(incoming["tc_dwell"].as<int>() * 1000);
    if (incoming.containsKey("tc_hold"))        DPX_TC_HOLD        = incoming["tc_hold"].as<bool>();
    if (incoming.containsKey("tc_show_frames")) DPX_TC_SHOW_FRAMES = incoming["tc_show_frames"].as<bool>();
    if (incoming.containsKey("tc_stop_beep"))   DPX_TC_STOP_BEEP   = incoming["tc_stop_beep"].as<bool>();
    if (incoming.containsKey("tc_mute"))        DPX_TC_MUTE        = incoming["tc_mute"].as<bool>();
    // show_time/show_date: flags only here; callers (dpxApplySettings) handle dpxHiddenApps sync
    if (incoming.containsKey("show_time")) DPX_SHOW_TIME = incoming["show_time"].as<bool>();
    if (incoming.containsKey("show_date")) DPX_SHOW_DATE = incoming["show_date"].as<bool>();
    if (incoming.containsKey("min_brightness")) DPX_MIN_BRI        = incoming["min_brightness"].as<int>();
    if (incoming.containsKey("max_brightness")) DPX_MAX_BRI        = incoming["max_brightness"].as<int>();
    if (incoming.containsKey("ldr_factor"))     DPX_LDR_FACTOR     = incoming["ldr_factor"].as<float>();
    if (incoming.containsKey("ldr_gamma"))      DPX_LDR_GAMMA      = incoming["ldr_gamma"].as<float>();
    if (incoming.containsKey("timezone_posix")) {
        DPX_TIMEZONE = incoming["timezone_posix"].as<String>();
        setenv("TZ", DPX_TIMEZONE.c_str(), 1);
        tzset();
    }

    return true;
}

// ── Read dev.json as a String ─────────────────────────────────────────────────
static String dpxReadDevJson() {
    if (!LittleFS.exists("/dev.json")) return F("{}");
    File f = LittleFS.open("/dev.json", "r");
    if (!f) return F("{}");
    String s = f.readString();
    f.close();
    return s;
}
