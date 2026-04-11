# UDP Config Tool — Water Curtain ESP32

Đổi cấu hình ESP32 **mà không cần upload lại firmware**. UDP server chạy trên **port 8888**.

## Các lệnh UDP

| Command           | Purpose                           | Example                                                |
| ----------------- | --------------------------------- | ------------------------------------------------------ |
| `GET_INFO`        | Lấy thông tin thiết bị            | `set_config.py --esp 192.168.1.241 --info`             |
| `SET_IP:<IP>`     | Đổi remote target IP              | `set_config.py --esp 192.168.1.241 --ip 192.168.1.100` |
| `SET_PORT:<PORT>` | Đổi remote target port            | `set_config.py --esp 192.168.1.241 --port 3333`        |
| `RESET`           | Restart ESP32                     | `set_config.py --esp 192.168.1.241 --reset`            |
| `SCAN`            | Heartbeat response (for scanning) | Tự động khi quét                                       |

## Sử dụng Python Tool

### 1. GUI Mode (mặc định)

```bash
python set_config.py
```

- Giao diện đồ họa
- Quét tự động
- Đổi config dễ dàng

### 2. Scan Devices

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

### 3. Get Info

```bash
python set_config.py --esp 192.168.1.241 --info
```

Output:

```
[OK] OK:ESP32:192.168.1.241 WS:3333 TD:192.168.1.100 Ver:1.0
```

### 4. Set Remote IP

```bash
python set_config.py --esp 192.168.1.241 --ip 192.168.1.100
```

Output:

```
[OK] OK:192.168.1.100
```

### 5. Set Remote Port

```bash
python set_config.py --esp 192.168.1.241 --port 3333
```

Output:

```
[OK] OK:3333
```

### 6. Interactive CLI

```bash
python set_config.py --cli
```

Interactive prompt:

```
=====================================
Water Curtain ESP32 Config Tool
Target: 192.168.1.241:8888
=====================================

Commands:
  info              - Get device info
  set-ip <IP>       - Set remote IP
  set-port <PORT>   - Set remote port
  reset             - Restart ESP32
  scan [NETWORK]    - Scan for devices
  quit              - Exit

> info
[OK] OK:ESP32:192.168.1.241 WS:3333 TD:192.168.1.100 Ver:1.0

> set-ip 192.168.1.150
[OK] OK:192.168.1.150

> quit
```

## Firmware Side

### UDP Server in firmware

Located in [include/config_server.h](include/config_server.h)

- Listens on **port 8888**
- Non-blocking (chạy trong loop)
- Supports 5 commands (GET_INFO, SET_IP, SET_PORT, RESET, SCAN)
- Integrated into [src/main.cpp](src/main.cpp) **Priority 3** in loop

### Loop Priority

```
Priority 1: Valve control (frames)
Priority 2: WebSocket (commands)
Priority 3: UDP Config (get_info, set_ip, etc.)
Priority 4: Heartbeat (every 5s)
```

## Use Cases

### Use Case 1: Quick IP Check

```bash
python set_config.py --scan
# Hiểm tất cả ESP32 trên mạng
```

### Use Case 2: Update Remote IP without Reboot

```bash
# Old: Manual edit config.h → compile → upload (3+ min)
# New: 1 command (< 1 sec)
python set_config.py --esp 192.168.1.241 --ip 192.168.1.200
```

### Use Case 3: Troubleshooting

```bash
# Check if ESP32 is alive
python set_config.py --esp 192.168.1.241 --info

# If no response → check WiFi, power, IP range
# If response OK but frames not working → check WS connection
```

### Use Case 4: Multi-device Setup

```bash
# Scan for all
python set_config.py --scan 192.168.1

# Configure each one
python set_config.py --esp 192.168.1.241 --ip 192.168.1.100
python set_config.py --esp 192.168.1.159 --ip 192.168.1.100
```

## Serial Output Example

After firmware boot:

```
[SETUP] Starting UDP Config server...
[CFG] ✓ UDP Config server on port 8888

[SETUP] ✓ Ready!
  WebSocket:  ws://<ESP32_IP>:3333
  Config UDP: <ESP32_IP>:8888

[HEARTBEAT] t=5000 ms | Queue: 0 | Loop: 85 µs
[CFG] INFO requested → OK:ESP32:192.168.1.241 WS:3333 TD:192.168.1.100 Ver:1.0
[CFG] ✓ TD_IP changed to: 192.168.1.150
```

## Performance

- **Scan 254 IPs:** ~3 seconds (parallel with 50 workers)
- **UDP command response:** < 50ms
- **No WiFi/WebSocket disconnection** during config change
- **Non-blocking** - valve control not affected

## Files

- [include/config_server.h](include/config_server.h) — UDP server implementation
- [src/main.cpp](src/main.cpp) — Integration (line 128: `g_cfg.tick()`)
- [set_config.py](set_config.py) — Python config tool
- `CONFIG_UDP.md` — This file

## Troubleshooting

### "No response from IP:8888"

1. Check ESP32 is powered + WiFi connected
2. Check IP is correct (use `--scan`)
3. Check firewall allows UDP 8888

### "Invalid IP" error

- IP must be in format `XXX.XXX.XXX.XXX`
- Ranges: 0-255 for each octet

### GUI slow when scanning

- This is normal (timeout 0.5-0.8s per IP)
- Use `--scan` CLI for faster non-GUI scanning

---

**Created:** 2026-04-03  
**Version:** 1.0  
**Compatibility:** Water Curtain ESP32 with UDP config server enabled
