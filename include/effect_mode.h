#pragma once
#include <Arduino.h>
#include "valve_driver.h"
#include "config.h"

// ============================================================
//  EffectMode — long-running seamless autonomous patterns
//
//  Patterns:
//    rain    — independent random drops per column
//    wave    — traveling sine wave left→right
//    chase   — sweeping band of columns
//    pulse   — all columns pulse in sync
//    heart   — row-scan heart shape (repeating)
//    star    — row-scan 5-pointed star (repeating)
//    music   — row-scan quarter-note (repeating)
//    diamond — row-scan rhombus (repeating)
//    script  — 2-min auto-playlist cycling all patterns
// ============================================================

enum EffectPattern {
    EFX_RAIN, EFX_WAVE, EFX_CHASE, EFX_PULSE,
    EFX_HEART, EFX_STAR, EFX_MUSIC, EFX_DIAMOND,
    EFX_SCRIPT
};

// Script sequence { pattern, durationSeconds, speed(0-100) }
struct ScriptStep { uint8_t pat; uint16_t durSec; uint8_t speed; };
static const ScriptStep SCRIPT_SEQ[] = {
    { EFX_RAIN,     12, 40 },
    { EFX_HEART,    15, 50 },
    { EFX_WAVE,     10, 55 },
    { EFX_STAR,     15, 50 },
    { EFX_CHASE,    10, 65 },
    { EFX_MUSIC,    15, 50 },
    { EFX_PULSE,     8, 70 },
    { EFX_DIAMOND,  10, 50 },
    { EFX_RAIN,     10, 75 },
    { EFX_WAVE,      8, 75 },
    { EFX_HEART,    12, 40 },
};
static const int SCRIPT_N = (int)(sizeof(SCRIPT_SEQ) / sizeof(SCRIPT_SEQ[0]));

class EffectMode {
public:
    void setEffect(EffectPattern p, int speed) {
        _pat      = p;
        _speed    = constrain(speed, 0, 100);
        _t0       = millis();
        _lastTick = 0;
        _initSub(p, _speed, _t0);
    }

    void tick(ValveDriver& v) {
        uint32_t now = millis();
        if (now - _lastTick < TICK_MS) return;
        _lastTick = now;
        switch (_pat) {
            case EFX_RAIN:    _rain(v, now);          break;
            case EFX_WAVE:    _waveAt(v, now, _t0);   break;
            case EFX_CHASE:   _chaseAt(v, now, _t0);  break;
            case EFX_PULSE:   _pulseAt(v, now, _t0);  break;
            case EFX_HEART:   _shapeTick(v, now, 0);  break;
            case EFX_STAR:    _shapeTick(v, now, 1);  break;
            case EFX_MUSIC:   _shapeTick(v, now, 2);  break;
            case EFX_DIAMOND: _shapeTick(v, now, 3);  break;
            case EFX_SCRIPT:  _scriptTick(v, now);    break;
        }
    }

private:
    static const uint32_t TICK_MS = 20;

    EffectPattern _pat      = EFX_RAIN;
    int           _speed    = 50;
    uint32_t      _t0       = 0;
    uint32_t      _lastTick = 0;

    // Rain
    uint32_t _nextOpen[NUM_VALVES];
    uint32_t _closeAt[NUM_VALVES];

    // Shape scan (shared: heart / star / music / diamond)
    int      _shapeRow     = 0;
    uint32_t _shapeNextRow = 0;
    bool     _shapeInGap   = false;
    uint32_t _shapeGapEnd  = 0;

    // Script sequencer
    int      _scriptStep = -1;
    uint32_t _scriptT0   = 0;
    uint32_t _stepT0     = 0;

    // ── Helpers ────────────────────────────────────────────

    // Linear scale: speed 0→hi, speed 100→lo
    uint32_t _scaleU(int spd, uint32_t hi, uint32_t lo) {
        return (uint32_t)((long)hi - (long)spd * ((long)hi - (long)lo) / 100);
    }

    void _initSub(EffectPattern p, int speed, uint32_t now) {
        if (p == EFX_RAIN) {
            uint32_t coolMax = _scaleU(speed, 2000, 100);
            for (int i = 0; i < NUM_VALVES; i++) {
                _nextOpen[i] = now + (uint32_t)random(0, (long)coolMax);
                _closeAt[i]  = 0;
            }
        } else if (p == EFX_HEART || p == EFX_STAR || p == EFX_MUSIC || p == EFX_DIAMOND) {
            _shapeRow     = 0;
            _shapeNextRow = now;
            _shapeInGap   = false;
        } else if (p == EFX_SCRIPT) {
            _scriptT0   = now;
            _scriptStep = -1;
        }
    }

