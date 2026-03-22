#include "settings_manager.h"
#include "app_state.h"
#include "globals.h"
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
#include "http_security.h"
#ifdef DAC_ENABLED
#include "dac_hal.h"
#include "hal/hal_device_manager.h"
#include "hal/hal_device_db.h"
#endif
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>

// Note: Certificate management removed - now using Mozilla certificate bundle
// via ESP32CertBundle library for automatic SSL validation of all public
// servers

// ===== Settings Persistence =====

// Load NVS-stored settings (wifiMinSec, otaChannel, halAutoDisco)
static void loadNvsSettings() {
  {
    Preferences wifiPrefs;
    wifiPrefs.begin("wifi-list", true);
    appState.wifi.minSecurity = wifiPrefs.getUChar("wifiMinSec", 0);
    wifiPrefs.end();
  }
  {
    Preferences otaPrefs;
    otaPrefs.begin("ota-prefs", true);
    appState.ota.channel = otaPrefs.getUChar("otaChannel", 0);
    otaPrefs.end();
  }
  {
    Preferences halPrefs;
    halPrefs.begin("hal-prefs", true);
    appState.halAutoDiscovery = halPrefs.getBool("halAutoDisco", true);
    halPrefs.end();
  }
}

// Apply settings from a parsed JSON document
static void applySettingsFromJson(const JsonDocument &doc) {
  if (doc["autoUpdate"].is<bool>()) appState.ota.autoUpdateEnabled = doc["autoUpdate"].as<bool>();
  if (doc["timezone"].is<int>()) appState.general.timezoneOffset = doc["timezone"].as<int>();
  if (doc["dst"].is<int>()) appState.general.dstOffset = doc["dst"].as<int>();
  if (doc["darkMode"].is<bool>()) appState.general.darkMode = doc["darkMode"].as<bool>();
  if (doc["certValidation"].is<bool>()) appState.general.enableCertValidation = doc["certValidation"].as<bool>();

  if (doc["hwStatsInterval"].is<unsigned long>()) {
    unsigned long interval = doc["hwStatsInterval"].as<unsigned long>();
    if (interval == 1000 || interval == 2000 || interval == 3000 ||
        interval == 5000 || interval == 10000)
      appState.debug.hardwareStatsInterval = interval;
  }

  if (doc["autoAP"].is<bool>()) appState.wifi.autoAPEnabled = doc["autoAP"].as<bool>();

#ifdef GUI_ENABLED
  if (doc["bootAnim"].is<bool>()) appState.bootAnimEnabled = doc["bootAnim"].as<bool>();
  if (doc["bootAnimStyle"].is<int>()) {
    int style = doc["bootAnimStyle"].as<int>();
    if (style >= 0 && style <= 5) appState.bootAnimStyle = style;
  }
#endif

  if (doc["screenTimeout"].is<unsigned long>()) {
    unsigned long timeout = doc["screenTimeout"].as<unsigned long>();
    if (timeout == 0 || timeout == 30000 || timeout == 60000 ||
        timeout == 300000 || timeout == 600000)
      appState.display.screenTimeout = timeout;
  }

  if (doc["buzzer"].is<bool>()) appState.buzzer.enabled = doc["buzzer"].as<bool>();
  if (doc["buzzerVol"].is<int>()) {
    int vol = doc["buzzerVol"].as<int>();
    if (vol >= 0 && vol <= 2) appState.buzzer.volume = vol;
  }

  if (doc["backlight"].is<int>()) {
    int bright = doc["backlight"].as<int>();
    if (bright >= 1 && bright <= 255) appState.display.backlightBrightness = (uint8_t)bright;
  }

  if (doc["dimTimeout"].is<unsigned long>()) {
    unsigned long dimVal = doc["dimTimeout"].as<unsigned long>();
    if (dimVal == 5000 || dimVal == 10000 || dimVal == 15000 ||
        dimVal == 30000 || dimVal == 60000)
      appState.display.dimTimeout = dimVal;
  }
  if (doc["dimBright"].is<int>()) {
    int dimBright = doc["dimBright"].as<int>();
    if (dimBright == 26 || dimBright == 64 || dimBright == 128 || dimBright == 191)
      appState.display.dimBrightness = (uint8_t)dimBright;
  }
  if (doc["dimEnabled"].is<bool>()) appState.display.dimEnabled = doc["dimEnabled"].as<bool>();

  if (doc["audioRate"].is<int>()) {
    int rate = doc["audioRate"].as<int>();
    if (rate == 33 || rate == 50 || rate == 100) appState.audio.updateRate = (uint16_t)rate;
  }

  if (doc["vuMeter"].is<bool>()) appState.audio.vuMeterEnabled = doc["vuMeter"].as<bool>();
  if (doc["waveform"].is<bool>()) appState.audio.waveformEnabled = doc["waveform"].as<bool>();
  if (doc["spectrum"].is<bool>()) appState.audio.spectrumEnabled = doc["spectrum"].as<bool>();

  if (doc["debugMode"].is<bool>()) appState.debug.debugMode = doc["debugMode"].as<bool>();
  if (doc["debugSerial"].is<int>()) {
    int level = doc["debugSerial"].as<int>();
    if (level >= 0 && level <= 3) appState.debug.serialLevel = level;
  }
  if (doc["debugHwStats"].is<bool>()) appState.debug.hwStats = doc["debugHwStats"].as<bool>();
  if (doc["debugI2s"].is<bool>()) appState.debug.i2sMetrics = doc["debugI2s"].as<bool>();
  if (doc["debugTasks"].is<bool>()) appState.debug.taskMonitor = doc["debugTasks"].as<bool>();

  if (doc["fftWindow"].is<int>()) {
    int wt = doc["fftWindow"].as<int>();
    if (wt >= 0 && wt < FFT_WINDOW_COUNT) appState.audio.fftWindowType = (FftWindowType)wt;
  }

  if (doc["adcEnabled"].is<JsonArrayConst>()) {
    JsonArrayConst arr = doc["adcEnabled"].as<JsonArrayConst>();
    for (size_t i = 0; i < arr.size() && i < AUDIO_PIPELINE_MAX_INPUTS; i++)
      appState.audio.adcEnabled[i] = arr[i].as<bool>();
  }

#ifdef USB_AUDIO_ENABLED
  if (doc["usbAudio"].is<bool>()) appState.usbAudio.enabled = doc["usbAudio"].as<bool>();
#endif
}

// Flag set by loadSettingsJson() when the JSON config contains an "mqtt" section.
// loadMqttSettings() checks this to skip /mqtt_config.txt when already loaded.
static bool _mqttLoadedFromJson = false;

bool settingsMqttLoadedFromJson() { return _mqttLoadedFromJson; }

// Apply MQTT fields from a parsed /config.json document.
// Only called when the document contains an "mqtt" object.
static void applyMqttFromJson(const JsonDocument &doc) {
  if (!doc["mqtt"].is<JsonObjectConst>()) return;
  JsonObjectConst m = doc["mqtt"].as<JsonObjectConst>();
  if (m["enabled"].is<bool>())        appState.mqtt.enabled     = m["enabled"].as<bool>();
  if (m["broker"].is<const char*>())  appState.mqtt.broker      = m["broker"].as<const char*>();
  if (m["port"].is<int>())            appState.mqtt.port        = m["port"].as<int>();
  if (m["username"].is<const char*>()) appState.mqtt.username   = m["username"].as<const char*>();
  if (m["password"].is<const char*>()) appState.mqtt.password   = m["password"].as<const char*>();
  if (m["baseTopic"].is<const char*>()) appState.mqtt.baseTopic = m["baseTopic"].as<const char*>();
  if (m["haDiscovery"].is<bool>())    appState.mqtt.haDiscovery = m["haDiscovery"].as<bool>();
}

