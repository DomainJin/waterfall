# 💾 SD Card Read Fix — Data Management Update

## ❌ Vấn Đề Gốc

Web UI hiển thị:

```
Storage Used: 0.00 MB
Free Space: 4.00 MB
Stored Animations: — No files loaded —
```

Nhưng ESP32 serial log cho thấy:

```
[SDManager] √ SD card initialized successfully
[SDManager] Card size: 7540 MB
[SDManager] Total space: 7530 MB
[SDManager] Used space: 0 MB
[SDManager] √ Listed 2 files from /
```

**Nguyên Nhân:** Backend không trả về thông tin SD card chính xác từ lệnh UDP.

---

## ✅ Giải Pháp Áp Dụng

### 1. Backend (backend.py) — 2 Endpoint Mới

#### Endpoint `/api/storage/<ip>`

**Mục đích:** Lấy thông tin SD card trực tiếp từ ESP32

```python
GET /api/storage/192.168.1.241

Response:
{
  "success": true,
  "device": "192.168.1.241",
  "used_bytes": 123456,
  "total_bytes": 7540000000,
  "used_mb": 123.46,
  "total_mb": 7540.00,
  "used_pct": 1.64
}
```

**Cách hoạt động:**

1. Gửi lệnh `GET_STORAGE` tới ESP32 via UDP port 8888
2. Parse response format: `OK: used=123456, total=7540000`
3. Chuyển đổi bytes → MB + tính %

#### Endpoint `/api/device/<ip>/files`

**Mục đích:** Liệt kê các file animation trên SD card

```python
GET /api/device/192.168.1.241/files

Response:
{
  "success": true,
  "device": "192.168.1.241",
  "files": [
    {"name": "animation_001.bin", "size": 2048, "size_kb": 2.0},
    {"name": "animation_002.bin", "size": 4096, "size_kb": 4.0}
  ],
  "count": 2
}
```

**Cách hoạt động:**

1. Gửi lệnh `LIST_FILES` tới ESP32
2. Parse response: `OK: file1.bin,2048;file2.bin,4096;...`
3. Trả về JSON array với thông tin từng file

### 2. Web UI (index.html) — Cập Nhật

#### Hàm `loadStorageInfo()`

**Thay đổi:**

- Thay vì gọi `/api/device/{ip}/info` → gọi `/api/storage/{ip}`
- Parse response từ `used_mb`, `total_mb`, `used_pct`
- Hiển thị % sử dụng
- Load file list từ `/api/device/{ip}/files`
- Hiển thị danh sách file trong "Stored Animations"
- Update dropdown select cho "Advanced" section

---

## 🔧 Cần Thiết Trên ESP32

### Lệnh UDP Bắt Buộc

ESP32 firmware cần hỗ trợ 2 lệnh mới:

#### 1. `GET_STORAGE`

Trả về thông tin SD card

**Request:** Gửi UDP packet "GET_STORAGE" tới port 8888

**Response:**

```
OK: used=123456, total=7540000000
```

**Code Reference (trong ESP32 firmware):**

```cpp
if (cmd == "GET_STORAGE") {
  unsigned long usedSpace = SD.usedBytes();
  unsigned long totalSpace = SD.totalBytes();

  response = "OK: used=" + String(usedSpace) +
              ", total=" + String(totalSpace);
  sendResponse(response, remoteIP, remotePort);
}
```

#### 2. `LIST_FILES`

Liệt kê các file animation

**Request:** Gửi UDP packet "LIST_FILES" tới port 8888

**Response:**

```
OK: animation_001.bin,2048;animation_002.bin,4096;...
```

**Code Reference (trong ESP32 firmware):**

```cpp
if (cmd == "LIST_FILES") {
  response = "OK: ";
  File root = SD.open("/");
  File file = root.openNextFile();
  bool first = true;
  while (file) {
    if (!first) response += ";";
    response += file.name() + "," + String(file.size());
    first = false;
    file = root.openNextFile();
  }
  sendResponse(response, remoteIP, remotePort);
}
```

---

## 📊 Kiểm Tra Kết Quả

### 1. Backend Log (Khi GET_STORAGE thành công)

```
[12:34:56] INFO: Getting storage info from 192.168.1.241...
[12:34:56] INFO: [OK] Storage: 123.46 MB / 7540.00 MB
```

