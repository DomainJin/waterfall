#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "frame_queue.h"
#include "valve_driver.h"
#include "tcp_server.h"
#include "scheduler.h"
#include "sd_manager.h"
#include "config_server.h"

// ============================================================
//  Global objects
// ============================================================
FrameQueue    g_queue;
ValveDriver   g_valve;
TcpServer     g_tcp(g_queue, g_valve);
Scheduler     g_scheduler(g_queue, g_valve, g_tcp);
SDManager     g_sd;
ConfigServer  g_cfg;

// ============================================================
//  WiFi setup
// ============================================================
void setupWiFi() {
    Serial.printf("\n[WiFi] Connecting to %s...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_TIMEOUT_MS) {
            Serial.println("[WiFi] TIMEOUT — WiFi unavailable, continuing anyway");
            break;
        }
        delay(100);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    }
}

// ============================================================
//  Setup
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(3000);  // ← Increased from 2000 to 3000
    Serial.println("\n\n╔═══════════════════════════════════════════════╗");
    Serial.println("║  Water Curtain Valve Control — ESP32         ║");
    Serial.println("║  WebSocket Server (Frame Receiver)           ║");
    Serial.println("╚═══════════════════════════════════════════════╝");
    Serial.flush();

    // Setup hardware
    Serial.printf("\n[SETUP] Initializing hardware...\n");
    Serial.flush();
    g_valve.begin();
    Serial.printf("[SETUP] ✓ Valve driver initialized\n");
    Serial.flush();
    
    // Setup WiFi
    setupWiFi();
    Serial.printf("[SETUP] ✓ WiFi setup done\n");
    Serial.flush();
    
    // Setup SD Card
    Serial.printf("[SETUP] Initializing SD card...\n");
    Serial.flush();
    g_sd.begin(5, 400000);  // CS=5, Speed=400kHz for stability
    Serial.printf("[SETUP] ✓ SD card initialized\n");
    Serial.flush();
    g_sd.printFileList("/");
    Serial.flush();
    
    // Setup WebSocket server
    Serial.printf("[SETUP] Starting WebSocket server on port %d...\n", TCP_PORT);
    Serial.flush();
    g_tcp.begin();
    Serial.printf("[SETUP] ✓ WebSocket server started\n");
    Serial.flush();
    
    // Setup UDP Config server
    Serial.printf("[SETUP] Starting UDP Config server...\n");
    Serial.flush();
    g_cfg.begin(&g_sd);  // Pass SDManager reference
    Serial.printf("[SETUP] ✓ UDP Config server started (port 8888)\n");
    Serial.flush();

    Serial.println("\n[SETUP] ✓ Ready!");
    Serial.println("  WebSocket:  ws://<ESP32_IP>:3333");
    Serial.println("  Config UDP: <ESP32_IP>:8888");
    Serial.flush();
}

// ============================================================
//  Main loop — PRIORITY: Valve control #1, WebSocket #2
// ============================================================

// Heartbeat timing
static uint32_t last_heartbeat = 0;
const  uint16_t HEARTBEAT_MS = 5000;  // Show every 5 seconds

void loop() {
    uint32_t loop_start = micros();
    
    // ─────────────────────────────────────────────────────
    // PRIORITY 1: Process any pending valve control frames
    // (should complete in milliseconds)
    // ─────────────────────────────────────────────────────
    if (g_tcp.streaming()) {
        g_scheduler.tick();  // Process frame queue, apply valve changes
    }
    
    // ─────────────────────────────────────────────────────
    // PRIORITY 2: Handle WebSocket communication (events)
    // (non-blocking event pump)
    // ─────────────────────────────────────────────────────
    g_tcp.tick();
    
    // ─────────────────────────────────────────────────────
    // PRIORITY 3: Handle UDP Config commands
    // (non-blocking config updates)
    // ─────────────────────────────────────────────────────
    g_cfg.tick();
    
    // ─────────────────────────────────────────────────────
    // Heartbeat: Show ESP is alive every 5 seconds
    // (Optional for debugging - disabled by default)
    // ─────────────────────────────────────────────────────
    // uint32_t now = millis();
    // if (now - last_heartbeat >= HEARTBEAT_MS) {
    //     last_heartbeat = now;
    //     uint32_t loop_time_us = micros() - loop_start;
    //     Serial.printf("[HEARTBEAT] t=%lu ms | Queue: %u | Loop: %lu µs\n",
    //                   now, g_queue.size(), loop_time_us);
    //     Serial.flush();
    // }
    
    // ─────────────────────────────────────────────────────
    // Minimal delay to let WiFi task run
    // (ESP32 has core 0 for WiFi, core 1 for our code)
    // ─────────────────────────────────────────────────────
    // delayMicroseconds(10);  // Or just: yield();
}

