# CLAUDE.md — Water Curtain Control System

## Tổng quan dự án

Hệ thống điều khiển **80 van solenoid** tạo hiệu ứng rèm nước có thể lập trình.
ESP32 nhận frame animation qua WebSocket, điều khiển van qua shift register 74HC595 (daisy-chain).

---

## Hardware

| Thành phần | Chi tiết |
|---|---|
| Vi điều khiển | ESP32 DevKit v1 |
| Van | 80 solenoid, điều khiển qua 10× 74HC595 |
| Chân GPIO | `PIN_SHCP=2` (clock), `PIN_STCP=4` (latch), `PIN_DS=16` (data) |
| SD Card | SPI: MOSI=23, MISO=19, CLK=18, CS=5 — FAT32, tốc độ 400kHz |
| Micro | GPIO34 (ADC input, sound mode) |

Dây dẫn: ESP32 → 74HC245 transceiver → 74HC595 × 10 (daisy-chain) → 80 van.

---

## Cấu trúc firmware (`include/`)

| File | Chức năng |
|---|---|
| `config.h` | Tất cả hằng số: GPIO, WiFi, MQTT, `FW_VERSION`, `NUM_BOARDS=10` |
| `valve_driver.h` | Ghi bytes ra shift register qua SPI |
| `frame_queue.h` | Circular buffer 512 frame |
| `scheduler.h` | Đọc queue, gửi valve theo timestamp |
| `tcp_server.h` | WebSocket server port 3333, nhận frame binary + lệnh JSON |
| `sound_mode.h` | ADC microphone → pattern (ripple/columns/wave) |
| `clock_mode.h` | Hiển thị giờ theo hàng (7 pixel font) |
| `text_mode.h` | Scroll/display text theo hàng |
| `effect_mode.h` | Hiệu ứng liên tục: rain/wave/chase/pulse/heart/star/music/diamond/script |
| `ota_server.h` | HTTP port 8080: `POST /update` (OTA), `GET /version` |
| `sd_manager.h` | Đọc/ghi SD card |
| `config_server.h` | UDP port 8888: GET_INFO, SET_IP, SET_PORT, RESET, GET_STORAGE, LIST_FILES |
| `mqtt_manager.h` | PubSubClient kết nối MQTT cloud |
| `ble_config.h` | BLE config ("Waterfall_Config") |

### Loop priority (`src/main.cpp`)

```
1. Valve control  — sound/clock/text/effect/stream tick
2. WebSocket      — g_tcp.tick()
3. UDP Config     — g_cfg.tick()
4. BLE            — g_ble.tick()
5. MQTT           — g_mqtt.tick()
6. OTA HTTP       — g_ota.tick()
```

### Device modes (enum `DeviceMode`)

```
MODE_STREAM  — nhận frame WebSocket binary
MODE_SOUND   — microphone → valve pattern
MODE_CLOCK   — hiển thị giờ
MODE_TEXT    — hiển thị text
MODE_EFFECT  — hiệu ứng tự động (rain/wave/chase/pulse)
```

---

## Protocol

### Frame binary (WebSocket LAN — 14 bytes/frame)

```
[ts_ms : 4 bytes LE] [bits : 10 bytes, 1 bit/van, MSB-first]

TS_RESET = 0xFFFFFFFF  → xóa queue, tắt van
TS_START = 0xFFFFFFFE  → bắt đầu phát
```

Bit mapping: `byte[i]` = van `i*8` đến `i*8+7`, bit 7 = van đầu tiên trong nhóm.

### Lệnh JSON (WebSocket text hoặc MQTT)

```json
{"cmd":"ALL_OFF"}
{"cmd":"ALL_ON"}
{"cmd":"SET","bits":"FF00FF00FF00FF00FF00"}
{"cmd":"SET_MODE","mode":"clock","sensitivity":50,"gapMs":1000}
{"cmd":"SET_MODE","mode":"effect","pattern":"rain","sensitivity":70}
{"cmd":"SET_MODE","mode":"effect","pattern":"heart","sensitivity":50}
{"cmd":"SET_MODE","mode":"effect","pattern":"star","sensitivity":50}
{"cmd":"SET_MODE","mode":"effect","pattern":"music","sensitivity":50}
{"cmd":"SET_MODE","mode":"effect","pattern":"diamond","sensitivity":50}
{"cmd":"SET_MODE","mode":"effect","pattern":"script","sensitivity":50}
```

Trường `"target":"waterfall-b87524"` để chỉ định thiết bị cụ thể (MQTT).

### MQTT Topics

| Topic | Chiều | Nội dung |
|---|---|---|
| `waterfall/status` | ESP32 → Cloud | `{"online":true,"name":"...","ip":"..."}` |
| `waterfall/cmd/valve` | Cloud → ESP32 | JSON lệnh điều khiển |
| `waterfall/cmd/stream` | Cloud → ESP32 | `{"frame":"hex14bytes"}` |

---

## Ports

