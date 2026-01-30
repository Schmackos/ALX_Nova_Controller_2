#include <WiFi.h>
#include <WebSocketsServer.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <mbedtls/md.h>
#include <time.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include "config.h"
#include "app_state.h"
#include "button_handler.h"
#include "mqtt_handler.h"
#include "ota_updater.h"
#include "smart_sensing.h"
#include "wifi_manager.h"
#include "settings_manager.h"
#include "websocket_handler.h"
#include "web_pages.h"
#include "debug_serial.h"

// Forward declarations
int compareVersions(const String& v1, const String& v2);

// MQTT functions are defined in mqtt_handler.h/.cpp
// OTA update functions are defined in ota_updater.h/.cpp
// Smart Sensing functions are defined in smart_sensing.h/.cpp
// WiFi functions are defined in wifi_manager.h/.cpp
// Settings functions are defined in settings_manager.h/.cpp
// WebSocket functions are defined in websocket_handler.h/.cpp

// WiFi credentials
String wifiSSID = "";
String wifiPassword = "";

// Device serial number (generated from eFuse MAC, stored in NVS)
String deviceSerialNumber = "";

// LED_PIN is defined in config.h

// Web server on port 80
WebServer server(80);

// WebSocket server on port 81
WebSocketsServer webSocket = WebSocketsServer(81);

// State variables
bool blinkingEnabled = true;
bool ledState = false;
unsigned long previousMillis = 0;

// AP Mode variables
bool isAPMode = false;
bool apEnabled = false;
String apSSID = "";
String apPassword = "12345678";

// ===== Button Handler =====
// ButtonHandler class is defined in button_handler.h/.cpp
ButtonHandler resetButton(RESET_BUTTON_PIN);
bool factoryResetInProgress = false;

// OTA Update variables (constants defined in config.h)
const char* firmwareVer = FIRMWARE_VERSION;
const char* githubRepoOwner = GITHUB_REPO_OWNER;
const char* githubRepoName = GITHUB_REPO_NAME;
unsigned long lastOTACheck = 0;
bool otaInProgress = false;
int otaProgress = 0;
String otaStatus = "idle"; // idle, preparing, downloading, complete, error
String otaStatusMessage = "idle";  // Detailed status message
int otaProgressBytes = 0;          // Bytes downloaded
int otaTotalBytes = 0;             // Total firmware size
bool autoUpdateEnabled = false; // whether to auto-check/apply updates
String cachedFirmwareUrl = "";     // Cached firmware download URL
String cachedChecksum = "";        // Cached firmware SHA256 checksum
int timezoneOffset = 0;            // Timezone offset in seconds (default: UTC)
bool nightMode = false;            // Night mode (dark theme) enabled

// Cached update state (AUTO_UPDATE_COUNTDOWN defined in config.h)
bool updateAvailable = false;
String cachedLatestVersion = "";
unsigned long updateDiscoveredTime = 0;  // millis() when update was first detected

// OTA just updated state (set after successful OTA reboot)
bool justUpdated = false;
String previousFirmwareVersion = "";

// Smart Sensing feature variables (pins and enum defined in config.h)
SensingMode currentMode = ALWAYS_ON;
unsigned long timerDuration = 15;     // minutes (default 15)
unsigned long timerRemaining = 0;     // seconds remaining
unsigned long lastVoltageDetection = 0;
unsigned long lastTimerUpdate = 0;
float voltageThreshold = 1.0;         // volts (user configurable)
bool amplifierState = false;
float lastVoltageReading = 0.0;       // Last voltage reading for display
bool previousVoltageState = false;    // Track previous voltage detection state for edge detection

// Smart Sensing broadcast optimization - track previous state for change detection
SensingMode prevBroadcastMode = ALWAYS_ON;
bool prevBroadcastAmplifierState = false;
unsigned long prevBroadcastTimerRemaining = 0;
float prevBroadcastVoltageReading = 0.0;
unsigned long lastSmartSensingHeartbeat = 0;
// SMART_SENSING_HEARTBEAT_INTERVAL defined in config.h

