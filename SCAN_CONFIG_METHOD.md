# 🎭 Water Curtain — Scan + Config Phương Pháp

## 📋 Tổng Quan

Hệ thống có **2 cách** để tìm và cấu hình ESP32:

### **Cách 1: Web UI (Browser)**

- ✅ Dễ dùng, trực quan
- ✅ Chạy trực tiếp từ trình duyệt
- ⚠️ HTTP fallback (UDP không hỗ trợ trong browser)

### **Cách 2: Python Tool**

- ✅ UDP trực tiếp (đầy đủ chức năng)
- ✅ CLI + GUI modes
- ✅ Parallel scanning (nhanh)

---

## 🔍 **Phương Pháp Scanning**

### **Ban Đầu: Tìm Thiết Bị**

**2 bước để tìm tất cả ESP32 trên mạng:**

#### **Cách nhanh nhất (Python):**

```bash
python set_config.py --scan 192.168.1
```

Output:

```
[*] Scanning 192.168.1.* for devices...
  [+] 192.168.1.241
  [+] 192.168.1.159
[OK] Found 2: 192.168.1.241, 192.168.1.159
```

**Thời gian:** ~3 giây (parallel scan 254 IPs với 50 workers)

#### **Cách từ Web UI:**

1. Mở `index.html` trong browser
2. Kéo xuống tìm "Scan ESP" section
3. Click "🔍 Scan Network"
4. Chờ kết quả hiện ra

**Thời gian:** ~5-10 giây (sequential scan, hạn chế của browser)

---

## ⚙️ **Phương Pháp Configuration**

### **Sau khi tìm được device:**

#### **Cách 1: Python Tool (Recommend)**

**a) Xem thông tin hiện tại:**

```bash
python set_config.py --esp 192.168.1.241 --info
```

Output:

```
[OK] OK:ESP32:192.168.1.241 WS:3333 TD:192.168.1.100 Ver:1.0
```

**b) Đổi Remote IP (không cần reboot):**

```bash
python set_config.py --esp 192.168.1.241 --ip 192.168.1.150
```

Output:

```
[OK] OK:192.168.1.150
```

**c) Đổi Remote Port:**

```bash
python set_config.py --esp 192.168.1.241 --port 3333
```

**d) Restart device:**

```bash
python set_config.py --esp 192.168.1.241 --reset
```

#### **Cách 2: Web UI**

1. **Sau khi scan:** Device xuất hiện trong "Found Devices" dropdown
2. **Click device:** Tự động điền IP vào Config section
3. **Get Info:** Click button để xem thông tin
4. **Đổi config:**
   - Nhập IP mới → click "Set"
   - Nhập Port mới → click "Set"
5. **Restart:** Click "Restart Device"

---

## 🔄 **Complete Workflow: Step-by-step**

### **Scenario: Set up 2 devices để phát animation**

**Step 1: Scan tìm thiết bị**

```bash
python set_config.py --scan 192.168.1
```

→ Output: Found 192.168.1.241, 192.168.1.159

**Step 2: Configure each device**

Device 1:

```bash
python set_config.py --esp 192.168.1.241 --info
# Check: ESP32:192.168.1.241 WS:3333 TD:192.168.1.100

python set_config.py --esp 192.168.1.241 --ip 192.168.1.100
# Change remote IP to 192.168.1.100
```

Device 2:

```bash
python set_config.py --esp 192.168.1.159 --info
# Check: ESP32:192.168.1.159 WS:3333 TD:192.168.1.100

# Same IP as device 1 (both will receive same stream)
python set_config.py --esp 192.168.1.159 --ip 192.168.1.100
```

**Step 3: Mở web UI và start playback**

```
1. Open index.html in browser
2. Go to "Scan ESP" → click "Scan Network"
3. Select device 1 (192.168.1.241) → click Connect
4. Load animation file
5. Click "Send"
→ Both devices will play animation simultaneously
```

---

## 🎯 **Khi Nào Nhấn Button Nào?**

| Situation               | Action            | Tool                                       |
| ----------------------- | ----------------- | ------------------------------------------ |
| **Lần đầu setup**       | Scan network      | Python: `--scan`                           |
| **Xem hiện tại config** | Get info          | Python: `--info` OR Web: "Get Device Info" |
| **Đổi remote IP**       | Set IP            | Python: `--ip` OR Web: "Set IP" button     |
| **Đổi remote port**     | Set port          | Python: `--port` OR Web: "Set Port" button |
| **Device bị lỗi**       | Restart           | Python: `--reset` OR Web: "Restart Device" |
| **Chạy animation**      | Connect + Send    | Web: "Connect" → "Send"                    |
| **Interactive config**  | Multiple commands | Python: `--cli` (interactive mode)         |

---

