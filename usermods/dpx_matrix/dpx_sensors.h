// ================================================================================
// dpx_sensors.h — TC001 Hardware Sensor Reads (GH #15, phase 1: 1.8.1-1.8.3)
// ================================================================================
// Original work — dubpixel / dpx_tc002 (EUPL v1.2)
// ================================================================================
// PROJECT: dpx_tc002_frm
// ================================================================================
//
// Raw sensor reads only — no native apps/UI yet (that's #15's 1.8.4/1.8.5,
// deliberately held back until these readings are confirmed real on hardware
// via /api/stats rather than assumed from the ticket's spec).
//
//   SHT3x   I2C (GPIO21 SDA / GPIO22 SCL), addr 0x44 — temp (°C) + humidity (%RH)
//   Battery GPIO34 (ADC1) — raw 0-4095, no voltage-divider ratio confirmed yet
//   LDR     GPIO35 (ADC1) — raw 0-4095, no lux conversion confirmed yet
//
// Battery/LDR are exposed as RAW ADC values deliberately — mapping raw counts
// to volts/percent or lux requires knowing this board's actual voltage-divider
// resistors and LDR characteristics, which we don't have confirmed yet. Report
// raw so real hardware readings can be sanity-checked before inventing a
// conversion formula that might be wrong.
// ================================================================================

#pragma once

#include <Wire.h>
#include "dpx_icons.h"

#define DPX_I2C_SDA 21
#define DPX_I2C_SCL 22
#define DPX_SHT3X_ADDR 0x44
#define DPX_LDR_PIN 35
#define DPX_BATT_PIN 34
#define DPX_SENSOR_READ_MS 10000

// Battery raw-ADC range — ported from Blueforcer/awtrix3's PeripheryManager.cpp
// defaults (MIN_BATTERY=475, MAX_BATTERY=665), which target this exact board's
// GPIO34 circuit. Those are 10-bit (0-1023) values from awtrix3's ESP8266
// lineage; scaled x4 for our ESP32's 12-bit (0-4095) analogRead(). Confirmed
// plausible against real hardware: two independent test devices read
// batRaw 2362 and 2464, both inside this scaled 1900-2660 range.
#define DPX_BATT_RAW_MIN 1900
#define DPX_BATT_RAW_MAX 2660

static bool     dpxSht3xFound   = false;
static float    dpxTemp         = NAN;   // °C, DPX_TEMP_OFFSET already applied
static float    dpxHum          = NAN;   // %RH, DPX_HUM_OFFSET already applied
static int      dpxBattRaw      = -1;    // 0-4095, -1 = not yet read
static int      dpxBattPct      = -1;    // 0-100, calibrated from DPX_BATT_RAW_MIN/MAX
static int      dpxLdrRaw       = -1;    // 0-4095, -1 = not yet read
static int      dpxLdrPct       = -1;    // 0-100 "brightness" — awtrix3's LDR_FACTOR/GAMMA curve
static uint32_t dpxSensorsLastReadMs = 0;

