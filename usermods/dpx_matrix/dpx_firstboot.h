// ================================================================================
// dpx_firstboot.h — First-Boot Config Injection
// ================================================================================
// Original work — dubpixel / dpx_tc002 (MIT)
// ================================================================================
// PROJECT: dpx_tc002_frm
// ================================================================================
//
// File: dpx_firstboot.h
// Purpose: On first boot (no /cfg.json on LittleFS), write a complete default
//          configuration for the Ulanzi TC001 hardware so the device is usable
//          out of the box without needing the WLED setup wizard.
//
// Pre-configured defaults:
//   - LED:    GPIO 32, 256 WS2812B, GRB, no 2D yet (set via WLED UI)
//   - AP:     behav=1 (always open when disconnected, no timeout)
//   - AP SSID: dpx-tc002  password: dubpixel1
//   - mDNS:   dpx-tc002
//   - Name:   dpx_tc002
// ================================================================================

#pragma once

static void dpxFirstBoot() {
    // Only run if /cfg.json does not exist (genuine first boot or wiped flash)
    if (LittleFS.exists(F("/cfg.json"))) return;

    DEBUG_PRINTLN(F("DpxMatrix: first boot — writing default cfg.json"));

    // Write a minimal but complete cfg.json that WLED will load at next boot.
    // We use a raw string to keep it readable and avoid ArduinoJson overhead.
    // Field reference: wled00/cfg.cpp deserializeConfig()
    //
    // Key fields set here:
    //   id.mdns / id.name  — hostname and device name
    //   ap.ssid / ap.psk   — AP credentials (behav 1 = always open when disco)
    //   hw.led.ins         — GPIO 32, 256 WS2812B GRB
    //   hw.btn.ins         — left=GPIO26 short-press (type 2 = push button)
    //   light.tr.dur       — transition duration 0 (instant, better for matrix)

    File f = LittleFS.open(F("/cfg.json"), "w");
    if (!f) {
        DEBUG_PRINTLN(F("DpxMatrix: failed to open /cfg.json for writing"));
        return;
    }

    f.print(F(
        "{"
        "\"rev\":[1,0],"
        "\"id\":{"
            "\"mdns\":\"dpx-tc002\","
            "\"name\":\"dpx_tc002\","
            "\"inv\":\"TC001\","
            "\"sui\":false"
        "},"
        "\"ap\":{"
            "\"ssid\":\"dpx-tc002\","
            "\"psk\":\"dubpixel1\","
            "\"chan\":6,"
            "\"hide\":0,"
            "\"behav\":1"
        "},"
        "\"wifi\":{\"sleep\":false},"
        "\"hw\":{"
            "\"led\":{"
                "\"total\":256,"
                "\"maxpwr\":8500,"
                "\"fps\":42,"
                "\"ins\":[{"
                    "\"start\":0,"
                    "\"len\":256,"
                    "\"pin\":[32],"
                    "\"order\":0,"
                    "\"rev\":false,"
                    "\"skip\":0,"
                    "\"type\":22,"
                    "\"ref\":false,"
                    "\"rgbwm\":255,"
                    "\"freq\":0,"
                    "\"ledma\":55,"
                    "\"drv\":0"
                "}]"
            "},"
            "\"btn\":{"
                "\"max\":3,"
                "\"pull\":true,"
                "\"ins\":["
                    "{\"type\":2,\"pin\":[26],\"macros\":[0,0,0]},"
                    "{\"type\":2,\"pin\":[14],\"macros\":[0,0,0]},"
                    "{\"type\":2,\"pin\":[27],\"macros\":[0,0,0]}"
                "]"
            "}"
        "},"
        "\"light\":{"
            "\"tr\":{\"dur\":0,\"rpc\":5,\"hrp\":false}"
        "},"
        "\"def\":{\"ps\":0,\"on\":true,\"bri\":128},"
        "\"ota\":{\"lock\":false}"
        "}"
    ));

    f.close();
    DEBUG_PRINTLN(F("DpxMatrix: default cfg.json written — reboot to apply"));
}
