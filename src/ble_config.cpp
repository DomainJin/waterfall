#include "ble_config.h"
#include "config.h"
#include <Preferences.h>
#include <WiFi.h>

class WiFiCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) {
        String value = pChar->getValue().c_str();
        Serial.printf("[BLE] WiFi config received: %s\n", value.c_str());
        g_ble.onWiFiWrite(value);
    }
};

class StatusCallbacks: public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic* pChar) {
        pChar->setValue(g_ble.getStatus().c_str());
    }
};

class InfoCallbacks: public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic* pChar) {
        pChar->setValue(g_ble.getDeviceInfo().c_str());
    }
};

class ServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        Serial.println("[BLE] >>> CLIENT CONNECTED <<<");
        g_ble.setConnected(true);
    }

    void onDisconnect(BLEServer* pServer) {
        Serial.println("[BLE] <<< CLIENT DISCONNECTED >>>");
        g_ble.setConnected(false);
        g_ble.startAdvertising();
    }
};

BLEConfig::BLEConfig()
    : _server(nullptr)
    , _service(nullptr)
    , _charWiFi(nullptr)
    , _charStatus(nullptr)
    , _charInfo(nullptr)
    , _adv(nullptr)
    , _initialized(false)
    , _isAdvertising(false)
    , _deviceConnected(false)
    , _wifiSSID("")
    , _wifiPassword("")
    , _connectionStatus("Not connected")
{
}

BLEConfig::~BLEConfig() {
    stop();
}

void BLEConfig::begin() {
    if (_initialized) return;

    Serial.println("[BLE] Initializing BLE...");

    BLEDevice::init(BLE_DEVICE_NAME);

    _server = BLEDevice::createServer();
    _server->setCallbacks(new ServerCallbacks());

    _service = _server->createService(BLE_SERVICE_UUID);

    _charWiFi = _service->createCharacteristic(
        BLE_CHAR_WIFI_UUID,
        BLECharacteristic::PROPERTY_WRITE);
    _charWiFi->setCallbacks(new WiFiCallbacks());

    _charStatus = _service->createCharacteristic(
        BLE_CHAR_STATUS_UUID,
        BLECharacteristic::PROPERTY_READ);
    _charStatus->setCallbacks(new StatusCallbacks());

    _charInfo = _service->createCharacteristic(
        BLE_CHAR_INFO_UUID,
        BLECharacteristic::PROPERTY_READ);
    _charInfo->setCallbacks(new InfoCallbacks());

    _service->start();

    _adv = BLEDevice::getAdvertising();

    loadWiFiConfig();

    _initialized = true;
    startAdvertising();
    Serial.println("[BLE] BLE initialized, advertising started");
}

void BLEConfig::tick() {
    if (!_initialized) return;

    wl_status_t status = WiFi.status();
    String newStatus;
    
    switch (status) {
        case WL_CONNECTED:
            newStatus = "Connected: " + WiFi.localIP().toString();
            break;
        case WL_CONNECT_FAILED:
            newStatus = "Connection failed";
            break;
        case WL_NO_SSID_AVAIL:
            newStatus = "SSID not available";
            break;
        case WL_IDLE_STATUS:
            newStatus = "Idle (not connected)";
            break;
        case WL_DISCONNECTED:
            newStatus = "Disconnected";
            break;
        default:
            newStatus = "Unknown";
            break;
    }

    if (_connectionStatus != newStatus) {
        _connectionStatus = newStatus;
        updateStatusCharacteristic();
    }
}

void BLEConfig::stop() {
    if (!_initialized) return;

    stopAdvertising();
    
    if (_service) {
        _service->stop();
    }
    
    if (_isAdvertising) {
        BLEDevice::deinit(false);
    }
    
    _initialized = false;
    Serial.println("[BLE] BLE stopped");
}

bool BLEConfig::setWiFiCredentials(const String& ssid, const String& password) {
    _wifiSSID = ssid;
    _wifiPassword = password;
    
    if (!saveWiFiConfig()) {
        Serial.println("[BLE] Failed to save WiFi config");
        return false;
    }

    Serial.printf("[BLE] WiFi credentials saved. SSID: %s\n", ssid.c_str());
    
    WiFi.disconnect(true, true);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    return true;
}

String BLEConfig::getStatus() {
    return _connectionStatus;
}

String BLEConfig::getDeviceInfo() {
    String info = "Waterfall_Config|";
    if (WiFi.status() == WL_CONNECTED) {
        info += WiFi.localIP().toString() + "|";
    } else {
        info += "Not connected|";
    }
    info += String(NUM_BOARDS) + " boards|" + String(NUM_VALVES) + " valves";
    return info;
}

void BLEConfig::startAdvertising() {
    if (!_initialized) {
        Serial.println("[BLE] Not initialized, cannot start advertising");
        return;
    }

    if (_isAdvertising) {
        Serial.println("[BLE] Already advertising, stopping first...");
        _adv->stop();
    }

    BLEAdvertisementData advertisementData;
    advertisementData.setName(BLE_DEVICE_NAME);
    advertisementData.setCompleteServices(BLEUUID(BLE_SERVICE_UUID));
    advertisementData.setAppearance(0x00);
    
    _adv->setAdvertisementData(advertisementData);
    
    _adv->start();
    _isAdvertising = true;
    Serial.println("[BLE] Advertising started - Device: " BLE_DEVICE_NAME);
    Serial.println("[BLE] Check your phone for 'Waterfall_Config'");
}

void BLEConfig::stopAdvertising() {
    if (!_initialized || !_isAdvertising) return;

    _adv->stop();
    _isAdvertising = false;
    Serial.println("[BLE] Advertising stopped");
}

void BLEConfig::onWiFiWrite(const String& value) {
    int separator = value.indexOf('|');
    if (separator == -1) {
        Serial.println("[BLE] Invalid WiFi format, expected: SSID|PASSWORD");
        return;
    }

    String ssid = value.substring(0, separator);
    String password = value.substring(separator + 1);
    
    ssid.trim();
    password.trim();

    if (ssid.length() == 0) {
        Serial.println("[BLE] SSID is empty");
        return;
    }

    setWiFiCredentials(ssid, password);
}

void BLEConfig::updateStatusCharacteristic() {
    if (_charStatus) {
        _charStatus->setValue(_connectionStatus.c_str());
    }
}

void BLEConfig::updateInfoCharacteristic() {
    if (_charInfo) {
        _charInfo->setValue(getDeviceInfo().c_str());
    }
}

void BLEConfig::setConnected(bool connected) {
    _deviceConnected = connected;
}

bool BLEConfig::saveWiFiConfig() {
    Preferences prefs;
    if (!prefs.begin("ble_wifi", false)) {
        Serial.println("[BLE] Failed to open preferences");
        return false;
    }

    prefs.putString("ssid", _wifiSSID);
    prefs.putString("pass", _wifiPassword);
    prefs.end();
    
    return true;
}

bool BLEConfig::loadWiFiConfig() {
    Preferences prefs;
    if (!prefs.begin("ble_wifi", true)) {
        Serial.println("[BLE] Failed to open preferences for reading");
        return false;
    }

    _wifiSSID = prefs.getString("ssid", "");
    _wifiPassword = prefs.getString("pass", "");
    prefs.end();

    if (_wifiSSID.length() > 0) {
        Serial.printf("[BLE] Loaded WiFi config. SSID: %s\n", _wifiSSID.c_str());
        
        if (_wifiPassword.length() > 0) {
            WiFi.mode(WIFI_STA);
            WiFi.begin(_wifiSSID.c_str(), _wifiPassword.c_str());
        }
        return true;
    }

    return false;
}