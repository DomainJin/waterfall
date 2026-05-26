#pragma once
#include <Arduino.h>
#include <time.h>
#include "valve_driver.h"
#include "config.h"

// ── 5×7 pixel font ── index 0-9 = digits, 10 = ':', 11 = '-' (no sync)
// Each byte = one row; bit4 = leftmost pixel, bit0 = rightmost pixel
static const uint8_t CLOCK_FONT[12][7] = {
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},  // 0
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},  // 1
    {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F},  // 2
    {0x0E,0x11,0x01,0x06,0x01,0x11,0x0E},  // 3
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},  // 4
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},  // 5
    {0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E},  // 6
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},  // 7
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},  // 8
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},  // 9
    {0x00,0x04,0x04,0x00,0x04,0x04,0x00},  // :
    {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00},  // - (NTP not synced)
};

class ClockMode {
public:
    // Pixel scale: each font pixel → SCALE valves wide
    static const int SCALE    = 2;
    static const int FONT_W   = 5;
    static const int FONT_H   = 7;
    static const int CHAR_W   = FONT_W * SCALE;   // 10 valves
    static const int CHAR_GAP = 2;                // valves between chars
    static const int NUM_CHARS = 5;               // "HH:MM"
    // Total width: 5×10 + 4×2 = 58 valves; center in 80: offset 11
    static const int X_OFFSET = (NUM_BOARDS * 8 - (NUM_CHARS * CHAR_W + (NUM_CHARS - 1) * CHAR_GAP)) / 2;
    static const int BLANK_ROWS = 4;  // blank rows appended after each frame

    void begin(ValveDriver& v) {
        _v = &v;
        _refreshChars();
        Serial.printf("[CLK] begin x_offset=%d char_w=%d total=%d\n",
                      X_OFFSET, CHAR_W, NUM_CHARS * CHAR_W + (NUM_CHARS - 1) * CHAR_GAP);
    }

    // ms between rows — controls physical text height in falling water
    void setRowInterval(uint32_t ms) { _rowInterval = constrain(ms, 20, 500); }

    // ms to wait (valves off) after each complete cycle before the next
    void setCycleGap(uint32_t ms)    { _gapMs = ms; }

    // Flip left↔right if digits appear mirrored on the curtain
    void setMirror(bool m)  { _mirror = m; }

    // Flip top↔bottom (reverse row order)
    void setFlipV(bool v)   { _flipV = v; }

    // Invert: open all valves EXCEPT the digit pixels
    void setInvert(bool inv) { _invert = inv; }

    void tick() {
        uint32_t now = millis();

        // Cycle gap: hold valves off between complete displays
        if (_inGap) {
            if (now - _gapStart >= _gapMs) {
                _inGap = false;
                _currentRow = 0;
                _refreshChars();
            }
            return;
        }

        if ((now - _lastRow) < _rowInterval) return;
        _lastRow = now;

        uint8_t bits[NUM_BOARDS] = {};
        if (_currentRow < FONT_H) {
            int renderRow = _flipV ? (FONT_H - 1 - _currentRow) : _currentRow;
            _renderRow(renderRow, bits);
            if (_invert) {
                for (int i = 0; i < NUM_BOARDS; i++) bits[i] = ~bits[i];
            }
        }
        // else: blank row — if inverted, all valves open during blank rows too
        else if (_invert) {
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
                _refreshChars();
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
    uint8_t      _chars[NUM_CHARS] = {11,11,10,11,11};  // "--:--" until NTP syncs

    void _refreshChars() {
        struct tm t;
        if (!getLocalTime(&t, 0)) {
            // NTP not synced: show "--:--"
            _chars[0] = 11; _chars[1] = 11;
            _chars[2] = 10;
            _chars[3] = 11; _chars[4] = 11;
            return;
        }
        _chars[0] = t.tm_hour / 10;
        _chars[1] = t.tm_hour % 10;
        _chars[2] = 10;  // ':'
        _chars[3] = t.tm_min / 10;
        _chars[4] = t.tm_min % 10;
    }

    // Set one valve in the bits array (v = 0 is leftmost valve)
    void _setValve(uint8_t* bits, int v) {
        if (_mirror) v = (NUM_BOARDS * 8 - 1) - v;
        if (v < 0 || v >= NUM_BOARDS * 8) return;
        // MSB-first convention: bit7 of bits[board] = leftmost valve of that board
        bits[v / 8] |= (1 << (7 - (v % 8)));
    }

    void _renderRow(int row, uint8_t* bits) {
        int x = X_OFFSET;
        for (int c = 0; c < NUM_CHARS; c++) {
            uint8_t fontRow = CLOCK_FONT[_chars[c]][row];
            for (int px = 0; px < FONT_W; px++) {
                bool on = (fontRow >> (FONT_W - 1 - px)) & 1;
                if (on) {
                    for (int s = 0; s < SCALE; s++) {
                        _setValve(bits, x + px * SCALE + s);
                    }
                }
            }
            x += CHAR_W + CHAR_GAP;
        }
    }
};