## 📝 **Serial Output: Khi Device Nhận Commands**

Khi bạn chạy các commands, serial output sẽ hiện:

**Scan:**

```
[WiFi] Connected! IP: 192.168.1.241
[CFG] ✓ UDP Config server on port 8888
[SCAN] listening :<CFG_PORT>
```

**Get Info:**

```
[CFG] INFO requested → OK:ESP32:192.168.1.241 WS:3333 TD:192.168.1.100 Ver:1.0
```

**Set IP:**

```
[CFG] ✓ TD_IP changed to: 192.168.1.150
```

**Get Frame:**

```
[WS] Client #0 connected from 192.168.168.X
[CMD] Stream start t0=20 ms
[FRAME] Queued ts=50 bits=[89 ab cd ef ...]
[SCHED] Executed frame ts=50 (count=100)
```

---

## ⚡ **Performance Notes**

### **Scan Performance**

- **Python parallel:** ~3 sec (254 IPs × 50 workers)
- **Web sequential:** ~5-10 sec (browser limitation)

### **Config Performance**

- **Response time:** <50ms per command
- **No impact on playback:** UDP runs separately, doesn't block WebSocket

### **Loop Performance**

- **Before heartbeat:** 84-85 µs per loop
- **With config running:** Still 84-85 µs (non-blocking)

---

## 🔐 **Architecture Overview**

```
┌─────────────────────────────────────────────────────┐
│                  ESP32 Firmware                     │
├─────────────────────────────────────────────────────┤
│                                                     │
│  Core 0: WiFi stack (runs in background)           │
│  Core 1: Application                               │
│    ├─ Loop() { (Priority 1,2,3)                   │
│    │  ├─① Valve control (frames) — 2.4ms         │
│    │  ├─② WebSocket (commands) — non-blocking    │
│    │  └─③ UDP Config (set IP/port) — <50ms      │
│    │                                               │
│    ├─ g_tcp (WebSocket server) — port 3333        │
│    ├─ g_cfg (UDP config server) — port 8888       │
│    └─ g_valve (shift register control)            │
│                                                     │
└─────────────────────────────────────────────────────┘

Connections:
  PC ─ WiFi ─ ESP32:3333 (WebSocket, frames)
  PC ─ WiFi ─ ESP32:8888 (UDP config commands)
```

---

## 🚀 **Quick Start**

### **First Time Setup:**

```bash
# 1. Scan for devices
python set_config.py --scan

# 2. Check device info
python set_config.py --esp 192.168.1.241 --info

# 3. Set remote target IP (where animations come from)
python set_config.py --esp 192.168.1.241 --ip 192.168.1.100

# 4. Open web UI and start playing
# index.html → Scan ESP → Connect → Send
```

### **Daily Usage:**

```bash
# Option A: Python tool (recommended for reliability)
python set_config.py --scan
python set_config.py --esp 192.168.1.241 --info

# Option B: Web UI (for simplicity)
# Open index.html → Scan ESP → Connect → Load animation → Send
```

---

## ❓ **Troubleshooting**

### **Cannot find device**

```bash
# Check if device is online
python set_config.py --scan 192.168.1

# If nothing found:
# 1. Check ESP32 power
# 2. Check WiFi connection (serial monitor)
# 3. Check IP range (might be 192.168.0 instead)
python set_config.py --scan 192.168.0
```

### **Cannot connect to ESP32**

```bash
# Check if port 3333 is reachable
python set_config.py --esp 192.168.1.241 --info

# If timeout:
# 1. ESP32 might be restarting (wait 10s)
# 2. Wrong IP address
# 3. Firewall blocking UDP port 8888
```

### **Animation not playing**

```bash
# Check remote IP is correct
python set_config.py --esp 192.168.1.241 --info
# Should show: TD:192.168.1.XXX (where animation is sent from)

# Set correct IP if needed
python set_config.py --esp 192.168.1.241 --ip <CORRECT_IP>
```

---

## 📚 **Related Files**

- [CONFIG_UDP.md](CONFIG_UDP.md) — Full UDP command reference
- [WEB_UI_CONFIG.md](WEB_UI_CONFIG.md) — Web UI details
- [set_config.py](set_config.py) — Python tool source
- [include/config_server.h](include/config_server.h) — ESP32 config server

---

## 💡 **Design Philosophy**

**Why 2 methods?**

1. **Python tool:** 100% reliable, full UDP support, scriptable
2. **Web UI:** User-friendly, visual, browser-based reference

**Choose based on your need:**

- **Setup/Troubleshooting:** Python tool (more reliable)
- **Casual use:** Web UI (simpler)
- **Production:** Python tool (guaranteed)

---

**Version:** 1.0  
**Date:** 2026-04-03  
**Status:** ✅ Ready for production use
