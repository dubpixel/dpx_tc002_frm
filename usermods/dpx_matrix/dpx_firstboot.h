// ================================================================================
// dpx_firstboot.h — First-Boot Config Injection
// ================================================================================
// Original work — dubpixel / dpx_tc002 (EUPL v1.2)
// ================================================================================
// PROJECT: dpx_tc002_frm
// ================================================================================
//
// Writes /cfg.json on first boot (file absent) so the device is usable
// out of the box without the WLED setup wizard.
//
// Defaults written:
//   AP      dpx-tc002-XXXXXX / dubpixel1, behav=1 (always open when disconnected)
//           XXXXXX = last 3 bytes of the chip's MAC, lowercase hex — so two
//           unclaimed devices on the same network never collide (same
//           convention WLED itself uses for its own default "wled-XXXXXX").
//   mDNS    dpx-tc002-XXXXXX (same suffix as the AP SSID — same string, so
//           "what WiFi network did I just join" tells you the mDNS name too)
//   LED     GPIO 32, 256× WS2812B GRB, 42fps, 8.5W limit
//   2D      32×8 panel, non-serpentine (change via WLED UI if needed)
//   Buttons GPIO 26/14/27, push-button type
//   Trans   0ms (instant, better for matrix text)
//   MQTT    mb.dubpixel.tv:1883, no auth — so a fresh device is claimable
//           out of the box (friendster's claim flow is MQTT-only, #82)
// ================================================================================

#pragma once

// ESP32 LittleFS does not auto-create parent directories when opening a
// nested path for write — /upload silently fails into a subfolder that
// doesn't exist yet. Runs every boot (mkdir on an existing dir is a no-op).
static void dpxEnsureDirs() {
    if (!LittleFS.exists("/ICONS"))    LittleFS.mkdir("/ICONS");
    if (!LittleFS.exists("/MELODIES")) LittleFS.mkdir("/MELODIES");
}

