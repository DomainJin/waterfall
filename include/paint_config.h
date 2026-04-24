#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUDP.h>
#include <Preferences.h>

// Lightweight config/WiFi module for the Paint ESP32 device.
class ConfigModule {
public:
    ConfigModule() : _udp(nullptr), _port(0) {
        strncpy(_td_ip, "192.168.1.81", sizeof(_td_ip));
    }

    void connect_wifi(const char* ssid, const char* pass) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, pass);
        Serial.print("[WiFi] Connecting");
        for (int i = 0; WiFi.status() != WL_CONNECTED && i < 30; i++) {
            delay(500);
            Serial.print(".");
        }
        if (WiFi.status() == WL_CONNECTED)
            Serial.printf("\n[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
        else
            Serial.println("\n[WiFi] FAILED — continuing offline");
    }

    void init(WiFiUDP* udp, int port, const char* default_td_ip) {
        _udp  = udp;
        _port = port;
        Preferences prefs;
        if (prefs.begin("paint_cfg", true)) {
            String saved = prefs.getString("td_ip", default_td_ip);
            strncpy(_td_ip, saved.c_str(), sizeof(_td_ip) - 1);
            prefs.end();
        } else {
            strncpy(_td_ip, default_td_ip, sizeof(_td_ip) - 1);
        }
        Serial.printf("[CFG] TD IP: %s\n", _td_ip);
    }

    void check_config() {
        if (!_udp) return;
        int len = _udp->parsePacket();
        if (len <= 0) return;
        char buf[128] = {0};
        len = _udp->read(buf, sizeof(buf) - 1);
        buf[len] = '\0';
        _handle(buf);
    }

    void update() {}

    const char* get_td_ip() const { return _td_ip; }

private:
    WiFiUDP* _udp;
    int      _port;
    char     _td_ip[32];

    void _reply(const char* msg) {
        if (!_udp) return;
        IPAddress ip = _udp->remoteIP();
        uint16_t  pt = _udp->remotePort();
        _udp->beginPacket(ip, pt);
        _udp->write((const uint8_t*)msg, strlen(msg));
        _udp->endPacket();
    }

    void _handle(const char* cmd) {
        Serial.printf("[CFG] cmd: %s\n", cmd);
        if (strncmp(cmd, "SET_IP:", 7) == 0) {
            const char* new_ip = cmd + 7;
            strncpy(_td_ip, new_ip, sizeof(_td_ip) - 1);
            Preferences prefs;
            if (prefs.begin("paint_cfg", false)) {
                prefs.putString("td_ip", new_ip);
                prefs.end();
            }
            char reply[48];
            snprintf(reply, sizeof(reply), "OK:%s", new_ip);
            _reply(reply);
            Serial.printf("[CFG] TD IP → %s\n", new_ip);
        } else if (strcmp(cmd, "GET_INFO") == 0) {
            char reply[80];
            snprintf(reply, sizeof(reply), "OK:PAINT:%s td=%s",
                     WiFi.localIP().toString().c_str(), _td_ip);
            _reply(reply);
        }
    }
};