// Certificate validation - can be enabled/disabled via settings
bool enableCertValidation = false;  // Default: DISABLED (can be enabled via web interface)

// Hardware stats broadcast interval - user configurable (1000, 3000, 5000, 10000 ms)
unsigned long hardwareStatsInterval = 2000;  // Default: 2 seconds

// ===== MQTT Client Configuration =====
bool mqttEnabled = false;
String mqttBroker = "";
int mqttPort = 1883;
String mqttUsername = "";
String mqttPassword = "";
String mqttBaseTopic = "esp32-audio";
bool mqttHADiscovery = false;

// MQTT client objects
WiFiClient mqttWifiClient;
PubSubClient mqttClient(mqttWifiClient);

// MQTT reconnect timing (MQTT_RECONNECT_INTERVAL defined in config.h)
unsigned long lastMqttReconnect = 0;
bool mqttConnected = false;

// MQTT state tracking for change detection
bool prevMqttLedState = false;
bool prevMqttBlinkingEnabled = true;
bool prevMqttAmplifierState = false;
SensingMode prevMqttSensingMode = ALWAYS_ON;
unsigned long prevMqttTimerRemaining = 0;
float prevMqttVoltageReading = 0.0;
unsigned long lastMqttPublish = 0;
// MQTT_PUBLISH_INTERVAL defined in config.h

// Note: GitHub Root CA Certificate removed - now using Mozilla certificate bundle
// via ESP32CertBundle library for automatic SSL validation of all public servers

// ===== Serial Number Generation =====
// Generates a unique serial number from eFuse MAC and stores it in NVS
// Regenerates when firmware version changes
void initSerialNumber() {
  Preferences prefs;
  prefs.begin("device", false);  // Open NVS namespace "device" in read-write mode
  
  // Get stored firmware version
  String storedFwVer = prefs.getString("fw_ver", "");
  String currentFwVer = String(FIRMWARE_VERSION);
  
  // Check if we need to regenerate (firmware version mismatch or missing serial)
  if (storedFwVer != currentFwVer || !prefs.isKey("serial")) {
    // Generate serial number from eFuse MAC
    uint64_t mac = ESP.getEfuseMac();
    char serial[17];
    snprintf(serial, sizeof(serial), "ALX-%02X%02X%02X%02X%02X%02X",
             (uint8_t)(mac), (uint8_t)(mac >> 8), (uint8_t)(mac >> 16),
             (uint8_t)(mac >> 24), (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));
    
    deviceSerialNumber = String(serial);
    
    // Store serial number and firmware version in NVS
    prefs.putString("serial", deviceSerialNumber);
    prefs.putString("fw_ver", currentFwVer);
    
    DebugOut.printf("Serial number generated: %s (firmware: %s)\n", 
                  deviceSerialNumber.c_str(), currentFwVer.c_str());
  } else {
    // Load existing serial number
    deviceSerialNumber = prefs.getString("serial", "");
    DebugOut.printf("Serial number loaded: %s\n", deviceSerialNumber.c_str());
  }
  
  prefs.end();
}

