#include "settings_manager.h"
#include "app_state.h"
#include "buzzer_handler.h"
#include "config.h"
#include "crash_log.h"
#include "debug_serial.h"
#include "i2s_audio.h"
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
  String line13 = file.readStringUntil('\n');
  String line14 = file.readStringUntil('\n');
  String line15 = file.readStringUntil('\n');
  String line16 = file.readStringUntil('\n');
  String line17 = file.readStringUntil('\n');
  String line18 = file.readStringUntil('\n');
  String line19 = file.readStringUntil('\n');
  String line20 = file.readStringUntil('\n');
  String line21 = file.readStringUntil('\n');
  String line22 = file.readStringUntil('\n');
  String line23 = file.readStringUntil('\n');
  String line24 = file.readStringUntil('\n');
  String line25 = file.readStringUntil('\n');
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
    darkMode = (line4.toInt() != 0);
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

  // Load backlight brightness (if available, otherwise default to 255)
  if (line13.length() > 0) {
    line13.trim();
    int bright = line13.toInt();
    if (bright >= 1 && bright <= 255) {
      appState.backlightBrightness = (uint8_t)bright;
    }
  }

  // Load dim timeout (if available)
  // Legacy: dimVal==0 meant disabled; now handled by dimEnabled toggle
  if (line14.length() > 0) {
    line14.trim();
    unsigned long dimVal = line14.toInt();
    if (dimVal == 0) {
      // Legacy "disabled" value — keep default timeout, dimEnabled stays false
    } else if (dimVal == 5000 || dimVal == 10000 || dimVal == 15000 ||
               dimVal == 30000 || dimVal == 60000) {
      appState.dimTimeout = dimVal;
    }
  }

  // Load dim brightness (if available, otherwise default to 26 = 10%)
  if (line15.length() > 0) {
    line15.trim();
    int dimBright = line15.toInt();
    if (dimBright == 26 || dimBright == 64 || dimBright == 128 ||
        dimBright == 191) {
      appState.dimBrightness = (uint8_t)dimBright;
    }
  }

  // Load dim enabled (if available, otherwise default to false)
  if (line16.length() > 0) {
    line16.trim();
    appState.dimEnabled = (line16 == "1");
  }

  // Load audio update rate (if available, otherwise default to 50ms)
  if (line17.length() > 0) {
    line17.trim();
    int rate = line17.toInt();
    if (rate == 20 || rate == 33 || rate == 50 || rate == 100) {
      appState.audioUpdateRate = (uint16_t)rate;
    }
  }

  // Load audio graph toggles (if available, default to true)
  if (line18.length() > 0) {
    line18.trim();
    appState.vuMeterEnabled = (line18.toInt() != 0);
  }
  if (line19.length() > 0) {
    line19.trim();
    appState.waveformEnabled = (line19.toInt() != 0);
  }
  if (line20.length() > 0) {
    line20.trim();
    appState.spectrumEnabled = (line20.toInt() != 0);
  }

  // Load debug mode toggles (lines 21-25)
  if (line21.length() > 0) {
    line21.trim();
    appState.debugMode = (line21.toInt() != 0);
  }
  if (line22.length() > 0) {
    line22.trim();
    int level = line22.toInt();
    if (level >= 0 && level <= 3) {
      appState.debugSerialLevel = level;
    }
  }
  if (line23.length() > 0) {
    line23.trim();
    appState.debugHwStats = (line23.toInt() != 0);
  }
  if (line24.length() > 0) {
    line24.trim();
    appState.debugI2sMetrics = (line24.toInt() != 0);
  }
  if (line25.length() > 0) {
    line25.trim();
    appState.debugTaskMonitor = (line25.toInt() != 0);
  }

  return true;
}

void saveSettings() {
  File file = LittleFS.open("/settings.txt", "w");
  if (!file) {
    LOG_E("[Settings] Failed to open settings file for writing");
    return;
  }

  file.println(autoUpdateEnabled ? "1" : "0");
  file.println(timezoneOffset);
  file.println(dstOffset);
  file.println(darkMode ? "1" : "0");
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
  file.println(appState.backlightBrightness);
  file.println(appState.dimTimeout);
  file.println(appState.dimBrightness);
  file.println(appState.dimEnabled ? "1" : "0");
  file.println(appState.audioUpdateRate);
  file.println(appState.vuMeterEnabled ? "1" : "0");
  file.println(appState.waveformEnabled ? "1" : "0");
  file.println(appState.spectrumEnabled ? "1" : "0");
  file.println(appState.debugMode ? "1" : "0");
  file.println(appState.debugSerialLevel);
  file.println(appState.debugHwStats ? "1" : "0");
  file.println(appState.debugI2sMetrics ? "1" : "0");
  file.println(appState.debugTaskMonitor ? "1" : "0");
  file.close();
  LOG_I("[Settings] Settings saved to LittleFS");
}

// ===== Signal Generator Settings =====

