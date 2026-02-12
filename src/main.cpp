#include "app_state.h"
#include "auth_handler.h"
#include "button_handler.h"
#include "buzzer_handler.h"
#include "config.h"
#include "crash_log.h"
#include "debug_serial.h"
#include "login_page.h"
#include "mqtt_handler.h"
#include "ota_updater.h"
#include "settings_manager.h"
#include "signal_generator.h"
#include "task_monitor.h"
#include "i2s_audio.h"
#ifdef DSP_ENABLED
#include "dsp_api.h"
#include "dsp_pipeline.h"
#endif
#include "smart_sensing.h"
#include "utils.h"
#include "web_pages.h"
#include "websocket_handler.h"
#include "wifi_manager.h"
#ifdef GUI_ENABLED
#include "gui/gui_manager.h"
#endif
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
#include <esp_task_wdt.h>
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
#define darkMode appState.darkMode
#define updateAvailable appState.updateAvailable
#define cachedLatestVersion appState.cachedLatestVersion
#define updateDiscoveredTime appState.updateDiscoveredTime
#define justUpdated appState.justUpdated
#define previousFirmwareVersion appState.previousFirmwareVersion
#define currentMode appState.currentMode
#define timerDuration appState.timerDuration
#define timerRemaining appState.timerRemaining
#define lastSignalDetection appState.lastSignalDetection
#define lastTimerUpdate appState.lastTimerUpdate
#define audioThreshold_dBFS appState.audioThreshold_dBFS
#define amplifierState appState.amplifierState
#define audioLevel_dBFS appState.audioLevel_dBFS
#define previousSignalState appState.previousSignalState
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

    LOG_I("[Main] Serial number generated: %s (firmware: %s)",
          appState.deviceSerialNumber.c_str(), currentFwVer.c_str());
  } else {
    // Load existing serial number
    appState.deviceSerialNumber = prefs.getString("serial", "");
    LOG_I("[Main] Serial number loaded: %s",
          appState.deviceSerialNumber.c_str());
  }

  prefs.end();
}

