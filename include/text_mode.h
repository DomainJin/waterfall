#pragma once
#include <Arduino.h>
#include "valve_driver.h"
#include "config.h"

// 5×7 pixel font
static const uint8_t TEXT_FONT[43][7] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00},  //  0: ' '
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},  //  1: 0
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},  //  2: 1
    {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F},  //  3: 2
    {0x0E,0x11,0x01,0x06,0x01,0x11,0x0E},  //  4: 3
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},  //  5: 4
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},  //  6: 5
    {0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E},  //  7: 6
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},  //  8: 7
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},  //  9: 8
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},  // 10: 9
    {0x00,0x04,0x04,0x00,0x04,0x04,0x00},  // 11: :
    {0x00,0x00,0x00,0x1F,0x00,0x00,0x00},  // 12: -
    {0x04,0x04,0x04,0x04,0x04,0x00,0x04},  // 13: !
    {0x0E,0x11,0x01,0x02,0x04,0x00,0x04},  // 14: ?
    {0x00,0x00,0x00,0x00,0x00,0x00,0x04},  // 15: .
    {0x00,0x00,0x00,0x00,0x04,0x04,0x08},  // 16: ,
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},  // 17: A
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},  // 18: B
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},  // 19: C
    {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C},  // 20: D
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},  // 21: E
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},  // 22: F
    {0x0E,0x11,0x10,0x13,0x11,0x11,0x0E},  // 23: G
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},  // 24: H
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},  // 25: I
    {0x07,0x02,0x02,0x02,0x02,0x12,0x0C},  // 26: J
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11},  // 27: K
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},  // 28: L
    {0x11,0x1B,0x15,0x11,0x11,0x11,0x11},  // 29: M
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11},  // 30: N
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},  // 31: O
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},  // 32: P
    {0x0E,0x11,0x11,0x11,0x15,0x13,0x0F},  // 33: Q
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},  // 34: R
    {0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E},  // 35: S
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},  // 36: T
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},  // 37: U
    {0x11,0x11,0x11,0x11,0x0A,0x0A,0x04},  // 38: V
    {0x11,0x11,0x11,0x15,0x15,0x1B,0x11},  // 39: W
    {0x11,0x0A,0x04,0x04,0x04,0x0A,0x11},  // 40: X
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},  // 41: Y
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},  // 42: Z
};

static uint8_t _textFontIdx(char c) {
    if (c == ' ') return 0;
    if (c >= '0' && c <= '9') return 1 + (uint8_t)(c - '0');
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c >= 'A' && c <= 'Z') return 17 + (uint8_t)(c - 'A');
    if (c == ':') return 11; if (c == '-') return 12;
    if (c == '!') return 13; if (c == '?') return 14;
    if (c == '.') return 15; if (c == ',') return 16;
    return 0;
}

class TextMode {
public:
    static const int FONT_W     = 5;
    static const int FONT_H     = 7;
    static const int CHAR_GAP   = 2;
    static const int BLANK_ROWS = 4;
    static const int MAX_CHARS  = 64;
    static const int BIG_SCALE  = 14;
    static const int BIG_CHAR_W = FONT_W * BIG_SCALE;
    static const int BIG_X_OFF  = (NUM_BOARDS * 8 - BIG_CHAR_W) / 2;

    void begin(ValveDriver& v) { _v = &v; setText(" "); }

    void setText(const char* text) {
        _numChars = 0;
        for (int i = 0; text[i] && _numChars < MAX_CHARS; i++)
            _chars[_numChars++] = _textFontIdx(text[i]);
        if (_numChars == 0) { _chars[0] = 0; _numChars = 1; }
        _recalc();
        _charIdx   = 0;
        _cycleInit = false;
    }

    void setScale(int s) {
        _scale = constrain(s, 1, 8);
        _recalc();
        _cycleInit = false;
    }

    void setPerChar(bool p)          { _perChar = p; _charIdx = 0; _cycleInit = false; }
    void setRowInterval(uint32_t ms) { _rowInterval = constrain(ms, 20, 500); _cycleInit = false; }
    void setCycleGap(uint32_t ms)    { _gapMs = ms; }
    void setMirror(bool m)  { _mirror = m; }
    void setFlipV(bool v)   { _flipV  = v; }
    void setInvert(bool inv){ _invert = inv; }

