#include "app_state.h"
#include "auth_handler.h"
#include "button_handler.h"
#include "config.h"
#include "debug_serial.h"
#include "login_page.h"
#include "mqtt_handler.h"
#include "ota_updater.h"
#include "settings_manager.h"
#include "smart_sensing.h"
#include "utils.h"
#include "web_pages.h"
#include "websocket_handler.h"
#include "wifi_manager.h"
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <Update.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <mbedtls/md.h>
#include <time.h>

// Forward declarations
int compareVersions(const String &v1, const String &v2);

// Global DNS Server (defined in wifi_manager.cpp)
extern DNSServer dnsServer;

// MQTT functions are defined in mqtt_handler.h/.cpp
// OTA update functions are defined in ota_updater.h/.cpp
// Smart Sensing functions are defined in smart_sensing.h/.cpp
// WiFi functions are defined in wifi_manager.h/.cpp
// Settings functions are defined in settings_manager.h/.cpp
// WebSocket functions are defined in websocket_handler.h/.cpp

// ===== Global Server Instances (required for library callbacks) =====
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// ===== MQTT Client Objects =====
WiFiClient mqttWifiClient;
PubSubClient mqttClient(mqttWifiClient);

// ===== Firmware Constants =====
const char *firmwareVer = FIRMWARE_VERSION;
const char *githubRepoOwner = GITHUB_REPO_OWNER;
const char *githubRepoName = GITHUB_REPO_NAME;

// ===== Button Handler =====
ButtonHandler resetButton(RESET_BUTTON_PIN);

// ===== Legacy extern variable aliases (for backward compatibility) =====
// These reference the AppState singleton internally
// TODO: Remove these once all handlers are updated to use appState directly
#define wifiSSID appState.wifiSSID
#define wifiPassword appState.wifiPassword
#define deviceSerialNumber appState.deviceSerialNumber
#define blinkingEnabled appState.blinkingEnabled
#define ledState appState.ledState
#define previousMillis appState.previousMillis
#define isAPMode appState.isAPMode
#define apEnabled appState.apEnabled
#define apSSID appState.apSSID
#define apPassword appState.apPassword
#define factoryResetInProgress appState.factoryResetInProgress
#define lastOTACheck appState.lastOTACheck
#define otaInProgress appState.otaInProgress
#define otaProgress appState.otaProgress
#define otaStatus appState.otaStatus
#define otaStatusMessage appState.otaStatusMessage
#define otaProgressBytes appState.otaProgressBytes
#define otaTotalBytes appState.otaTotalBytes
#define autoUpdateEnabled appState.autoUpdateEnabled
#define cachedFirmwareUrl appState.cachedFirmwareUrl
#define cachedChecksum appState.cachedChecksum
#define timezoneOffset appState.timezoneOffset
#define nightMode appState.nightMode
#define updateAvailable appState.updateAvailable
#define cachedLatestVersion appState.cachedLatestVersion
#define updateDiscoveredTime appState.updateDiscoveredTime
#define justUpdated appState.justUpdated
#define previousFirmwareVersion appState.previousFirmwareVersion
#define currentMode appState.currentMode
#define timerDuration appState.timerDuration
#define timerRemaining appState.timerRemaining
#define lastVoltageDetection appState.lastVoltageDetection
#define lastTimerUpdate appState.lastTimerUpdate
#define voltageThreshold appState.voltageThreshold
#define amplifierState appState.amplifierState
#define lastVoltageReading appState.lastVoltageReading
#define previousVoltageState appState.previousVoltageState
#define lastSmartSensingHeartbeat appState.lastSmartSensingHeartbeat
#define enableCertValidation appState.enableCertValidation
#define hardwareStatsInterval appState.hardwareStatsInterval
#define mqttEnabled appState.mqttEnabled
#define mqttBroker appState.mqttBroker
#define mqttPort appState.mqttPort
#define mqttUsername appState.mqttUsername
#define mqttPassword appState.mqttPassword
#define mqttBaseTopic appState.mqttBaseTopic
#define mqttHADiscovery appState.mqttHADiscovery
#define lastMqttReconnect appState.lastMqttReconnect
#define mqttConnected appState.mqttConnected
#define lastMqttPublish appState.lastMqttPublish

// Note: GitHub Root CA Certificate removed - now using Mozilla certificate
// bundle via ESP32CertBundle library for automatic SSL validation of all public
// servers

