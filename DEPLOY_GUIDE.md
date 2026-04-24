# Hướng dẫn Deploy — Waterfall Control System

## Tổng quan kiến trúc

```
Internet User
    │
    ▼
[Nginx :443 HTTPS]  ←── Let's Encrypt SSL
    │
    ├── /           → Flask backend (Gunicorn :5000)
    │                   ├── REST API (/api/devices, /api/cmd, ...)
    │                   └── MQTT Relay (paho-mqtt)
    │
    └── MQTT Broker (Mosquitto :1883 plain / :8883 TLS)
            │
            └──── ESP32 (waterfall-xxxxxx)
                    ├── MQTT client (PubSubClient)
                    └── WebSocket server :3333 (LAN direct)

LAN User
    │
    ├── WebSocket ws://ESP_IP:3333  ← streaming animation (binary frames, <5ms)
    └── HTTPS → /api/cmd → MQTT → ESP32 (lệnh điều khiển, ~200ms)
```

---

## Thông tin hạ tầng

| Thành phần | Giá trị |
|---|---|
| VPS | DigitalOcean Singapore, 512MB RAM, Ubuntu 22.04 |
| IP VPS | 139.59.107.54 |
| Domain | waterfall.domainjin.io.vn |
| DNS | ZoneDNS (ns1-ns4.zonedns.vn) |
| MQTT User | waterfall |
| MQTT Port (plain) | 1883 |
| MQTT Port (TLS) | 8883 |
| WebSocket (LAN) | ws://ESP_IP:3333 |

---

## Cấu trúc thư mục deploy

```
deploy/
├── docker-compose.yml       # Orchestration: nginx + backend + mosquitto + certbot
├── Dockerfile               # Flask backend image
├── .env                     # Biến môi trường (không commit)
├── nginx/
│   └── default.conf         # Reverse proxy + SSL config
└── mosquitto/
    ├── mosquitto.conf        # MQTT broker config
    ├── passwd               # MQTT credentials (tạo bằng mosquitto_passwd)
    └── certs/               # TLS certs copy từ Let's Encrypt
        ├── fullchain.pem
        └── privkey.pem
```

### File `.env` (tạo thủ công trên VPS)

```env
FLASK_SECRET_KEY=<random string>
MQTT_BROKER=mosquitto
MQTT_PORT=1883
MQTT_USER=waterfall
MQTT_PASSWORD=<password>
```

---

## Các bước deploy lần đầu

### 1. Chuẩn bị VPS

```bash
# Cài Docker
curl -fsSL https://get.docker.com | sh
apt install -y docker-compose-plugin

# Tạo swap 1GB (bắt buộc cho VPS 512MB RAM)
fallocate -l 1G /swapfile
chmod 600 /swapfile
mkswap /swapfile
swapon /swapfile
echo '/swapfile none swap sw 0 0' >> /etc/fstab

# Clone repo
git clone <repo_url> ~/fall
cd ~/fall/deploy
cp .env.example .env
nano .env   # điền thông tin thực
```

### 2. Tạo MQTT password

```bash
# Tạo file passwd trong container (tránh lỗi /dev/stdout)
docker run --rm -v $(pwd)/mosquitto:/mosquitto/config eclipse-mosquitto \
  mosquitto_passwd -b -c /mosquitto/config/passwd waterfall <password>
```

### 3. Lấy SSL certificate (Let's Encrypt)

```bash
# Dừng nginx nếu đang chạy (cần port 80 trống)
docker compose stop nginx

# Chạy certbot standalone
docker compose run --rm -p 80:80 \
  --entrypoint "certbot certonly --standalone \
    -d waterfall.domainjin.io.vn \
    --email admin@domainjin.io.vn \
    --agree-tos --non-interactive" certbot

# Copy cert sang mosquitto (cho MQTT TLS)
docker compose run --rm --entrypoint sh certbot -c "
  cp /etc/letsencrypt/live/waterfall.domainjin.io.vn/fullchain.pem /etc/letsencrypt/certs/
  cp /etc/letsencrypt/live/waterfall.domainjin.io.vn/privkey.pem /etc/letsencrypt/certs/
"
chmod 644 mosquitto/certs/privkey.pem
```