    void tick() {
        uint32_t now = millis();

        // Start new cycle if not started or previous finished
        const uint32_t cycleMs = (uint32_t)(FONT_H * _scale + BLANK_ROWS) * _rowInterval + _gapMs;
        if (!_cycleInit || (now - _cycleStart) >= cycleMs) {
            if (_cycleInit) {
                // Advance scroll / per-char
                if (_perChar) _charIdx = (_charIdx + 1) % _numChars;
                else          _advanceScroll();
            }
            _cycleStart = now;
            _cycleInit  = true;
            _evIdx      = 0;
            _buildTimeline();
        }

        // Fire all events whose time has come
        uint32_t elapsed = now - _cycleStart;
        while (_evIdx < _evN && _ev[_evIdx].t_ms <= elapsed) {
            int c = _ev[_evIdx].col;
            int b = c / 8, bit = 7 - c % 8;
            if (_ev[_evIdx].open) _curBits[b] |=  (1 << bit);
            else                  _curBits[b] &= ~(1 << bit);

            // Flush when all events at this timestamp have been processed
            if (_evIdx + 1 >= _evN || _ev[_evIdx + 1].t_ms != elapsed) {
                uint8_t out[NUM_BOARDS];
                memcpy(out, _curBits, NUM_BOARDS);
                if (_invert) for (int i = 0; i < NUM_BOARDS; i++) out[i] = ~out[i];
                _v->write(out);
            }
            _evIdx++;
        }
    }

private:
    ValveDriver* _v           = nullptr;
    uint32_t     _rowInterval = 80;
    uint32_t     _gapMs       = 0;
    int          _scale       = 2;
    bool         _mirror      = false;
    bool         _flipV       = false;
    bool         _invert      = false;
    bool         _perChar     = false;
    uint8_t      _chars[MAX_CHARS];
    int          _numChars    = 0;
    int          _totalW      = 0;
    int          _xStart      = 0;
    int          _charIdx     = 0;
    uint32_t     _cycleStart  = 0;
    bool         _cycleInit   = false;

    // ── Smooth event timeline ──────────────────────────────────
    struct ValveEv { uint32_t t_ms; uint8_t col; bool open; };
    static const int EV_MAX = 640;  // 80 cols × up to 8 events
    ValveEv  _ev[EV_MAX];
    int      _evN   = 0;
    int      _evIdx = 0;
    uint8_t  _curBits[NUM_BOARDS] = {};

    int _charW()  const { return FONT_W * _scale; }
    int _stepW()  const { return _charW() + CHAR_GAP; }

    void _recalc() {
        _totalW = _numChars * _stepW() - CHAR_GAP;
        _xStart = (_totalW <= NUM_BOARDS * 8 * 3 / 2)
                  ? (NUM_BOARDS * 8 - _totalW) / 2 : 0;
    }

    void _advanceScroll() {
        if (_totalW <= NUM_BOARDS * 8 * 3 / 2) return;
        _xStart -= _stepW();
        if (_xStart <= -_totalW) _xStart = NUM_BOARDS * 8;
    }

    void _setValve(uint8_t* bits, int v) {
        if (_mirror) v = (NUM_BOARDS * 8 - 1) - v;
        if (v < 0 || v >= NUM_BOARDS * 8) return;
        bits[v / 8] |= (1 << (7 - v % 8));
    }

    // Render one font row into bits[]
    void _renderRow(int row, uint8_t* bits) {
        const int cw = _charW(), step = _stepW();
        for (int v = 0; v < NUM_BOARDS * 8; v++) {
            int tX = v - _xStart;
            if (tX < 0 || tX >= _totalW) continue;
            int slot = tX / step, off = tX % step;
            if (slot >= _numChars || off >= cw) continue;
            int px = off / _scale;
            uint8_t fRow = TEXT_FONT[_chars[slot]][row];
            if ((fRow >> (FONT_W - 1 - px)) & 1) _setValve(bits, v);
        }
    }

    void _renderCharBig(int row, uint8_t* bits) {
        uint8_t fRow = TEXT_FONT[_chars[_charIdx]][row];
        for (int px = 0; px < FONT_W; px++) {
            if ((fRow >> (FONT_W - 1 - px)) & 1)
                for (int s = 0; s < BIG_SCALE; s++)
                    _setValve(bits, BIG_X_OFF + px * BIG_SCALE + s);
        }
    }