void setup() {
  DebugOut.begin(9600);
  delay(1000);

  LOG_I("[Main] ESP32-S3 ALX Nova Controller starting");
  LOG_I("[Main] Firmware version: %s", firmwareVer);

  // Initialize device serial number from NVS (generates on first boot or
  // firmware update)
  initSerialNumber();

  // Set AP SSID to the device serial number (e.g., ALX-AABBCCDDEEFF)
  appState.apSSID = appState.deviceSerialNumber;
  LOG_I("[Main] AP SSID set to: %s", appState.apSSID.c_str());

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Configure factory reset button with enhanced detection
  resetButton.begin();
  buzzer_init();
  LOG_I("[Main] Factory reset button configured: GPIO%d", RESET_BUTTON_PIN);
  LOG_D("[Main] Button: short=status, double=AP, triple=blink, long=restart, vlong=reboot");

  // Configure Smart Sensing pins
  pinMode(AMPLIFIER_PIN, OUTPUT);
  digitalWrite(AMPLIFIER_PIN, LOW); // Start with amplifier OFF (fail-safe)
  LOG_I("[Main] Amplifier relay configured: GPIO%d", AMPLIFIER_PIN);

  // Initialize LittleFS and load settings BEFORE GUI so boot animation
  // settings are available when gui_init() runs.
  if (!LittleFS.begin(true)) {
    LOG_E("[Main] LittleFS initialization failed");
  } else {
    LOG_I("[Main] LittleFS initialized");
  }

  // Record reset reason to crash log ring buffer (persisted in LittleFS)
  String resetReason = getResetReasonString();
  crashlog_record(resetReason);
  if (crashlog_last_was_crash()) {
    LOG_W("[Main] *** CRASH DETECTED: previous reset was '%s' ***", resetReason.c_str());
  } else {
    LOG_I("[Main] Reset reason: %s", resetReason.c_str());
  }

  // Check if device just rebooted after successful OTA update
  appState.justUpdated =
      checkAndClearOTASuccessFlag(appState.previousFirmwareVersion);
  if (appState.justUpdated) {
    LOG_I("[Main] Firmware updated from %s to %s",
          appState.previousFirmwareVersion.c_str(), firmwareVer);
  }

  // Load persisted settings (e.g., auto-update preference)
  if (!loadSettings()) {
    LOG_I("[Main] No settings file found, using defaults");
  }

  // Apply debug serial log level from loaded settings
  applyDebugSerialLevel(appState.debugMode, appState.debugSerialLevel);

#ifdef GUI_ENABLED
  // Initialize TFT display + rotary encoder GUI (may play boot animation
  // using settings loaded above).
  gui_init();
#endif

  // Load Smart Sensing settings
  if (!loadSmartSensingSettings()) {
    LOG_I("[Main] No Smart Sensing settings found, using defaults");
  }

  // Initialize I2S audio ADC (PCM1808) — uses sample rate from loaded settings
  i2s_audio_init();

  // Load MQTT settings
  if (!loadMqttSettings()) {
    LOG_I("[Main] No MQTT settings found, using defaults");
  }

  // Load Signal Generator settings (always boots disabled)
  if (!loadSignalGenSettings()) {
    LOG_I("[Main] No signal generator settings found, using defaults");
  }

  // Load input channel names
  if (!loadInputNames()) {
    LOG_I("[Main] No input names found, using defaults");
  }

  // Initialize Signal Generator PWM
  siggen_init();

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
    LOG_W("[Main] 404 Not Found: %s %s",
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
  // Signal Generator API
  server.on("/api/signalgenerator", HTTP_GET, []() {
    if (!requireAuth())
      return;
    JsonDocument doc;
    doc["success"] = true;
    doc["enabled"] = appState.sigGenEnabled;
    const char *waveNames[] = {"sine", "square", "white_noise", "sweep"};
    doc["waveform"] = waveNames[appState.sigGenWaveform % 4];
    doc["frequency"] = appState.sigGenFrequency;
    doc["amplitude"] = appState.sigGenAmplitude;
    const char *chanNames[] = {"left", "right", "both"};
    doc["channel"] = chanNames[appState.sigGenChannel % 3];
    doc["outputMode"] = appState.sigGenOutputMode == 0 ? "software" : "pwm";
    doc["sweepSpeed"] = appState.sigGenSweepSpeed;
    const char *targetNames[] = {"input1", "input2", "both"};
    doc["targetAdc"] = targetNames[appState.sigGenTargetAdc % 3];
    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
  });
  server.on("/api/signalgenerator", HTTP_POST, []() {
    if (!requireAuth())
      return;
    if (!server.hasArg("plain")) {
      server.send(400, "application/json",
                  "{\"success\":false,\"message\":\"No data\"}");
      return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, server.arg("plain"))) {
      server.send(400, "application/json",
                  "{\"success\":false,\"message\":\"Invalid JSON\"}");
      return;
    }
    bool changed = false;
    if (doc["enabled"].is<bool>()) {
      appState.sigGenEnabled = doc["enabled"].as<bool>();
      changed = true;
    }
    if (doc["waveform"].is<String>()) {
      String w = doc["waveform"].as<String>();
      if (w == "sine") appState.sigGenWaveform = 0;
      else if (w == "square") appState.sigGenWaveform = 1;
      else if (w == "white_noise") appState.sigGenWaveform = 2;
      else if (w == "sweep") appState.sigGenWaveform = 3;
      changed = true;
    }
    if (doc["frequency"].is<float>()) {
      float f = doc["frequency"].as<float>();
      if (f >= 1.0f && f <= 22000.0f) { appState.sigGenFrequency = f; changed = true; }
    }
    if (doc["amplitude"].is<float>()) {
      float a = doc["amplitude"].as<float>();
      if (a >= -96.0f && a <= 0.0f) { appState.sigGenAmplitude = a; changed = true; }
    }
    if (doc["channel"].is<String>()) {
      String c = doc["channel"].as<String>();
      if (c == "left") appState.sigGenChannel = 0;
      else if (c == "right") appState.sigGenChannel = 1;
      else if (c == "both") appState.sigGenChannel = 2;
      changed = true;
    }
    if (doc["outputMode"].is<String>()) {
      String m = doc["outputMode"].as<String>();
      if (m == "software") appState.sigGenOutputMode = 0;
      else if (m == "pwm") appState.sigGenOutputMode = 1;
      changed = true;
    }
    if (doc["sweepSpeed"].is<float>()) {
      float s = doc["sweepSpeed"].as<float>();
      if (s >= 1.0f && s <= 22000.0f) { appState.sigGenSweepSpeed = s; changed = true; }
    }
    if (doc["targetAdc"].is<String>()) {
      String t = doc["targetAdc"].as<String>();
      if (t == "input1") appState.sigGenTargetAdc = 0;
      else if (t == "input2") appState.sigGenTargetAdc = 1;
      else if (t == "both") appState.sigGenTargetAdc = 2;
      changed = true;
    }
    if (changed) {
      siggen_apply_params();
      saveSignalGenSettings();
      appState.markSignalGenDirty();
    }
    server.send(200, "application/json", "{\"success\":true}");
  });
  // Input Names API
  server.on("/api/inputnames", HTTP_GET, []() {
    if (!requireAuth())
      return;
    JsonDocument doc;
    doc["success"] = true;
    JsonArray names = doc["names"].to<JsonArray>();
    for (int i = 0; i < NUM_AUDIO_ADCS * 2; i++) {
      names.add(appState.inputNames[i]);
    }
    doc["numAdcsDetected"] = appState.numAdcsDetected;
    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
  });
  server.on("/api/inputnames", HTTP_POST, []() {
    if (!requireAuth())
      return;
    if (!server.hasArg("plain")) {
      server.send(400, "application/json",
                  "{\"success\":false,\"message\":\"No data\"}");
      return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, server.arg("plain"))) {
      server.send(400, "application/json",
                  "{\"success\":false,\"message\":\"Invalid JSON\"}");
      return;
    }
    if (doc["names"].is<JsonArray>()) {
      JsonArray names = doc["names"].as<JsonArray>();
      for (int i = 0; i < NUM_AUDIO_ADCS * 2 && i < (int)names.size(); i++) {
        String name = names[i].as<String>();
        if (name.length() > 0) appState.inputNames[i] = name;
      }
      saveInputNames();
    }
    server.send(200, "application/json", "{\"success\":true}");
  });

  // Note: Certificate API routes removed - now using Mozilla certificate bundle

