# Cấu trúc dữ liệu — Waterfall Effect System

> Tài liệu này mô tả đầy đủ định dạng nhị phân để tự viết phần mềm thiết kế hiệu ứng
> và xuất file `.bin` tương thích với ESP32 firmware.

---

## 1. Tổng quan phần cứng

```
[ESP32] ──GPIO──► [74HC245D] ──► [74HC595 #1] ──Q7'──► [74HC595 #2] ──► ... ──► [74HC595 #10]
                                      │                      │                          │
                                   Van 1–8               Van 9–16                  Van 73–80
```

| Thông số | Giá trị |
|---|---|
| Số van | 80 |
| Số board (74HC595) | 10 |
| Van mỗi board | 8 |
| Frame size | **14 bytes** = 4 (timestamp) + 10 (bits) |
| Queue size | 512 frame |
| Giao tiếp | WebSocket port **3333** |
| Timestamp resolution | **1 ms** |
| Shift order | MSB first (bit 7 ra trước) |

---

## 2. Đánh số van (valve numbering)

```
Van số:  1   2   3   4   5   6   7   8 | 9  10  11  ...  16 | ... | 73  74  ...  80
         ←────────── byte[0] ──────────► ←──── byte[1] ──────►     ←──── byte[9] ──►
Bit:     7   6   5   4   3   2   1   0   7   6   5  ...   0         7   6   ...   0
```

**Công thức (0-indexed, van đầu = van số 0):**

```
byte_index = valve >> 3          // chia 8, lấy nguyên
bit_index  = 7 - (valve & 7)    // bit cao nhất = van đầu của mỗi board
```

**Đặt bit cho van v trong mảng bytes:**

```
buf[v >> 3] |= (1 << (7 - (v & 7)))
```

**Ví dụ cụ thể:**

| Van (0-idx) | Van (1-idx) | byte | bit | mask hex |
|---|---|---|---|---|
| 0 | 1 | 0 | 7 | `byte[0] = 0x80` |
| 7 | 8 | 0 | 0 | `byte[0] = 0x01` |
| 8 | 9 | 1 | 7 | `byte[1] = 0x80` |
| 39 | 40 | 4 | 7 | `byte[4] = 0x80` |
| 79 | 80 | 9 | 0 | `byte[9] = 0x01` |

**Mở van 1, 5, 9 (0-indexed: 0, 4, 8):**
```
byte[0] = (1<<7) | (1<<3) = 0b10001000 = 0x88
byte[1] = (1<<7)          = 0b10000000 = 0x80
byte[2..9] = 0x00
```

---

## 3. Định dạng frame nhị phân

Mỗi frame = **14 bytes** liên tiếp:

```
Byte  0   1   2   3   4   5   6   7   8   9  10  11  12  13
     ├───────────────┤├───────────────────────────────────────┤
       ts_ms (LE u32)          bits[10]
```

| Offset | Size | Kiểu | Endian | Ý nghĩa |
|---|---|---|---|---|
| 0 | 4 | uint32 | Little-Endian | `ts_ms` — thời điểm thực thi (ms từ TS_START) |
| 4 | 10 | uint8[10] | — | `bits` — trạng thái 80 van |

### Giá trị ts_ms đặc biệt (reserved)

| ts_ms | Hex (LE bytes) | Ý nghĩa |
|---|---|---|
| `0xFFFFFFFF` | `FF FF FF FF` | **TS_RESET** — xóa queue, tắt tất cả van |
| `0xFFFFFFFE` | `FE FF FF FF` | **TS_START** — bắt đầu đồng hồ, scheduler chạy |
| `0x00000000` – `0xFFFFFFFD` | — | Frame dữ liệu thông thường |

---

## 4. Giao thức stream — thứ tự bắt buộc

