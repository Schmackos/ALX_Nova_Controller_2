#include "settings_manager.h"
#include "app_state.h"
#include "config.h"
#include "debug_serial.h"
#include "mqtt_handler.h"
#include "ota_updater.h"
#include "smart_sensing.h"
#include "utils.h"
#include "wifi_manager.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>

// Note: Certificate management removed - now using Mozilla certificate bundle
// via ESP32CertBundle library for automatic SSL validation of all public
// servers

// ===== Settings Persistence =====

bool loadSettings() {
  // Use create=true to avoid "no permits for creation" error log if file is
  // missing
  File file = LittleFS.open("/settings.txt", "r", true);
  if (!file || file.size() == 0) {
    if (file)
      file.close();
    return false;
  }

  // Read ALL lines before closing the file
  String line1 = file.readStringUntil('\n');
  String line2 = file.readStringUntil('\n');
  String line3 = file.readStringUntil('\n');
  String line4 = file.readStringUntil('\n');
  String line5 = file.readStringUntil('\n');
  String line6 = file.readStringUntil('\n');
  String line7 = file.readStringUntil('\n');
  String line8 = file.readStringUntil('\n');
  String line9 = file.readStringUntil('\n');
  String line10 = file.readStringUntil('\n');
  String line11 = file.readStringUntil('\n');
  String line12 = file.readStringUntil('\n');
  file.close();

  line1.trim();
  if (line1.length() == 0) {
    return false;
  }

  autoUpdateEnabled = (line1.toInt() != 0);

  // Load timezone offset (if available, otherwise default to 0)
  if (line2.length() > 0) {
    line2.trim();
    timezoneOffset = line2.toInt();
  }

  // Load DST offset (if available, otherwise default to 0)
  if (line3.length() > 0) {
    line3.trim();
    dstOffset = line3.toInt();
  }

  // Load night mode (if available, otherwise default to false)
  if (line4.length() > 0) {
    line4.trim();
    nightMode = (line4.toInt() != 0);
  }

  // Load cert validation setting (if available, otherwise default to true from
  // AppState)
  if (line5.length() > 0) {
    line5.trim();
    enableCertValidation = (line5.toInt() != 0);
  }

  // Load hardware stats interval (if available, otherwise default to 2000ms)
  if (line6.length() > 0) {
    line6.trim();
    unsigned long interval = line6.toInt();
    // Validate: only allow 1000, 2000, 3000, 5000, or 10000 ms
    if (interval == 1000 || interval == 2000 || interval == 3000 ||
        interval == 5000 || interval == 10000) {
      hardwareStatsInterval = interval;
    }
  }

  if (line7.length() > 0) {
    line7.trim();
    autoAPEnabled = (line7.toInt() != 0);
  } else {
    // Default to true if not present (backward compatibility)
    autoAPEnabled = true;
  }

#ifdef GUI_ENABLED
  // Load boot animation enabled (if available, otherwise default to true)
  if (line8.length() > 0) {
    line8.trim();
    appState.bootAnimEnabled = (line8.toInt() != 0);
  }

  // Load boot animation style (if available, otherwise default to 0)
  if (line9.length() > 0) {
    line9.trim();
    int style = line9.toInt();
    if (style >= 0 && style <= 5) {
      appState.bootAnimStyle = style;
    }
  }
#endif

  // Load screen timeout (if available, otherwise keep default)
  if (line10.length() > 0) {
    line10.trim();
    unsigned long timeout = line10.toInt();
    // Validate: only allow 0 (never), 30000, 60000, 300000, 600000 ms
    if (timeout == 0 || timeout == 30000 || timeout == 60000 ||
        timeout == 300000 || timeout == 600000) {
      appState.screenTimeout = timeout;
    }
  }

  // Load buzzer enabled (if available, otherwise default to true)
  if (line11.length() > 0) {
    line11.trim();
    appState.buzzerEnabled = (line11.toInt() != 0);
  }

  // Load buzzer volume (if available, otherwise default to 1=Medium)
  if (line12.length() > 0) {
    line12.trim();
    int vol = line12.toInt();
    if (vol >= 0 && vol <= 2) {
      appState.buzzerVolume = vol;
    }
  }

  return true;
}

