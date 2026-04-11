# SD Card Module - Hướng Dẫn Sử Dụng

## 📁 Cấu Trúc Module

Module SD card được chia thành 3 phần chính:

### 1. **sd_manager.h / sd_manager.cpp**

- **Lớp chính**: `SDManager`
- **Chức năng**: Quản lý toàn bộ hoạt động SD card
- **Tính năng**:
  - ✅ Khởi tạo SD card (400kHz cho ổn định)
  - ✅ Đọc/ghi file
  - ✅ Liệt kê thư mục
  - ✅ Xóa/tạo file/thư mục
  - ✅ Lấy thông tin card (dung lượng, loại, v.v.)

### 2. **sd_web_handler.h**

- **Lớp**: `SDWebHandler`
- **Chức năng**: Xử lý các request web (HTTP/WebSocket)
- **Endpoints** (sẽ tích hợp vào TCP server sau):
  - `GET /files?dir=<path>` → Liệt kê file (JSON)
  - `GET /file?path=<filepath>` → Đọc nội dung file
  - `POST /file` → Ghi file
  - `DELETE /file?path=<path>` → Xóa file
  - `GET /sd/info` → Thông tin card

### 3. **main.cpp**

- Khởi tạo global object: `SDManager g_sd`
- Gọi `g_sd.begin()` trong `setup()`
- Sẵn sàng sử dụng: `g_sd.readFile()`, `g_sd.writeFile()`, v.v.

---

## 🚀 Cách Sử Dụng

### **1. Khởi tạo trong setup()**

```cpp
g_sd.begin(5, 400000);  // CS pin 5, Speed 400kHz
```

### **2. Viết dữ liệu vào file**

```cpp
// Ghi (ghi đè file)
g_sd.writeFile("/data.txt", "Hello World", false);

// Ghi thêm (append)
g_sd.writeFile("/log.txt", "New entry\n", true);
```

### **3. Đọc từ file**

```cpp
String content = g_sd.readFile("/data.txt");
Serial.println(content);
```

### **4. Liệt kê file trong thư mục**

```cpp
std::vector<SDManager::FileInfo> files;
g_sd.listFiles("/", files);

for (const auto& f : files) {
    Serial.printf("%s (%u bytes, isDir=%d)\n",
                  f.name.c_str(), f.size, f.isDir);
}
```

### **5. Thao tác file/thư mục**

```cpp
g_sd.createDir("/effects");          // Tạo thư mục
g_sd.renameFile("/old.txt", "/new.txt");  // Rename
g_sd.deleteFile("/temp.txt");        // Xóa file
```

### **6. Lấy thông tin card**

```cpp
g_sd.printCardInfo();

uint64_t totalBytes = g_sd.getTotalSpace();
uint64_t usedBytes = g_sd.getUsedSpace();
Serial.printf("Dung lượng trống: %llu MB\n",
              (totalBytes - usedBytes) / (1024 * 1024));
```

---

## 📝 Cấu Trúc File Được Khuyến Nghị

Trên thẻ SD của bạn, tạo như sau (cho ứng dụng nạp hiệu ứng):

```
/
├── /effects/          ← Lưu các file hiệu ứng
│   ├── effect1.bin
│   ├── effect2.bin
│   └── effects.json   ← Metadata tất cả hiệu ứng
│
├── /logs/             ← Lưu log hoạt động
│   └── current.log
│
├── /config/           ← Lưu cấu hình
│   └── settings.json
│
└── README.txt         ← Hướng dẫn
```

---

## 🌐 Tích Hợp Web API (Bước Tiếp Theo)

Để truy cập file qua web, bạn cần:

1. **Thêm `SDWebHandler` vào `tcp_server.h`**:

   ```cpp
   #include "sd_web_handler.h"

   class TcpServer {
   private:
       SDWebHandler m_sdHandler;  // Thêm này
   };
   ```

2. **Xử lý WebSocket messages** khi nhận "sd" commands:

   ```cpp
   // Trong tcp_server.cpp
   if (command == "list_files") {
       String path = doc["path"] | "/";
       String response = m_sdHandler.handleListFiles(path.c_str());
       // Gửi response về client
   }
   ```

3. **Client JavaScript** có thể gọi:
   ```javascript
   ws.send(
     JSON.stringify({
       type: "sd",
       command: "list_files",
       path: "/",
     }),
   );
   ```

---

## 🔧 Pinout ESP32-SPI-SD

Kết nối như sau (mặc định):

| ESP32  | SD Card Module | Ghi chú     |
| ------ | -------------- | ----------- |
| GPIO23 | MOSI           | Data out    |
| GPIO19 | MISO           | Data in     |
| GPIO18 | CLK            | Clock       |
| GPIO5  | CS             | Chip Select |
| GND    | GND            |             |
| 3.3V   | VCC            |             |

---

## ⚠️ Lưu Ý

- **Tốc độ SPI**: 400kHz được khuyến nghị để ổn định. Có thể tăng lên 1-4MHz khi ổn định.
- **Độ ổn định**: Nếu gặp lỗi, hạ tốc độ SPI thêm (200kHz).
- **Định dạng**: SD card nên là FAT32 cho tương thích tối đa.
- **Cỡ file**: Tránh ghi những file quá lớn (>1MB) cùng lúc để không block luồng chính.

---

## 📚 Tài Liệu Thêm

- [Arduino SD Library](https://www.arduino.cc/en/Reference/SD)
- [ESP32 SPI](https://randomnwaysblog.wordpress.com/2020/09/13/configure-esp32-spi-for-sd-card/)
- [ArduinoJson](https://arduinojson.org/)

---

**Tiếp theo**: Chờ bạn mô tả chi tiết cấu trúc dữ liệu hiệu ứng và flow nạp để hoàn thiện module! 🎯
