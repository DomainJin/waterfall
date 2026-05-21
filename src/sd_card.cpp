#include "sd_card.h"

// ===== GLOBALS =====
static bool sd_initialized = false;

// ===== IMPLEMENTATION =====

bool sd_init() {
  Serial.println("[SD] Initializing SD card module...");
  
  // Initialize SPI (VSPI)
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS_PIN);
  
  // Initialize SD card with reduced speed for stability
  if (!SD.begin(SD_CS_PIN, SPI, SD_SPI_FREQ)) {
    Serial.println("[SD] ERROR: Failed to initialize SD card!");
    return false;
  }
  
  // Check card type
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("[SD] ERROR: No SD card detected!");
    return false;
  }
  
  // Print card information
  uint32_t cardSize = sd_getCardSize();
  Serial.print("[SD] Card Type: ");
  Serial.println(cardType == CARD_MMC ? "MMC" : cardType == CARD_SD ? "SD" : 
                 cardType == CARD_SDHC ? "SDHC" : "UNKNOWN");
  Serial.print("[SD] Card Size: ");
  Serial.print(cardSize);
  Serial.println(" MB");
  
  sd_initialized = true;
  Serial.println("[SD] Initialization OK");
  
  return true;
}

void sd_listFiles() {
  if (!sd_initialized) {
    Serial.println("[SD] ERROR: SD card not initialized!");
    return;
  }
  
  Serial.println("[SD] === FILE LIST ===");
  File root = SD.open("/");
  if (!root) {
    Serial.println("[SD] ERROR: Cannot open root directory!");
    return;
  }
  
  int fileCount = 0;
  File file = root.openNextFile();
  while (file) {
    Serial.print("[SD]   ");
    Serial.print(file.name());
    if (file.isDirectory()) {
      Serial.println("/");
    } else {
      Serial.print(" (");
      Serial.print(file.size());
      Serial.println(" bytes)");
    }
    file = root.openNextFile();
    fileCount++;
  }
  root.close();
  
  Serial.print("[SD] Total files: ");
  Serial.println(fileCount);
}

bool sd_writeFile(const char* filename, const char* data, bool append) {
  if (!sd_initialized) {
    Serial.println("[SD] ERROR: SD card not initialized!");
    return false;
  }
  
  const char* mode = append ? "a" : "w";
  
  Serial.print("[SD] Opening file '");
  Serial.print(filename);
  Serial.print("' for ");
  Serial.println(append ? "appending..." : "writing...");
  
  File myFile = SD.open(filename, FILE_APPEND);
  if (!myFile) {
    Serial.print("[SD] ERROR: Cannot open file '");
    Serial.print(filename);
    Serial.println("'");
    return false;
  }
  
  // Write data
  size_t bytesWritten = myFile.print(data);
  myFile.flush();  // Force write to SD card
  myFile.close();
  
  Serial.print("[SD] Written ");
  Serial.print(bytesWritten);
  Serial.print(" bytes to '");
  Serial.print(filename);
  Serial.println("'");
  
  return true;
}

String sd_readFile(const char* filename) {
  if (!sd_initialized) {
    Serial.println("[SD] ERROR: SD card not initialized!");
    return "";
  }
  
  Serial.print("[SD] Reading file '");
  Serial.print(filename);
  Serial.println("'...");
  
  File myFile = SD.open(filename);
  if (!myFile) {
    Serial.print("[SD] ERROR: Cannot open file '");
    Serial.print(filename);
    Serial.println("'");
    return "";
  }
  
  String content = "";
  while (myFile.available()) {
    content += (char)myFile.read();
  }
  myFile.close();
  
  Serial.print("[SD] Read ");
  Serial.print(content.length());
  Serial.print(" bytes from '");
  Serial.print(filename);
  Serial.println("'");
  
  return content;
}

bool sd_fileExists(const char* filename) {
  if (!sd_initialized) {
    return false;
  }
  
  return SD.exists(filename);
}

bool sd_deleteFile(const char* filename) {
  if (!sd_initialized) {
    Serial.println("[SD] ERROR: SD card not initialized!");
    return false;
  }
  
  if (!SD.remove(filename)) {
    Serial.print("[SD] ERROR: Cannot delete file '");
    Serial.print(filename);
    Serial.println("'");
    return false;
  }
  
  Serial.print("[SD] Deleted file '");
  Serial.print(filename);
  Serial.println("'");
  
  return true;
}

uint32_t sd_getCardSize() {
  if (!sd_initialized) {
    return 0;
  }
  
  return SD.cardSize() / (1024 * 1024);  // Convert to MB
}

uint32_t sd_getAvailableSpace() {
  if (!sd_initialized) {
    return 0;
  }
  
  uint32_t totalSize = SD.cardSize();
  uint32_t usedSize = SD.usedBytes();
  
  return totalSize - usedSize;
}
