#include "settings_manager.h"
#include "config.h"
#include "app_state.h"
#include "wifi_manager.h"
#include "smart_sensing.h"
#include "mqtt_handler.h"
#include "ota_updater.h"
#include "debug_serial.h"
#include <WiFi.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

// Note: Certificate management removed - now using Mozilla certificate bundle
// via ESP32CertBundle library for automatic SSL validation of all public servers

// ===== Settings Persistence =====

bool loadSettings() {
  File file = SPIFFS.open("/settings.txt", "r");
  if (!file) {
    return false;
  }

  String line1 = file.readStringUntil('\n');
  String line2 = file.readStringUntil('\n');
  String line3 = file.readStringUntil('\n');
  String line4 = file.readStringUntil('\n');
  String line5 = file.readStringUntil('\n');
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
  
  // Load night mode (if available, otherwise default to false)
  if (line3.length() > 0) {
    line3.trim();
    nightMode = (line3.toInt() != 0);
  }
  
  // Load cert validation setting (if available, otherwise default to false)
  if (line4.length() > 0) {
    line4.trim();
    enableCertValidation = (line4.toInt() != 0);
  }
  
  // Load hardware stats interval (if available, otherwise default to 2000ms)
  if (line5.length() > 0) {
    line5.trim();
    unsigned long interval = line5.toInt();
    // Validate: only allow 1000, 2000, 3000, 5000, or 10000 ms
    if (interval == 1000 || interval == 2000 || interval == 3000 || interval == 5000 || interval == 10000) {
      hardwareStatsInterval = interval;
    }
  }
  
  return true;
}

void saveSettings() {
  File file = SPIFFS.open("/settings.txt", "w");
  if (!file) {
    DebugOut.println("Failed to open settings file for writing");
    return;
  }

  file.println(autoUpdateEnabled ? "1" : "0");
  file.println(timezoneOffset);
  file.println(nightMode ? "1" : "0");
  file.println(enableCertValidation ? "1" : "0");
  file.println(hardwareStatsInterval);
  file.close();
  DebugOut.println("Settings saved to SPIFFS");
}

// ===== Factory Reset =====

void performFactoryReset() {
  DebugOut.println("\n=== FACTORY RESET INITIATED ===");
  factoryResetInProgress = true;
  
  // Visual feedback: solid LED
  digitalWrite(LED_PIN, HIGH);
  
  // Format SPIFFS (erases all persistent data)
  DebugOut.println("Formatting SPIFFS...");
  if (SPIFFS.format()) {
    DebugOut.println("SPIFFS formatted successfully");
  } else {
    DebugOut.println("SPIFFS format failed");
  }
  
  // End SPIFFS
  SPIFFS.end();
  
  DebugOut.println("=== FACTORY RESET COMPLETE ===");
  DebugOut.println("Rebooting in 2 seconds...");
  
  delay(2000);
  ESP.restart();
}

// ===== Settings HTTP API Handlers =====

void handleSettingsGet() {
  JsonDocument doc;
  doc["success"] = true;
  doc["autoUpdateEnabled"] = autoUpdateEnabled;
  doc["timezoneOffset"] = timezoneOffset;
  doc["nightMode"] = nightMode;
  doc["enableCertValidation"] = enableCertValidation;
  doc["hardwareStatsInterval"] = hardwareStatsInterval / 1000;  // Send as seconds

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleSettingsUpdate() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"success\": false, \"message\": \"No data received\"}");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    server.send(400, "application/json", "{\"success\": false, \"message\": \"Invalid JSON\"}");
    return;
  }

  bool settingsChanged = false;
  
  if (doc["autoUpdateEnabled"].is<bool>()) {
    autoUpdateEnabled = doc["autoUpdateEnabled"].as<bool>();
    settingsChanged = true;
  }
  
  if (doc["timezoneOffset"].is<int>()) {
    int newOffset = doc["timezoneOffset"].as<int>();
    if (newOffset != timezoneOffset) {
      timezoneOffset = newOffset;
      settingsChanged = true;
      // Re-sync time with new timezone
      if (WiFi.status() == WL_CONNECTED) {
        syncTimeWithNTP();
      }
    }
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
      DebugOut.printf("Certificate validation %s\n", enableCertValidation ? "ENABLED" : "DISABLED");
    }
  }
  
  if (doc["hardwareStatsInterval"].is<int>()) {
    int newInterval = doc["hardwareStatsInterval"].as<int>();
    // Validate: only allow 1, 2, 3, 5, or 10 seconds
    if (newInterval == 1 || newInterval == 2 || newInterval == 3 || newInterval == 5 || newInterval == 10) {
      unsigned long newIntervalMs = newInterval * 1000UL;
      if (newIntervalMs != hardwareStatsInterval) {
        hardwareStatsInterval = newIntervalMs;
        settingsChanged = true;
        DebugOut.printf("Hardware stats interval set to %d seconds\n", newInterval);
      }
    }
  }
  
  if (settingsChanged) {
    saveSettings();
  }

  JsonDocument resp;
  resp["success"] = true;
  resp["autoUpdateEnabled"] = autoUpdateEnabled;
  resp["timezoneOffset"] = timezoneOffset;
  resp["nightMode"] = nightMode;
  resp["enableCertValidation"] = enableCertValidation;
  resp["hardwareStatsInterval"] = hardwareStatsInterval / 1000;
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
  doc["deviceInfo"]["chipId"] = String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF), HEX);
  
  // WiFi settings
  doc["wifi"]["ssid"] = wifiSSID;
  doc["wifi"]["password"] = wifiPassword;
  
  // AP settings
  doc["accessPoint"]["enabled"] = apEnabled;
  doc["accessPoint"]["ssid"] = apSSID;
  doc["accessPoint"]["password"] = apPassword;
  
  // General settings
  doc["settings"]["autoUpdateEnabled"] = autoUpdateEnabled;
  doc["settings"]["timezoneOffset"] = timezoneOffset;
  doc["settings"]["nightMode"] = nightMode;
  doc["settings"]["enableCertValidation"] = enableCertValidation;
  doc["settings"]["blinkingEnabled"] = blinkingEnabled;
  doc["settings"]["hardwareStatsInterval"] = hardwareStatsInterval / 1000;
  
  // Smart Sensing settings
  String modeStr;
  switch (currentMode) {
    case ALWAYS_ON: modeStr = "always_on"; break;
    case ALWAYS_OFF: modeStr = "always_off"; break;
    case SMART_AUTO: modeStr = "smart_auto"; break;
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
  server.sendHeader("Content-Disposition", "attachment; filename=\"device-settings.json\"");
  server.send(200, "application/json", json);
  
  DebugOut.println("Settings exported successfully");
}

