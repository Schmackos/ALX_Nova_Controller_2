#include "websocket_handler.h"
#include "config.h"
#include "app_state.h"
#include "wifi_manager.h"
#include "smart_sensing.h"
#include "debug_serial.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

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

void sendHardwareStats() {
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
