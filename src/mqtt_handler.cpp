#include "mqtt_handler.h"
#include "app_state.h"
#include "buzzer_handler.h"
#include "config.h"
#include "debug_serial.h"
#include "utils.h"
#include "websocket_handler.h"
#include <LittleFS.h>
#include <cmath>

// State tracking for hardware stats change detection
static unsigned long prevMqttUptime = 0;
static uint32_t prevMqttHeapFree = 0;
static float prevMqttCpuUsage = 0;
static float prevMqttTemperature = 0;

// State tracking for settings change detection
static bool prevMqttNightMode = false;
static bool prevMqttAutoUpdate = false;
static bool prevMqttCertValidation = true;

// External functions from other modules
extern void sendBlinkingState();
extern void sendLEDState();
extern void saveSmartSensingSettings();
extern void sendSmartSensingStateInternal();
extern void sendWiFiStatus();
extern void saveSettings();
extern void performFactoryReset();
extern void checkForFirmwareUpdate();
extern bool performOTAUpdate(String firmwareUrl);
extern void setAmplifierState(bool state);

// ===== MQTT Settings Functions =====

// Load MQTT settings from LittleFS
bool loadMqttSettings() {
  // Use create=true to avoid "no permits for creation" error log if file is
  // missing
  File file = LittleFS.open("/mqtt_config.txt", "r", true);
  if (!file || file.size() == 0) {
    if (file)
      file.close();
    return false;
  }

  String line1 = file.readStringUntil('\n'); // enabled
  String line2 = file.readStringUntil('\n'); // broker
  String line3 = file.readStringUntil('\n'); // port
  String line4 = file.readStringUntil('\n'); // username
  String line5 = file.readStringUntil('\n'); // password
  String line6 = file.readStringUntil('\n'); // base topic
  String line7 = file.readStringUntil('\n'); // HA discovery
  file.close();

  line1.trim();
  line2.trim();
  line3.trim();
  line4.trim();
  line5.trim();
  line6.trim();
  line7.trim();

  if (line1.length() > 0) {
    mqttEnabled = (line1.toInt() != 0);
  }

  if (line2.length() > 0) {
    mqttBroker = line2;
  }

  if (line3.length() > 0) {
    int port = line3.toInt();
    if (port > 0 && port <= 65535) {
      mqttPort = port;
    }
  }

  if (line4.length() > 0) {
    mqttUsername = line4;
  }

  if (line5.length() > 0) {
    mqttPassword = line5;
  }

  if (line6.length() > 0) {
    mqttBaseTopic = line6;
  }

  if (line7.length() > 0) {
    mqttHADiscovery = (line7.toInt() != 0);
  }

  LOG_I("[MQTT] Settings loaded - Enabled: %s, Broker: %s:%d", mqttEnabled ? "true" : "false", mqttBroker.c_str(), mqttPort);
  LOG_I("[MQTT] Base Topic: %s, HA Discovery: %s", mqttBaseTopic.c_str(), mqttHADiscovery ? "true" : "false");

  return true;
}

// Save MQTT settings to LittleFS
void saveMqttSettings() {
  File file = LittleFS.open("/mqtt_config.txt", "w");
  if (!file) {
    LOG_E("[MQTT] Failed to open settings file for writing");
    return;
  }

  file.println(mqttEnabled ? "1" : "0");
  file.println(mqttBroker);
  file.println(String(mqttPort));
  file.println(mqttUsername);
  file.println(mqttPassword);
  file.println(mqttBaseTopic);
  file.println(mqttHADiscovery ? "1" : "0");
  file.close();

  LOG_I("[MQTT] Settings saved to LittleFS");
}

// Get unique device ID for MQTT client ID and HA discovery
String getMqttDeviceId() {
  uint64_t chipId = ESP.getEfuseMac();
  uint16_t shortId = (uint16_t)(chipId & 0xFFFF);
  char idBuf[5];
  snprintf(idBuf, sizeof(idBuf), "%04X", shortId);
  return String("esp32_audio_") + idBuf;
}

// Get effective MQTT base topic (falls back to ALX/{serialNumber} if not
// configured)
String getEffectiveMqttBaseTopic() {
  if (mqttBaseTopic.length() > 0) {
    return mqttBaseTopic;
  }
  // Default: ALX/{deviceSerialNumber}
  return String("ALX/") + deviceSerialNumber;
}

// ===== MQTT Core Functions =====

// Subscribe to all command topics
void subscribeToMqttTopics() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();

  // Subscribe to command topics
  mqttClient.subscribe((base + "/led/blinking/set").c_str());
  mqttClient.subscribe((base + "/smartsensing/mode/set").c_str());
  mqttClient.subscribe((base + "/smartsensing/amplifier/set").c_str());
  mqttClient.subscribe((base + "/smartsensing/timer_duration/set").c_str());
  mqttClient.subscribe((base + "/smartsensing/audio_threshold/set").c_str());
  mqttClient.subscribe((base + "/ap/enabled/set").c_str());
  mqttClient.subscribe((base + "/settings/auto_update/set").c_str());
  mqttClient.subscribe((base + "/settings/night_mode/set").c_str());
  mqttClient.subscribe((base + "/settings/cert_validation/set").c_str());
  mqttClient.subscribe((base + "/settings/screen_timeout/set").c_str());
  mqttClient.subscribe((base + "/display/backlight/set").c_str());
  mqttClient.subscribe((base + "/display/brightness/set").c_str());
  mqttClient.subscribe((base + "/settings/buzzer/set").c_str());
  mqttClient.subscribe((base + "/settings/buzzer_volume/set").c_str());
  mqttClient.subscribe((base + "/system/reboot").c_str());
  mqttClient.subscribe((base + "/system/factory_reset").c_str());
  mqttClient.subscribe((base + "/system/check_update").c_str());
  mqttClient.subscribe((base + "/system/update/command").c_str());

  LOG_D("[MQTT] Subscribed to command topics");
}

