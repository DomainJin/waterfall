#pragma once
#include <Arduino.h>
#include "valve_driver.h"
#include "config.h"

// ============================================================
//  EffectMode — long-running seamless autonomous patterns
//
//  Patterns:
//    rain  — independent random drops per column
//    wave  — traveling sine wave left→right
//    chase — sweeping band of columns
//    pulse — all columns pulse in sync
//
//  Usage:
//    g_effect.setEffect(EFX_RAIN, speed);   // speed 0-100
//    g_effect.tick(g_valve);                // call every loop
// ============================================================

enum EffectPattern { EFX_RAIN, EFX_WAVE, EFX_CHASE, EFX_PULSE };

class EffectMode {
public:
    void setEffect(EffectPattern p, int speed) {
        _pat      = p;
        _speed    = constrain(speed, 0, 100);
        _t0       = millis();
        _lastTick = 0;
        if (p == EFX_RAIN) {
            uint32_t coolMax = _scaleU(speed, 2000, 100);
            for (int i = 0; i < NUM_VALVES; i++) {
                _nextOpen[i] = millis() + (uint32_t)random(0, (long)coolMax);
                _closeAt[i]  = 0;
            }
        }
    }

    void tick(ValveDriver& v) {
        uint32_t now = millis();
        if (now - _lastTick < TICK_MS) return;
        _lastTick = now;
        switch (_pat) {
            case EFX_RAIN:  _rain(v, now);  break;
            case EFX_WAVE:  _wave(v, now);  break;
            case EFX_CHASE: _chase(v, now); break;
            case EFX_PULSE: _pulse(v, now); break;
        }
    }

private:
    static const uint32_t TICK_MS = 20; // 50 Hz update rate

    EffectPattern _pat      = EFX_RAIN;
    int           _speed    = 50;
    uint32_t      _t0       = 0;
    uint32_t      _lastTick = 0;
    uint32_t      _nextOpen[NUM_VALVES];
    uint32_t      _closeAt[NUM_VALVES];

    // Linear map: speed 0→hi, speed 100→lo  (inverse — faster at higher speed)
    uint32_t _scaleU(int spd, uint32_t hi, uint32_t lo) {
        return (uint32_t)((long)hi - (long)spd * ((long)hi - (long)lo) / 100);
    }

    void _rain(ValveDriver& v, uint32_t now) {
        uint32_t openMs  = _scaleU(_speed, 200, 80);
        uint32_t coolMin = _scaleU(_speed, 400, 30);
        uint32_t coolMax = _scaleU(_speed, 2000, 100);
        if (coolMax <= coolMin) coolMax = coolMin + 10;
        uint8_t bits[NUM_BOARDS] = {};
        for (int col = 0; col < NUM_VALVES; col++) {
            if (now >= _nextOpen[col]) {
                _closeAt[col]  = now + openMs;
                _nextOpen[col] = _closeAt[col] + coolMin
                               + (uint32_t)random(0, (long)(coolMax - coolMin));
            }
            if (now < _closeAt[col])
                bits[col >> 3] |= 1 << (7 - (col & 7));
        }
        v.write(bits);
    }

    void _wave(ValveDriver& v, uint32_t now) {
        uint32_t periodMs = _scaleU(_speed, 3000, 300);
        float omega = TWO_PI / (float)periodMs;
        float k     = TWO_PI * 2.0f / (float)NUM_VALVES; // 2 wave crests
        float t     = (float)(now - _t0);
        uint8_t bits[NUM_BOARDS] = {};
        for (int col = 0; col < NUM_VALVES; col++) {
            if (sinf(omega * t - k * (float)col) > 0.0f)
                bits[col >> 3] |= 1 << (7 - (col & 7));
        }
        v.write(bits);
    }

    void _chase(ValveDriver& v, uint32_t now) {
        uint32_t periodMs = _scaleU(_speed, 4000, 400);
        int bandW = NUM_VALVES / 5; // 20% width ≈ 16 columns
        int pos   = (int)((uint64_t)(now - _t0) % periodMs * NUM_VALVES / periodMs);
        uint8_t bits[NUM_BOARDS] = {};
        for (int col = 0; col < NUM_VALVES; col++) {
            int dist = ((col - pos) % NUM_VALVES + NUM_VALVES) % NUM_VALVES;
            if (dist < bandW)
                bits[col >> 3] |= 1 << (7 - (col & 7));
        }
        v.write(bits);
    }

    void _pulse(ValveDriver& v, uint32_t now) {
        uint32_t periodMs = _scaleU(_speed, 3000, 300);
        uint32_t phase    = (now - _t0) % periodMs;
        uint32_t dutyMs   = periodMs * 40 / 100; // 40% on, 60% off
        uint8_t bits[NUM_BOARDS];
        if (phase < dutyMs) memset(bits, 0xFF, NUM_BOARDS);
        else                memset(bits, 0x00, NUM_BOARDS);
        v.write(bits);
    }
};