// ===== Serial Number Generation =====
// Generates a unique serial number from eFuse MAC and stores it in NVS
// Regenerates when firmware version changes
void initSerialNumber() {
  Preferences prefs;
  prefs.begin("device",
              false); // Open NVS namespace "device" in read-write mode

  // Get stored firmware version
  String storedFwVer = prefs.getString("fw_ver", "");
  String currentFwVer = String(FIRMWARE_VERSION);

  // Check if we need to regenerate (firmware version mismatch or missing
  // serial)
  if (storedFwVer != currentFwVer || !prefs.isKey("serial")) {
    // Generate serial number from eFuse MAC
    uint64_t mac = ESP.getEfuseMac();
    char serial[17];
    snprintf(serial, sizeof(serial), "ALX-%02X%02X%02X%02X%02X%02X",
             (uint8_t)(mac), (uint8_t)(mac >> 8), (uint8_t)(mac >> 16),
             (uint8_t)(mac >> 24), (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));

    appState.deviceSerialNumber = String(serial);

    // Store serial number and firmware version in NVS
    prefs.putString("serial", appState.deviceSerialNumber);
    prefs.putString("fw_ver", currentFwVer);

    DebugOut.printf("Serial number generated: %s (firmware: %s)\n",
                    appState.deviceSerialNumber.c_str(), currentFwVer.c_str());
  } else {
    // Load existing serial number
    appState.deviceSerialNumber = prefs.getString("serial", "");
    DebugOut.printf("Serial number loaded: %s\n",
                    appState.deviceSerialNumber.c_str());
  }

  prefs.end();
}