bool loadSignalGenSettings() {
  File file = LittleFS.open("/siggen.txt", "r", true);
  if (!file || file.size() == 0) {
    if (file)
      file.close();
    return false;
  }

  String line1 = file.readStringUntil('\n'); // waveform
  String line2 = file.readStringUntil('\n'); // frequency
  String line3 = file.readStringUntil('\n'); // amplitude
  String line4 = file.readStringUntil('\n'); // channel
  String line5 = file.readStringUntil('\n'); // outputMode
  String line6 = file.readStringUntil('\n'); // sweepSpeed
  String line7 = file.readStringUntil('\n'); // targetAdc (added for dual ADC)
  file.close();

  line1.trim();
  line2.trim();
  line3.trim();
  line4.trim();
  line5.trim();
  line6.trim();
  line7.trim();

  if (line1.length() > 0) {
    int wf = line1.toInt();
    if (wf >= 0 && wf <= 3) appState.sigGenWaveform = wf;
  }
  if (line2.length() > 0) {
    float freq = line2.toFloat();
    if (freq >= 1.0f && freq <= 22000.0f) appState.sigGenFrequency = freq;
  }
  if (line3.length() > 0) {
    float amp = line3.toFloat();
    if (amp >= -96.0f && amp <= 0.0f) appState.sigGenAmplitude = amp;
  }
  if (line4.length() > 0) {
    int ch = line4.toInt();
    if (ch >= 0 && ch <= 2) appState.sigGenChannel = ch;
  }
  if (line5.length() > 0) {
    int mode = line5.toInt();
    if (mode >= 0 && mode <= 1) appState.sigGenOutputMode = mode;
  }
  if (line6.length() > 0) {
    float speed = line6.toFloat();
    if (speed >= 1.0f && speed <= 22000.0f) appState.sigGenSweepSpeed = speed;
  }
  if (line7.length() > 0) {
    int target = line7.toInt();
    if (target >= 0 && target <= 2) appState.sigGenTargetAdc = target;
  }

  // Always boot disabled regardless of saved state
  appState.sigGenEnabled = false;

  LOG_I("[Settings] Signal generator settings loaded");
  return true;
}

void saveSignalGenSettings() {
  File file = LittleFS.open("/siggen.txt", "w");
  if (!file) {
    LOG_E("[Settings] Failed to open siggen settings file for writing");
    return;
  }

  file.println(appState.sigGenWaveform);
  file.println(String(appState.sigGenFrequency, 1));
  file.println(String(appState.sigGenAmplitude, 1));
  file.println(appState.sigGenChannel);
  file.println(appState.sigGenOutputMode);
  file.println(String(appState.sigGenSweepSpeed, 1));
  file.println(appState.sigGenTargetAdc);
  file.close();
  LOG_I("[Settings] Signal generator settings saved");
}

// ===== Input Names Settings =====

static const char *INPUT_NAME_DEFAULTS[] = {"Subwoofer 1", "Subwoofer 2",
                                             "Subwoofer 3", "Subwoofer 4"};

bool loadInputNames() {
  File file = LittleFS.open("/inputnames.txt", "r", true);
  if (!file || file.size() == 0) {
    if (file)
      file.close();
    // Set defaults
    for (int i = 0; i < NUM_AUDIO_ADCS * 2; i++) {
      appState.inputNames[i] = INPUT_NAME_DEFAULTS[i];
    }
    return false;
  }

  for (int i = 0; i < NUM_AUDIO_ADCS * 2; i++) {
    String line = file.readStringUntil('\n');
    line.trim();
    appState.inputNames[i] =
        (line.length() > 0) ? line : String(INPUT_NAME_DEFAULTS[i]);
  }
  file.close();
  LOG_I("[Settings] Input names loaded");
  return true;
}

void saveInputNames() {
  File file = LittleFS.open("/inputnames.txt", "w");
  if (!file) {
    LOG_E("[Settings] Failed to open input names file for writing");
    return;
  }

  for (int i = 0; i < NUM_AUDIO_ADCS * 2; i++) {
    file.println(appState.inputNames[i]);
  }
  file.close();
  LOG_I("[Settings] Input names saved");
}

// ===== Factory Reset =====

void performFactoryReset() {
  LOG_W("[Settings] Factory reset initiated");
  factoryResetInProgress = true;

  // Visual feedback: solid LED
  digitalWrite(LED_PIN, HIGH);

  // 1. Clear WiFi settings from NVS
  LOG_I("[Settings] Clearing WiFi credentials (NVS: wifi-list)");
  Preferences wifiPrefs;
  wifiPrefs.begin("wifi-list", false);
  wifiPrefs.clear();
  wifiPrefs.end();

  // 2. Clear Auth settings from NVS
  LOG_I("[Settings] Clearing Auth settings (NVS: auth)");
  Preferences authPrefs;
  authPrefs.begin("auth", false);
  authPrefs.clear();
  authPrefs.end();

  // 3. Format LittleFS (erases /settings.txt, /mqtt_config.txt,
  // /smartsensing.txt, etc.)
  LOG_I("[Settings] Formatting LittleFS");
  if (LittleFS.format()) {
    LOG_I("[Settings] LittleFS formatted successfully");
  } else {
    LOG_E("[Settings] LittleFS format failed");
  }

  // End LittleFS
  LittleFS.end();

  // 4. Force AP Mode defaults for next boot
  apEnabled = true;
  isAPMode = true;

  LOG_W("[Settings] Factory reset complete");

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

  LOG_W("[Settings] Rebooting in 2 seconds");

  // Broadcast rebooting message
  JsonDocument rebootDoc;
  rebootDoc["type"] = "factoryResetStatus";
  rebootDoc["status"] = "rebooting";
  rebootDoc["message"] = "Rebooting device...";
  String rebootJson;
  serializeJson(rebootDoc, rebootJson);
  webSocket.broadcastTXT((uint8_t *)rebootJson.c_str(), rebootJson.length());
  webSocket.loop(); // Ensure message is sent

  buzzer_play_blocking(BUZZ_SHUTDOWN, 1200);
  ESP.restart();
}

// ===== Settings HTTP API Handlers =====