// Try loading settings from /config.json
static bool loadSettingsJson() {
  File file = LittleFS.open("/config.json", "r");
  if (!file || file.size() == 0) {
    if (file) file.close();
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();

  if (err) {
    LOG_W("[Settings] config.json parse error: %s", err.c_str());
    return false;
  }

  if (!doc["version"].is<int>()) {
    LOG_W("[Settings] config.json missing version field");
    return false;
  }

  int version = doc["version"].as<int>();
  applySettingsFromJson(doc);

  // If the JSON file includes an "mqtt" section, load it now and record
  // the fact so loadMqttSettings() can skip the legacy /mqtt_config.txt.
  if (doc["mqtt"].is<JsonObjectConst>()) {
    applyMqttFromJson(doc);
    _mqttLoadedFromJson = true;
    LOG_I("[Settings] JSON config v%d loaded (with MQTT), skipping legacy paths", version);
  } else {
    LOG_I("[Settings] JSON config v%d loaded, skipping legacy paths", version);
  }

  return true;
}

// Load settings from legacy /settings.txt (positional line format)
static bool loadSettingsLegacy() {
  File file = LittleFS.open("/settings.txt", "r", true);
  if (!file || file.size() == 0) {
    if (file) file.close();
    return false;
  }

  String lines[30];
  for (int i = 0; i < 30; i++) {
    if (file.available()) {
      lines[i] = file.readStringUntil('\n');
      lines[i].trim();
    }
  }
  file.close();

  int dataStart = 0;
  if (lines[0].startsWith("VER=")) {
    dataStart = 1;
  }

  if (lines[dataStart].length() == 0) return false;

  appState.ota.autoUpdateEnabled = (lines[dataStart + 0].toInt() != 0);
  if (lines[dataStart + 1].length() > 0) appState.general.timezoneOffset = lines[dataStart + 1].toInt();
  if (lines[dataStart + 2].length() > 0) appState.general.dstOffset = lines[dataStart + 2].toInt();
  if (lines[dataStart + 3].length() > 0) appState.general.darkMode = (lines[dataStart + 3].toInt() != 0);
  if (lines[dataStart + 4].length() > 0) appState.general.enableCertValidation = (lines[dataStart + 4].toInt() != 0);

  if (lines[dataStart + 5].length() > 0) {
    unsigned long interval = lines[dataStart + 5].toInt();
    if (interval == 1000 || interval == 2000 || interval == 3000 ||
        interval == 5000 || interval == 10000)
      appState.debug.hardwareStatsInterval = interval;
  }

  if (lines[dataStart + 6].length() > 0) {
    appState.wifi.autoAPEnabled = (lines[dataStart + 6].toInt() != 0);
  } else {
    appState.wifi.autoAPEnabled = true;
  }

#ifdef GUI_ENABLED
  if (lines[dataStart + 7].length() > 0) appState.bootAnimEnabled = (lines[dataStart + 7].toInt() != 0);
  if (lines[dataStart + 8].length() > 0) {
    int style = lines[dataStart + 8].toInt();
    if (style >= 0 && style <= 5) appState.bootAnimStyle = style;
  }
#endif

  if (lines[dataStart + 9].length() > 0) {
    unsigned long timeout = lines[dataStart + 9].toInt();
    if (timeout == 0 || timeout == 30000 || timeout == 60000 ||
        timeout == 300000 || timeout == 600000)
      appState.display.screenTimeout = timeout;
  }

  if (lines[dataStart + 10].length() > 0) appState.buzzer.enabled = (lines[dataStart + 10].toInt() != 0);
  if (lines[dataStart + 11].length() > 0) {
    int vol = lines[dataStart + 11].toInt();
    if (vol >= 0 && vol <= 2) appState.buzzer.volume = vol;
  }
  if (lines[dataStart + 12].length() > 0) {
    int bright = lines[dataStart + 12].toInt();
    if (bright >= 1 && bright <= 255) appState.display.backlightBrightness = (uint8_t)bright;
  }

  if (lines[dataStart + 13].length() > 0) {
    unsigned long dimVal = lines[dataStart + 13].toInt();
    if (dimVal == 0) {
      // Legacy "disabled" value
    } else if (dimVal == 5000 || dimVal == 10000 || dimVal == 15000 ||
               dimVal == 30000 || dimVal == 60000) {
      appState.display.dimTimeout = dimVal;
    }
  }
  if (lines[dataStart + 14].length() > 0) {
    int dimBright = lines[dataStart + 14].toInt();
    if (dimBright == 26 || dimBright == 64 || dimBright == 128 || dimBright == 191)
      appState.display.dimBrightness = (uint8_t)dimBright;
  }
  if (lines[dataStart + 15].length() > 0) appState.display.dimEnabled = (lines[dataStart + 15] == "1");

  if (lines[dataStart + 16].length() > 0) {
    int rate = lines[dataStart + 16].toInt();
    if (rate == 33 || rate == 50 || rate == 100) appState.audio.updateRate = (uint16_t)rate;
  }

  if (lines[dataStart + 17].length() > 0) appState.audio.vuMeterEnabled = (lines[dataStart + 17].toInt() != 0);
  if (lines[dataStart + 18].length() > 0) appState.audio.waveformEnabled = (lines[dataStart + 18].toInt() != 0);
  if (lines[dataStart + 19].length() > 0) appState.audio.spectrumEnabled = (lines[dataStart + 19].toInt() != 0);

  if (lines[dataStart + 20].length() > 0) appState.debug.debugMode = (lines[dataStart + 20].toInt() != 0);
  if (lines[dataStart + 21].length() > 0) {
    int level = lines[dataStart + 21].toInt();
    if (level >= 0 && level <= 3) appState.debug.serialLevel = level;
  }
  if (lines[dataStart + 22].length() > 0) appState.debug.hwStats = (lines[dataStart + 22].toInt() != 0);
  if (lines[dataStart + 23].length() > 0) appState.debug.i2sMetrics = (lines[dataStart + 23].toInt() != 0);
  if (lines[dataStart + 24].length() > 0) appState.debug.taskMonitor = (lines[dataStart + 24].toInt() != 0);
  if (lines[dataStart + 25].length() > 0) {
    int wt = lines[dataStart + 25].toInt();
    if (wt >= 0 && wt < FFT_WINDOW_COUNT) appState.audio.fftWindowType = (FftWindowType)wt;
  }

  if (lines[dataStart + 26].length() > 0) {
    int commaIdx = lines[dataStart + 26].indexOf(',');
    if (commaIdx > 0) {
      appState.audio.adcEnabled[0] = (lines[dataStart + 26].substring(0, commaIdx).toInt() != 0);
      appState.audio.adcEnabled[1] = (lines[dataStart + 26].substring(commaIdx + 1).toInt() != 0);
    } else {
      bool val = (lines[dataStart + 26].toInt() != 0);
      appState.audio.adcEnabled[0] = val;
      appState.audio.adcEnabled[1] = val;
    }
  }

#ifdef USB_AUDIO_ENABLED
  if (lines[dataStart + 27].length() > 0)
    appState.usbAudio.enabled = (lines[dataStart + 27].toInt() != 0);
#endif

  return true;
}

bool loadSettings() {
  // Recovery: if a .tmp file exists without config.json, the rename was
  // interrupted — complete it now
  if (LittleFS.exists("/config.json.tmp") && !LittleFS.exists("/config.json")) {
    LittleFS.rename("/config.json.tmp", "/config.json");
  }

  // 1. Try JSON format first
  if (loadSettingsJson()) {
    loadNvsSettings();
    return true;
  }

  // 2. Fall back to legacy text format
  if (loadSettingsLegacy()) {
    LOG_I("[Settings] Migrating settings.txt -> config.json");
    loadNvsSettings();
    saveSettings();  // Auto-migrate to JSON (old file preserved)
    return true;
  }

  // 3. No settings found — use defaults
  loadNvsSettings();
  return false;
}

// ===== Deferred Settings Save =====
static bool _settingsSavePending = false;
static unsigned long _lastSettingsSaveRequest = 0;
static const unsigned long SETTINGS_SAVE_DEBOUNCE_MS = 2000;

void saveSettingsDeferred() {
    _settingsSavePending = true;
    _lastSettingsSaveRequest = millis();
}

void checkDeferredSettingsSave() {
    if (_settingsSavePending && (millis() - _lastSettingsSaveRequest >= SETTINGS_SAVE_DEBOUNCE_MS)) {
        saveSettings();
        _settingsSavePending = false;
    }
}

void saveSettings() {
  JsonDocument doc;
  doc["version"] = 1;
  doc["autoUpdate"] = appState.ota.autoUpdateEnabled;
  doc["timezone"] = appState.general.timezoneOffset;
  doc["dst"] = appState.general.dstOffset;
  doc["darkMode"] = appState.general.darkMode;
  doc["certValidation"] = appState.general.enableCertValidation;
  doc["hwStatsInterval"] = appState.debug.hardwareStatsInterval;
  doc["autoAP"] = appState.wifi.autoAPEnabled;
#ifdef GUI_ENABLED
  doc["bootAnim"] = appState.bootAnimEnabled;
  doc["bootAnimStyle"] = appState.bootAnimStyle;
#else
  doc["bootAnim"] = true;
  doc["bootAnimStyle"] = 0;
#endif
  doc["screenTimeout"] = appState.display.screenTimeout;
  doc["buzzer"] = appState.buzzer.enabled;
  doc["buzzerVol"] = appState.buzzer.volume;
  doc["backlight"] = appState.display.backlightBrightness;
  doc["dimTimeout"] = appState.display.dimTimeout;
  doc["dimBright"] = appState.display.dimBrightness;
  doc["dimEnabled"] = appState.display.dimEnabled;
  doc["audioRate"] = appState.audio.updateRate;
  doc["vuMeter"] = appState.audio.vuMeterEnabled;
  doc["waveform"] = appState.audio.waveformEnabled;
  doc["spectrum"] = appState.audio.spectrumEnabled;
  doc["debugMode"] = appState.debug.debugMode;
  doc["debugSerial"] = appState.debug.serialLevel;
  doc["debugHwStats"] = appState.debug.hwStats;
  doc["debugI2s"] = appState.debug.i2sMetrics;
  doc["debugTasks"] = appState.debug.taskMonitor;
  doc["fftWindow"] = (int)appState.audio.fftWindowType;
  {
    JsonArray adcArr = doc["adcEnabled"].to<JsonArray>();
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) adcArr.add(appState.audio.adcEnabled[i]);
  }
#ifdef USB_AUDIO_ENABLED
  doc["usbAudio"] = appState.usbAudio.enabled;
#else
  doc["usbAudio"] = false;
#endif

  // Atomic write: write to .tmp then rename
  File file = LittleFS.open("/config.json.tmp", "w");
  if (!file) {
    LOG_E("[Settings] Failed to open config.json.tmp for writing");
    return;
  }
  serializeJson(doc, file);
  file.close();

  // Rename atomically (old settings.txt preserved as fallback)
  LittleFS.remove("/config.json");
  LittleFS.rename("/config.json.tmp", "/config.json");
  LOG_I("[Settings] Settings saved to config.json");

  // Save NVS settings (survive LittleFS format)
  {
    Preferences wifiPrefs;
    wifiPrefs.begin("wifi-list", false);
    wifiPrefs.putUChar("wifiMinSec", appState.wifi.minSecurity);
    wifiPrefs.end();
  }
  {
    Preferences otaPrefs;
    otaPrefs.begin("ota-prefs", false);
    otaPrefs.putUChar("otaChannel", appState.ota.channel);
    otaPrefs.end();
  }
  {
    Preferences halPrefs;
    halPrefs.begin("hal-prefs", false);
    halPrefs.putBool("halAutoDisco", appState.halAutoDiscovery);
    halPrefs.end();
  }
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
  file.close();

  line1.trim();
  line2.trim();
  line3.trim();
  line4.trim();
  line5.trim();
  line6.trim();

  if (line1.length() > 0) {
    int wf = line1.toInt();
    if (wf >= 0 && wf <= 3) appState.sigGen.waveform = wf;
  }
  if (line2.length() > 0) {
    float freq = line2.toFloat();
    if (freq >= 1.0f && freq <= 22000.0f) appState.sigGen.frequency = freq;
  }
  if (line3.length() > 0) {
    float amp = line3.toFloat();
    if (amp >= -96.0f && amp <= 0.0f) appState.sigGen.amplitude = amp;
  }
  if (line4.length() > 0) {
    int ch = line4.toInt();
    if (ch >= 0 && ch <= 2) appState.sigGen.channel = ch;
  }
  if (line5.length() > 0) {
    int mode = line5.toInt();
    if (mode >= 0 && mode <= 1) appState.sigGen.outputMode = mode;
  }
  if (line6.length() > 0) {
    float speed = line6.toFloat();
    if (speed >= 1.0f && speed <= 22000.0f) appState.sigGen.sweepSpeed = speed;
  }

  // Always boot disabled regardless of saved state
  appState.sigGen.enabled = false;

  LOG_I("[Settings] Signal generator settings loaded");
  return true;
}