void saveSettings() {
  File file = LittleFS.open("/settings.txt", "w");
  if (!file) {
    DebugOut.println("Failed to open settings file for writing");
    return;
  }

  file.println(autoUpdateEnabled ? "1" : "0");
  file.println(timezoneOffset);
  file.println(dstOffset);
  file.println(nightMode ? "1" : "0");
  file.println(enableCertValidation ? "1" : "0");
  file.println(hardwareStatsInterval);
  file.println(autoAPEnabled ? "1" : "0");
#ifdef GUI_ENABLED
  file.println(appState.bootAnimEnabled ? "1" : "0");
  file.println(appState.bootAnimStyle);
#else
  file.println("1"); // placeholder for bootAnimEnabled
  file.println("0"); // placeholder for bootAnimStyle
#endif
  file.println(appState.screenTimeout);
  file.println(appState.buzzerEnabled ? "1" : "0");
  file.println(appState.buzzerVolume);
  file.close();
  DebugOut.println("Settings saved to LittleFS");
}

// ===== Factory Reset =====

void performFactoryReset() {
  DebugOut.println("\n=== FACTORY RESET INITIATED ===");
  factoryResetInProgress = true;

  // Visual feedback: solid LED
  digitalWrite(LED_PIN, HIGH);

  // 1. Clear WiFi settings from NVS
  DebugOut.println("Clearing WiFi credentials (NVS: wifi-list)...");
  Preferences wifiPrefs;
  wifiPrefs.begin("wifi-list", false);
  wifiPrefs.clear();
  wifiPrefs.end();

  // 2. Clear Auth settings from NVS
  DebugOut.println("Clearing Auth settings (NVS: auth)...");
  Preferences authPrefs;
  authPrefs.begin("auth", false);
  authPrefs.clear();
  authPrefs.end();

  // 3. Format LittleFS (erases /settings.txt, /mqtt_config.txt,
  // /smartsensing.txt, etc.)
  DebugOut.println("Formatting LittleFS...");
  if (LittleFS.format()) {
    DebugOut.println("LittleFS formatted successfully");
  } else {
    DebugOut.println("LittleFS format failed");
  }

  // End LittleFS
  LittleFS.end();

  // 4. Force AP Mode defaults for next boot
  apEnabled = true;
  isAPMode = true;

  DebugOut.println("=== FACTORY RESET COMPLETE ===");

  // Broadcast factory reset complete message
  JsonDocument completeDoc;
  completeDoc["type"] = "factoryResetStatus";
  completeDoc["status"] = "complete";
  completeDoc["message"] = "Factory reset complete";
  String completeJson;
  serializeJson(completeDoc, completeJson);
  webSocket.broadcastTXT((uint8_t *)completeJson.c_str(),
                         completeJson.length());
  webSocket.loop(); // Ensure message is sent

  delay(500); // Give time for message to be received

  DebugOut.println("Rebooting in 2 seconds...");

  // Broadcast rebooting message
  JsonDocument rebootDoc;
  rebootDoc["type"] = "factoryResetStatus";
  rebootDoc["status"] = "rebooting";
  rebootDoc["message"] = "Rebooting device...";
  String rebootJson;
  serializeJson(rebootDoc, rebootJson);
  webSocket.broadcastTXT((uint8_t *)rebootJson.c_str(), rebootJson.length());
  webSocket.loop(); // Ensure message is sent

  delay(2000);
  ESP.restart();
}

// ===== Settings HTTP API Handlers =====

