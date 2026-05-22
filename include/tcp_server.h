#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <functional>
#include "config.h"
#include "frame_queue.h"
#include "valve_driver.h"

// ============================================================
//  tcp_server.h — WebSocket Server trực tiếp trên ESP32
//
//  Flow control:
//    CONNECTED   → reset() (clear old state, new session)
//    TS_RESET    → reset() (explicit clear command)
//    TS_START    → _streaming = true, record _t0
//    Data frames → pushed to queue
//    DISCONNECTED → **KHÔNG reset** — scheduler tiếp tục drain queue
//    Auto-finish → scheduler gọi autoFinish() khi queue rỗng
// ============================================================

class TcpServer {
public:
    TcpServer(FrameQueue& q, ValveDriver& v)
        : _q(q), _v(v), _ws(TCP_PORT) {
        _instance = this;
    }

    void begin() {
        _ws.begin();
        _ws.onEvent(TcpServer::_staticEvent);
        Serial.printf("[WS] WebSocket server on port %d\n", TCP_PORT);
    }

    // Register callback for SET_MODE commands: cb(mode, pattern, sensitivity)
    void onModeChange(std::function<void(const String&, const String&, int)> cb) {
        _onModeChange = cb;
    }

    void tick() { _ws.loop(); }

    bool     streaming() const { return _streaming; }
    bool     hasClient() const { return _hasClient; }
    uint32_t t0()        const { return _t0; }

    // Hard reset — gọi khi client mới kết nối hoặc nhận TS_RESET
    void reset() {
        _streaming  = false;
        _rxLen      = 0;
        _drainStart = 0;
        _q.clear();
        _v.allOff();
        Serial.println("[WS] State reset");
    }

    // Gọi từ main loop sau khi scheduler drain xong queue
    // Dừng stream và tắt van nếu queue rỗng đủ lâu
    void autoFinish() {
        if (!_streaming) return;
        if (!_q.empty()) { _drainStart = 0; return; }

        // Queue vừa trống — bắt đầu đếm
        if (_drainStart == 0) { _drainStart = millis(); return; }

        // Đợi 2s sau khi queue rỗng → xác nhận animation xong
        if (millis() - _drainStart >= 2000) {
            Serial.println("[WS] Auto-finish: queue drained, stream complete");
            _streaming  = false;
            _drainStart = 0;
            _v.allOff();
        }
    }

private:
    WebSocketsServer _ws;
    FrameQueue&      _q;
    ValveDriver&     _v;
    bool             _streaming  = false;
    bool             _hasClient  = false;
    uint32_t         _t0         = 0;
    uint32_t         _drainStart = 0;
    std::function<void(const String&, const String&, int)> _onModeChange;

    static const uint16_t RX_BUF_SIZE = FRAME_BYTES * 64;
    uint8_t  _rx[RX_BUF_SIZE];
    uint16_t _rxLen = 0;

    static TcpServer* _instance;

    // ── Text JSON commands (LAN) ──────────────────────────────────
    void _handleTextCmd(const String& json) {
        if (json.indexOf("ALL_OFF") >= 0) {
            _v.allOff();
            Serial.println("[WS] CMD ALL_OFF");
        } else if (json.indexOf("ALL_ON") >= 0) {
            _v.allOn();
            Serial.println("[WS] CMD ALL_ON");
        } else if (json.indexOf("STREAM_STOP") >= 0) {
            reset();
            Serial.println("[WS] CMD STREAM_STOP");
        } else if (json.indexOf("\"SET\"") >= 0) {
            // Robust parse: handle "bits":"..." và "bits": "..." (Python adds space)
            int idx = json.indexOf("\"bits\":");
            if (idx < 0) return;
            int q1 = json.indexOf('"', idx + 7);   // quote mở của value
            if (q1 < 0) return;
            int q2 = json.indexOf('"', q1 + 1);    // quote đóng
            if (q2 <= q1) return;
            String hex = json.substring(q1 + 1, q2);
            if ((int)hex.length() < NUM_BOARDS * 2) return;
            uint8_t bits[NUM_BOARDS];
            for (int i = 0; i < NUM_BOARDS; i++) {
                bits[i] = (uint8_t)strtol(hex.substring(i*2, i*2+2).c_str(), nullptr, 16);
            }
            _v.write(bits);
            Serial.printf("[WS] CMD SET %s\n", hex.c_str());
        } else if (json.indexOf("SET_MODE") >= 0) {
            // {"cmd":"SET_MODE","mode":"sound","pattern":"ripple","sensitivity":50}
            // {"cmd":"SET_MODE","mode":"stream"}
            if (!_onModeChange) return;
            // parse mode
            String mode = _parseStrField(json, "mode");
            String pattern = _parseStrField(json, "pattern");
            int sensitivity = 50;
            int sidx = json.indexOf("\"sensitivity\":");
            if (sidx >= 0) sensitivity = json.substring(sidx + 14).toInt();
            Serial.printf("[WS] CMD SET_MODE mode=%s pattern=%s sens=%d\n",
                          mode.c_str(), pattern.c_str(), sensitivity);
            _onModeChange(mode, pattern, sensitivity);
        }
    }