// MQTT callback for incoming messages
void mqttCallback(char *topic, byte *payload, unsigned int length) {
  // Convert payload to string
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  message.trim();

  String topicStr = String(topic);
  String base = getEffectiveMqttBaseTopic();

  LOG_D("[MQTT] Received: %s = %s", topic, message.c_str());

  // Handle LED blinking control
  if (topicStr == base + "/led/blinking/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    if (blinkingEnabled != newState) {
      blinkingEnabled = newState;
      LOG_I("[MQTT] Blinking set to %s", blinkingEnabled ? "ON" : "OFF");
      sendBlinkingState();

      if (!blinkingEnabled) {
        ledState = false;
        digitalWrite(LED_PIN, LOW);
        sendLEDState();
      }
    }
    publishMqttBlinkingState();
  }
  // Handle Smart Sensing mode
  else if (topicStr == base + "/smartsensing/mode/set") {
    SensingMode newMode;
    bool validMode = true;

    if (message == "always_on") {
      newMode = ALWAYS_ON;
    } else if (message == "always_off") {
      newMode = ALWAYS_OFF;
    } else if (message == "smart_auto") {
      newMode = SMART_AUTO;
    } else {
      validMode = false;
      LOG_W("[MQTT] Invalid mode: %s", message.c_str());
    }

    if (validMode && currentMode != newMode) {
      currentMode = newMode;
      LOG_I("[MQTT] Mode set to %s", message.c_str());

      if (currentMode == SMART_AUTO) {
        timerRemaining = timerDuration * 60;
      }

      saveSmartSensingSettings();
      sendSmartSensingStateInternal();
    }
    publishMqttSmartSensingState();
  }
  // Handle amplifier control
  else if (topicStr == base + "/smartsensing/amplifier/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    setAmplifierState(newState);

    if (currentMode == SMART_AUTO) {
      if (newState) {
        timerRemaining = timerDuration * 60;
        lastTimerUpdate = millis();
      } else {
        timerRemaining = 0;
      }
    }
    publishMqttSmartSensingState();
  }
  // Handle timer duration
  else if (topicStr == base + "/smartsensing/timer_duration/set") {
    int duration = message.toInt();
    if (duration >= 1 && duration <= 60) {
      timerDuration = duration;

      if (currentMode == SMART_AUTO) {
        timerRemaining = timerDuration * 60;
        if (amplifierState) {
          lastTimerUpdate = millis();
        }
      }

      saveSmartSensingSettings();
      sendSmartSensingStateInternal();
      LOG_I("[MQTT] Timer duration set to %d minutes", duration);
    }
    publishMqttSmartSensingState();
  }
  // Handle audio threshold
  else if (topicStr == base + "/smartsensing/audio_threshold/set") {
    float threshold = message.toFloat();
    if (threshold >= -96.0 && threshold <= 0.0) {
      audioThreshold_dBFS = threshold;
      saveSmartSensingSettings();
      sendSmartSensingStateInternal();
      LOG_I("[MQTT] Audio threshold set to %+.0f dBFS", threshold);
    }
    publishMqttSmartSensingState();
  }
  // Handle AP toggle
  else if (topicStr == base + "/ap/enabled/set") {
    bool enabled = (message == "ON" || message == "1" || message == "true");
    apEnabled = enabled;

    if (enabled) {
      if (!isAPMode) {
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(apSSID.c_str(), apPassword);
        isAPMode = true;
        LOG_I("[MQTT] Access Point enabled");
      }
    } else {
      if (isAPMode && WiFi.status() == WL_CONNECTED) {
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        isAPMode = false;
        LOG_I("[MQTT] Access Point disabled");
      }
    }

    sendWiFiStatus();
    publishMqttWifiStatus();
  }
  // Handle auto-update setting
  else if (topicStr == base + "/settings/auto_update/set") {
    bool enabled = (message == "ON" || message == "1" || message == "true");
    if (autoUpdateEnabled != enabled) {
      autoUpdateEnabled = enabled;
      saveSettings();
      LOG_I("[MQTT] Auto-update set to %s", enabled ? "ON" : "OFF");
      sendWiFiStatus(); // Broadcast to web clients
    }
    publishMqttSystemStatus();
  }
  // Handle night mode setting
  else if (topicStr == base + "/settings/night_mode/set") {
    bool enabled = (message == "ON" || message == "1" || message == "true");
    if (nightMode != enabled) {
      nightMode = enabled;
      saveSettings();
      LOG_I("[MQTT] Night mode set to %s", enabled ? "ON" : "OFF");
      sendWiFiStatus(); // Night mode is part of WiFi status in web UI
    }
    publishMqttSystemStatus();
  }
  // Handle certificate validation setting
  else if (topicStr == base + "/settings/cert_validation/set") {
    bool enabled = (message == "ON" || message == "1" || message == "true");
    if (enableCertValidation != enabled) {
      enableCertValidation = enabled;
      saveSettings();
      LOG_I("[MQTT] Certificate validation set to %s", enabled ? "ON" : "OFF");
      sendWiFiStatus(); // Broadcast to web clients
    }
    publishMqttSystemStatus();
  }
  // Handle screen timeout setting
  else if (topicStr == base + "/settings/screen_timeout/set") {
    int timeoutSec = message.toInt();
    unsigned long timeoutMs = (unsigned long)timeoutSec * 1000UL;
    if (timeoutMs == 0 || timeoutMs == 30000 || timeoutMs == 60000 ||
        timeoutMs == 300000 || timeoutMs == 600000) {
      appState.setScreenTimeout(timeoutMs);
      saveSettings();
      LOG_I("[MQTT] Screen timeout set to %d seconds", timeoutSec);
      sendWiFiStatus();
    }
    publishMqttDisplayState();
  }
  // Handle backlight control
  else if (topicStr == base + "/display/backlight/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.setBacklightOn(newState);
    LOG_I("[MQTT] Backlight set to %s", newState ? "ON" : "OFF");
    publishMqttDisplayState();
  }
  // Handle brightness control
  else if (topicStr == base + "/display/brightness/set") {
    int bright = message.toInt();
    // Accept percentage (10-100) and convert to PWM
    if (bright >= 10 && bright <= 100) {
      uint8_t pwm = (uint8_t)(bright * 255 / 100);
      appState.setBacklightBrightness(pwm);
      saveSettings();
      LOG_I("[MQTT] Brightness set to %d%% (PWM %d)", bright, pwm);
      publishMqttDisplayState();
    }
  }
  // Handle buzzer enable/disable
  else if (topicStr == base + "/settings/buzzer/set") {
    bool enabled = (message == "ON" || message == "1" || message == "true");
    appState.setBuzzerEnabled(enabled);
    saveSettings();
    LOG_I("[MQTT] Buzzer set to %s", enabled ? "ON" : "OFF");
    publishMqttBuzzerState();
  }
  // Handle buzzer volume
  else if (topicStr == base + "/settings/buzzer_volume/set") {
    int vol = message.toInt();
    if (vol >= 0 && vol <= 2) {
      appState.setBuzzerVolume(vol);
      saveSettings();
      LOG_I("[MQTT] Buzzer volume set to %d", vol);
      publishMqttBuzzerState();
    }
  }
  // Handle reboot command
  else if (topicStr == base + "/system/reboot") {
    LOG_W("[MQTT] Reboot command received");
    buzzer_play_blocking(BUZZ_SHUTDOWN, 1200);
    ESP.restart();
  }
  // Handle factory reset command
  else if (topicStr == base + "/system/factory_reset") {
    LOG_W("[MQTT] Factory reset command received");
    delay(500);
    performFactoryReset();
  }
  // Handle update check command
  else if (topicStr == base + "/system/check_update") {
    LOG_I("[MQTT] Update check command received");
    checkForFirmwareUpdate();
    publishMqttSystemStatus();
    publishMqttUpdateState();
  }
  // Handle update install command (from HA Update entity)
  else if (topicStr == base + "/system/update/command") {
    if (message == "install") {
      LOG_I("[MQTT] Firmware install command received from Home Assistant");
      if (updateAvailable && cachedFirmwareUrl.length() > 0) {
        LOG_I("[MQTT] Starting OTA update...");
        // Publish in_progress state before starting
        publishMqttUpdateState();
        bool success = performOTAUpdate(cachedFirmwareUrl);
        if (success) {
          LOG_I("[MQTT] OTA update successful, rebooting...");
          delay(1000);
          ESP.restart();
        } else {
          LOG_E("[MQTT] OTA update failed");
          publishMqttUpdateState(); // Update state to show failure
        }
      } else {
        LOG_W("[MQTT] No update available or firmware URL missing");
      }
    }
  }
}

