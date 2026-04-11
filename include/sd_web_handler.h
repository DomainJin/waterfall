#ifndef SD_WEB_HANDLER_H
#define SD_WEB_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "sd_manager.h"

// ============================================================
//  SD Card Web Handler
//  
//  Định nghĩa các endpoint web để đọc/ghi file qua HTTP/WebSocket
//  Cấu trúc: 
//    - GET /files?dir=<path>      → Liệt kê file
//    - GET /file?path=<filepath>  → Đọc file nội dung
//    - POST /file                 → Ghi/cập nhật file
//    - DELETE /file?path=         → Xóa file
// ============================================================

class SDWebHandler {
public:
    SDWebHandler(SDManager& sdManager) : m_sd(sdManager) {}
    
    // ===== Handlers for different requests =====
    
    /// GET /files?dir=<path>
    /// Response: JSON array of files in directory
    String handleListFiles(const char* dirPath = "/") {
        String json;
        if (m_sd.listFilesJSON(dirPath, json)) {
            return json;
        }
        return "{\"error\":\"failed_to_list\"}";
    }
    
    /// GET /file?path=<filepath>
    /// Response: JSON with file content (base64 encoded for binary files)
    String handleReadFile(const char* filePath) {
        if (!m_sd.fileExists(filePath)) {
            return "{\"error\":\"file_not_found\"}";
        }
        
        String content = m_sd.readFile(filePath);
        
        // Build JSON response
        StaticJsonDocument<256> doc;
        doc["path"] = filePath;
        doc["size"] = content.length();
        doc["success"] = true;
        
        String response;
        serializeJson(doc, response);
        return response;
    }
    
    /// POST /file (JSON body: {path, data, append})
    /// Response: {success, size, message}
    String handleWriteFile(const char* filePath, const char* data, bool append = false) {
        StaticJsonDocument<256> doc;
        doc["path"] = filePath;
        
        if (m_sd.writeFile(filePath, data, append)) {
            doc["success"] = true;
            doc["size"] = strlen(data);
            doc["message"] = "file_written";
        } else {
            doc["success"] = false;
            doc["message"] = "write_failed";
        }
        
        String response;
        serializeJson(doc, response);
        return response;
    }
    
    /// DELETE /file?path=<filepath>
    /// Response: {success, message}
    String handleDeleteFile(const char* filePath) {
        StaticJsonDocument<128> doc;
        doc["path"] = filePath;
        
        if (m_sd.deleteFile(filePath)) {
            doc["success"] = true;
            doc["message"] = "file_deleted";
        } else {
            doc["success"] = false;
            doc["message"] = "delete_failed";
        }
        
        String response;
        serializeJson(doc, response);
        return response;
    }
    
    /// GET /sd/info
    /// Response: {card_type, size_mb, total_mb, used_mb}
    String handleCardInfo() {
        StaticJsonDocument<256> doc;
        
        if (!m_sd.isInitialized()) {
            doc["error"] = "not_initialized";
            String response;
            serializeJson(doc, response);
            return response;
        }
        
        uint64_t size = m_sd.getCardSize();
        uint64_t total = m_sd.getTotalSpace();
        uint64_t used = m_sd.getUsedSpace();
        
        uint8_t cardType = m_sd.getCardType();
        const char* typeStr = "UNKNOWN";
        if (cardType == CARD_MMC) typeStr = "MMC";
        else if (cardType == CARD_SD) typeStr = "SDSC";
        else if (cardType == CARD_SDHC) typeStr = "SDHC";
        
        doc["card_type"] = typeStr;
        doc["size_mb"] = size / (1024 * 1024);
        doc["total_mb"] = total / (1024 * 1024);
        doc["used_mb"] = used / (1024 * 1024);
        doc["free_mb"] = (total - used) / (1024 * 1024);
        
        String response;
        serializeJson(doc, response);
        return response;
    }
    
private:
    SDManager& m_sd;
};

#endif // SD_WEB_HANDLER_H
