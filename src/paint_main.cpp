#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WiFiUDP.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include "paint_config.h"

// ── WiFi ────────────────────────────────────────────────────────
#define WIFI_SSID     "SGM"
#define WIFI_PASSWORD "19121996"

// ── UDP config server ────────────────────────────────────────────
#define CFG_PORT      8888
#define TD_PORT       12345
#define DEFAULT_TD_IP "192.168.1.81"

// ── NeoPixel ─────────────────────────────────────────────────────
#define PIN_LED   18
#define NUM_LED   7

// ── Rotary encoder + button ──────────────────────────────────────
#define PIN_A     32
#define PIN_B     33
#define PIN_SW    25

// ── MQTT (same broker as waterfall) ──────────────────────────────
#define MQTT_HOST "waterfall.domainjin.io.vn"
#define MQTT_PORT 1883
#define MQTT_USER "waterfall"
#define MQTT_PASS "Dmc0918273645!"

#define TOPIC_PAINT_STATUS "paint/status"
#define TOPIC_PAINT_CMD    "paint/cmd"

// ── Palette (20 colors) ───────────────────────────────────────────
const uint32_t colors[20] = {
  0xFF0000, 0xFF7F00, 0xFFFF00, 0x7FFF00, 0x00FF00,
  0x00FF7F, 0x00FFFF, 0x007FFF, 0x0000FF, 0x7F00FF,
  0xFF00FF, 0xFF007F, 0xFFFFFF, 0xAAAAAA, 0x555555,
  0xFF5555, 0x55FF55, 0x5555FF, 0xFFFFAA, 0xAAFFFF
};

// ── Objects ───────────────────────────────────────────────────────
Adafruit_NeoPixel strip(NUM_LED, PIN_LED, NEO_GRB + NEO_KHZ800);
WiFiUDP      udp_config;
WiFiUDP      udp_td;
ConfigModule configModule;
WiFiClient   wifi_client;
PubSubClient mqtt_client(wifi_client);

// ── State ─────────────────────────────────────────────────────────
int colorIndex = 0;
int lastA = 0, lastB = 0;
int level = 0;

unsigned long lastUpdate         = 0;
unsigned long last_config_check  = 0;
unsigned long last_send_to_td    = 0;
unsigned long last_mqtt_reconnect = 0;
unsigned long last_mqtt_publish  = 0;

const int stepDelay             = 60;
const int config_check_interval = 100;
const int send_to_td_interval   = 500;
const int mqtt_reconnect_interval = 5000;
const int mqtt_heartbeat_interval = 10000;

int last_button_state = 0;
int last_color_index  = -1;
int last_level        = -1;

// ── Forward declarations ──────────────────────────────────────────
void updateLED();
void publish_paint_status(int btn, int color_idx, int lv);

// ── MQTT callback (remote color/level control) ────────────────────
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    char msg[128] = {0};
    memcpy(msg, payload, min(length, (unsigned int)127));
    Serial.printf("[MQTT] %s → %s\n", topic, msg);

    char* p = strstr(msg, "\"color\":");
    if (p) {
        int idx = atoi(p + 8);
        if (idx >= 0 && idx <= 19) { colorIndex = idx; updateLED(); }
    }
    p = strstr(msg, "\"level\":");
    if (p) {
        int lv = atoi(p + 8);
        if (lv >= 0 && lv < NUM_LED) { level = lv; updateLED(); }
    }
}

// ── MQTT connect / reconnect ──────────────────────────────────────
void mqtt_connect() {
    char client_id[32];
    snprintf(client_id, sizeof(client_id), "paint-%06X",
             (uint32_t)(ESP.getEfuseMac() & 0xFFFFFF));

    // LWT so broker marks device offline on disconnect
    char will[80];
    snprintf(will, sizeof(will),
             "{\"online\":false,\"name\":\"%s\"}", client_id);

    if (mqtt_client.connect(client_id, MQTT_USER, MQTT_PASS,
                            TOPIC_PAINT_STATUS, 1, true, will)) {
        Serial.printf("[MQTT] Connected as %s\n", client_id);
        mqtt_client.subscribe(TOPIC_PAINT_CMD, 1);

        // Announce online
        char online_msg[128];
        snprintf(online_msg, sizeof(online_msg),
                 "{\"online\":true,\"name\":\"%s\",\"ip\":\"%s\","
                 "\"btn\":%d,\"color\":%d,\"level\":%d,\"hex\":\"#%06X\"}",
                 client_id, WiFi.localIP().toString().c_str(),
                 last_button_state, colorIndex, level,
                 (unsigned int)colors[colorIndex]);
        mqtt_client.publish(TOPIC_PAINT_STATUS, online_msg, true);
    } else {
        Serial.printf("[MQTT] Failed rc=%d\n", mqtt_client.state());
    }
}