// Setup MQTT client
void setupMqtt() {
  if (!mqttEnabled || mqttBroker.length() == 0) {
    LOG_I("[MQTT] Disabled or no broker configured");
    return;
  }

  LOG_I("[MQTT] Setting up...");
  LOG_I("[MQTT] Broker: %s:%d", mqttBroker.c_str(), mqttPort);
  LOG_I("[MQTT] Base Topic: %s", mqttBaseTopic.c_str());
  LOG_I("[MQTT] HA Discovery: %s", mqttHADiscovery ? "enabled" : "disabled");

  mqttClient.setServer(mqttBroker.c_str(), mqttPort);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(1024); // Increase buffer for HA discovery payloads

  // Attempt initial connection
  mqttReconnect();
}

// Reconnect to MQTT broker with exponential backoff
void mqttReconnect() {
  if (!mqttEnabled || mqttBroker.length() == 0) {
    return;
  }

  if (mqttClient.connected()) {
    return;
  }

  // Respect backoff interval (exponential backoff on failures)
  unsigned long currentMillis = millis();
  if (currentMillis - lastMqttReconnect < appState.mqttBackoffDelay) {
    return;
  }
  lastMqttReconnect = currentMillis;

  LOG_I("[MQTT] Connecting to broker (backoff: %lums)...", appState.mqttBackoffDelay);

  String clientId = getMqttDeviceId();
  String lwt = mqttBaseTopic + "/status";

  bool connected = false;

  if (mqttUsername.length() > 0) {
    connected = mqttClient.connect(clientId.c_str(), mqttUsername.c_str(),
                                   mqttPassword.c_str(), lwt.c_str(), 0, true,
                                   "offline");
  } else {
    connected =
        mqttClient.connect(clientId.c_str(), lwt.c_str(), 0, true, "offline");
  }

  if (connected) {
    LOG_I("[MQTT] Connected to %s:%d", mqttBroker.c_str(), mqttPort);
    mqttConnected = true;
    appState.resetMqttBackoff(); // Reset backoff on successful connection

    // Publish online status
    mqttClient.publish(lwt.c_str(), "online", true);

    // Subscribe to command topics
    subscribeToMqttTopics();

    // Publish Home Assistant discovery configs if enabled
    if (mqttHADiscovery) {
      publishHADiscovery();
      LOG_I("[MQTT] Home Assistant discovery published");
    }

    // Publish initial state
    publishMqttState();
  } else {
    LOG_W("[MQTT] Connection failed (rc=%d)", mqttClient.state());
    mqttConnected = false;
    appState.increaseMqttBackoff(); // Increase backoff on failure
    LOG_W("[MQTT] Next retry in %lums", appState.mqttBackoffDelay);
  }
}

// MQTT loop - call from main loop()
void mqttLoop() {
  if (!mqttEnabled || mqttBroker.length() == 0) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (!mqttClient.connected()) {
    mqttConnected = false;
    mqttReconnect();
  }

  mqttClient.loop();

  // Periodic state publishing
  unsigned long currentMillis = millis();
  if (mqttClient.connected() &&
      (currentMillis - lastMqttPublish >= MQTT_PUBLISH_INTERVAL)) {
    lastMqttPublish = currentMillis;

    // Check for state changes and publish
    bool stateChanged =
        (ledState != prevMqttLedState) ||
        (blinkingEnabled != prevMqttBlinkingEnabled) ||
        (amplifierState != prevMqttAmplifierState) ||
        (currentMode != prevMqttSensingMode) ||
        (timerRemaining != prevMqttTimerRemaining) ||
        (fabs(audioLevel_dBFS - prevMqttAudioLevel) > 0.5f) ||
        (appState.backlightOn != prevMqttBacklightOn) ||
        (appState.screenTimeout != prevMqttScreenTimeout) ||
        (appState.buzzerEnabled != prevMqttBuzzerEnabled) ||
        (appState.buzzerVolume != prevMqttBuzzerVolume) ||
        (appState.backlightBrightness != prevMqttBrightness) ||
        (nightMode != prevMqttNightMode) ||
        (autoUpdateEnabled != prevMqttAutoUpdate) ||
        (enableCertValidation != prevMqttCertValidation);

    if (stateChanged) {
      publishMqttState();

      // Update tracked state
      prevMqttLedState = ledState;
      prevMqttBlinkingEnabled = blinkingEnabled;
      prevMqttAmplifierState = amplifierState;
      prevMqttSensingMode = currentMode;
      prevMqttTimerRemaining = timerRemaining;
      prevMqttAudioLevel = audioLevel_dBFS;
      prevMqttBacklightOn = appState.backlightOn;
      prevMqttScreenTimeout = appState.screenTimeout;
      prevMqttBuzzerEnabled = appState.buzzerEnabled;
      prevMqttBuzzerVolume = appState.buzzerVolume;
      prevMqttBrightness = appState.backlightBrightness;
      prevMqttNightMode = nightMode;
      prevMqttAutoUpdate = autoUpdateEnabled;
      prevMqttCertValidation = enableCertValidation;
    }
  }
}