| Port | Giao thức | Chức năng |
|---|---|---|
| 3333 | WebSocket | Stream frame binary + lệnh JSON (LAN) |
| 8080 | HTTP | OTA upload (`POST /update`), version check (`GET /version`) |
| 8888 | UDP | Config: GET_INFO, SET_IP, SET_PORT, RESET, GET_STORAGE, LIST_FILES |
| 1883 | MQTT | Cloud relay (plain — chưa dùng TLS 8883) |

---

## Web UI (`control.html`)

File chính. Chạy trực tiếp trên browser (file:// hoặc http://).

**Kết nối:**
- LAN (http://): WebSocket ws://ESP_IP:3333
- Cloud (https://): MQTT qua backend `/api/cmd`

**Mode panel:** Stream | Sound | Clock | Text | Effects

**Simulation:**
- `liveColIv[col]`: array các interval `{vOpen, vClose}` cho mỗi cột van
- `liveTick()`: render từng frame — group theo `vOpen` (mỗi font row là 1 layer)
- `_renderRuns()`: merge các cột liền kề cùng `vOpen`, vẽ stream body + drop
- `startEffectLocalSim(name, speed)`: chạy hiệu ứng local không cần ESP

**OTA (Admin section):**
- Mở bằng password `waterfall` (chỉ UI)
- Upload `.bin` tới `http://<ip>:8080/update`
- Sau flash: tự poll `GET /version` mỗi 2s (tối đa 6 lần) để xác nhận version mới

**Firmware version badge:**
- Hiện trên thanh status bar: `FW 1.1.0`
- Tự fetch khi click Connect, hoặc click badge để refresh
- Nguồn: `GET http://<ip>:8080/version`

---

## Home page (`home.html`)

- Hiện danh sách ESP32 từ MQTT (`/api/devices`)
- Auto-refresh 5s
- Mỗi card online: tự fetch `GET /version` và hiện firmware version
- localStorage lưu devices đã thấy (persistent khi MQTT offline)

---

## Firmware version

Định nghĩa trong `include/config.h`:

```c
#define FW_VERSION "1.1.0"
```

**Tăng số này trước mỗi lần build** để xác nhận OTA thành công.  
Sau OTA: web tự poll version → badge hiện version mới → xác nhận.

---

## Build & OTA (không dùng USB)

```bash
# PlatformIO: build env esp32
pio run -e esp32
# Output: .pio/build/esp32/firmware.bin (cũng copy ra firmware.bin gốc qua copy_firmware.py)

# Upload qua web:
# 1. Mở control.html → Admin → Unlock (password: waterfall)
# 2. Chọn firmware.bin → Upload & Flash
# 3. Đợi ESP restart (~5s) → badge FW tự cập nhật
```

Partition table: `min_spiffs.csv` (dual OTA app partition — bắt buộc).

---

## Cloud Deploy

- **VPS**: DigitalOcean Singapore, 512MB RAM, Ubuntu 22.04
- **IP**: 139.59.107.54
- **Domain**: waterfall.domainjin.io.vn
- **Stack**: Docker Compose — Nginx + Flask (Gunicorn 1 worker) + Mosquitto

```bash
# Redeploy sau update
ssh root@139.59.107.54
cd ~/waterfall && git pull
cd deploy && docker compose build --no-cache backend && docker compose up -d backend
```

**Backend API** (`backend.py` / Flask):

| Endpoint | Method | Mô tả |
|---|---|---|
| `/api/devices` | GET | Danh sách ESP32 online từ MQTT |
| `/api/cmd` | POST | Relay lệnh → MQTT → ESP32 |
| `/api/mqtt/status` | GET | Trạng thái MQTT broker |

---

## Lỗi đã biết / TODO

- ESP32 TLS port 8883 chưa hoạt động → đang dùng plain 1883
- Streaming animation chỉ qua LAN (WebSocket binary), không qua MQTT
- SD card SPI speed 400kHz (có thể tăng lên 1-4MHz khi ổn định)
- MQTT Will Message xử lý offline detection (không dùng timeout thủ công)

---

## Cấu trúc file dự án

```
waterfall/
├── src/main.cpp              # Entry point, loop, mode switching
├── include/
│   ├── config.h              # ← SỬA ĐỔI FW_VERSION Ở ĐÂY
│   ├── valve_driver.h
│   ├── tcp_server.h
│   ├── scheduler.h
│   ├── frame_queue.h
│   ├── sound_mode.h
│   ├── clock_mode.h
│   ├── text_mode.h
│   ├── effect_mode.h         # rain/wave/chase/pulse
│   ├── ota_server.h          # HTTP port 8080
│   ├── sd_manager.h
│   ├── config_server.h       # UDP port 8888
│   ├── mqtt_manager.h
│   └── ble_config.h
├── control.html              # Main web UI (simulation + control)
├── home.html                 # Device list + quick connect
├── platformio.ini            # board=esp32doit-devkit-v1, partitions=min_spiffs.csv
├── deploy/                   # Docker Compose cho VPS
│   ├── docker-compose.yml
│   ├── Dockerfile
│   ├── nginx/default.conf
│   └── mosquitto/
└── backend.py                # Flask server (local dev hoặc production)
```
