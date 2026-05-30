#pragma once
#include <Arduino.h>
#include "config.h"

// ============================================================
//  PumpController — giám sát mức nước + điều khiển bơm
//
//  Cảm biến: float switch, INPUT_PULLUP
//    PIN_LEVEL_LOW  LOW → mức nước thấp (cần bơm)
//    PIN_LEVEL_HIGH LOW → mức nước đầy  (dừng bơm)
//
//  Auto mode:
//    Bơm ON  khi: levelLow=true  AND levelHigh=false
//    Bơm OFF khi: levelHigh=true OR  timeout (PUMP_TIMEOUT_MS)
//
//  Manual mode:
//    Bơm ON/OFF theo lệnh ngoài, bỏ qua cảm biến
// ============================================================

class PumpController {
public:
    enum Mode { AUTO, MANUAL };

    void begin() {
        pinMode(PIN_LEVEL_LOW,  INPUT_PULLUP);
        pinMode(PIN_LEVEL_HIGH, INPUT_PULLUP);
        pinMode(PIN_PUMP,       OUTPUT);
        digitalWrite(PIN_PUMP, LOW);
        Serial.printf("[PUMP] Init — LowSensor=GPIO%d HighSensor=GPIO%d Relay=GPIO%d\n",
                      PIN_LEVEL_LOW, PIN_LEVEL_HIGH, PIN_PUMP);
    }

    void tick() {
        bool low  = (digitalRead(PIN_LEVEL_LOW)  == LOW);
        bool high = (digitalRead(PIN_LEVEL_HIGH) == LOW);

        // Debounce: chỉ cập nhật khi đọc ổn định 2 lần liên tiếp
        _stableLow  = (low  == _prevLow)  ? low  : _stableLow;
        _stableHigh = (high == _prevHigh) ? high : _stableHigh;
        _prevLow    = low;
        _prevHigh   = high;

        bool changed = (_stableLow != _levelLow) || (_stableHigh != _levelHigh);
        _levelLow  = _stableLow;
        _levelHigh = _stableHigh;
        if (changed) _sensorChanged = true;

        if (_mode == AUTO) _autoTick();
    }

    // Điều khiển thủ công — tự động chuyển sang MANUAL
    void setPump(bool on) {
        _mode = MANUAL;
        _drive(on);
    }

    void setMode(Mode m) {
        if (_mode == m) return;
        _mode = m;
        Serial.printf("[PUMP] Mode → %s\n", m == AUTO ? "AUTO" : "MANUAL");
        if (m == MANUAL) _drive(false);   // an toàn: tắt bơm khi vào manual
    }

    // Kiểm tra và xóa cờ thay đổi (dùng cho MQTT publish)
    bool takeChanged() {
        bool c = _pumpChanged || _sensorChanged;
        _pumpChanged = _sensorChanged = false;
        return c;
    }

    // Getters
    bool   pumpOn()    const { return _pumpOn;    }
    bool   levelLow()  const { return _levelLow;  }
    bool   levelHigh() const { return _levelHigh; }
    Mode   mode()      const { return _mode;       }
    uint32_t pumpRunSec() const {
        if (!_pumpOn) return 0;
        return (millis() - _pumpStartMs) / 1000;
    }

private:
    bool     _pumpOn      = false;
    bool     _levelLow    = false;
    bool     _levelHigh   = false;
    bool     _prevLow     = false;
    bool     _prevHigh    = false;
    bool     _stableLow   = false;
    bool     _stableHigh  = false;
    bool     _pumpChanged   = false;
    bool     _sensorChanged = false;
    Mode     _mode        = AUTO;
    uint32_t _pumpStartMs = 0;

    void _autoTick() {
        uint32_t now = millis();
        if (!_pumpOn) {
            if (_levelLow && !_levelHigh) _drive(true);   // bắt đầu bơm
        } else {
            bool timeout = (now - _pumpStartMs) >= PUMP_TIMEOUT_MS;
            if (_levelHigh || timeout) {
                if (timeout) Serial.println("[PUMP] ⚠ Safety timeout — force stop");
                _drive(false);
            }
        }
    }

    void _drive(bool on) {
        if (_pumpOn == on) return;
        _pumpOn = on;
        digitalWrite(PIN_PUMP, on ? HIGH : LOW);
        if (on) _pumpStartMs = millis();
        _pumpChanged = true;
        Serial.printf("[PUMP] %s\n", on ? "▶ ON" : "■ OFF");
    }
};
