#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include "valve_driver.h"
#include "config.h"

// ============================================================
//  OTAServer — nhận firmware update qua HTTP POST (port 8080)
//
//  UI được tích hợp sẵn trong control.html (không serve HTML ở đây).
//  CORS header cho phép control.html gọi từ bất kỳ origin nào.
//
//  Endpoint: POST http://<ESP-IP>:8080/update
//    Form fields: password=<string>, firmware=<binary .bin>
// ============================================================

class OTAServer {
public:
    explicit OTAServer(ValveDriver& v) : _v(v), _http(8080) {}

    void begin() {
        // Firmware version query — used by control.html to verify OTA success
        _http.on("/version", HTTP_GET, [this]() {
            _addCors();
            _http.send(200, "application/json",
                       "{\"fw\":\"" FW_VERSION "\"}");
        });
        _http.on("/version", HTTP_OPTIONS, [this]() {
            _addCors(); _http.send(204);
        });

        // CORS preflight
        _http.on("/update", HTTP_OPTIONS, [this]() {
            _addCors();
            _http.send(204);
        });

        _http.on("/update", HTTP_POST,
            // Done handler
            [this]() {
                _addCors();
                if (Update.hasError()) {
                    String err = Update.errorString();
                    _http.send(500, "text/plain", err);
                    Serial.printf("[OTA] Flash failed: %s\n", err.c_str());
                } else {
                    _http.send(200, "text/plain", "OK");
                    Serial.println("[OTA] Flash OK — restarting");
                    delay(600);
                    ESP.restart();
                }
                _uploading = false;
                _pwOk      = false;
            },
            // Chunk handler
            [this]() {
                HTTPUpload& up = _http.upload();

                // Validate password on first chunk
                if (!_pwOk) {
                    String pw = _http.arg("password");
                    if (pw != OTA_PASSWORD) {
                        Serial.println("[OTA] Wrong password");
                        _http.send(403, "text/plain", "Wrong password");
                        return;
                    }
                    _pwOk = true;
                }

                if (up.status == UPLOAD_FILE_START) {
                    Serial.printf("[OTA] Start: %s\n", up.filename.c_str());
                    _v.allOff();
                    _uploading = true;
                    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                        Update.printError(Serial);
                    }
                } else if (up.status == UPLOAD_FILE_WRITE) {
                    if (Update.write(up.buf, up.currentSize) != up.currentSize) {
                        Update.printError(Serial);
                    }
                } else if (up.status == UPLOAD_FILE_END) {
                    if (Update.end(true)) {
                        Serial.printf("[OTA] Done: %u bytes\n", up.totalSize);
                    } else {
                        Update.printError(Serial);
                    }
                }
            }
        );

        _http.begin();
        Serial.printf("[OTA] Update endpoint: http://%s:8080/update\n",
                      WiFi.localIP().toString().c_str());
    }

    void tick() { _http.handleClient(); }

    bool isUpdating() const { return _uploading; }

private:
    ValveDriver& _v;
    WebServer    _http;
    bool         _uploading = false;
    bool         _pwOk      = false;

    // Đổi password ở đây (phải khớp với ADMIN_PASSWORD trong control.html)
    static constexpr const char* OTA_PASSWORD = "waterfall";

    void _addCors() {
        _http.sendHeader("Access-Control-Allow-Origin",  "*");
        _http.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
        _http.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    }
};
