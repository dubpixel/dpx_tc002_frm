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
//   AP      dpx-tc002 / dubpixel1, behav=1 (always open when disconnected)
//   mDNS    dpx-tc002
//   LED     GPIO 32, 256× WS2812B GRB, 42fps, 8.5W limit
//   2D      32×8 panel, non-serpentine (change via WLED UI if needed)
//   Buttons GPIO 26/14/27, push-button type
//   Trans   0ms (instant, better for matrix text)
// ================================================================================

#pragma once

static void dpxFirstBoot() {
    if (LittleFS.exists(F("/cfg.json"))) return;

    DEBUG_PRINTLN(F("DpxMatrix: first boot — writing /cfg.json"));

    DynamicJsonDocument doc(2048);

    // Identity
    JsonObject id   = doc.createNestedObject("id");
    id["mdns"]      = "dpx-tc002";
    id["name"]      = "dpx_tc002";
    id["inv"]       = "TC001";
    id["sui"]       = false;

    // Access point
    JsonObject ap   = doc.createNestedObject("ap");
    ap["ssid"]      = "dpx-tc002";
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
    panel["s"]  = false;  // serpentine — set true in WLED UI if needed
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

    // Light — instant transitions
    doc["light"]["tr"]["dur"] = 0;
    doc["light"]["tr"]["rpc"] = 5;

    // Defaults
    doc["def"]["ps"]  = 0;
    doc["def"]["on"]  = true;
    doc["def"]["bri"] = 128;

    doc["ota"]["lock"] = false;

    File f = LittleFS.open(F("/cfg.json"), "w");
    if (!f) { DEBUG_PRINTLN(F("DpxMatrix: failed to open /cfg.json")); return; }
    serializeJson(doc, f);
    f.close();
    DEBUG_PRINTLN(F("DpxMatrix: /cfg.json written — takes effect on next boot"));
}

