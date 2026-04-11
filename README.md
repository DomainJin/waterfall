# 💧 Water Curtain Valve Control System

**ESP32-based WebSocket-driven valve control system for creating programmable water curtain animations.**

---

## 📋 Table of Contents

- [Project Overview](#project-overview)
- [Effect Data Structure](#effect-data-structure)
- [Architecture](#architecture)
- [Quick Start](#quick-start)
- [Documentation](#documentation)
- [Hardware Setup](#hardware-setup)

---

## 🎯 Project Overview

This system controls up to **80 solenoid valves** arranged in a daisy-chain configuration using shift registers (74HC595). The ESP32 receives animation frames via WebSocket and executes them in real-time with precise timing.

**Key Features:**

- ✅ Real-time valve control via WebSocket
- ✅ Animation frames stored on SD Card
- ✅ Web UI for animation creation and management
- ✅ UDP configuration protocol
- ✅ Storage management (backup/restore)

---

## 🌊 Effect Data Structure

### **What is an Effect/Animation?**

An **effect** is a **time-series sequence of valve states** that creates a visual animation. Each effect is composed of multiple **frames**, and each frame defines which valves are open/closed at a specific moment in time.

### **Frame Structure**

Each frame is **14 bytes**:

```c
struct Frame {
    uint32_t ts_ms;          // Timestamp in milliseconds (4 bytes)
    uint8_t  bits[NUM_BOARDS];  // Valve states (10 bytes for 80 valves)
};
```

| Field    | Size     | Purpose                                                 |
| -------- | -------- | ------------------------------------------------------- |
| `ts_ms`  | 4 bytes  | Time offset from animation start (milliseconds)         |
| `bits[]` | 10 bytes | Binary state of 80 valves: bit=1 (open), bit=0 (closed) |

### **Valve Mapping in Bits Array**

```
byte[0]: valves 0-7      (Van 0-7)
byte[1]: valves 8-15     (Van 8-15)
byte[2]: valves 16-23    (Van 16-23)
  ⋮
byte[9]: valves 72-79    (Van 72-79)
```

Each bit within a byte represents one valve:

```
byte[0] = 0b10101010
           └─ bit 7 = Van 7 (MSB)
              bit 6 = Van 6
              ...
              bit 0 = Van 0 (LSB)
```

### **Example: Wave Effect**

Creating a wave animation that moves left-to-right:

```
Frame 0: ts_ms=0    | bits=[0x00, 0xFF, 0x00, ...] = Van 8-15 ON
Frame 1: ts_ms=100  | bits=[0x00, 0x00, 0xFF, ...] = Van 16-23 ON
Frame 2: ts_ms=200  | bits=[0x00, 0x00, 0x00, ...] = Van 24-31 ON
Frame 3: ts_ms=300  | bits=[0xFF, 0x00, 0x00, ...] = Van 0-7 ON (wrap)
```

**Result:** Light wave appears to move across the curtain over 300ms.

### **Animation = Multiple Frames**

```
Animation File (.bin):
[Frame 0: 14 bytes]
[Frame 1: 14 bytes]
[Frame 2: 14 bytes]
     ⋮
[Frame N: 14 bytes]
```

**Total size = N × 14 bytes**

Playback:

1. Send Frame 0 → Valves update at t=0ms
2. Wait until Frame 1's timestamp → Send Frame 1 → Valves update at t=100ms
3. Continue until animation ends

---

## 🏗️ Architecture

### **System Components**

```
┌─────────────────────────────────────────┐
│          Web UI (index.html)            │
│     - Animation creation                │
│     - Configuration                     │
│     - Data management                   │
└──────────────┬──────────────────────────┘
               │ WebSocket
               ▼
┌──────────────────────────────────┐
│   ESP32 (PlatformIO)             │
│ ┌────────────────────────────┐   │
│ │ TcpServer (WebSocket)      │   │
│ │ - Receive frames           │   │
│ │ - Broadcast status         │   │
│ └────────────┬───────────────┘   │
│              │                    │
│ ┌────────────▼───────────────┐   │
│ │ FrameQueue                 │   │
│ │ - Buffer incoming frames   │   │
│ └────────────┬───────────────┘   │
│              │                    │
│ ┌────────────▼───────────────┐   │
│ │ Scheduler                  │   │
│ │ - Timing control           │   │
│ │ - Apply valve changes      │   │
│ └────────────┬───────────────┘   │
│              │                    │
│ ┌────────────▼───────────────┐   │
│ │ ValveDriver (ShiftRegister)│   │
│ │ - Send bytes to hardware   │   │
│ └────────────┬───────────────┘   │
│              │                    │
│ ┌────────────▼───────────────┐   │
│ │ SDManager                  │   │
│ │ - Load/save animations     │   │
│ └────────────────────────────┘   │
└──────────────────────────────────┘
               │ SPI
               ▼
      ┌──────────────────┐
      │   SD Card        │
      │ /effects/        │
      │  ├─ effect1.bin  │
      │  ├─ effect2.bin  │
      │  └─ effects.json │
      └──────────────────┘
```

### **Hardware Connection**

```
ESP32 GPIO Pins → 74HC245 Transceiver → 74HC595 Shift Registers → Solenoid Valves

GPIO2  (SHCP) → Serial Clock Input
GPIO4  (STCP) → Storage Clock Input (Latch)
GPIO23 (DS)   → Serial Data Input
```

**Daisy-chain:** Each 74HC595 outputs to the next one's data input, allowing control of multiple valves sequentially.

---

## ⚡ Quick Start

### **Prerequisites**

- PlatformIO installed
- ESP32 development board
- WiFi network (SSID: `SGM`, Password: `19121996`)
- Python 3.8+ for backend

### **Build & Upload**

```bash
# Install dependencies
platformio lib install

# Build firmware
platformio run

# Upload to ESP32
platformio run --target upload

# Monitor serial output
platformio device monitor
```

### **Web UI Access**

1. Open browser: `http://<ESP32_IP>/`
2. Navigate to **Data Management** tab
3. Create or load animations

### **Backend Server** (Python)

```bash
# Install Python dependencies
pip install -r requirements_backend.txt

# Run server
python backend.py
```

---

## 📚 Documentation

### **System Guides**

| File                                                 | Purpose                         |
| ---------------------------------------------------- | ------------------------------- |
| [QUICK_START.md](QUICK_START.md)                     | Basic setup and first animation |
| [BACKEND_GUIDE.md](BACKEND_GUIDE.md)                 | Backend API and endpoints       |
| [WEB_UI_CONFIG.md](WEB_UI_CONFIG.md)                 | Web UI configuration options    |
| [DATA_MANAGEMENT_GUIDE.md](DATA_MANAGEMENT_GUIDE.md) | Data Management tab features    |

### **Hardware & Troubleshooting**

| File                                                     | Purpose                        |
| -------------------------------------------------------- | ------------------------------ |
| [SD_CARD_MODULE.md](SD_CARD_MODULE.md)                   | SD card setup and organization |
| [CONFIG_UDP.md](CONFIG_UDP.md)                           | UDP configuration protocol     |
| [TROUBLESHOOTING_SUMMARY.md](TROUBLESHOOTING_SUMMARY.md) | Common issues and solutions    |

### **Development**

| File                                         | Purpose                      |
| -------------------------------------------- | ---------------------------- |
| [ESP32_CODE_UPDATE.md](ESP32_CODE_UPDATE.md) | Firmware update procedures   |
| [WEBSOCKET_DEBUG.md](WEBSOCKET_DEBUG.md)     | WebSocket protocol debugging |

---

## ⚙️ Hardware Setup

### **Pin Configuration** (see `include/config.h`)

```c
#define PIN_SHCP    2      // GPIO2  - Shift register clock
#define PIN_STCP    4      // GPIO4  - Latch clock
#define PIN_DS      23     // GPIO23 - Serial data

#define NUM_BOARDS  10     // 10 shift registers = 80 valves
#define NUM_VALVES  80     // 10 × 8 bits/register
```

### **Frame Protocol**

- **Frame Size:** 14 bytes (fixed)
- **Max Frames in Queue:** 512
- **Max Animation Duration:** Depends on SD card space (~4MB available)

---

## 🔧 Configuration

**WiFi Credentials** (`include/config.h`):

```c
#define WIFI_SSID        "SGM"
#define WIFI_PASSWORD    "19121996"
```

**Server Ports**:

- WebSocket: Port 3333 (`ws://<IP>:3333`)
- UDP Config: Port 8888

**Watchdog Timer**: 5000ms (device restarts if main task stalls)

---

## 📊 File Structure

```
project/
├── src/
│   ├── main.cpp                # Main ESP32 entry point
│   └── sd_manager.cpp          # SD card operations
├── include/
│   ├── config.h                # Configuration & pin definitions
│   ├── frame_queue.h           # Circular frame buffer
│   ├── valve_driver.h          # Shift register control
│   ├── tcp_server.h            # WebSocket server
│   ├── scheduler.h             # Frame timing & execution
│   ├── sd_manager.h            # SD card interface
│   ├── config_server.h         # UDP config endpoint
│   └── sd_web_handler.h        # Web file serving
├── lib/                        # External libraries
├── test/                       # Unit tests
├── index.html                  # Web UI
├── backend.py                  # Python backend (optional)
└── platformio.ini              # PlatformIO configuration
```

---

## 📞 Support & Troubleshooting

**Common Issues:**

- **ESP32 won't connect WiFi:**
  - Check SSID/password in `config.h`
  - Verify WiFi signal strength
  - See [TROUBLESHOOTING_SUMMARY.md](TROUBLESHOOTING_SUMMARY.md)

- **Frames not playing:**
  - Verify animation file format (14 bytes per frame)
  - Check frame queue isn't full (max 512 frames)
  - Monitor serial output for errors

- **SD card not detected:**
  - Try reformatting (FAT32)
  - Check pinout connections
  - See [SD_CARD_MODULE.md](SD_CARD_MODULE.md)

---

## 📝 License

Proprietary - Water Curtain Control System

---

**Last Updated:** April 2026

For detailed information on any component, see the documentation files listed above.
#   w a t e r f a l l  
 