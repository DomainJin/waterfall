#pragma once
#include <Arduino.h>
#include "config.h"
#include "frame_queue.h"
#include "valve_driver.h"
#include "tcp_server.h"

class Scheduler {
public:
    Scheduler(FrameQueue& q, ValveDriver& v, TcpServer& tcp)
        : _q(q), _v(v), _tcp(tcp) {}

    void tick() {
        if (!_tcp.streaming()) return;
        if (_q.empty()) return;
        
        uint32_t now = millis() - _tcp.t0();
        
        // Process all frames that are ready (non-blocking)
        while (!_q.empty() && _q.peek().ts_ms <= now) {
            Frame f = _q.peek();
            _v.write(f.bits);  // NO logging here — valve write must be fast!
            _q.pop();
            _lastMs = millis();
            // Periodic debug output (every 100th frame or on error)
            if ((_frameCount++ % 100) == 0) {
                Serial.printf("[SCHED] Executed frame ts=%u (count=%u)\n", 
                              f.ts_ms, _frameCount);
            }
        }
    }

    uint32_t lastMs() const { return _lastMs; }

private:
    FrameQueue&  _q;
    ValveDriver& _v;
    TcpServer&   _tcp;
    uint32_t     _lastMs = 0;
    uint32_t     _frameCount = 0;
};