### 4. Khởi động toàn bộ stack

```bash
docker compose up -d
docker compose logs -f   # kiểm tra
```

### 5. Redeploy sau khi update code

```bash
cd ~/fall && git pull
cd deploy
docker compose build --no-cache backend
docker compose up -d backend
```

---

## Lỗi đã gặp và cách xử lý

### Lỗi 1: `certbot` — webroot challenge thất bại (Connection refused)

**Nguyên nhân:** Nginx chưa chạy → không serve `.well-known/acme-challenge/`

**Fix:** Dùng `--standalone` thay vì `--webroot` — certbot tự mở HTTP server trên port 80:
```bash
docker compose run --rm -p 80:80 --entrypoint "certbot certonly --standalone ..." certbot
```

---

### Lỗi 2: `mosquitto_passwd` — không ghi được `/dev/stdout`

**Nguyên nhân:** Trong Docker, `/dev/stdout` không writable cho mosquitto_passwd

**Fix:** Mount volume và ghi thẳng vào file:
```bash
docker run --rm -v $(pwd)/mosquitto:/mosquitto/config eclipse-mosquitto \
  mosquitto_passwd -b -c /mosquitto/config/passwd waterfall <password>
```

---

### Lỗi 3: Mosquitto — `Duplicate password_file` / `chown: Read-only file system`

**Nguyên nhân:**
- `password_file` khai báo 2 lần trong config
- Mount volume với `:ro` (read-only) nên mosquitto không thể chown

**Fix trong `mosquitto.conf`:**
```conf
# Đặt auth settings ở GLOBAL scope, không lặp lại trong listener block
allow_anonymous false
password_file /mosquitto/config/passwd
```

**Fix trong `docker-compose.yml`:** Bỏ `:ro` khỏi mosquitto config mounts

---

### Lỗi 4: Mosquitto — không tìm thấy certs

**Nguyên nhân:** Volume `mosquitto/certs` chưa được mount vào container

**Fix trong `docker-compose.yml`:**
```yaml
volumes:
  - ./mosquitto/mosquitto.conf:/mosquitto/config/mosquitto.conf
  - ./mosquitto/passwd:/mosquitto/config/passwd
  - ./mosquitto/certs:/mosquitto/certs          # ← thêm dòng này
  - mosquitto_data:/mosquitto/data
  - mosquitto_log:/mosquitto/log
```

---

### Lỗi 5: ESP32 TLS rc=-2 (handshake thất bại)

**Nguyên nhân:** `WiFiClientSecure` trên ESP32 không handshake được với cert Let's Encrypt

**Workaround hiện tại:** Dùng plain MQTT port 1883 thay vì TLS 8883

```cpp
// include/config.h
#define MQTT_BROKER_PORT  1883   // plain (đổi lại 8883 sau khi fix TLS)
```

```cpp
// include/mqtt_manager.h
#define MQTT_USE_PLAIN_CLIENT   // comment out để dùng TLS
```

**Trạng thái:** Chưa giải quyết — ESP32 TLS cần upload CA cert hoặc dùng fingerprint

---

### Lỗi 6: OOM kills — VPS 512MB bị kill process

**Nguyên nhân:** `chown -R appuser:appuser /app` trong Dockerfile đọc toàn bộ filesystem khi build

**Fix trong `Dockerfile`:** Xóa lệnh `chown -R`, giảm gunicorn workers xuống 1:
```dockerfile
# Xóa dòng này:
# RUN chown -R appuser:appuser /app
```

```bash
CMD ["gunicorn", "--workers", "1", ...]
```

---

### Lỗi 7: MQTT relay không start khi dùng Gunicorn

**Nguyên nhân:** Gunicorn import module nhưng không gọi `main()`, nên `mqtt_relay.start()` trong `main()` không được gọi

**Fix trong `backend.py`:**
```python
mqtt_relay = MQTTRelay()
mqtt_relay.start()   # ← gọi ở module level, ngoài main()
```

---

### Lỗi 8: Nhiều ESP32 dùng cùng clientId

**Nguyên nhân:** Khi ESP32 thứ 2 kết nối với cùng `clientId`, broker kick ESP32 thứ 1

