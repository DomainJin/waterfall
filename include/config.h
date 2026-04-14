#pragma once

// ── WiFi ─────────────────────────────────────────────────────
#define WIFI_SSID        "SGM"
#define WIFI_PASSWORD    "19121996"
#define WIFI_TIMEOUT_MS  10000

// ── TCP ──────────────────────────────────────────────────────
#define TCP_PORT         3333

// ── Hardware ─────────────────────────────────────────────────
// Mapping từ schematic (74HC245D A→B → 1K → 74HC595D):
//   A3 → B3 → SH_CP (pin 11, shift clock)
//   A5 → B5 → ST_CP (pin 12, latch)
//   A7 → B7 → DS    (pin 14, serial data)
//
// ESP32 → 74HC245D:
//   GPIO2  → A3 (SH_CP / shift clock)
//   GPIO4  → A5 (ST_CP / latch)
//   GPIO23 → A7 (DS / data) [SHARED WITH SD_MOSI - OK, both outputs]
//
// Daisy-chain: Board1.Q7' → Board2.A7 → ... → Board10.A7

#define PIN_SHCP    2     // GPIO2  → SH_CP (shift clock) [NO CONFLICT with SD]
#define PIN_STCP    4     // GPIO4  → ST_CP (latch)
#define PIN_DS     16    // GPIO23 → DS   (data)

// DIR của 74HC245D nối VCC — A→B luôn enabled, không cần GPIO
#define PIN_DIR     -1

// ─── BOARDS CONFIGURATION ────────────────────────────────────
// Thay đổi NUM_BOARDS để cấu hình số lượng shift register boards
// Mỗi board = 1 shift register = 8 van
// Ví dụ:
//   NUM_BOARDS = 1  →  8 van
//   NUM_BOARDS = 2  → 16 van
//   NUM_BOARDS = 10 → 80 van (DEFAULT - thiết kế gốc)
//   NUM_BOARDS = 16 → 128 van (max, frame size = 4+16)
// NOTE: Frame size cần >= 4 + NUM_BOARDS bytes
// Sau khi thay đổi, upload firmware lại
#define NUM_BOARDS  10
#define NUM_VALVES  80    // NUM_BOARDS * 8 (tự động tính)

// ── Protocol ─────────────────────────────────────────────────
// Frame 14 bytes: [0..3]=ts_ms LE, [4..13]=bits[10]
#define FRAME_BYTES      14
#define TS_RESET         0xFFFFFFFFUL
#define TS_START         0xFFFFFFFEUL

// ── Queue ────────────────────────────────────────────────────
#define QUEUE_LEN        512

// ── Watchdog ─────────────────────────────────────────────────
#define WATCHDOG_MS      5000

// ── MQTT Cloud (để trống nếu không dùng) ─────────────────────
// Điền thông tin VPS cloud của bạn vào đây
#define MQTT_BROKER_HOST  "waterfall.domainjin.io.vn"
#define MQTT_BROKER_PORT  8883        // TLS port
#define MQTT_USER         "waterfall"
#define MQTT_PASSWORD     "Dmc0918273645!"