void saveSignalGenSettings() {
  File file = LittleFS.open("/siggen.txt", "w");
  if (!file) {
    LOG_E("[Settings] Failed to open siggen settings file for writing");
    return;
  }

  file.println(appState.sigGen.waveform);
  file.println(String(appState.sigGen.frequency, 1));
  file.println(String(appState.sigGen.amplitude, 1));
  file.println(appState.sigGen.channel);
  file.println(appState.sigGen.outputMode);
  file.println(String(appState.sigGen.sweepSpeed, 1));
  file.close();
  LOG_I("[Settings] Signal generator settings saved");
}

// ===== Deferred Signal Generator Settings Save =====
static bool _sigGenSavePending = false;
static unsigned long _lastSigGenSaveRequest = 0;
static const unsigned long SIGGEN_SAVE_DEBOUNCE_MS = 2000;

void saveSignalGenSettingsDeferred() {
    _sigGenSavePending = true;
    _lastSigGenSaveRequest = millis();
}

void checkDeferredSignalGenSave() {
    if (_sigGenSavePending &&
        (millis() - _lastSigGenSaveRequest >= SIGGEN_SAVE_DEBOUNCE_MS)) {
        _sigGenSavePending = false;
        saveSignalGenSettings();
    }
}

// ===== Input Names Settings =====

static const char *INPUT_NAME_DEFAULTS[] = {
    "Subwoofer 1", "Subwoofer 2", "Subwoofer 3", "Subwoofer 4",
    "Input 3 Left", "Input 3 Right", "Input 4 Left", "Input 4 Right",
    "Input 5 Left", "Input 5 Right", "Input 6 Left", "Input 6 Right",
    "Input 7 Left", "Input 7 Right", "Input 8 Left", "Input 8 Right"};

bool loadInputNames() {
  File file = LittleFS.open("/inputnames.txt", "r", true);
  if (!file || file.size() == 0) {
    if (file)
      file.close();
    // Set defaults
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS * 2; i++) {
      appState.audio.inputNames[i] = INPUT_NAME_DEFAULTS[i];
    }
    return false;
  }

  for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS * 2; i++) {
    String line = file.readStringUntil('\n');
    line.trim();
    appState.audio.inputNames[i] =
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

  for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS * 2; i++) {
    file.println(appState.audio.inputNames[i]);
  }
  file.close();
  LOG_I("[Settings] Input names saved");
}

// ===== Factory Reset =====

void performFactoryReset() {
  LOG_W("[Settings] Factory reset initiated");
  appState.general.factoryResetInProgress = true;

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
  appState.wifi.apEnabled = true;
  appState.wifi.isAPMode = true;

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
  doc["appState.autoUpdateEnabled"] = appState.ota.autoUpdateEnabled;
  doc["appState.timezoneOffset"] = appState.general.timezoneOffset;
  doc["appState.dstOffset"] = appState.general.dstOffset;
  doc["appState.darkMode"] = appState.general.darkMode;
  doc["appState.enableCertValidation"] = appState.general.enableCertValidation;
  doc["appState.autoAPEnabled"] = appState.wifi.autoAPEnabled;
  doc["appState.debug.hardwareStatsInterval"] =
      appState.debug.hardwareStatsInterval / 1000; // Send as seconds
  doc["audioUpdateRate"] = appState.audio.updateRate;
  doc["screenTimeout"] = appState.display.screenTimeout / 1000; // Send as seconds
  doc["backlightOn"] = appState.display.backlightOn;
  doc["buzzerEnabled"] = appState.buzzer.enabled;
  doc["buzzerVolume"] = appState.buzzer.volume;
  doc["backlightBrightness"] = appState.display.backlightBrightness;
  doc["dimEnabled"] = appState.display.dimEnabled;
  doc["dimTimeout"] = appState.display.dimTimeout / 1000;
  doc["dimBrightness"] = appState.display.dimBrightness;
  doc["debugMode"] = appState.debug.debugMode;
  doc["debugSerialLevel"] = appState.debug.serialLevel;
  doc["debugHwStats"] = appState.debug.hwStats;
  doc["debugI2sMetrics"] = appState.debug.i2sMetrics;
  doc["debugTaskMonitor"] = appState.debug.taskMonitor;
  doc["fftWindowType"] = (int)appState.audio.fftWindowType;
  {
    JsonArray adcArr = doc["adcEnabled"].to<JsonArray>();
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) adcArr.add(appState.audio.adcEnabled[i]);
  }
#ifdef USB_AUDIO_ENABLED
  doc["usbAudioEnabled"] = appState.usbAudio.enabled;
#endif
#ifdef GUI_ENABLED
  doc["bootAnimEnabled"] = appState.bootAnimEnabled;
  doc["bootAnimStyle"] = appState.bootAnimStyle;
#endif
  doc["otaChannel"] = appState.ota.channel;

  String json;
  serializeJson(doc, json);
  server_send(200, "application/json", json);
}