    // ── Basic effects ──────────────────────────────────────

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

    void _waveAt(ValveDriver& v, uint32_t now, uint32_t t0) {
        uint32_t periodMs = _scaleU(_speed, 3000, 300);
        float omega = TWO_PI / (float)periodMs;
        float k     = TWO_PI * 2.0f / (float)NUM_VALVES;
        float t     = (float)(now - t0);
        uint8_t bits[NUM_BOARDS] = {};
        for (int col = 0; col < NUM_VALVES; col++) {
            if (sinf(omega * t - k * (float)col) > 0.0f)
                bits[col >> 3] |= 1 << (7 - (col & 7));
        }
        v.write(bits);
    }

    void _chaseAt(ValveDriver& v, uint32_t now, uint32_t t0) {
        uint32_t periodMs = _scaleU(_speed, 4000, 400);
        int bandW = NUM_VALVES / 5;
        int pos   = (int)((uint64_t)(now - t0) % periodMs * NUM_VALVES / periodMs);
        uint8_t bits[NUM_BOARDS] = {};
        for (int col = 0; col < NUM_VALVES; col++) {
            int dist = ((col - pos) % NUM_VALVES + NUM_VALVES) % NUM_VALVES;
            if (dist < bandW)
                bits[col >> 3] |= 1 << (7 - (col & 7));
        }
        v.write(bits);
    }

    void _pulseAt(ValveDriver& v, uint32_t now, uint32_t t0) {
        uint32_t periodMs = _scaleU(_speed, 3000, 300);
        uint32_t phase    = (now - t0) % periodMs;
        uint32_t dutyMs   = periodMs * 40 / 100;
        uint8_t bits[NUM_BOARDS];
        if (phase < dutyMs) memset(bits, 0xFF, NUM_BOARDS);
        else                memset(bits, 0x00, NUM_BOARDS);
        v.write(bits);
    }

    // ── Shape helpers ──────────────────────────────────────

    // Heart: (x²+y²−1)³ ≤ x²y³
    static bool _inHeart(float nx, float ny) {
        float a = nx*nx + ny*ny - 1.0f;
        return a*a*a - nx*nx * ny*ny*ny <= 0.0f;
    }

    // 5-pointed star (one tip at top, outer r=0.92, inner r=0.38)
    static bool _inStar5(float nx, float ny) {
        float r = sqrtf(nx*nx + ny*ny);
        if (r < 0.01f) return true;
        float theta = atan2f(nx, ny);       // angle from +Y axis, clockwise
        if (theta < 0.0f) theta += TWO_PI;
        float sectorF = theta / (PI / 5.0f); // 10 sectors of 36°
        int   isec    = (int)sectorF % 10;
        float t       = sectorF - floorf(sectorF);
        const float R = 0.92f, r0 = 0.38f;
        float rb = (isec % 2 == 0)
                   ? R  - (R - r0) * t   // outer→inner
                   : r0 + (R - r0) * t;  // inner→outer
        return r <= rb;
    }

    // Quarter note: pre-computed column ranges per row
    static bool _inMusic(int col, int row) {
        // Stem cols 44-46, flag right side, note head rows 6-9
        static const int8_t NR[10][4] = {
            { 44, 46, 47, 62 }, // 0: stem + flag
            { 44, 46, 47, 56 }, // 1: stem + flag shorter
            { 44, 46, -1, -1 }, // 2: stem
            { 44, 46, -1, -1 }, // 3
            { 44, 46, -1, -1 }, // 4
            { 44, 46, -1, -1 }, // 5
            { 30, 48, -1, -1 }, // 6: head top merges with stem
            { 28, 52, -1, -1 }, // 7: head wider
            { 28, 54, -1, -1 }, // 8: head widest
            { 30, 52, -1, -1 }, // 9: head base
        };
        if (row < 0 || row > 9) return false;
        bool on = (col >= NR[row][0] && col < NR[row][1]);
        if (!on && NR[row][2] >= 0)
            on = (col >= NR[row][2] && col < NR[row][3]);
        return on;
    }