void handleSettingsImport() {
  DebugOut.println("Settings import requested via web interface");
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"success\": false, \"message\": \"No data received\"}");
    return;
  }
  
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    DebugOut.printf("JSON parsing failed: %s\n", error.c_str());
    server.send(400, "application/json", "{\"success\": false, \"message\": \"Invalid JSON format\"}");
    return;
  }
  
  // Validate it's a settings export file
  if (doc["exportInfo"].isNull() || doc["settings"].isNull()) {
    server.send(400, "application/json", "{\"success\": false, \"message\": \"Invalid settings file format\"}");
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
    // Save WiFi credentials
    if (wifiSSID.length() > 0) {
      saveWiFiCredentials(wifiSSID.c_str(), wifiPassword.c_str());
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
  }
  
  // Import general settings
  if (!doc["settings"].isNull()) {
    if (doc["settings"]["autoUpdateEnabled"].is<bool>()) {
      autoUpdateEnabled = doc["settings"]["autoUpdateEnabled"].as<bool>();
      DebugOut.printf("Auto Update: %s\n", autoUpdateEnabled ? "enabled" : "disabled");
    }
    if (doc["settings"]["timezoneOffset"].is<int>()) {
      timezoneOffset = doc["settings"]["timezoneOffset"].as<int>();
      DebugOut.printf("Timezone Offset: %d\n", timezoneOffset);
    }
    if (doc["settings"]["nightMode"].is<bool>()) {
      nightMode = doc["settings"]["nightMode"].as<bool>();
      DebugOut.printf("Night Mode: %s\n", nightMode ? "enabled" : "disabled");
    }
    if (doc["settings"]["enableCertValidation"].is<bool>()) {
      enableCertValidation = doc["settings"]["enableCertValidation"].as<bool>();
      DebugOut.printf("Cert Validation: %s\n", enableCertValidation ? "enabled" : "disabled");
    }
    if (doc["settings"]["blinkingEnabled"].is<bool>()) {
      blinkingEnabled = doc["settings"]["blinkingEnabled"].as<bool>();
      DebugOut.printf("Blinking: %s\n", blinkingEnabled ? "enabled" : "disabled");
    }
    if (doc["settings"]["hardwareStatsInterval"].is<int>()) {
      int interval = doc["settings"]["hardwareStatsInterval"].as<int>();
      if (interval == 1 || interval == 2 || interval == 3 || interval == 5 || interval == 10) {
        hardwareStatsInterval = interval * 1000UL;
        DebugOut.printf("Hardware Stats Interval: %d seconds\n", interval);
      }
    }
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
    if (doc["smartSensing"]["timerDuration"].is<int>() || doc["smartSensing"]["timerDuration"].is<unsigned long>()) {
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
      DebugOut.printf("MQTT HA Discovery: %s\n", mqttHADiscovery ? "enabled" : "disabled");
    }
    // Note: Password is not imported for security - user needs to re-enter it
    // Save MQTT settings
    saveMqttSettings();
  }
  
  // Note: Certificate import removed - now using Mozilla certificate bundle
  
  DebugOut.println("All settings imported successfully!");
  
  // Send success response
  server.send(200, "application/json", "{\"success\": true, \"message\": \"Settings imported successfully. Device will reboot in 3 seconds.\"}");
  
  // Schedule reboot after 3 seconds
  delay(100); // Give time for response to be sent
  DebugOut.println("Rebooting in 3 seconds...");
  delay(3000);
  ESP.restart();
}

void handleFactoryReset() {
  DebugOut.println("Factory reset requested via web interface");
  
  // Send success response before performing reset
  server.send(200, "application/json", "{\"success\": true, \"message\": \"Factory reset initiated\"}");
  
  // Give time for response to be sent
  delay(500);
  
  // Perform factory reset
  performFactoryReset();
}

void handleReboot() {
  DebugOut.println("Reboot requested via web interface");
  
  // Send success response before rebooting
  server.send(200, "application/json", "{\"success\": true, \"message\": \"Rebooting device\"}");
  
  // Give time for response to be sent
  delay(500);
  
  // Reboot the ESP32
  ESP.restart();
}

// Note: Certificate HTTP API handlers removed - now using Mozilla certificate bundle
// The enableCertValidation setting still works to toggle between:
// - ENABLED: Uses Mozilla certificate bundle for validation
// - DISABLED: Insecure mode (no certificate validation)