void handleSettingsGet() {
  JsonDocument doc;
  doc["success"] = true;
  doc["autoUpdateEnabled"] = autoUpdateEnabled;
  doc["timezoneOffset"] = timezoneOffset;
  doc["dstOffset"] = dstOffset;
  doc["nightMode"] = nightMode;
  doc["enableCertValidation"] = enableCertValidation;
  doc["autoAPEnabled"] = autoAPEnabled;
  doc["hardwareStatsInterval"] =
      hardwareStatsInterval / 1000; // Send as seconds
  doc["screenTimeout"] = appState.screenTimeout / 1000; // Send as seconds
  doc["backlightOn"] = appState.backlightOn;
  doc["buzzerEnabled"] = appState.buzzerEnabled;
  doc["buzzerVolume"] = appState.buzzerVolume;
#ifdef GUI_ENABLED
  doc["bootAnimEnabled"] = appState.bootAnimEnabled;
  doc["bootAnimStyle"] = appState.bootAnimStyle;
#endif

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleSettingsUpdate() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"No data received\"}");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"Invalid JSON\"}");
    return;
  }

  bool settingsChanged = false;

  if (doc["autoUpdateEnabled"].is<bool>()) {
    autoUpdateEnabled = doc["autoUpdateEnabled"].as<bool>();
    settingsChanged = true;
  }

  bool timezoneChanged = false;

  if (doc["timezoneOffset"].is<int>()) {
    int newOffset = doc["timezoneOffset"].as<int>();
    if (newOffset != timezoneOffset) {
      timezoneOffset = newOffset;
      settingsChanged = true;
      timezoneChanged = true;
    }
  }

  if (doc["dstOffset"].is<int>()) {
    int newDstOffset = doc["dstOffset"].as<int>();
    if (newDstOffset != dstOffset) {
      dstOffset = newDstOffset;
      settingsChanged = true;
      timezoneChanged = true;
    }
  }

  // Re-sync time if timezone or DST changed
  if (timezoneChanged && WiFi.status() == WL_CONNECTED) {
    syncTimeWithNTP();
  }

  if (doc["nightMode"].is<bool>()) {
    bool newNightMode = doc["nightMode"].as<bool>();
    if (newNightMode != nightMode) {
      nightMode = newNightMode;
      settingsChanged = true;
    }
  }

  if (doc["enableCertValidation"].is<bool>()) {
    bool newCertValidation = doc["enableCertValidation"].as<bool>();
    if (newCertValidation != enableCertValidation) {
      enableCertValidation = newCertValidation;
      settingsChanged = true;
      DebugOut.printf("Certificate validation %s\n",
                      enableCertValidation ? "ENABLED" : "DISABLED");
    }
  }

  if (doc["hardwareStatsInterval"].is<int>()) {
    int newInterval = doc["hardwareStatsInterval"].as<int>();
    // Validate: only allow 1, 2, 3, 5, or 10 seconds
    if (newInterval == 1 || newInterval == 2 || newInterval == 3 ||
        newInterval == 5 || newInterval == 10) {
      unsigned long newIntervalMs = newInterval * 1000UL;
      if (newIntervalMs != hardwareStatsInterval) {
        hardwareStatsInterval = newIntervalMs;
        settingsChanged = true;
        DebugOut.printf("Hardware stats interval set to %d seconds\n",
                        newInterval);
      }
    }
  }

  if (doc["autoAPEnabled"].is<bool>()) {
    bool newAutoAP = doc["autoAPEnabled"].as<bool>();
    if (newAutoAP != autoAPEnabled) {
      autoAPEnabled = newAutoAP;
      settingsChanged = true;
      DebugOut.printf("Auto AP %s\n", autoAPEnabled ? "enabled" : "disabled");
    }
  }

  if (doc["screenTimeout"].is<int>()) {
    int newTimeoutSec = doc["screenTimeout"].as<int>();
    // Convert seconds to milliseconds and validate allowed values
    unsigned long newTimeoutMs = (unsigned long)newTimeoutSec * 1000UL;
    if (newTimeoutMs == 0 || newTimeoutMs == 30000 || newTimeoutMs == 60000 ||
        newTimeoutMs == 300000 || newTimeoutMs == 600000) {
      if (newTimeoutMs != appState.screenTimeout) {
        appState.setScreenTimeout(newTimeoutMs);
        settingsChanged = true;
        DebugOut.printf("Screen timeout set to %d seconds\n", newTimeoutSec);
      }
    }
  }

  if (doc["backlightOn"].is<bool>()) {
    bool newBacklight = doc["backlightOn"].as<bool>();
    if (newBacklight != appState.backlightOn) {
      appState.setBacklightOn(newBacklight);
      DebugOut.printf("Backlight set to %s\n", newBacklight ? "ON" : "OFF");
    }
    // backlightOn is runtime only, no save
  }

  if (doc["buzzerEnabled"].is<bool>()) {
    bool newBuzzer = doc["buzzerEnabled"].as<bool>();
    if (newBuzzer != appState.buzzerEnabled) {
      appState.setBuzzerEnabled(newBuzzer);
      settingsChanged = true;
      DebugOut.printf("Buzzer %s\n", newBuzzer ? "enabled" : "disabled");
    }
  }

  if (doc["buzzerVolume"].is<int>()) {
    int newVol = doc["buzzerVolume"].as<int>();
    if (newVol >= 0 && newVol <= 2 && newVol != appState.buzzerVolume) {
      appState.setBuzzerVolume(newVol);
      settingsChanged = true;
      DebugOut.printf("Buzzer volume set to %d\n", newVol);
    }
  }