### 2. Web UI Display

```
Data Management Tab:
├─ Storage Status:
│  ├─ Storage Used: 123.46 MB
│  ├─ Free Space: 7416.54 MB
│  └─ [████░░░░░░░░░░░░░░] 1.6%
│
├─ Stored Animations:
│  ├─ animation_001.bin — 2.0 KB
│  ├─ animation_002.bin — 4.0 KB
│  └─ (total: 2 files)
│
└─ Console Log:
   [12:34:56] Storage: 123.46MB / 7540.00MB (1.6%)
   [12:34:56] Found 2 animation file(s)
```

### 3. Browser Console Check (F12)

```
// Test storage endpoint
fetch('http://localhost:5000/api/storage/192.168.1.241')
  .then(r => r.json())
  .then(d => console.log(d))

// Output should show:
{
  success: true,
  used_mb: 123.46,
  total_mb: 7540.00,
  used_pct: 1.64
}
```

---

## 🚨 Troubleshooting

### "Backend not available" Error

**Nguyên nhân:** Backend server không chạy hoặc không kết nối được

**Giải pháp:**

```bash
# Terminal 1: Khởi động backend
python backend.py

# Kiểm tra output:
# [12:34:56] INFO: Flask running on http://localhost:5000
```

### "Device not responding to GET_STORAGE" Error

**Nguyên nhân:** ESP32 firmware không hỗ trợ lệnh `GET_STORAGE`

**Giải pháp:**

1. Kiểm tra `set_config.py` để xem hỗ trợ lệnh nào
2. Cập nhật ESP32 firmware để thêm `GET_STORAGE` command
3. Hoặc sử dụng `/api/device/{ip}/info` fallback (parse GET_INFO response)

### Storage Shows "Used space: 0 MB"

**Nguyên nhân:** Chưa upload animation nào

**Bình thường:** Lần đầu tiên thường có 0 MB vì chưa lưu file

**Giải pháp:** Tạo animation và send tới device, rồi click "Refresh"

### File List Empty "— No files loaded —"

**Nguyên nhân 1:** Chưa upload file nào  
**Giải pháp:** Upload animation trước

**Nguyên nhân 2:** Device không respond  
**Giải pháp:** Kiểm tra device connection, click "Refresh"

**Nguyên nhân 3:** LIST_FILES lệnh không hỗ trợ  
**Giải pháp:** Thêm LIST_FILES command vào ESP32 firmware

---

## 📝 Files Changed

### backend.py

- ✅ Endpoint mới: `/api/storage/<ip>` (Parse GET_STORAGE)
- ✅ Endpoint cái thiện: `/api/device/<ip>/files` (Parse LIST_FILES)
- ✅ Enhanced `/api/device/<ip>/info` (Parse GET_INFO)

### index.html

- ✅ Updated `loadStorageInfo()` (use `/api/storage` endpoint)
- ✅ Auto-load file list (from `/api/device/{ip}/files`)
- ✅ Display storage % and details
- ✅ Update file select dropdown

---

## 🎯 Tiếp Theo

### Cần Làm Ngay

1. **Kiểm tra ESP32 hỗ trợ lệnh nào:**

   ```bash
   # Chạy set_config.py test
   python set_config.py 192.168.1.241
   ```

2. **Thêm missing commands vào ESP32 nếu cần:**
   - `GET_STORAGE` — trả về used/total bytes
   - `LIST_FILES` — trả về danh sách file

3. **Kiểm tra hoạt động:**
   - Click "Refresh" button
   - Xem console log
   - Kiểm tra storage bar nhập nhằng

### Nâng Cao (Optional)

- [ ] Upload file tới SD card qua HTTP endpoint
- [ ] Delete file từ web UI
- [ ] Real-time storage monitoring
- [ ] Backup/restore animations

---

## 🔗 Related Documentation

- [DATA_MANAGEMENT_GUIDE.md](DATA_MANAGEMENT_GUIDE.md)
- [BACKEND_GUIDE.md](BACKEND_GUIDE.md)
- [TROUBLESHOOTING_SUMMARY.md](TROUBLESHOOTING_SUMMARY.md)

---

**Updated:** 2026-04-03  
**Status:** ✅ Backend & Frontend synced  
**Next Step:** Test with running ESP32 device + `GET_STORAGE` & `LIST_FILES` commands