    // Extract string value for a JSON key: "key":"value"
    String _parseStrField(const String& json, const char* key) {
        String searchKey = String("\"") + key + "\":\"";
        int idx = json.indexOf(searchKey);
        if (idx < 0) return "";
        int start = idx + searchKey.length();
        int end   = json.indexOf('"', start);
        if (end <= start) return "";
        return json.substring(start, end);
    }

    static void _staticEvent(uint8_t num, WStype_t type,
                              uint8_t* payload, size_t len) {
        if (_instance) _instance->_onEvent(num, type, payload, len);
    }

    void _onEvent(uint8_t num, WStype_t type,
                  uint8_t* payload, size_t len) {
        switch (type) {

            case WStype_CONNECTED:
                Serial.printf("[WS] Client #%d connected from %s\n",
                              num, _ws.remoteIP(num).toString().c_str());
                _hasClient = true;
                reset();   // Clear previous session on NEW connection
                break;

            case WStype_DISCONNECTED:
                Serial.printf("[WS] Client #%d disconnected\n", num);
                _hasClient = false;
                // ⚠ KHÔNG gọi reset() — scheduler tiếp tục drain queue
                // autoFinish() trong main loop sẽ tắt sau khi queue rỗng
                if (_q.empty()) {
                    _streaming = false;
                    _v.allOff();
                }
                break;

            case WStype_TEXT: {
                String json((char*)payload, len);
                _handleTextCmd(json);
                break;
            }

            case WStype_BIN: {
                if ((uint32_t)_rxLen + len > RX_BUF_SIZE) {
                    Serial.println("[WS] RX overflow — clearing");
                    _rxLen = 0;
                }
                memcpy(_rx + _rxLen, payload, len);
                _rxLen += len;
                _parse();
                break;
            }

            case WStype_ERROR:
                Serial.printf("[WS] Error on client #%d\n", num);
                break;

            default: break;
        }
    }

    void _parse() {
        while (_rxLen >= FRAME_BYTES) {
            uint32_t ts = (uint32_t)_rx[0]
                        | (uint32_t)_rx[1] << 8
                        | (uint32_t)_rx[2] << 16
                        | (uint32_t)_rx[3] << 24;

            if (ts == TS_RESET) {
                _rxLen -= FRAME_BYTES;
                memmove(_rx, _rx + FRAME_BYTES, _rxLen);
                reset();
                Serial.println("[CMD] TS_RESET received");
                return;

            } else if (ts == TS_START) {
                _t0         = millis();
                _streaming  = true;
                _drainStart = 0;
                Serial.printf("[CMD] TS_START — t0=%u ms\n", _t0);

            } else {
                Frame f;
                f.ts_ms = ts;
                memcpy(f.bits, _rx + 4, NUM_BOARDS);
                if (!_q.push(f)) {
                    Serial.println("[WARN] Queue full — frame dropped");
                }
                // ⚠ Không log từng frame — quá chậm (Serial.printf block CPU)
            }

            _rxLen -= FRAME_BYTES;
            memmove(_rx, _rx + FRAME_BYTES, _rxLen);
        }
    }
};

TcpServer* TcpServer::_instance = nullptr;
