#pragma once
#include <Arduino.h>
#include "config.h"

// ============================================================
//  PumpController — giám sát mức nước + điều khiển bơm
//
//  Cảm biến: float switch, INPUT_PULLUP
//    GPIO LOW  = sensor kích hoạt  (low: nước thấp | high: nước đầy)
//    GPIO HIGH = sensor không kích hoạt
//
//  State machine AUTO:
//
//    IDLE ──(low=1)──► PUMPING ──(high=1 | timeout)──► WAIT_RESET
//      ▲                                                      │
//      └─────────────────────(low=0)──────────────────────────┘
//
//    IDLE:       bơm tắt, chờ low=1 (level-triggered, kể cả lúc khởi động)
//    PUMPING:    bơm chạy, dừng khi high=1 hoặc timeout an toàn
//    WAIT_RESET: bơm tắt, chờ low=0 (nước vừa qua cảm biến thấp) trước khi bơm lại
//
//  Manual mode: bơm ON/OFF theo lệnh ngoài, bỏ qua cảm biến
// ============================================================

class PumpController {
public:
    enum Mode      { AUTO, MANUAL };
    enum PumpState { PS_IDLE, PS_PUMPING, PS_WAIT_RESET };

    void begin() {
        pinMode(PIN_LEVEL_LOW,  INPUT_PULLUP);
        pinMode(PIN_LEVEL_HIGH, INPUT_PULLUP);
        pinMode(PIN_PUMP,       OUTPUT);
        digitalWrite(PIN_PUMP, LOW);   // emitter follower: LOW = bơm tắt
        Serial.printf("[PUMP] Init — LowSensor=GPIO%d HighSensor=GPIO%d Relay=GPIO%d\n",
                      PIN_LEVEL_LOW, PIN_LEVEL_HIGH, PIN_PUMP);
    }

    void tick() {
        bool low  = (digitalRead(PIN_LEVEL_LOW)  == LOW);    // NC: LOW=phao xuống=nước thấp=cần bơm
        bool high = (digitalRead(PIN_LEVEL_HIGH) == HIGH);  // NC: HIGH=phao nổi=nước đầy=dừng bơm

        // Debounce 2-sample
        _stableLow  = (low  == _prevLow)  ? low  : _stableLow;
        _stableHigh = (high == _prevHigh) ? high : _stableHigh;
        _prevLow  = low;
        _prevHigh = high;

        bool changed = (_stableLow != _levelLow) || (_stableHigh != _levelHigh);
        _levelLow  = _stableLow;
        _levelHigh = _stableHigh;
        if (changed) _sensorChanged = true;

        if (_mode == AUTO) _autoTick();
    }

    void setPump(bool on) {
        _mode = MANUAL;
        _drive(on);
    }

    void setMode(Mode m) {
        if (_mode == m) return;
        _mode = m;
        Serial.printf("[PUMP] Mode → %s\n", m == AUTO ? "AUTO" : "MANUAL");
        if (m == MANUAL) {
            _drive(false);
        } else {
            // Vào AUTO: reset state, không bơm ngay dù low đang active
            _pumpState = PS_IDLE;
            _drive(false);
            Serial.printf("[PUMP] AUTO reset — state=IDLE low=%d high=%d\n",
                          _levelLow, _levelHigh);
        }
    }

    bool takeChanged() {
        bool c = _pumpChanged || _sensorChanged;
        _pumpChanged = _sensorChanged = false;
        return c;
    }

    // Getters
    bool       pumpOn()     const { return _pumpOn;     }
    bool       levelLow()   const { return _levelLow;   }
    bool       levelHigh()  const { return _levelHigh;  }
    Mode       mode()       const { return _mode;        }
    PumpState  pumpState()  const { return _pumpState;   }
    uint32_t   pumpRunSec() const {
        if (!_pumpOn) return 0;
        return (millis() - _pumpStartMs) / 1000;
    }
    const char* stateStr() const {
        switch (_pumpState) {
            case PS_IDLE:       return "idle";
            case PS_PUMPING:    return "pumping";
            case PS_WAIT_RESET: return "wait_reset";
        }
        return "?";
    }

private:
    // Sensor (debounced)
    bool     _levelLow    = false;
    bool     _levelHigh   = false;
    bool     _prevLow     = false;
    bool     _prevHigh    = false;
    bool     _stableLow   = false;
    bool     _stableHigh  = false;

    // Pump state
    bool      _pumpOn      = false;
    Mode      _mode        = AUTO;
    PumpState _pumpState   = PS_IDLE;
    uint32_t  _pumpStartMs = 0;

    // Change flags for MQTT/WS
    bool _pumpChanged   = false;
    bool _sensorChanged = false;

    // ── State machine AUTO ────────────────────────────────────
    void _autoTick() {
        uint32_t now = millis();

        switch (_pumpState) {

            case PS_IDLE:
                // Bơm ngay khi cảm biến thấp đang kích hoạt (level-triggered)
                // Xử lý cả trường hợp nước đã thấp lúc khởi động
                if (_levelLow) {
                    _drive(true);
                    _pumpState = PS_PUMPING;
                    Serial.println("[PUMP] LOW active → PUMPING");
                }
                break;

            case PS_PUMPING:
                // Dừng khi cảm biến cao kích hoạt hoặc timeout an toàn
                {
                    bool timeout = (now - _pumpStartMs) >= PUMP_TIMEOUT_MS;
                    if (_levelHigh || timeout) {
                        if (timeout) Serial.println("[PUMP] ⚠ Timeout → force stop");
                        else         Serial.println("[PUMP] HIGH ↑ → STOP, wait reset");
                        _drive(false);
                        _pumpState = PS_WAIT_RESET;
                    }
                }
                break;

            case PS_WAIT_RESET:
                // Chờ cảm biến thấp về 0 → cho phép bơm lại lần sau
                if (!_levelLow) {
                    _pumpState = PS_IDLE;
                    Serial.println("[PUMP] LOW ↓ → IDLE (ready)");
                }
                break;
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
