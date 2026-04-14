#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include "config.h"
#include "frame_queue.h"
#include "valve_driver.h"

// ============================================================
//  tcp_server.h — WebSocket Server trực tiếp trên ESP32
//  Dùng static callback thay vì lambda để tránh crash
// ============================================================

class TcpServer {
public:
    TcpServer(FrameQueue& q, ValveDriver& v)
        : _q(q), _v(v), _ws(TCP_PORT) {
        _instance = this;  // global pointer cho static callback
    }

    void begin() {
        _ws.begin();
        _ws.onEvent(TcpServer::_staticEvent);
        Serial.printf("[WS] WebSocket server on port %d\n", TCP_PORT);
    }

    // Gọi trong loop() — xử lý WebSocket events
    void tick() {
        _ws.loop();
    }

    bool     streaming() const { return _streaming; }
    uint32_t t0()        const { return _t0; }

    void reset() {
        _streaming = false;
        _rxLen     = 0;
        _q.clear();
        _v.allOff();
    }

private:
    WebSocketsServer _ws;
    FrameQueue&      _q;
    ValveDriver&     _v;
    bool             _streaming = false;
    uint32_t         _t0        = 0;

    // Receive buffer — đủ lớn cho burst frames
    static const uint16_t RX_BUF_SIZE = FRAME_BYTES * 64;
    uint8_t  _rx[RX_BUF_SIZE];
    uint16_t _rxLen = 0;

    // Static instance pointer để dùng trong static callback
    static TcpServer* _instance;

    // Handle JSON text command from WebSocket (LAN path)
    // {"cmd":"ALL_OFF"} / {"cmd":"ALL_ON"} / {"cmd":"SET","bits":"FF00..."} / {"cmd":"STREAM_STOP"}
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
            // {"cmd":"SET","bits":"AABB..."} — NUM_BOARDS*2 hex chars
            int idx = json.indexOf("\"bits\":\"");
            if (idx < 0) return;
            int start = idx + 8;
            int end   = json.indexOf('"', start);
            if (end <= start) return;
            String hex = json.substring(start, end);
            if ((int)hex.length() < NUM_BOARDS * 2) return;
            uint8_t bits[NUM_BOARDS];
            for (int i = 0; i < NUM_BOARDS; i++) {
                bits[i] = (uint8_t)strtol(hex.substring(i*2, i*2+2).c_str(), nullptr, 16);
            }
            _v.write(bits);
            Serial.printf("[WS] CMD SET %s\n", hex.c_str());
        }
    }

    // Static callback — không capture bất kỳ thứ gì
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
                reset();
                break;

            case WStype_DISCONNECTED:
                Serial.printf("[WS] Client #%d disconnected\n", num);
                reset();
                break;

            case WStype_TEXT: {
                String json((char*)payload, len);
                _handleTextCmd(json);
                break;
            }

            case WStype_BIN: {
                // Append vào rx buffer
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

            default:
                break;
        }
    }

    void _parse() {
        while (_rxLen >= FRAME_BYTES) {
            // Đọc timestamp little-endian
            uint32_t ts = (uint32_t)_rx[0]
                        | (uint32_t)_rx[1] << 8
                        | (uint32_t)_rx[2] << 16
                        | (uint32_t)_rx[3] << 24;

            if (ts == TS_RESET) {
                Serial.println("[CMD] Reset");
                // Reset TRƯỚC khi shift buffer để tránh re-entry
                _rxLen -= FRAME_BYTES;
                memmove(_rx, _rx + FRAME_BYTES, _rxLen);
                reset();  // gọi sau khi đã shift
                return;   // thoát luôn, tránh tiếp tục parse

            } else if (ts == TS_START) {
                _t0 = millis();
                _streaming = true;
                Serial.printf("[CMD] Stream start t0=%u ms\n", _t0);

            } else {
                // Data frame
                Frame f;
                f.ts_ms = ts;
                memcpy(f.bits, _rx + 4, NUM_BOARDS);
                if (!_q.push(f)) {
                    Serial.println("[WARN] Queue full — frame dropped");
                } else {
                    Serial.printf("[FRAME] Queued ts=%u bits=[", ts);
                    for (int i = 0; i < NUM_BOARDS; i++) {
                        Serial.printf("%02x ", f.bits[i]);
                    }
                    Serial.println("]");
                }
            }

            _rxLen -= FRAME_BYTES;
            memmove(_rx, _rx + FRAME_BYTES, _rxLen);
        }
    }
};

// Định nghĩa static member
TcpServer* TcpServer::_instance = nullptr;