static void dpxFirstBoot() {
    if (LittleFS.exists(F("/cfg.json"))) return;

    DEBUG_PRINTLN(F("DpxMatrix: first boot — writing /cfg.json"));

    DynamicJsonDocument doc(2048);

    // Unique per-device suffix (last 3 MAC bytes, lowercase hex) — same
    // technique WLED itself falls back to for its own default hostname
    // (wled.cpp: escapedMac.c_str()+6). Computed directly from
    // WiFi.macAddress() here rather than relying on WLED's escapedMac
    // global — that's populated earlier in WLED::begin() so it'd likely
    // work too, but this way dpxFirstBoot() has no ordering dependency on
    // WLED core internals.
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    mac.toLowerCase();
    String macSuffix = mac.substring(6); // last 3 bytes = 6 hex chars
    String dpxName = "dpx-tc002-" + macSuffix;

    // Identity
    JsonObject id   = doc.createNestedObject("id");
    id["mdns"]      = dpxName;
    id["name"]      = "dpx_tc002";
    id["inv"]       = "TC001";
    id["sui"]       = false;

    // Access point — same name as mDNS (GH: "it really needs to assign
    // itself a dynamic mac thing... so duplicates don't break dns" — using
    // one shared name for both means the AP you just joined tells you the
    // mDNS name too, no separate lookup needed).
    JsonObject ap   = doc.createNestedObject("ap");
    ap["ssid"]      = dpxName;
    ap["psk"]       = "dubpixel1";
    ap["chan"]       = 6;
    ap["hide"]      = 0;
    ap["behav"]     = 1;  // AP_BEHAVIOR_NO_CONN — always open when disconnected

    // WiFi
    doc["wifi"]["sleep"] = false;

    // Hardware — LED
    JsonObject hw   = doc.createNestedObject("hw");
    JsonObject led  = hw.createNestedObject("led");
    led["total"]    = 256;
    led["maxpwr"]   = 8500;
    led["fps"]      = 42;

    JsonObject ins  = led["ins"].createNestedObject();
    ins["start"]    = 0;
    ins["len"]      = 256;
    ins["pin"][0]   = 32;
    ins["order"]    = 0;   // GRB
    ins["rev"]      = false;
    ins["skip"]     = 0;
    ins["type"]     = 22;  // TYPE_WS2812_RGB
    ins["ref"]      = false;
    ins["rgbwm"]    = 255;
    ins["freq"]     = 0;
    ins["ledma"]    = 55;
    ins["drv"]      = 0;

    // 2D matrix — 32×8, non-serpentine (user can toggle serpentine in WLED UI)
    JsonObject matrix  = led.createNestedObject("matrix");
    matrix["mpc"]      = 1;
    JsonObject panel   = matrix["panels"].createNestedObject();
    panel["b"]  = false;  // top start
    panel["r"]  = false;  // left start
    panel["v"]  = false;  // horizontal
    panel["s"]  = true;   // TC001 is serpentine-wired
    panel["x"]  = 0;
    panel["y"]  = 0;
    panel["h"]  = 8;
    panel["w"]  = 32;

    // Hardware — buttons
    JsonObject btn  = hw.createNestedObject("btn");
    btn["max"]      = 3;
    btn["pull"]     = true;
    JsonObject b0 = btn["ins"].createNestedObject(); b0["type"]=2; b0["pin"][0]=26; b0["macros"][0]=0; b0["macros"][1]=0; b0["macros"][2]=0;
    JsonObject b1 = btn["ins"].createNestedObject(); b1["type"]=2; b1["pin"][0]=14; b1["macros"][0]=0; b1["macros"][1]=0; b1["macros"][2]=0;
    JsonObject b2 = btn["ins"].createNestedObject(); b2["type"]=2; b2["pin"][0]=27; b2["macros"][0]=0; b2["macros"][1]=0; b2["macros"][2]=0;

    // Light — keep WLED's default transition (750ms) so power fade works
    // Do NOT set dur=0 here; that kills the power-on/off fade animation.
    // Users can reduce transition in WLED → LED Preferences if desired.

    // Defaults — DNA Spiral as startup effect (FX_MODE_2DDNASPIRAL = 182)
    doc["def"]["ps"]  = 0;
    doc["def"]["on"]  = true;
    doc["def"]["bri"] = 128;
    // def.fx intentionally omitted — dpxActivateEffect() in connected() sets
    // the correct dynamic effect ID after strip.addEffect() has run.

    doc["ota"]["lock"] = false;

    // Enable NTP — time sync is needed for Time/Date apps
    doc["if"]["ntp"]["en"]     = true;
    doc["if"]["ntp"]["host"]   = "pool.ntp.org";
    doc["if"]["ntp"]["tz"]     = 0;   // UTC; user sets timezone in WLED → Config → Time

    // GH #82 — a fresh device was previously unclaimable: friendster's claim
    // flow is MQTT-only (mqtt_client.publish(f"{prefix}/dpx/pair", ...), no
    // HTTP fallback), and nothing wrote MQTT config before this. DNS name,
    // not a raw droplet IP, so infra can move without re-flashing every
    // device in the field. Plain 1883/no-auth matches what's already proven
    // live on real hardware (WSS+TLS on 443 is documented as the eventual
    // target in dpx_tc002_server.md but isn't set up yet — upgrade later,
    // not blocking this fix). cid/device-topic left unset: WLED's own core
    // derives both from the MAC (wled.cpp) identically to what's already
    // observed live — no need to duplicate that logic here.
    doc["if"]["mqtt"]["en"]     = true;
    doc["if"]["mqtt"]["broker"] = "mb.dubpixel.tv";
    doc["if"]["mqtt"]["port"]   = 1883;

    File f = LittleFS.open(F("/cfg.json"), "w");
    if (!f) { DEBUG_PRINTLN(F("DpxMatrix: failed to open /cfg.json")); return; }
    serializeJson(doc, f);
    f.close();

    // Force an IMMEDIATE, SYNCHRONOUS reboot — setting doReboot=true and
    // waiting for loop() is NOT enough. WLED::setup() calls
    // UsermodManager::setup() (where this function runs) at wled.cpp:501,
    // then unconditionally hits `if (needsCfgSave) serializeConfigToFS();`
    // three lines later at wled.cpp:504 — and needsCfgSave was already
    // latched true back at wled.cpp:484 (deserializeConfigFromFS(), before
    // cfg.json existed). That re-serializes the still-100%-stock in-RAM
    // config back over the file we just wrote, synchronously, before
    // loop() (and doReboot) ever gets a chance to run. A prior fix here
    // used doReboot=true and it was NOT sufficient — confirmed live: file
    // got clobbered back to stock every time regardless. Calling reset()
    // here instead halts execution inside this call (ESP.restart() doesn't
    // return), so WLED::setup() never reaches line 504 at all.
    DEBUG_PRINTLN(F("DpxMatrix: /cfg.json written — rebooting now to load it"));
    WLED::instance().reset();
}

