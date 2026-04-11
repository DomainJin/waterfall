# 🎭 Water Curtain — Quick Start Guide

## ⚡ TL;DR — 30 giây để có animations chạy

### **Step 1: Tìm device (10 sec)**

```bash
python set_config.py --scan 192.168.1
```

✓ Thấy: `192.168.1.241` và `192.168.1.159`

### **Step 2: Config device (5 sec)**

```bash
python set_config.py --esp 192.168.1.241 --info
python set_config.py --esp 192.168.1.241 --ip 192.168.1.100
```

### **Step 3: Mở web UI**

```
1. Open index.html in browser
2. Scan ESP → select device
3. Load animation file
4. Click "Send"
```

✓ Device phát animation!

---

## 📱 **Hai Cách Để Nguồn & Cấu Hình**

### **Cách A: Python CLI (Fastest & Most Reliable) ⭐**

**Lợi ích:**

- ✅ UDP trực tiếp (không qua HTTP)
- ✅ Scan parallel (3 giây cho 254 IPs)
- ✅ Commands instant (<50ms response)
- ✅ CLI + GUI modes
- ✅ Có thể script/automate

**Các commands:**

```bash
# Scan mạng
python set_config.py --scan 192.168.1

# Xem thông tin device
python set_config.py --esp 192.168.1.241 --info

# Đổi IP đích (không cần reboot)
python set_config.py --esp 192.168.1.241 --ip 192.168.1.100

# Đổi port
python set_config.py --esp 192.168.1.241 --port 3333

# Restart device
python set_config.py --esp 192.168.1.241 --reset

# Interactive mode
python set_config.py --cli

# GUI mode
python set_config.py --gui
```

---

### **Cách B: Web UI (Visual & Easy) 📱**

**Lợi ích:**

- ✅ Không cần terminal
- ✅ Giao diện trực quan
- ✅ Tích hợp trong animation player
- ✅ Click button thay vì gõ lệnh

**Các bước:**

1. Mở **index.html** trong browser
2. Kéo xuống tìm **"Scan ESP"** section
3. Click **"🔍 Scan Network"** → chờ kết quả
4. Chọn device từ dropdown
5. Kéo xuống tìm **"Config Device"** section
6. Click **"🔄 Get Device Info"** → xem thông tin
7. Nhập IP mới → click **"Set IP"**

---

## 🎯 **Khi Nào Dùng Cách Nào?**

| Tình Huống                   | Dùng Cách Nào     | Lệnh                                              |
| ---------------------------- | ----------------- | ------------------------------------------------- |
| Lần đầu setup, chưa biết IP  | **Python Scan**   | `python set_config.py --scan 192.168.1`           |
| Kiểm tra device có OK không  | **Python Info**   | `python set_config.py --esp 192.168.1.241 --info` |
| Đổi config một lần           | **Python CLI**    | `python set_config.py --esp IP --ip NEW_IP`       |
| Cấu hình nhiều device        | **Python Script** | Script các lệnh liên tiếp                         |
| Chơi animation bình thường   | **Web UI**        | Mở index.html → click buttons                     |
| Không có terminal (GUI only) | **Python GUI**    | `python set_config.py --gui`                      |
| Setup production, automation | **Python Batch**  | `for` loop các commands                           |

---

## 🔄 **Complete Workflow: Ví Dụ Thực Tế**

### **Scenario: Setup 2 devices để phát animation cùng lúc**

**Terminal (Python tool):**

```bash
# 1. Tìm device
$ python set_config.py --scan 192.168.1
[*] Scanning 192.168.1.* for devices...
  [+] 192.168.1.241
  [+] 192.168.1.159
[OK] Found 2: 192.168.1.241, 192.168.1.159

# 2. Kiểm tra device 1
$ python set_config.py --esp 192.168.1.241 --info
[OK] OK:ESP32:192.168.1.241 WS:3333 TD:192.168.1.100 Ver:1.0

# 3. Kiểm tra device 2
$ python set_config.py --esp 192.168.1.159 --info
[OK] OK:ESP32:192.168.1.159 WS:3333 TD:192.168.1.100 Ver:1.0

# (Both devices already configured, no need to change)
```

**Browser (Web UI):**

```
1. Open index.html
2. Go to "Scan ESP" section
3. Click "🔍 Scan Network"
   → Shows: 192.168.1.241, 192.168.1.159
4. Select 192.168.1.241 → Auto-fills config form
5. Go to "Config Device" section
6. Click "🔄 Get Device Info"
   → Shows: ESP32:192.168.1.241 WS:3333 TD:192.168.1.100
7. Go to "Image Source" section
8. Load animation file (GIF, PNG, MP4)
9. Wait for preview
10. Click "📤 Send" to play animation
    → Animation plays on 192.168.1.241

11. Go back to "Scan ESP"
12. Select 192.168.1.159 → Auto-fills config
13. Click "Connect"
14. Click "📤 Send" again
    → Animation plays on 192.168.1.159
```

