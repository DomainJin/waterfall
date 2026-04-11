# 📝 ESP32 Code Update — GET_STORAGE & LIST_FILES Support

## ✅ Changes Applied

### 1. config_server.h (Updated)

**Added 2 new UDP commands:**

#### Command: `GET_STORAGE`

```
Request:  UDP packet "GET_STORAGE" to port 8888
Response: "OK: used=<bytes>, total=<bytes>"

Example:
  Request:  GET_STORAGE
  Response: OK: used=123456, total=7540000000
```

**Implementation:**

```cpp
void handle_get_storage() {
    if (!m_sd || !m_sd->isInitialized()) {
        send_reply("ERROR:SD not initialized");
        return;
    }
    uint64_t used = m_sd->getUsedSpace();
    uint64_t total = m_sd->getTotalSpace();

    char buf[128];
    snprintf(buf, sizeof(buf), "OK: used=%llu, total=%llu", used, total);
    send_reply(buf);
}
```

#### Command: `LIST_FILES`

```
Request:  UDP packet "LIST_FILES" to port 8888
Response: "OK: file1.bin,<size1>;file2.bin,<size2>;..."

Example:
  Request:  LIST_FILES
  Response: OK: animation_001.bin,2048;animation_002.bin,4096
```

**Implementation:**

```cpp
void handle_list_files() {
    if (!m_sd || !m_sd->isInitialized()) {
        send_reply("ERROR:SD not initialized");
        return;
    }

    std::vector<SDManager::FileInfo> files;
    if (!m_sd->listFiles("/", files)) {
        send_reply("ERROR:Failed to list files");
        return;
    }

    // Build response: "OK: file1.bin,size1;file2.bin,size2;..."
    String response = "OK: ";
    for (size_t i = 0; i < files.size(); i++) {
        if (!files[i].isDir) {
            if (i > 0) response += ";";
            response += files[i].name;
            response += ",";
            response += (uint32_t)files[i].size;
        }
    }
    // UDP packet limit ~1500 bytes
    if (response.length() > 1400) {
        response = response.substring(0, 1400) + "...";
    }
    send_reply(response.c_str());
}
```

### 2. main.cpp (Updated)

**Pass SDManager to ConfigServer:**

```cpp
// Before:
g_cfg.begin();

// After:
g_cfg.begin(&g_sd);  // Pass SDManager reference
```

This allows ConfigServer to access:

- `m_sd->getUsedSpace()` — get used bytes
- `m_sd->getTotalSpace()` — get total bytes
- `m_sd->listFiles("/", files)` — list files in root

---

## 🎯 Complete Command List

| Command           | Purpose                | Response                                      |
| ----------------- | ---------------------- | --------------------------------------------- |
| `GET_INFO`        | Device info            | `OK:ESP32:<IP> WS:<port> TD:<target> Ver:1.0` |
| `SET_IP:<ip>`     | Set target IP          | `OK:<new_ip>`                                 |
| `SET_PORT:<port>` | Set target port        | `OK:<port>`                                   |
| `RESET`           | Restart device         | `OK:RESET` (then restarts)                    |
| `SCAN`            | Heartbeat for scanning | `SCAN:ESP32_at_<IP>_alive`                    |
| `GET_STORAGE`     | SD card info           | `OK: used=<bytes>, total=<bytes>`             |
| `LIST_FILES`      | Animation files        | `OK: file1.bin,<size>;file2.bin,<size>;...`   |

---

## 🧪 Testing

### Test with Terminal

```bash
# Test GET_STORAGE
nc -u 192.168.1.241 8888
> GET_STORAGE
< OK: used=123456, total=7540000000

# Test LIST_FILES
nc -u 192.168.1.241 8888
> LIST_FILES
< OK: animation_001.bin,2048;animation_002.bin,4096
```

### Test with Python

```python
import socket

def test_cmd(ip, cmd):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(1.0)
    try:
        sock.sendto(cmd.encode(), (ip, 8888))
        reply, _ = sock.recvfrom(512)
        return reply.decode()
    finally:
        sock.close()

# Test storage
print(test_cmd("192.168.1.241", "GET_STORAGE"))
# Output: OK: used=123456, total=7540000000

# Test file list
print(test_cmd("192.168.1.241", "LIST_FILES"))
# Output: OK: animation_001.bin,2048;animation_002.bin,4096
```

### Test with Web UI

1. **Backend must be running:**

   ```bash
   python backend.py
   ```

2. **Click "Refresh" button** in Data Management tab

3. **Check console logs (F12):**

   ```
   [12:34:56] INFO: Getting storage info from 192.168.1.241...
   [12:34:56] INFO: [OK] Storage: 123.46 MB / 7540.00 MB
   [12:34:56] Found 2 animation file(s)
   ```

4. **Verify display:**
   - Storage Used: 123.46 MB
   - Free Space: 7416.54 MB
   - Progress bar: 1.6%
   - Stored Animations: animation_001.bin — 2.0 KB, animation_002.bin — 4.0 KB

---

## 📋 Build & Upload

**Compile code:**

```bash
cd /path/to/fall
platformio run
```

**Upload to ESP32:**

```bash
platformio run --target upload
```

**Monitor serial output:**

```bash
platformio device monitor
```

**Expected output:**

```
[SETUP] Starting UDP Config server...
[CFG] ✓ UDP Config server on port 8888
```

---

## 🔗 Integration Flow

```
Web UI
  ↓
Click "Refresh"
  ↓
Backend: GET /api/storage/192.168.1.241
  ↓
Backend: send UDP "GET_STORAGE" to port 8888
  ↓
ESP32 ConfigServer: receive "GET_STORAGE"
  ↓
ESP32 SDManager: getUsedSpace() & getTotalSpace()
  ↓
ESP32: send reply "OK: used=123456, total=7540000000"
  ↓
Backend: parse response → JSON
  ↓
Web UI: display "123.46 MB / 7540 MB"
```

---

## ⚠️ Important Notes

1. **SDManager must be initialized** before ConfigServer uses it
   - Order in setup(): Initialize SD first, then start ConfigServer

2. **UDP packet size limit** is ~1500 bytes
   - Response is truncated if file list exceeds 1400 bytes

3. **File listing from root directory only** (`/`)
   - Can be extended to list recursively if needed

4. **Encoding issue on Windows**
   - Backend already handles encoding fallback

---

## 🚀 Next Steps

1. ✅ **Upload new firmware** to ESP32
2. ✅ **Restart ESP32** (power cycle or RESET button)
3. ✅ **Test commands** with `set_config.py` or `nc -u`
4. ✅ **Open web UI** → Data Management tab
5. ✅ **Click "Refresh"** → verify storage info displays
6. ✅ **Create animation** → click "Send to ESP32"
7. ✅ **Click "Refresh"** again → verify file list updates

---

## 📞 Troubleshooting

### "ERROR:SD not initialized"

- **Cause:** SD card not detected when ConfigServer started
- **Fix:** Check SD card connection, restart ESP32

### File list shows "..."

- **Cause:** Too many files, response truncated
- **Fix:** Filter file list or increase buffer size (expert mode)

### No response to GET_STORAGE

- **Cause:** Command not recognized
- **Fix:** Ensure new firmware uploaded, check serial logs

### Storage shows "0 MB / 0 MB"

- **Cause:** SDManager methods not working correctly
- **Fix:** Check SDManager::getTotalSpace() implementation

---

**Updated:** 2026-04-03  
**Status:** ✅ Code ready for deployment  
**Next:** Upload firmware and test with web UI
