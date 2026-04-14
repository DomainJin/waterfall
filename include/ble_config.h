#pragma once

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEAdvertising.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Arduino.h>
#include <WiFi.h>

#define BLE_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_CHAR_WIFI_UUID     "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define BLE_CHAR_STATUS_UUID   "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define BLE_CHAR_INFO_UUID     "beb5483e-36e1-4688-b7f5-ea07361b26aa"
#define BLE_DEVICE_NAME        "Waterfall_Config"

class BLEConfig {
public:
    BLEConfig();
    ~BLEConfig();

    void begin();
    void tick();
    void stop();

    bool setWiFiCredentials(const String& ssid, const String& password);
    String getStatus();
    String getDeviceInfo();

    void startAdvertising();
    void stopAdvertising();
    bool isAdvertising() { return _isAdvertising; }

    void onWiFiWrite(const String& value);
    void setConnected(bool connected);

private:
    BLEServer*        _server;
    BLEService*       _service;
    BLECharacteristic* _charWiFi;
    BLECharacteristic* _charStatus;
    BLECharacteristic* _charInfo;
    BLEAdvertising*   _adv;

    bool              _initialized;
    bool              _isAdvertising;
    bool              _deviceConnected;

    String            _wifiSSID;
    String            _wifiPassword;
    String            _connectionStatus;

    void updateStatusCharacteristic();
    void updateInfoCharacteristic();
    bool saveWiFiConfig();
    bool loadWiFiConfig();
};

extern BLEConfig g_ble;