**Result:** Both devices play animation simultaneously! 🎉

---

## 🔌 **Understanding the Ports**

### **Port 3333 — WebSocket (Frame Playback)**

- ✅ Browser connects here to send animation frames
- ✅ Real-time playback
- ✅ Used by: Web UI "Send" button

**Test:**

```
Browser → ws://192.168.1.241:3333
```

### **Port 8888 — UDP Config (Settings)**

- ✅ Standalone config (no WiFi disruption)
- ✅ Change IP/port without stopping playback
- ✅ Used by: `python set_config.py` tool

**Test:**

```bash
python set_config.py --esp 192.168.1.241 --info
```

---

## ❓ **FAQ — Khi Nào Nhấn Cái Gì?**

### **Q: Làm sao biết device nào online?**

**A:** Chạy scan:

```bash
python set_config.py --scan 192.168.1
```

### **Q: Sau scan xong, bước tiếp theo là gì?**

**A:**

- Nếu chỉ muốn chơi: Mở web UI → connect → send
- Nếu cần đổi config: `python set_config.py --esp IP --info`

### **Q: Device không phát animation?**

**A:** Check:

```bash
python set_config.py --esp 192.168.1.241 --info
# Should show TD: (remote IP where animation comes from)
# If different, change:
python set_config.py --esp 192.168.1.241 --ip 192.168.1.XXX
```

### **Q: Animation bị delay/lag?**

**A:**

- Check hết device trên Web UI có kết nối không
- Check heartbeat (optional, dùng `--cli` để debug)

### **Q: Muốn scan device khác IP range?**

**A:**

```bash
python set_config.py --scan 192.168.0
python set_config.py --scan 10.0.0
```

---

## 📊 **Architecture Summary**

```
┌─ Your PC ──────────────────────────────────────┐
│ ┌─ Web Browser ──────────────────────────────┐ │
│ │ 1. Load index.html                         │ │
│ │ 2. Click "Scan" → scan network            │ │
│ │ 3. Select device → connect WebSocket      │ │
│ │ 4. Load animation → send frames           │ │
│ └────────────────────────────────────────────┘ │
│ ┌─ Terminal (Optional) ──────────────────────┐ │
│ │ 1. $ python set_config.py --scan         │ │
│ │ 2. $ python set_config.py --esp .. --ip  │ │
│ │ 3. $ python set_config.py --esp .. --gui │ │
│ └────────────────────────────────────────────┘ │
└────────────────────────────────────────────────┘
         WiFi Network 192.168.1.x
            │
    ┌───────┼───────┐
    ▼       ▼       ▼
┌────────────────────────────────┐
│   ESP32 #1                    │
│   IP: 192.168.1.241           │
│   ├─ Port 3333 (WebSocket)    │
│   ├─ Port 8888 (UDP Config)   │
│   ├─ Valve control (80 valves)│
│   └─ SD card                  │
└────────────────────────────────┘
┌────────────────────────────────┐
│   ESP32 #2                    │
│   IP: 192.168.1.159           │
│   ├─ Port 3333 (WebSocket)    │
│   ├─ Port 8888 (UDP Config)   │
│   ├─ Valve control (80 valves)│
│   └─ SD card                  │
└────────────────────────────────┘
```

---

## ✅ **Before You Start: Checklist**

- [ ] Both ESP32 devices powered on
- [ ] Both devices connected to WiFi "SGM"
- [ ] Python 3.8+ installed with dependencies:
  ```bash
  pip install tkinter  # Usually pre-installed
  ```
- [ ] index.html accessible from browser
- [ ] Opening files in browser (not file://)

**Optional for development:**

- [ ] Serial monitor to see debug logs
- [ ] Understanding of animation frame format

---

## 🚀 **Let's Go!**

### **Minimal 3-step setup:**

```bash
# Step 1: Find devices
python set_config.py --scan 192.168.1

# Step 2: Verify device
python set_config.py --esp 192.168.1.241 --info

# Step 3: Open browser and play!
# index.html → Scan → Connect → Load → Send
```

**Expected output:**

```
✓ Found 2 devices
✓ Device online and configured
✓ Animation playing on valves!
```

---

**Questions?** See [SCAN_CONFIG_METHOD.md](SCAN_CONFIG_METHOD.md) for detailed explanation.

**Version:** 1.0  
**Date:** 2026-04-03  
**Status:** ✅ Ready to use
