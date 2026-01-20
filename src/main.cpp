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

// Forward declarations
int compareVersions(const String& v1, const String& v2);

// Test 2 
// MQTT functions are defined in mqtt_handler.h/.cpp
// OTA update functions are defined in ota_updater.h/.cpp
// Smart Sensing functions are defined in smart_sensing.h/.cpp
// WiFi functions are defined in wifi_manager.h/.cpp
// Settings functions are defined in settings_manager.h/.cpp
// WebSocket functions are defined in websocket_handler.h/.cpp

// WiFi credentials
String wifiSSID = "";
String wifiPassword = "";

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

// GitHub Root CA Certificate (DigiCert Global Root G2)
// GitHub uses DigiCert certificates - this root CA is valid until 2038
const char* github_root_ca = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDRjCCAsugAwIBAgIQGp6v7G3o4ZtcGTFBto2Q3TAKBggqhkjOPQQDAzCBiDEL\n" \
"MAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNl\n" \
"eSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMT\n" \
"JVVTRVJUcnVzdCBFQ0MgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMjEwMzIy\n" \
"MDAwMDAwWhcNMzgwMTE4MjM1OTU5WjBfMQswCQYDVQQGEwJHQjEYMBYGA1UEChMP\n" \
"U2VjdGlnbyBMaW1pdGVkMTYwNAYDVQQDEy1TZWN0aWdvIFB1YmxpYyBTZXJ2ZXIg\n" \
"QXV0aGVudGljYXRpb24gUm9vdCBFNDYwdjAQBgcqhkjOPQIBBgUrgQQAIgNiAAR2\n" \
"+pmpbiDt+dd34wc7qNs9Xzjoq1WmVk/WSOrsfy2qw7LFeeyZYX8QeccCWvkEN/U0\n" \
"NSt3zn8gj1KjAIns1aeibVvjS5KToID1AZTc8GgHHs3u/iVStSBDHBv+6xnOQ6Oj\n" \
"ggEgMIIBHDAfBgNVHSMEGDAWgBQ64QmG1M8ZwpZ2dEl23OA1xmNjmjAdBgNVHQ4E\n" \
"FgQU0SLaTFnxS18mOKqd1u7rDcP7qWEwDgYDVR0PAQH/BAQDAgGGMA8GA1UdEwEB\n" \
"/wQFMAMBAf8wHQYDVR0lBBYwFAYIKwYBBQUHAwEGCCsGAQUFBwMCMBEGA1UdIAQK\n" \
"MAgwBgYEVR0gADBQBgNVHR8ESTBHMEWgQ6BBhj9odHRwOi8vY3JsLnVzZXJ0cnVz\n" \
"dC5jb20vVVNFUlRydXN0RUNDQ2VydGlmaWNhdGlvbkF1dGhvcml0eS5jcmwwNQYI\n" \
"KwYBBQUHAQEEKTAnMCUGCCsGAQUFBzABhhlodHRwOi8vb2NzcC51c2VydHJ1c3Qu\n" \
"Y29tMAoGCCqGSM49BAMDA2kAMGYCMQCMCyBit99vX2ba6xEkDe+YO7vC0twjbkv9\n" \
"PKpqGGuZ61JZryjFsp+DFpEclCVy4noCMQCwvZDXD/m2Ko1HA5Bkmz7YQOFAiNDD\n" \
"49IWa2wdT7R3DtODaSXH/BiXv8fwB9su4tU=\n" \
"-----END CERTIFICATE-----\n" \
"";

