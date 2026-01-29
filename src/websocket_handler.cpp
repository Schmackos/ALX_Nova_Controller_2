#include "websocket_handler.h"
#include "config.h"
#include "app_state.h"
#include "wifi_manager.h"
#include "smart_sensing.h"
#include "ota_updater.h"
#include "debug_serial.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "esp_freertos_hooks.h"

// ===== CPU Utilization Tracking =====
// Using FreeRTOS idle hooks to measure CPU usage
static volatile uint32_t idleCounter0 = 0;
static volatile uint32_t idleCounter1 = 0;
static uint32_t lastIdleCount0 = 0;
static uint32_t lastIdleCount1 = 0;
static unsigned long lastCpuMeasureTime = 0;
static float cpuUsageCore0 = 0.0;
static float cpuUsageCore1 = 0.0;
static bool cpuHooksInstalled = false;

// Idle hook callbacks - called when core is idle
static bool idleHookCore0() {
  idleCounter0++;
  return false; // Don't yield
}

static bool idleHookCore1() {
  idleCounter1++;
  return false;
}

// ===== WebSocket Event Handler =====

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      DebugOut.printf("Client [%u] disconnected\n", num);
      break;
      
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        DebugOut.printf("Client [%u] connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
        
        sendLEDState();
        sendBlinkingState();
        sendWiFiStatus();
        sendSmartSensingStateInternal();  // Force send for new client
        
        // If device just updated, notify the client
        if (justUpdated) {
          broadcastJustUpdated();
        }
      }
      break;
      
    case WStype_TEXT:
      {
        DebugOut.printf("Received from client [%u]: %s\n", num, payload);
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, (const char*)payload, length);
        
        if (error) {
          DebugOut.print("JSON parsing failed: ");
          DebugOut.println(error.c_str());
          return;
        }
        
        if (doc["type"] == "toggle") {
          blinkingEnabled = doc["enabled"];
          DebugOut.printf("Blinking %s by client [%u]\n", blinkingEnabled ? "enabled" : "disabled", num);
          
          sendBlinkingState();
          
          if (!blinkingEnabled) {
            ledState = false;
            digitalWrite(LED_PIN, LOW);
            sendLEDState();
            DebugOut.println("LED turned OFF");
          }
        } else if (doc["type"] == "toggleAP") {
          bool enabled = doc["enabled"].as<bool>();
          apEnabled = enabled;
          
          if (enabled) {
            if (!isAPMode) {
              WiFi.mode(WIFI_AP_STA);
              WiFi.softAP(apSSID.c_str(), apPassword);
              isAPMode = true;
              DebugOut.println("Access Point enabled via WebSocket");
              DebugOut.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
            }
          } else {
            if (isAPMode && WiFi.status() == WL_CONNECTED) {
              WiFi.softAPdisconnect(true);
              WiFi.mode(WIFI_STA);
              isAPMode = false;
              DebugOut.println("Access Point disabled via WebSocket");
            }
          }
          
          sendWiFiStatus();
        } else if (doc["type"] == "getHardwareStats") {
          // Client requesting hardware stats
          sendHardwareStats();
        }
      }
      break;
      
    default:
      break;
  }
}

// ===== State Broadcasting Functions =====

void sendLEDState() {
  JsonDocument doc;
  doc["type"] = "ledState";
  doc["state"] = ledState;
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}

void sendBlinkingState() {
  JsonDocument doc;
  doc["type"] = "blinkingEnabled";
  doc["enabled"] = blinkingEnabled;
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}

void sendFactoryResetProgress(unsigned long secondsHeld, bool resetTriggered) {
  JsonDocument doc;
  doc["type"] = "factoryResetProgress";
  doc["secondsHeld"] = secondsHeld;
  doc["secondsRequired"] = BTN_VERY_LONG_PRESS_MIN / 1000;
  doc["resetTriggered"] = resetTriggered;
  doc["progress"] = (secondsHeld * 100) / (BTN_VERY_LONG_PRESS_MIN / 1000);
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}

void sendRebootProgress(unsigned long secondsHeld, bool rebootTriggered) {
  JsonDocument doc;
  doc["type"] = "rebootProgress";
  doc["secondsHeld"] = secondsHeld;
  doc["secondsRequired"] = BTN_VERY_LONG_PRESS_MIN / 1000;
  doc["rebootTriggered"] = rebootTriggered;
  doc["progress"] = (secondsHeld * 100) / (BTN_VERY_LONG_PRESS_MIN / 1000);
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}

// ===== CPU Utilization Functions =====

void initCpuUsageMonitoring() {
  if (!cpuHooksInstalled) {
    // Register idle hooks for both cores
    esp_register_freertos_idle_hook_for_cpu(idleHookCore0, 0);
    esp_register_freertos_idle_hook_for_cpu(idleHookCore1, 1);
    cpuHooksInstalled = true;
    lastCpuMeasureTime = millis();
  }
}

