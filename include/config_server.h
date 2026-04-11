#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUDP.h>
#include "config.h"
#include "sd_manager.h"

/**
 * UDP Config Server — set TD_IP, port, mode động mà không cần upload lại
 * 
 * Commands (UDP port 8888):
 *   GET_INFO           → "OK:ESP32:<IP> WS:<port> V:<version>"
 *   SET_IP:<new_ip>    → "OK:<new_ip>"
 *   SET_PORT:<port>    → "OK:<port>"
 *   RESET              → "OK:RESET"
 *   SCAN               → "OK:SCAN_MODE"
 *   GET_STORAGE        → "OK: used=<bytes>, total=<bytes>"
 *   LIST_FILES         → "OK: file1.bin,<size>;file2.bin,<size>;..."
 */

class ConfigServer {
private:
    WiFiUDP udp;
    const uint16_t CFG_PORT = 8888;
    const uint16_t TIMEOUT = 500;  // ms
    
    // Stored config (could be EEPROM in production)
    char stored_td_ip[16] = "0.0.0.0";  // Remote target IP
    uint16_t stored_port = 3333;
    
    // Reference to SDManager (set via begin())
    SDManager* m_sd = nullptr;
    
public:
    ConfigServer() {}
    
    void begin(SDManager* sd = nullptr) {
        m_sd = sd;
        if (!udp.begin(CFG_PORT)) {
            Serial.printf("[CFG] ❌ Failed to start UDP server on port %d\n", CFG_PORT);
            return;
        }
        Serial.printf("[CFG] ✓ UDP Config server on port %d\n", CFG_PORT);
        
        // Load any stored config (in real project, from EEPROM)
        strcpy(stored_td_ip, "192.168.1.100");
    }
    
    // Call this from loop() to process config commands
    void tick() {
        int pkt = udp.parsePacket();
        if (pkt <= 0) return;

        char buf[256] = {0};
        int n = udp.read((uint8_t*)buf, sizeof(buf)-1);
        buf[n] = 0;

        // Parse command
        if (strncmp(buf, "GET_INFO", 8) == 0) {
            handle_get_info();
        }
        else if (strncmp(buf, "SET_IP:", 7) == 0) {
            char* new_ip = buf + 7;
            handle_set_ip(new_ip);
        }
        else if (strncmp(buf, "SET_PORT:", 9) == 0) {
            uint16_t new_port = atoi(buf + 9);
            handle_set_port(new_port);
        }
        else if (strncmp(buf, "RESET", 5) == 0) {
            handle_reset();
        }
        else if (strncmp(buf, "SCAN", 4) == 0) {
            handle_scan();
        }
        else if (strncmp(buf, "GET_STORAGE", 11) == 0) {
            handle_get_storage();
        }
        else if (strncmp(buf, "LIST_FILES", 10) == 0) {
            handle_list_files();
        }
        else {
            send_reply("ERROR:Unknown command");
        }
    }
    
    // Get stored config
    const char* get_td_ip() const { return stored_td_ip; }
    uint16_t get_port() const { return stored_port; }

private:
    void send_reply(const char* msg) {
        udp.beginPacket(udp.remoteIP(), udp.remotePort());
        udp.print(msg);
        udp.endPacket();
    }

    void handle_get_info() {
        char buf[256];
        snprintf(buf, sizeof(buf), "OK:ESP32:%s WS:%d TD:%s Ver:1.0",
                 WiFi.localIP().toString().c_str(), TCP_PORT, stored_td_ip);
        send_reply(buf);
        Serial.printf("[CFG] INFO requested → %s\n", buf);
    }

    void handle_set_ip(const char* new_ip) {
        // Strip newlines
        char clean_ip[16] = {0};
        for (int i = 0; i < 15 && new_ip[i]; i++) {
            if (new_ip[i] == '\n' || new_ip[i] == '\r') break;
            clean_ip[i] = new_ip[i];
        }
        
        // Validate IP (simple check)
        if (!is_valid_ip(clean_ip)) {
            send_reply("ERROR:Invalid IP");
            Serial.printf("[CFG] ❌ Invalid IP: %s\n", clean_ip);
            return;
        }
        
        strncpy(stored_td_ip, clean_ip, 15);
        Serial.printf("[CFG] ✓ TD_IP changed to: %s\n", stored_td_ip);
        
        char reply[64];
        snprintf(reply, sizeof(reply), "OK:%s", stored_td_ip);
        send_reply(reply);
    }

    void handle_set_port(uint16_t new_port) {
        if (new_port < 1000 || new_port > 65535) {
            send_reply("ERROR:Invalid port (1000-65535)");
            return;
        }
        stored_port = new_port;
        Serial.printf("[CFG] ✓ Port changed to: %d\n", stored_port);
        
        char reply[64];
        snprintf(reply, sizeof(reply), "OK:%d", stored_port);
        send_reply(reply);
    }

    void handle_reset() {
        Serial.println("[CFG] RESET command received");
        send_reply("OK:RESET");
        delay(500);
        ESP.restart();
    }

    void handle_scan() {
        // Send heartbeat for scanning
        char buf[128];
        snprintf(buf, sizeof(buf), "SCAN:ESP32_at_%s_alive", WiFi.localIP().toString().c_str());
        send_reply(buf);
        Serial.printf("[CFG] SCAN heartbeat sent\n");
    }

    void handle_get_storage() {
        if (!m_sd || !m_sd->isInitialized()) {
            send_reply("ERROR:SD not initialized");
            Serial.printf("[CFG] STORAGE: SD not available\n");
            return;
        }

        uint64_t used = m_sd->getUsedSpace();
        uint64_t total = m_sd->getTotalSpace();
        
        char buf[128];
        snprintf(buf, sizeof(buf), "OK: used=%llu, total=%llu", used, total);
        send_reply(buf);
        Serial.printf("[CFG] STORAGE: %llu / %llu bytes\n", used, total);
    }

    void handle_list_files() {
        if (!m_sd || !m_sd->isInitialized()) {
            send_reply("ERROR:SD not initialized");
            Serial.printf("[CFG] FILES: SD not available\n");
            return;
        }

        std::vector<SDManager::FileInfo> files;
        if (!m_sd->listFiles("/", files)) {
            send_reply("ERROR:Failed to list files");
            Serial.printf("[CFG] FILES: Failed to list\n");
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
        
        // UDP packet size limit is ~1500 bytes, truncate if needed
        if (response.length() > 1400) {
            response = response.substring(0, 1400);
            response += "...";
        }
        
        send_reply(response.c_str());
        Serial.printf("[CFG] FILES: %d files found\n", (int)files.size());
    }

    bool is_valid_ip(const char* ip) {
        int a, b, c, d;
        return sscanf(ip, "%d.%d.%d.%d", &a, &b, &c, &d) == 4
            && a >= 0 && a <= 255
            && b >= 0 && b <= 255
            && c >= 0 && c <= 255
            && d >= 0 && d <= 255;
    }
};
    