void handleSettingsUpdate() {
  if (!server.hasArg("plain")) {
    server_send(400, "application/json",
                "{\"success\": false, \"message\": \"No data received\"}");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    server_send(400, "application/json",
                "{\"success\": false, \"message\": \"Invalid JSON\"}");
    return;
  }

  bool settingsChanged = false;

  if (doc["appState.autoUpdateEnabled"].is<bool>()) {
    appState.ota.autoUpdateEnabled = doc["appState.autoUpdateEnabled"].as<bool>();
    settingsChanged = true;
  }

  bool timezoneChanged = false;

  if (doc["appState.timezoneOffset"].is<int>()) {
    int newOffset = doc["appState.timezoneOffset"].as<int>();
    if (newOffset != appState.general.timezoneOffset) {
      appState.general.timezoneOffset = newOffset;
      settingsChanged = true;
      timezoneChanged = true;
    }
  }

  if (doc["appState.dstOffset"].is<int>()) {
    int newDstOffset = doc["appState.dstOffset"].as<int>();
    if (newDstOffset != appState.general.dstOffset) {
      appState.general.dstOffset = newDstOffset;
      settingsChanged = true;
      timezoneChanged = true;
    }
  }

  // Re-sync time if timezone or DST changed
  if (timezoneChanged && WiFi.status() == WL_CONNECTED) {
    syncTimeWithNTP();
  }

  if (doc["appState.darkMode"].is<bool>()) {
    bool newDarkMode = doc["appState.darkMode"].as<bool>();
    if (newDarkMode != appState.general.darkMode) {
      appState.general.darkMode = newDarkMode;
      settingsChanged = true;
    }
  }

  if (doc["appState.enableCertValidation"].is<bool>()) {
    bool newCertValidation = doc["appState.enableCertValidation"].as<bool>();
    if (newCertValidation != appState.general.enableCertValidation) {
      appState.general.enableCertValidation = newCertValidation;
      settingsChanged = true;
      LOG_I("[Settings] Certificate validation %s",
            appState.general.enableCertValidation ? "ENABLED" : "DISABLED");
    }
  }

  if (doc["appState.debug.hardwareStatsInterval"].is<int>()) {
    int newInterval = doc["appState.debug.hardwareStatsInterval"].as<int>();
    // Validate: only allow 1, 2, 3, 5, or 10 seconds
    if (newInterval == 1 || newInterval == 2 || newInterval == 3 ||
        newInterval == 5 || newInterval == 10) {
      unsigned long newIntervalMs = newInterval * 1000UL;
      if (newIntervalMs != appState.debug.hardwareStatsInterval) {
        appState.debug.hardwareStatsInterval = newIntervalMs;
        settingsChanged = true;
        LOG_I("[Settings] Hardware stats interval set to %d seconds",
              newInterval);
      }
    }
  }

  if (doc["audioUpdateRate"].is<int>()) {
    int newRate = doc["audioUpdateRate"].as<int>();
    if (newRate == 33 || newRate == 50 || newRate == 100) {
      if ((uint16_t)newRate != appState.audio.updateRate) {
        appState.audio.updateRate = (uint16_t)newRate;
        settingsChanged = true;
        LOG_I("[Settings] Audio update rate set to %d ms", newRate);
      }
    }
  }

  if (doc["appState.autoAPEnabled"].is<bool>()) {
    bool newAutoAP = doc["appState.autoAPEnabled"].as<bool>();
    if (newAutoAP != appState.wifi.autoAPEnabled) {
      appState.wifi.autoAPEnabled = newAutoAP;
      settingsChanged = true;
      LOG_I("[Settings] Auto AP %s", appState.wifi.autoAPEnabled ? "enabled" : "disabled");
    }
  }

  if (doc["screenTimeout"].is<int>()) {
    int newTimeoutSec = doc["screenTimeout"].as<int>();
    // Convert seconds to milliseconds and validate allowed values
    unsigned long newTimeoutMs = (unsigned long)newTimeoutSec * 1000UL;
    if (newTimeoutMs == 0 || newTimeoutMs == 30000 || newTimeoutMs == 60000 ||
        newTimeoutMs == 300000 || newTimeoutMs == 600000) {
      if (newTimeoutMs != appState.display.screenTimeout) {
        appState.setScreenTimeout(newTimeoutMs);
        settingsChanged = true;
        LOG_I("[Settings] Screen timeout set to %d seconds", newTimeoutSec);
      }
    }
  }

  if (doc["backlightOn"].is<bool>()) {
    bool newBacklight = doc["backlightOn"].as<bool>();
    if (newBacklight != appState.display.backlightOn) {
      appState.setBacklightOn(newBacklight);
      LOG_I("[Settings] Backlight set to %s", newBacklight ? "ON" : "OFF");
    }
    // backlightOn is runtime only, no save
  }

  if (doc["buzzerEnabled"].is<bool>()) {
    bool newBuzzer = doc["buzzerEnabled"].as<bool>();
    if (newBuzzer != appState.buzzer.enabled) {
      appState.setBuzzerEnabled(newBuzzer);
      settingsChanged = true;
      LOG_I("[Settings] Buzzer %s", newBuzzer ? "enabled" : "disabled");
    }
  }

  if (doc["buzzerVolume"].is<int>()) {
    int newVol = doc["buzzerVolume"].as<int>();
    if (newVol >= 0 && newVol <= 2 && newVol != appState.buzzer.volume) {
      appState.setBuzzerVolume(newVol);
      settingsChanged = true;
      LOG_I("[Settings] Buzzer volume set to %d", newVol);
    }
  }

  if (doc["backlightBrightness"].is<int>()) {
    int newBright = doc["backlightBrightness"].as<int>();
    if (newBright >= 1 && newBright <= 255 &&
        (uint8_t)newBright != appState.display.backlightBrightness) {
      appState.setBacklightBrightness((uint8_t)newBright);
      settingsChanged = true;
      LOG_I("[Settings] Backlight brightness set to %d", newBright);
    }
  }

  if (doc["dimEnabled"].is<bool>()) {
    bool newDimEnabled = doc["dimEnabled"].as<bool>();
    if (newDimEnabled != appState.display.dimEnabled) {
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
      if (newDimMs != appState.display.dimTimeout) {
        appState.setDimTimeout(newDimMs);
        settingsChanged = true;
      }
    }
  }

  if (doc["dimBrightness"].is<int>()) {
    int newDimBright = doc["dimBrightness"].as<int>();
    if ((newDimBright == 26 || newDimBright == 64 || newDimBright == 128 ||
         newDimBright == 191) &&
        (uint8_t)newDimBright != appState.display.dimBrightness) {
      appState.setDimBrightness((uint8_t)newDimBright);
      settingsChanged = true;
      LOG_I("[Settings] Dim brightness set to %d", newDimBright);
    }
  }

  if (doc["debugMode"].is<bool>()) {
    bool newVal = doc["debugMode"].as<bool>();
    if (newVal != appState.debug.debugMode) {
      appState.debug.debugMode = newVal;
      applyDebugSerialLevel(appState.debug.debugMode, appState.debug.serialLevel);
      settingsChanged = true;
    }
  }
  if (doc["debugSerialLevel"].is<int>()) {
    int newLevel = doc["debugSerialLevel"].as<int>();
    if (newLevel >= 0 && newLevel <= 3 && newLevel != appState.debug.serialLevel) {
      appState.debug.serialLevel = newLevel;
      applyDebugSerialLevel(appState.debug.debugMode, appState.debug.serialLevel);
      settingsChanged = true;
    }
  }
  if (doc["debugHwStats"].is<bool>()) {
    bool newVal = doc["debugHwStats"].as<bool>();
    if (newVal != appState.debug.hwStats) {
      appState.debug.hwStats = newVal;
      settingsChanged = true;
    }
  }
  if (doc["debugI2sMetrics"].is<bool>()) {
    bool newVal = doc["debugI2sMetrics"].as<bool>();
    if (newVal != appState.debug.i2sMetrics) {
      appState.debug.i2sMetrics = newVal;
      settingsChanged = true;
    }
  }
  if (doc["debugTaskMonitor"].is<bool>()) {
    bool newVal = doc["debugTaskMonitor"].as<bool>();
    if (newVal != appState.debug.taskMonitor) {
      appState.debug.taskMonitor = newVal;
      settingsChanged = true;
    }
  }
  if (doc["fftWindowType"].is<int>()) {
    int newWt = doc["fftWindowType"].as<int>();
    if (newWt >= 0 && newWt < FFT_WINDOW_COUNT && newWt != (int)appState.audio.fftWindowType) {
      appState.audio.fftWindowType = (FftWindowType)newWt;
      settingsChanged = true;
    }
  }

  if (doc["adcEnabled"].is<JsonArray>()) {
    JsonArray arr = doc["adcEnabled"].as<JsonArray>();
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS && i < (int)arr.size(); i++) {
      bool newVal = arr[i].as<bool>();
      if (newVal != appState.audio.adcEnabled[i]) {
        appState.audio.adcEnabled[i] = newVal;
        settingsChanged = true;
        LOG_I("[Settings] ADC%d %s", i + 1, newVal ? "enabled" : "disabled");
      }
    }
  } else if (doc["adcEnabled"].is<bool>()) {
    // Legacy single bool — apply to both
    bool newVal = doc["adcEnabled"].as<bool>();
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
      if (newVal != appState.audio.adcEnabled[i]) {
        appState.audio.adcEnabled[i] = newVal;
        settingsChanged = true;
      }
    }
  }