// ===== MQTT State Publishing Functions =====

// Publish LED state
void publishMqttLedState() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();
  mqttClient.publish((base + "/led/state").c_str(), ledState ? "ON" : "OFF",
                     true);
}

// Publish blinking state
void publishMqttBlinkingState() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();
  mqttClient.publish((base + "/led/blinking").c_str(),
                     blinkingEnabled ? "ON" : "OFF", true);
}

// Publish Smart Sensing state
void publishMqttSmartSensingState() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();

  // Convert mode enum to string
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

  mqttClient.publish((base + "/smartsensing/mode").c_str(), modeStr.c_str(),
                     true);
  mqttClient.publish((base + "/smartsensing/amplifier").c_str(),
                     amplifierState ? "ON" : "OFF", true);
  mqttClient.publish((base + "/smartsensing/timer_duration").c_str(),
                     String(timerDuration).c_str(), true);
  mqttClient.publish((base + "/smartsensing/timer_remaining").c_str(),
                     String(timerRemaining).c_str(), true);
  mqttClient.publish((base + "/smartsensing/audio_level").c_str(),
                     String(audioLevel_dBFS, 1).c_str(), true);
  mqttClient.publish((base + "/smartsensing/audio_threshold").c_str(),
                     String(audioThreshold_dBFS, 1).c_str(), true);
  mqttClient.publish((base + "/smartsensing/signal_detected").c_str(),
                     (audioLevel_dBFS >= audioThreshold_dBFS) ? "ON" : "OFF",
                     true);

  // Last signal detection timestamp (seconds since boot, 0 if never detected)
  unsigned long lastDetectionSecs =
      lastSignalDetection > 0 ? lastSignalDetection / 1000 : 0;
  mqttClient.publish((base + "/smartsensing/last_detection_time").c_str(),
                     String(lastDetectionSecs).c_str(), true);
}

// Publish WiFi status
void publishMqttWifiStatus() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();

  bool connected = (WiFi.status() == WL_CONNECTED);
  mqttClient.publish((base + "/wifi/connected").c_str(),
                     connected ? "ON" : "OFF", true);

  if (connected) {
    int rssi = WiFi.RSSI();
    mqttClient.publish((base + "/wifi/rssi").c_str(), String(rssi).c_str(),
                       true);
    mqttClient.publish((base + "/wifi/signal_quality").c_str(),
                       String(rssiToQuality(rssi)).c_str(), true);
    mqttClient.publish((base + "/wifi/ip").c_str(),
                       WiFi.localIP().toString().c_str(), true);
    mqttClient.publish((base + "/wifi/ssid").c_str(), WiFi.SSID().c_str(),
                       true);
  }

  mqttClient.publish((base + "/ap/enabled").c_str(), apEnabled ? "ON" : "OFF",
                     true);

  if (isAPMode) {
    mqttClient.publish((base + "/ap/ip").c_str(),
                       WiFi.softAPIP().toString().c_str(), true);
    mqttClient.publish((base + "/ap/ssid").c_str(), apSSID.c_str(), true);
  }
}

// Publish system status
void publishMqttSystemStatus() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();

  // Device information
  mqttClient.publish((base + "/system/manufacturer").c_str(), MANUFACTURER_NAME,
                     true);
  mqttClient.publish((base + "/system/model").c_str(), MANUFACTURER_MODEL,
                     true);
  mqttClient.publish((base + "/system/serial_number").c_str(),
                     deviceSerialNumber.c_str(), true);
  mqttClient.publish((base + "/system/firmware").c_str(), firmwareVer, true);
  mqttClient.publish((base + "/system/update_available").c_str(),
                     updateAvailable ? "ON" : "OFF", true);

  if (cachedLatestVersion.length() > 0) {
    mqttClient.publish((base + "/system/latest_version").c_str(),
                       cachedLatestVersion.c_str(), true);
  }

  mqttClient.publish((base + "/settings/auto_update").c_str(),
                     autoUpdateEnabled ? "ON" : "OFF", true);
  mqttClient.publish((base + "/system/mac").c_str(), WiFi.macAddress().c_str(),
                     true);

  // Reset/boot reason
  mqttClient.publish((base + "/system/reset_reason").c_str(),
                     getResetReasonString().c_str(), true);

  // Additional settings
  mqttClient.publish((base + "/settings/timezone_offset").c_str(),
                     String(timezoneOffset).c_str(), true);
  mqttClient.publish((base + "/settings/night_mode").c_str(),
                     nightMode ? "ON" : "OFF", true);
  mqttClient.publish((base + "/settings/cert_validation").c_str(),
                     enableCertValidation ? "ON" : "OFF", true);
}

