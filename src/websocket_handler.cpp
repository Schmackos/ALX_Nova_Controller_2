#include "websocket_handler.h"
#include "auth_handler.h"
#include "config.h"
#include "app_state.h"
#include "settings_manager.h"
#include "wifi_manager.h"
#include "smart_sensing.h"
#include "ota_updater.h"
#include "debug_serial.h"
#include "utils.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "esp_freertos_hooks.h"

// ===== WebSocket Authentication Tracking =====
bool wsAuthStatus[MAX_WS_CLIENTS] = {false};
unsigned long wsAuthTimeout[MAX_WS_CLIENTS] = {0};

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
      LOG_I("[WebSocket] Client [%u] disconnected", num);
      wsAuthStatus[num] = false;
      wsAuthTimeout[num] = 0;
      break;

    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        LOG_I("[WebSocket] Client [%u] connected from %d.%d.%d.%d", num, ip[0], ip[1], ip[2], ip[3]);

        // Set auth timeout (5 seconds to authenticate)
        wsAuthStatus[num] = false;
        wsAuthTimeout[num] = millis() + 5000;

        // Request authentication
        webSocket.sendTXT(num, "{\"type\":\"authRequired\"}");
      }
      break;

    case WStype_TEXT:
      {
        LOG_D("[WebSocket] Received from client [%u]: %s", num, payload);

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, (const char*)payload, length);

        if (error) {
          LOG_E("[WebSocket] JSON parsing failed: %s", error.c_str());
          return;
        }

        String msgType = doc["type"].as<String>();

        // Handle authentication message
        if (msgType == "auth") {
          String sessionId = doc["sessionId"].as<String>();

          if (validateSession(sessionId)) {
            wsAuthStatus[num] = true;
            wsAuthTimeout[num] = 0;
            webSocket.sendTXT(num, "{\"type\":\"authSuccess\"}");
            LOG_D("[WebSocket] Client [%u] authenticated", num);

            // Send initial state after authentication
            sendLEDState();
            sendBlinkingState();
            sendWiFiStatus();
            sendSmartSensingStateInternal();
            sendDisplayState();
            sendBuzzerState();

            // If device just updated, notify the client
            if (justUpdated) {
              broadcastJustUpdated();
            }
          } else {
            webSocket.sendTXT(num, "{\"type\":\"authFailed\",\"error\":\"Invalid session\"}");
            webSocket.disconnect(num);
          }
          return;
        }

        // Check if client is authenticated for all other commands
        if (!wsAuthStatus[num]) {
          webSocket.sendTXT(num, "{\"type\":\"error\",\"message\":\"Not authenticated\"}");
          webSocket.disconnect(num);
          return;
        }

        if (msgType == "toggle") {
          blinkingEnabled = doc["enabled"];
          LOG_I("[WebSocket] Blinking %s by client [%u]", blinkingEnabled ? "enabled" : "disabled", num);
          
          sendBlinkingState();
          
          if (!blinkingEnabled) {
            ledState = false;
            digitalWrite(LED_PIN, LOW);
            sendLEDState();
            LOG_I("[WebSocket] LED turned OFF");
          }
        } else if (doc["type"] == "toggleAP") {
          bool enabled = doc["enabled"].as<bool>();
          apEnabled = enabled;
          
          if (enabled) {
            if (!isAPMode) {
              WiFi.mode(WIFI_AP_STA);
              WiFi.softAP(apSSID.c_str(), apPassword);
              isAPMode = true;
              LOG_I("[WebSocket] Access Point enabled");
              LOG_I("[WebSocket] AP IP: %s", WiFi.softAPIP().toString().c_str());
            }
          } else {
            if (isAPMode && WiFi.status() == WL_CONNECTED) {
              WiFi.softAPdisconnect(true);
              WiFi.mode(WIFI_STA);
              isAPMode = false;
              LOG_I("[WebSocket] Access Point disabled");
            }
          }
          
          sendWiFiStatus();
        } else if (doc["type"] == "getHardwareStats") {
          // Client requesting hardware stats
          sendHardwareStats();
        } else if (msgType == "setBacklight") {
          bool newState = doc["enabled"].as<bool>();
          AppState::getInstance().setBacklightOn(newState);
          LOG_I("[WebSocket] Backlight set to %s", newState ? "ON" : "OFF");
          sendDisplayState();
        } else if (msgType == "setScreenTimeout") {
          int timeoutSec = doc["value"].as<int>();
          unsigned long timeoutMs = (unsigned long)timeoutSec * 1000UL;
          if (timeoutMs == 0 || timeoutMs == 30000 || timeoutMs == 60000 ||
              timeoutMs == 300000 || timeoutMs == 600000) {
            AppState::getInstance().setScreenTimeout(timeoutMs);
            saveSettings();
            LOG_I("[WebSocket] Screen timeout set to %d seconds", timeoutSec);
            sendDisplayState();
          }
        } else if (msgType == "setBrightness") {
          int newBright = doc["value"].as<int>();
          if (newBright >= 1 && newBright <= 255) {
            AppState::getInstance().setBacklightBrightness((uint8_t)newBright);
            saveSettings();
            LOG_I("[WebSocket] Brightness set to %d", newBright);
            sendDisplayState();
          }
        } else if (msgType == "setBuzzerEnabled") {
          bool newState = doc["enabled"].as<bool>();
          AppState::getInstance().setBuzzerEnabled(newState);
          saveSettings();
          LOG_I("[WebSocket] Buzzer set to %s", newState ? "ON" : "OFF");
          sendBuzzerState();
        } else if (msgType == "setBuzzerVolume") {
          int newVol = doc["value"].as<int>();
          if (newVol >= 0 && newVol <= 2) {
            AppState::getInstance().setBuzzerVolume(newVol);
            saveSettings();
            LOG_I("[WebSocket] Buzzer volume set to %d", newVol);
            sendBuzzerState();
          }
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

void sendDisplayState() {
  JsonDocument doc;
  doc["type"] = "displayState";
  doc["backlightOn"] = AppState::getInstance().backlightOn;
  doc["screenTimeout"] = AppState::getInstance().screenTimeout / 1000; // Send as seconds
  doc["backlightBrightness"] = AppState::getInstance().backlightBrightness;
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

void sendBuzzerState() {
  JsonDocument doc;
  doc["type"] = "buzzerState";
  doc["enabled"] = AppState::getInstance().buzzerEnabled;
  doc["volume"] = AppState::getInstance().buzzerVolume;
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}

void sendMqttSettingsState() {
  JsonDocument doc;
  doc["type"] = "mqttSettings";
  doc["enabled"] = appState.mqttEnabled;
  doc["broker"] = appState.mqttBroker;
  doc["port"] = appState.mqttPort;
  doc["username"] = appState.mqttUsername;
  doc["hasPassword"] = (appState.mqttPassword.length() > 0);
  doc["baseTopic"] = appState.mqttBaseTopic;
  doc["haDiscovery"] = appState.mqttHADiscovery;
  doc["connected"] = appState.mqttConnected;
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t *)json.c_str(), json.length());
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
  
  // Storage - LittleFS
  doc["storage"]["LittleFSTotal"] = LittleFS.totalBytes();
  doc["storage"]["LittleFSUsed"] = LittleFS.usedBytes();
  
  // WiFi Information
  doc["wifi"]["rssi"] = WiFi.RSSI();
  doc["wifi"]["channel"] = WiFi.channel();
  doc["wifi"]["apClients"] = WiFi.softAPgetStationNum();
  doc["wifi"]["connected"] = (WiFi.status() == WL_CONNECTED);
  
  // Uptime (milliseconds since boot)
  doc["uptime"] = millis();

  // Reset reason
  doc["resetReason"] = getResetReasonString();

  // Broadcast to all WebSocket clients
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}