void handleSettingsGet() {
  JsonDocument doc;
  doc["success"] = true;
  doc["autoUpdateEnabled"] = autoUpdateEnabled;
  doc["timezoneOffset"] = timezoneOffset;
  doc["dstOffset"] = dstOffset;
  doc["darkMode"] = darkMode;
  doc["enableCertValidation"] = enableCertValidation;
  doc["autoAPEnabled"] = autoAPEnabled;
  doc["hardwareStatsInterval"] =
      hardwareStatsInterval / 1000; // Send as seconds
  doc["audioUpdateRate"] = appState.audioUpdateRate;
  doc["screenTimeout"] = appState.screenTimeout / 1000; // Send as seconds
  doc["backlightOn"] = appState.backlightOn;
  doc["buzzerEnabled"] = appState.buzzerEnabled;
  doc["buzzerVolume"] = appState.buzzerVolume;
  doc["backlightBrightness"] = appState.backlightBrightness;
  doc["dimEnabled"] = appState.dimEnabled;
  doc["dimTimeout"] = appState.dimTimeout / 1000;
  doc["dimBrightness"] = appState.dimBrightness;
  doc["debugMode"] = appState.debugMode;
  doc["debugSerialLevel"] = appState.debugSerialLevel;
  doc["debugHwStats"] = appState.debugHwStats;
  doc["debugI2sMetrics"] = appState.debugI2sMetrics;
  doc["debugTaskMonitor"] = appState.debugTaskMonitor;
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

  if (doc["darkMode"].is<bool>()) {
    bool newDarkMode = doc["darkMode"].as<bool>();
    if (newDarkMode != darkMode) {
      darkMode = newDarkMode;
      settingsChanged = true;
    }
  }

  if (doc["enableCertValidation"].is<bool>()) {
    bool newCertValidation = doc["enableCertValidation"].as<bool>();
    if (newCertValidation != enableCertValidation) {
      enableCertValidation = newCertValidation;
      settingsChanged = true;
      LOG_I("[Settings] Certificate validation %s",
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
        LOG_I("[Settings] Hardware stats interval set to %d seconds",
              newInterval);
      }
    }
  }

  if (doc["audioUpdateRate"].is<int>()) {
    int newRate = doc["audioUpdateRate"].as<int>();
    if (newRate == 20 || newRate == 33 || newRate == 50 || newRate == 100) {
      if ((uint16_t)newRate != appState.audioUpdateRate) {
        appState.audioUpdateRate = (uint16_t)newRate;
        settingsChanged = true;
        LOG_I("[Settings] Audio update rate set to %d ms", newRate);
      }
    }
  }

  if (doc["autoAPEnabled"].is<bool>()) {
    bool newAutoAP = doc["autoAPEnabled"].as<bool>();
    if (newAutoAP != autoAPEnabled) {
      autoAPEnabled = newAutoAP;
      settingsChanged = true;
      LOG_I("[Settings] Auto AP %s", autoAPEnabled ? "enabled" : "disabled");
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
        LOG_I("[Settings] Screen timeout set to %d seconds", newTimeoutSec);
      }
    }
  }

  if (doc["backlightOn"].is<bool>()) {
    bool newBacklight = doc["backlightOn"].as<bool>();
    if (newBacklight != appState.backlightOn) {
      appState.setBacklightOn(newBacklight);
      LOG_I("[Settings] Backlight set to %s", newBacklight ? "ON" : "OFF");
    }
    // backlightOn is runtime only, no save
  }

  if (doc["buzzerEnabled"].is<bool>()) {
    bool newBuzzer = doc["buzzerEnabled"].as<bool>();
    if (newBuzzer != appState.buzzerEnabled) {
      appState.setBuzzerEnabled(newBuzzer);
      settingsChanged = true;
      LOG_I("[Settings] Buzzer %s", newBuzzer ? "enabled" : "disabled");
    }
  }

  if (doc["buzzerVolume"].is<int>()) {
    int newVol = doc["buzzerVolume"].as<int>();
    if (newVol >= 0 && newVol <= 2 && newVol != appState.buzzerVolume) {
      appState.setBuzzerVolume(newVol);
      settingsChanged = true;
      LOG_I("[Settings] Buzzer volume set to %d", newVol);
    }
  }

  if (doc["backlightBrightness"].is<int>()) {
    int newBright = doc["backlightBrightness"].as<int>();
    if (newBright >= 1 && newBright <= 255 &&
        (uint8_t)newBright != appState.backlightBrightness) {
      appState.setBacklightBrightness((uint8_t)newBright);
      settingsChanged = true;
      LOG_I("[Settings] Backlight brightness set to %d", newBright);
    }
  }

  if (doc["dimEnabled"].is<bool>()) {
    bool newDimEnabled = doc["dimEnabled"].as<bool>();
    if (newDimEnabled != appState.dimEnabled) {
      appState.setDimEnabled(newDimEnabled);
      settingsChanged = true;
      LOG_I("[Settings] Dim %s", newDimEnabled ? "enabled" : "disabled");
    }
  }

  if (doc["dimTimeout"].is<int>()) {
    int newDimSec = doc["dimTimeout"].as<int>();
    unsigned long newDimMs = (unsigned long)newDimSec * 1000UL;
    if (newDimMs == 5000 || newDimMs == 10000 ||
        newDimMs == 15000 || newDimMs == 30000 || newDimMs == 60000) {
      if (newDimMs != appState.dimTimeout) {
        appState.setDimTimeout(newDimMs);
        settingsChanged = true;
      }
    }
  }

  if (doc["dimBrightness"].is<int>()) {
    int newDimBright = doc["dimBrightness"].as<int>();
    if ((newDimBright == 26 || newDimBright == 64 || newDimBright == 128 ||
         newDimBright == 191) &&
        (uint8_t)newDimBright != appState.dimBrightness) {
      appState.setDimBrightness((uint8_t)newDimBright);
      settingsChanged = true;
      LOG_I("[Settings] Dim brightness set to %d", newDimBright);
    }
  }

  if (doc["debugMode"].is<bool>()) {
    bool newVal = doc["debugMode"].as<bool>();
    if (newVal != appState.debugMode) {
      appState.debugMode = newVal;
      applyDebugSerialLevel(appState.debugMode, appState.debugSerialLevel);
      settingsChanged = true;
    }
  }
  if (doc["debugSerialLevel"].is<int>()) {
    int newLevel = doc["debugSerialLevel"].as<int>();
    if (newLevel >= 0 && newLevel <= 3 && newLevel != appState.debugSerialLevel) {
      appState.debugSerialLevel = newLevel;
      applyDebugSerialLevel(appState.debugMode, appState.debugSerialLevel);
      settingsChanged = true;
    }
  }
  if (doc["debugHwStats"].is<bool>()) {
    bool newVal = doc["debugHwStats"].as<bool>();
    if (newVal != appState.debugHwStats) {
      appState.debugHwStats = newVal;
      settingsChanged = true;
    }
  }
  if (doc["debugI2sMetrics"].is<bool>()) {
    bool newVal = doc["debugI2sMetrics"].as<bool>();
    if (newVal != appState.debugI2sMetrics) {
      appState.debugI2sMetrics = newVal;
      settingsChanged = true;
    }
  }
  if (doc["debugTaskMonitor"].is<bool>()) {
    bool newVal = doc["debugTaskMonitor"].as<bool>();
    if (newVal != appState.debugTaskMonitor) {
      appState.debugTaskMonitor = newVal;
      settingsChanged = true;
    }
  }