```
① [TS_RESET]              xóa state cũ, tắt van
② [TS_START]              t0 = millis(), scheduler bắt đầu
③ [Frame ts=0    bits=…]  các frame dữ liệu theo thứ tự ts tăng dần
④ [Frame ts=80   bits=…]
   ...
⑤ [Frame ts=N    bits=0]  sentinel cuối — tắt tất cả van
```

> **Quan trọng:** Gửi toàn bộ luồng binary liên tục, không delay giữa các byte.
> ESP32 nhận vào buffer, parse đủ 14 byte thì xử lý ngay.

### Cấu trúc bytes của từng frame điều khiển

**TS_RESET:**
```
FF FF FF FF   00 00 00 00 00 00 00 00 00 00
```

**TS_START:**
```
FE FF FF FF   00 00 00 00 00 00 00 00 00 00
```

**Frame dữ liệu tại t=160ms, tất cả van board 1 ON:**
```
A0 00 00 00   FF 00 00 00 00 00 00 00 00 00
↑ 160 = 0xA0  ↑ bits[0]=0xFF = van 1-8 ON
```

---

## 5. Cấu trúc file `.bin`

File `.bin` = **nối tiếp các frame 14-byte**, không header, không padding.

```
┌─────────────────────────────────────────┐
│ Frame  0 : TS_RESET  (14 bytes)          │
│ Frame  1 : TS_START  (14 bytes)          │
│ Frame  2 : ts=0      (14 bytes)          │
│ Frame  3 : ts=Δt     (14 bytes)          │
│ ...                                      │
│ Frame  N : ts=T_max  (14 bytes) bits=0   │  ← sentinel all-off
└─────────────────────────────────────────┘
Total = (N + 1) × 14 bytes
```

**Kích thước điển hình:**
- 100 rows × 80ms/row → 103 frames → **1442 bytes**
- 512 frames tối đa trong queue → gửi nhiều đợt nếu animation dài hơn

---

## 6. Code mẫu — xây dựng file `.bin`

### Python

```python
import struct

NUM_BOARDS = 10
FRAME_SIZE = 14
TS_RESET   = 0xFFFFFFFF
TS_START   = 0xFFFFFFFE

def pack_frame(ts_ms: int, bits: bytes) -> bytes:
    return struct.pack('<I', ts_ms) + bits

def valve_bits(valves_0indexed: list) -> bytes:
    """valves_0indexed: danh sách van muốn mở (0..79)"""
    buf = bytearray(NUM_BOARDS)
    for v in valves_0indexed:
        buf[v >> 3] |= (1 << (7 - (v & 7)))
    return bytes(buf)

def build_animation(rows: list, row_interval_ms: int = 80) -> bytes:
    """
    rows: list of list[int] — mỗi phần tử là list số van mở (0-indexed)
    Ví dụ: rows = [[0,1,2], [3,4,5], ...]
    """
    stream = bytearray()
    stream += pack_frame(TS_RESET, b'\x00' * NUM_BOARDS)
    stream += pack_frame(TS_START, b'\x00' * NUM_BOARDS)
    for i, open_valves in enumerate(rows):
        ts = i * row_interval_ms
        stream += pack_frame(ts, valve_bits(open_valves))
    # sentinel: tắt tất cả
    stream += pack_frame(len(rows) * row_interval_ms, b'\x00' * NUM_BOARDS)
    return bytes(stream)

# ── Ví dụ: đường chéo từ van 0 đến van 79 ──
rows = [[i] for i in range(80)]           # mỗi row mở 1 van
animation = build_animation(rows, row_interval_ms=80)

with open('diagonal.bin', 'wb') as f:
    f.write(animation)

print(f"File size: {len(animation)} bytes, {len(animation)//14} frames")
```

### Python — Sub-frame smooth timing (đường chéo mịn)