#ifdef GUI_ENABLED
  if (doc["bootAnimEnabled"].is<bool>()) {
    bool newBootAnim = doc["bootAnimEnabled"].as<bool>();
    if (newBootAnim != appState.bootAnimEnabled) {
      appState.bootAnimEnabled = newBootAnim;
      settingsChanged = true;
      DebugOut.printf("Boot animation %s\n",
                      appState.bootAnimEnabled ? "enabled" : "disabled");
    }
  }

  if (doc["bootAnimStyle"].is<int>()) {
    int newStyle = doc["bootAnimStyle"].as<int>();
    if (newStyle >= 0 && newStyle <= 5 &&
        newStyle != appState.bootAnimStyle) {
      appState.bootAnimStyle = newStyle;
      settingsChanged = true;
      DebugOut.printf("Boot animation style set to %d\n",
                      appState.bootAnimStyle);
    }
  }
#endif

  if (settingsChanged) {
    saveSettings();
  }

  JsonDocument resp;
  resp["success"] = true;
  resp["autoUpdateEnabled"] = autoUpdateEnabled;
  resp["timezoneOffset"] = timezoneOffset;
  resp["dstOffset"] = dstOffset;
  resp["nightMode"] = nightMode;
  resp["nightMode"] = nightMode;
  resp["enableCertValidation"] = enableCertValidation;
  resp["autoAPEnabled"] = autoAPEnabled;
  resp["hardwareStatsInterval"] = hardwareStatsInterval / 1000;
  resp["screenTimeout"] = appState.screenTimeout / 1000;
  resp["backlightOn"] = appState.backlightOn;
  resp["buzzerEnabled"] = appState.buzzerEnabled;
  resp["buzzerVolume"] = appState.buzzerVolume;
#ifdef GUI_ENABLED
  resp["bootAnimEnabled"] = appState.bootAnimEnabled;
  resp["bootAnimStyle"] = appState.bootAnimStyle;
#endif
  String json;
  serializeJson(resp, json);
  server.send(200, "application/json", json);
}

void handleSettingsExport() {
  DebugOut.println("Settings export requested via web interface");

  JsonDocument doc;

  // Device info
  doc["deviceInfo"]["manufacturer"] = MANUFACTURER_NAME;
  doc["deviceInfo"]["model"] = MANUFACTURER_MODEL;
  doc["deviceInfo"]["serialNumber"] = deviceSerialNumber;
  doc["deviceInfo"]["firmwareVersion"] = firmwareVer;
  doc["deviceInfo"]["mac"] = WiFi.macAddress();
  doc["deviceInfo"]["chipId"] =
      String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF), HEX);

  // WiFi settings
  doc["wifi"]["ssid"] = wifiSSID;
  doc["wifi"]["password"] = wifiPassword;

  // AP settings
  doc["accessPoint"]["enabled"] = apEnabled;
  doc["accessPoint"]["ssid"] = apSSID;
  doc["accessPoint"]["password"] = apPassword;
  doc["accessPoint"]["autoAPEnabled"] = autoAPEnabled;

  // General settings
  doc["settings"]["autoUpdateEnabled"] = autoUpdateEnabled;
  doc["settings"]["timezoneOffset"] = timezoneOffset;
  doc["settings"]["dstOffset"] = dstOffset;
  doc["settings"]["nightMode"] = nightMode;
  doc["settings"]["enableCertValidation"] = enableCertValidation;
  doc["settings"]["blinkingEnabled"] = blinkingEnabled;
  doc["settings"]["hardwareStatsInterval"] = hardwareStatsInterval / 1000;
  doc["settings"]["screenTimeout"] = appState.screenTimeout / 1000;
  doc["settings"]["buzzerEnabled"] = appState.buzzerEnabled;
  doc["settings"]["buzzerVolume"] = appState.buzzerVolume;
#ifdef GUI_ENABLED
  doc["settings"]["bootAnimEnabled"] = appState.bootAnimEnabled;
  doc["settings"]["bootAnimStyle"] = appState.bootAnimStyle;