void setup() {
  DebugOut.begin(9600);
  delay(1000);
  
  DebugOut.println("\n\n=== ESP32-S3 LED Blink with WebSocket ===");
  DebugOut.println("Initializing...");
  DebugOut.printf("Current Firmware Version: %s\n", firmwareVer);

  // Initialize device serial number from NVS (generates on first boot or firmware update)
  initSerialNumber();

  // Generate a unique AP SSID based on the device ID (last 4 hex chars of MAC)
  uint64_t chipId = ESP.getEfuseMac();
  uint16_t shortId = (uint16_t)(chipId & 0xFFFF);
  char idBuf[5];
  snprintf(idBuf, sizeof(idBuf), "%04X", shortId);
  apSSID = String("ALX Audio Controller ") + idBuf;
  DebugOut.printf("AP SSID set to: %s\n", apSSID.c_str());
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  //DebugOut.printf("LED pin configured: GPIO%d\n", LED_PIN);
  
  // Configure factory reset button with enhanced detection
  resetButton.begin();
  DebugOut.printf("Factory reset button configured: GPIO%d\n", RESET_BUTTON_PIN);
  DebugOut.println("  Button actions:");
  DebugOut.println("    - Short press (< 0.5s): Print status info");
  DebugOut.println("    - Double click: Toggle AP mode");
  DebugOut.println("    - Triple click: Toggle LED blinking");
  DebugOut.println("    - Long press (2s): Restart ESP32");
  DebugOut.println("    - Very long press (10s): Reboot ESP32");
  
  // Configure Smart Sensing pins
  pinMode(AMPLIFIER_PIN, OUTPUT);
  digitalWrite(AMPLIFIER_PIN, LOW);  // Start with amplifier OFF (fail-safe)
  pinMode(VOLTAGE_SENSE_PIN, INPUT);
  DebugOut.printf("Smart Sensing configured: Amplifier GPIO%d, Voltage Sense GPIO%d\n", 
                AMPLIFIER_PIN, VOLTAGE_SENSE_PIN);
  
  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    DebugOut.println("ERROR: SPIFFS initialization failed!");
  } else {
    DebugOut.println("SPIFFS initialized");
  }

  // Check if device just rebooted after successful OTA update
  justUpdated = checkAndClearOTASuccessFlag(previousFirmwareVersion);
  if (justUpdated) {
    DebugOut.printf("🎉 Firmware was just updated from %s to %s\n", previousFirmwareVersion.c_str(), firmwareVer);
  }

  // Load persisted settings (e.g., auto-update preference)
  if (!loadSettings()) {
    DebugOut.println("No settings file found, using defaults");
  }
  
  // Load Smart Sensing settings
  if (!loadSmartSensingSettings()) {
    DebugOut.println("No Smart Sensing settings found, using defaults");
  }
  
  // Load MQTT settings
  if (!loadMqttSettings()) {
    DebugOut.println("No MQTT settings found, using defaults");
  }
  
  // Note: Certificate loading removed - now using Mozilla certificate bundle
  // via ESP32CertBundle library for automatic SSL validation
  
  // Define server routes here (before WiFi setup)
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", htmlPage);
  });
  
  server.on("/api/wificonfig", HTTP_POST, handleWiFiConfig);
  server.on("/api/wifiscan", HTTP_GET, handleWiFiScan);
  server.on("/api/apconfig", HTTP_POST, handleAPConfigUpdate);
  server.on("/api/toggleap", HTTP_POST, handleAPToggle);
  server.on("/api/wifistatus", HTTP_GET, handleWiFiStatus);
  server.on("/api/checkupdate", HTTP_GET, handleCheckUpdate);
  server.on("/api/startupdate", HTTP_POST, handleStartUpdate);
  server.on("/api/updatestatus", HTTP_GET, handleUpdateStatus);
  server.on("/api/releasenotes", HTTP_GET, handleGetReleaseNotes);
  server.on("/api/settings", HTTP_GET, handleSettingsGet);
  server.on("/api/settings", HTTP_POST, handleSettingsUpdate);
  server.on("/api/settings/export", HTTP_GET, handleSettingsExport);
  server.on("/api/settings/import", HTTP_POST, handleSettingsImport);
  server.on("/api/factoryreset", HTTP_POST, handleFactoryReset);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.on("/api/smartsensing", HTTP_GET, handleSmartSensingGet);
  server.on("/api/smartsensing", HTTP_POST, handleSmartSensingUpdate);
  server.on("/api/mqtt", HTTP_GET, handleMqttGet);
  server.on("/api/mqtt", HTTP_POST, handleMqttUpdate);
  server.on("/api/firmware/upload", HTTP_POST, handleFirmwareUploadComplete, handleFirmwareUploadChunk);
  // Note: Certificate API routes removed - now using Mozilla certificate bundle
  
  // Initialize CPU usage monitoring
  initCpuUsageMonitoring();
  
  // Try to load WiFi credentials from storage
  if (loadWiFiCredentials(wifiSSID, wifiPassword) && wifiSSID.length() > 0) {
    DebugOut.println("Loaded credentials from storage");
    connectToWiFi(wifiSSID.c_str(), wifiPassword.c_str());
  } else {
    DebugOut.println("No stored credentials found, starting Access Point mode");
    startAccessPoint();
  }
}

