#include "app_state.h"
#include "globals.h"
#include "globals.h"
#include "auth_handler.h"
#include "diag_journal.h"
#include "diag_event.h"
#include "button_handler.h"
#include "buzzer_handler.h"
#include "config.h"
#include "crash_log.h"
#include "debug_serial.h"
#include "login_page.h"
#include "mqtt_handler.h"
#include "mqtt_task.h"
#include "app_events.h"
#include "ota_updater.h"
#include "settings_manager.h"
#include "signal_generator.h"
#include "task_monitor.h"
#include "i2s_audio.h"
#include "audio_pipeline.h"
#ifdef DSP_ENABLED
#include "dsp_api.h"
#include "dsp_pipeline.h"
#include "output_dsp.h"
#endif
#ifdef DAC_ENABLED
#include "dac_hal.h"
#include "dac_api.h"
#include "hal/hal_builtin_devices.h"
#include "hal/hal_device_db.h"
#include "hal/hal_device_manager.h"
#include "hal/hal_api.h"
#include "hal/hal_pipeline_bridge.h"
#include "hal/hal_audio_health_bridge.h"
#include "hal/hal_settings.h"
#include "hal/hal_ns4150b.h"
#include "hal/hal_temp_sensor.h"
#include "hal/hal_display.h"
#include "hal/hal_encoder.h"
#include "hal/hal_buzzer.h"
#include "hal/hal_led.h"
#include "hal/hal_relay.h"
#include "hal/hal_button.h"
#include "hal/hal_signal_gen.h"
#include "hal/hal_custom_device.h"
#include "drivers/es8311_regs.h"
#include "pipeline_api.h"
#endif
#ifdef USB_AUDIO_ENABLED
#include "usb_audio.h"
#endif
#include "smart_sensing.h"
#include "utils.h"
#include "web_pages.h"
#include "websocket_handler.h"
#include "wifi_manager.h"
#include "eth_manager.h"
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

#ifdef DAC_ENABLED
// ===== HAL device instances (registered during setup) =====
static HalNs4150b* _halNs4150b = nullptr;
static HalTempSensor* _halTempSensor = nullptr;
#ifdef GUI_ENABLED
static HalDisplay* _halDisplay = nullptr;
static HalEncoder* _halEncoder = nullptr;
#endif
static HalBuzzer* _halBuzzer = nullptr;
static HalLed* _halLed = nullptr;
static HalRelay* _halRelay = nullptr;
static HalButton* _halButton = nullptr;
static HalSignalGen* _halSignalGen = nullptr;
#endif

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

    appState.general.deviceSerialNumber = String(serial);

    // Store serial number and firmware version in NVS
    prefs.putString("serial", appState.general.deviceSerialNumber);
    prefs.putString("fw_ver", currentFwVer);

    LOG_I("[Main] Serial number generated: %s (firmware: %s)",
          appState.general.deviceSerialNumber.c_str(), currentFwVer.c_str());
  } else {
    // Load existing serial number
    appState.general.deviceSerialNumber = prefs.getString("serial", "");
    LOG_I("[Main] Serial number loaded: %s",
          appState.general.deviceSerialNumber.c_str());
  }

  prefs.end();
}