#ifdef DSP_ENABLED
  // Register DSP API endpoints and load persisted config
  registerDspApiEndpoints();
  loadDspSettings();
#endif

  // Initialize CPU usage monitoring
  initCpuUsageMonitoring();

  // Initialize WiFi event handler for automatic reconnection
  initWiFiEventHandler();

  // Migrate old WiFi credentials to new multi-WiFi system (one-time operation)
  migrateWiFiCredentials();

  // Try to connect to stored WiFi networks (tries all saved networks in
  // priority order)
  if (!connectToStoredNetworks()) {
    LOG_W("[Main] No WiFi connection established, running in AP mode");
  }

  // Always start server and WebSocket regardless of mode
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  DebugOut.setWebSocket(&webSocket);
  server.begin();
  LOG_I("[Main] Web server and WebSocket started");

  LOG_I("[Main] Free heap: %lu bytes, largest block: %lu bytes",
        (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMaxAllocHeap());

  // Initialize task monitor (loop timing + FreeRTOS task snapshots)
  task_monitor_init();

  // Set initial FSM state
  appState.setFSMState(STATE_IDLE);

  // Subscribe main loop to the Arduino-managed Task Watchdog Timer
  // Timeout is set to 15s via CONFIG_ESP_TASK_WDT_TIMEOUT_S build flag in platformio.ini
  // Do NOT call esp_task_wdt_init() — Arduino framework already initializes the WDT
  esp_task_wdt_add(NULL);          // Register main loop (loopTask)

  // Unsubscribe IDLE0 from WDT — audio_cap (priority 3) + OTA TLS crypto on Core 0
  // can starve IDLE0 for >15s. All important tasks (loopTask, audio_cap, gui_task)
  // have their own WDT entries and feed them explicitly.
  esp_task_wdt_delete(xTaskGetIdleTaskHandleForCPU(0));

  LOG_I("[Main] Main loop subscribed to task watchdog");
}

