#pragma once
#include <Arduino.h>
#include <math.h>
#include "valve_driver.h"
#include "config.h"

enum SoundPattern { SOUND_RIPPLE, SOUND_COLUMNS, SOUND_WAVE };

// Sound-reactive mode: reads mic ADC and drives valves in real-time.
// Call begin() once, then tick() every loop iteration.
class SoundMode {
public:
    void begin(uint8_t pin, ValveDriver& v) {
        _pin = pin;
        _v   = &v;
        pinMode(pin, INPUT);
        // Calibrate baseline from 64 samples
        uint32_t sum = 0;
        for (int i = 0; i < 64; i++) { sum += analogRead(pin); delayMicroseconds(50); }
        _baseline = sum / 64;
        Serial.printf("[SND] begin pin=%d baseline=%d\n", pin, _baseline);
    }

    void setPattern(SoundPattern p) {
        _pattern = p;
        memset(_bits, 0, NUM_BOARDS);
    }

    // sensitivity 0–100: higher = reacts to quieter sounds
    void setSensitivity(uint8_t s) { _sensitivity = s; }

    void tick() {
        uint32_t now = millis();
        if (now - _lastUpdate < 33) return;  // ~30 fps
        _lastUpdate = now;

        // Average 8 ADC samples to reduce noise
        int32_t sum = 0;
        for (int i = 0; i < 8; i++) sum += analogRead(_pin);
        int sample    = (int)(sum / 8);
        int amplitude = abs(sample - (int)_baseline);

        // Smooth rolling peak (slow decay)
        _peakAmplitude = max((int)(_peakAmplitude * 0.93f), amplitude);
        if (_peakAmplitude < 20) _peakAmplitude = 20;  // floor to avoid /0

        // Normalize to 0–255 relative to recent peak
        int norm = min(255, amplitude * 255 / _peakAmplitude);

        // Beat detection: amplitude exceeds threshold, with 200ms cooldown
        int threshold = map(_sensitivity, 0, 100, 180, 25);
        bool beat = (amplitude > threshold) && ((int)(now - _lastBeat) > 200);
        if (beat) _lastBeat = now;

        switch (_pattern) {
            case SOUND_RIPPLE:  _doRipple(norm);       break;
            case SOUND_COLUMNS: _doColumns(norm, beat); break;
            case SOUND_WAVE:    _doWave(norm);          break;
        }

        _v->write(_bits);
    }

private:
    uint8_t      _pin          = MIC_PIN;
    ValveDriver* _v            = nullptr;
    SoundPattern _pattern      = SOUND_RIPPLE;
    uint8_t      _sensitivity  = 50;
    uint32_t     _lastUpdate   = 0;
    uint32_t     _lastBeat     = 0;
    int          _baseline     = 2048;
    int          _peakAmplitude = 20;
    uint8_t      _bits[NUM_BOARDS] = {};
    float        _wavePhase    = 0.0f;

    // Fill valves left→right proportional to volume (VU meter)
    void _doRipple(int norm) {
        int fill = (int)((long)norm * NUM_BOARDS * 8 / 255);
        for (int b = 0; b < NUM_BOARDS; b++) {
            if (fill >= 8)        { _bits[b] = 0xFF; fill -= 8; }
            else if (fill > 0)    { _bits[b] = (uint8_t)((1 << fill) - 1); fill = 0; }
            else                    _bits[b] = 0x00;
        }
    }

    // Random columns flash on each beat; fade between beats
    void _doColumns(int norm, bool beat) {
        if (beat) {
            // Density proportional to volume
            uint8_t density = (uint8_t)map(norm, 0, 255, 0x11, 0xFF);
            for (int b = 0; b < NUM_BOARDS; b++) {
                _bits[b] = (uint8_t)(random(256) & density);
            }
        } else {
            // Shift out 1 bit per tick (fade)
            for (int b = 0; b < NUM_BOARDS; b++) {
                _bits[b] = (_bits[b] >> 1) & 0x7F;
            }
        }
    }

    // Sine wave scrolls across all valves; speed follows volume
    void _doWave(int norm) {
        float speed = 0.08f + norm * 0.004f;
        _wavePhase += speed;
        for (int b = 0; b < NUM_BOARDS; b++) {
            uint8_t byte_val = 0;
            for (int bit = 0; bit < 8; bit++) {
                float angle = _wavePhase + (b * 8 + bit) * 0.25f;
                if (sinf(angle) > 0) byte_val |= (1 << bit);
            }
            _bits[b] = byte_val;
        }
    }
};