```python
def build_smooth_diagonal(num_valves=80, row_ms=80, num_rows=32) -> bytes:
    """
    Mỗi van mở tại timestamp riêng (ms chính xác) thay vì đồng loạt.
    Kết quả: đường chéo hoàn toàn mịn, không bậc thang Bresenham.
    """
    slope = num_rows / num_valves   # rows per valve step

    events = []
    for v in range(num_valves):
        t_open  = round(v * slope * row_ms)
        t_close = t_open + row_ms
        events.append((t_open,  v, True ))
        events.append((t_close, v, False))

    # Sort: cùng ts thì open trước close
    events.sort(key=lambda e: (e[0], not e[2]))

    state  = bytearray(NUM_BOARDS)
    stream = bytearray()
    stream += pack_frame(TS_RESET, b'\x00' * NUM_BOARDS)
    stream += pack_frame(TS_START, b'\x00' * NUM_BOARDS)

    i = 0
    while i < len(events):
        t = events[i][0]
        while i < len(events) and events[i][0] == t:
            _, valve, is_open = events[i]
            b, bit = valve >> 3, 7 - (valve & 7)
            if is_open: state[b] |=  (1 << bit)
            else:       state[b] &= ~(1 << bit)
            i += 1
        stream += pack_frame(t, bytes(state))

    # sentinel
    stream += pack_frame(t + row_ms, b'\x00' * NUM_BOARDS)
    return bytes(stream)
```

### JavaScript / Node.js

```javascript
const NUM_BOARDS = 10;
const TS_RESET   = 0xFFFFFFFF;
const TS_START   = 0xFFFFFFFE;

function packFrame(ts_ms, bits) {
    const buf = Buffer.alloc(14);
    buf.writeUInt32LE(ts_ms >>> 0, 0);  // >>> 0 đảm bảo unsigned
    Buffer.from(bits).copy(buf, 4);
    return buf;
}

function valveBits(valves) {
    const buf = Buffer.alloc(NUM_BOARDS);
    for (const v of valves) buf[v >> 3] |= (1 << (7 - (v & 7)));
    return buf;
}

function buildAnimation(rows, rowIntervalMs = 80) {
    const chunks = [];
    chunks.push(packFrame(TS_RESET, Buffer.alloc(10)));
    chunks.push(packFrame(TS_START, Buffer.alloc(10)));
    rows.forEach((openValves, i) => {
        chunks.push(packFrame(i * rowIntervalMs, valveBits(openValves)));
    });
    chunks.push(packFrame(rows.length * rowIntervalMs, Buffer.alloc(10)));
    return Buffer.concat(chunks);
}

// Ví dụ
const rows = Array.from({length: 80}, (_, i) => [i]);
const bin = buildAnimation(rows, 80);
require('fs').writeFileSync('diagonal.bin', bin);
```

---

## 7. Gửi file qua WebSocket

### Python

```python
import websocket

def send_bin(ip: str, filepath: str, port: int = 3333):
    ws = websocket.WebSocket()
    ws.connect(f'ws://{ip}:{port}')
    with open(filepath, 'rb') as f:
        ws.send_binary(f.read())
    ws.close()

send_bin('192.168.1.222', 'diagonal.bin')
```

### Python — gửi nhiều đợt (animation dài hơn 512 frames)

```python
CHUNK_FRAMES = 400   # gửi 400 frame mỗi lần (< 512 queue limit)

def send_chunked(ip: str, frames_data: list, row_ms: int):
    ws = websocket.WebSocket()
    ws.connect(f'ws://{ip}:3333')

    # Gửi RESET + START một lần duy nhất
    ws.send_binary(
        pack_frame(TS_RESET, b'\x00'*10) +
        pack_frame(TS_START, b'\x00'*10)
    )

    for i in range(0, len(frames_data), CHUNK_FRAMES):
        chunk = frames_data[i:i+CHUNK_FRAMES]
        payload = bytearray()
        for row_idx, open_valves in enumerate(chunk):
            ts = (i + row_idx) * row_ms
            payload += pack_frame(ts, valve_bits(open_valves))
        ws.send_binary(bytes(payload))
        time.sleep(CHUNK_FRAMES * row_ms / 1000 * 0.8)  # chờ ESP32 drain

    # Sentinel
    ws.send_binary(pack_frame(len(frames_data)*row_ms, b'\x00'*10))
    ws.close()
```