void loop() {
  server.handleClient();
  webSocket.loop();
  mqttLoop();
  
  // Enhanced button monitoring with multiple press types
  if (!factoryResetInProgress) {
    ButtonPressType pressType = resetButton.update();
    
    // Handle different button press types
    switch (pressType) {
      case BTN_SHORT_PRESS:
        DebugOut.println("=== Button: Short Press ===");
        DebugOut.printf("WiFi: %s\n", WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
        DebugOut.printf("AP Mode: %s\n", isAPMode ? "Active" : "Inactive");
        DebugOut.printf("LED Blinking: %s\n", blinkingEnabled ? "Enabled" : "Disabled");
        DebugOut.printf("Firmware: %s\n", firmwareVer);
        break;
        
      case BTN_DOUBLE_CLICK:
        DebugOut.println("=== Button: Double Click - Toggle AP Mode ===");
        if (isAPMode) {
          stopAccessPoint();
        } else {
          startAccessPoint();
        }
        break;
        
      case BTN_TRIPLE_CLICK:
        DebugOut.println("=== Button: Triple Click - Toggle LED Blinking ===");
        blinkingEnabled = !blinkingEnabled;
        DebugOut.printf("LED Blinking is now: %s\n", blinkingEnabled ? "ON" : "OFF");
        sendBlinkingState();
        break;
        
      case BTN_LONG_PRESS:
        DebugOut.println("=== Button: Long Press - Restarting ESP32 ===");
        delay(500);
        ESP.restart();
        break;
        
      case BTN_VERY_LONG_PRESS:
        DebugOut.println("=== Button: Very Long Press - Rebooting ESP32 ===");
        sendRebootProgress(10, true);
        delay(100); // Give time for WebSocket message to send
        ESP.restart();
        break;
        
      case BTN_NONE:
      default:
        // No button press detected, continue normal operation
        break;
    }
    
    // Visual feedback for very long press (reboot)
    if (resetButton.isPressed()) {
      unsigned long holdDuration = resetButton.getHoldDuration();
      
      // Fast blink LED to indicate progress during hold (every 200ms)
      static unsigned long lastBlinkTime = 0;
      if (millis() - lastBlinkTime >= 200) {
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        lastBlinkTime = millis();
      }
      
      // Print progress and send WebSocket update every second for long holds
      if (holdDuration >= BTN_LONG_PRESS_MIN) {
        static unsigned long lastProgressPrint = 0;
        if (millis() - lastProgressPrint >= 1000) {
          unsigned long secondsHeld = holdDuration / 1000;
          DebugOut.printf("Button held for %lu seconds...\n", secondsHeld);
          
          // Send progress if approaching reboot (from 5 seconds onward)
          if (holdDuration >= 5000) {
            sendRebootProgress(secondsHeld, false);
          }
          lastProgressPrint = millis();
        }
      }
    } else {
      // Button not pressed - restore LED to normal state if it was blinking for feedback
      static bool wasPressed = false;
      if (wasPressed) {
        digitalWrite(LED_PIN, ledState);
        wasPressed = false;
      }
      if (resetButton.isPressed()) {
        wasPressed = true;
      }
    }
  }
  
  // Periodic firmware check (every 5 minutes) - runs regardless of auto-update setting
  if (!isAPMode && WiFi.status() == WL_CONNECTED && !otaInProgress) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastOTACheck >= OTA_CHECK_INTERVAL || lastOTACheck == 0) {
      lastOTACheck = currentMillis;
      checkForFirmwareUpdate();
    }
  }
  
  // Auto-update logic (runs on every periodic check when update is available)
  // Will retry on next periodic check (5 min) if amplifier is in use
  if (autoUpdateEnabled && updateAvailable && !otaInProgress && updateDiscoveredTime > 0) {
    if (amplifierState) {
      // Amplifier is ON - skip this check, will retry on next periodic check
      // Reset updateDiscoveredTime so countdown restarts when amp turns off
      DebugOut.println("⚠️  Auto-update skipped: Amplifier is in use. Will retry on next check.");
      updateDiscoveredTime = 0;
      broadcastUpdateStatus();
    } else {
      // Amplifier is OFF - safe to proceed with countdown
      unsigned long elapsed = millis() - updateDiscoveredTime;
      
      // Broadcast countdown every second
      static unsigned long lastCountdownBroadcast = 0;
      if (millis() - lastCountdownBroadcast >= 1000) {
        lastCountdownBroadcast = millis();
        broadcastUpdateStatus();
      }
      
      if (elapsed >= AUTO_UPDATE_COUNTDOWN) {
        // Double-check amplifier state before starting update
        if (amplifierState) {
          DebugOut.println("⚠️  Auto-update cancelled: Amplifier turned on during countdown. Will retry on next check.");
          updateDiscoveredTime = 0;  // Reset to retry on next periodic check
          broadcastUpdateStatus();
        } else {
          DebugOut.println("Auto-update: Starting update (amplifier is off)...");
          otaStatus = "downloading";
          otaProgress = 0;
          
          if (performOTAUpdate(cachedFirmwareUrl)) {
            DebugOut.println("OTA update successful! Rebooting...");
            saveOTASuccessFlag(firmwareVer);  // Save current version as "previous" before reboot
            delay(2000);
            ESP.restart();
          } else {
            DebugOut.println("OTA update failed! Will retry on next check.");
            otaInProgress = false;
            updateDiscoveredTime = 0;  // Reset to retry on next periodic check
          }
        }
      }
    }
  }
  
  // Smart Sensing logic update
  updateSmartSensingLogic();
  
  // Broadcast Smart Sensing state every second
  static unsigned long lastSmartSensingBroadcast = 0;
  if (millis() - lastSmartSensingBroadcast >= 1000) {
    lastSmartSensingBroadcast = millis();
    sendSmartSensingState();
  }
  
  // Broadcast Hardware Stats periodically (user-configurable interval)
  static unsigned long lastHardwareStatsBroadcast = 0;
  if (millis() - lastHardwareStatsBroadcast >= hardwareStatsInterval) {
    lastHardwareStatsBroadcast = millis();
    sendHardwareStats();
  }

  // IMPORTANT: blinking must NOT depend on isAPMode
  if (blinkingEnabled) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= LED_BLINK_INTERVAL) {
      previousMillis = currentMillis;
      
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      
      sendLEDState();
      
      //DebugOut.printf("[%lu ms] LED: %s\n", currentMillis, ledState ? "ON" : "OFF");
    }
  } else {
    if (ledState) {
      ledState = false;
      digitalWrite(LED_PIN, LOW);
      sendLEDState();
      DebugOut.println("Blinking stopped - LED turned OFF");
    }
  }
}

// WiFi functions are defined in wifi_manager.h/.cpp
// Settings persistence functions are defined in settings_manager.h/.cpp

// Compare semantic version strings like "1.0.7" and "1.1.2"
// Returns: -1 if v1 < v2, 0 if equal, 1 if v1 > v2
int compareVersions(const String& v1, const String& v2) {
  int i = 0, j = 0;
  const int n1 = v1.length();
  const int n2 = v2.length();

  while (i < n1 || j < n2) {
    long num1 = 0;
    long num2 = 0;

    while (i < n1 && isDigit(v1[i])) {
      num1 = num1 * 10 + (v1[i] - '0');
      i++;
    }
    // Skip non-digit separators like '.'
    while (i < n1 && !isDigit(v1[i])) i++;

    while (j < n2 && isDigit(v2[j])) {
      num2 = num2 * 10 + (v2[j] - '0');
      j++;
    }
    while (j < n2 && !isDigit(v2[j])) j++;

    if (num1 < num2) return -1;
    if (num1 > num2) return 1;
  }

  return 0;
}
