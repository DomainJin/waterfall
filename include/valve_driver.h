#pragma once
#include <Arduino.h>
#include "config.h"

class ValveDriver {
public:
    void begin() {
        pinMode(PIN_SHCP, OUTPUT);
        pinMode(PIN_STCP, OUTPUT);
        pinMode(PIN_DS,   OUTPUT);
        digitalWrite(PIN_SHCP, LOW);
        digitalWrite(PIN_STCP, LOW);
        digitalWrite(PIN_DS,   LOW);
        Serial.printf("[HW] Shift register pins: SHCP=%d STCP=%d DS=%d\n", 
                      PIN_SHCP, PIN_STCP, PIN_DS);
        allOff();
    }

    // Send bits array from frame (1 byte per board)
    // Daisy-chain: shift all bytes first, then latch once
    // ⚠️ NO Serial.printf here — must stay non-blocking!
    void write(const uint8_t* bits) {
        // Shift all boards sequentially (no latch between them)
        for (int i = 0; i < NUM_BOARDS; i++) {
            _shiftByteNoLatch(bits[i]);
        }
        // Latch once for all boards
        _latch();
    }

    // Debug version with logging (use sparingly)
    void writeDebug(const uint8_t* bits) {
        Serial.print("[VALVE] Shifting: ");
        for (int i = 0; i < NUM_BOARDS; i++) {
            Serial.printf("%02x ", bits[i]);
        }
        Serial.println();
        write(bits);
        Serial.println("[VALVE] Latched!");
    }

    void allOff() {
        uint8_t zeros[NUM_BOARDS];
        memset(zeros, 0, NUM_BOARDS);
        write(zeros);
    }

    void allOn() {
        uint8_t ones[NUM_BOARDS];
        memset(ones, 0xFF, NUM_BOARDS);
        write(ones);
    }

private:
    // Shift byte without latch (for daisy-chain)
    // ⚠️ Shift LSB first (bit 0) to match web UI mapping
    void _shiftByteNoLatch(uint8_t val) {
        for (int i = 0; i < 8; i++) {  // LSB -> MSB (i = 0, 1, 2, ..., 7)
            digitalWrite(PIN_DS, (val >> i) & 1);
            delayMicroseconds(10);
            digitalWrite(PIN_SHCP, HIGH);
            delayMicroseconds(10);
            digitalWrite(PIN_SHCP, LOW);
            delayMicroseconds(10);
        }
        digitalWrite(PIN_DS, LOW);
    }

    // Latch pulse
    void _latch() {
        digitalWrite(PIN_STCP, HIGH);
        delayMicroseconds(10);
        digitalWrite(PIN_STCP, LOW);
        delayMicroseconds(10);
    }

    // Single byte quick shift (for test)
    // ⚠️ Shift LSB first to match mapping
    void shiftByte(uint8_t val) {
        for (int i = 0; i < 8; i++) {  // LSB -> MSB
            digitalWrite(PIN_DS, (val >> i) & 1);
            delayMicroseconds(10);
            digitalWrite(PIN_SHCP, HIGH);
            delayMicroseconds(10);
            digitalWrite(PIN_SHCP, LOW);
            delayMicroseconds(10);
        }
        digitalWrite(PIN_STCP, HIGH);
        delayMicroseconds(10);
        digitalWrite(PIN_STCP, LOW);
        delayMicroseconds(10);
        digitalWrite(PIN_DS, LOW);
    }
};