---

## 8. Lệnh JSON (text command)

Ngoài binary frame, ESP32 cũng nhận WebSocket text (JSON):

```json
{"cmd":"ALL_OFF"}
{"cmd":"ALL_ON"}
{"cmd":"STREAM_STOP"}
{"cmd":"SET","bits":"FF00FF00FF00FF00FF00"}
{"cmd":"SET_MODE","mode":"effect","pattern":"rain","sensitivity":50}
{"cmd":"SET_MODE","mode":"effect","pattern":"heart","sensitivity":50}
{"cmd":"SET_MODE","mode":"effect","pattern":"star","sensitivity":50}
{"cmd":"SET_MODE","mode":"effect","pattern":"wave","sensitivity":60}
{"cmd":"SET_MODE","mode":"effect","pattern":"script","sensitivity":50}
{"cmd":"SET_MODE","mode":"clock"}
{"cmd":"SET_MODE","mode":"sound","pattern":"ripple","sensitivity":70}
{"cmd":"SET_MODE","mode":"text","pattern":"HELLO","sensitivity":80}
{"cmd":"SET_MODE","mode":"stream"}
```

`"bits"` trong lệnh SET = chuỗi hex 20 ký tự (10 bytes), không có 0x prefix.

---

## 9. Giới hạn và ràng buộc

| Thông số | Giới hạn | Ghi chú |
|---|---|---|
| Frame size | **14 bytes cố định** | 4 ts + 10 bits |
| Queue size | **512 frames** | Vượt → frame bị drop |
| ts_ms range | `0` – `0xFFFFFFFD` | 2 giá trị cao nhất reserved |
| ts_ms endian | **Little-Endian** | `struct.pack('<I', ...)` |
| Timestamp resolution | **1 ms** | `millis()` ESP32 |
| Shift order | **MSB first** | bit 7 = van đầu mỗi board |
| Auto-finish | Sau 2s queue rỗng | ESP32 tự tắt van |
| WebSocket | Binary (opcode 0x2) | RFC 6455 |
| Port | **3333** | |

---

## 10. Checklist khi viết phần mềm

```
☐ Timestamp Little-Endian (struct.pack('<I', ts))
☐ TS_RESET = 0xFFFFFFFF  →  bytes: [FF, FF, FF, FF]
☐ TS_START = 0xFFFFFFFE  →  bytes: [FE, FF, FF, FF]
☐ Thứ tự: RESET trước, START sau, data frames cuối
☐ Van đánh số từ 0 (hoặc 1 — nhất quán trong code)
☐ bit 7 của byte[0] = van 0 (van số 1 theo 1-indexed)
☐ Gửi dưới dạng WebSocket binary message (không phải text)
☐ Sentinel frame (bits=0) ở cuối để tắt van
☐ Không gửi quá 512 frame mà không chờ drain
```

---

## 11. Sơ đồ luồng phần mềm thiết kế

```
[Editor — ma trận 80×N bool]
         │
         ▼ chọn mode
  ┌──────────────┐      ┌────────────────────────┐
  │  Grid mode   │      │    Smooth mode          │
  │  ts = row×ms │      │  ts = linearFit(col)×ms │
  └──────┬───────┘      └──────────┬──────────────┘
         │                         │
         └──────────┬──────────────┘
                    ▼
         [Pack bits: buf[v>>3] |= 1<<(7-(v&7))]
                    │
                    ▼
         [Build stream: RESET + START + frames + sentinel]
                    │
            ┌───────┴────────┐
            ▼                ▼
      [Xuất .bin]     [Gửi WebSocket ws://IP:3333]
```
