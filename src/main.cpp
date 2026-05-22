#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "frame_queue.h"
#include "valve_driver.h"
#include "tcp_server.h"
#include "scheduler.h"
#include "sd_manager.h"
#include "config_server.h"
#include "ble_config.h"
#include "mqtt_manager.h"
#include "sound_mode.h"

// ============================================================
//  Global objects
// ============================================================
FrameQueue    g_queue;
ValveDriver   g_valve;
TcpServer     g_tcp(g_queue, g_valve);
Scheduler     g_scheduler(g_queue, g_valve, g_tcp);
SoundMode     g_sound;

enum DeviceMode { MODE_STREAM, MODE_SOUND };
DeviceMode    g_mode = MODE_STREAM;
SDManager     g_sd;
ConfigServer  g_cfg;
BLEConfig     g_ble;
MQTTManager   g_mqtt;

// ============================================================
//  WiFi setup
// ============================================================
void setupWiFi() {
    Serial.printf("\n[WiFi] Connecting to %s...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_TIMEOUT_MS) {
            Serial.println("[WiFi] TIMEOUT — WiFi unavailable, continuing anyway");
            break;
        }
        delay(100);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    }
}

// ============================================================
//  Setup
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(3000);  // ← Increased from 2000 to 3000
    Serial.println("\n\n╔═══════════════════════════════════════════════╗");
    Serial.println("║  Water Curtain Valve Control — ESP32         ║");
    Serial.println("║  WebSocket Server (Frame Receiver)           ║");
    Serial.println("╚═══════════════════════════════════════════════╝");
    Serial.flush();

    // Setup hardware
    Serial.printf("\n[SETUP] Initializing hardware...\n");
    Serial.flush();
    g_valve.begin();
    Serial.printf("[SETUP] ✓ Valve driver initialized\n");
    Serial.flush();

    // Setup microphone
    g_sound.begin(MIC_PIN, g_valve);
    Serial.printf("[SETUP] ✓ Sound mode initialized (pin %d)\n", MIC_PIN);
    Serial.flush();

    // Setup BLE FIRST (before WiFi to avoid conflicts)
    Serial.printf("\n[SETUP] Initializing BLE...\n");
    Serial.flush();
    g_ble.begin();
    Serial.printf("[SETUP] ✓ BLE initialized\n");
    Serial.flush();

    // Setup WiFi
    Serial.printf("\n[SETUP] Connecting to WiFi...\n");
    Serial.flush();
    setupWiFi();
    Serial.printf("[SETUP] ✓ WiFi setup done\n");
    Serial.flush();
    
    // Setup SD Card
    Serial.printf("[SETUP] Initializing SD card...\n");
    Serial.flush();
    g_sd.begin(5, 400000);  // CS=5, Speed=400kHz for stability
    Serial.printf("[SETUP] ✓ SD card initialized\n");
    Serial.flush();
    g_sd.printFileList("/");
    Serial.flush();
    
    // Setup WebSocket server
    Serial.printf("[SETUP] Starting WebSocket server on port %d...\n", TCP_PORT);
    Serial.flush();
    g_tcp.begin();
    // Mode switching via WebSocket SET_MODE command
    g_tcp.onModeChange([](const String& mode, const String& pattern, int sensitivity) {
        if (mode == "sound") {
            g_mode = MODE_SOUND;
            g_sound.setSensitivity((uint8_t)sensitivity);
            SoundPattern p = SOUND_RIPPLE;
            if (pattern == "columns") p = SOUND_COLUMNS;
            else if (pattern == "wave") p = SOUND_WAVE;
            g_sound.setPattern(p);
            Serial.printf("[MODE] → SOUND pattern=%s sens=%d\n", pattern.c_str(), sensitivity);
        } else {
            g_mode = MODE_STREAM;
            g_valve.allOff();
            Serial.println("[MODE] → STREAM");
        }
    });
    // Broadcast sound telemetry to connected clients
    g_sound.onTick([](int amplitude, int norm, bool beat) {
        if (!g_tcp.hasClient()) return;
        char buf[64];
        snprintf(buf, sizeof(buf),
                 "{\"type\":\"sound\",\"a\":%d,\"n\":%d,\"b\":%d}",
                 amplitude, norm, beat ? 1 : 0);
        g_tcp.broadcastJSON(buf);
    });
    Serial.printf("[SETUP] ✓ WebSocket server started\n");
    Serial.flush();

    // Setup UDP Config server
    Serial.printf("[SETUP] Starting UDP Config server...\n");
    Serial.flush();
    g_cfg.begin(&g_sd);
    Serial.printf("[SETUP] ✓ UDP Config server started (port 8888)\n");
    Serial.flush();

    // Setup MQTT (kết nối cloud — chỉ khi WiFi thành công)
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[SETUP] Starting MQTT client...\n");
        Serial.flush();
        // Tạo client ID duy nhất từ 3 byte cuối MAC address
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char clientId[32];
        snprintf(clientId, sizeof(clientId), "waterfall-%02x%02x%02x", mac[3], mac[4], mac[5]);
        g_mqtt.begin(MQTT_BROKER_HOST, MQTT_BROKER_PORT,
                     MQTT_USER, MQTT_PASSWORD, clientId);
        // Xử lý lệnh từ cloud
        g_mqtt.onCommand([](const String& topic, const String& payload) {
            Serial.printf("[MQTT] Cmd topic=%s payload=%s\n", topic.c_str(), payload.c_str());

            // Optional target filtering: {"target":"waterfall-abc123",...}
            // If target present and doesn't match our clientId → ignore
            int tgt_idx = payload.indexOf("\"target\":\"");
            if (tgt_idx >= 0) {
                int ts = tgt_idx + 10;
                int te = payload.indexOf('"', ts);
                if (te > ts) {
                    String target = payload.substring(ts, te);
                    if (target != g_mqtt.getClientId()) return;
                }
            }

            if (topic == MQTT_TOPIC_VALVE) {
                // {"cmd":"ALL_OFF"} / {"cmd":"ALL_ON"} / {"cmd":"SET","bits":"FF00..."}
                if (payload.indexOf("ALL_OFF") >= 0) {
                    g_valve.allOff();
                    Serial.println("[MQTT] ALL_OFF executed");
                } else if (payload.indexOf("ALL_ON") >= 0) {
                    g_valve.allOn();
                    Serial.println("[MQTT] ALL_ON executed");
                } else if (payload.indexOf("\"SET\"") >= 0) {
                    // Tìm key "bits": linh hoạt (có hoặc không có space sau dấu ':')
                    // Vì Python json.dumps() mặc định thêm space: "bits": "..."
                    // nhưng compact JSON không có space: "bits":"..."
                    int idx = payload.indexOf("\"bits\":");
                    if (idx >= 0) {
                        // Bỏ qua space/tab giữa ':' và '"'
                        int q1 = payload.indexOf('"', idx + 7);  // tìm quote mở của value
                        if (q1 > 0) {
                            int q2 = payload.indexOf('"', q1 + 1);  // tìm quote đóng
                            if (q2 > q1) {
                                String hex = payload.substring(q1 + 1, q2);
                                if ((int)hex.length() >= NUM_BOARDS * 2) {
                                    uint8_t bits[NUM_BOARDS];
                                    for (int i = 0; i < NUM_BOARDS; i++) {
                                        bits[i] = (uint8_t)strtol(hex.substring(i*2, i*2+2).c_str(), nullptr, 16);
                                    }
                                    g_valve.write(bits);
                                    Serial.printf("[MQTT] SET bits executed: %s\n", hex.c_str());
                                } else {
                                    Serial.printf("[MQTT] SET bits too short: %d < %d (hex=%s)\n",
                                                  hex.length(), NUM_BOARDS*2, hex.c_str());
                                }
                            }
                        }
                    }
                }
            } else if (topic == MQTT_TOPIC_STREAM) {
                // {"frame":"AABBCC..."} — hex encoded frame bytes
                int idx = payload.indexOf("\"frame\":\"");
                if (idx >= 0) {
                    int start = idx + 9;
                    int end   = payload.indexOf("\"", start);
                    if (end > start) {
                        String hex = payload.substring(start, end);
                        int len = hex.length() / 2;
                        if (len == FRAME_BYTES) {
                            uint8_t raw[FRAME_BYTES];
                            for (int i = 0; i < FRAME_BYTES; i++) {
                                raw[i] = strtol(hex.substring(i*2, i*2+2).c_str(), nullptr, 16);
                            }
                            Frame f;
                            memcpy(&f.ts_ms, raw, 4);
                            memcpy(f.bits, raw + 4, NUM_BOARDS);
                            g_queue.push(f);
                            Serial.printf("[MQTT] Frame queued\n");
                        }
                    }
                }
            }
        });
        Serial.printf("[SETUP] ✓ MQTT client started\n");
        Serial.flush();
    }

    Serial.println("\n[SETUP] ✓ Ready!");
    Serial.println("  WebSocket:  ws://<ESP32_IP>:3333");
    Serial.println("  Config UDP: <ESP32_IP>:8888");
    Serial.println("  BLE:        Waterfall_Config");
    Serial.println("  MQTT:       " MQTT_BROKER_HOST);
    Serial.flush();
}