**Fix trong `main.cpp`:**
```cpp
uint8_t mac[6];
WiFi.macAddress(mac);
char clientId[32];
snprintf(clientId, sizeof(clientId), "waterfall-%02x%02x%02x", mac[3], mac[4], mac[5]);
g_mqtt.begin(MQTT_BROKER_HOST, MQTT_BROKER_PORT, MQTT_USER, MQTT_PASSWORD, clientId);
```

---

### Lỗi 9: Device bị flag offline trong khi ESP vẫn online

**Nguyên nhân:** Backend có timeout 60s tự đổi trạng thái thành offline

**Fix:** Xóa timeout logic — chỉ dựa vào MQTT Will Message:
```python
# Will Message (ESP32 tự publish khi mất kết nối):
# {"online": false, "name": "waterfall-xxxxxx"}
# Không cần timeout thủ công
```

---

### Lỗi 10: MQTT mode — spam ALL_OFF khi bấm stream/test buttons

**Nguyên nhân:** `sendReset()` trong MQTT mode map sang ALL_OFF; các hàm streaming gọi `sendReset()` mà không check mode

**Fix:** Thêm guard vào tất cả hàm streaming:
```javascript
function startStream() {
  if (connMode === 'mqtt') {
    log("Streaming không khả dụng qua MQTT", "log-err");
    return;
  }
  // ...
}
```

---

## Protocol Dual-Path (LAN + Cloud)

### Chọn đường tự động

| Truy cập | Mode | Giao thức | Latency |
|---|---|---|---|
| `file://` hoặc `http://` | LAN | WebSocket `ws://ESP_IP:3333` | <5ms |
| `https://waterfall...` | MQTT | POST `/api/cmd` → MQTT Broker → ESP32 | ~200ms |

### Command format (JSON — dùng cho cả LAN WS text và MQTT)

```json
{"cmd": "ALL_OFF"}
{"cmd": "ALL_ON"}
{"cmd": "SET", "bits": "FF00FF00FF00FF00FF00"}
{"cmd": "STREAM_STOP"}
```

Với `target` (điều khiển device cụ thể):
```json
{"cmd": "ALL_OFF", "target": "waterfall-b87524"}
```

### Streaming animation (chỉ LAN)

Binary WebSocket frame 14 bytes:
```
[ts_ms : 4 bytes LE] [bits : 10 bytes, 1 byte/board]

TS_RESET = 0xFFFFFFFF  → xóa queue, tắt van
TS_START = 0xFFFFFFFE  → bắt đầu phát
```

### MQTT Topics

| Topic | Chiều | Nội dung |
|---|---|---|
| `waterfall/status` | ESP32 → Cloud | `{"online":true,"name":"...","ip":"..."}` |
| `waterfall/cmd/valve` | Cloud → ESP32 | `{"cmd":"ALL_OFF"\|"ALL_ON"\|"SET","bits":"..."}` |
| `waterfall/cmd/stream` | Cloud → ESP32 | `{"frame":"hex14bytes"}` |

### Backend API

| Endpoint | Method | Mô tả |
|---|---|---|
| `/api/devices` | GET | Danh sách ESP32 từ MQTT |
| `/api/cmd` | POST | Gửi lệnh điều khiển qua MQTT |
| `/api/mqtt/status` | GET | Trạng thái kết nối MQTT broker |

---

## Monitoring

### Xem logs realtime

```bash
docker compose logs -f backend      # Flask + MQTT relay
docker compose logs -f mosquitto    # MQTT broker
docker compose logs -f nginx        # Access logs
```

### GoAccess — phân tích traffic nginx

```bash
docker compose exec nginx sh
goaccess /var/log/nginx/access.log --log-format=COMBINED
```

### Kiểm tra thiết bị đang online

```
GET https://waterfall.domainjin.io.vn/api/devices
```

---

## Việc cần làm (TODO)

- [ ] Fix ESP32 TLS kết nối port 8883 (upload CA cert Let's Encrypt vào firmware)
- [ ] Thêm MQTT heartbeat từ ESP32 mỗi 30s (tránh lag offline detection)
- [ ] Auto-renew cert Let's Encrypt (certbot renew + copy sang mosquitto/certs)