// Publish update state for Home Assistant Update entity
void publishMqttUpdateState() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();

  // Build JSON state for HA Update entity
  JsonDocument doc;
  doc["installed_version"] = firmwareVer;
  doc["latest_version"] =
      cachedLatestVersion.length() > 0 ? cachedLatestVersion : firmwareVer;
  doc["title"] = String(MANUFACTURER_MODEL) + " Firmware";
  doc["release_url"] = String("https://github.com/") + GITHUB_REPO_OWNER + "/" +
                       GITHUB_REPO_NAME + "/releases";
  doc["in_progress"] = otaInProgress;

  // Add release summary if update available
  if (updateAvailable && cachedLatestVersion.length() > 0) {
    doc["release_summary"] =
        "New firmware version " + cachedLatestVersion + " is available";
  }

  String json;
  serializeJson(doc, json);
  mqttClient.publish((base + "/system/update/state").c_str(), json.c_str(),
                     true);

  // Publish separate OTA progress topics for easier monitoring
  mqttClient.publish((base + "/system/update/in_progress").c_str(),
                     otaInProgress ? "ON" : "OFF", true);
  mqttClient.publish((base + "/system/update/progress").c_str(),
                     String(otaProgress).c_str(), true);
  mqttClient.publish((base + "/system/update/status").c_str(),
                     otaStatus.c_str(), true);

  if (otaStatusMessage.length() > 0) {
    mqttClient.publish((base + "/system/update/message").c_str(),
                       otaStatusMessage.c_str(), true);
  }

  if (otaTotalBytes > 0) {
    mqttClient.publish((base + "/system/update/bytes_downloaded").c_str(),
                       String(otaProgressBytes).c_str(), true);
    mqttClient.publish((base + "/system/update/bytes_total").c_str(),
                       String(otaTotalBytes).c_str(), true);
  }
}

// Publish hardware statistics
void publishMqttHardwareStats() {
  if (!mqttClient.connected())
    return;

  // Update CPU usage before reading
  updateCpuUsage();

  String base = getEffectiveMqttBaseTopic();

  // Memory - Internal Heap
  mqttClient.publish((base + "/hardware/heap_total").c_str(),
                     String(ESP.getHeapSize()).c_str(), true);
  mqttClient.publish((base + "/hardware/heap_free").c_str(),
                     String(ESP.getFreeHeap()).c_str(), true);
  mqttClient.publish((base + "/hardware/heap_min_free").c_str(),
                     String(ESP.getMinFreeHeap()).c_str(), true);
  mqttClient.publish((base + "/hardware/heap_max_block").c_str(),
                     String(ESP.getMaxAllocHeap()).c_str(), true);

  // Memory - PSRAM (if available)
  uint32_t psramSize = ESP.getPsramSize();
  if (psramSize > 0) {
    mqttClient.publish((base + "/hardware/psram_total").c_str(),
                       String(psramSize).c_str(), true);
    mqttClient.publish((base + "/hardware/psram_free").c_str(),
                       String(ESP.getFreePsram()).c_str(), true);
  }

  // CPU Information
  mqttClient.publish((base + "/hardware/cpu_freq").c_str(),
                     String(ESP.getCpuFreqMHz()).c_str(), true);
  mqttClient.publish((base + "/hardware/cpu_model").c_str(), ESP.getChipModel(),
                     true);
  mqttClient.publish((base + "/hardware/cpu_cores").c_str(),
                     String(ESP.getChipCores()).c_str(), true);

  // CPU Utilization
  float cpuCore0 = getCpuUsageCore0();
  float cpuCore1 = getCpuUsageCore1();
  float cpuTotal = (cpuCore0 + cpuCore1) / 2.0;
  mqttClient.publish((base + "/hardware/cpu_usage_core0").c_str(),
                     String(cpuCore0, 1).c_str(), true);
  mqttClient.publish((base + "/hardware/cpu_usage_core1").c_str(),
                     String(cpuCore1, 1).c_str(), true);
  mqttClient.publish((base + "/hardware/cpu_usage").c_str(),
                     String(cpuTotal, 1).c_str(), true);

  // Temperature
  float temp = temperatureRead();
  mqttClient.publish((base + "/hardware/temperature").c_str(),
                     String(temp, 1).c_str(), true);

  // Storage - Flash
  mqttClient.publish((base + "/hardware/flash_size").c_str(),
                     String(ESP.getFlashChipSize()).c_str(), true);
  mqttClient.publish((base + "/hardware/sketch_size").c_str(),
                     String(ESP.getSketchSize()).c_str(), true);
  mqttClient.publish((base + "/hardware/sketch_free").c_str(),
                     String(ESP.getFreeSketchSpace()).c_str(), true);

  // Storage - LittleFS
  mqttClient.publish((base + "/hardware/LittleFS_total").c_str(),
                     String(LittleFS.totalBytes()).c_str(), true);
  mqttClient.publish((base + "/hardware/LittleFS_used").c_str(),
                     String(LittleFS.usedBytes()).c_str(), true);

  // WiFi channel and AP clients
  mqttClient.publish((base + "/wifi/channel").c_str(),
                     String(WiFi.channel()).c_str(), true);
  mqttClient.publish((base + "/ap/clients").c_str(),
                     String(WiFi.softAPgetStationNum()).c_str(), true);

  // Uptime (in seconds for easier reading)
  unsigned long uptimeSeconds = millis() / 1000;
  mqttClient.publish((base + "/system/uptime").c_str(),
                     String(uptimeSeconds).c_str(), true);
}

// Publish buzzer state
void publishMqttBuzzerState() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();

  mqttClient.publish((base + "/settings/buzzer").c_str(),
                     appState.buzzerEnabled ? "ON" : "OFF", true);
  mqttClient.publish((base + "/settings/buzzer_volume").c_str(),
                     String(appState.buzzerVolume).c_str(), true);
}

// Publish display state (backlight + screen timeout)
void publishMqttDisplayState() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();

  mqttClient.publish((base + "/display/backlight").c_str(),
                     appState.backlightOn ? "ON" : "OFF", true);
  mqttClient.publish((base + "/settings/screen_timeout").c_str(),
                     String(appState.screenTimeout / 1000).c_str(), true);

  // Publish brightness as percentage (0-100)
  int brightPct = (int)appState.backlightBrightness * 100 / 255;
  mqttClient.publish((base + "/display/brightness").c_str(),
                     String(brightPct).c_str(), true);
}

// Publish all states
void publishMqttState() {
  publishMqttLedState();
  publishMqttBlinkingState();
  publishMqttSmartSensingState();
  publishMqttWifiStatus();
  publishMqttSystemStatus();
  publishMqttUpdateState();
  publishMqttHardwareStats();
  publishMqttDisplayState();
  publishMqttBuzzerState();
}

// ===== Home Assistant Auto-Discovery =====