    // ── Local linear regression: fractional font-row at column c ──
    // bm[FONT_H][NV] — pre-rendered bitmap
    float _smoothRow(const bool bm[][NUM_BOARDS * 8], int c, int baseRow, int radius) {
        float sw=0, sx=0, sy=0, sxx=0, sxy=0; int n=0;
        for (int dc = -radius; dc <= radius; dc++) {
            int cc = c + dc;
            if (cc < 0 || cc >= NUM_BOARDS * 8) continue;
            const int rowSearch = 3;  // strict: prevents cross-feature contamination
            int bestR = -1, bestD = rowSearch + 1;
            for (int r = max(0, baseRow-rowSearch); r < min(FONT_H, baseRow+rowSearch+1); r++) {
                if (bm[r][cc]) {
                    int d = abs(r - baseRow);
                    if (d < bestD) { bestD = d; bestR = r; }
                }
            }
            if (bestR < 0) continue;
            float w = 1.0f / (fabsf((float)dc) + 1.0f);
            sw += w; sx += w*dc; sy += w*bestR;
            sxx += w*dc*dc; sxy += w*dc*bestR; n++;
        }
        if (n < 2) return (float)baseRow;
        float det = sw*sxx - sx*sx;
        return fabsf(det) < 1e-6f ? sy/sw : (sxx*sy - sx*sxy)/det;
    }

    // Build smooth event timeline for current cycle
    void _buildTimeline() {
        memset(_curBits, 0, NUM_BOARDS);
        _evN = 0;

        // Step 1: render flat 2D bitmap [FONT_H][NV]
        bool bm[FONT_H][NUM_BOARDS * 8] = {};
        for (int r = 0; r < FONT_H; r++) {
            uint8_t row[NUM_BOARDS] = {};
            int renderRow = _flipV ? (FONT_H - 1 - r) : r;
            if (_perChar) _renderCharBig(renderRow, row);
            else          _renderRow(renderRow, row);
            for (int v = 0; v < NUM_BOARDS * 8; v++)
                bm[r][v] = (row[v/8] >> (7 - v%8)) & 1;
        }

        // Step 2: per-column smooth open/close events
        struct Ev { uint32_t t; uint8_t col; bool open; };
        Ev tmpEv[EV_MAX];
        int nEv = 0;
        const int scaled = FONT_H * _scale;

        for (int c = 0; c < NUM_BOARDS * 8; c++) {
            int runStart = -1;
            for (int r = 0; r <= scaled; r++) {
                int fr = (r < scaled) ? r / _scale : FONT_H;
                bool on = (r < scaled) && (fr < FONT_H) && bm[fr][c];
                if (on && runStart < 0) {
                    runStart = r;
                } else if (!on && runStart >= 0) {
                    int runEnd   = r;
                    float frac   = _smoothRow(bm, c, runStart / _scale, 5);
                    uint32_t tO  = (uint32_t)max(0.0f, roundf(frac * _scale * _rowInterval));
                    uint32_t dur = (uint32_t)(runEnd - runStart) * _rowInterval;
                    if (nEv + 2 < EV_MAX) {
                        tmpEv[nEv++] = {tO,       (uint8_t)c, true };
                        tmpEv[nEv++] = {tO + dur, (uint8_t)c, false};
                    }
                    runStart = -1;
                }
            }
        }

        // Step 3: insertion sort by time
        for (int i = 1; i < nEv; i++) {
            Ev key = tmpEv[i]; int j = i - 1;
            while (j >= 0 && tmpEv[j].t > key.t) { tmpEv[j+1] = tmpEv[j]; j--; }
            tmpEv[j+1] = key;
        }

        // Step 4: copy to _ev[], add all-off sentinel at end
        for (int i = 0; i < nEv && _evN < EV_MAX; i++) {
            _ev[_evN].t_ms = tmpEv[i].t;
            _ev[_evN].col  = tmpEv[i].col;
            _ev[_evN].open = tmpEv[i].open;
            _evN++;
        }
        // All-off at end of BLANK period
        uint32_t endT = (uint32_t)(FONT_H * _scale + BLANK_ROWS) * _rowInterval;
        for (int c = 0; c < NUM_BOARDS * 8 && _evN < EV_MAX; c++)
            _ev[_evN++] = {endT, (uint8_t)c, false};
    }
};
