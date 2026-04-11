# 💧 Hệ Thống Điều Khiển Van Rèm Nước

**Hệ thống web-based điều khiển lên tới 80 van solenoid bằng WebSocket, tạo ra những hiệu ứng animation có thể lập trình cho rèm nước.**

---

## 📋 Mục Lục

- [Tổng Quan Dự Án](#tổng-quan-dự-án)
- [Cấu Trúc Dữ Liệu Hiệu Ứng](#cấu-trúc-dữ-liệu-hiệu-ứng)
- [Kiến Trúc Hệ Thống](#kiến-trúc-hệ-thống)
- [Hướng Dẫn Nhanh](#hướng-dẫn-nhanh)
- [Tài Liệu Tham Khảo](#tài-liệu-tham-khảo)
- [Cấu Hình Phần Cứng](#cấu-hình-phần-cứng)

---

## 🎯 Tổng Quan Dự Án

Hệ thống này điều khiển tối đa **80 van solenoid** được kết nối theo kiếu xuyến (daisy-chain) sử dụng các shift register (74HC595). ESP32 nhận các khung hình (frame) animation qua WebSocket và thực thi chúng theo thời gian thực với độ chính xác cao.

**Các Tính Năng Chính:**

- ✅ Điều khiển van theo thời gian thực qua WebSocket
- ✅ Lưu trữ các khung hình animation trên thẻ SD
- ✅ Giao diện Web để tạo và quản lý animation
- ✅ Giao thức UDP cho cấu hình thiết bị
- ✅ Quản lý lưu trữ (backup/restore)

---

## 🌊 Cấu Trúc Dữ Liệu Hiệu Ứng

### **Hiệu Ứng (Effect) Là Gì?**

**Hiệu ứng** (effect) là một **chuỗi dữ liệu trạng thái van theo thời gian** tạo nên một animation trực quan. Mỗi hiệu ứng được tạo thành từ nhiều **khung hình** (frame), mỗi frame định nghĩa trạng thái mở/đóng của các van tại một thời điểm cụ thể.

### **Cấu Trúc Khung Hình (Frame Structure)**

Mỗi frame chiếm **14 bytes**:

```c
struct Frame {
    uint32_t ts_ms;          // Thời gian (milliseconds) - 4 bytes
    uint8_t  bits[NUM_BOARDS];  // Trạng thái van - 10 bytes (80 van)
};
```

| Trường   | Kích Thước | Mục Đích                                             |
| -------- | ---------- | ---------------------------------------------------- |
| `ts_ms`  | 4 bytes    | Offset thời gian từ đầu animation (milliseconds)     |
| `bits[]` | 10 bytes   | Trạng thái nhị phân 80 van: bit=1 (mở), bit=0 (đóng) |

### **Ánh Xạ Van trong Mảng Bits**

```
byte[0]: van 0-7      (Van 0-7)
byte[1]: van 8-15     (Van 8-15)
byte[2]: van 16-23    (Van 16-23)
  ⋮
byte[9]: van 72-79    (Van 72-79)
```

Mỗi bit trong một byte đại diện cho một van:

```
byte[0] = 0b10101010
           └─ bit 7 = Van 7 (MSB - quan trọng nhất)
              bit 6 = Van 6
              ...
              bit 0 = Van 0 (LSB - ít quan trọng nhất)
```

### **Ví Dụ: Hiệu Ứng Sóng Nước**

Tạo animation sóng di chuyển từ trái sang phải:

```
Frame 0: ts_ms=0    | bits=[0x00, 0xFF, 0x00, ...] = Van 8-15 MỞ
Frame 1: ts_ms=100  | bits=[0x00, 0x00, 0xFF, ...] = Van 16-23 MỞ
Frame 2: ts_ms=200  | bits=[0x00, 0x00, 0x00, ...] = Van 24-31 MỞ
Frame 3: ts_ms=300  | bits=[0xFF, 0x00, 0x00, ...] = Van 0-7 MỞ (quay về đầu)
```

**Kết Quả:** Một làn sáng giống như sóng di chuyển từ trái sang phải trên rèm nước trong 300ms.

### **Animation = Nhiều Frames**

```
File Animation (.bin):
[Frame 0: 14 bytes]
[Frame 1: 14 bytes]
[Frame 2: 14 bytes]
     ⋮
[Frame N: 14 bytes]
```

**Kích thước tổng = N × 14 bytes**

**Quá trình phát lại (Playback):**

1. Gửi Frame 0 → Van cập nhật lúc t=0ms
2. Chờ đến thời gian của Frame 1 → Gửi Frame 1 → Van cập nhật lúc t=100ms
3. Tiếp tục cho đến khi animation kết thúc

---

## 🏗️ Kiến Trúc Hệ Thống

### **Các Thành Phần Chính**

```
┌─────────────────────────────────────────┐
│       Giao Diện Web (index.html)        │
│   - Tạo animation                       │
│   - Cấu hình                            │
│   - Quản lý dữ liệu                     │
└──────────────┬──────────────────────────┘
               │ WebSocket
               ▼
┌──────────────────────────────────┐
│   ESP32 (PlatformIO)             │
│ ┌────────────────────────────┐   │
│ │ TcpServer (WebSocket)      │   │
│ │ - Nhận frame               │   │
│ │ - Phát trạng thái          │   │
│ └────────────┬───────────────┘   │
│              │                    │
│ ┌────────────▼───────────────┐   │
│ │ FrameQueue (Hàng Frame)    │   │
│ │ - Lưu frame vào vùng đệm   │   │
│ └────────────┬───────────────┘   │
│              │                    │
│ ┌────────────▼───────────────┐   │
│ │ Scheduler (Bộ Lên Lịch)    │   │
│ │ - Điều khiển thời gian     │   │
│ │ - Áp dụng thay đổi van     │   │
│ └────────────┬───────────────┘   │
│              │                    │
│ ┌────────────▼───────────────┐   │
│ │ ValveDriver (ShiftRegister)│   │
│ │ - Gửi bytes tới phần cứng  │   │
│ └────────────┬───────────────┘   │
│              │                    │
│ ┌────────────▼───────────────┐   │
│ │ SDManager                  │   │
│ │ - Tải/lưu animation        │   │
│ └────────────────────────────┘   │
└──────────────────────────────────┘
               │ SPI
               ▼
      ┌──────────────────┐
      │   Thẻ SD         │
      │ /effects/        │
      │  ├─ effect1.bin  │
      │  ├─ effect2.bin  │
      │  └─ effects.json │
      └──────────────────┘
```

### **Kết Nối Phần Cứng**

```
ESP32 GPIO → 74HC245 Transceiver → 74HC595 Shift Registers → Van Solenoid

GPIO2  (SHCP) → Serial Clock Input (Xung Xuyến)
GPIO4  (STCP) → Storage Clock Input (Latch - Khóa)
GPIO23 (DS)   → Serial Data Input (Dữ Liệu Nối Tiếp)
```

**Xuyến (Daisy-chain):** Mỗi 74HC595 xuất ra dữ liệu vào shift register tiếp theo, cho phép điều khiển nhiều van tuần tự.

---

## ⚡ Hướng Dẫn Nhanh

### **Yêu Cầu Cần Thiết**

- PlatformIO được cài đặt
- Board phát triển ESP32
- Mạng WiFi (SSID: `SGM`, Mật khẩu: `19121996`)
- Python 3.8+ cho backend (tùy chọn)

### **Build & Upload Firmware**

```bash
# Cài đặt thư viện
platformio lib install

# Biên dịch firmware
platformio run

# Upload lên ESP32
platformio run --target upload

# Theo dõi output từ Serial
platformio device monitor
```

### **Truy Cập Giao Diện Web**

1. Mở trình duyệt: `http://<IP_ESP32>/`
2. Đi tới tab **Quản Lý Dữ Liệu**
3. Tạo hoặc tải animation

### **Máy Chủ Backend** (Python - Tùy Chọn)

```bash
# Cài đặt thư viện Python
pip install -r requirements_backend.txt

# Chạy máy chủ
python backend.py
```

---

## 📚 Tài Liệu Tham Khảo

### **Hướng Dẫn Chính**

| File                                                 | Nội Dung                                  |
| ---------------------------------------------------- | ----------------------------------------- |
| [QUICK_START.md](QUICK_START.md)                     | Cấu hình cơ bản và tạo animation đầu tiên |
| [BACKEND_GUIDE.md](BACKEND_GUIDE.md)                 | API backend và các endpoint               |
| [WEB_UI_CONFIG.md](WEB_UI_CONFIG.md)                 | Tuỳ chọn cấu hình giao diện Web           |
| [DATA_MANAGEMENT_GUIDE.md](DATA_MANAGEMENT_GUIDE.md) | Các tính năng tab Quản Lý Dữ Liệu         |

### **Phần Cứng & Xử Lý Sự Cố**

| File                                                     | Nội Dung                            |
| -------------------------------------------------------- | ----------------------------------- |
| [SD_CARD_MODULE.md](SD_CARD_MODULE.md)                   | Cấu hình thẻ SD và cấu trúc thư mục |
| [CONFIG_UDP.md](CONFIG_UDP.md)                           | Giao thức UDP cho cấu hình          |
| [TROUBLESHOOTING_SUMMARY.md](TROUBLESHOOTING_SUMMARY.md) | Các vấn đề phổ biến và giải pháp    |

### **Phát Triển**

| File                                         | Nội Dung                    |
| -------------------------------------------- | --------------------------- |
| [ESP32_CODE_UPDATE.md](ESP32_CODE_UPDATE.md) | Quy trình cập nhật firmware |
| [WEBSOCKET_DEBUG.md](WEBSOCKET_DEBUG.md)     | Gỡ lỗi giao thức WebSocket  |

---

## ⚙️ Cấu Hình Phần Cứng

### **Cấu Hình Chân GPIO** (xem `include/config.h`)

```c
#define PIN_SHCP    2      // GPIO2  - Xung xuyến shift register
#define PIN_STCP    4      // GPIO4  - Khóa (Latch)
#define PIN_DS      23     // GPIO23 - Dữ liệu nối tiếp

#define NUM_BOARDS  10     // 10 shift register = 80 van
#define NUM_VALVES  80     // 10 × 8 bit/register
```

### **Giao Thức Frame**

- **Kích Thước Frame:** 14 bytes (cố định)
- **Kích Thước Max Hàng:** 512 frame
- **Thời Lượng Animation Max:** Phụ thuộc không gian SD (~4MB có sẵn)

---

## 🔧 Cấu Hình Hệ Thống

**Thông Tin WiFi** (`include/config.h`):

```c
#define WIFI_SSID        "SGM"
#define WIFI_PASSWORD    "19121996"
```

**Cổng Máy Chủ:**

- WebSocket: Cổng 3333 (`ws://<IP>:3333`)
- UDP Config: Cổng 8888

**Bộ Đếm Thời Gian (Watchdog):** 5000ms (thiết bị khởi động lại nếu task chính bị treo)

---

## 📊 Cấu Trúc Tệp Dự Án

```
project/
├── src/
│   ├── main.cpp                # Điểm vào chính ESP32
│   └── sd_manager.cpp          # Các phép toán thẻ SD
├── include/
│   ├── config.h                # Cấu hình & định nghĩa chân
│   ├── frame_queue.h           # Hàng buffer frame (vòng)
│   ├── valve_driver.h          # Điều khiển shift register
│   ├── tcp_server.h            # Máy chủ WebSocket
│   ├── scheduler.h             # Timing & execution
│   ├── sd_manager.h            # Giao diện thẻ SD
│   ├── config_server.h         # Endpoint cấu hình UDP
│   └── sd_web_handler.h        # Web file serving
├── lib/                        # Thư viện bên ngoài
├── test/                       # Unit tests
├── index.html                  # Giao diện Web
├── backend.py                  # Backend Python (tùy chọn)
└── platformio.ini              # Cấu hình PlatformIO
```

---

## 📞 Hỗ Trợ & Xử Lý Sự Cố

**Các Vấn Đề Phổ Biến:**

- **ESP32 không kết nối WiFi:**
  - Kiểm tra SSID/password trong `config.h`
  - Xác minh cường độ tín hiệu WiFi
  - Xem [TROUBLESHOOTING_SUMMARY.md](TROUBLESHOOTING_SUMMARY.md)

- **Frames không phát:**
  - Kiểm tra định dạng file animation (14 bytes/frame)
  - Kiểm tra hàng frame không đầy (max 512 frame)
  - Theo dõi output Serial để tìm lỗi

- **Thẻ SD không được phát hiện:**
  - Thử định dạng lại (FAT32)
  - Kiểm tra kết nối chân
  - Xem [SD_CARD_MODULE.md](SD_CARD_MODULE.md)

---

## 📝 Giấy Phép

Độc quyền - Hệ Thống Điều Khiển Rèm Nước

---

**Cập Nhật Lần Cuối:** Tháng 4 năm 2026

Để biết thông tin chi tiết về bất kỳ thành phần nào, hãy xem các tệp tài liệu được liệt kê ở trên.