#endif

  // Smart Sensing settings
  String modeStr;
  switch (currentMode) {
  case ALWAYS_ON:
    modeStr = "always_on";
    break;
  case ALWAYS_OFF:
    modeStr = "always_off";
    break;
  case SMART_AUTO:
    modeStr = "smart_auto";
    break;
  }
  doc["smartSensing"]["mode"] = modeStr;
  doc["smartSensing"]["timerDuration"] = timerDuration;
  doc["smartSensing"]["voltageThreshold"] = voltageThreshold;

  // MQTT settings (password excluded for security)
  doc["mqtt"]["enabled"] = mqttEnabled;
  doc["mqtt"]["broker"] = mqttBroker;
  doc["mqtt"]["port"] = mqttPort;
  doc["mqtt"]["username"] = mqttUsername;
  doc["mqtt"]["baseTopic"] = mqttBaseTopic;
  doc["mqtt"]["haDiscovery"] = mqttHADiscovery;
  // Note: Password is intentionally excluded from export for security

  // Note: Certificate management removed - now using Mozilla certificate bundle

  // Generate timestamp
  time_t now = time(nullptr);
  struct tm timeinfo;
  char timestamp[32];
  if (getLocalTime(&timeinfo)) {
    snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02dT%02d:%02d:%02d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  } else {
    snprintf(timestamp, sizeof(timestamp), "unknown");
  }
  doc["exportInfo"]["timestamp"] = timestamp;
  doc["exportInfo"]["version"] = "1.0";

  String json;
  serializeJsonPretty(doc, json);

  // Send as downloadable JSON file
  server.sendHeader("Content-Disposition",
                    "attachment; filename=\"device-settings.json\"");
  server.send(200, "application/json", json);

  DebugOut.println("Settings exported successfully");
}