void setup() {
  DebugOut.begin(9600);
  delay(1000);

  DebugOut.println("\n\n=== ESP32-S3 LED Blink with WebSocket ===");
  DebugOut.println("Initializing...");
  DebugOut.printf("Current Firmware Version: %s\n", firmwareVer);

  // Initialize device serial number from NVS (generates on first boot or
  // firmware update)
  initSerialNumber();

  // Set AP SSID to the device serial number (e.g., ALX-AABBCCDDEEFF)
  appState.apSSID = appState.deviceSerialNumber;
  DebugOut.printf("AP SSID set to: %s\n", appState.apSSID.c_str());

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Configure factory reset button with enhanced detection
  resetButton.begin();
  DebugOut.printf("Factory reset button configured: GPIO%d\n",
                  RESET_BUTTON_PIN);
  DebugOut.println("  Button actions:");
  DebugOut.println("    - Short press (< 0.5s): Print status info");
  DebugOut.println("    - Double click: Toggle AP mode");
  DebugOut.println("    - Triple click: Toggle LED blinking");
  DebugOut.println("    - Long press (2s): Restart ESP32");
  DebugOut.println("    - Very long press (10s): Reboot ESP32");

  // Configure Smart Sensing pins
  pinMode(AMPLIFIER_PIN, OUTPUT);
  digitalWrite(AMPLIFIER_PIN, LOW); // Start with amplifier OFF (fail-safe)
  pinMode(VOLTAGE_SENSE_PIN, INPUT);
  DebugOut.printf(
      "Smart Sensing configured: Amplifier GPIO%d, Voltage Sense GPIO%d\n",
      AMPLIFIER_PIN, VOLTAGE_SENSE_PIN);

  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    DebugOut.println("ERROR: LittleFS initialization failed!");
  } else {
    DebugOut.println("LittleFS initialized");
  }

  // Check if device just rebooted after successful OTA update
  appState.justUpdated =
      checkAndClearOTASuccessFlag(appState.previousFirmwareVersion);
  if (appState.justUpdated) {
    DebugOut.printf("🎉 Firmware was just updated from %s to %s\n",
                    appState.previousFirmwareVersion.c_str(), firmwareVer);
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

  // Initialize authentication system
  initAuth();

  // Note: Certificate loading removed - now using Mozilla certificate bundle
  // via ESP32CertBundle library for automatic SSL validation

  // ===== Header Collection for Auth and Gzip =====
  // IMPORTANT: We must collect the "Cookie" header to read the session ID
  // Also collecting X-Session-ID as a fallback for API calls
  // Accept-Encoding allows us to serve gzipped content when supported
  const char *headerkeys[] = {"Cookie", "X-Session-ID", "Accept-Encoding"};
  server.collectHeaders(headerkeys, 3);

  // Define server routes here (before WiFi setup)

  // Favicon (don't redirect/auth for this)
  server.on("/favicon.ico", HTTP_GET,
            []() { server.send(404, "text/plain", "Not Found"); });

  // Common browser auto-requests (reduce console noise)
  server.on("/manifest.json", HTTP_GET,
            []() { server.send(404, "text/plain", "Not Found"); });
  server.on("/robots.txt", HTTP_GET,
            []() { server.send(404, "text/plain", "Not Found"); });
  server.on("/sitemap.xml", HTTP_GET,
            []() { server.send(404, "text/plain", "Not Found"); });
  server.on("/apple-touch-icon.png", HTTP_GET,
            []() { server.send(404, "text/plain", "Not Found"); });
  server.on("/apple-touch-icon-precomposed.png", HTTP_GET,
            []() { server.send(404, "text/plain", "Not Found"); });

  // Android/Chrome captive portal check
  server.on("/generate_204", HTTP_GET, []() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "Redirecting...");
  });

  // Apple captive portal check
  server.on("/hotspot-detect.html", HTTP_GET, []() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "Redirecting...");
  });

  // Redirect all unknown routes to root in AP mode (Captive Portal)
  server.onNotFound([]() {
    // Log the request for debugging
    DebugOut.printf("404 Not Found: %s %s\n",
                    server.method() == HTTP_GET ? "GET" :
                    server.method() == HTTP_POST ? "POST" : "OTHER",
                    server.uri().c_str());

    if (appState.isAPMode) {
      server.sendHeader("Location",
                        String("http://") + WiFi.softAPIP().toString() + "/",
                        true);
      server.send(302, "text/plain", "Redirecting to Captive Portal");
    } else {
      server.send(404, "text/plain", "Not Found");
    }
  });

  // Authentication routes (unprotected)
  server.on("/login", HTTP_GET, []() {
    if (!sendGzipped(server, loginPage_gz, loginPage_gz_len)) {
      server.send_P(200, "text/html", loginPage);
    }
  });
  server.on("/api/auth/login", HTTP_POST, handleLogin);
  server.on("/api/auth/logout", HTTP_POST, handleLogout);
  server.on("/api/auth/status", HTTP_GET, handleAuthStatus);
  server.on("/api/auth/change", HTTP_POST, handlePasswordChange);

  // Protected routes
  server.on("/", HTTP_GET, []() {
    if (!requireAuth())
      return;

    // Serve gzipped dashboard if client supports it (~85% smaller)
    if (!sendGzipped(server, htmlPage_gz, htmlPage_gz_len)) {
      server.send_P(200, "text/html", htmlPage);
    }
  });

  server.on("/api/wificonfig", HTTP_POST, []() {
    if (!requireAuth())
      return;
    handleWiFiConfig();
  });
  server.on("/api/wifisave", HTTP_POST, []() {
    if (!requireAuth())
      return;
    handleWiFiSave();
  });
  server.on("/api/wifiscan", HTTP_GET, []() {
    if (!requireAuth())
      return;
    handleWiFiScan();
  });
  server.on("/api/wifilist", HTTP_GET, []() {
    if (!requireAuth())
      return;
    handleWiFiList();
  });
  server.on("/api/wifiremove", HTTP_POST, []() {
    if (!requireAuth())
      return;
    handleWiFiRemove();
  });
  server.on("/api/apconfig", HTTP_POST, []() {
    if (!requireAuth())
      return;
    handleAPConfigUpdate();
  });
  server.on("/api/toggleap", HTTP_POST, []() {
    if (!requireAuth())
      return;
    handleAPToggle();
  });
  server.on("/api/wifistatus", HTTP_GET, []() {
    if (!requireAuth())
      return;
    handleWiFiStatus();
  });
  server.on("/api/checkupdate", HTTP_GET, []() {
    if (!requireAuth())
      return;
    handleCheckUpdate();
  });
  server.on("/api/startupdate", HTTP_POST, []() {
    if (!requireAuth())
      return;
    handleStartUpdate();
  });
  server.on("/api/updatestatus", HTTP_GET, []() {
    if (!requireAuth())
      return;
    handleUpdateStatus();
  });
  server.on("/api/releasenotes", HTTP_GET, []() {
    if (!requireAuth())
      return;
    handleGetReleaseNotes();
  });
  server.on("/api/settings", HTTP_GET, []() {
    if (!requireAuth())
      return;
    handleSettingsGet();
  });
  server.on("/api/settings", HTTP_POST, []() {
    if (!requireAuth())
      return;
    handleSettingsUpdate();
  });
  server.on("/api/settings/export", HTTP_GET, []() {
    if (!requireAuth())
      return;
    handleSettingsExport();
  });
  server.on("/api/settings/import", HTTP_POST, []() {
    if (!requireAuth())
      return;
    handleSettingsImport();
  });
  server.on("/api/diagnostics", HTTP_GET, []() {
    if (!requireAuth())
      return;
    handleDiagnostics();
  });
  server.on("/api/factoryreset", HTTP_POST, []() {
    if (!requireAuth())
      return;
    handleFactoryReset();
  });
  server.on("/api/reboot", HTTP_POST, []() {
    if (!requireAuth())
      return;
    handleReboot();
  });
  server.on("/api/smartsensing", HTTP_GET, []() {
    if (!requireAuth())
      return;
    handleSmartSensingGet();
  });
  server.on("/api/smartsensing", HTTP_POST, []() {
    if (!requireAuth())
      return;
    handleSmartSensingUpdate();
  });
  server.on("/api/mqtt", HTTP_GET, []() {
    if (!requireAuth())
      return;
    handleMqttGet();
  });
  server.on("/api/mqtt", HTTP_POST, []() {
    if (!requireAuth())
      return;
    handleMqttUpdate();
  });
  server.on(
      "/api/firmware/upload", HTTP_POST,
      []() {
        if (!requireAuth())
          return;
        handleFirmwareUploadComplete();
      },
      []() {
        if (!requireAuth())
          return;
        handleFirmwareUploadChunk();
      });
  // Note: Certificate API routes removed - now using Mozilla certificate bundle

  // Initialize CPU usage monitoring
  initCpuUsageMonitoring();

  // Initialize WiFi event handler for automatic reconnection
  initWiFiEventHandler();

  // Migrate old WiFi credentials to new multi-WiFi system (one-time operation)
  migrateWiFiCredentials();

  // Try to connect to stored WiFi networks (tries all saved networks in
  // priority order)
  if (!connectToStoredNetworks()) {
    DebugOut.println("No WiFi connection established. Running in AP mode.");
  }

  // Always start server and WebSocket regardless of mode
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  DebugOut.setWebSocket(&webSocket);
  server.begin();
  DebugOut.println("Web server and WebSocket started");

  // Set initial FSM state
  appState.setFSMState(STATE_IDLE);
}