// 8x8 icons for the native Temperature/Humidity/Battery apps — baked in so
// they work without the user having to browse/download an icon first. Written
// to LittleFS on first use so they flow through the same name-based
// dpxGetIcon()/app.icon pipeline as any user-picked icon (dpx_icons.h) rather
// than needing a separate render path. 0x000000 = transparent.
// Sourced from LaMetric's public icon catalog (same catalog our own /browse
// icon picker and awtrix3's downloader both use) — real, recognizable,
// natively 8x8 pixel art already, no resizing/redrawing needed:
//   Thermometer: icon #2262 ("Temperature Celcius")
//   Waterdrop:   icon #623  ("Drop")
//   Battery:     icon #6358 ("Battery 100")
// Fetched via https://developer.lametric.com/content/apps/icon_thumbs/<id>
// and converted with the exact same alpha-to-transparent rule dpx_html.h's
// browser-side installer uses (dpxRenderIcon() skips 0x000000 pixels).
static const uint32_t DPX_ICON_THERMOMETER[64] = {
    0x000000,0xFFFFFF,0xFFFFFF,0xFFFFFF,0x000000,0x000000,0xFFFFFF,0xFFFFFF,
    0x000000,0xFFFFFF,0x000000,0xFFFFFF,0x000000,0x000000,0xFFFFFF,0x000000,
    0x000000,0xFFFFFF,0xF10E0E,0xFFFFFF,0x000000,0x000000,0xFFFFFF,0xFFFFFF,
    0x000000,0xFFFFFF,0xF10E0E,0xFFFFFF,0x000000,0x000000,0xFFFFFF,0x000000,
    0x000000,0xFFFFFF,0xF10E0E,0xFFFFFF,0x000000,0x000000,0x000000,0x000000,
    0xFFFFFF,0xF10E0E,0xF10E0E,0xF10E0E,0xFFFFFF,0x000000,0x000000,0x000000,
    0xFFFFFF,0xF10E0E,0xF10E0E,0xF10E0E,0xFFFFFF,0x000000,0x000000,0x000000,
    0x000000,0xFFFFFF,0xFFFFFF,0xFFFFFF,0x000000,0x000000,0x000000,0x000000,
};
static const uint32_t DPX_ICON_WATERDROP[64] = {
    0x000000,0x000000,0x000000,0x000000,0x4FFFFE,0x000000,0x000000,0x000000,
    0x000000,0x000000,0x000000,0x4FFFFE,0x49BDFE,0x000000,0x000000,0x000000,
    0x000000,0x000000,0x4FFFFE,0x49BDFE,0x49BDFE,0x49BDFE,0x000000,0x000000,
    0x000000,0x000000,0x49BDFE,0x49BDFE,0x49BDFE,0x49BDFE,0x000000,0x000000,
    0x000000,0x49BDFE,0x49BDFE,0x49BDFE,0x49BDFE,0x49BDFE,0x4237FE,0x000000,
    0x000000,0x49BDFE,0xFFFFFF,0x49BDFE,0x49BDFE,0x49BDFE,0x4237FE,0x000000,
    0x000000,0x000000,0x49BDFE,0x49BDFE,0x49BDFE,0x4237FE,0x000000,0x000000,
    0x000000,0x000000,0x000000,0x49BDFE,0x4237FE,0x000000,0x000000,0x000000,
};
static const uint32_t DPX_ICON_BATTERY[64] = {
    0x000000,0x000000,0x000000,0xFFFFFF,0xFFFFFF,0x000000,0x000000,0x000000,
    0x000000,0x000000,0xFFFFFF,0xFFFFFF,0xFFFFFF,0xFFFFFF,0x000000,0x000000,
    0x000000,0x000000,0xFFFFFF,0x34BF26,0x34BF26,0xFFFFFF,0x000000,0x000000,
    0x000000,0x000000,0xFFFFFF,0x34BF26,0x34BF26,0xFFFFFF,0x000000,0x000000,
    0x000000,0x000000,0xFFFFFF,0x34BF26,0x34BF26,0xFFFFFF,0x000000,0x000000,
    0x000000,0x000000,0xFFFFFF,0x34BF26,0x34BF26,0xFFFFFF,0x000000,0x000000,
    0x000000,0x000000,0xFFFFFF,0x34BF26,0x34BF26,0xFFFFFF,0x000000,0x000000,
    0x000000,0x000000,0xFFFFFF,0xFFFFFF,0xFFFFFF,0xFFFFFF,0x000000,0x000000,
};

// Idempotent — writes each icon file only if missing, so a user deleting one
// via the File Manager doesn't get it silently rewritten every boot, but a
// missing one (fresh device) gets recreated.
static void dpxWriteDefaultIcon(const char* name, const uint32_t* pixels) {
    String path = "/ICONS/" + String(name) + ".raw";
    if (LittleFS.exists(path)) return;
    File f = LittleFS.open(path, "w");
    if (!f) return;
    for (int i = 0; i < DPX_ICON_PIXELS; i++) {
        uint32_t c = pixels[i];
        f.write((uint8_t)(c >> 16));
        f.write((uint8_t)(c >> 8));
        f.write((uint8_t)c);
    }
    f.close();
}

// SHT3x CRC-8 check (polynomial 0x31, init 0xFF) — per Sensirion datasheet.
// Catches a bad/missing sensor giving plausible-looking garbage instead of
// silently reporting wrong temp/hum.
static uint8_t dpxSht3xCrc8(const uint8_t* data, int len) {
    uint8_t crc = 0xFF;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
            crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
    }
    return crc;
}

static void dpxSensorsInit() {
    if (!PinManager::allocatePin(DPX_I2C_SDA, true, PinOwner::UM_Temperature))  { DEBUG_PRINTLN(F("DpxSensors: SDA pin alloc failed")); return; }
    if (!PinManager::allocatePin(DPX_I2C_SCL, true, PinOwner::UM_Temperature))  { DEBUG_PRINTLN(F("DpxSensors: SCL pin alloc failed")); return; }
    if (!PinManager::allocatePin(DPX_BATT_PIN, false, PinOwner::UM_Battery))    { DEBUG_PRINTLN(F("DpxSensors: battery pin alloc failed")); }
    if (!PinManager::allocatePin(DPX_LDR_PIN, false, PinOwner::UM_LDR_DUSK_DAWN)) { DEBUG_PRINTLN(F("DpxSensors: LDR pin alloc failed")); }

    Wire.begin(DPX_I2C_SDA, DPX_I2C_SCL);

    // Probe for SHT3x at 0x44 — a bare address ack, no data transaction yet.
    Wire.beginTransmission(DPX_SHT3X_ADDR);
    dpxSht3xFound = (Wire.endTransmission() == 0);
    DEBUG_PRINTF("DpxSensors: SHT3x %s at 0x%02X\n", dpxSht3xFound ? "found" : "NOT found", DPX_SHT3X_ADDR);

    dpxWriteDefaultIcon("dpx_thermo", DPX_ICON_THERMOMETER);
    dpxWriteDefaultIcon("dpx_drop",   DPX_ICON_WATERDROP);
    dpxWriteDefaultIcon("dpx_batt",   DPX_ICON_BATTERY);
}