// Helper function to create device info JSON object
void addHADeviceInfo(JsonDocument &doc) {
  String deviceId = getMqttDeviceId();

  // Extract short ID for display name
  uint64_t chipId = ESP.getEfuseMac();
  uint16_t shortId = (uint16_t)(chipId & 0xFFFF);
  char idBuf[5];
  snprintf(idBuf, sizeof(idBuf), "%04X", shortId);

  JsonObject device = doc["device"].to<JsonObject>();
  JsonArray identifiers = device["identifiers"].to<JsonArray>();
  identifiers.add(deviceId);
  device["name"] = String(MANUFACTURER_MODEL) + " " + idBuf;
  device["model"] = MANUFACTURER_MODEL;
  device["manufacturer"] = MANUFACTURER_NAME;
  device["serial_number"] = deviceSerialNumber;
  device["sw_version"] = firmwareVer;
}

// Publish Home Assistant auto-discovery configuration
void publishHADiscovery() {
  if (!mqttClient.connected() || !mqttHADiscovery)
    return;

  LOG_I("[MQTT] Publishing Home Assistant discovery configs...");

  String deviceId = getMqttDeviceId();
  String base = getEffectiveMqttBaseTopic();

  // ===== LED Blinking Switch =====
  {
    JsonDocument doc;
    doc["name"] = "LED Blinking";
    doc["unique_id"] = deviceId + "_blinking";
    doc["state_topic"] = base + "/led/blinking";
    doc["command_topic"] = base + "/led/blinking/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:led-on";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/blinking/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Amplifier Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Amplifier";
    doc["unique_id"] = deviceId + "_amplifier";
    doc["state_topic"] = base + "/smartsensing/amplifier";
    doc["command_topic"] = base + "/smartsensing/amplifier/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:amplifier";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/amplifier/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== AP Mode Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Access Point";
    doc["unique_id"] = deviceId + "_ap";
    doc["state_topic"] = base + "/ap/enabled";
    doc["command_topic"] = base + "/ap/enabled/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:access-point";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/ap/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Smart Sensing Mode Select =====
  {
    JsonDocument doc;
    doc["name"] = "Smart Sensing Mode";
    doc["unique_id"] = deviceId + "_mode";
    doc["state_topic"] = base + "/smartsensing/mode";
    doc["command_topic"] = base + "/smartsensing/mode/set";
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("always_on");
    options.add("always_off");
    options.add("smart_auto");
    doc["icon"] = "mdi:auto-fix";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/select/" + deviceId + "/mode/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Timer Duration Number =====
  {
    JsonDocument doc;
    doc["name"] = "Timer Duration";
    doc["unique_id"] = deviceId + "_timer_duration";
    doc["state_topic"] = base + "/smartsensing/timer_duration";
    doc["command_topic"] = base + "/smartsensing/timer_duration/set";
    doc["min"] = 1;
    doc["max"] = 60;
    doc["step"] = 1;
    doc["unit_of_measurement"] = "min";
    doc["icon"] = "mdi:timer-outline";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/number/" + deviceId + "/timer_duration/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Audio Threshold Number =====
  {
    JsonDocument doc;
    doc["name"] = "Audio Threshold";
    doc["unique_id"] = deviceId + "_audio_threshold";
    doc["state_topic"] = base + "/smartsensing/audio_threshold";
    doc["command_topic"] = base + "/smartsensing/audio_threshold/set";
    doc["min"] = -96;
    doc["max"] = 0;
    doc["step"] = 1;
    doc["unit_of_measurement"] = "dBFS";
    doc["icon"] = "mdi:volume-vibrate";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/number/" + deviceId + "/audio_threshold/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Audio Level Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Audio Level";
    doc["unique_id"] = deviceId + "_audio_level";
    doc["state_topic"] = base + "/smartsensing/audio_level";
    doc["unit_of_measurement"] = "dBFS";
    doc["state_class"] = "measurement";
    doc["suggested_display_precision"] = 1;
    doc["icon"] = "mdi:volume-vibrate";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/audio_level/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Timer Remaining Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Timer Remaining";
    doc["unique_id"] = deviceId + "_timer_remaining";
    doc["state_topic"] = base + "/smartsensing/timer_remaining";
    doc["unit_of_measurement"] = "s";
    doc["icon"] = "mdi:timer-sand";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/sensor/" + deviceId + "/timer_remaining/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== WiFi RSSI Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "WiFi Signal";
    doc["unique_id"] = deviceId + "_rssi";
    doc["state_topic"] = base + "/wifi/rssi";
    doc["unit_of_measurement"] = "dBm";
    doc["device_class"] = "signal_strength";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:wifi";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/rssi/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== WiFi Connected Binary Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "WiFi Connected";
    doc["unique_id"] = deviceId + "_wifi_connected";
    doc["state_topic"] = base + "/wifi/connected";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["device_class"] = "connectivity";
    doc["entity_category"] = "diagnostic";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/binary_sensor/" + deviceId + "/wifi_connected/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Signal Detected Binary Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Detected";
    doc["unique_id"] = deviceId + "_signal_detected";
    doc["state_topic"] = base + "/smartsensing/signal_detected";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:sine-wave";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/binary_sensor/" + deviceId + "/signal_detected/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Update Available Binary Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Update Available";
    doc["unique_id"] = deviceId + "_update_available";
    doc["state_topic"] = base + "/system/update_available";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["device_class"] = "update";
    doc["entity_category"] = "diagnostic";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/binary_sensor/" + deviceId + "/update_available/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Firmware Version Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Firmware Version";
    doc["unique_id"] = deviceId + "_firmware";
    doc["state_topic"] = base + "/system/firmware";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:tag";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/firmware/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Latest Firmware Version Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Latest Firmware Version";
    doc["unique_id"] = deviceId + "_latest_firmware";
    doc["state_topic"] = base + "/system/latest_version";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:tag-arrow-up";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/sensor/" + deviceId + "/latest_firmware/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Reboot Button =====
  {
    JsonDocument doc;
    doc["name"] = "Reboot";
    doc["unique_id"] = deviceId + "_reboot";
    doc["command_topic"] = base + "/system/reboot";
    doc["payload_press"] = "REBOOT";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:restart";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/button/" + deviceId + "/reboot/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Check Update Button =====
  {
    JsonDocument doc;
    doc["name"] = "Check for Updates";
    doc["unique_id"] = deviceId + "_check_update";
    doc["command_topic"] = base + "/system/check_update";
    doc["payload_press"] = "CHECK";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:update";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/button/" + deviceId + "/check_update/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Auto-Update Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Auto Update";
    doc["unique_id"] = deviceId + "_auto_update";
    doc["state_topic"] = base + "/settings/auto_update";
    doc["command_topic"] = base + "/settings/auto_update/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:update";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/auto_update/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Firmware Update Entity =====
  // This provides the native HA Update entity with install capability
  {
    JsonDocument doc;
    doc["name"] = "Firmware";
    doc["unique_id"] = deviceId + "_firmware_update";
    doc["device_class"] = "firmware";
    doc["state_topic"] = base + "/system/update/state";
    doc["command_topic"] = base + "/system/update/command";
    doc["payload_install"] = "install";
    doc["entity_picture"] =
        "https://brands.home-assistant.io/_/esphome/icon.png";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/update/" + deviceId + "/firmware/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== IP Address Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "IP Address";
    doc["unique_id"] = deviceId + "_ip";
    doc["state_topic"] = base + "/wifi/ip";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:ip-network";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/ip/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Hardware Diagnostics =====

  // ===== CPU Temperature Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "CPU Temperature";
    doc["unique_id"] = deviceId + "_cpu_temp";
    doc["state_topic"] = base + "/hardware/temperature";
    doc["unit_of_measurement"] = "°C";
    doc["device_class"] = "temperature";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:thermometer";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/cpu_temp/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== CPU Usage Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "CPU Usage";
    doc["unique_id"] = deviceId + "_cpu_usage";
    doc["state_topic"] = base + "/hardware/cpu_usage";
    doc["unit_of_measurement"] = "%";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:cpu-64-bit";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/cpu_usage/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Free Heap Memory Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Free Heap Memory";
    doc["unique_id"] = deviceId + "_heap_free";
    doc["state_topic"] = base + "/hardware/heap_free";
    doc["unit_of_measurement"] = "B";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:memory";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/heap_free/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Uptime Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Uptime";
    doc["unique_id"] = deviceId + "_uptime";
    doc["state_topic"] = base + "/system/uptime";
    doc["unit_of_measurement"] = "s";
    doc["device_class"] = "duration";
    doc["state_class"] = "total_increasing";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:clock-outline";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/uptime/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== LittleFS Used Storage Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "LittleFS Used";
    doc["unique_id"] = deviceId + "_LittleFS_used";
    doc["state_topic"] = base + "/hardware/LittleFS_used";
    doc["unit_of_measurement"] = "B";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:harddisk";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/LittleFS_used/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== WiFi Channel Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "WiFi Channel";
    doc["unique_id"] = deviceId + "_wifi_channel";
    doc["state_topic"] = base + "/wifi/channel";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:wifi";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/wifi_channel/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Night Mode Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Night Mode";
    doc["unique_id"] = deviceId + "_night_mode";
    doc["state_topic"] = base + "/settings/night_mode";
    doc["command_topic"] = base + "/settings/night_mode/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:weather-night";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/night_mode/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Certificate Validation Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Certificate Validation";
    doc["unique_id"] = deviceId + "_cert_validation";
    doc["state_topic"] = base + "/settings/cert_validation";
    doc["command_topic"] = base + "/settings/cert_validation/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:certificate";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/switch/" + deviceId + "/cert_validation/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Display Backlight Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Display Backlight";
    doc["unique_id"] = deviceId + "_backlight";
    doc["state_topic"] = base + "/display/backlight";
    doc["command_topic"] = base + "/display/backlight/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:brightness-6";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/backlight/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Screen Timeout Number =====
  {
    JsonDocument doc;
    doc["name"] = "Screen Timeout";
    doc["unique_id"] = deviceId + "_screen_timeout";
    doc["state_topic"] = base + "/settings/screen_timeout";
    doc["command_topic"] = base + "/settings/screen_timeout/set";
    doc["min"] = 0;
    doc["max"] = 600;
    doc["step"] = 30;
    doc["unit_of_measurement"] = "s";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:timer-off-outline";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/number/" + deviceId + "/screen_timeout/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Display Brightness Number =====
  {
    JsonDocument doc;
    doc["name"] = "Display Brightness";
    doc["unique_id"] = deviceId + "_brightness";
    doc["state_topic"] = base + "/display/brightness";
    doc["command_topic"] = base + "/display/brightness/set";
    doc["min"] = 10;
    doc["max"] = 100;
    doc["step"] = 25;
    doc["unit_of_measurement"] = "%";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:brightness-percent";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/number/" + deviceId + "/brightness/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Buzzer Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Buzzer";
    doc["unique_id"] = deviceId + "_buzzer";
    doc["state_topic"] = base + "/settings/buzzer";
    doc["command_topic"] = base + "/settings/buzzer/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:volume-high";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/buzzer/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Buzzer Volume Number =====
  {
    JsonDocument doc;
    doc["name"] = "Buzzer Volume";
    doc["unique_id"] = deviceId + "_buzzer_volume";
    doc["state_topic"] = base + "/settings/buzzer_volume";
    doc["command_topic"] = base + "/settings/buzzer_volume/set";
    doc["min"] = 0;
    doc["max"] = 2;
    doc["step"] = 1;
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:volume-medium";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/number/" + deviceId + "/buzzer_volume/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  LOG_I("[MQTT] Home Assistant discovery configs published");
}

// Remove Home Assistant auto-discovery configuration
void removeHADiscovery() {
  if (!mqttClient.connected())
    return;

  LOG_I("[MQTT] Removing Home Assistant discovery configs...");

  String deviceId = getMqttDeviceId();

  // List of all discovery topics to remove
  const char *topics[] = {
      "homeassistant/switch/%s/blinking/config",
      "homeassistant/switch/%s/amplifier/config",
      "homeassistant/switch/%s/ap/config",
      "homeassistant/switch/%s/auto_update/config",
      "homeassistant/switch/%s/night_mode/config",
      "homeassistant/switch/%s/cert_validation/config",
      "homeassistant/select/%s/mode/config",
      "homeassistant/number/%s/timer_duration/config",
      "homeassistant/number/%s/audio_threshold/config",
      "homeassistant/sensor/%s/audio_level/config",
      "homeassistant/sensor/%s/timer_remaining/config",
      "homeassistant/sensor/%s/rssi/config",
      "homeassistant/sensor/%s/firmware/config",
      "homeassistant/sensor/%s/latest_firmware/config",
      "homeassistant/sensor/%s/ip/config",
      "homeassistant/sensor/%s/cpu_temp/config",
      "homeassistant/sensor/%s/cpu_usage/config",
      "homeassistant/sensor/%s/heap_free/config",
      "homeassistant/sensor/%s/uptime/config",
      "homeassistant/sensor/%s/LittleFS_used/config",
      "homeassistant/sensor/%s/wifi_channel/config",
      "homeassistant/binary_sensor/%s/wifi_connected/config",
      "homeassistant/binary_sensor/%s/signal_detected/config",
      "homeassistant/binary_sensor/%s/update_available/config",
      "homeassistant/button/%s/reboot/config",
      "homeassistant/button/%s/check_update/config",
      "homeassistant/update/%s/firmware/config",
      "homeassistant/switch/%s/backlight/config",
      "homeassistant/number/%s/screen_timeout/config",
      "homeassistant/number/%s/brightness/config",
      "homeassistant/switch/%s/buzzer/config",
      "homeassistant/number/%s/buzzer_volume/config"};

  char topicBuf[128];
  for (const char *topicTemplate : topics) {
    snprintf(topicBuf, sizeof(topicBuf), topicTemplate, deviceId.c_str());
    mqttClient.publish(topicBuf, "", true); // Empty payload removes the config
  }

  LOG_I("[MQTT] Home Assistant discovery configs removed");
}

// ===== MQTT HTTP API Handlers =====

// GET /api/mqtt - Get MQTT settings and status
void handleMqttGet() {
  JsonDocument doc;
  doc["success"] = true;

  // Settings
  doc["enabled"] = mqttEnabled;
  doc["broker"] = mqttBroker;
  doc["port"] = mqttPort;
  doc["username"] = mqttUsername;
  // Don't send password for security, just indicate if set
  doc["hasPassword"] = (mqttPassword.length() > 0);
  doc["baseTopic"] = mqttBaseTopic;
  doc["effectiveBaseTopic"] = getEffectiveMqttBaseTopic();
  doc["defaultBaseTopic"] = String("ALX/") + deviceSerialNumber;
  doc["haDiscovery"] = mqttHADiscovery;

  // Status
  doc["connected"] = mqttConnected;
  doc["deviceId"] = getMqttDeviceId();

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// POST /api/mqtt - Update MQTT settings
void handleMqttUpdate() {
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
  bool needReconnect = false;
  bool prevHADiscovery = mqttHADiscovery;

  // Update enabled state
  if (doc["enabled"].is<bool>()) {
    bool newEnabled = doc["enabled"].as<bool>();
    if (mqttEnabled != newEnabled) {
      mqttEnabled = newEnabled;
      settingsChanged = true;
      needReconnect = true;

      if (!mqttEnabled && mqttClient.connected()) {
        // Remove HA discovery before disconnecting
        if (mqttHADiscovery) {
          removeHADiscovery();
        }
        mqttClient.disconnect();
        mqttConnected = false;
      }
    }
  }

  // Update broker
  if (doc["broker"].is<String>()) {
    String newBroker = doc["broker"].as<String>();
    if (mqttBroker != newBroker) {
      mqttBroker = newBroker;
      settingsChanged = true;
      needReconnect = true;
    }
  }

  // Update port
  if (doc["port"].is<int>()) {
    int newPort = doc["port"].as<int>();
    if (newPort > 0 && newPort <= 65535 && mqttPort != newPort) {
      mqttPort = newPort;
      settingsChanged = true;
      needReconnect = true;
    }
  }

  // Update username
  if (doc["username"].is<String>()) {
    String newUsername = doc["username"].as<String>();
    if (mqttUsername != newUsername) {
      mqttUsername = newUsername;
      settingsChanged = true;
      needReconnect = true;
    }
  }

  // Update password (empty string keeps existing, like WiFi pattern)
  if (!doc["password"].isNull()) {
    String newPassword = doc["password"].as<String>();
    if (newPassword.length() > 0 && mqttPassword != newPassword) {
      mqttPassword = newPassword;
      settingsChanged = true;
      needReconnect = true;
    }
  }

  // Update base topic (empty string uses default ALX/{serial})
  if (!doc["baseTopic"].isNull()) {
    String newBaseTopic = doc["baseTopic"].as<String>();
    if (mqttBaseTopic != newBaseTopic) {
      // Remove old HA discovery before changing topic
      if (mqttHADiscovery && mqttClient.connected()) {
        removeHADiscovery();
      }
      mqttBaseTopic = newBaseTopic;
      settingsChanged = true;
      needReconnect = true;
      LOG_I("[MQTT] Base topic changed to: %s", newBaseTopic.length() > 0 ? newBaseTopic.c_str() : "(default)");
    }
  }

  // Update HA Discovery
  if (doc["haDiscovery"].is<bool>()) {
    bool newHADiscovery = doc["haDiscovery"].as<bool>();
    if (mqttHADiscovery != newHADiscovery) {
      // If disabling HA discovery, remove existing configs
      if (!newHADiscovery && mqttClient.connected()) {
        removeHADiscovery();
      }
      mqttHADiscovery = newHADiscovery;
      settingsChanged = true;

      // If enabling HA discovery and connected, publish configs
      if (newHADiscovery && mqttClient.connected()) {
        publishHADiscovery();
      }
    }
  }

  // Save settings if changed
  if (settingsChanged) {
    saveMqttSettings();
    LOG_I("[MQTT] Settings updated");
  }

  // Reconnect if needed
  if (needReconnect && mqttEnabled && mqttBroker.length() > 0) {
    if (mqttClient.connected()) {
      mqttClient.disconnect();
    }
    mqttConnected = false;
    lastMqttReconnect = 0; // Force immediate reconnect attempt
    setupMqtt();
  }

  // Send response
  JsonDocument resp;
  resp["success"] = true;
  resp["connected"] = mqttConnected;
  resp["message"] = settingsChanged ? "Settings updated" : "No changes";

  String json;
  serializeJson(resp, json);
  server.send(200, "application/json", json);
}