void handleSettingsImport() {
  DebugOut.println("Settings import requested via web interface");

  if (!server.hasArg("plain")) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"No data received\"}");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    DebugOut.printf("JSON parsing failed: %s\n", error.c_str());
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"Invalid JSON format\"}");
    return;
  }

  // Validate it's a settings export file
  if (doc["exportInfo"].isNull() || doc["settings"].isNull()) {
    server.send(
        400, "application/json",
        "{\"success\": false, \"message\": \"Invalid settings file format\"}");
    return;
  }

  DebugOut.println("Importing settings...");

  // Import WiFi settings
  if (!doc["wifi"].isNull()) {
    if (doc["wifi"]["ssid"].is<String>()) {
      wifiSSID = doc["wifi"]["ssid"].as<String>();
      DebugOut.printf("WiFi SSID: %s\n", wifiSSID.c_str());
    }
    if (doc["wifi"]["password"].is<String>()) {
      wifiPassword = doc["wifi"]["password"].as<String>();
      DebugOut.println("WiFi password imported");
    }
    // Save WiFi credentials to multi-WiFi list
    if (wifiSSID.length() > 0) {
      saveWiFiNetwork(wifiSSID.c_str(), wifiPassword.c_str());
    }
  }

  // Import AP settings
  if (!doc["accessPoint"].isNull()) {
    if (doc["accessPoint"]["enabled"].is<bool>()) {
      apEnabled = doc["accessPoint"]["enabled"].as<bool>();
      DebugOut.printf("AP Enabled: %s\n", apEnabled ? "true" : "false");
    }
    if (doc["accessPoint"]["ssid"].is<String>()) {
      apSSID = doc["accessPoint"]["ssid"].as<String>();
      DebugOut.printf("AP SSID: %s\n", apSSID.c_str());
    }
    if (doc["accessPoint"]["password"].is<String>()) {
      apPassword = doc["accessPoint"]["password"].as<String>();
      DebugOut.println("AP password imported");
    }
    if (doc["accessPoint"]["autoAPEnabled"].is<bool>()) {
      autoAPEnabled = doc["accessPoint"]["autoAPEnabled"].as<bool>();
      DebugOut.printf("Auto AP: %s\n", autoAPEnabled ? "enabled" : "disabled");
    }
  }

  // Import general settings
  if (!doc["settings"].isNull()) {
    if (doc["settings"]["autoUpdateEnabled"].is<bool>()) {
      autoUpdateEnabled = doc["settings"]["autoUpdateEnabled"].as<bool>();
      DebugOut.printf("Auto Update: %s\n",
                      autoUpdateEnabled ? "enabled" : "disabled");
    }
    if (doc["settings"]["timezoneOffset"].is<int>()) {
      timezoneOffset = doc["settings"]["timezoneOffset"].as<int>();
      DebugOut.printf("Timezone Offset: %d\n", timezoneOffset);
    }
    if (doc["settings"]["dstOffset"].is<int>()) {
      dstOffset = doc["settings"]["dstOffset"].as<int>();
      DebugOut.printf("DST Offset: %d\n", dstOffset);
    }
    if (doc["settings"]["nightMode"].is<bool>()) {
      nightMode = doc["settings"]["nightMode"].as<bool>();
      DebugOut.printf("Night Mode: %s\n", nightMode ? "enabled" : "disabled");
    }
    if (doc["settings"]["enableCertValidation"].is<bool>()) {
      enableCertValidation = doc["settings"]["enableCertValidation"].as<bool>();
      DebugOut.printf("Cert Validation: %s\n",
                      enableCertValidation ? "enabled" : "disabled");
    }
    if (doc["settings"]["blinkingEnabled"].is<bool>()) {
      blinkingEnabled = doc["settings"]["blinkingEnabled"].as<bool>();
      DebugOut.printf("Blinking: %s\n",
                      blinkingEnabled ? "enabled" : "disabled");
    }
    if (doc["settings"]["hardwareStatsInterval"].is<int>()) {
      int interval = doc["settings"]["hardwareStatsInterval"].as<int>();
      if (interval == 1 || interval == 2 || interval == 3 || interval == 5 ||
          interval == 10) {
        hardwareStatsInterval = interval * 1000UL;
        DebugOut.printf("Hardware Stats Interval: %d seconds\n", interval);
      }
    }
    if (doc["settings"]["screenTimeout"].is<int>()) {
      int timeoutSec = doc["settings"]["screenTimeout"].as<int>();
      unsigned long timeoutMs = (unsigned long)timeoutSec * 1000UL;
      if (timeoutMs == 0 || timeoutMs == 30000 || timeoutMs == 60000 ||
          timeoutMs == 300000 || timeoutMs == 600000) {
        appState.screenTimeout = timeoutMs;
        DebugOut.printf("Screen Timeout: %d seconds\n", timeoutSec);
      }
    }
    if (doc["settings"]["buzzerEnabled"].is<bool>()) {
      appState.buzzerEnabled = doc["settings"]["buzzerEnabled"].as<bool>();
      DebugOut.printf("Buzzer: %s\n",
                      appState.buzzerEnabled ? "enabled" : "disabled");
    }
    if (doc["settings"]["buzzerVolume"].is<int>()) {
      int vol = doc["settings"]["buzzerVolume"].as<int>();
      if (vol >= 0 && vol <= 2) {
        appState.buzzerVolume = vol;
        DebugOut.printf("Buzzer Volume: %d\n", vol);
      }
    }
#ifdef GUI_ENABLED
    if (doc["settings"]["bootAnimEnabled"].is<bool>()) {
      appState.bootAnimEnabled = doc["settings"]["bootAnimEnabled"].as<bool>();
      DebugOut.printf("Boot Animation: %s\n",
                      appState.bootAnimEnabled ? "enabled" : "disabled");
    }
    if (doc["settings"]["bootAnimStyle"].is<int>()) {
      int style = doc["settings"]["bootAnimStyle"].as<int>();
      if (style >= 0 && style <= 5) {
        appState.bootAnimStyle = style;
        DebugOut.printf("Boot Animation Style: %d\n", style);
      }
    }
#endif
    // Save general settings
    saveSettings();
  }

  // Import Smart Sensing settings
  if (!doc["smartSensing"].isNull()) {
    if (doc["smartSensing"]["mode"].is<String>()) {
      String modeStr = doc["smartSensing"]["mode"].as<String>();
      if (modeStr == "always_on") {
        currentMode = ALWAYS_ON;
      } else if (modeStr == "always_off") {
        currentMode = ALWAYS_OFF;
      } else if (modeStr == "smart_auto") {
        currentMode = SMART_AUTO;
      }
      DebugOut.printf("Smart Sensing Mode: %s\n", modeStr.c_str());
    }
    if (doc["smartSensing"]["timerDuration"].is<int>() ||
        doc["smartSensing"]["timerDuration"].is<unsigned long>()) {
      timerDuration = doc["smartSensing"]["timerDuration"].as<unsigned long>();
      DebugOut.printf("Timer Duration: %lu minutes\n", timerDuration);
    }
    if (doc["smartSensing"]["voltageThreshold"].is<float>()) {
      voltageThreshold = doc["smartSensing"]["voltageThreshold"].as<float>();
      DebugOut.printf("Voltage Threshold: %.2fV\n", voltageThreshold);
    }
    // Save Smart Sensing settings
    saveSmartSensingSettings();
  }

  // Import MQTT settings
  if (!doc["mqtt"].isNull()) {
    if (doc["mqtt"]["enabled"].is<bool>()) {
      mqttEnabled = doc["mqtt"]["enabled"].as<bool>();
      DebugOut.printf("MQTT Enabled: %s\n", mqttEnabled ? "true" : "false");
    }
    if (doc["mqtt"]["broker"].is<String>()) {
      mqttBroker = doc["mqtt"]["broker"].as<String>();
      DebugOut.printf("MQTT Broker: %s\n", mqttBroker.c_str());
    }
    if (doc["mqtt"]["port"].is<int>()) {
      mqttPort = doc["mqtt"]["port"].as<int>();
      DebugOut.printf("MQTT Port: %d\n", mqttPort);
    }
    if (doc["mqtt"]["username"].is<String>()) {
      mqttUsername = doc["mqtt"]["username"].as<String>();
      DebugOut.println("MQTT username imported");
    }
    if (doc["mqtt"]["baseTopic"].is<String>()) {
      mqttBaseTopic = doc["mqtt"]["baseTopic"].as<String>();
      DebugOut.printf("MQTT Base Topic: %s\n", mqttBaseTopic.c_str());
    }
    if (doc["mqtt"]["haDiscovery"].is<bool>()) {
      mqttHADiscovery = doc["mqtt"]["haDiscovery"].as<bool>();
      DebugOut.printf("MQTT HA Discovery: %s\n",
                      mqttHADiscovery ? "enabled" : "disabled");
    }
    // Note: Password is not imported for security - user needs to re-enter it
    // Save MQTT settings
    saveMqttSettings();
  }

  // Note: Certificate import removed - now using Mozilla certificate bundle

  DebugOut.println("All settings imported successfully!");

  // Send success response
  server.send(200, "application/json",
              "{\"success\": true, \"message\": \"Settings imported "
              "successfully. Device will reboot in 3 seconds.\"}");

  // Schedule reboot after 3 seconds
  delay(100); // Give time for response to be sent
  DebugOut.println("Rebooting in 3 seconds...");
  delay(3000);
  ESP.restart();
}

