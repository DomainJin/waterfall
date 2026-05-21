#ifndef SD_CARD_H
#define SD_CARD_H

#include <SPI.h>
#include <SD.h>

// ===== CONSTANTS =====
#define SD_CS_PIN       5           // Chip Select pin
#define SD_SPI_FREQ     400000      // 400kHz for stability
#define SD_MOSI        23
#define SD_MISO        19
#define SD_SCK         18

// ===== SD CARD FUNCTIONS =====

/**
 * @brief Initialize SD card module
 * @return true if initialization successful, false otherwise
 */
bool sd_init();

/**
 * @brief List all files in SD card root directory
 */
void sd_listFiles();

/**
 * @brief Write data to a file
 * @param filename File path (e.g., "/data.txt")
 * @param data Data to write
 * @param append If true, append to file; if false, truncate
 * @return true if write successful, false otherwise
 */
bool sd_writeFile(const char* filename, const char* data, bool append = true);

/**
 * @brief Read entire file content
 * @param filename File path to read
 * @return File content as string (empty if read fails)
 */
String sd_readFile(const char* filename);

/**
 * @brief Check if file exists
 * @param filename File path
 * @return true if file exists, false otherwise
 */
bool sd_fileExists(const char* filename);

/**
 * @brief Delete a file
 * @param filename File path to delete
 * @return true if delete successful, false otherwise
 */
bool sd_deleteFile(const char* filename);

/**
 * @brief Get SD card information
 * @return Card size in MB, 0 if error
 */
uint32_t sd_getCardSize();

/**
 * @brief Get available space on SD card
 * @return Available space in bytes
 */
uint32_t sd_getAvailableSpace();

#endif // SD_CARD_H