#ifdef USB_AUDIO_ENABLED
  if (doc["usbAudioEnabled"].is<bool>()) {
    bool newVal = doc["usbAudioEnabled"].as<bool>();
    if (newVal != appState.usbAudio.enabled) {
      appState.usbAudio.enabled = newVal;
      settingsChanged = true;
      LOG_I("[Settings] USB Audio %s", newVal ? "enabled" : "disabled");
    }
  }
#endif

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

  if (doc["otaChannel"].is<int>()) {
    uint8_t newChannel = (uint8_t)doc["otaChannel"].as<int>();
    if (newChannel <= 1 && newChannel != appState.ota.channel) {
      appState.ota.channel = newChannel;
      settingsChanged = true;
      LOG_I("[Settings] OTA channel set to %s", newChannel == 0 ? "stable" : "beta");
    }
  }

  if (settingsChanged) {
    saveSettings();
  }

  JsonDocument resp;
  resp["success"] = true;
  resp["appState.autoUpdateEnabled"] = appState.ota.autoUpdateEnabled;
  resp["appState.timezoneOffset"] = appState.general.timezoneOffset;
  resp["appState.dstOffset"] = appState.general.dstOffset;
  resp["appState.darkMode"] = appState.general.darkMode;
  resp["appState.enableCertValidation"] = appState.general.enableCertValidation;
  resp["appState.autoAPEnabled"] = appState.wifi.autoAPEnabled;
  resp["appState.debug.hardwareStatsInterval"] = appState.debug.hardwareStatsInterval / 1000;
  resp["audioUpdateRate"] = appState.audio.updateRate;
  resp["screenTimeout"] = appState.display.screenTimeout / 1000;
  resp["backlightOn"] = appState.display.backlightOn;
  resp["buzzerEnabled"] = appState.buzzer.enabled;
  resp["buzzerVolume"] = appState.buzzer.volume;
  resp["backlightBrightness"] = appState.display.backlightBrightness;
  resp["dimEnabled"] = appState.display.dimEnabled;
  resp["dimTimeout"] = appState.display.dimTimeout / 1000;
  resp["dimBrightness"] = appState.display.dimBrightness;
  resp["debugMode"] = appState.debug.debugMode;
  resp["debugSerialLevel"] = appState.debug.serialLevel;
  resp["debugHwStats"] = appState.debug.hwStats;
  resp["debugI2sMetrics"] = appState.debug.i2sMetrics;
  resp["debugTaskMonitor"] = appState.debug.taskMonitor;
  resp["fftWindowType"] = (int)appState.audio.fftWindowType;
  {
    JsonArray adcArr = resp["adcEnabled"].to<JsonArray>();
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) adcArr.add(appState.audio.adcEnabled[i]);
  }
#ifdef USB_AUDIO_ENABLED
  resp["usbAudioEnabled"] = appState.usbAudio.enabled;
#endif
#ifdef GUI_ENABLED
  resp["bootAnimEnabled"] = appState.bootAnimEnabled;
  resp["bootAnimStyle"] = appState.bootAnimStyle;
#endif
  resp["otaChannel"] = appState.ota.channel;
  String json;
  serializeJson(resp, json);
  server_send(200, "application/json", json);
}

void handleSettingsExport() {
  LOG_I("[Settings] Settings export requested via web interface");

  JsonDocument doc;

  // Device info
  doc["deviceInfo"]["manufacturer"] = MANUFACTURER_NAME;
  doc["deviceInfo"]["model"] = MANUFACTURER_MODEL;
  doc["deviceInfo"]["serialNumber"] = appState.general.deviceSerialNumber;
  doc["deviceInfo"]["firmwareVersion"] = firmwareVer;
  doc["deviceInfo"]["mac"] = WiFi.macAddress();
  doc["deviceInfo"]["chipId"] =
      String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF), HEX);

  // WiFi settings
  doc["wifi"]["ssid"] = appState.wifi.ssid;
  doc["wifi"]["password"] = appState.wifi.password;

  // AP settings
  doc["accessPoint"]["enabled"] = appState.wifi.apEnabled;
  doc["accessPoint"]["ssid"] = appState.wifi.apSSID;
  doc["accessPoint"]["password"] = appState.wifi.apPassword;
  doc["accessPoint"]["appState.autoAPEnabled"] = appState.wifi.autoAPEnabled;

  // General settings
  doc["settings"]["appState.autoUpdateEnabled"] = appState.ota.autoUpdateEnabled;
  doc["settings"]["appState.timezoneOffset"] = appState.general.timezoneOffset;
  doc["settings"]["appState.dstOffset"] = appState.general.dstOffset;
  doc["settings"]["appState.darkMode"] = appState.general.darkMode;
  doc["settings"]["appState.enableCertValidation"] = appState.general.enableCertValidation;
  doc["settings"]["appState.debug.hardwareStatsInterval"] = appState.debug.hardwareStatsInterval / 1000;
  doc["settings"]["audioUpdateRate"] = appState.audio.updateRate;
  doc["settings"]["screenTimeout"] = appState.display.screenTimeout / 1000;
  doc["settings"]["buzzerEnabled"] = appState.buzzer.enabled;
  doc["settings"]["buzzerVolume"] = appState.buzzer.volume;
  doc["settings"]["backlightBrightness"] = appState.display.backlightBrightness;
  doc["settings"]["dimEnabled"] = appState.display.dimEnabled;
  doc["settings"]["dimTimeout"] = appState.display.dimTimeout / 1000;
  doc["settings"]["dimBrightness"] = appState.display.dimBrightness;
  doc["settings"]["vuMeterEnabled"] = appState.audio.vuMeterEnabled;
  doc["settings"]["waveformEnabled"] = appState.audio.waveformEnabled;
  doc["settings"]["spectrumEnabled"] = appState.audio.spectrumEnabled;
  doc["settings"]["debugMode"] = appState.debug.debugMode;
  doc["settings"]["debugSerialLevel"] = appState.debug.serialLevel;
  doc["settings"]["debugHwStats"] = appState.debug.hwStats;
  doc["settings"]["debugI2sMetrics"] = appState.debug.i2sMetrics;
  doc["settings"]["debugTaskMonitor"] = appState.debug.taskMonitor;
  doc["settings"]["fftWindowType"] = (int)appState.audio.fftWindowType;
  {
    JsonArray adcArr = doc["settings"]["adcEnabled"].to<JsonArray>();
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) adcArr.add(appState.audio.adcEnabled[i]);
  }
#ifdef USB_AUDIO_ENABLED
  doc["settings"]["usbAudioEnabled"] = appState.usbAudio.enabled;
#endif
#ifdef GUI_ENABLED
  doc["settings"]["bootAnimEnabled"] = appState.bootAnimEnabled;
  doc["settings"]["bootAnimStyle"] = appState.bootAnimStyle;
#endif
  doc["settings"]["otaChannel"] = appState.ota.channel;

  // Smart Sensing settings
  String modeStr;
  switch (appState.audio.currentMode) {
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
  doc["smartSensing"]["appState.timerDuration"] = appState.audio.timerDuration;
  doc["smartSensing"]["audioThreshold"] = appState.audio.threshold_dBFS;
  doc["smartSensing"]["adcVref"] = appState.audio.adcVref;

  // Signal Generator settings
  doc["signalGenerator"]["waveform"] = appState.sigGen.waveform;
  doc["signalGenerator"]["frequency"] = appState.sigGen.frequency;
  doc["signalGenerator"]["amplitude"] = appState.sigGen.amplitude;
  doc["signalGenerator"]["channel"] = appState.sigGen.channel;
  doc["signalGenerator"]["outputMode"] = appState.sigGen.outputMode;
  doc["signalGenerator"]["sweepSpeed"] = appState.sigGen.sweepSpeed;

#ifdef DAC_ENABLED
  // DAC settings are persisted in /hal_config.json — no longer in /config.json
#endif

  // Input channel names
  JsonArray names = doc["inputNames"].to<JsonArray>();
  for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS * 2; i++) {
    names.add(appState.audio.inputNames[i]);
  }

  // MQTT settings (password excluded for security)
  doc["mqtt"]["enabled"] = appState.mqtt.enabled;
  doc["mqtt"]["broker"] = appState.mqtt.broker;
  doc["mqtt"]["port"] = appState.mqtt.port;
  doc["mqtt"]["username"] = appState.mqtt.username;
  doc["mqtt"]["baseTopic"] = appState.mqtt.baseTopic;
  doc["mqtt"]["haDiscovery"] = appState.mqtt.haDiscovery;
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
  server_send(200, "application/json", json);

  LOG_I("[Settings] Settings exported successfully");
}