void loop() {
  // Small delay to reduce CPU usage - allows other tasks to run
  // Without this, the loop runs as fast as possible (~49% CPU)
  delay(1);

  server.handleClient();
  if (appState.isAPMode) {
    dnsServer.processNextRequest();
  }
  webSocket.loop();
  mqttLoop();
  updateWiFiConnection();
  checkWiFiConnection(); // Monitor WiFi and auto-reconnect if disconnected

  // Check WebSocket auth timeouts
  for (int i = 0; i < MAX_WS_CLIENTS; i++) {
    if (wsAuthTimeout[i] > 0 && millis() > wsAuthTimeout[i]) {
      if (!wsAuthStatus[i]) {
        DebugOut.printf("WebSocket client [%u] auth timeout\n", i);
        webSocket.disconnect(i);
      }
      wsAuthTimeout[i] = 0;
    }
  }

  // Enhanced button monitoring with multiple press types
  if (!appState.factoryResetInProgress) {
    ButtonPressType pressType = resetButton.update();

    // Handle different button press types
    switch (pressType) {
    case BTN_SHORT_PRESS:
      DebugOut.println("=== Button: Short Press ===");
      DebugOut.printf("WiFi: %s\n", WiFi.status() == WL_CONNECTED
                                        ? "Connected"
                                        : "Disconnected");
      DebugOut.printf("AP Mode: %s\n",
                      appState.isAPMode ? "Active" : "Inactive");
      DebugOut.printf("LED Blinking: %s\n",
                      appState.blinkingEnabled ? "Enabled" : "Disabled");
      DebugOut.printf("Firmware: %s\n", firmwareVer);
      break;

    case BTN_DOUBLE_CLICK:
      DebugOut.println("=== Button: Double Click - Toggle AP Mode ===");
      if (appState.isAPMode) {
        stopAccessPoint();
      } else {
        startAccessPoint();
      }
      break;

    case BTN_TRIPLE_CLICK:
      DebugOut.println("=== Button: Triple Click - Toggle LED Blinking ===");
      appState.setBlinkingEnabled(!appState.blinkingEnabled);
      DebugOut.printf("LED Blinking is now: %s\n",
                      appState.blinkingEnabled ? "ON" : "OFF");
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
      // Button not pressed - restore LED to normal state if it was blinking for
      // feedback
      static bool wasPressed = false;
      if (wasPressed) {
        digitalWrite(LED_PIN, appState.ledState);
        wasPressed = false;
      }
      if (resetButton.isPressed()) {
        wasPressed = true;
      }
    }
  }

  // Periodic firmware check (every 5 minutes) - runs regardless of auto-update
  // setting
  if (!appState.isAPMode && WiFi.status() == WL_CONNECTED &&
      !appState.otaInProgress) {
    unsigned long currentMillis = millis();
    if (currentMillis - appState.lastOTACheck >= OTA_CHECK_INTERVAL ||
        appState.lastOTACheck == 0) {
      appState.lastOTACheck = currentMillis;
      checkForFirmwareUpdate();
    }
  }

  // Auto-update logic (runs on every periodic check when update is available)
  // Will retry on next periodic check (5 min) if amplifier is in use
  if (appState.autoUpdateEnabled && appState.updateAvailable &&
      !appState.otaInProgress && appState.updateDiscoveredTime > 0) {
    if (appState.amplifierState) {
      // Amplifier is ON - skip this check, will retry on next periodic check
      // Reset updateDiscoveredTime so countdown restarts when amp turns off
      DebugOut.println("⚠️  Auto-update skipped: Amplifier is in use. Will "
                       "retry on next check.");
      appState.updateDiscoveredTime = 0;
      broadcastUpdateStatus();
    } else {
      // Amplifier is OFF - safe to proceed with countdown
      unsigned long elapsed = millis() - appState.updateDiscoveredTime;

      // Broadcast countdown every second
      static unsigned long lastCountdownBroadcast = 0;
      if (millis() - lastCountdownBroadcast >= 1000) {
        lastCountdownBroadcast = millis();
        broadcastUpdateStatus();
      }

      if (elapsed >= AUTO_UPDATE_COUNTDOWN) {
        // Double-check amplifier state before starting update
        if (appState.amplifierState) {
          DebugOut.println("⚠️  Auto-update cancelled: Amplifier turned on "
                           "during countdown. Will retry on next check.");
          appState.updateDiscoveredTime =
              0; // Reset to retry on next periodic check
          broadcastUpdateStatus();
        } else {
          DebugOut.println(
              "Auto-update: Starting update (amplifier is off)...");
          appState.otaStatus = "downloading";
          appState.otaProgress = 0;
          appState.setFSMState(STATE_OTA_UPDATE);

          if (performOTAUpdate(appState.cachedFirmwareUrl)) {
            DebugOut.println("OTA update successful! Rebooting...");
            saveOTASuccessFlag(firmwareVer); // Save current version as
                                             // "previous" before reboot
            delay(2000);
            ESP.restart();
          } else {
            DebugOut.println("OTA update failed! Will retry on next check.");
            appState.otaInProgress = false;
            appState.updateDiscoveredTime =
                0; // Reset to retry on next periodic check
            appState.setFSMState(STATE_IDLE);
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
  if (millis() - lastHardwareStatsBroadcast >= appState.hardwareStatsInterval) {
    lastHardwareStatsBroadcast = millis();
    sendHardwareStats();
  }

  // IMPORTANT: blinking must NOT depend on isAPMode
  if (appState.blinkingEnabled) {
    unsigned long currentMillis = millis();
    if (currentMillis - appState.previousMillis >= LED_BLINK_INTERVAL) {
      appState.previousMillis = currentMillis;

      appState.setLedState(!appState.ledState);
      digitalWrite(LED_PIN, appState.ledState);

      sendLEDState();
    }
  } else {
    if (appState.ledState) {
      appState.setLedState(false);
      digitalWrite(LED_PIN, LOW);
      sendLEDState();
      DebugOut.println("Blinking stopped - LED turned OFF");
    }
  }
}

// WiFi functions are defined in wifi_manager.h/.cpp
// Settings persistence functions are defined in settings_manager.h/.cpp
// Utility functions (compareVersions, etc.) are defined in utils.h/.cpp
