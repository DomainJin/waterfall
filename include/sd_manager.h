#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <vector>
#include <string>

// ============================================================
//  SD Card Manager
// ============================================================
class SDManager {
public:
    SDManager();
    
    // Initialization
    bool begin(int chipSelect = 5, uint32_t speed = 400000);
    bool isInitialized() const { return m_initialized; }
    
    // Card info
    uint8_t getCardType() const;
    uint64_t getCardSize() const;  // in bytes
    uint64_t getTotalSpace() const;
    uint64_t getUsedSpace() const;
    
    // File operations
    bool fileExists(const char* path);
    bool writeFile(const char* path, const char* data, bool append = false);
    String readFile(const char* path);
    bool deleteFile(const char* path);
    bool createDir(const char* path);
    bool renameFile(const char* oldPath, const char* newPath);
    
    // Directory listing
    struct FileInfo {
        String name;
        uint32_t size;
        bool isDir;
    };
    
    bool listFiles(const char* dirPath, std::vector<FileInfo>& fileList);
    bool listFilesJSON(const char* dirPath, String& jsonOutput);
    
    // Utility
    void printCardInfo();
    void printFileList(const char* dirPath = "/");
    
private:
    bool m_initialized;
    int m_chipSelect;
    
    bool openDir(const char* path, File& dir);
};

#endif // SD_MANAGER_H