void handleSettingsImport() {
  LOG_I("[Settings] Settings import requested via web interface");

  if (!server.hasArg("plain")) {
    server_send(400, "application/json",
                "{\"success\": false, \"message\": \"No data received\"}");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    LOG_E("[Settings] JSON parsing failed: %s", error.c_str());
    server_send(400, "application/json",
                "{\"success\": false, \"message\": \"Invalid JSON format\"}");
    return;
  }

  // Validate it's a settings export file
  if (doc["exportInfo"].isNull() || doc["settings"].isNull()) {
    server_send(
        400, "application/json",
        "{\"success\": false, \"message\": \"Invalid settings file format\"}");
    return;
  }

  LOG_I("[Settings] Importing settings");

  // Import WiFi settings
  if (!doc["wifi"].isNull()) {
    if (doc["wifi"]["ssid"].is<String>()) {
      appState.wifi.ssid = doc["wifi"]["ssid"].as<String>();
      LOG_D("[Settings] WiFi SSID: %s", appState.wifi.ssid.c_str());
    }
    if (doc["wifi"]["password"].is<String>()) {
      appState.wifi.password = doc["wifi"]["password"].as<String>();
      LOG_D("[Settings] WiFi password imported");
    }
    // Save WiFi credentials to multi-WiFi list
    if (appState.wifi.ssid.length() > 0) {
      saveWiFiNetwork(appState.wifi.ssid.c_str(), appState.wifi.password.c_str());
    }
  }

  // Import AP settings
  if (!doc["accessPoint"].isNull()) {
    if (doc["accessPoint"]["enabled"].is<bool>()) {
      appState.wifi.apEnabled = doc["accessPoint"]["enabled"].as<bool>();
      LOG_D("[Settings] AP Enabled: %s", appState.wifi.apEnabled ? "true" : "false");
    }
    if (doc["accessPoint"]["ssid"].is<String>()) {
      appState.wifi.apSSID = doc["accessPoint"]["ssid"].as<String>();
      LOG_D("[Settings] AP SSID: %s", appState.wifi.apSSID.c_str());
    }
    if (doc["accessPoint"]["password"].is<String>()) {
      appState.wifi.apPassword = doc["accessPoint"]["password"].as<String>();
      LOG_D("[Settings] AP password imported");
    }
    if (doc["accessPoint"]["appState.autoAPEnabled"].is<bool>()) {
      appState.wifi.autoAPEnabled = doc["accessPoint"]["appState.autoAPEnabled"].as<bool>();
      LOG_D("[Settings] Auto AP: %s", appState.wifi.autoAPEnabled ? "enabled" : "disabled");
    }
  }

  // Import general settings
  if (!doc["settings"].isNull()) {
    if (doc["settings"]["appState.autoUpdateEnabled"].is<bool>()) {
      appState.ota.autoUpdateEnabled = doc["settings"]["appState.autoUpdateEnabled"].as<bool>();
      LOG_D("[Settings] Auto Update: %s",
            appState.ota.autoUpdateEnabled ? "enabled" : "disabled");
    }
    if (doc["settings"]["appState.timezoneOffset"].is<int>()) {
      appState.general.timezoneOffset = doc["settings"]["appState.timezoneOffset"].as<int>();
      LOG_D("[Settings] Timezone Offset: %d", appState.general.timezoneOffset);
    }
    if (doc["settings"]["appState.dstOffset"].is<int>()) {
      appState.general.dstOffset = doc["settings"]["appState.dstOffset"].as<int>();
      LOG_D("[Settings] DST Offset: %d", appState.general.dstOffset);
    }
    if (doc["settings"]["appState.darkMode"].is<bool>()) {
      appState.general.darkMode = doc["settings"]["appState.darkMode"].as<bool>();
      LOG_D("[Settings] Dark Mode: %s", appState.general.darkMode ? "enabled" : "disabled");
    }
    if (doc["settings"]["appState.enableCertValidation"].is<bool>()) {
      appState.general.enableCertValidation = doc["settings"]["appState.enableCertValidation"].as<bool>();
      LOG_D("[Settings] Cert Validation: %s",
            appState.general.enableCertValidation ? "enabled" : "disabled");
    }
    if (doc["settings"]["appState.debug.hardwareStatsInterval"].is<int>()) {
      int interval = doc["settings"]["appState.debug.hardwareStatsInterval"].as<int>();
      if (interval == 1 || interval == 2 || interval == 3 || interval == 5 ||
          interval == 10) {
        appState.debug.hardwareStatsInterval = interval * 1000UL;
        LOG_D("[Settings] Hardware Stats Interval: %d seconds", interval);
      }
    }
    if (doc["settings"]["audioUpdateRate"].is<int>()) {
      int rate = doc["settings"]["audioUpdateRate"].as<int>();
      if (rate == 33 || rate == 50 || rate == 100) {
        appState.audio.updateRate = (uint16_t)rate;
        LOG_D("[Settings] Audio Update Rate: %d ms", rate);
      }
    }
    if (doc["settings"]["screenTimeout"].is<int>()) {
      int timeoutSec = doc["settings"]["screenTimeout"].as<int>();
      unsigned long timeoutMs = (unsigned long)timeoutSec * 1000UL;
      if (timeoutMs == 0 || timeoutMs == 30000 || timeoutMs == 60000 ||
          timeoutMs == 300000 || timeoutMs == 600000) {
        appState.display.screenTimeout = timeoutMs;
        LOG_D("[Settings] Screen Timeout: %d seconds", timeoutSec);
      }
    }
    if (doc["settings"]["buzzerEnabled"].is<bool>()) {
      appState.buzzer.enabled = doc["settings"]["buzzerEnabled"].as<bool>();
      LOG_D("[Settings] Buzzer: %s",
            appState.buzzer.enabled ? "enabled" : "disabled");
    }
    if (doc["settings"]["buzzerVolume"].is<int>()) {
      int vol = doc["settings"]["buzzerVolume"].as<int>();
      if (vol >= 0 && vol <= 2) {
        appState.buzzer.volume = vol;
        LOG_D("[Settings] Buzzer Volume: %d", vol);
      }
    }
    if (doc["settings"]["backlightBrightness"].is<int>()) {
      int bright = doc["settings"]["backlightBrightness"].as<int>();
      if (bright >= 1 && bright <= 255) {
        appState.display.backlightBrightness = (uint8_t)bright;
        LOG_D("[Settings] Backlight Brightness: %d", bright);
      }
    }
    if (doc["settings"]["dimEnabled"].is<bool>()) {
      appState.display.dimEnabled = doc["settings"]["dimEnabled"].as<bool>();
      LOG_D("[Settings] Dim: %s", appState.display.dimEnabled ? "enabled" : "disabled");
    }
    if (doc["settings"]["dimTimeout"].is<int>()) {
      int dimSec = doc["settings"]["dimTimeout"].as<int>();
      unsigned long dimMs = (unsigned long)dimSec * 1000UL;
      if (dimMs == 5000 || dimMs == 10000 || dimMs == 15000 ||
          dimMs == 30000 || dimMs == 60000) {
        appState.display.dimTimeout = dimMs;
        LOG_D("[Settings] Dim Timeout: %d seconds", dimSec);
      }
    }
    if (doc["settings"]["dimBrightness"].is<int>()) {
      int dimBright = doc["settings"]["dimBrightness"].as<int>();
      if (dimBright == 26 || dimBright == 64 || dimBright == 128 ||
          dimBright == 191) {
        appState.display.dimBrightness = (uint8_t)dimBright;
        LOG_D("[Settings] Dim Brightness: %d", dimBright);
      }
    }
    if (doc["settings"]["vuMeterEnabled"].is<bool>()) {
      appState.audio.vuMeterEnabled = doc["settings"]["vuMeterEnabled"].as<bool>();
      LOG_D("[Settings] VU Meter: %s", appState.audio.vuMeterEnabled ? "enabled" : "disabled");
    }
    if (doc["settings"]["waveformEnabled"].is<bool>()) {
      appState.audio.waveformEnabled = doc["settings"]["waveformEnabled"].as<bool>();
      LOG_D("[Settings] Waveform: %s", appState.audio.waveformEnabled ? "enabled" : "disabled");
    }
    if (doc["settings"]["spectrumEnabled"].is<bool>()) {
      appState.audio.spectrumEnabled = doc["settings"]["spectrumEnabled"].as<bool>();
      LOG_D("[Settings] Spectrum: %s", appState.audio.spectrumEnabled ? "enabled" : "disabled");
    }
    if (doc["settings"]["debugMode"].is<bool>()) {
      appState.debug.debugMode = doc["settings"]["debugMode"].as<bool>();
    }
    if (doc["settings"]["debugSerialLevel"].is<int>()) {
      int level = doc["settings"]["debugSerialLevel"].as<int>();
      if (level >= 0 && level <= 3) appState.debug.serialLevel = level;
    }
    if (doc["settings"]["debugHwStats"].is<bool>()) {
      appState.debug.hwStats = doc["settings"]["debugHwStats"].as<bool>();
    }
    if (doc["settings"]["debugI2sMetrics"].is<bool>()) {
      appState.debug.i2sMetrics = doc["settings"]["debugI2sMetrics"].as<bool>();
    }
    if (doc["settings"]["debugTaskMonitor"].is<bool>()) {
      appState.debug.taskMonitor = doc["settings"]["debugTaskMonitor"].as<bool>();
    }
    if (doc["settings"]["fftWindowType"].is<int>()) {
      int wt = doc["settings"]["fftWindowType"].as<int>();
      if (wt >= 0 && wt < FFT_WINDOW_COUNT) appState.audio.fftWindowType = (FftWindowType)wt;
    }
    if (doc["settings"]["adcEnabled"].is<JsonArray>()) {
      JsonArray arr = doc["settings"]["adcEnabled"].as<JsonArray>();
      for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS && i < (int)arr.size(); i++) {
        appState.audio.adcEnabled[i] = arr[i].as<bool>();
        LOG_D("[Settings] ADC%d: %s", i + 1, appState.audio.adcEnabled[i] ? "enabled" : "disabled");
      }
    } else if (doc["settings"]["adcEnabled"].is<bool>()) {
      bool val = doc["settings"]["adcEnabled"].as<bool>();
      for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) appState.audio.adcEnabled[i] = val;
      LOG_D("[Settings] ADC: %s", val ? "enabled" : "disabled");
    }