void setup() {
  Serial.begin(9600);
  delay(1000);
  
  Serial.println("\n\n=== ESP32-S3 LED Blink with WebSocket ===");
  Serial.println("Initializing...");
  Serial.printf("Current Firmware Version: %s\n", firmwareVer);

  // Generate a unique AP SSID based on the device ID (last 4 hex chars of MAC)
  uint64_t chipId = ESP.getEfuseMac();
  uint16_t shortId = (uint16_t)(chipId & 0xFFFF);
  char idBuf[5];
  snprintf(idBuf, sizeof(idBuf), "%04X", shortId);
  apSSID = String("ALX Audio Controller ") + idBuf;
  Serial.printf("AP SSID set to: %s\n", apSSID.c_str());
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  //Serial.printf("LED pin configured: GPIO%d\n", LED_PIN);
  
  // Configure factory reset button with enhanced detection
  resetButton.begin();
  Serial.printf("Factory reset button configured: GPIO%d\n", RESET_BUTTON_PIN);
  Serial.println("  Button actions:");
  Serial.println("    - Short press (< 0.5s): Print status info");
  Serial.println("    - Double click: Toggle AP mode");
  Serial.println("    - Triple click: Toggle LED blinking");
  Serial.println("    - Long press (2s): Restart ESP32");
  Serial.println("    - Very long press (10s): Reboot ESP32");
  
  // Configure Smart Sensing pins
  pinMode(AMPLIFIER_PIN, OUTPUT);
  digitalWrite(AMPLIFIER_PIN, LOW);  // Start with amplifier OFF (fail-safe)
  pinMode(VOLTAGE_SENSE_PIN, INPUT);
  Serial.printf("Smart Sensing configured: Amplifier GPIO%d, Voltage Sense GPIO%d\n", 
                AMPLIFIER_PIN, VOLTAGE_SENSE_PIN);
  
  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialization failed!");
  } else {
    Serial.println("SPIFFS initialized");
  }

  // Load persisted settings (e.g., auto-update preference)
  if (!loadSettings()) {
    Serial.println("No settings file found, using defaults");
  }
  
  // Load Smart Sensing settings
  if (!loadSmartSensingSettings()) {
    Serial.println("No Smart Sensing settings found, using defaults");
  }
  
  // Load MQTT settings
  if (!loadMqttSettings()) {
    Serial.println("No MQTT settings found, using defaults");
  }
  
  // Define server routes here (before WiFi setup)
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", htmlPage);
  });
  
  server.on("/api/wificonfig", HTTP_POST, handleWiFiConfig);
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
  
  // Try to load WiFi credentials from storage
  if (loadWiFiCredentials(wifiSSID, wifiPassword) && wifiSSID.length() > 0) {
    Serial.println("Loaded credentials from storage");
    connectToWiFi(wifiSSID.c_str(), wifiPassword.c_str());
  } else {
    Serial.println("No stored credentials found, starting Access Point mode");
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
        Serial.println("=== Button: Short Press ===");
        Serial.printf("WiFi: %s\n", WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
        Serial.printf("AP Mode: %s\n", isAPMode ? "Active" : "Inactive");
        Serial.printf("LED Blinking: %s\n", blinkingEnabled ? "Enabled" : "Disabled");
        Serial.printf("Firmware: %s\n", firmwareVer);
        break;
        
      case BTN_DOUBLE_CLICK:
        Serial.println("=== Button: Double Click - Toggle AP Mode ===");
        if (isAPMode) {
          stopAccessPoint();
        } else {
          startAccessPoint();
        }
        break;
        
      case BTN_TRIPLE_CLICK:
        Serial.println("=== Button: Triple Click - Toggle LED Blinking ===");
        blinkingEnabled = !blinkingEnabled;
        Serial.printf("LED Blinking is now: %s\n", blinkingEnabled ? "ON" : "OFF");
        sendBlinkingState();
        break;
        
      case BTN_LONG_PRESS:
        Serial.println("=== Button: Long Press - Restarting ESP32 ===");
        delay(500);
        ESP.restart();
        break;
        
      case BTN_VERY_LONG_PRESS:
        Serial.println("=== Button: Very Long Press - Rebooting ESP32 ===");
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
          Serial.printf("Button held for %lu seconds...\n", secondsHeld);
          
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
  
  // Auto-update countdown logic
  if (autoUpdateEnabled && updateAvailable && !otaInProgress && updateDiscoveredTime > 0) {
    unsigned long elapsed = millis() - updateDiscoveredTime;
    
    // Broadcast countdown every second
    static unsigned long lastCountdownBroadcast = 0;
    if (millis() - lastCountdownBroadcast >= 1000) {
      lastCountdownBroadcast = millis();
      broadcastUpdateStatus();
    }
    
    if (elapsed >= AUTO_UPDATE_COUNTDOWN) {
      Serial.println("Auto-update countdown expired, starting update...");
      otaStatus = "downloading";
      otaProgress = 0;
      
      if (performOTAUpdate(cachedFirmwareUrl)) {
        Serial.println("OTA update successful! Rebooting...");
        delay(2000);
        ESP.restart();
      } else {
        Serial.println("OTA update failed!");
        otaInProgress = false;
        updateAvailable = false;  // Reset to avoid retry loop
        updateDiscoveredTime = 0;
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

  // IMPORTANT: blinking must NOT depend on isAPMode
  if (blinkingEnabled) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= LED_BLINK_INTERVAL) {
      previousMillis = currentMillis;
      
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      
      sendLEDState();
      
      //Serial.printf("[%lu ms] LED: %s\n", currentMillis, ledState ? "ON" : "OFF");
    }
  } else {
    if (ledState) {
      ledState = false;
      digitalWrite(LED_PIN, LOW);
      sendLEDState();
      Serial.println("Blinking stopped - LED turned OFF");
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
