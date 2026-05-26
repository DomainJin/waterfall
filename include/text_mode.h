#pragma once
#include <Arduino.h>
#include "valve_driver.h"
#include "config.h"

// 5×7 pixel font — same bit convention as clock_mode.h (bit4=leftmost pixel)
// Index: 0=space, 1-10='0'-'9', 11=':', 12='-', 13='!', 14='?', 15='.', 16=','
//        17-42 = 'A'-'Z'
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
    static const int SCALE      = 2;
    static const int FONT_W     = 5;
    static const int FONT_H     = 7;
    static const int CHAR_W     = FONT_W * SCALE;  // 10 valves
    static const int CHAR_GAP   = 2;
    static const int BLANK_ROWS = 4;
    static const int MAX_CHARS  = 64;

    void begin(ValveDriver& v) {
        _v = &v;
        setText(" ");
    }

    void setText(const char* text) {
        _numChars = 0;
        for (int i = 0; text[i] && _numChars < MAX_CHARS; i++) {
            _chars[_numChars++] = _textFontIdx(text[i]);
        }
        if (_numChars == 0) { _chars[0] = 0; _numChars = 1; }
        _totalW = _numChars * (CHAR_W + CHAR_GAP) - CHAR_GAP;
        _initScroll();
        _currentRow = 0;
        _inGap      = false;
        Serial.printf("[TEXT] setText: %d chars, totalW=%d, xStart=%d\n",
                      _numChars, _totalW, _xStart);
    }

    void setRowInterval(uint32_t ms) { _rowInterval = constrain(ms, 20, 500); }
    void setCycleGap(uint32_t ms)    { _gapMs = ms; }
    void setMirror(bool m)   { _mirror = m; }
    void setFlipV(bool v)    { _flipV  = v; }
    void setInvert(bool inv) { _invert = inv; }

    void tick() {
        uint32_t now = millis();
        if (_inGap) {
            if (now - _gapStart >= _gapMs) {
                _inGap = false;
                _currentRow = 0;
                _advanceScroll();
            }
            return;
        }
        if ((now - _lastRow) < _rowInterval) return;
        _lastRow = now;

        uint8_t bits[NUM_BOARDS] = {};
        if (_currentRow < FONT_H) {
            int renderRow = _flipV ? (FONT_H - 1 - _currentRow) : _currentRow;
            _renderRow(renderRow, bits);
            if (_invert) for (int i = 0; i < NUM_BOARDS; i++) bits[i] = ~bits[i];
        } else if (_invert) {
            for (int i = 0; i < NUM_BOARDS; i++) bits[i] = 0xFF;
        }
        _v->write(bits);

        _currentRow++;
        if (_currentRow >= FONT_H + BLANK_ROWS) {
            if (_gapMs > 0) {
                _inGap    = true;
                _gapStart = now;
            } else {
                _currentRow = 0;
                _advanceScroll();
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
    bool         _mirror      = false;
    bool         _flipV       = false;
    bool         _invert      = false;
    bool         _inGap       = false;
    uint8_t      _chars[MAX_CHARS];
    int          _numChars    = 0;
    int          _totalW      = 0;
    int          _xStart      = 0;

    void _initScroll() {
        const int VALVES = NUM_BOARDS * 8;
        if (_totalW <= VALVES) {
            _xStart = (VALVES - _totalW) / 2;
        } else {
            _xStart = VALVES;  // starts off the right edge, scrolls left
        }
    }

    void _advanceScroll() {
        const int VALVES = NUM_BOARDS * 8;
        if (_totalW <= VALVES) return;
        _xStart--;
        if (_xStart <= -_totalW) _xStart = VALVES;
    }

    void _setValve(uint8_t* bits, int v) {
        if (_mirror) v = (NUM_BOARDS * 8 - 1) - v;
        if (v < 0 || v >= NUM_BOARDS * 8) return;
        bits[v / 8] |= (1 << (7 - (v % 8)));
    }

    void _renderRow(int row, uint8_t* bits) {
        for (int v = 0; v < NUM_BOARDS * 8; v++) {
            int tX = v - _xStart;
            if (tX < 0 || tX >= _totalW) continue;
            int slot    = tX / (CHAR_W + CHAR_GAP);
            int charOff = tX % (CHAR_W + CHAR_GAP);
            if (slot >= _numChars || charOff >= CHAR_W) continue;
            int     px   = charOff / SCALE;
            uint8_t fRow = TEXT_FONT[_chars[slot]][row];
            if ((fRow >> (FONT_W - 1 - px)) & 1) _setValve(bits, v);
        }
    }
};
