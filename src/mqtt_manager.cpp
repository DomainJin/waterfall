#include "mqtt_manager.h"
#include "valve_driver.h"   // allValvesOff(), applyFrame()

MQTTManager* MQTTManager::_instance = nullptr;

MQTTManager::MQTTManager() : _client(_wifiClient) {
    _instance = this;
}

void MQTTManager::begin(const char* broker,
                         uint16_t    port,
                         const char* user,
                         const char* password,
                         const char* clientId)
{
    _broker   = broker;
    _port     = port;
    _user     = user;
    _password = password;
    _clientId = clientId;

#ifndef MQTT_USE_PLAIN_CLIENT
    // Bỏ qua xác thực CA cert (dùng cho development)
    // Production: _wifiClient.setCACert(root_ca_pem);
    _wifiClient.setInsecure();
#endif

    _client.setServer(_broker, _port);
    _client.setCallback(_onMessage);
    _client.setBufferSize(2048);   // frame data có thể lớn

    Serial.printf("[MQTT] Cấu hình broker: %s:%u\n", _broker, _port);
}

void MQTTManager::tick() {
    if (WiFi.status() != WL_CONNECTED) return;

    if (!_client.connected()) {
        uint32_t now = millis();
        if (now - _lastReconnect >= RECONNECT_INTERVAL) {
            _lastReconnect = now;
            _reconnect();
        }
        return;
    }

    _client.loop();
}

void MQTTManager::_reconnect() {
    Serial.printf("[MQTT] Đang kết nối %s:%u...\n", _broker, _port);

    String willPayload = "{\"online\":false,\"name\":\"" + String(_clientId) + "\"}";
    bool ok = _client.connect(_clientId, _user, _password,
                               MQTT_TOPIC_STATUS,          // will topic
                               1,                           // will QoS
                               true,                        // will retain
                               willPayload.c_str());        // will payload

    if (ok) {
        Serial.println("[MQTT] Kết nối thành công");

        // Subscribe các topic nhận lệnh
        _client.subscribe(MQTT_TOPIC_VALVE,  1);
        _client.subscribe(MQTT_TOPIC_STREAM, 1);

        // Thông báo online kèm IP
        String ip = WiFi.localIP().toString();
        String status = "{\"online\":true,\"name\":\"" + String(_clientId) + "\",\"ip\":\"" + ip + "\"}";
        publishStatus(status);
    } else {
        Serial.printf("[MQTT] Thất bại, rc=%d. Thử lại sau %lus\n",
                      _client.state(), RECONNECT_INTERVAL / 1000);
    }
}

void MQTTManager::publishStatus(const String& json) {
    if (_client.connected()) {
        _client.publish(MQTT_TOPIC_STATUS, json.c_str(), /*retain=*/true);
    }
}

// ── Static message callback ────────────────────────────────────────────────
void MQTTManager::_onMessage(char* topic, byte* payload, unsigned int length) {
    if (!_instance) return;

    String topicStr(topic);
    String payloadStr;
    payloadStr.reserve(length);
    for (unsigned int i = 0; i < length; i++) {
        payloadStr += (char)payload[i];
    }

    Serial.printf("[MQTT] Nhận: %s → %s\n", topic, payloadStr.c_str());

    if (_instance->_callback) {
        _instance->_callback(topicStr, payloadStr);
    }
}