void loop() {
  esp_task_wdt_reset();  // Feed watchdog at top of every loop iteration
  task_monitor_loop_start();

  // Small delay to reduce CPU usage - allows other tasks to run
  // Without this, the loop runs as fast as possible (~49% CPU)
  delay(5);

  server.handleClient();
  if (appState.isAPMode) {
    dnsServer.processNextRequest();
  }
  webSocket.loop();
  mqttLoop();
  updateWiFiConnection();

  // Monitor WiFi and auto-reconnect (throttled to every 5 seconds)
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck >= 5000) {
    lastWiFiCheck = millis();
    checkWiFiConnection();
  }

  // Check WebSocket auth timeouts
  for (int i = 0; i < MAX_WS_CLIENTS; i++) {
    if (wsAuthTimeout[i] > 0 && millis() > wsAuthTimeout[i]) {
      if (!wsAuthStatus[i]) {
        LOG_W("[WebSocket] Client [%u] auth timeout", i);
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
      buzzer_play(BUZZ_BTN_SHORT);
      LOG_I("[Button] Short press");
#ifdef GUI_ENABLED
      gui_wake(); // Wake TFT screen on K0 short press
#endif
      LOG_D("[Button] WiFi: %s", WiFi.status() == WL_CONNECTED
                                    ? "Connected"
                                    : "Disconnected");
      LOG_D("[Button] AP Mode: %s",
            appState.isAPMode ? "Active" : "Inactive");
      LOG_D("[Button] LED Blinking: %s",
            appState.blinkingEnabled ? "Enabled" : "Disabled");
      LOG_D("[Button] Firmware: %s", firmwareVer);
      break;

    case BTN_DOUBLE_CLICK:
      buzzer_play(BUZZ_BTN_DOUBLE);
      LOG_I("[Button] Double click - toggle AP mode");
      if (appState.isAPMode) {
        stopAccessPoint();
      } else {
        startAccessPoint();
      }
      break;

    case BTN_TRIPLE_CLICK:
      buzzer_play(BUZZ_BTN_TRIPLE);
      LOG_I("[Button] Triple click - toggle LED blinking");
      appState.setBlinkingEnabled(!appState.blinkingEnabled);
      LOG_D("[Button] LED Blinking is now: %s",
            appState.blinkingEnabled ? "ON" : "OFF");
      sendBlinkingState();
      break;

    case BTN_LONG_PRESS:
      buzzer_play_blocking(BUZZ_BTN_LONG, 400);
      LOG_W("[Button] Long press - restarting ESP32");
      buzzer_play_blocking(BUZZ_SHUTDOWN, 1200);
      ESP.restart();
      break;

    case BTN_VERY_LONG_PRESS:
      buzzer_play_blocking(BUZZ_BTN_VERY_LONG, 700);
      LOG_W("[Button] Very long press - rebooting ESP32");
      sendRebootProgress(10, true);
      buzzer_play_blocking(BUZZ_SHUTDOWN, 1200);
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
          LOG_D("[Button] Held for %lu seconds", secondsHeld);

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

  // Periodic firmware check (every 5 minutes) - non-blocking via FreeRTOS task
  if (!appState.isAPMode && WiFi.status() == WL_CONNECTED &&
      !appState.otaInProgress && !isOTATaskRunning()) {
    unsigned long currentMillis = millis();
    if (currentMillis - appState.lastOTACheck >= OTA_CHECK_INTERVAL ||
        appState.lastOTACheck == 0) {
      appState.lastOTACheck = currentMillis;
      startOTACheckTask();
    }
  }

  // Auto-update logic (runs on every periodic check when update is available)
  // Will retry on next periodic check (5 min) if amplifier is in use
  if (appState.autoUpdateEnabled && appState.updateAvailable &&
      !appState.otaInProgress && !isOTATaskRunning() &&
      appState.updateDiscoveredTime > 0) {
    if (appState.amplifierState) {
      // Amplifier is ON - skip this check, will retry on next periodic check
      // Reset updateDiscoveredTime so countdown restarts when amp turns off
      LOG_W("[OTA] Auto-update skipped: amplifier is in use, will retry on next check");
      appState.updateDiscoveredTime = 0;
      appState.markOTADirty();
    } else {
      // Amplifier is OFF - safe to proceed with countdown
      unsigned long elapsed = millis() - appState.updateDiscoveredTime;

      // Broadcast countdown every second via dirty flag
      static unsigned long lastCountdownBroadcast = 0;
      if (millis() - lastCountdownBroadcast >= 1000) {
        lastCountdownBroadcast = millis();
        appState.markOTADirty();
      }

      if (elapsed >= AUTO_UPDATE_COUNTDOWN) {
        // Double-check amplifier state before starting update
        if (appState.amplifierState) {
          LOG_W("[OTA] Auto-update cancelled: amplifier turned on during countdown, will retry on next check");
          appState.updateDiscoveredTime = 0;
          appState.markOTADirty();
        } else {
          LOG_I("[OTA] Auto-update starting (amplifier is off)");
          startOTADownloadTask();
        }
      }
    }
  }

  // Smart Sensing logic update
  updateSmartSensingLogic();

  // Broadcast OTA status changes (OTA task -> WS clients)
  if (appState.isOTADirty()) {
    broadcastUpdateStatus();
    sendWiFiStatus();
    appState.clearOTADirty();
  }

  // Broadcast display state changes (GUI auto-sleep/wake -> WS clients + MQTT)
  if (appState.isDisplayDirty()) {
    sendDisplayState();
    appState.clearDisplayDirty();
  }

  // Broadcast buzzer state changes (GUI -> WS clients + MQTT)
  if (appState.isBuzzerDirty()) {
    sendBuzzerState();
    publishMqttBuzzerState();
    appState.clearBuzzerDirty();
  }

  // Broadcast signal generator state changes (GUI/API -> WS clients + MQTT)
  if (appState.isSignalGenDirty()) {
    sendSignalGenState();
    publishMqttSignalGenState();
    appState.clearSignalGenDirty();
  }

#ifdef DSP_ENABLED
  // Broadcast DSP config changes (API -> WS clients)
  if (appState.isDspConfigDirty()) {
    sendDspState();
    appState.clearDspConfigDirty();
  }

  // DSP metrics broadcast (1s interval when DSP is active)
  static unsigned long lastDspMetricsBroadcast = 0;
  if (appState.dspEnabled && millis() - lastDspMetricsBroadcast >= 1000) {
    lastDspMetricsBroadcast = millis();
    sendDspMetrics();
  }

  // Check for debounced DSP settings save
  dsp_check_debounced_save();
#endif

  // Broadcast blinking state changes (GUI -> WS clients)
  if (appState.isBlinkingDirty()) {
    sendBlinkingState();
    appState.clearBlinkingDirty();
  }

  // Broadcast settings changes (GUI -> WS clients + MQTT)
  if (appState.isSettingsDirty()) {
    sendWiFiStatus();
    sendMqttSettingsState();
    publishMqttSystemStatus();
    appState.clearSettingsDirty();
  }

  // Broadcast Smart Sensing state every second
  static unsigned long lastSmartSensingBroadcast = 0;
  if (millis() - lastSmartSensingBroadcast >= 1000) {
    lastSmartSensingBroadcast = millis();
    sendSmartSensingState();
  }

  // Task monitor snapshot (every 5 seconds, independent of HW stats broadcast)
  static unsigned long lastTaskMonUpdate = 0;
  if (appState.debugMode && appState.debugTaskMonitor) {
    if (millis() - lastTaskMonUpdate >= 5000) {
      lastTaskMonUpdate = millis();
      task_monitor_update();
    }
  }

  // Heap health monitor — detect fragmentation before OOM crash (every 30s)
  static unsigned long lastHeapCheck = 0;
  if (millis() - lastHeapCheck >= 30000) {
    lastHeapCheck = millis();
    uint32_t maxBlock = ESP.getMaxAllocHeap();
    bool wasCritical = appState.heapCritical;
    appState.heapCritical = (maxBlock < 40000);
    if (appState.heapCritical && !wasCritical) {
      LOG_W("[Main] HEAP CRITICAL: largest free block=%lu bytes (<40KB)", (unsigned long)maxBlock);
    } else if (!appState.heapCritical && wasCritical) {
      LOG_I("[Main] Heap recovered: largest free block=%lu bytes", (unsigned long)maxBlock);
    }
  }

  // Broadcast Hardware Stats periodically (user-configurable interval)
  static unsigned long lastHardwareStatsBroadcast = 0;
  if (millis() - lastHardwareStatsBroadcast >= appState.hardwareStatsInterval) {
    lastHardwareStatsBroadcast = millis();
    if (appState.debugMode && appState.debugHwStats) {
      sendHardwareStats();
    }
  }

  // Send audio waveform/spectrum data to subscribed WebSocket clients
  static unsigned long lastAudioSend = 0;
  if (millis() - lastAudioSend >= appState.audioUpdateRate) {
    lastAudioSend = millis();
    sendAudioData();
  }

  // IMPORTANT: blinking must NOT depend on isAPMode
  if (appState.blinkingEnabled) {
    unsigned long currentMillis = millis();
    if (currentMillis - appState.previousMillis >= LED_BLINK_INTERVAL) {
      appState.previousMillis = currentMillis;

      appState.setLedState(!appState.ledState);
      digitalWrite(LED_PIN, appState.ledState);

      // Don't broadcast every toggle — client animates locally from blinkingEnabled state.
      // Only sendLEDState() on explicit user actions (toggle blink on/off).
    }
  } else {
    if (appState.ledState) {
      appState.setLedState(false);
      digitalWrite(LED_PIN, LOW);
      sendLEDState();
      LOG_I("[Main] Blinking stopped - LED turned OFF");
    }
  }

  // Fallback buzzer processing (primary path is gui_task on Core 1)
  // Non-blocking mutex: skips if gui_task is already processing
  buzzer_update();

  task_monitor_loop_end();
}

// WiFi functions are defined in wifi_manager.h/.cpp
// Settings persistence functions are defined in settings_manager.h/.cpp
// Utility functions (compareVersions, etc.) are defined in utils.h/.cpp