#ifdef USB_AUDIO_ENABLED
    if (doc["settings"]["usbAudioEnabled"].is<bool>()) {
      appState.usbAudio.enabled = doc["settings"]["usbAudioEnabled"].as<bool>();
      LOG_D("[Settings] USB Audio: %s", appState.usbAudio.enabled ? "enabled" : "disabled");
    }
#endif
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
    if (doc["settings"]["otaChannel"].is<int>()) {
      uint8_t ch = (uint8_t)doc["settings"]["otaChannel"].as<int>();
      if (ch <= 1) appState.ota.channel = ch;
    }
    // Save general settings
    saveSettings();
  }

  // Import Smart Sensing settings
  if (!doc["smartSensing"].isNull()) {
    if (doc["smartSensing"]["mode"].is<String>()) {
      String modeStr = doc["smartSensing"]["mode"].as<String>();
      if (modeStr == "always_on") {
        appState.audio.currentMode = ALWAYS_ON;
      } else if (modeStr == "always_off") {
        appState.audio.currentMode = ALWAYS_OFF;
      } else if (modeStr == "smart_auto") {
        appState.audio.currentMode = SMART_AUTO;
      }
      LOG_D("[Settings] Smart Sensing Mode: %s", modeStr.c_str());
    }
    if (doc["smartSensing"]["appState.timerDuration"].is<int>() ||
        doc["smartSensing"]["appState.timerDuration"].is<unsigned long>()) {
      appState.audio.timerDuration = doc["smartSensing"]["appState.timerDuration"].as<unsigned long>();
      LOG_D("[Settings] Timer Duration: %lu minutes", appState.audio.timerDuration);
    }
    if (doc["smartSensing"]["audioThreshold"].is<float>()) {
      appState.audio.threshold_dBFS = doc["smartSensing"]["audioThreshold"].as<float>();
      LOG_D("[Settings] Audio Threshold: %+.0f dBFS", appState.audio.threshold_dBFS);
    }
    if (doc["smartSensing"]["adcVref"].is<float>()) {
      float vref = doc["smartSensing"]["adcVref"].as<float>();
      if (vref >= 1.0f && vref <= 5.0f) {
        appState.audio.adcVref = vref;
        LOG_D("[Settings] ADC VREF: %.2f V", vref);
      }
    }
    // Save Smart Sensing settings
    saveSmartSensingSettings();
  }

  // Import MQTT settings
  if (!doc["mqtt"].isNull()) {
    if (doc["mqtt"]["enabled"].is<bool>()) {
      appState.mqtt.enabled = doc["mqtt"]["enabled"].as<bool>();
      LOG_D("[Settings] MQTT Enabled: %s", appState.mqtt.enabled ? "true" : "false");
    }
    if (doc["mqtt"]["broker"].is<String>()) {
      appState.mqtt.broker = doc["mqtt"]["broker"].as<String>();
      LOG_D("[Settings] MQTT Broker: %s", appState.mqtt.broker.c_str());
    }
    if (doc["mqtt"]["port"].is<int>()) {
      appState.mqtt.port = doc["mqtt"]["port"].as<int>();
      LOG_D("[Settings] MQTT Port: %d", appState.mqtt.port);
    }
    if (doc["mqtt"]["username"].is<String>()) {
      appState.mqtt.username = doc["mqtt"]["username"].as<String>();
      LOG_D("[Settings] MQTT username imported");
    }
    if (doc["mqtt"]["baseTopic"].is<String>()) {
      appState.mqtt.baseTopic = doc["mqtt"]["baseTopic"].as<String>();
      LOG_D("[Settings] MQTT Base Topic: %s", appState.mqtt.baseTopic.c_str());
    }
    if (doc["mqtt"]["haDiscovery"].is<bool>()) {
      appState.mqtt.haDiscovery = doc["mqtt"]["haDiscovery"].as<bool>();
      LOG_D("[Settings] MQTT HA Discovery: %s",
            appState.mqtt.haDiscovery ? "enabled" : "disabled");
    }
    // Note: Password is not imported for security - user needs to re-enter it
    // Save MQTT settings
    saveMqttSettings();
  }

  // Import Signal Generator settings
  if (!doc["signalGenerator"].isNull()) {
    if (doc["signalGenerator"]["waveform"].is<int>()) {
      int wf = doc["signalGenerator"]["waveform"].as<int>();
      if (wf >= 0 && wf <= 3) appState.sigGen.waveform = wf;
    }
    if (doc["signalGenerator"]["frequency"].is<float>()) {
      float freq = doc["signalGenerator"]["frequency"].as<float>();
      if (freq >= 1.0f && freq <= 22000.0f) appState.sigGen.frequency = freq;
    }
    if (doc["signalGenerator"]["amplitude"].is<float>()) {
      float amp = doc["signalGenerator"]["amplitude"].as<float>();
      if (amp >= -96.0f && amp <= 0.0f) appState.sigGen.amplitude = amp;
    }
    if (doc["signalGenerator"]["channel"].is<int>()) {
      int ch = doc["signalGenerator"]["channel"].as<int>();
      if (ch >= 0 && ch <= 2) appState.sigGen.channel = ch;
    }
    if (doc["signalGenerator"]["outputMode"].is<int>()) {
      int mode = doc["signalGenerator"]["outputMode"].as<int>();
      if (mode >= 0 && mode <= 1) appState.sigGen.outputMode = mode;
    }
    if (doc["signalGenerator"]["sweepSpeed"].is<float>()) {
      float speed = doc["signalGenerator"]["sweepSpeed"].as<float>();
      if (speed >= 1.0f && speed <= 22000.0f) appState.sigGen.sweepSpeed = speed;
    }
    appState.sigGen.enabled = false; // Always boot disabled
    saveSignalGenSettings();
  }

#ifdef DAC_ENABLED
  // Import legacy DAC Output settings → HAL config migration
  if (!doc["dacOutput"].isNull()) {
    HalDeviceManager& mgr = HalDeviceManager::instance();

    // Migrate PCM5102A fields
    HalDevice* pcm = mgr.findByCompatible("ti,pcm5102a");
    if (pcm) {
      HalDeviceConfig* cfg = mgr.getConfig(pcm->getSlot());
      if (cfg) {
        if (doc["dacOutput"]["enabled"].is<bool>()) cfg->enabled = doc["dacOutput"]["enabled"].as<bool>();
        if (doc["dacOutput"]["volume"].is<int>()) {
          int v = doc["dacOutput"]["volume"].as<int>();
          if (v >= 0 && v <= 100) cfg->volume = (uint8_t)v;
        }
        if (doc["dacOutput"]["mute"].is<bool>()) cfg->mute = doc["dacOutput"]["mute"].as<bool>();
        hal_save_device_config(pcm->getSlot());
      }
    }

    // Migrate ES8311 fields
    HalDevice* es = mgr.findByCompatible("everest-semi,es8311");
    if (es) {
      HalDeviceConfig* cfg = mgr.getConfig(es->getSlot());
      if (cfg) {
        if (doc["dacOutput"]["es8311Enabled"].is<bool>()) cfg->enabled = doc["dacOutput"]["es8311Enabled"].as<bool>();
        if (doc["dacOutput"]["es8311Volume"].is<int>()) {
          int v = doc["dacOutput"]["es8311Volume"].as<int>();
          if (v >= 0 && v <= 100) cfg->volume = (uint8_t)v;
        }
        if (doc["dacOutput"]["es8311Mute"].is<bool>()) cfg->mute = doc["dacOutput"]["es8311Mute"].as<bool>();
        hal_save_device_config(es->getSlot());
      }
    }

    LOG_I("[Settings] Legacy dacOutput settings migrated to HAL config");
  }