void updateCpuUsage() {
  // Initialize hooks if not done yet
  if (!cpuHooksInstalled) {
    initCpuUsageMonitoring();
    return;
  }
  
  unsigned long currentTime = millis();
  unsigned long elapsed = currentTime - lastCpuMeasureTime;
  
  // Only update every 500ms minimum for stable readings
  if (elapsed < 500) {
    return;
  }
  
  // Calculate idle iterations since last measurement
  uint32_t idleDelta0 = idleCounter0 - lastIdleCount0;
  uint32_t idleDelta1 = idleCounter1 - lastIdleCount1;
  
  // Save current counts for next calculation
  lastIdleCount0 = idleCounter0;
  lastIdleCount1 = idleCounter1;
  lastCpuMeasureTime = currentTime;
  
  // The maximum idle count represents 100% idle (0% CPU usage)
  // We need to calibrate this - typically idle hook runs many times per ms when idle
  // A good estimate is about 10000-50000 iterations per second when fully idle
  // We'll use a dynamic max based on the highest count seen
  static uint32_t maxIdlePerSecond0 = 100000;
  static uint32_t maxIdlePerSecond1 = 100000;
  
  // Scale to per-second rate
  float idlePerSecond0 = (float)idleDelta0 * 1000.0f / (float)elapsed;
  float idlePerSecond1 = (float)idleDelta1 * 1000.0f / (float)elapsed;
  
  // Update max if we see higher (indicating lower CPU usage baseline)
  if (idlePerSecond0 > maxIdlePerSecond0) maxIdlePerSecond0 = idlePerSecond0;
  if (idlePerSecond1 > maxIdlePerSecond1) maxIdlePerSecond1 = idlePerSecond1;
  
  // Calculate CPU usage: 100% - (idle% of max)
  cpuUsageCore0 = 100.0f - (idlePerSecond0 / maxIdlePerSecond0 * 100.0f);
  cpuUsageCore1 = 100.0f - (idlePerSecond1 / maxIdlePerSecond1 * 100.0f);
  
  // Clamp values
  if (cpuUsageCore0 < 0) cpuUsageCore0 = 0;
  if (cpuUsageCore0 > 100) cpuUsageCore0 = 100;
  if (cpuUsageCore1 < 0) cpuUsageCore1 = 0;
  if (cpuUsageCore1 > 100) cpuUsageCore1 = 100;
}

float getCpuUsageCore0() {
  return cpuUsageCore0;
}

float getCpuUsageCore1() {
  return cpuUsageCore1;
}

void sendHardwareStats() {
  // Update CPU usage before sending stats
  updateCpuUsage();
  
  JsonDocument doc;
  doc["type"] = "hardware_stats";
  
  // Memory - Internal Heap
  doc["memory"]["heapTotal"] = ESP.getHeapSize();
  doc["memory"]["heapFree"] = ESP.getFreeHeap();
  doc["memory"]["heapMinFree"] = ESP.getMinFreeHeap();
  doc["memory"]["heapMaxBlock"] = ESP.getMaxAllocHeap();
  
  // Memory - PSRAM (external, may not be available)
  doc["memory"]["psramTotal"] = ESP.getPsramSize();
  doc["memory"]["psramFree"] = ESP.getFreePsram();
  
  // CPU Information
  doc["cpu"]["freqMHz"] = ESP.getCpuFreqMHz();
  doc["cpu"]["model"] = ESP.getChipModel();
  doc["cpu"]["revision"] = ESP.getChipRevision();
  doc["cpu"]["cores"] = ESP.getChipCores();
  
  // CPU Utilization per core
  doc["cpu"]["usageCore0"] = cpuUsageCore0;
  doc["cpu"]["usageCore1"] = cpuUsageCore1;
  doc["cpu"]["usageTotal"] = (cpuUsageCore0 + cpuUsageCore1) / 2.0;
  
  // Temperature - ESP32-S3 internal sensor
  // temperatureRead() returns temperature in Celsius
  doc["cpu"]["temperature"] = temperatureRead();
  
  // Storage - Flash
  doc["storage"]["flashSize"] = ESP.getFlashChipSize();
  doc["storage"]["sketchSize"] = ESP.getSketchSize();
  doc["storage"]["sketchFree"] = ESP.getFreeSketchSpace();
  
  // Storage - SPIFFS
  doc["storage"]["spiffsTotal"] = SPIFFS.totalBytes();
  doc["storage"]["spiffsUsed"] = SPIFFS.usedBytes();
  
  // WiFi Information
  doc["wifi"]["rssi"] = WiFi.RSSI();
  doc["wifi"]["channel"] = WiFi.channel();
  doc["wifi"]["apClients"] = WiFi.softAPgetStationNum();
  doc["wifi"]["connected"] = (WiFi.status() == WL_CONNECTED);
  
  // Uptime (milliseconds since boot)
  doc["uptime"] = millis();
  
  // Broadcast to all WebSocket clients
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}
