#pragma once
#include <Arduino.h>
#include "config.h"
#include "frame_queue.h"
#include "valve_driver.h"
#include "tcp_server.h"

// ============================================================
//  scheduler.h — Thực thi frame theo timestamp
//
//  Design:
//  - Xử lý frame khi _tcp.streaming() == true
//  - Không phụ thuộc vào trạng thái kết nối client
//  - Gọi _tcp.autoFinish() để tự dừng khi queue rỗng
// ============================================================

class Scheduler {
public:
    Scheduler(FrameQueue& q, ValveDriver& v, TcpServer& tcp)
        : _q(q), _v(v), _tcp(tcp) {}

    void tick() {
        // Auto-finish: kiểm tra khi queue rỗng
        _tcp.autoFinish();

        // Không có gì để làm
        if (!_tcp.streaming()) return;
        if (_q.empty()) return;

        // Thực thi các frame đến hạn
        uint32_t now = millis() - _tcp.t0();

        while (!_q.empty() && _q.peek().ts_ms <= now) {
            const Frame& f = _q.peek();
            _v.write(f.bits);
            _q.pop();
            _lastMs = millis();
            _frameCount++;
        }
    }

    uint32_t lastMs()     const { return _lastMs; }
    uint32_t frameCount() const { return _frameCount; }

private:
    FrameQueue&  _q;
    ValveDriver& _v;
    TcpServer&   _tcp;
    uint32_t     _lastMs     = 0;
    uint32_t     _frameCount = 0;
};
