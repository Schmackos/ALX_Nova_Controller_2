#include "mqtt_handler.h"
#include "app_state.h"
#include "config.h"
#include "websocket_handler.h"
#include "debug_serial.h"
#include <SPIFFS.h>

// External functions from other modules
extern void sendBlinkingState();
extern void sendLEDState();
extern void saveSmartSensingSettings();
extern void sendSmartSensingStateInternal();
extern void sendWiFiStatus();
extern void saveSettings();
extern void performFactoryReset();
extern void checkForFirmwareUpdate();
extern void setAmplifierState(bool state);

// ===== MQTT Settings Functions =====

// Load MQTT settings from SPIFFS
bool loadMqttSettings() {
  File file = SPIFFS.open("/mqtt_config.txt", "r");
  if (!file) {
    return false;
  }
  
  String line1 = file.readStringUntil('\n');  // enabled
  String line2 = file.readStringUntil('\n');  // broker
  String line3 = file.readStringUntil('\n');  // port
  String line4 = file.readStringUntil('\n');  // username
  String line5 = file.readStringUntil('\n');  // password
  String line6 = file.readStringUntil('\n');  // base topic
  String line7 = file.readStringUntil('\n');  // HA discovery
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
  
  DebugOut.println("MQTT settings loaded from SPIFFS");
  DebugOut.printf("  Enabled: %s, Broker: %s:%d\n", 
                mqttEnabled ? "true" : "false", mqttBroker.c_str(), mqttPort);
  DebugOut.printf("  Base Topic: %s, HA Discovery: %s\n", 
                mqttBaseTopic.c_str(), mqttHADiscovery ? "true" : "false");
  
  return true;
}

// Save MQTT settings to SPIFFS
void saveMqttSettings() {
  File file = SPIFFS.open("/mqtt_config.txt", "w");
  if (!file) {
    DebugOut.println("Failed to open MQTT settings file for writing");
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
  
  DebugOut.println("MQTT settings saved to SPIFFS");
}

// Get unique device ID for MQTT client ID and HA discovery
String getMqttDeviceId() {
  uint64_t chipId = ESP.getEfuseMac();
  uint16_t shortId = (uint16_t)(chipId & 0xFFFF);
  char idBuf[5];
  snprintf(idBuf, sizeof(idBuf), "%04X", shortId);
  return String("esp32_audio_") + idBuf;
}

// ===== MQTT Core Functions =====

// Subscribe to all command topics
void subscribeToMqttTopics() {
  if (!mqttClient.connected()) return;
  
  String base = mqttBaseTopic;
  
  // Subscribe to command topics
  mqttClient.subscribe((base + "/led/blinking/set").c_str());
  mqttClient.subscribe((base + "/smartsensing/mode/set").c_str());
  mqttClient.subscribe((base + "/smartsensing/amplifier/set").c_str());
  mqttClient.subscribe((base + "/smartsensing/timer_duration/set").c_str());
  mqttClient.subscribe((base + "/smartsensing/voltage_threshold/set").c_str());
  mqttClient.subscribe((base + "/ap/enabled/set").c_str());
  mqttClient.subscribe((base + "/settings/auto_update/set").c_str());
  mqttClient.subscribe((base + "/system/reboot").c_str());
  mqttClient.subscribe((base + "/system/factory_reset").c_str());
  mqttClient.subscribe((base + "/system/check_update").c_str());
  
  DebugOut.println("MQTT: Subscribed to command topics");
}

// MQTT callback for incoming messages
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Convert payload to string
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  message.trim();
  
  String topicStr = String(topic);
  String base = mqttBaseTopic;
  
  DebugOut.printf("MQTT received: %s = %s\n", topic, message.c_str());
  
  // Handle LED blinking control
  if (topicStr == base + "/led/blinking/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    if (blinkingEnabled != newState) {
      blinkingEnabled = newState;
      DebugOut.printf("MQTT: Blinking set to %s\n", blinkingEnabled ? "ON" : "OFF");
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
      DebugOut.printf("MQTT: Invalid mode: %s\n", message.c_str());
    }
    
    if (validMode && currentMode != newMode) {
      currentMode = newMode;
      DebugOut.printf("MQTT: Mode set to %s\n", message.c_str());
      
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
      DebugOut.printf("MQTT: Timer duration set to %d minutes\n", duration);
    }
    publishMqttSmartSensingState();
  }
  // Handle voltage threshold
  else if (topicStr == base + "/smartsensing/voltage_threshold/set") {
    float threshold = message.toFloat();
    if (threshold >= 0.1 && threshold <= 3.3) {
      voltageThreshold = threshold;
      saveSmartSensingSettings();
      sendSmartSensingStateInternal();
      DebugOut.printf("MQTT: Voltage threshold set to %.2fV\n", threshold);
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
        DebugOut.println("MQTT: Access Point enabled");
      }
    } else {
      if (isAPMode && WiFi.status() == WL_CONNECTED) {
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        isAPMode = false;
        DebugOut.println("MQTT: Access Point disabled");
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
      DebugOut.printf("MQTT: Auto-update set to %s\n", enabled ? "ON" : "OFF");
    }
    publishMqttSystemStatus();
  }
  // Handle reboot command
  else if (topicStr == base + "/system/reboot") {
    DebugOut.println("MQTT: Reboot command received");
    delay(500);
    ESP.restart();
  }
  // Handle factory reset command
  else if (topicStr == base + "/system/factory_reset") {
    DebugOut.println("MQTT: Factory reset command received");
    delay(500);
    performFactoryReset();
  }
  // Handle update check command
  else if (topicStr == base + "/system/check_update") {
    DebugOut.println("MQTT: Update check command received");
    checkForFirmwareUpdate();
    publishMqttSystemStatus();
  }
}

