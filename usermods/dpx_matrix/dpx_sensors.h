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

#define DPX_I2C_SDA 21
#define DPX_I2C_SCL 22
#define DPX_SHT3X_ADDR 0x44
#define DPX_LDR_PIN 35
#define DPX_BATT_PIN 34
#define DPX_SENSOR_READ_MS 10000

static bool     dpxSht3xFound   = false;
static float    dpxTemp         = NAN;   // °C
static float    dpxHum          = NAN;   // %RH
static int      dpxBattRaw      = -1;    // 0-4095, -1 = not yet read
static int      dpxLdrRaw       = -1;    // 0-4095, -1 = not yet read
static uint32_t dpxSensorsLastReadMs = 0;

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
    dpxTemp = -45.0f + 175.0f * (rawTemp / 65535.0f);
    dpxHum  = 100.0f * (rawHum / 65535.0f);
}

static void dpxSensorsTick() {
    uint32_t now = millis();
    if (now - dpxSensorsLastReadMs < DPX_SENSOR_READ_MS) return;
    dpxSensorsLastReadMs = now;

    dpxSht3xRead();
    dpxBattRaw = analogRead(DPX_BATT_PIN);
    dpxLdrRaw  = analogRead(DPX_LDR_PIN);

    DEBUG_PRINTF("DpxSensors: temp=%.1f hum=%.1f battRaw=%d ldrRaw=%d\n",
                 dpxTemp, dpxHum, dpxBattRaw, dpxLdrRaw);
}
