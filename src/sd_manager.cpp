#include "sd_manager.h"

SDManager::SDManager() : m_initialized(false), m_chipSelect(5) {
}

// ============================================================
//  Initialization
// ============================================================
bool SDManager::begin(int chipSelect, uint32_t speed) {
    m_chipSelect = chipSelect;
    
    Serial.print("[SDManager] Initializing SD card (CS=");
    Serial.print(chipSelect);
    Serial.print(", Speed=");
    Serial.print(speed);
    Serial.println("Hz)...");
    
    // Initialize SPI
    SPI.begin(18, 19, 23, chipSelect);
    
    // Begin SD with low speed for stability
    if (!SD.begin(chipSelect, SPI, speed)) {
        Serial.println("[SDManager] ❌ FAILED to initialize SD card");
        return false;
    }
    
    m_initialized = true;
    Serial.println("[SDManager] ✓ SD card initialized successfully");
    
    // Print card info
    printCardInfo();
    
    return true;
}

// ============================================================
//  Card Info
// ============================================================
uint8_t SDManager::getCardType() const {
    if (!m_initialized) return CARD_NONE;
    return SD.cardType();
}

uint64_t SDManager::getCardSize() const {
    if (!m_initialized) return 0;
    return SD.cardSize();
}

uint64_t SDManager::getTotalSpace() const {
    if (!m_initialized) return 0;
    return SD.totalBytes();
}

uint64_t SDManager::getUsedSpace() const {
    if (!m_initialized) return 0;
    return SD.usedBytes();
}

void SDManager::printCardInfo() {
    if (!m_initialized) {
        Serial.println("[SDManager] Card not initialized");
        return;
    }
    
    uint8_t cardType = getCardType();
    Serial.print("[SDManager] Card type: ");
    
    if (cardType == CARD_MMC) {
        Serial.println("MMC");
    } else if (cardType == CARD_SD) {
        Serial.println("SDSC");
    } else if (cardType == CARD_SDHC) {
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }
    
    uint64_t cardSize = getCardSize();
    Serial.printf("[SDManager] Card size: %llu MB\n", cardSize / (1024 * 1024));
    
    uint64_t totalBytes = getTotalSpace();
    uint64_t usedBytes = getUsedSpace();
    Serial.printf("[SDManager] Total space: %llu MB\n", totalBytes / (1024 * 1024));
    Serial.printf("[SDManager] Used space: %llu MB\n", usedBytes / (1024 * 1024));
}

// ============================================================
//  File Operations
// ============================================================
bool SDManager::fileExists(const char* path) {
    if (!m_initialized) return false;
    return SD.exists(path);
}

bool SDManager::writeFile(const char* path, const char* data, bool append) {
    if (!m_initialized) {
        Serial.println("[SDManager] ❌ Card not initialized");
        return false;
    }
    
    const char* mode = append ? FILE_APPEND : FILE_WRITE;
    File file = SD.open(path, mode);
    
    if (!file) {
        Serial.printf("[SDManager] ❌ Failed to open file: %s\n", path);
        return false;
    }
    
    size_t written = file.print(data);
    file.flush();
    file.close();
    
    Serial.printf("[SDManager] ✓ Wrote %u bytes to %s\n", written, path);
    return written > 0;
}

String SDManager::readFile(const char* path) {
    String result = "";
    
    if (!m_initialized) {
        Serial.println("[SDManager] ❌ Card not initialized");
        return result;
    }
    
    File file = SD.open(path, FILE_READ);
    
    if (!file) {
        Serial.printf("[SDManager] ❌ Failed to open file: %s\n", path);
        return result;
    }
    
    while (file.available()) {
        result += (char)file.read();
    }
    
    file.close();
    Serial.printf("[SDManager] ✓ Read %d bytes from %s\n", result.length(), path);
    return result;
}

bool SDManager::deleteFile(const char* path) {
    if (!m_initialized) {
        Serial.println("[SDManager] ❌ Card not initialized");
        return false;
    }
    
    if (!SD.remove(path)) {
        Serial.printf("[SDManager] ❌ Failed to delete: %s\n", path);
        return false;
    }
    
    Serial.printf("[SDManager] ✓ Deleted: %s\n", path);
    return true;
}

bool SDManager::createDir(const char* path) {
    if (!m_initialized) {
        Serial.println("[SDManager] ❌ Card not initialized");
        return false;
    }
    
    if (!SD.mkdir(path)) {
        Serial.printf("[SDManager] ❌ Failed to create directory: %s\n", path);
        return false;
    }
    
    Serial.printf("[SDManager] ✓ Created directory: %s\n", path);
    return true;
}

bool SDManager::renameFile(const char* oldPath, const char* newPath) {
    if (!m_initialized) {
        Serial.println("[SDManager] ❌ Card not initialized");
        return false;
    }
    
    if (!SD.rename(oldPath, newPath)) {
        Serial.printf("[SDManager] ❌ Failed to rename %s to %s\n", oldPath, newPath);
        return false;
    }
    
    Serial.printf("[SDManager] ✓ Renamed: %s -> %s\n", oldPath, newPath);
    return true;
}

// ============================================================
//  Directory Listing
// ============================================================
bool SDManager::openDir(const char* path, File& dir) {
    if (!m_initialized) {
        Serial.println("[SDManager] ❌ Card not initialized");
        return false;
    }
    
    dir = SD.open(path);
    if (!dir || !dir.isDirectory()) {
        Serial.printf("[SDManager] ❌ Failed to open directory: %s\n", path);
        return false;
    }
    
    return true;
}

bool SDManager::listFiles(const char* dirPath, std::vector<FileInfo>& fileList) {
    fileList.clear();
    
    File dir;
    if (!openDir(dirPath, dir)) {
        return false;
    }
    
    File file = dir.openNextFile();
    while (file) {
        FileInfo info;
        info.name = file.name();
        info.size = file.size();
        info.isDir = file.isDirectory();
        fileList.push_back(info);
        
        file = dir.openNextFile();
    }
    
    dir.close();
    Serial.printf("[SDManager] ✓ Listed %u files from %s\n", fileList.size(), dirPath);
    return true;
}

bool SDManager::listFilesJSON(const char* dirPath, String& jsonOutput) {
    std::vector<FileInfo> fileList;
    
    if (!listFiles(dirPath, fileList)) {
        return false;
    }
    
    jsonOutput = "{\"path\":\"";
    jsonOutput += dirPath;
    jsonOutput += "\",\"files\":[";
    
    for (size_t i = 0; i < fileList.size(); i++) {
        if (i > 0) jsonOutput += ",";
        
        jsonOutput += "{\"name\":\"";
        jsonOutput += fileList[i].name;
        jsonOutput += "\",\"size\":";
        jsonOutput += fileList[i].size;
        jsonOutput += ",\"isDir\":";
        jsonOutput += fileList[i].isDir ? "true" : "false";
        jsonOutput += "}";
    }
    
    jsonOutput += "]}";
    return true;
}

void SDManager::printFileList(const char* dirPath) {
    std::vector<FileInfo> fileList;
    
    if (!listFiles(dirPath, fileList)) {
        return;
    }
    
    Serial.printf("\n[SDManager] Files in %s:\n", dirPath);
    Serial.println("================================================");
    
    for (const auto& info : fileList) {
        Serial.print("  ");
        if (info.isDir) {
            Serial.print("[DIR]  ");
        } else {
            Serial.print("[FILE] ");
        }
        Serial.print(info.name);
        Serial.print(" (");
        Serial.print(info.size);
        Serial.println(" bytes)");
    }
    
    Serial.println("================================================\n");
}