// ── Publish current paint state ────────────────────────────────────
void publish_paint_status(int btn, int color_idx, int lv) {
    if (!mqtt_client.connected()) return;
    uint32_t c = colors[color_idx];
    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"online\":true,\"btn\":%d,\"color\":%d,\"level\":%d,\"hex\":\"#%06X\"}",
             btn, color_idx, lv, (unsigned int)c);
    mqtt_client.publish(TOPIC_PAINT_STATUS, payload, true);
    Serial.printf("[MQTT] Published: %s\n", payload);
}

// ── Send state to Target Device via UDP ───────────────────────────
void sendToTD(int btn_state, int color, int level_val) {
    const char* td_ip = configModule.get_td_ip();
    char buffer[64];
    snprintf(buffer, sizeof(buffer),
             "PAINT:btn=%d,color=%d,level=%d", btn_state, color, level_val);
    udp_td.beginPacket(td_ip, TD_PORT);
    udp_td.write((const uint8_t*)buffer, strlen(buffer));
    udp_td.endPacket();
    Serial.printf("[TD] Sent: %s to %s:%d\n", buffer, td_ip, TD_PORT);
}

// ── NeoPixel update ───────────────────────────────────────────────
void updateLED() {
    uint32_t color = colors[colorIndex];
    strip.setPixelColor(0, color);          // LED 0 always on
    for (int i = 1; i < NUM_LED; i++)
        strip.setPixelColor(i, i <= level ? color : 0);
    strip.show();
}

// ─────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== ESP32 Paint Control + MQTT ===");

    pinMode(PIN_A, INPUT_PULLUP);
    pinMode(PIN_B, INPUT_PULLUP);
    pinMode(PIN_SW, INPUT_PULLUP);

    strip.begin();
    strip.show();
    lastA = digitalRead(PIN_A);

    configModule.connect_wifi(WIFI_SSID, WIFI_PASSWORD);

    if (!udp_config.begin(CFG_PORT))
        Serial.printf("[ERROR] UDP config failed on port %d\n", CFG_PORT);
    else
        Serial.printf("[SETUP] UDP config on port %d\n", CFG_PORT);

    configModule.init(&udp_config, CFG_PORT, DEFAULT_TD_IP);

    mqtt_client.setServer(MQTT_HOST, MQTT_PORT);
    mqtt_client.setCallback(mqtt_callback);
    mqtt_client.setKeepAlive(60);

    if (WiFi.status() == WL_CONNECTED)
        mqtt_connect();

    Serial.println("[SETUP] Done!");
}

// ─────────────────────────────────────────────────────────────────
void loop() {
    unsigned long now = millis();

    // ── Config UDP ──────────────────────────────────────────────
    if (now - last_config_check > config_check_interval) {
        configModule.check_config();
        configModule.update();
        last_config_check = now;
    }

    // ── MQTT loop / reconnect ───────────────────────────────────
    if (WiFi.status() == WL_CONNECTED) {
        if (mqtt_client.connected()) {
            mqtt_client.loop();
        } else if (now - last_mqtt_reconnect > mqtt_reconnect_interval) {
            last_mqtt_reconnect = now;
            Serial.println("[MQTT] Reconnecting...");
            mqtt_connect();
        }
    }

    // ── Rotary encoder (state machine) ──────────────────────────
    int currentA = digitalRead(PIN_A);
    int currentB = digitalRead(PIN_B);

    if (currentA != lastA || currentB != lastB) {
        if (currentA != lastA)
            colorIndex += (currentB == currentA) ? 1 : -1;
        else
            colorIndex += (currentB == currentA) ? -1 : 1;

        if (colorIndex < 0)  colorIndex = 19;
        if (colorIndex > 19) colorIndex = 0;
        updateLED();
        Serial.printf("[ENC] Color: %d (A=%d B=%d)\n", colorIndex, currentA, currentB);
    }
    lastA = currentA;
    lastB = currentB;

    // ── Button → level ──────────────────────────────────────────
    bool pressed = (digitalRead(PIN_SW) == LOW);
    int  current_button_state = pressed ? 1 : 0;

    if (now - lastUpdate > stepDelay) {
        lastUpdate = now;
        if (pressed) { if (level < (NUM_LED - 1)) level++; }
        else         { if (level > 0)              level--; }
        updateLED();
    }

    // ── Send to TD + MQTT on change (checked every 500 ms) ─────
    if (now - last_send_to_td > send_to_td_interval) {
        last_send_to_td = now;
        bool changed = (current_button_state != last_button_state) ||
                       (colorIndex != last_color_index) ||
                       (level != last_level);
        if (changed) {
            sendToTD(current_button_state, colorIndex, level);
            publish_paint_status(current_button_state, colorIndex, level);
            last_button_state = current_button_state;
            last_color_index  = colorIndex;
            last_level        = level;
            last_mqtt_publish = now;    // reset heartbeat timer
        }
    }

    // ── MQTT heartbeat (every 10 s even without changes) ────────
    if (now - last_mqtt_publish > mqtt_heartbeat_interval) {
        last_mqtt_publish = now;
        publish_paint_status(current_button_state, colorIndex, level);
    }
}
