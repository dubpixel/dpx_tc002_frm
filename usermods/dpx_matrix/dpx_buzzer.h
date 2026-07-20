// ================================================================================
// dpx_buzzer.h — RTTTL tone player for TC001 passive piezo (GPIO 15)
// ================================================================================
// Original work — dubpixel / dpx_tc002 (EUPL v1.2)
// ================================================================================
// Uses ESP32 LEDC channel 6 for PWM square-wave output.
// All endpoints pass RTTTL as a query param (?q=) — no body parsing needed.
//
// Public API:
//   dpxBuzzerInit()              — call once from setup()
//   dpxBuzzerPlay(const char*)   — start playing RTTTL string
//   dpxBuzzerStop()              — silence and reset
//   dpxBuzzerTick()              — advance sequencer; call from loop()
// ================================================================================

#pragma once
#include "Arduino.h"

// DPX_SOUND_ENABLED is a static bool in dpx_persist.h, included before this file.

#define DPX_BUZZER_PIN       15
#define DPX_BUZZER_LEDC_CH    6    // LEDC channel — away from WLED's channels
#define DPX_BUZZER_LEDC_RES  10   // 10-bit resolution: duty 0-1023, 512 = 50%
#define DPX_BUZZER_MAX_NOTES 96   // max notes per song

// ── Note ─────────────────────────────────────────────────────────────────────
struct DpxBuzzerNote { uint16_t freq; uint16_t ms; };  // freq=0 → rest/pause

static DpxBuzzerNote  _bzNotes[DPX_BUZZER_MAX_NOTES];
static uint8_t        _bzCount  = 0;
static uint8_t        _bzIdx    = 0;
static bool           _bzActive = false;
static unsigned long  _bzStart  = 0;   // millis() when current note began

// ── Frequency table: C4 through B4 (equal temperament, Hz) ───────────────────
// Index: 0=C  1=C#  2=D  3=D#  4=E  5=F  6=F#  7=G  8=G#  9=A  10=A#  11=B
static const uint16_t _BZ_FREQ[12] PROGMEM = {
    262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494
};

// Pitch letter → semitone index 0–11.  Returns -1 for rest ('p' or unknown).
static int8_t _bzSemi(char c) {
    switch (tolower((uint8_t)c)) {
        case 'c': return  0;
        case 'd': return  2;
        case 'e': return  4;
        case 'f': return  5;
        case 'g': return  7;
        case 'a': return  9;
        case 'b': return 11;
        case 'h': return 11;  // German notation: H = B
        default:  return -1;  // 'p' or anything else = rest
    }
}

// ── Parser ────────────────────────────────────────────────────────────────────
// RTTTL format: "Title:d=4,o=5,b=120:note,note,..."
// note format:  [dur][pitch][#][octave][.]
static void _bzParse(const char* s) {
    _bzCount = 0;
    if (!s || !*s) return;

    // 1. Skip title (up to first ':')
    const char* p = s;
    while (*p && *p != ':') p++;
    if (*p++ != ':') return;

    // 2. Parse header defaults: d=4,o=5,b=120
    uint8_t  defDur = 4, defOct = 5;
    uint16_t bpm    = 120;
    const char* hEnd = p;
    while (*hEnd && *hEnd != ':') hEnd++;

    while (p < hEnd) {
        while (p < hEnd && (*p == ' ' || *p == ',')) p++;
        if (p >= hEnd) break;
        char key = tolower((uint8_t)*p++);
        if (*p++ != '=') continue;
        uint16_t val = 0;
        while (*p >= '0' && *p <= '9') val = val * 10 + (*p++ - '0');
        switch (key) {
            case 'd': if (val >= 1 && val <= 32) defDur = (uint8_t)val; break;
            case 'o': if (val >= 4 && val <= 8)  defOct = (uint8_t)val; break;
            case 'b': if (val >= 1)               bpm    = val;          break;
        }
    }
    if (*hEnd != ':') return;
    p = hEnd + 1;

    // Whole-note = 60000ms/beat × 4 beats/whole = 240000/BPM
    uint32_t wholeMs = 240000UL / bpm;

    // 3. Parse notes
    while (*p && _bzCount < DPX_BUZZER_MAX_NOTES) {
        while (*p == ' ' || *p == ',') p++;
        if (!*p) break;

        // [duration] — optional, default = defDur
        uint8_t dur = 0;
        while (*p >= '0' && *p <= '9') dur = dur * 10 + (*p++ - '0');
        if (dur < 1) dur = defDur;

        // [pitch] — required
        if (!*p) break;
        int8_t semi = _bzSemi(*p++);

        // [#] — optional sharp
        if (*p == '#') { if (semi >= 0) semi = (semi + 1) % 12; p++; }

        // [octave] — optional digit 4–8
        uint8_t oct = defOct;
        if (*p >= '4' && *p <= '8') oct = (uint8_t)(*p++ - '0');

        // [.] — optional dotted (×1.5)
        bool dot = (*p == '.'); if (dot) p++;

        // Duration in ms, clamped to [10, 4000]
        uint32_t ms = wholeMs / dur;
        if (dot) ms = ms * 3 / 2;
        ms = constrain(ms, 10UL, 4000UL);

        // Frequency
        uint16_t freq = 0;
        if (semi >= 0) {
            uint32_t f = pgm_read_word(&_BZ_FREQ[(uint8_t)semi]);
            int8_t shift = (int8_t)(oct - 4);
            if      (shift > 0) f <<= (uint8_t) shift;
            else if (shift < 0) f >>= (uint8_t)(-shift);
            freq = (uint16_t)constrain((int32_t)f, 20, 20000);
        }
        _bzNotes[_bzCount++] = { freq, (uint16_t)ms };
    }
}

// ── LEDC tone helper ──────────────────────────────────────────────────────────
// ledcWriteTone() changes frequency in-place at 50% duty — no timer reconfiguration,
// no inter-note glitch. This is how Nokia 3310 drove its piezo.
static inline void _bzTone(uint16_t freq) {
    ledcWriteTone(DPX_BUZZER_LEDC_CH, freq);  // freq=0 silences, >0 plays square wave at 50%
}

// ── Public API ────────────────────────────────────────────────────────────────
static void dpxBuzzerStop();

static void dpxBuzzerInit() {
    ledcAttachPin(DPX_BUZZER_PIN, DPX_BUZZER_LEDC_CH);
    ledcWriteTone(DPX_BUZZER_LEDC_CH, 0);  // silent
}

static void dpxBuzzerPlay(const char* rtttl) {
    if (!DPX_SOUND_ENABLED) return;
    dpxBuzzerStop();
    if (!rtttl || !*rtttl) return;
    _bzParse(rtttl);
    if (_bzCount == 0) return;
    _bzIdx    = 0;
    _bzActive = true;
    _bzStart  = millis();
    _bzTone(_bzNotes[0].freq);
}

static void dpxBuzzerStop() {
    ledcWriteTone(DPX_BUZZER_LEDC_CH, 0);
    _bzActive = false;
    _bzIdx    = 0;
    _bzCount  = 0;
}

static void dpxBuzzerTick() {
    if (!_bzActive) return;
    if (millis() - _bzStart < _bzNotes[_bzIdx].ms) return;
    _bzIdx++;
    if (_bzIdx >= _bzCount) { dpxBuzzerStop(); return; }
    _bzStart = millis();
    _bzTone(_bzNotes[_bzIdx].freq);
}