#endif

  // Import input channel names
  if (!doc["inputNames"].isNull() && doc["inputNames"].is<JsonArray>()) {
    JsonArray names = doc["inputNames"].as<JsonArray>();
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS * 2 && i < (int)names.size(); i++) {
      String name = names[i].as<String>();
      if (name.length() > 0) appState.audio.inputNames[i] = name;
    }
    saveInputNames();
  }

  // Note: Certificate import removed - now using Mozilla certificate bundle

  LOG_I("[Settings] All settings imported successfully");

  // Send success response
  server_send(200, "application/json",
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
  server_send(200, "application/json",
              "{\"success\": true, \"message\": \"Factory reset initiated\"}");

  // Give time for response to be sent
  delay(500);

  // Perform factory reset
  performFactoryReset();
}

void handleReboot() {
  LOG_W("[Settings] Reboot requested via web interface");

  // Send success response before rebooting
  server_send(200, "application/json",
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
  device["serialNumber"] = appState.general.deviceSerialNumber;
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
  system["heapCritical"] = appState.debug.heapCritical;
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
  wifi["mode"] = appState.wifi.isAPMode ? "ap" : "sta";
  wifi["ssid"] = WiFi.SSID();
  wifi["rssi"] = WiFi.RSSI();
  wifi["localIP"] = WiFi.localIP().toString();
  wifi["gateway"] = WiFi.gatewayIP().toString();
  wifi["subnetMask"] = WiFi.subnetMask().toString();
  wifi["dnsIP"] = WiFi.dnsIP().toString();
  wifi["hostname"] = WiFi.getHostname();
  wifi["appState.apEnabled"] = appState.wifi.apEnabled;
  wifi["appState.apSSID"] = appState.wifi.apSSID;
  wifi["apIP"] = WiFi.softAPIP().toString();
  wifi["apClients"] = WiFi.softAPgetStationNum();

  // Get saved networks count
  wifi["savedNetworksCount"] = getWiFiNetworkCount();

  // ===== Settings =====
  JsonObject settings = doc["settings"].to<JsonObject>();
  settings["appState.autoUpdateEnabled"] = appState.ota.autoUpdateEnabled;
  settings["appState.timezoneOffset"] = appState.general.timezoneOffset;
  settings["appState.dstOffset"] = appState.general.dstOffset;
  settings["appState.darkMode"] = appState.general.darkMode;
  settings["appState.enableCertValidation"] = appState.general.enableCertValidation;
  settings["appState.debug.hardwareStatsInterval"] = appState.debug.hardwareStatsInterval;
  settings["audioUpdateRate"] = appState.audio.updateRate;
  settings["screenTimeout"] = appState.display.screenTimeout;
  settings["backlightOn"] = appState.display.backlightOn;
  settings["backlightBrightness"] = appState.display.backlightBrightness;
  settings["dimEnabled"] = appState.display.dimEnabled;
  settings["dimTimeout"] = appState.display.dimTimeout;
  settings["dimBrightness"] = appState.display.dimBrightness;
  settings["buzzerEnabled"] = appState.buzzer.enabled;
  settings["buzzerVolume"] = appState.buzzer.volume;
  settings["vuMeterEnabled"] = appState.audio.vuMeterEnabled;
  settings["waveformEnabled"] = appState.audio.waveformEnabled;
  settings["spectrumEnabled"] = appState.audio.spectrumEnabled;
  settings["debugMode"] = appState.debug.debugMode;
  settings["debugSerialLevel"] = appState.debug.serialLevel;
  settings["debugHwStats"] = appState.debug.hwStats;
  settings["debugI2sMetrics"] = appState.debug.i2sMetrics;
  settings["debugTaskMonitor"] = appState.debug.taskMonitor;
  settings["fftWindowType"] = (int)appState.audio.fftWindowType;
  {
    JsonArray adcArr = settings["adcEnabled"].to<JsonArray>();
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) adcArr.add(appState.audio.adcEnabled[i]);
  }
#ifdef DSP_ENABLED
  // dcBlockEnabled removed (now a DSP preset)
#endif
#ifdef USB_AUDIO_ENABLED
  settings["usbAudioEnabled"] = appState.usbAudio.enabled;
#endif

  // ===== Smart Sensing =====
  JsonObject sensing = doc["smartSensing"].to<JsonObject>();
  String modeStr;
  switch (appState.audio.currentMode) {
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
  sensing["appState.amplifierState"] = appState.audio.amplifierState;
  sensing["appState.timerDuration"] = appState.audio.timerDuration;
  sensing["appState.timerRemaining"] = appState.audio.timerRemaining;
  sensing["audioThreshold"] = appState.audio.threshold_dBFS;
  sensing["audioLevel"] = appState.audio.level_dBFS;
  sensing["appState.lastSignalDetection"] = appState.audio.lastSignalDetection;

  // ===== Audio ADC Diagnostics =====
  {
    JsonObject audioAdcObj = doc["audioAdc"].to<JsonObject>();
    audioAdcObj["numAdcsDetected"] = appState.audio.numAdcsDetected;
    audioAdcObj["sampleRate"] = appState.audio.sampleRate;
    audioAdcObj["adcVref"] = appState.audio.adcVref;
    JsonArray adcArr = audioAdcObj["adcs"].to<JsonArray>();
    for (int a = 0; a < AUDIO_PIPELINE_MAX_INPUTS; a++) {
      JsonObject adcObj = adcArr.add<JsonObject>();
      const AdcState &adc = appState.audio.adc[a];
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
      adcObj["snrDb"] = appState.audio.snrDb[a];
      adcObj["sfdrDb"] = appState.audio.sfdrDb[a];
    }
    // Input names
    JsonArray names = audioAdcObj["inputNames"].to<JsonArray>();
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS * 2; i++) {
      names.add(appState.audio.inputNames[i]);
    }

    // I2S Configuration
    I2sStaticConfig i2sCfg = i2s_audio_get_static_config();
    JsonObject i2sObj = audioAdcObj["i2sConfig"].to<JsonObject>();
    JsonArray i2sAdcArr = i2sObj["adcs"].to<JsonArray>();
    for (int a = 0; a < AUDIO_PIPELINE_MAX_INPUTS; a++) {
      JsonObject c = i2sAdcArr.add<JsonObject>();
      c["mode"] = i2sCfg.adc[a].isMaster ? "Master RX" : "Slave RX";
      c["sampleRate"] = i2sCfg.adc[a].sampleRate;
      c["bitsPerSample"] = i2sCfg.adc[a].bitsPerSample;
      c["channelFormat"] = i2sCfg.adc[a].channelFormat;
      c["dmaBufCount"] = i2sCfg.adc[a].dmaBufCount;
      c["dmaBufLen"] = i2sCfg.adc[a].dmaBufLen;
      c["apll"] = i2sCfg.adc[a].pllEnabled;
      c["mclkHz"] = i2sCfg.adc[a].mclkHz;
      c["commFormat"] = i2sCfg.adc[a].commFormat;
    }
    JsonObject i2sRt = i2sObj["runtime"].to<JsonObject>();
    i2sRt["audioTaskStackFree"] = appState.audio.i2sMetrics.audioTaskStackFree;
    JsonArray bpsArr = i2sRt["buffersPerSec"].to<JsonArray>();
    JsonArray latArr = i2sRt["avgReadLatencyUs"].to<JsonArray>();
    for (int a = 0; a < AUDIO_PIPELINE_MAX_INPUTS; a++) {
      bpsArr.add(appState.audio.i2sMetrics.buffersPerSec[a]);
      latArr.add(appState.audio.i2sMetrics.avgReadLatencyUs[a]);
    }
  }

  // ===== MQTT Settings (password excluded) =====
  JsonObject mqtt = doc["mqtt"].to<JsonObject>();
  mqtt["enabled"] = appState.mqtt.enabled;
  mqtt["broker"] = appState.mqtt.broker;
  mqtt["port"] = appState.mqtt.port;
  mqtt["username"] = appState.mqtt.username;
  mqtt["baseTopic"] = appState.mqtt.baseTopic;
  mqtt["haDiscovery"] = appState.mqtt.haDiscovery;
  mqtt["connected"] = appState.mqtt.connected;

  // ===== OTA Status =====
  JsonObject ota = doc["ota"].to<JsonObject>();
  ota["appState.updateAvailable"] = appState.ota.updateAvailable;
  ota["latestVersion"] = appState.ota.cachedLatestVersion;
  ota["inProgress"] = appState.ota.inProgress;
  ota["status"] = appState.ota.status;

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
  server_send(200, "application/json", json);

  LOG_I("[Settings] Diagnostics exported successfully");
}

// Note: Certificate HTTP API handlers removed - now using Mozilla certificate
// bundle The appState.general.enableCertValidation setting still works to toggle between:
// - ENABLED: Uses Mozilla certificate bundle for validation
// - DISABLED: Insecure mode (no certificate validation)
