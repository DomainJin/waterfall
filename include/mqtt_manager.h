#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// ── MQTT Topics (phải khớp với backend.py) ────────────────────────────────
#define MQTT_TOPIC_STATUS   "waterfall/status"
#define MQTT_TOPIC_VALVE    "waterfall/cmd/valve"
#define MQTT_TOPIC_STREAM   "waterfall/cmd/stream"
#define MQTT_TOPIC_CMD_ALL  "waterfall/cmd/#"

// ── Callback type ─────────────────────────────────────────────────────────
// Được gọi khi nhận lệnh từ cloud: (topic, payload)
using MQTTCommandCallback = std::function<void(const String&, const String&)>;

class MQTTManager {
public:
    MQTTManager();

    // Khởi tạo với thông tin broker
    void begin(const char* broker,
               uint16_t    port,
               const char* user,
               const char* password,
               const char* clientId = "waterfall-esp32");

    // Gọi trong loop() — xử lý kết nối và messages
    void tick();

    // Publish trạng thái lên cloud
    void publishStatus(const String& json);

    // Đăng ký callback nhận lệnh
    void onCommand(MQTTCommandCallback cb) { _callback = cb; }

    bool isConnected() { return _client.connected(); }

private:
    WiFiClientSecure _wifiClient;
    PubSubClient     _client;
    MQTTCommandCallback _callback;

    const char* _broker   = nullptr;
    uint16_t    _port     = 8883;
    const char* _user     = nullptr;
    const char* _password = nullptr;
    const char* _clientId = nullptr;

    uint32_t _lastReconnect = 0;
    static const uint32_t RECONNECT_INTERVAL = 5000;  // ms

    void _reconnect();
    static void _onMessage(char* topic, byte* payload, unsigned int length);
    static MQTTManager* _instance;  // singleton cho static callback
};
