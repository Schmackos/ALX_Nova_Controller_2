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
#include "i2s_audio.h"
#include "signal_generator.h"
#include "task_monitor.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "esp_freertos_hooks.h"

// ===== WebSocket Authentication Tracking =====
bool wsAuthStatus[MAX_WS_CLIENTS] = {false};
unsigned long wsAuthTimeout[MAX_WS_CLIENTS] = {0};

// ===== Per-client Audio Streaming Subscription =====
static bool _audioSubscribed[MAX_WS_CLIENTS] = {};

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
      _audioSubscribed[num] = false;
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
            sendSignalGenState();
            sendAudioGraphState();
            sendDebugState();

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
        } else if (msgType == "setDimEnabled") {
          bool newState = doc["enabled"].as<bool>();
          AppState::getInstance().setDimEnabled(newState);
          saveSettings();
          LOG_I("[WebSocket] Dim %s", newState ? "enabled" : "disabled");
          sendDisplayState();
        } else if (msgType == "setDimTimeout") {
          int dimSec = doc["value"].as<int>();
          unsigned long dimMs = (unsigned long)dimSec * 1000UL;
          if (dimMs == 5000 || dimMs == 10000 || dimMs == 15000 ||
              dimMs == 30000 || dimMs == 60000) {
            AppState::getInstance().setDimTimeout(dimMs);
            saveSettings();
            LOG_I("[WebSocket] Dim timeout set to %d seconds", dimSec);
            sendDisplayState();
          }
        } else if (msgType == "setDimBrightness") {
          int dimPwm = doc["value"].as<int>();
          if (dimPwm == 26 || dimPwm == 64 || dimPwm == 128 || dimPwm == 191) {
            AppState::getInstance().setDimBrightness((uint8_t)dimPwm);
            saveSettings();
            LOG_I("[WebSocket] Dim brightness set to %d", dimPwm);
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
        } else if (msgType == "subscribeAudio") {
          bool enabled = doc["enabled"] | false;
          _audioSubscribed[num] = enabled;
          LOG_I("[WebSocket] Client [%u] audio subscription %s", num, enabled ? "enabled" : "disabled");
        } else if (msgType == "setAudioUpdateRate") {
          int rate = doc["value"].as<int>();
          if (rate == 20 || rate == 33 || rate == 50 || rate == 100) {
            appState.audioUpdateRate = (uint16_t)rate;
            saveSettings();
            LOG_I("[WebSocket] Audio update rate set to %d ms", rate);
          }
        } else if (msgType == "setVuMeterEnabled") {
          appState.vuMeterEnabled = doc["enabled"].as<bool>();
          saveSettings();
          sendAudioGraphState();
          LOG_I("[WebSocket] VU meter %s", appState.vuMeterEnabled ? "enabled" : "disabled");
        } else if (msgType == "setWaveformEnabled") {
          appState.waveformEnabled = doc["enabled"].as<bool>();
          saveSettings();
          sendAudioGraphState();
          LOG_I("[WebSocket] Waveform %s", appState.waveformEnabled ? "enabled" : "disabled");
        } else if (msgType == "setSpectrumEnabled") {
          appState.spectrumEnabled = doc["enabled"].as<bool>();
          saveSettings();
          sendAudioGraphState();
          LOG_I("[WebSocket] Spectrum %s", appState.spectrumEnabled ? "enabled" : "disabled");
        } else if (msgType == "setSignalGen") {
          bool changed = false;
          if (doc["enabled"].is<bool>()) {
            appState.sigGenEnabled = doc["enabled"].as<bool>();
            changed = true;
          }
          if (doc["waveform"].is<int>()) {
            int w = doc["waveform"].as<int>();
            if (w >= 0 && w <= 3) { appState.sigGenWaveform = w; changed = true; }
          }
          if (doc["frequency"].is<float>()) {
            float f = doc["frequency"].as<float>();
            if (f >= 1.0f && f <= 22000.0f) { appState.sigGenFrequency = f; changed = true; }
          }
          if (doc["amplitude"].is<float>()) {
            float a = doc["amplitude"].as<float>();
            if (a >= -96.0f && a <= 0.0f) { appState.sigGenAmplitude = a; changed = true; }
          }
          if (doc["channel"].is<int>()) {
            int c = doc["channel"].as<int>();
            if (c >= 0 && c <= 2) { appState.sigGenChannel = c; changed = true; }
          }
          if (doc["outputMode"].is<int>()) {
            int m = doc["outputMode"].as<int>();
            if (m >= 0 && m <= 1) { appState.sigGenOutputMode = m; changed = true; }
          }
          if (doc["sweepSpeed"].is<float>()) {
            float s = doc["sweepSpeed"].as<float>();
            if (s >= 1.0f && s <= 22000.0f) { appState.sigGenSweepSpeed = s; changed = true; }
          }
          if (doc["targetAdc"].is<int>()) {
            int t = doc["targetAdc"].as<int>();
            if (t >= 0 && t <= 2) { appState.sigGenTargetAdc = t; changed = true; }
          }
          if (changed) {
            siggen_apply_params();
            saveSignalGenSettings();
            sendSignalGenState();
            LOG_I("[WebSocket] Signal generator updated by client [%u]", num);
          }
        } else if (msgType == "setInputNames") {
          if (doc["names"].is<JsonArray>()) {
            JsonArray names = doc["names"].as<JsonArray>();
            for (int i = 0; i < NUM_AUDIO_ADCS * 2 && i < (int)names.size(); i++) {
              String name = names[i].as<String>();
              if (name.length() > 0) appState.inputNames[i] = name;
            }
            saveInputNames();
            // Broadcast updated names
            JsonDocument resp;
            resp["type"] = "inputNames";
            JsonArray outNames = resp["names"].to<JsonArray>();
            for (int i = 0; i < NUM_AUDIO_ADCS * 2; i++) {
              outNames.add(appState.inputNames[i]);
            }
            String json;
            serializeJson(resp, json);
            webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
            LOG_I("[WebSocket] Input names updated by client [%u]", num);
          }
        } else if (msgType == "setDebugMode") {
          appState.debugMode = doc["enabled"].as<bool>();
          applyDebugSerialLevel(appState.debugMode, appState.debugSerialLevel);
          saveSettings();
          sendDebugState();
          LOG_I("[WebSocket] Debug mode %s", appState.debugMode ? "enabled" : "disabled");
        } else if (msgType == "setDebugSerialLevel") {
          int level = doc["level"].as<int>();
          if (level >= 0 && level <= 3) {
            appState.debugSerialLevel = level;
            applyDebugSerialLevel(appState.debugMode, appState.debugSerialLevel);
            saveSettings();
            sendDebugState();
            LOG_I("[WebSocket] Debug serial level set to %d", level);
          }
        } else if (msgType == "setDebugHwStats") {
          appState.debugHwStats = doc["enabled"].as<bool>();
          saveSettings();
          sendDebugState();
          LOG_I("[WebSocket] Debug HW stats %s", appState.debugHwStats ? "enabled" : "disabled");
        } else if (msgType == "setDebugI2sMetrics") {
          appState.debugI2sMetrics = doc["enabled"].as<bool>();
          saveSettings();
          sendDebugState();
          LOG_I("[WebSocket] Debug I2S metrics %s", appState.debugI2sMetrics ? "enabled" : "disabled");
        } else if (msgType == "setDebugTaskMonitor") {
          appState.debugTaskMonitor = doc["enabled"].as<bool>();
          saveSettings();
          sendDebugState();
          LOG_I("[WebSocket] Debug task monitor %s", appState.debugTaskMonitor ? "enabled" : "disabled");
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
  doc["dimEnabled"] = AppState::getInstance().dimEnabled;
  doc["dimTimeout"] = AppState::getInstance().dimTimeout / 1000;
  doc["dimBrightness"] = AppState::getInstance().dimBrightness;
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

void sendSignalGenState() {
  JsonDocument doc;
  doc["type"] = "signalGenerator";
  doc["enabled"] = appState.sigGenEnabled;
  doc["waveform"] = appState.sigGenWaveform;
  doc["frequency"] = appState.sigGenFrequency;
  doc["amplitude"] = appState.sigGenAmplitude;
  doc["channel"] = appState.sigGenChannel;
  doc["outputMode"] = appState.sigGenOutputMode;
  doc["sweepSpeed"] = appState.sigGenSweepSpeed;
  doc["targetAdc"] = appState.sigGenTargetAdc;
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}

void sendAudioGraphState() {
  JsonDocument doc;
  doc["type"] = "audioGraphState";
  doc["vuMeterEnabled"] = appState.vuMeterEnabled;
  doc["waveformEnabled"] = appState.waveformEnabled;
  doc["spectrumEnabled"] = appState.spectrumEnabled;
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}

void sendDebugState() {
  JsonDocument doc;
  doc["type"] = "debugState";
  doc["debugMode"] = appState.debugMode;
  doc["debugSerialLevel"] = appState.debugSerialLevel;
  doc["debugHwStats"] = appState.debugHwStats;
  doc["debugI2sMetrics"] = appState.debugI2sMetrics;
  doc["debugTaskMonitor"] = appState.debugTaskMonitor;
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
  // Early return if debug HW stats is disabled
  if (!(appState.debugMode && appState.debugHwStats)) return;

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
  
  // Audio ADC diagnostics (per-ADC)
  doc["audio"]["sampleRate"] = appState.audioSampleRate;
  doc["audio"]["adcVref"] = appState.adcVref;
  doc["audio"]["numAdcsDetected"] = appState.numAdcsDetected;
  JsonArray adcArr = doc["audio"]["adcs"].to<JsonArray>();
  for (int a = 0; a < NUM_AUDIO_ADCS; a++) {
    JsonObject adcObj = adcArr.add<JsonObject>();
    const AppState::AdcState &adc = appState.audioAdc[a];
    const char *statusStr = "OK";
    switch (adc.healthStatus) {
      case 1: statusStr = "NO_DATA"; break;
      case 2: statusStr = "NOISE_ONLY"; break;
      case 3: statusStr = "CLIPPING"; break;
      case 4: statusStr = "I2S_ERROR"; break;
      case 5: statusStr = "HW_FAULT"; break;
    }
    adcObj["status"] = statusStr;
    adcObj["noiseFloorDbfs"] = adc.noiseFloorDbfs;
    adcObj["i2sErrors"] = adc.i2sErrors;
    adcObj["consecutiveZeros"] = adc.consecutiveZeros;
    adcObj["totalBuffers"] = adc.totalBuffers;
    adcObj["vrms"] = adc.vrmsCombined;
  }
  // Legacy flat fields for backward compat
  doc["audio"]["adcStatus"] = adcArr[0]["status"];
  doc["audio"]["noiseFloorDbfs"] = appState.audioNoiseFloorDbfs;
  doc["audio"]["vrms"] = appState.audioVrmsCombined;

  // I2S Static Config
  I2sStaticConfig i2sCfg = i2s_audio_get_static_config();
  JsonArray i2sCfgArr = doc["audio"]["i2sConfig"].to<JsonArray>();
  for (int a = 0; a < NUM_AUDIO_ADCS; a++) {
    JsonObject c = i2sCfgArr.add<JsonObject>();
    c["mode"] = i2sCfg.adc[a].isMaster ? "Master RX" : "Slave RX";
    c["sampleRate"] = i2sCfg.adc[a].sampleRate;
    c["bitsPerSample"] = i2sCfg.adc[a].bitsPerSample;
    c["channelFormat"] = i2sCfg.adc[a].channelFormat;
    c["dmaBufCount"] = i2sCfg.adc[a].dmaBufCount;
    c["dmaBufLen"] = i2sCfg.adc[a].dmaBufLen;
    c["apll"] = i2sCfg.adc[a].apllEnabled;
    c["mclkHz"] = i2sCfg.adc[a].mclkHz;
    c["commFormat"] = i2sCfg.adc[a].commFormat;
  }

  // I2S Runtime Metrics
  JsonObject i2sRt = doc["audio"]["i2sRuntime"].to<JsonObject>();
  i2sRt["stackFree"] = appState.i2sMetrics.audioTaskStackFree;
  JsonArray bpsArr = i2sRt["buffersPerSec"].to<JsonArray>();
  JsonArray latArr = i2sRt["avgReadLatencyUs"].to<JsonArray>();
  for (int a = 0; a < NUM_AUDIO_ADCS; a++) {
    bpsArr.add(serialized(String(appState.i2sMetrics.buffersPerSec[a], 1)));
    latArr.add(serialized(String(appState.i2sMetrics.avgReadLatencyUs[a], 0)));
  }

  // Task Monitor (gated by debug toggle)
  // Note: task_monitor_update() runs on its own 5s timer in main loop
  if (appState.debugMode && appState.debugTaskMonitor) {
    const TaskMonitorData& tm = task_monitor_get_data();
    doc["tasks"]["count"] = tm.taskCount;
    doc["tasks"]["loopUs"] = tm.loopTimeUs;
    doc["tasks"]["loopMaxUs"] = tm.loopTimeMaxUs;
    doc["tasks"]["loopAvgUs"] = tm.loopTimeAvgUs;
    JsonArray taskArr = doc["tasks"]["list"].to<JsonArray>();
    for (int i = 0; i < tm.taskCount; i++) {
      JsonObject t = taskArr.add<JsonObject>();
      t["name"] = tm.tasks[i].name;
      t["stackFree"] = tm.tasks[i].stackFreeBytes;
      t["stackAlloc"] = tm.tasks[i].stackAllocBytes;
      t["pri"] = tm.tasks[i].priority;
      t["state"] = tm.tasks[i].state;
      t["core"] = tm.tasks[i].coreId;
    }
  }

  // Uptime (milliseconds since boot)
  doc["uptime"] = millis();

  // Reset reason
  doc["resetReason"] = getResetReasonString();

  // Broadcast to all WebSocket clients
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}

// ===== Audio Streaming to Subscribed Clients =====

void sendAudioData() {
  // Early return if no clients are subscribed
  bool anySubscribed = false;
  for (int i = 0; i < MAX_WS_CLIENTS; i++) {
    if (_audioSubscribed[i]) {
      anySubscribed = true;
      break;
    }
  }
  if (!anySubscribed) return;

  // --- Audio levels (VU, peak, RMS, diagnostics) ---
  {
    JsonDocument doc;
    doc["type"] = "audioLevels";
    doc["audioLevel"] = audioLevel_dBFS;
    doc["signalDetected"] = (audioLevel_dBFS >= audioThreshold_dBFS);
    doc["numAdcsDetected"] = appState.numAdcsDetected;
    // Per-ADC data array
    JsonArray adcArr = doc["adc"].to<JsonArray>();
    JsonArray adcStatusArr = doc["adcStatus"].to<JsonArray>();
    JsonArray adcNoiseArr = doc["adcNoiseFloor"].to<JsonArray>();
    for (int a = 0; a < NUM_AUDIO_ADCS; a++) {
      const AppState::AdcState &adc = appState.audioAdc[a];
      JsonObject adcObj = adcArr.add<JsonObject>();
      adcObj["vu1"] = adc.vu1;
      adcObj["vu2"] = adc.vu2;
      adcObj["peak1"] = adc.peak1;
      adcObj["peak2"] = adc.peak2;
      adcObj["rms1"] = adc.rms1;
      adcObj["rms2"] = adc.rms2;
      adcObj["vrms1"] = adc.vrms1;
      adcObj["vrms2"] = adc.vrms2;
      adcObj["dBFS"] = adc.dBFS;
      const char *statusStr = "OK";
      switch (adc.healthStatus) {
        case 1: statusStr = "NO_DATA"; break;
        case 2: statusStr = "NOISE_ONLY"; break;
        case 3: statusStr = "CLIPPING"; break;
        case 4: statusStr = "I2S_ERROR"; break;
        case 5: statusStr = "HW_FAULT"; break;
      }
      adcStatusArr.add(statusStr);
      adcNoiseArr.add(adc.noiseFloorDbfs);
    }
    // Legacy flat fields for backward compat (ADC 0)
    doc["audioRms1"] = appState.audioRmsLeft;
    doc["audioRms2"] = appState.audioRmsRight;
    doc["audioVu1"] = appState.audioVuLeft;
    doc["audioVu2"] = appState.audioVuRight;
    doc["audioPeak1"] = appState.audioPeakLeft;
    doc["audioPeak2"] = appState.audioPeakRight;
    doc["audioPeak"] = appState.audioPeakCombined;
    doc["audioVrms1"] = appState.audioVrms1;
    doc["audioVrms2"] = appState.audioVrms2;
    doc["audioVrms"] = appState.audioVrmsCombined;
    String json;
    serializeJson(doc, json);
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
      if (_audioSubscribed[i]) {
        webSocket.sendTXT(i, (uint8_t*)json.c_str(), json.length());
      }
    }
  }

  // --- Waveform data (per-ADC) — binary: [type:1][adc:1][samples:256] ---
  if (appState.waveformEnabled) {
    uint8_t wfBin[2 + WAVEFORM_BUFFER_SIZE]; // 258 bytes
    wfBin[0] = WS_BIN_WAVEFORM;
    for (int a = 0; a < appState.numAdcsDetected; a++) {
      if (i2s_audio_get_waveform(wfBin + 2, a)) {
        wfBin[1] = (uint8_t)a;
        for (int i = 0; i < MAX_WS_CLIENTS; i++) {
          if (_audioSubscribed[i]) {
            webSocket.sendBIN(i, wfBin, sizeof(wfBin));
          }
        }
      }
    }
  }

  // --- Spectrum data (per-ADC) — binary: [type:1][adc:1][freq:f32LE][bands:16xf32LE] ---
  if (appState.spectrumEnabled) {
    uint8_t spBin[2 + sizeof(float) + SPECTRUM_BANDS * sizeof(float)]; // 70 bytes
    spBin[0] = WS_BIN_SPECTRUM;
    float bands[SPECTRUM_BANDS];
    float freq = 0.0f;
    for (int a = 0; a < appState.numAdcsDetected; a++) {
      if (i2s_audio_get_spectrum(bands, &freq, a)) {
        spBin[1] = (uint8_t)a;
        memcpy(spBin + 2, &freq, sizeof(float));
        memcpy(spBin + 2 + sizeof(float), bands, SPECTRUM_BANDS * sizeof(float));
        for (int i = 0; i < MAX_WS_CLIENTS; i++) {
          if (_audioSubscribed[i]) {
            webSocket.sendBIN(i, spBin, sizeof(spBin));
          }
        }
      }
    }
  }
}