#ifdef GUI_ENABLED
  if (doc["bootAnimEnabled"].is<bool>()) {
    bool newBootAnim = doc["bootAnimEnabled"].as<bool>();
    if (newBootAnim != appState.bootAnimEnabled) {
      appState.bootAnimEnabled = newBootAnim;
      settingsChanged = true;
      LOG_I("[Settings] Boot animation %s",
            appState.bootAnimEnabled ? "enabled" : "disabled");
    }
  }

  if (doc["bootAnimStyle"].is<int>()) {
    int newStyle = doc["bootAnimStyle"].as<int>();
    if (newStyle >= 0 && newStyle <= 5 &&
        newStyle != appState.bootAnimStyle) {
      appState.bootAnimStyle = newStyle;
      settingsChanged = true;
      LOG_I("[Settings] Boot animation style set to %d",
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
  resp["darkMode"] = darkMode;
  resp["enableCertValidation"] = enableCertValidation;
  resp["autoAPEnabled"] = autoAPEnabled;
  resp["hardwareStatsInterval"] = hardwareStatsInterval / 1000;
  resp["audioUpdateRate"] = appState.audioUpdateRate;
  resp["screenTimeout"] = appState.screenTimeout / 1000;
  resp["backlightOn"] = appState.backlightOn;
  resp["buzzerEnabled"] = appState.buzzerEnabled;
  resp["buzzerVolume"] = appState.buzzerVolume;
  resp["backlightBrightness"] = appState.backlightBrightness;
  resp["dimEnabled"] = appState.dimEnabled;
  resp["dimTimeout"] = appState.dimTimeout / 1000;
  resp["dimBrightness"] = appState.dimBrightness;
  resp["debugMode"] = appState.debugMode;
  resp["debugSerialLevel"] = appState.debugSerialLevel;
  resp["debugHwStats"] = appState.debugHwStats;
  resp["debugI2sMetrics"] = appState.debugI2sMetrics;
  resp["debugTaskMonitor"] = appState.debugTaskMonitor;
#ifdef GUI_ENABLED
  resp["bootAnimEnabled"] = appState.bootAnimEnabled;
  resp["bootAnimStyle"] = appState.bootAnimStyle;
#endif
  String json;
  serializeJson(resp, json);
  server.send(200, "application/json", json);
}

void handleSettingsExport() {
  LOG_I("[Settings] Settings export requested via web interface");

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
  doc["settings"]["darkMode"] = darkMode;
  doc["settings"]["enableCertValidation"] = enableCertValidation;
  doc["settings"]["blinkingEnabled"] = blinkingEnabled;
  doc["settings"]["hardwareStatsInterval"] = hardwareStatsInterval / 1000;
  doc["settings"]["audioUpdateRate"] = appState.audioUpdateRate;
  doc["settings"]["screenTimeout"] = appState.screenTimeout / 1000;
  doc["settings"]["buzzerEnabled"] = appState.buzzerEnabled;
  doc["settings"]["buzzerVolume"] = appState.buzzerVolume;
  doc["settings"]["backlightBrightness"] = appState.backlightBrightness;
  doc["settings"]["dimEnabled"] = appState.dimEnabled;
  doc["settings"]["dimTimeout"] = appState.dimTimeout / 1000;
  doc["settings"]["dimBrightness"] = appState.dimBrightness;
  doc["settings"]["vuMeterEnabled"] = appState.vuMeterEnabled;
  doc["settings"]["waveformEnabled"] = appState.waveformEnabled;
  doc["settings"]["spectrumEnabled"] = appState.spectrumEnabled;
  doc["settings"]["debugMode"] = appState.debugMode;
  doc["settings"]["debugSerialLevel"] = appState.debugSerialLevel;
  doc["settings"]["debugHwStats"] = appState.debugHwStats;
  doc["settings"]["debugI2sMetrics"] = appState.debugI2sMetrics;
  doc["settings"]["debugTaskMonitor"] = appState.debugTaskMonitor;
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
  doc["smartSensing"]["audioThreshold"] = audioThreshold_dBFS;
  doc["smartSensing"]["adcVref"] = appState.adcVref;

  // Signal Generator settings
  doc["signalGenerator"]["waveform"] = appState.sigGenWaveform;
  doc["signalGenerator"]["frequency"] = appState.sigGenFrequency;
  doc["signalGenerator"]["amplitude"] = appState.sigGenAmplitude;
  doc["signalGenerator"]["channel"] = appState.sigGenChannel;
  doc["signalGenerator"]["outputMode"] = appState.sigGenOutputMode;
  doc["signalGenerator"]["sweepSpeed"] = appState.sigGenSweepSpeed;
  doc["signalGenerator"]["targetAdc"] = appState.sigGenTargetAdc;

  // Input channel names
  JsonArray names = doc["inputNames"].to<JsonArray>();
  for (int i = 0; i < NUM_AUDIO_ADCS * 2; i++) {
    names.add(appState.inputNames[i]);
  }

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

  LOG_I("[Settings] Settings exported successfully");
}

void handleSettingsImport() {
  LOG_I("[Settings] Settings import requested via web interface");

  if (!server.hasArg("plain")) {
    server.send(400, "application/json",
                "{\"success\": false, \"message\": \"No data received\"}");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    LOG_E("[Settings] JSON parsing failed: %s", error.c_str());
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

  LOG_I("[Settings] Importing settings");

  // Import WiFi settings
  if (!doc["wifi"].isNull()) {
    if (doc["wifi"]["ssid"].is<String>()) {
      wifiSSID = doc["wifi"]["ssid"].as<String>();
      LOG_D("[Settings] WiFi SSID: %s", wifiSSID.c_str());
    }
    if (doc["wifi"]["password"].is<String>()) {
      wifiPassword = doc["wifi"]["password"].as<String>();
      LOG_D("[Settings] WiFi password imported");
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
      LOG_D("[Settings] AP Enabled: %s", apEnabled ? "true" : "false");
    }
    if (doc["accessPoint"]["ssid"].is<String>()) {
      apSSID = doc["accessPoint"]["ssid"].as<String>();
      LOG_D("[Settings] AP SSID: %s", apSSID.c_str());
    }
    if (doc["accessPoint"]["password"].is<String>()) {
      apPassword = doc["accessPoint"]["password"].as<String>();
      LOG_D("[Settings] AP password imported");
    }
    if (doc["accessPoint"]["autoAPEnabled"].is<bool>()) {
      autoAPEnabled = doc["accessPoint"]["autoAPEnabled"].as<bool>();
      LOG_D("[Settings] Auto AP: %s", autoAPEnabled ? "enabled" : "disabled");
    }
  }

  // Import general settings
  if (!doc["settings"].isNull()) {
    if (doc["settings"]["autoUpdateEnabled"].is<bool>()) {
      autoUpdateEnabled = doc["settings"]["autoUpdateEnabled"].as<bool>();
      LOG_D("[Settings] Auto Update: %s",
            autoUpdateEnabled ? "enabled" : "disabled");
    }
    if (doc["settings"]["timezoneOffset"].is<int>()) {
      timezoneOffset = doc["settings"]["timezoneOffset"].as<int>();
      LOG_D("[Settings] Timezone Offset: %d", timezoneOffset);
    }
    if (doc["settings"]["dstOffset"].is<int>()) {
      dstOffset = doc["settings"]["dstOffset"].as<int>();
      LOG_D("[Settings] DST Offset: %d", dstOffset);
    }
    if (doc["settings"]["darkMode"].is<bool>()) {
      darkMode = doc["settings"]["darkMode"].as<bool>();
      LOG_D("[Settings] Dark Mode: %s", darkMode ? "enabled" : "disabled");
    }
    if (doc["settings"]["enableCertValidation"].is<bool>()) {
      enableCertValidation = doc["settings"]["enableCertValidation"].as<bool>();
      LOG_D("[Settings] Cert Validation: %s",
            enableCertValidation ? "enabled" : "disabled");
    }
    if (doc["settings"]["blinkingEnabled"].is<bool>()) {
      blinkingEnabled = doc["settings"]["blinkingEnabled"].as<bool>();
      LOG_D("[Settings] Blinking: %s",
            blinkingEnabled ? "enabled" : "disabled");
    }
    if (doc["settings"]["hardwareStatsInterval"].is<int>()) {
      int interval = doc["settings"]["hardwareStatsInterval"].as<int>();
      if (interval == 1 || interval == 2 || interval == 3 || interval == 5 ||
          interval == 10) {
        hardwareStatsInterval = interval * 1000UL;
        LOG_D("[Settings] Hardware Stats Interval: %d seconds", interval);
      }
    }
    if (doc["settings"]["audioUpdateRate"].is<int>()) {
      int rate = doc["settings"]["audioUpdateRate"].as<int>();
      if (rate == 20 || rate == 33 || rate == 50 || rate == 100) {
        appState.audioUpdateRate = (uint16_t)rate;
        LOG_D("[Settings] Audio Update Rate: %d ms", rate);
      }
    }
    if (doc["settings"]["screenTimeout"].is<int>()) {
      int timeoutSec = doc["settings"]["screenTimeout"].as<int>();
      unsigned long timeoutMs = (unsigned long)timeoutSec * 1000UL;
      if (timeoutMs == 0 || timeoutMs == 30000 || timeoutMs == 60000 ||
          timeoutMs == 300000 || timeoutMs == 600000) {
        appState.screenTimeout = timeoutMs;
        LOG_D("[Settings] Screen Timeout: %d seconds", timeoutSec);
      }
    }
    if (doc["settings"]["buzzerEnabled"].is<bool>()) {
      appState.buzzerEnabled = doc["settings"]["buzzerEnabled"].as<bool>();
      LOG_D("[Settings] Buzzer: %s",
            appState.buzzerEnabled ? "enabled" : "disabled");
    }
    if (doc["settings"]["buzzerVolume"].is<int>()) {
      int vol = doc["settings"]["buzzerVolume"].as<int>();
      if (vol >= 0 && vol <= 2) {
        appState.buzzerVolume = vol;
        LOG_D("[Settings] Buzzer Volume: %d", vol);
      }
    }
    if (doc["settings"]["backlightBrightness"].is<int>()) {
      int bright = doc["settings"]["backlightBrightness"].as<int>();
      if (bright >= 1 && bright <= 255) {
        appState.backlightBrightness = (uint8_t)bright;
        LOG_D("[Settings] Backlight Brightness: %d", bright);
      }
    }
    if (doc["settings"]["dimEnabled"].is<bool>()) {
      appState.dimEnabled = doc["settings"]["dimEnabled"].as<bool>();
      LOG_D("[Settings] Dim: %s", appState.dimEnabled ? "enabled" : "disabled");
    }
    if (doc["settings"]["dimTimeout"].is<int>()) {
      int dimSec = doc["settings"]["dimTimeout"].as<int>();
      unsigned long dimMs = (unsigned long)dimSec * 1000UL;
      if (dimMs == 5000 || dimMs == 10000 || dimMs == 15000 ||
          dimMs == 30000 || dimMs == 60000) {
        appState.dimTimeout = dimMs;
        LOG_D("[Settings] Dim Timeout: %d seconds", dimSec);
      }
    }
    if (doc["settings"]["dimBrightness"].is<int>()) {
      int dimBright = doc["settings"]["dimBrightness"].as<int>();
      if (dimBright == 26 || dimBright == 64 || dimBright == 128 ||
          dimBright == 191) {
        appState.dimBrightness = (uint8_t)dimBright;
        LOG_D("[Settings] Dim Brightness: %d", dimBright);
      }
    }
    if (doc["settings"]["vuMeterEnabled"].is<bool>()) {
      appState.vuMeterEnabled = doc["settings"]["vuMeterEnabled"].as<bool>();
      LOG_D("[Settings] VU Meter: %s", appState.vuMeterEnabled ? "enabled" : "disabled");
    }
    if (doc["settings"]["waveformEnabled"].is<bool>()) {
      appState.waveformEnabled = doc["settings"]["waveformEnabled"].as<bool>();
      LOG_D("[Settings] Waveform: %s", appState.waveformEnabled ? "enabled" : "disabled");
    }
    if (doc["settings"]["spectrumEnabled"].is<bool>()) {
      appState.spectrumEnabled = doc["settings"]["spectrumEnabled"].as<bool>();
      LOG_D("[Settings] Spectrum: %s", appState.spectrumEnabled ? "enabled" : "disabled");
    }
    if (doc["settings"]["debugMode"].is<bool>()) {
      appState.debugMode = doc["settings"]["debugMode"].as<bool>();
    }
    if (doc["settings"]["debugSerialLevel"].is<int>()) {
      int level = doc["settings"]["debugSerialLevel"].as<int>();
      if (level >= 0 && level <= 3) appState.debugSerialLevel = level;
    }
    if (doc["settings"]["debugHwStats"].is<bool>()) {
      appState.debugHwStats = doc["settings"]["debugHwStats"].as<bool>();
    }
    if (doc["settings"]["debugI2sMetrics"].is<bool>()) {
      appState.debugI2sMetrics = doc["settings"]["debugI2sMetrics"].as<bool>();
    }
    if (doc["settings"]["debugTaskMonitor"].is<bool>()) {
      appState.debugTaskMonitor = doc["settings"]["debugTaskMonitor"].as<bool>();
    }
#ifdef GUI_ENABLED
    if (doc["settings"]["bootAnimEnabled"].is<bool>()) {
      appState.bootAnimEnabled = doc["settings"]["bootAnimEnabled"].as<bool>();
      LOG_D("[Settings] Boot Animation: %s",
            appState.bootAnimEnabled ? "enabled" : "disabled");
    }
    if (doc["settings"]["bootAnimStyle"].is<int>()) {
      int style = doc["settings"]["bootAnimStyle"].as<int>();
      if (style >= 0 && style <= 5) {
        appState.bootAnimStyle = style;
        LOG_D("[Settings] Boot Animation Style: %d", style);
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
      LOG_D("[Settings] Smart Sensing Mode: %s", modeStr.c_str());
    }
    if (doc["smartSensing"]["timerDuration"].is<int>() ||
        doc["smartSensing"]["timerDuration"].is<unsigned long>()) {
      timerDuration = doc["smartSensing"]["timerDuration"].as<unsigned long>();
      LOG_D("[Settings] Timer Duration: %lu minutes", timerDuration);
    }
    if (doc["smartSensing"]["audioThreshold"].is<float>()) {
      audioThreshold_dBFS = doc["smartSensing"]["audioThreshold"].as<float>();
      LOG_D("[Settings] Audio Threshold: %+.0f dBFS", audioThreshold_dBFS);
    }
    if (doc["smartSensing"]["adcVref"].is<float>()) {
      float vref = doc["smartSensing"]["adcVref"].as<float>();
      if (vref >= 1.0f && vref <= 5.0f) {
        appState.adcVref = vref;
        LOG_D("[Settings] ADC VREF: %.2f V", vref);
      }
    }
    // Save Smart Sensing settings
    saveSmartSensingSettings();
  }

  // Import MQTT settings
  if (!doc["mqtt"].isNull()) {
    if (doc["mqtt"]["enabled"].is<bool>()) {
      mqttEnabled = doc["mqtt"]["enabled"].as<bool>();
      LOG_D("[Settings] MQTT Enabled: %s", mqttEnabled ? "true" : "false");
    }
    if (doc["mqtt"]["broker"].is<String>()) {
      mqttBroker = doc["mqtt"]["broker"].as<String>();
      LOG_D("[Settings] MQTT Broker: %s", mqttBroker.c_str());
    }
    if (doc["mqtt"]["port"].is<int>()) {
      mqttPort = doc["mqtt"]["port"].as<int>();
      LOG_D("[Settings] MQTT Port: %d", mqttPort);
    }
    if (doc["mqtt"]["username"].is<String>()) {
      mqttUsername = doc["mqtt"]["username"].as<String>();
      LOG_D("[Settings] MQTT username imported");
    }
    if (doc["mqtt"]["baseTopic"].is<String>()) {
      mqttBaseTopic = doc["mqtt"]["baseTopic"].as<String>();
      LOG_D("[Settings] MQTT Base Topic: %s", mqttBaseTopic.c_str());
    }
    if (doc["mqtt"]["haDiscovery"].is<bool>()) {
      mqttHADiscovery = doc["mqtt"]["haDiscovery"].as<bool>();
      LOG_D("[Settings] MQTT HA Discovery: %s",
            mqttHADiscovery ? "enabled" : "disabled");
    }
    // Note: Password is not imported for security - user needs to re-enter it
    // Save MQTT settings
    saveMqttSettings();
  }

  // Import Signal Generator settings
  if (!doc["signalGenerator"].isNull()) {
    if (doc["signalGenerator"]["waveform"].is<int>()) {
      int wf = doc["signalGenerator"]["waveform"].as<int>();
      if (wf >= 0 && wf <= 3) appState.sigGenWaveform = wf;
    }
    if (doc["signalGenerator"]["frequency"].is<float>()) {
      float freq = doc["signalGenerator"]["frequency"].as<float>();
      if (freq >= 1.0f && freq <= 22000.0f) appState.sigGenFrequency = freq;
    }
    if (doc["signalGenerator"]["amplitude"].is<float>()) {
      float amp = doc["signalGenerator"]["amplitude"].as<float>();
      if (amp >= -96.0f && amp <= 0.0f) appState.sigGenAmplitude = amp;
    }
    if (doc["signalGenerator"]["channel"].is<int>()) {
      int ch = doc["signalGenerator"]["channel"].as<int>();
      if (ch >= 0 && ch <= 2) appState.sigGenChannel = ch;
    }
    if (doc["signalGenerator"]["outputMode"].is<int>()) {
      int mode = doc["signalGenerator"]["outputMode"].as<int>();
      if (mode >= 0 && mode <= 1) appState.sigGenOutputMode = mode;
    }
    if (doc["signalGenerator"]["sweepSpeed"].is<float>()) {
      float speed = doc["signalGenerator"]["sweepSpeed"].as<float>();
      if (speed >= 1.0f && speed <= 22000.0f) appState.sigGenSweepSpeed = speed;
    }
    if (doc["signalGenerator"]["targetAdc"].is<int>()) {
      int target = doc["signalGenerator"]["targetAdc"].as<int>();
      if (target >= 0 && target <= 2) appState.sigGenTargetAdc = target;
    }
    appState.sigGenEnabled = false; // Always boot disabled
    saveSignalGenSettings();
  }

  // Import input channel names
  if (!doc["inputNames"].isNull() && doc["inputNames"].is<JsonArray>()) {
    JsonArray names = doc["inputNames"].as<JsonArray>();
    for (int i = 0; i < NUM_AUDIO_ADCS * 2 && i < (int)names.size(); i++) {
      String name = names[i].as<String>();
      if (name.length() > 0) appState.inputNames[i] = name;
    }
    saveInputNames();
  }

  // Note: Certificate import removed - now using Mozilla certificate bundle

  LOG_I("[Settings] All settings imported successfully");

  // Send success response
  server.send(200, "application/json",
              "{\"success\": true, \"message\": \"Settings imported "
              "successfully. Device will reboot in 3 seconds.\"}");

  // Give time for response, then play shutdown melody and reboot
  delay(100); // Give time for response to be sent
  LOG_W("[Settings] Rebooting after shutdown melody");
  buzzer_play_blocking(BUZZ_SHUTDOWN, 1200);
  ESP.restart();
}

void handleFactoryReset() {
  LOG_W("[Settings] Factory reset requested via web interface");

  // Send success response before performing reset
  server.send(200, "application/json",
              "{\"success\": true, \"message\": \"Factory reset initiated\"}");

  // Give time for response to be sent
  delay(500);

  // Perform factory reset
  performFactoryReset();
}

void handleReboot() {
  LOG_W("[Settings] Reboot requested via web interface");

  // Send success response before rebooting
  server.send(200, "application/json",
              "{\"success\": true, \"message\": \"Rebooting device\"}");

  // Play shutdown melody then reboot
  buzzer_play_blocking(BUZZ_SHUTDOWN, 1200);
  ESP.restart();
}

void handleDiagnostics() {
  LOG_I("[Settings] Diagnostics export requested via web interface");

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
  doc["diagnosticsVersion"] = "1.1";

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
  system["wasCrash"] = crashlog_last_was_crash();
  system["heapCritical"] = appState.heapCritical;
  system["sdkVersion"] = ESP.getSdkVersion();
  system["freeHeap"] = ESP.getFreeHeap();
  system["heapSize"] = ESP.getHeapSize();
  system["minFreeHeap"] = ESP.getMinFreeHeap();
  system["maxAllocHeap"] = ESP.getMaxAllocHeap();
  system["psramSize"] = ESP.getPsramSize();
  system["freePsram"] = ESP.getFreePsram();
  system["flashSize"] = ESP.getFlashChipSize();
  system["flashSpeed"] = ESP.getFlashChipSpeed();

  // ===== Crash History =====
  const CrashLogData &clog = crashlog_get();
  JsonArray crashArr = system["crashHistory"].to<JsonArray>();
  for (int i = 0; i < (int)clog.count && i < CRASH_LOG_MAX_ENTRIES; i++) {
    const CrashLogEntry *entry = crashlog_get_recent(i);
    if (!entry) break;
    JsonObject obj = crashArr.add<JsonObject>();
    obj["reason"] = entry->reason;
    obj["heapFree"] = entry->heapFree;
    obj["heapMinFree"] = entry->heapMinFree;
    if (entry->timestamp[0] != '\0') {
      obj["timestamp"] = entry->timestamp;
    }
    obj["wasCrash"] = crashlog_was_crash(entry->reason);
  }

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
  settings["darkMode"] = darkMode;
  settings["enableCertValidation"] = enableCertValidation;
  settings["blinkingEnabled"] = blinkingEnabled;
  settings["hardwareStatsInterval"] = hardwareStatsInterval;
  settings["audioUpdateRate"] = appState.audioUpdateRate;
  settings["screenTimeout"] = appState.screenTimeout;
  settings["backlightOn"] = appState.backlightOn;
  settings["backlightBrightness"] = appState.backlightBrightness;
  settings["dimEnabled"] = appState.dimEnabled;
  settings["dimTimeout"] = appState.dimTimeout;
  settings["dimBrightness"] = appState.dimBrightness;
  settings["buzzerEnabled"] = appState.buzzerEnabled;
  settings["buzzerVolume"] = appState.buzzerVolume;
  settings["vuMeterEnabled"] = appState.vuMeterEnabled;
  settings["waveformEnabled"] = appState.waveformEnabled;
  settings["spectrumEnabled"] = appState.spectrumEnabled;
  settings["debugMode"] = appState.debugMode;
  settings["debugSerialLevel"] = appState.debugSerialLevel;
  settings["debugHwStats"] = appState.debugHwStats;
  settings["debugI2sMetrics"] = appState.debugI2sMetrics;
  settings["debugTaskMonitor"] = appState.debugTaskMonitor;

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
  sensing["audioThreshold"] = audioThreshold_dBFS;
  sensing["audioLevel"] = audioLevel_dBFS;
  sensing["lastSignalDetection"] = lastSignalDetection;

  // ===== Audio ADC Diagnostics =====
  {
    JsonObject audioAdcObj = doc["audioAdc"].to<JsonObject>();
    audioAdcObj["numAdcsDetected"] = appState.numAdcsDetected;
    audioAdcObj["sampleRate"] = appState.audioSampleRate;
    audioAdcObj["adcVref"] = appState.adcVref;
    JsonArray adcArr = audioAdcObj["adcs"].to<JsonArray>();
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
      adcObj["index"] = a;
      adcObj["healthStatus"] = statusStr;
      adcObj["noiseFloorDbfs"] = adc.noiseFloorDbfs;
      adcObj["i2sReadErrors"] = adc.i2sErrors;
      adcObj["allZeroBuffers"] = adc.allZeroBuffers;
      adcObj["consecutiveZeros"] = adc.consecutiveZeros;
      adcObj["clippedSamples"] = adc.clippedSamples;
      adcObj["clipRate"] = adc.clipRate;
      adcObj["lastNonZeroMs"] = adc.lastNonZeroMs;
      adcObj["totalBuffersRead"] = adc.totalBuffers;
      adcObj["vrms"] = adc.vrmsCombined;
      adcObj["dcOffset"] = adc.dcOffset;
    }
    // Input names
    JsonArray names = audioAdcObj["inputNames"].to<JsonArray>();
    for (int i = 0; i < NUM_AUDIO_ADCS * 2; i++) {
      names.add(appState.inputNames[i]);
    }

    // I2S Configuration
    I2sStaticConfig i2sCfg = i2s_audio_get_static_config();
    JsonObject i2sObj = audioAdcObj["i2sConfig"].to<JsonObject>();
    JsonArray i2sAdcArr = i2sObj["adcs"].to<JsonArray>();
    for (int a = 0; a < NUM_AUDIO_ADCS; a++) {
      JsonObject c = i2sAdcArr.add<JsonObject>();
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
    JsonObject i2sRt = i2sObj["runtime"].to<JsonObject>();
    i2sRt["audioTaskStackFree"] = appState.i2sMetrics.audioTaskStackFree;
    JsonArray bpsArr = i2sRt["buffersPerSec"].to<JsonArray>();
    JsonArray latArr = i2sRt["avgReadLatencyUs"].to<JsonArray>();
    for (int a = 0; a < NUM_AUDIO_ADCS; a++) {
      bpsArr.add(appState.i2sMetrics.buffersPerSec[a]);
      latArr.add(appState.i2sMetrics.avgReadLatencyUs[a]);
    }
  }

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

  LOG_I("[Settings] Diagnostics exported successfully");
}

// Note: Certificate HTTP API handlers removed - now using Mozilla certificate
// bundle The enableCertValidation setting still works to toggle between:
// - ENABLED: Uses Mozilla certificate bundle for validation
// - DISABLED: Insecure mode (no certificate validation)