void handleFactoryReset() {
  DebugOut.println("Factory reset requested via web interface");

  // Send success response before performing reset
  server.send(200, "application/json",
              "{\"success\": true, \"message\": \"Factory reset initiated\"}");

  // Give time for response to be sent
  delay(500);

  // Perform factory reset
  performFactoryReset();
}

void handleReboot() {
  DebugOut.println("Reboot requested via web interface");

  // Send success response before rebooting
  server.send(200, "application/json",
              "{\"success\": true, \"message\": \"Rebooting device\"}");

  // Give time for response to be sent
  delay(500);

  // Reboot the ESP32
  ESP.restart();
}

void handleDiagnostics() {
  DebugOut.println("Diagnostics export requested via web interface");

  JsonDocument doc;

  // ===== Timestamp =====
  time_t now = time(nullptr);
  struct tm timeinfo;
  char timestamp[32];
  if (getLocalTime(&timeinfo)) {
    snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02dT%02d:%02d:%02d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  } else {
    snprintf(timestamp, sizeof(timestamp), "unknown");
  }
  doc["timestamp"] = timestamp;
  doc["diagnosticsVersion"] = "1.0";

  // ===== Device Information =====
  JsonObject device = doc["device"].to<JsonObject>();
  device["manufacturer"] = MANUFACTURER_NAME;
  device["model"] = MANUFACTURER_MODEL;
  device["serialNumber"] = deviceSerialNumber;
  device["firmwareVersion"] = firmwareVer;
  device["mac"] = WiFi.macAddress();
  device["chipId"] = String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF), HEX);
  device["chipModel"] = ESP.getChipModel();
  device["chipRevision"] = ESP.getChipRevision();
  device["chipCores"] = ESP.getChipCores();
  device["cpuFreqMHz"] = ESP.getCpuFreqMHz();

  // ===== System Information =====
  JsonObject system = doc["system"].to<JsonObject>();
  system["uptimeSeconds"] = millis() / 1000;
  system["resetReason"] = getResetReasonString();
  system["sdkVersion"] = ESP.getSdkVersion();
  system["freeHeap"] = ESP.getFreeHeap();
  system["heapSize"] = ESP.getHeapSize();
  system["minFreeHeap"] = ESP.getMinFreeHeap();
  system["maxAllocHeap"] = ESP.getMaxAllocHeap();
  system["psramSize"] = ESP.getPsramSize();
  system["freePsram"] = ESP.getFreePsram();
  system["flashSize"] = ESP.getFlashChipSize();
  system["flashSpeed"] = ESP.getFlashChipSpeed();

  // ===== WiFi Status =====
  JsonObject wifi = doc["wifi"].to<JsonObject>();
  wifi["connected"] = (WiFi.status() == WL_CONNECTED);
  wifi["mode"] = isAPMode ? "ap" : "sta";
  wifi["ssid"] = WiFi.SSID();
  wifi["rssi"] = WiFi.RSSI();
  wifi["localIP"] = WiFi.localIP().toString();
  wifi["gateway"] = WiFi.gatewayIP().toString();
  wifi["subnetMask"] = WiFi.subnetMask().toString();
  wifi["dnsIP"] = WiFi.dnsIP().toString();
  wifi["hostname"] = WiFi.getHostname();
  wifi["apEnabled"] = apEnabled;
  wifi["apSSID"] = apSSID;
  wifi["apIP"] = WiFi.softAPIP().toString();
  wifi["apClients"] = WiFi.softAPgetStationNum();

  // Get saved networks count
  wifi["savedNetworksCount"] = getWiFiNetworkCount();

  // ===== Settings =====
  JsonObject settings = doc["settings"].to<JsonObject>();
  settings["autoUpdateEnabled"] = autoUpdateEnabled;
  settings["timezoneOffset"] = timezoneOffset;
  settings["dstOffset"] = dstOffset;
  settings["nightMode"] = nightMode;
  settings["enableCertValidation"] = enableCertValidation;
  settings["blinkingEnabled"] = blinkingEnabled;
  settings["hardwareStatsInterval"] = hardwareStatsInterval;
  settings["screenTimeout"] = appState.screenTimeout;
  settings["backlightOn"] = appState.backlightOn;
  settings["buzzerEnabled"] = appState.buzzerEnabled;
  settings["buzzerVolume"] = appState.buzzerVolume;

  // ===== Smart Sensing =====
  JsonObject sensing = doc["smartSensing"].to<JsonObject>();
  String modeStr;
  switch (currentMode) {
  case ALWAYS_ON:
    modeStr = "always_on";
    break;
  case ALWAYS_OFF:
    modeStr = "always_off";
    break;
  case SMART_AUTO:
    modeStr = "smart_auto";
    break;
  }
  sensing["mode"] = modeStr;
  sensing["amplifierState"] = amplifierState;
  sensing["timerDuration"] = timerDuration;
  sensing["timerRemaining"] = timerRemaining;
  sensing["voltageThreshold"] = voltageThreshold;
  sensing["lastVoltageReading"] = lastVoltageReading;
  sensing["lastVoltageDetection"] = lastVoltageDetection;

  // ===== MQTT Settings (password excluded) =====
  JsonObject mqtt = doc["mqtt"].to<JsonObject>();
  mqtt["enabled"] = mqttEnabled;
  mqtt["broker"] = mqttBroker;
  mqtt["port"] = mqttPort;
  mqtt["username"] = mqttUsername;
  mqtt["baseTopic"] = mqttBaseTopic;
  mqtt["haDiscovery"] = mqttHADiscovery;
  mqtt["connected"] = mqttConnected;

  // ===== OTA Status =====
  JsonObject ota = doc["ota"].to<JsonObject>();
  ota["updateAvailable"] = updateAvailable;
  ota["latestVersion"] = cachedLatestVersion;
  ota["inProgress"] = otaInProgress;
  ota["status"] = otaStatus;

  // ===== FSM State =====
  String fsmStateStr;
  switch (appState.fsmState) {
  case STATE_IDLE:
    fsmStateStr = "idle";
    break;
  case STATE_SIGNAL_DETECTED:
    fsmStateStr = "signal_detected";
    break;
  case STATE_AUTO_OFF_TIMER:
    fsmStateStr = "auto_off_timer";
    break;
  case STATE_WEB_CONFIG:
    fsmStateStr = "web_config";
    break;
  case STATE_OTA_UPDATE:
    fsmStateStr = "ota_update";
    break;
  case STATE_ERROR:
    fsmStateStr = "error";
    break;
  default:
    fsmStateStr = "unknown";
  }
  doc["fsmState"] = fsmStateStr;
  doc["ledState"] = ledState;

  // ===== Error State =====
  if (appState.hasError()) {
    JsonObject error = doc["error"].to<JsonObject>();
    error["code"] = appState.errorCode;
    error["message"] = appState.errorMessage;
  }

  // Generate JSON
  String json;
  serializeJsonPretty(doc, json);

  // Send as downloadable JSON file
  char filename[64];
  snprintf(filename, sizeof(filename),
           "attachment; filename=\"diagnostics-%s.json\"", timestamp);
  server.sendHeader("Content-Disposition", filename);
  server.send(200, "application/json", json);

  DebugOut.println("Diagnostics exported successfully");
}

// Note: Certificate HTTP API handlers removed - now using Mozilla certificate
// bundle The enableCertValidation setting still works to toggle between:
// - ENABLED: Uses Mozilla certificate bundle for validation
// - DISABLED: Insecure mode (no certificate validation)
