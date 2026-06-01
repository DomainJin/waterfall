#pragma once
#include <Arduino.h>
#include "valve_driver.h"
#include "config.h"

// 5×7 pixel font — same bit convention as clock_mode.h (bit4=leftmost pixel)
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
    if (c == ':') return 11;
    if (c == '-') return 12;
    if (c == '!') return 13;
    if (c == '?') return 14;
    if (c == '.') return 15;
    if (c == ',') return 16;
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
        _charIdx    = 0;
        _currentRow = 0;
        _inGap      = false;
        Serial.printf("[TEXT] setText: %d chars scale=%d totalW=%d xStart=%d perChar=%d\n",
                      _numChars, _scale, _totalW, _xStart, (int)_perChar);
    }

    void setScale(int s) {
        _scale = constrain(s, 1, 8);
        _recalc();
        _currentRow = 0;
    }

    void setPerChar(bool p)        { _perChar = p; _charIdx = 0; _currentRow = 0; _inGap = false; }
    void setRowInterval(uint32_t ms) { _rowInterval = constrain(ms, 20, 500); }
    void setCycleGap(uint32_t ms)    { _gapMs = ms; }
    void setMirror(bool m)   { _mirror = m; }
    void setFlipV(bool v)    { _flipV  = v; }
    void setInvert(bool inv) { _invert = inv; }

    void tick() {
        uint32_t now = millis();
        const int totalTicks = FONT_H * _scale + BLANK_ROWS;

        if (_inGap) {
            if (now - _gapStart >= _gapMs) {
                _inGap = false;
                _currentRow = 0;
                if (_perChar) _charIdx = (_charIdx + 1) % _numChars;
                else          _advanceScroll();
            }
            return;
        }
        if ((now - _lastRow) < _rowInterval) return;
        _lastRow = now;

        uint8_t bits[NUM_BOARDS] = {};
        if (_currentRow < FONT_H * _scale) {
            int fontRow   = _currentRow / _scale;   // scale vertically
            int renderRow = _flipV ? (FONT_H - 1 - fontRow) : fontRow;
            if (_perChar) _renderCharBig(renderRow, bits);
            else          _renderRow(renderRow, bits);
            if (_invert) for (int i = 0; i < NUM_BOARDS; i++) bits[i] = ~bits[i];
        } else if (_invert) {
            for (int i = 0; i < NUM_BOARDS; i++) bits[i] = 0xFF;
        }
        _v->write(bits);

        _currentRow++;
        if (_currentRow >= totalTicks) {
            if (_gapMs > 0) {
                _inGap    = true;
                _gapStart = now;
            } else {
                _currentRow = 0;
                if (_perChar) _charIdx = (_charIdx + 1) % _numChars;
                else          _advanceScroll();
            }
        }
    }

private:
    ValveDriver* _v           = nullptr;
    uint32_t     _rowInterval = 80;
    uint32_t     _gapMs       = 0;
    uint32_t     _lastRow     = 0;
    uint32_t     _gapStart    = 0;
    int          _currentRow  = 0;
    int          _scale       = 2;   // pixel size: 1–8
    bool         _mirror      = false;
    bool         _flipV       = false;
    bool         _invert      = false;
    bool         _inGap       = false;
    bool         _perChar     = false;
    uint8_t      _chars[MAX_CHARS];
    int          _numChars    = 0;
    int          _totalW      = 0;
    int          _xStart      = 0;
    int          _charIdx     = 0;

    int _charW()    const { return FONT_W * _scale; }
    int _stepW()    const { return _charW() + CHAR_GAP; }

    void _recalc() {
        _totalW = _numChars * _stepW() - CHAR_GAP;
        _initScroll();
    }

    void _initScroll() {
        const int VALVES = NUM_BOARDS * 8;
        _xStart = (_totalW <= VALVES * 3 / 2)
                  ? (VALVES - _totalW) / 2
                  : 0;
    }

    void _advanceScroll() {
        const int VALVES = NUM_BOARDS * 8;
        if (_totalW <= VALVES * 3 / 2) return;
        _xStart -= _stepW();
        if (_xStart <= -_totalW) _xStart = VALVES;
    }

    void _setValve(uint8_t* bits, int v) {
        if (_mirror) v = (NUM_BOARDS * 8 - 1) - v;
        if (v < 0 || v >= NUM_BOARDS * 8) return;
        bits[v / 8] |= (1 << (7 - (v % 8)));
    }

    void _renderRow(int row, uint8_t* bits) {
        const int cw   = _charW();
        const int step = _stepW();
        for (int v = 0; v < NUM_BOARDS * 8; v++) {
            int tX = v - _xStart;
            if (tX < 0 || tX >= _totalW) continue;
            int slot    = tX / step;
            int charOff = tX % step;
            if (slot >= _numChars || charOff >= cw) continue;
            int     px   = charOff / _scale;
            uint8_t fRow = TEXT_FONT[_chars[slot]][row];
            if ((fRow >> (FONT_W - 1 - px)) & 1) _setValve(bits, v);
        }
    }

    void _renderCharBig(int row, uint8_t* bits) {
        uint8_t fRow = TEXT_FONT[_chars[_charIdx]][row];
        for (int px = 0; px < FONT_W; px++) {
            if ((fRow >> (FONT_W - 1 - px)) & 1) {
                for (int s = 0; s < BIG_SCALE; s++)
                    _setValve(bits, BIG_X_OFF + px * BIG_SCALE + s);
            }
        }
    }
};