static void dpxSht3xRead() {
    if (!dpxSht3xFound) return;

    // 0x2C06 = single-shot measurement, high repeatability, clock stretching
    Wire.beginTransmission(DPX_SHT3X_ADDR);
    Wire.write(0x2C);
    Wire.write(0x06);
    if (Wire.endTransmission() != 0) { dpxSht3xFound = false; return; } // lost the sensor

    delay(15); // datasheet: up to 15ms for high-repeatability measurement

    if (Wire.requestFrom((uint8_t)DPX_SHT3X_ADDR, (uint8_t)6) != 6) return;
    uint8_t buf[6];
    for (int i = 0; i < 6; i++) buf[i] = Wire.read();

    if (dpxSht3xCrc8(buf, 2) != buf[2] || dpxSht3xCrc8(buf + 3, 2) != buf[5]) {
        DEBUG_PRINTLN(F("DpxSensors: SHT3x CRC mismatch, discarding read"));
        return;
    }

    uint16_t rawTemp = (buf[0] << 8) | buf[1];
    uint16_t rawHum  = (buf[3] << 8) | buf[4];
    // DPX_TEMP_OFFSET/DPX_HUM_OFFSET (dpx_persist.h) — temp default is -9.0C,
    // correcting for the sensor reading enclosure self-heating (ESP32 + LED
    // matrix), not ambient room temp. Confirmed necessary live: raw reads of
    // 38-42C against a real ~16.7C (62F) room.
    dpxTemp = -45.0f + 175.0f * (rawTemp / 65535.0f) + DPX_TEMP_OFFSET;
    dpxHum  = 100.0f * (rawHum / 65535.0f) + DPX_HUM_OFFSET;
    dpxHum  = constrain(dpxHum, 0.0f, 100.0f);
}

// Maps a raw ADC count to 0-100 using awtrix3's own gamma-curve formula
// (LDR_FACTOR/LDR_GAMMA, dpx_persist.h), scaled from their 10-bit (1023) to
// our 12-bit (4095) ADC range. Used for both LDR "brightness %" and, if ABRI
// is ever extended to non-linear response, could apply equally to battery —
// kept as its own function since the two aren't guaranteed to want the same
// curve.
static int dpxLdrToPercent(int raw) {
    float pct = (raw * DPX_LDR_FACTOR) / 4095.0f * 100.0f;
    pct = pow(pct, DPX_LDR_GAMMA) / pow(100.0f, DPX_LDR_GAMMA - 1.0f);
    return (int)constrain(pct, 0.0f, 100.0f);
}

static void dpxSensorsTick() {
    if (!DPX_SENSOR_READING) return;

    uint32_t now = millis();
    if (now - dpxSensorsLastReadMs < DPX_SENSOR_READ_MS) return;
    dpxSensorsLastReadMs = now;

    dpxSht3xRead();
    dpxBattRaw = analogRead(DPX_BATT_PIN);
    dpxLdrRaw  = analogRead(DPX_LDR_PIN);
    dpxBattPct = (int)constrain((float)map(dpxBattRaw, DPX_BATT_RAW_MIN, DPX_BATT_RAW_MAX, 0, 100), 0.0f, 100.0f);
    dpxLdrPct  = dpxLdrToPercent(dpxLdrRaw);

    DEBUG_PRINTF("DpxSensors: temp=%.1f hum=%.1f battRaw=%d battPct=%d ldrRaw=%d ldrPct=%d\n",
                 dpxTemp, dpxHum, dpxBattRaw, dpxBattPct, dpxLdrRaw, dpxLdrPct);
}

// GH #15 1.8.3 — smoothly ramps brightness from the LDR reading when enabled.
// Only takes effect once dpxSensorsTick() has produced a real dpxLdrPct
// (i.e. after the first 10s read), and only while DPX_ABRI is on.
static void dpxAbriTick() {
    if (!DPX_ABRI || dpxLdrPct < 0) return;
    int target = map(dpxLdrPct, 0, 100, DPX_MIN_BRI, DPX_MAX_BRI);
    target = constrain(target, DPX_MIN_BRI, DPX_MAX_BRI);
    if (target != bri) {
        bri = target;
        stateUpdated(CALL_MODE_NO_NOTIFY);
    }
}
