#pragma once
#include <Arduino.h>
#include "config.h"

struct Frame {
    uint32_t ts_ms;
    uint8_t  bits[NUM_BOARDS];
};

class FrameQueue {
public:
    void     clear()  { _h = _t = 0; }
    bool     empty()  const { return _h == _t; }
    bool     full()   const { return ((_t + 1) % QUEUE_LEN) == _h; }
    uint16_t size()   const { return (_t - _h + QUEUE_LEN) % QUEUE_LEN; }

    bool push(const Frame& f) {
        if (full()) return false;
        _buf[_t] = f;
        _t = (_t + 1) % QUEUE_LEN;
        return true;
    }
    const Frame& peek() const { return _buf[_h]; }
    void pop() { if (!empty()) _h = (_h + 1) % QUEUE_LEN; }

private:
    Frame    _buf[QUEUE_LEN];
    uint16_t _h = 0, _t = 0;
};