void setup() {
  DebugOut.begin(115200);
  delay(1000);

  // Reconfigure TWDT immediately — before WiFi, GUI, or audio tasks start.
  // The pre-built IDF5 Arduino-ESP32 library has CONFIG_ESP_TASK_WDT_TIMEOUT_S=5
  // baked in; the -D build flag has no effect on the compiled .a.
  // esp_task_wdt_reconfigure() before any tasks subscribe avoids "task not found"
  // errors. idle_core_mask=0 removes IDLE tasks from WDT monitoring without
  // corrupting the subscriber linked list (esp_task_wdt_delete causes list issues in IDF5.5).
  {
    esp_task_wdt_config_t twdt_cfg = {
      .timeout_ms    = 30000,
      .idle_core_mask = 0,
      .trigger_panic  = true,
    };
    esp_task_wdt_reconfigure(&twdt_cfg);
  }

  LOG_I("[Main] ALX Nova Controller starting (%s)", ESP.getChipModel());
  LOG_I("[Main] Firmware version: %s", firmwareVer);

  // Initialize device serial number from NVS (generates on first boot or
  // firmware update)
  initSerialNumber();

  // Set AP SSID to the device serial number (e.g., ALX-AABBCCDDEEFF)
  appState.wifi.apSSID = appState.general.deviceSerialNumber;
  LOG_I("[Main] AP SSID set to: %s", appState.wifi.apSSID.c_str());

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Configure factory reset button with enhanced detection
  resetButton.begin();
  // buzzer_init() removed — HalBuzzer::init() handles this via HAL lifecycle
  LOG_I("[Main] Factory reset button configured: GPIO%d", RESET_BUTTON_PIN);
  LOG_D("[Main] Button: short=status, double=AP, long=restart, vlong=reboot");

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

  // Initialize diagnostic journal (after LittleFS, before any diag_emit)
  diag_journal_init();

  // Record reset reason to crash log ring buffer (persisted in LittleFS)
  String resetReason = getResetReasonString();
  crashlog_record(resetReason);
  if (crashlog_last_was_crash()) {
    LOG_W("[Main] *** CRASH DETECTED: previous reset was '%s' ***", resetReason.c_str());
  } else {
    LOG_I("[Main] Reset reason: %s", resetReason.c_str());
  }

  // Rule 9: Boot Loop Detection — 3 consecutive crash boots → safe mode
  {
    const CrashLogData& cl = crashlog_get();
    int consecutiveCrashes = 0;
    for (int i = 0; i < cl.count && i < 3; i++) {
      const CrashLogEntry* e = crashlog_get_recent(i);
      if (e && crashlog_was_crash(e->reason)) {
        consecutiveCrashes++;
      } else {
        break;
      }
    }
    if (consecutiveCrashes >= 3) {
      LOG_W("[Main] BOOT LOOP DETECTED: %d consecutive crashes — entering safe mode", consecutiveCrashes);
      diag_emit(DIAG_SYS_BOOT_LOOP, DIAG_SEV_CRIT,
                0xFF, "System", "boot loop safe mode");
      appState.halSafeMode = true;
    }
  }

  // Check if device just rebooted after successful OTA update
  appState.ota.justUpdated =
      checkAndClearOTASuccessFlag(appState.ota.previousFirmwareVersion);
  if (appState.ota.justUpdated) {
    LOG_I("[Main] Firmware updated from %s to %s",
          appState.ota.previousFirmwareVersion.c_str(), firmwareVer);
  }

  // Load persisted settings (e.g., auto-update preference)
  if (!loadSettings()) {
    LOG_I("[Main] No settings file found, using defaults");
  }

  // Apply debug serial log level from loaded settings
  applyDebugSerialLevel(appState.debug.debugMode, appState.debug.serialLevel);

#ifdef USB_AUDIO_ENABLED
  // Initialize USB Audio (TinyUSB UAC2 speaker device on native USB port)
  // Must be called before WiFi since TinyUSB init happens here
  if (appState.usbAudio.enabled) {
    usb_audio_init();
  } else {
    LOG_I("[Main] USB Audio disabled in settings, skipping init");
  }
#endif

#ifdef GUI_ENABLED
  // Initialize TFT display + rotary encoder GUI (may play boot animation
  // using settings loaded above).
  gui_init();
#endif

  // Load Smart Sensing settings
  if (!loadSmartSensingSettings()) {
    LOG_I("[Main] No Smart Sensing settings found, using defaults");
  }

#ifdef DSP_ENABLED
  // Initialize output DSP engine (post-matrix per-channel processing)
  // Must be ready before audio_pipeline_init() so DSP config is loaded when pipeline starts
  output_dsp_init();
  output_dsp_load_all();
#endif

  // Initialize HAL framework (before audio/DAC init so drivers can register and
  // so i2s_configure_adc1() can query HAL config for pin overrides)
#ifdef DAC_ENABLED
  if (!appState.halSafeMode) {
    hal_register_builtins();
    hal_db_init();
    hal_load_device_configs();
    hal_load_custom_devices();
    hal_provision_defaults();    // Write /hal_auto_devices.json on first boot only
    hal_load_auto_devices();     // Instantiate add-on devices via HAL factory registry
  } else {
    LOG_W("[Main] HAL safe mode — skipping device registration");
  }
#endif

  // Initialize I2S audio ADC (PCM1808) — uses sample rate from loaded settings
  i2s_audio_init();

#ifdef DAC_ENABLED

  // Secondary DAC (ES8311) activation is now handled by hal_pipeline_bridge
  // when the device reaches HAL_STATE_AVAILABLE — removed legacy direct call

  // Sync HAL pipeline bridge after all devices registered
  hal_pipeline_sync();

  // Initialize audio health bridge (flap guard state)
  hal_audio_health_bridge_init();

  // Register NS4150B amplifier with HAL (ES8311 PA pin)
  _halNs4150b = new HalNs4150b(ES8311_PA_PIN);
  HalDeviceManager::instance().registerDevice(_halNs4150b, HAL_DISC_BUILTIN);
  _halNs4150b->probe();
  _halNs4150b->init();  // Reads gpioA config override, starts disabled (DAC readiness gates enable)

  // Register ESP32-P4 internal temperature sensor
#if CONFIG_IDF_TARGET_ESP32P4
  _halTempSensor = new HalTempSensor();
  HalDeviceManager::instance().registerDevice(_halTempSensor, HAL_DISC_BUILTIN);
#endif

  // Register peripheral devices
  {
    HalDeviceManager& mgr = HalDeviceManager::instance();
#ifdef GUI_ENABLED
    _halDisplay = new HalDisplay(TFT_MOSI_PIN, TFT_SCLK_PIN, TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN, TFT_BL_PIN);
    mgr.registerDevice(_halDisplay, HAL_DISC_BUILTIN);
    _halEncoder = new HalEncoder(ENCODER_A_PIN, ENCODER_B_PIN, ENCODER_SW_PIN);
    mgr.registerDevice(_halEncoder, HAL_DISC_BUILTIN);
    _halEncoder->probe();
    _halEncoder->init();  // Reads gpioA/B/C config overrides, claims pins
#endif
    _halBuzzer = new HalBuzzer(BUZZER_PIN);
    mgr.registerDevice(_halBuzzer, HAL_DISC_BUILTIN);
    _halBuzzer->probe();
    _halBuzzer->init();  // Calls buzzer_init(pin) via HAL lifecycle
    _halLed = new HalLed(LED_PIN);
    mgr.registerDevice(_halLed, HAL_DISC_BUILTIN);
    _halRelay = new HalRelay(AMPLIFIER_PIN);
    mgr.registerDevice(_halRelay, HAL_DISC_BUILTIN);
    _halButton = new HalButton(RESET_BUTTON_PIN);
    mgr.registerDevice(_halButton, HAL_DISC_BUILTIN);
    _halButton->probe();
    _halButton->init();  // Reads gpioA config override, claims pin
    // Update resetButton with resolved pin from HAL config
    resetButton.pin = _halButton->getPin();
    resetButton.begin();
    _halSignalGen = new HalSignalGen(SIGGEN_PWM_PIN);
    mgr.registerDevice(_halSignalGen, HAL_DISC_BUILTIN);
    _halSignalGen->probe();
    _halSignalGen->init();  // Reads gpioA config override, calls siggen_init(pin)
  }
#endif

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

    if (appState.wifi.isAPMode) {
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
    httpServingPage = true;
    if (!sendGzipped(server, loginPage_gz, loginPage_gz_len)) {
      server.send_P(200, "text/html", loginPage);
    }
    httpServingPage = false;
  });
  server.on("/api/auth/login", HTTP_POST, handleLogin);
  server.on("/api/auth/logout", HTTP_POST, handleLogout);
  server.on("/api/auth/status", HTTP_GET, handleAuthStatus);
  server.on("/api/auth/change", HTTP_POST, handlePasswordChange);
  server.on("/api/ws-token", HTTP_GET, []() {
    if (!requireAuth()) return;
    handleGetWsToken();
  });

  // Protected routes
  server.on("/", HTTP_GET, []() {
    if (!requireAuth())
      return;

    // Serve gzipped dashboard if client supports it (~85% smaller)
    httpServingPage = true;
    if (!sendGzipped(server, htmlPage_gz, htmlPage_gz_len)) {
      server.send_P(200, "text/html", htmlPage);
    }
    httpServingPage = false;
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
  server.on("/api/releases", HTTP_GET, []() {
    if (!requireAuth()) return;
    handleGetReleaseList();
  });
  server.on("/api/installrelease", HTTP_POST, []() {
    if (!requireAuth()) return;
    handleInstallRelease();
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
  // Diagnostic journal endpoints
  server.on("/api/diagnostics/journal", HTTP_GET, []() {
    if (!requireAuth()) return;
    JsonDocument doc;
    doc["type"] = "diagJournal";
    uint8_t count = diag_journal_count();
    JsonArray entries = doc["entries"].to<JsonArray>();
    for (uint8_t i = 0; i < count; i++) {
      DiagEvent ev;
      if (diag_journal_read(i, &ev)) {
        JsonObject e = entries.add<JsonObject>();
        e["seq"]   = ev.seq;
        e["boot"]  = ev.bootId;
        e["t"]     = ev.timestamp;
        e["heap"]  = ev.heapFree;
        char codeBuf[8];
        snprintf(codeBuf, sizeof(codeBuf), "0x%04X", ev.code);
        e["c"]     = codeBuf;
        e["corr"]  = ev.corrId;
        e["sub"]   = diag_subsystem_name(diag_subsystem_from_code((DiagErrorCode)ev.code));
        e["dev"]   = ev.device;
        e["slot"]  = ev.slot;
        e["msg"]   = ev.message;
        e["sev"]   = diag_severity_char((DiagSeverity)ev.severity);
        e["retry"] = ev.retryCount;
      }
    }
    doc["count"] = count;
    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
  });
  server.on("/api/diagnostics/journal", HTTP_DELETE, []() {
    if (!requireAuth()) return;
    diag_journal_clear();
    server.send(200, "application/json", "{\"success\":true}");
  });
  // Diagnostic snapshot — single JSON blob for AI-assisted debugging
  server.on("/api/diag/snapshot", HTTP_GET, []() {
    if (!requireAuth()) return;
    JsonDocument doc;
    doc["type"] = "diagSnapshot";
    doc["timestamp"] = millis();
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["freePsram"] = ESP.getFreePsram();
    doc["maxAllocHeap"] = ESP.getMaxAllocHeap();
    doc["fsmState"] = (int)appState.fsmState;
    // HAL devices
#ifdef DAC_ENABLED
    {
      JsonArray devs = doc["halDevices"].to<JsonArray>();
      HalDeviceManager::instance().forEach([](HalDevice* dev, void* ctx) {
        JsonArray* arr = static_cast<JsonArray*>(ctx);
        JsonObject d = arr->add<JsonObject>();
        d["slot"]       = dev->getSlot();
        d["name"]       = dev->getDescriptor().name;
        d["compatible"] = dev->getDescriptor().compatible;
        d["type"]       = (int)dev->getType();
        d["state"]      = (int)dev->_state;
        d["ready"]      = dev->_ready;
        const HalRetryState* rs = HalDeviceManager::instance().getRetryState(dev->getSlot());
        if (rs) {
          d["retries"]  = rs->count;
          d["lastErr"]  = rs->lastErrorCode;
        }
        d["faults"]     = HalDeviceManager::instance().getFaultCount(dev->getSlot());
      }, &devs);
    }
#endif
    // Recent diag events
    {
      JsonArray events = doc["recentEvents"].to<JsonArray>();
      uint8_t count = diag_journal_count();
      uint8_t limit = count < 10 ? count : 10;
      for (uint8_t i = 0; i < limit; i++) {
        DiagEvent ev;
        if (diag_journal_read(i, &ev)) {
          JsonObject e = events.add<JsonObject>();
          e["seq"]  = ev.seq;
          e["t"]    = ev.timestamp;
          char codeBuf[8];
          snprintf(codeBuf, sizeof(codeBuf), "0x%04X", ev.code);
          e["c"]    = codeBuf;
          e["dev"]  = ev.device;
          e["msg"]  = ev.message;
          e["sev"]  = diag_severity_char((DiagSeverity)ev.severity);
        }
      }
    }
    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
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
    doc["enabled"] = appState.sigGen.enabled;
    const char *waveNames[] = {"sine", "square", "white_noise", "sweep"};
    doc["waveform"] = waveNames[appState.sigGen.waveform % 4];
    doc["frequency"] = appState.sigGen.frequency;
    doc["amplitude"] = appState.sigGen.amplitude;
    const char *chanNames[] = {"left", "right", "both"};
    doc["channel"] = chanNames[appState.sigGen.channel % 3];
    doc["outputMode"] = appState.sigGen.outputMode == 0 ? "software" : "pwm";
    doc["sweepSpeed"] = appState.sigGen.sweepSpeed;
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
      appState.sigGen.enabled = doc["enabled"].as<bool>();
      changed = true;
    }
    if (doc["waveform"].is<String>()) {
      String w = doc["waveform"].as<String>();
      if (w == "sine") appState.sigGen.waveform = 0;
      else if (w == "square") appState.sigGen.waveform = 1;
      else if (w == "white_noise") appState.sigGen.waveform = 2;
      else if (w == "sweep") appState.sigGen.waveform = 3;
      changed = true;
    }
    if (doc["frequency"].is<float>()) {
      float f = doc["frequency"].as<float>();
      if (f >= 1.0f && f <= 22000.0f) { appState.sigGen.frequency = f; changed = true; }
    }
    if (doc["amplitude"].is<float>()) {
      float a = doc["amplitude"].as<float>();
      if (a >= -96.0f && a <= 0.0f) { appState.sigGen.amplitude = a; changed = true; }
    }
    if (doc["channel"].is<String>()) {
      String c = doc["channel"].as<String>();
      if (c == "left") appState.sigGen.channel = 0;
      else if (c == "right") appState.sigGen.channel = 1;
      else if (c == "both") appState.sigGen.channel = 2;
      changed = true;
    }
    if (doc["outputMode"].is<String>()) {
      String m = doc["outputMode"].as<String>();
      if (m == "software") appState.sigGen.outputMode = 0;
      else if (m == "pwm") appState.sigGen.outputMode = 1;
      changed = true;
    }
    if (doc["sweepSpeed"].is<float>()) {
      float s = doc["sweepSpeed"].as<float>();
      if (s >= 1.0f && s <= 22000.0f) { appState.sigGen.sweepSpeed = s; changed = true; }
    }
    if (changed) {
      siggen_apply_params();
      saveSignalGenSettings();
      appState.markSignalGenDirty();
    }
    server.send(200, "application/json", "{\"success\":true}");
  });
  // Audio Pipeline Matrix API
  server.on("/api/pipeline/matrix", HTTP_GET, []() {
    if (!requireAuth())
      return;
    JsonDocument doc;
    doc["success"] = true;
    doc["bypass"]  = audio_pipeline_is_matrix_bypass();
    doc["size"]    = AUDIO_PIPELINE_MATRIX_SIZE;
    JsonArray matrix = doc["matrix"].to<JsonArray>();
    for (int o = 0; o < AUDIO_PIPELINE_MATRIX_SIZE; o++) {
      JsonArray row = matrix.add<JsonArray>();
      for (int i = 0; i < AUDIO_PIPELINE_MATRIX_SIZE; i++) {
        row.add(audio_pipeline_get_matrix_gain(o, i));
      }
    }
    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
  });
  server.on("/api/pipeline/matrix", HTTP_PUT, []() {
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
    // Set bypass mode
    if (doc["bypass"].is<bool>()) {
      audio_pipeline_bypass_matrix(doc["bypass"].as<bool>());
    }
    // Set single cell (linear gain)
    if (doc["cell"].is<JsonObject>()) {
      int out_ch   = doc["cell"]["out"].as<int>();
      int in_ch    = doc["cell"]["in"].as<int>();
      float gain   = doc["cell"]["gain"].as<float>();
      audio_pipeline_set_matrix_gain(out_ch, in_ch, gain);
    }
    // Set single cell (dB gain)
    if (doc["cell_db"].is<JsonObject>()) {
      int out_ch     = doc["cell_db"]["out"].as<int>();
      int in_ch      = doc["cell_db"]["in"].as<int>();
      float gain_db  = doc["cell_db"]["gain_db"].as<float>();
      audio_pipeline_set_matrix_gain_db(out_ch, in_ch, gain_db);
    }
    // Set full matrix (array of 8 rows, each with 8 linear gain values)
    if (doc["matrix"].is<JsonArray>()) {
      JsonArray rows = doc["matrix"].as<JsonArray>();
      int o = 0;
      for (JsonArray row : rows) {
        if (o >= AUDIO_PIPELINE_MATRIX_SIZE) break;
        int i = 0;
        for (float gain : row) {
          if (i >= AUDIO_PIPELINE_MATRIX_SIZE) break;
          audio_pipeline_set_matrix_gain(o, i, gain);
          i++;
        }
        o++;
      }
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
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS * 2; i++) {
      names.add(appState.audio.inputNames[i]);
    }
    doc["numAdcsDetected"] = appState.audio.numAdcsDetected;
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
      for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS * 2 && i < (int)names.size(); i++) {
        String name = names[i].as<String>();
        if (name.length() > 0) appState.audio.inputNames[i] = name;
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
  // Sync pipeline bypass state with loaded dspEnabled flag.
  // Without this the pipeline processes DSP stages on boot even when disabled.
  audio_pipeline_bypass_dsp(0, !appState.dsp.enabled);
  audio_pipeline_bypass_dsp(1, !appState.dsp.enabled);
#endif

#ifdef DAC_ENABLED
  // Register DAC REST API endpoints
  registerDacApiEndpoints();
  registerHalApiEndpoints(server);
  registerPipelineApiEndpoints(server);

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

  // Disable WiFi modem power save AFTER WiFi is initialized.
  // Radio sleep/wake cycles cause 5-20ms latency spikes that generate
  // memory bus contention with I2S DMA, causing audio pops.
  wifi_ensure_ps_none();

  // Initialize Ethernet (P4 has onboard 100M Ethernet — primary network interface)
  eth_manager_init();

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

  // Initialize app event group (main loop wakes immediately on dirty flags)
  app_events_init();

  // Start dedicated MQTT task (owns connection, reconnect, publish)
  mqtt_task_init();

  // Set initial FSM state
  appState.setFSMState(STATE_IDLE);

  esp_task_wdt_add(NULL);  // Register main loop (loopTask)

  LOG_I("[Main] Main loop subscribed to task watchdog");
}

void loop() {
  esp_task_wdt_reset();  // Feed watchdog at top of every loop iteration
  task_monitor_loop_start();

  // Wake immediately on any dirty flag; fall back to 5ms tick when idle.
  // Replaces delay(5) — identical worst-case latency, zero latency on state changes.
  app_events_wait(5);

  server.handleClient();
  if (appState.wifi.isAPMode) {
    dnsServer.processNextRequest();
  }
  webSocket.loop();
  drainPendingInitState();
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
  if (!appState.general.factoryResetInProgress) {
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
            appState.wifi.isAPMode ? "Active" : "Inactive");
      LOG_D("[Button] Firmware: %s", firmwareVer);
      break;

    case BTN_DOUBLE_CLICK:
      buzzer_play(BUZZ_BTN_DOUBLE);
      LOG_I("[Button] Double click - toggle AP mode");
      if (appState.wifi.isAPMode) {
        stopAccessPoint();
      } else {
        startAccessPoint();
      }
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
      // Button not pressed - restore LED to low if it was lit for feedback
      static bool wasPressed = false;
      if (wasPressed) {
        digitalWrite(LED_PIN, LOW);
        wasPressed = false;
      }
      if (resetButton.isPressed()) {
        wasPressed = true;
      }
    }
  }

  // Periodic firmware check — backoff-aware interval (5min → 15min → 30min → 60min on failures)
  // Skip when heap is critical — TLS buffers need ~55KB and would worsen fragmentation
  bool networkReady = (WiFi.status() == WL_CONNECTED) || eth_manager_is_connected();
  if (!appState.wifi.isAPMode && networkReady &&
      !appState.ota.inProgress && !isOTATaskRunning() &&
      !appState.debug.heapCritical) {
    unsigned long currentMillis = millis();
    unsigned long effectiveInterval = getOTAEffectiveInterval();
    // Delay first OTA check 30s after boot to let Ethernet DHCP + DNS stabilize
    if (appState.ota.lastCheck == 0 && currentMillis < 30000) {
      // Too early — skip until network stack is stable
    } else if (currentMillis - appState.ota.lastCheck >= effectiveInterval ||
        appState.ota.lastCheck == 0) {
      appState.ota.lastCheck = currentMillis;
      startOTACheckTask();
    }
  }

  // Auto-update logic (runs on every periodic check when update is available)
  // Will retry on next periodic check (5 min) if amplifier is in use
  if (appState.ota.autoUpdateEnabled && appState.ota.updateAvailable &&
      !appState.ota.inProgress && !isOTATaskRunning() &&
      appState.ota.updateDiscoveredTime > 0) {
    if (appState.audio.amplifierState) {
      // Amplifier is ON - skip this check, will retry on next periodic check
      // Reset appState.ota.updateDiscoveredTime so countdown restarts when amp turns off
      LOG_W("[OTA] Auto-update skipped: amplifier is in use, will retry on next check");
      appState.ota.updateDiscoveredTime = 0;
      appState.markOTADirty();
    } else {
      // Amplifier is OFF - safe to proceed with countdown
      unsigned long elapsed = millis() - appState.ota.updateDiscoveredTime;

      // Broadcast countdown every second via dirty flag
      static unsigned long lastCountdownBroadcast = 0;
      if (millis() - lastCountdownBroadcast >= 1000) {
        lastCountdownBroadcast = millis();
        appState.markOTADirty();
      }

      if (elapsed >= AUTO_UPDATE_COUNTDOWN) {
        // Double-check amplifier state before starting update
        if (appState.audio.amplifierState) {
          LOG_W("[OTA] Auto-update cancelled: amplifier turned on during countdown, will retry on next check");
          appState.ota.updateDiscoveredTime = 0;
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
    appState.clearBuzzerDirty();
  }

  // Broadcast signal generator state changes (GUI/API -> WS clients + MQTT)
  if (appState.isSignalGenDirty()) {
    sendSignalGenState();
    appState.clearSignalGenDirty();
  }

#ifdef DSP_ENABLED
  // Broadcast DSP config changes (API/MQTT -> WS clients + MQTT)
  if (appState.isDspConfigDirty()) {
    sendDspState();
    appState.clearDspConfigDirty();
  }

  // DSP metrics broadcast (1s interval when DSP is active, one final 0% when disabled/bypassed)
  static unsigned long lastDspMetricsBroadcast = 0;
  static bool lastDspMetricsActive = false;
  bool dspActive = appState.dsp.enabled && !appState.dsp.bypass;
  if (millis() - lastDspMetricsBroadcast >= 1000) {
    if (dspActive || lastDspMetricsActive) {
      lastDspMetricsBroadcast = millis();
      sendDspMetrics();
    }
    lastDspMetricsActive = dspActive;
  }

  // Check for debounced DSP settings save
  dsp_check_debounced_save();
#endif

  // Execute pending AP toggle requested by mqttCallback()
  if (appState._pendingApToggle != 0) {
    int8_t toggle = appState._pendingApToggle;
    appState._pendingApToggle = 0;
    if (toggle > 0) {
      // Enable AP mode (add AP to existing station mode)
      WiFi.mode(WIFI_AP_STA);
      wifi_ensure_ps_none();
      WiFi.softAP(appState.wifi.apSSID.c_str(), appState.wifi.apPassword.c_str());
    } else {
      // Disable AP mode
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      wifi_ensure_ps_none();
    }
    appState.markSettingsDirty();
  }

  // Deferred settings saves (debounced from WebSocket handlers)
  checkDeferredSettingsSave();
  checkDeferredSmartSensingSave();
  checkDeferredSignalGenSave();
#ifdef DAC_ENABLED
  hal_check_deferred_save();
  pipeline_api_check_deferred_save();
#endif

#ifdef DAC_ENABLED
  // Process deferred device toggles — drain all pending entries per tick.
  // Toggle queue in HalCoordState supports multiple concurrent requests
  // with same-slot deduplication (replaces single-slot DacState.pendingToggle).
  if (appState.halCoord.hasPendingToggles()) {
    uint8_t count = appState.halCoord.pendingToggleCount();
    for (uint8_t i = 0; i < count; i++) {
      PendingDeviceToggle t = appState.halCoord.pendingToggleAt(i);
      if (t.halSlot >= 0xFF) continue;

      if (t.action > 0) {
        LOG_I("[HAL] Deferred device activation for HAL slot %u", t.halSlot);
        hal_pipeline_activate_device(t.halSlot);
        appState.markDacDirty();
      } else if (t.action < 0) {
        LOG_I("[HAL] Deferred device deactivation for HAL slot %u", t.halSlot);
        hal_pipeline_deactivate_device(t.halSlot);
        appState.markDacDirty();
      }
    }
    appState.halCoord.clearPendingToggles();
  }

  // Broadcast DAC state changes (WS/API/MQTT -> all clients)
  if (appState.isDacDirty()) {
    sendDacState();
    appState.clearDacDirty();
  }
  // Broadcast EEPROM state changes
  if (appState.isEepromDirty()) {
    sendDacState();  // dacState includes EEPROM diag data
    appState.clearEepromDirty();
  }
  // Broadcast HAL device state changes
  if (appState.isHalDeviceDirty()) {
    sendHalDeviceState();
    sendAudioChannelMap();  // Channel map depends on HAL device state
    appState.clearHalDeviceDirty();
  }
  // Broadcast audio channel map changes
  if (appState.isChannelMapDirty()) {
    sendAudioChannelMap();
    appState.clearChannelMapDirty();
  }
#endif

  // Broadcast Ethernet state changes (link up/down, IP acquired/lost)
  if (appState.isEthernetDirty()) {
    sendWiFiStatus();  // WiFi status payload already includes ETH fields
    appState.clearEthernetDirty();
  }

#ifdef USB_AUDIO_ENABLED
  // Poll USB connection state (~1s) — detects cable disconnect/reconnect
  {
    static unsigned long lastUsbPoll = 0;
    unsigned long nowMs = millis();
    if (appState.usbAudio.enabled && (nowMs - lastUsbPoll >= 1000)) {
      usb_audio_poll_connection();
      lastUsbPoll = nowMs;
    }
  }
  if (appState.isUsbAudioDirty()) {
    sendUsbAudioState();
    appState.clearUsbAudioDirty();
  }
  // USB Audio VU broadcast (rate-limited to match audioUpdateRate)
  {
    static unsigned long lastUsbVuBroadcast = 0;
    unsigned long nowMs = millis();
    if (appState.isUsbAudioVuDirty()
        && (nowMs - lastUsbVuBroadcast >= (unsigned long)appState.audio.updateRate)) {
      sendUsbAudioState();
      appState.clearUsbAudioVuDirty();
      lastUsbVuBroadcast = nowMs;
    }
  }
#endif

  // Broadcast settings changes (GUI -> WS clients + MQTT)
  if (appState.isSettingsDirty()) {
    sendWiFiStatus();
    sendMqttSettingsState();
    sendAudioGraphState();
    sendDebugState();
    appState.clearSettingsDirty();
  }

  // Broadcast diagnostic events (EVT_DIAG handler)
  if (appState.isDiagJournalDirty()) {
    sendDiagEvent();
    publishMqttDiagEvent();
    appState.clearDiagJournalDirty();
  }

  // Periodic journal flush (WARN+ entries to LittleFS, every 60s)
  {
    static unsigned long lastJournalFlush = 0;
    if (millis() - lastJournalFlush >= 60000) {
      diag_journal_flush();
      lastJournalFlush = millis();
    }
  }

  // Broadcast Smart Sensing state every second
  static unsigned long lastSmartSensingBroadcast = 0;
  if (millis() - lastSmartSensingBroadcast >= 1000) {
    lastSmartSensingBroadcast = millis();
    sendSmartSensingState();
  }

  // Task monitor snapshot (every 5 seconds, independent of HW stats broadcast)
  static unsigned long lastTaskMonUpdate = 0;
  if (appState.debug.debugMode && appState.debug.taskMonitor) {
    if (millis() - lastTaskMonUpdate >= 5000) {
      lastTaskMonUpdate = millis();
      task_monitor_update();
    }
  }

#ifdef DAC_ENABLED
  // HAL periodic health check (every 30s, aligned with heap check)
  static unsigned long lastHalHealthCheck = 0;
  if (millis() - lastHalHealthCheck >= 30000) {
    lastHalHealthCheck = millis();
    HalDeviceManager::instance().healthCheckAll();
  }

  // Audio → HAL health bridge (every 5s, aligned with audio_periodic_dump)
  {
    static unsigned long lastAudioHealthBridge = 0;
    if (millis() - lastAudioHealthBridge >= 5000) {
      lastAudioHealthBridge = millis();
      hal_audio_health_check();
    }
  }

  // Rule 8: Sustained Clipping — clipRate >1% for >5 consecutive checks (25s)
  {
    static uint8_t clipHighCount[AUDIO_PIPELINE_MAX_INPUTS] = {};
    static bool clipEmitted[AUDIO_PIPELINE_MAX_INPUTS] = {};
    static unsigned long lastClipCheck = 0;
    if (millis() - lastClipCheck >= 5000) {
      lastClipCheck = millis();
      for (uint8_t lane = 0; lane < AUDIO_PIPELINE_MAX_INPUTS; lane++) {
        if (appState.audio.adc[lane].clipRate > 0.01f) {
          clipHighCount[lane]++;
          if (clipHighCount[lane] > 5 && !clipEmitted[lane]) {
            clipEmitted[lane] = true;
            char msg[24];
            snprintf(msg, sizeof(msg), "ADC%u clip %.1f%%", lane, appState.audio.adc[lane].clipRate * 100.0f);
            diag_emit(DIAG_AUDIO_CLIPPING, DIAG_SEV_WARN,
                      0xFF, "Audio", msg);
          }
        } else {
          clipHighCount[lane] = 0;
          clipEmitted[lane] = false;
        }
      }
    }
  }
#endif

  // Rule 7: Loop Time Spike — loopTimeMaxUs > 50ms
  {
    static bool loopSpikeEmitted = false;
    const TaskMonitorData& tmd = task_monitor_get_data();
    if (tmd.loopTimeMaxUs > 50000) {
      if (!loopSpikeEmitted) {
        loopSpikeEmitted = true;
        char msg[24];
        snprintf(msg, sizeof(msg), "loop %lums", (unsigned long)(tmd.loopTimeMaxUs / 1000));
        diag_emit(DIAG_SYS_LOOP_TIME_SPIKE, DIAG_SEV_WARN,
                  0xFF, "System", msg);
      }
    } else {
      loopSpikeEmitted = false;
    }
  }

  // Heap health monitor — detect fragmentation before OOM crash (every 30s)
  static unsigned long lastHeapCheck = 0;
  if (millis() - lastHeapCheck >= 30000) {
    lastHeapCheck = millis();
    uint32_t maxBlock = ESP.getMaxAllocHeap();
    bool wasCritical = appState.debug.heapCritical;
    appState.debug.heapCritical = (maxBlock < 40000);
    if (appState.debug.heapCritical != wasCritical) {
      if (appState.debug.heapCritical) {
        LOG_W("[Main] HEAP CRITICAL: largest free block=%lu bytes (<40KB)", (unsigned long)maxBlock);
        diag_emit(DIAG_SYS_HEAP_CRITICAL, DIAG_SEV_WARN,
                  0xFF, "System", "heap critical");
      } else {
        LOG_I("[Main] Heap recovered: largest free block=%lu bytes", (unsigned long)maxBlock);
        diag_emit(DIAG_SYS_HEAP_RECOVERED, DIAG_SEV_INFO,
                  0xFF, "System", "heap recovered");
      }
      // heapCritical is included in next sendHardwareStats() cycle
    }
  }

  // Broadcast Hardware Stats periodically (user-configurable interval)
  // Stagger with audio data to avoid back-to-back large WebSocket sends
  static unsigned long lastHardwareStatsBroadcast = 0;
  bool hwStatsJustSent = false;
  if (millis() - lastHardwareStatsBroadcast >= appState.debug.hardwareStatsInterval) {
    lastHardwareStatsBroadcast = millis();
    if (appState.debug.debugMode) {
      sendHardwareStats();
      hwStatsJustSent = true;
    }
  }

  // Send audio waveform/spectrum data to subscribed WebSocket clients
  // Skip this iteration if hwStats just sent — prevents WiFi TX burst that starves I2S DMA
  static unsigned long lastAudioSend = 0;
  if (!hwStatsJustSent && millis() - lastAudioSend >= appState.audio.updateRate) {
    lastAudioSend = millis();
    sendAudioData();
  }

  // Flush periodic audio/DAC diagnostic logs from main loop context.
  // Skip if HW stats just sent — avoids piling Serial TX on top of a WiFi burst.
  if (!hwStatsJustSent) {
    audio_periodic_dump();
  }

  // Fallback buzzer processing (primary path is gui_task on Core 0)
  // Non-blocking mutex: skips if gui_task is already processing
  buzzer_update();

  task_monitor_loop_end();
}

// WiFi functions are defined in wifi_manager.h/.cpp
// Settings persistence functions are defined in settings_manager.h/.cpp
// Utility functions (compareVersions, etc.) are defined in utils.h/.cpp