// Setup MQTT client
void setupMqtt() {
  if (!mqttEnabled || mqttBroker.length() == 0) {
    DebugOut.println("MQTT: Disabled or no broker configured");
    return;
  }
  
  DebugOut.println("\n=== Setting up MQTT ===");
  DebugOut.printf("Broker: %s:%d\n", mqttBroker.c_str(), mqttPort);
  DebugOut.printf("Base Topic: %s\n", mqttBaseTopic.c_str());
  DebugOut.printf("HA Discovery: %s\n", mqttHADiscovery ? "enabled" : "disabled");
  
  mqttClient.setServer(mqttBroker.c_str(), mqttPort);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(1024);  // Increase buffer for HA discovery payloads
  
  // Attempt initial connection
  mqttReconnect();
}

// Reconnect to MQTT broker
void mqttReconnect() {
  if (!mqttEnabled || mqttBroker.length() == 0) {
    return;
  }
  
  if (mqttClient.connected()) {
    return;
  }
  
  // Respect reconnect interval
  unsigned long currentMillis = millis();
  if (currentMillis - lastMqttReconnect < MQTT_RECONNECT_INTERVAL) {
    return;
  }
  lastMqttReconnect = currentMillis;
  
  DebugOut.print("MQTT: Connecting to broker...");
  
  String clientId = getMqttDeviceId();
  String lwt = mqttBaseTopic + "/status";
  
  bool connected = false;
  
  if (mqttUsername.length() > 0) {
    connected = mqttClient.connect(
      clientId.c_str(),
      mqttUsername.c_str(),
      mqttPassword.c_str(),
      lwt.c_str(),
      0,
      true,
      "offline"
    );
  } else {
    connected = mqttClient.connect(
      clientId.c_str(),
      lwt.c_str(),
      0,
      true,
      "offline"
    );
  }
  
  if (connected) {
    DebugOut.printf(" connected to %s:%d!\n", mqttBroker.c_str(), mqttPort);
    mqttConnected = true;
    
    // Publish online status
    mqttClient.publish(lwt.c_str(), "online", true);
    
    // Subscribe to command topics
    subscribeToMqttTopics();
    
    // Publish Home Assistant discovery configs if enabled
    if (mqttHADiscovery) {
      publishHADiscovery();
      DebugOut.println("MQTT: Home Assistant discovery published");
    }
    
    // Publish initial state
    publishMqttState();
  } else {
    DebugOut.printf(" failed (rc=%d)\n", mqttClient.state());
    mqttConnected = false;
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
  if (mqttClient.connected() && (currentMillis - lastMqttPublish >= MQTT_PUBLISH_INTERVAL)) {
    lastMqttPublish = currentMillis;
    
    // Check for state changes and publish
    bool stateChanged = (ledState != prevMqttLedState) ||
                        (blinkingEnabled != prevMqttBlinkingEnabled) ||
                        (amplifierState != prevMqttAmplifierState) ||
                        (currentMode != prevMqttSensingMode) ||
                        (timerRemaining != prevMqttTimerRemaining) ||
                        (abs(lastVoltageReading - prevMqttVoltageReading) > 0.05);
    
    if (stateChanged) {
      publishMqttState();
      
      // Update tracked state
      prevMqttLedState = ledState;
      prevMqttBlinkingEnabled = blinkingEnabled;
      prevMqttAmplifierState = amplifierState;
      prevMqttSensingMode = currentMode;
      prevMqttTimerRemaining = timerRemaining;
      prevMqttVoltageReading = lastVoltageReading;
    }
  }
}

// ===== MQTT State Publishing Functions =====

// Publish LED state
void publishMqttLedState() {
  if (!mqttClient.connected()) return;
  
  String base = mqttBaseTopic;
  mqttClient.publish((base + "/led/state").c_str(), ledState ? "ON" : "OFF", true);
}

// Publish blinking state
void publishMqttBlinkingState() {
  if (!mqttClient.connected()) return;
  
  String base = mqttBaseTopic;
  mqttClient.publish((base + "/led/blinking").c_str(), blinkingEnabled ? "ON" : "OFF", true);
}

// Publish Smart Sensing state
void publishMqttSmartSensingState() {
  if (!mqttClient.connected()) return;
  
  String base = mqttBaseTopic;
  
  // Convert mode enum to string
  String modeStr;
  switch (currentMode) {
    case ALWAYS_ON: modeStr = "always_on"; break;
    case ALWAYS_OFF: modeStr = "always_off"; break;
    case SMART_AUTO: modeStr = "smart_auto"; break;
  }
  
  mqttClient.publish((base + "/smartsensing/mode").c_str(), modeStr.c_str(), true);
  mqttClient.publish((base + "/smartsensing/amplifier").c_str(), amplifierState ? "ON" : "OFF", true);
  mqttClient.publish((base + "/smartsensing/timer_duration").c_str(), String(timerDuration).c_str(), true);
  mqttClient.publish((base + "/smartsensing/timer_remaining").c_str(), String(timerRemaining).c_str(), true);
  mqttClient.publish((base + "/smartsensing/voltage").c_str(), String(lastVoltageReading, 2).c_str(), true);
  mqttClient.publish((base + "/smartsensing/voltage_threshold").c_str(), String(voltageThreshold, 2).c_str(), true);
  mqttClient.publish((base + "/smartsensing/voltage_detected").c_str(), 
                     (lastVoltageReading >= voltageThreshold) ? "ON" : "OFF", true);
}

// Publish WiFi status
void publishMqttWifiStatus() {
  if (!mqttClient.connected()) return;
  
  String base = mqttBaseTopic;
  
  bool connected = (WiFi.status() == WL_CONNECTED);
  mqttClient.publish((base + "/wifi/connected").c_str(), connected ? "ON" : "OFF", true);
  
  if (connected) {
    mqttClient.publish((base + "/wifi/rssi").c_str(), String(WiFi.RSSI()).c_str(), true);
    mqttClient.publish((base + "/wifi/ip").c_str(), WiFi.localIP().toString().c_str(), true);
    mqttClient.publish((base + "/wifi/ssid").c_str(), WiFi.SSID().c_str(), true);
  }
  
  mqttClient.publish((base + "/ap/enabled").c_str(), apEnabled ? "ON" : "OFF", true);
  
  if (isAPMode) {
    mqttClient.publish((base + "/ap/ip").c_str(), WiFi.softAPIP().toString().c_str(), true);
    mqttClient.publish((base + "/ap/ssid").c_str(), apSSID.c_str(), true);
  }
}

// Publish system status
void publishMqttSystemStatus() {
  if (!mqttClient.connected()) return;
  
  String base = mqttBaseTopic;
  
  // Device information
  mqttClient.publish((base + "/system/manufacturer").c_str(), MANUFACTURER_NAME, true);
  mqttClient.publish((base + "/system/model").c_str(), MANUFACTURER_MODEL, true);
  mqttClient.publish((base + "/system/serial_number").c_str(), deviceSerialNumber.c_str(), true);
  mqttClient.publish((base + "/system/firmware").c_str(), firmwareVer, true);
  mqttClient.publish((base + "/system/update_available").c_str(), updateAvailable ? "ON" : "OFF", true);
  
  if (cachedLatestVersion.length() > 0) {
    mqttClient.publish((base + "/system/latest_version").c_str(), cachedLatestVersion.c_str(), true);
  }
  
  mqttClient.publish((base + "/settings/auto_update").c_str(), autoUpdateEnabled ? "ON" : "OFF", true);
  mqttClient.publish((base + "/system/mac").c_str(), WiFi.macAddress().c_str(), true);
}

// Publish all states
void publishMqttState() {
  publishMqttLedState();
  publishMqttBlinkingState();
  publishMqttSmartSensingState();
  publishMqttWifiStatus();
  publishMqttSystemStatus();
}

// ===== Home Assistant Auto-Discovery =====

// Helper function to create device info JSON object
void addHADeviceInfo(JsonDocument& doc) {
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
  if (!mqttClient.connected() || !mqttHADiscovery) return;
  
  DebugOut.println("MQTT: Publishing Home Assistant discovery configs...");
  
  String deviceId = getMqttDeviceId();
  String base = mqttBaseTopic;
  
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
    String topic = "homeassistant/number/" + deviceId + "/timer_duration/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }
  
  // ===== Voltage Threshold Number =====
  {
    JsonDocument doc;
    doc["name"] = "Voltage Threshold";
    doc["unique_id"] = deviceId + "_voltage_threshold";
    doc["state_topic"] = base + "/smartsensing/voltage_threshold";
    doc["command_topic"] = base + "/smartsensing/voltage_threshold/set";
    doc["min"] = 0.1;
    doc["max"] = 3.3;
    doc["step"] = 0.1;
    doc["unit_of_measurement"] = "V";
    doc["icon"] = "mdi:flash-outline";
    addHADeviceInfo(doc);
    
    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/number/" + deviceId + "/voltage_threshold/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }
  
  // ===== Voltage Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Voltage";
    doc["unique_id"] = deviceId + "_voltage";
    doc["state_topic"] = base + "/smartsensing/voltage";
    doc["unit_of_measurement"] = "V";
    doc["device_class"] = "voltage";
    doc["state_class"] = "measurement";
    doc["icon"] = "mdi:flash";
    addHADeviceInfo(doc);
    
    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/voltage/config";
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
    String topic = "homeassistant/sensor/" + deviceId + "/timer_remaining/config";
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
    String topic = "homeassistant/binary_sensor/" + deviceId + "/wifi_connected/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }
  
  // ===== Voltage Detected Binary Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Voltage Detected";
    doc["unique_id"] = deviceId + "_voltage_detected";
    doc["state_topic"] = base + "/smartsensing/voltage_detected";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:sine-wave";
    addHADeviceInfo(doc);
    
    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/binary_sensor/" + deviceId + "/voltage_detected/config";
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
    String topic = "homeassistant/binary_sensor/" + deviceId + "/update_available/config";
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
  
  DebugOut.println("MQTT: Home Assistant discovery configs published");
}

// Remove Home Assistant auto-discovery configuration
void removeHADiscovery() {
  if (!mqttClient.connected()) return;
  
  DebugOut.println("MQTT: Removing Home Assistant discovery configs...");
  
  String deviceId = getMqttDeviceId();
  
  // List of all discovery topics to remove
  const char* topics[] = {
    "homeassistant/switch/%s/blinking/config",
    "homeassistant/switch/%s/amplifier/config",
    "homeassistant/switch/%s/ap/config",
    "homeassistant/switch/%s/auto_update/config",
    "homeassistant/select/%s/mode/config",
    "homeassistant/number/%s/timer_duration/config",
    "homeassistant/number/%s/voltage_threshold/config",
    "homeassistant/sensor/%s/voltage/config",
    "homeassistant/sensor/%s/timer_remaining/config",
    "homeassistant/sensor/%s/rssi/config",
    "homeassistant/sensor/%s/firmware/config",
    "homeassistant/sensor/%s/ip/config",
    "homeassistant/binary_sensor/%s/wifi_connected/config",
    "homeassistant/binary_sensor/%s/voltage_detected/config",
    "homeassistant/binary_sensor/%s/update_available/config",
    "homeassistant/button/%s/reboot/config",
    "homeassistant/button/%s/check_update/config"
  };
  
  char topicBuf[128];
  for (const char* topicTemplate : topics) {
    snprintf(topicBuf, sizeof(topicBuf), topicTemplate, deviceId.c_str());
    mqttClient.publish(topicBuf, "", true);  // Empty payload removes the config
  }
  
  DebugOut.println("MQTT: Home Assistant discovery configs removed");
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
  
  // Update password (only if provided)
  if (doc["password"].is<String>()) {
    String newPassword = doc["password"].as<String>();
    // Only update if a new password is provided (empty string clears it)
    if (newPassword.length() > 0 || !doc["clearPassword"].isNull()) {
      mqttPassword = newPassword;
      settingsChanged = true;
      needReconnect = true;
    }
  }
  
  // Update base topic
  if (doc["baseTopic"].is<String>()) {
    String newBaseTopic = doc["baseTopic"].as<String>();
    if (newBaseTopic.length() > 0 && mqttBaseTopic != newBaseTopic) {
      // Remove old HA discovery before changing topic
      if (mqttHADiscovery && mqttClient.connected()) {
        removeHADiscovery();
      }
      mqttBaseTopic = newBaseTopic;
      settingsChanged = true;
      needReconnect = true;
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
    DebugOut.println("MQTT settings updated");
  }
  
  // Reconnect if needed
  if (needReconnect && mqttEnabled && mqttBroker.length() > 0) {
    if (mqttClient.connected()) {
      mqttClient.disconnect();
    }
    mqttConnected = false;
    lastMqttReconnect = 0;  // Force immediate reconnect attempt
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