// ============================================================
//  Main loop — PRIORITY: Valve control #1, WebSocket #2
// ============================================================

// Heartbeat timing
static uint32_t last_heartbeat = 0;
const  uint16_t HEARTBEAT_MS = 5000;  // Show every 5 seconds

void loop() {
    uint32_t loop_start = micros();
    
    // ─────────────────────────────────────────────────────
    // PRIORITY 1: Valve control — sound mode or stream mode
    // ─────────────────────────────────────────────────────
    if (g_mode == MODE_SOUND) {
        g_sound.tick();
    } else {
        g_scheduler.tick();
    }
    
    // ─────────────────────────────────────────────────────
    // PRIORITY 2: Handle WebSocket communication (events)
    // (non-blocking event pump)
    // ─────────────────────────────────────────────────────
    g_tcp.tick();
    
    // ─────────────────────────────────────────────────────
    // PRIORITY 3: Handle UDP Config commands
    // (non-blocking config updates)
    // ─────────────────────────────────────────────────────
    g_cfg.tick();
    
    // ─────────────────────────────────────────────────────
    // PRIORITY 4: Handle BLE Config events
    // (non-blocking BLE processing)
    // ─────────────────────────────────────────────────────
    g_ble.tick();

    // ─────────────────────────────────────────────────────
    // PRIORITY 5: Handle MQTT cloud messages
    // ─────────────────────────────────────────────────────
    g_mqtt.tick();

    // ─────────────────────────────────────────────────────
    // Heartbeat: Show ESP is alive every 5 seconds
    // (Optional for debugging - disabled by default)
    // ─────────────────────────────────────────────────────
    // uint32_t now = millis();
    // if (now - last_heartbeat >= HEARTBEAT_MS) {
    //     last_heartbeat = now;
    //     uint32_t loop_time_us = micros() - loop_start;
    //     Serial.printf("[HEARTBEAT] t=%lu ms | Queue: %u | Loop: %lu µs\n",
    //                   now, g_queue.size(), loop_time_us);
    //     Serial.flush();
    // }
    
    // ─────────────────────────────────────────────────────
    // Minimal delay to let WiFi task run
    // (ESP32 has core 0 for WiFi, core 1 for our code)
    // ─────────────────────────────────────────────────────
    // delayMicroseconds(10);  // Or just: yield();
}