    // Diamond (rhombus) centered at col 40
    static bool _inDiamond(int col, int row, int nRows) {
        float mid = (float)(nRows - 1) * 0.5f;
        float hw  = (1.0f - fabsf((float)row - mid) / mid) * ((float)NUM_VALVES / 2.5f);
        return (float)abs(col - NUM_VALVES / 2) <= hw;
    }

    // ── Shape row scan ─────────────────────────────────────
    // shapeId: 0=heart 1=star 2=music 3=diamond
    void _shapeTick(ValveDriver& v, uint32_t now, int sid) {
        const int   nRows[4] = { 9, 10, 10, 10 };
        const int   N  = nRows[sid];
        uint32_t rowMs = _scaleU(_speed, 200, 60);
        const uint32_t gapMs = 2000;

        if (_shapeInGap) {
            uint8_t off[NUM_BOARDS] = {};
            v.write(off);
            if (now >= _shapeGapEnd) {
                _shapeInGap   = false;
                _shapeRow     = 0;
                _shapeNextRow = now;
            }
            return;
        }
        if (now < _shapeNextRow) return;

        uint8_t bits[NUM_BOARDS] = {};
        int row = _shapeRow;

        if (sid == 0) {  // heart
            // ny: +1.1 at row 0, −0.9 at row N−1
            float ny = 1.1f - 2.0f * (float)row / (float)(N - 1);
            for (int col = 0; col < NUM_VALVES; col++) {
                float nx = ((float)col - NUM_VALVES * 0.5f) / (float)(NUM_VALVES / 4) * 0.75f;
                if (_inHeart(nx, ny))
                    bits[col >> 3] |= 1 << (7 - (col & 7));
            }
        } else if (sid == 1) {  // star
            float ny = 0.90f - 1.80f * (float)row / (float)(N - 1);
            for (int col = 0; col < NUM_VALVES; col++) {
                float nx = ((float)col - NUM_VALVES * 0.5f) / ((float)NUM_VALVES / 2.2f);
                if (_inStar5(nx, ny))
                    bits[col >> 3] |= 1 << (7 - (col & 7));
            }
        } else if (sid == 2) {  // music note
            for (int col = 0; col < NUM_VALVES; col++) {
                if (_inMusic(col, row))
                    bits[col >> 3] |= 1 << (7 - (col & 7));
            }
        } else if (sid == 3) {  // diamond
            for (int col = 0; col < NUM_VALVES; col++) {
                if (_inDiamond(col, row, N))
                    bits[col >> 3] |= 1 << (7 - (col & 7));
            }
        }

        v.write(bits);
        _shapeRow++;
        _shapeNextRow = now + rowMs;
        if (_shapeRow >= N) {
            _shapeInGap  = true;
            _shapeGapEnd = now + gapMs;
        }
    }

    // ── Script sequencer ───────────────────────────────────
    void _scriptTick(ValveDriver& v, uint32_t now) {
        uint32_t total = 0;
        for (int i = 0; i < SCRIPT_N; i++)
            total += (uint32_t)SCRIPT_SEQ[i].durSec * 1000;

        uint32_t elapsed = (now - _scriptT0) % total;

        // Determine current step
        uint32_t acc = 0;
        int step = SCRIPT_N - 1;
        for (int i = 0; i < SCRIPT_N; i++) {
            uint32_t dur = (uint32_t)SCRIPT_SEQ[i].durSec * 1000;
            if (elapsed < acc + dur) { step = i; break; }
            acc += dur;
        }

        if (step != _scriptStep) {
            _scriptStep = step;
            _speed      = SCRIPT_SEQ[step].speed;
            _stepT0     = now;
            _initSub((EffectPattern)SCRIPT_SEQ[step].pat, _speed, now);
        }

        switch ((EffectPattern)SCRIPT_SEQ[_scriptStep].pat) {
            case EFX_RAIN:    _rain(v, now);             break;
            case EFX_WAVE:    _waveAt(v, now, _stepT0);  break;
            case EFX_CHASE:   _chaseAt(v, now, _stepT0); break;
            case EFX_PULSE:   _pulseAt(v, now, _stepT0); break;
            case EFX_HEART:   _shapeTick(v, now, 0);     break;
            case EFX_STAR:    _shapeTick(v, now, 1);     break;
            case EFX_MUSIC:   _shapeTick(v, now, 2);     break;
            case EFX_DIAMOND: _shapeTick(v, now, 3);     break;
            default: break;
        }
    }
};
