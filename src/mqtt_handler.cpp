#include "mqtt_handler.h"
#include "app_state.h"
#include "buzzer_handler.h"
#include "config.h"
#include "crash_log.h"
#include "debug_serial.h"
#include "signal_generator.h"
#include "task_monitor.h"
#include "audio_quality.h"
#include "settings_manager.h"
#include "utils.h"
#include "websocket_handler.h"
#include "ota_updater.h"
#ifdef DSP_ENABLED
#include "dsp_pipeline.h"
#endif
#include <LittleFS.h>
#include <cmath>

// ===== Heap-optimized MQTT helpers =====
// Reusable static buffers — eliminate hundreds of String heap allocations.
// Safe because MQTT publishes are single-threaded (main loop only).
static char _mqttTopicBase[128];    // Cached effective base topic
static char _topicBuf[192];         // Reusable topic buffer
static char _valBuf[32];            // Reusable value conversion buffer
static char _jsonBuf[1024];         // Reusable JSON serialization buffer
static char _deviceIdBuf[24];       // Cached device ID

// Build full MQTT topic from suffix. Returns pointer to static _topicBuf.
// WARNING: Return value is only valid until next mqttTopic() call.
static const char* mqttTopic(const char* suffix) {
  snprintf(_topicBuf, sizeof(_topicBuf), "%s%s", _mqttTopicBase, suffix);
  return _topicBuf;
}

// Update cached topic base — call on connect and when base topic changes
static void updateTopicCache() {
  if (strlen(appState.mqttBaseTopic) > 0) {
    strncpy(_mqttTopicBase, appState.mqttBaseTopic, sizeof(_mqttTopicBase) - 1);
    _mqttTopicBase[sizeof(_mqttTopicBase) - 1] = '\0';
  } else {
    snprintf(_mqttTopicBase, sizeof(_mqttTopicBase), "ALX/%s",
             appState.deviceSerialNumber);
  }
}

// Get device ID as static char* (no heap alloc)
static const char* mqttDeviceId() {
  if (_deviceIdBuf[0] == '\0') {
    uint64_t chipId = ESP.getEfuseMac();
    uint16_t shortId = (uint16_t)(chipId & 0xFFFF);
    snprintf(_deviceIdBuf, sizeof(_deviceIdBuf), "esp32_audio_%04X", shortId);
  }
  return _deviceIdBuf;
}

// Publish an integer value to a topic suffix using _valBuf
static void mqttPubInt(const char* suffix, int value, bool retained = true) {
  snprintf(_valBuf, sizeof(_valBuf), "%d", value);
  mqttClient.publish(mqttTopic(suffix), _valBuf, retained);
}

// Publish a float value to a topic suffix using _valBuf
static void mqttPubFloat(const char* suffix, float value, int decimals = 1, bool retained = true) {
  snprintf(_valBuf, sizeof(_valBuf), "%.*f", decimals, (double)value);
  mqttClient.publish(mqttTopic(suffix), _valBuf, retained);
}

// Publish a string value to a topic suffix
static void mqttPubStr(const char* suffix, const char* value, bool retained = true) {
  mqttClient.publish(mqttTopic(suffix), value, retained);
}

// Publish ON/OFF for bool
static void mqttPubBool(const char* suffix, bool value, bool retained = true) {
  mqttClient.publish(mqttTopic(suffix), value ? "ON" : "OFF", retained);
}

// FFT window type name helper
static const char* fftWindowName(FftWindowType t) {
  switch (t) {
    case FFT_WINDOW_BLACKMAN: return "blackman";
    case FFT_WINDOW_BLACKMAN_HARRIS: return "blackman_harris";
    case FFT_WINDOW_BLACKMAN_NUTTALL: return "blackman_nuttall";
    case FFT_WINDOW_NUTTALL: return "nuttall";
    case FFT_WINDOW_FLAT_TOP: return "flat_top";
    default: return "hann";
  }
}

// State tracking for hardware stats change detection
static unsigned long prevMqttUptime = 0;
static uint32_t prevMqttHeapFree = 0;
static float prevMqttCpuUsage = 0;
static float prevMqttTemperature = 0;

// State tracking for settings change detection
static bool prevMqttDarkMode = false;
static bool prevMqttAutoUpdate = false;
static bool prevMqttCertValidation = true;

// State tracking for USB auto-priority change detection
static bool prevMqttUsbAutoPriority = false;
static uint8_t prevMqttDacSourceInput = 0;

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
extern void sendAudioGraphState();
extern void sendDebugState();
#ifdef DSP_ENABLED
extern void saveDspSettingsDebounced();
#endif

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
    appState.mqttEnabled = (line1.toInt() != 0);
  }

  if (line2.length() > 0) {
    setCharField(appState.mqttBroker, sizeof(appState.mqttBroker), line2.c_str());
  }

  if (line3.length() > 0) {
    int port = line3.toInt();
    if (port > 0 && port <= 65535) {
      appState.mqttPort = port;
    }
  }

  if (line4.length() > 0) {
    setCharField(appState.mqttUsername, sizeof(appState.mqttUsername), line4.c_str());
  }

  if (line5.length() > 0) {
    setCharField(appState.mqttPassword, sizeof(appState.mqttPassword), line5.c_str());
  }

  if (line6.length() > 0) {
    setCharField(appState.mqttBaseTopic, sizeof(appState.mqttBaseTopic), line6.c_str());
  }

  if (line7.length() > 0) {
    appState.mqttHADiscovery = (line7.toInt() != 0);
  }

  LOG_I("[MQTT] Settings loaded - Enabled: %s, Broker: %s:%d", appState.mqttEnabled ? "true" : "false", appState.mqttBroker, appState.mqttPort);
  LOG_I("[MQTT] Base Topic: %s, HA Discovery: %s", appState.mqttBaseTopic, appState.mqttHADiscovery ? "true" : "false");

  return true;
}

// Save MQTT settings to LittleFS
void saveMqttSettings() {
  File file = LittleFS.open("/mqtt_config.txt", "w");
  if (!file) {
    LOG_E("[MQTT] Failed to open settings file for writing");
    return;
  }

  file.println(appState.mqttEnabled ? "1" : "0");
  file.println(appState.mqttBroker);
  file.println(String(appState.mqttPort));
  file.println(appState.mqttUsername);
  file.println(appState.mqttPassword);
  file.println(appState.mqttBaseTopic);
  file.println(appState.mqttHADiscovery ? "1" : "0");
  file.close();

  LOG_I("[MQTT] Settings saved to LittleFS");
}

// Get unique device ID for MQTT client ID and HA discovery
String getMqttDeviceId() {
  return String(mqttDeviceId());
}

// Get effective MQTT base topic (falls back to ALX/{serialNumber} if not
// configured)
String getEffectiveMqttBaseTopic() {
  return String(_mqttTopicBase);
}

// ===== MQTT Core Functions =====

// Subscribe to all command topics
void subscribeToMqttTopics() {
  if (!mqttClient.connected())
    return;

  // Subscribe to command topics
  mqttClient.subscribe(mqttTopic("/led/blinking/set"));
  mqttClient.subscribe(mqttTopic("/smartsensing/mode/set"));
  mqttClient.subscribe(mqttTopic("/smartsensing/amplifier/set"));
  mqttClient.subscribe(mqttTopic("/smartsensing/timer_duration/set"));
  mqttClient.subscribe(mqttTopic("/smartsensing/audio_threshold/set"));
  mqttClient.subscribe(mqttTopic("/ap/enabled/set"));
  mqttClient.subscribe(mqttTopic("/settings/auto_update/set"));
  mqttClient.subscribe(mqttTopic("/settings/dark_mode/set"));
  mqttClient.subscribe(mqttTopic("/settings/cert_validation/set"));
  mqttClient.subscribe(mqttTopic("/settings/screen_timeout/set"));
  mqttClient.subscribe(mqttTopic("/settings/device_name/set"));
  mqttClient.subscribe(mqttTopic("/display/dim_enabled/set"));
  mqttClient.subscribe(mqttTopic("/settings/dim_timeout/set"));
  mqttClient.subscribe(mqttTopic("/display/backlight/set"));
  mqttClient.subscribe(mqttTopic("/display/brightness/set"));
  mqttClient.subscribe(mqttTopic("/display/dim_brightness/set"));
  mqttClient.subscribe(mqttTopic("/settings/buzzer/set"));
  mqttClient.subscribe(mqttTopic("/settings/buzzer_volume/set"));
  mqttClient.subscribe(mqttTopic("/settings/audio_update_rate/set"));
  mqttClient.subscribe(mqttTopic("/system/reboot"));
  mqttClient.subscribe(mqttTopic("/system/factory_reset"));
  mqttClient.subscribe(mqttTopic("/system/check_update"));
  mqttClient.subscribe(mqttTopic("/system/update/command"));
  mqttClient.subscribe(mqttTopic("/signalgenerator/enabled/set"));
  mqttClient.subscribe(mqttTopic("/signalgenerator/waveform/set"));
  mqttClient.subscribe(mqttTopic("/signalgenerator/frequency/set"));
  mqttClient.subscribe(mqttTopic("/signalgenerator/amplitude/set"));
  mqttClient.subscribe(mqttTopic("/signalgenerator/channel/set"));
  mqttClient.subscribe(mqttTopic("/signalgenerator/output_mode/set"));
  mqttClient.subscribe(mqttTopic("/signalgenerator/target_adc/set"));
#ifdef DSP_ENABLED
  mqttClient.subscribe(mqttTopic("/emergency_limiter/enabled/set"));
  mqttClient.subscribe(mqttTopic("/emergency_limiter/threshold/set"));
#endif
  mqttClient.subscribe(mqttTopic("/settings/adc_vref/set"));
  mqttClient.subscribe(mqttTopic("/audio/input1/enabled/set"));
  mqttClient.subscribe(mqttTopic("/audio/input2/enabled/set"));
  mqttClient.subscribe(mqttTopic("/audio/vu_meter/set"));
  mqttClient.subscribe(mqttTopic("/audio/waveform/set"));
  mqttClient.subscribe(mqttTopic("/audio/spectrum/set"));
  mqttClient.subscribe(mqttTopic("/audio/fft_window/set"));
  mqttClient.subscribe(mqttTopic("/debug/mode/set"));
  mqttClient.subscribe(mqttTopic("/debug/serial_level/set"));
  mqttClient.subscribe(mqttTopic("/debug/hw_stats/set"));
  mqttClient.subscribe(mqttTopic("/debug/i2s_metrics/set"));
  mqttClient.subscribe(mqttTopic("/debug/task_monitor/set"));
  mqttClient.subscribe(mqttTopic("/signalgenerator/sweep_speed/set"));
  mqttClient.subscribe(mqttTopic("/settings/timezone_offset/set"));
#ifdef GUI_ENABLED
  mqttClient.subscribe(mqttTopic("/settings/boot_animation/set"));
  mqttClient.subscribe(mqttTopic("/settings/boot_animation_style/set"));
#endif
#ifdef DSP_ENABLED
  mqttClient.subscribe(mqttTopic("/dsp/enabled/set"));
  mqttClient.subscribe(mqttTopic("/dsp/bypass/set"));
  for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
    char chTopic[64];
    snprintf(chTopic, sizeof(chTopic), "/dsp/channel_%d/bypass/set", ch);
    mqttClient.subscribe(mqttTopic(chTopic));
  }
  mqttClient.subscribe(mqttTopic("/dsp/peq/bypass/set"));
  mqttClient.subscribe(mqttTopic("/dsp/preset/set"));
#endif
  mqttClient.subscribe(mqttTopic("/settings/usb_auto_priority/set"));
  mqttClient.subscribe(mqttTopic("/settings/dac_source/set"));

  // Subscribe to HA birth message for re-discovery after HA restart
  mqttClient.subscribe("homeassistant/status");

  LOG_D("[MQTT] Subscribed to command topics");
}

// MQTT callback for incoming messages
void mqttCallback(char *topic, byte *payload, unsigned int length) {
  // Convert payload to string — use stack buffer instead of heap String
  static char _payloadBuf[256];
  unsigned int copyLen = length < sizeof(_payloadBuf) - 1 ? length : sizeof(_payloadBuf) - 1;
  memcpy(_payloadBuf, payload, copyLen);
  _payloadBuf[copyLen] = '\0';
  // Trim trailing whitespace
  while (copyLen > 0 && (_payloadBuf[copyLen-1] == ' ' || _payloadBuf[copyLen-1] == '\r' || _payloadBuf[copyLen-1] == '\n')) {
    _payloadBuf[--copyLen] = '\0';
  }

  // Compare topic suffix efficiently — skip base prefix
  size_t baseLen = strlen(_mqttTopicBase);
  const char *suffix = nullptr;
  if (strncmp(topic, _mqttTopicBase, baseLen) == 0) {
    suffix = topic + baseLen;
  }

  auto parseBool = [](const char* s) -> bool {
    return (strcmp(s, "ON") == 0 || strcmp(s, "1") == 0 || strcmp(s, "true") == 0);
  };

  LOG_D("[MQTT] Received: %s = %s", topic, _payloadBuf);

  // Handle Home Assistant restart — re-publish discovery so HA picks up current device info
  if (strcmp(topic, "homeassistant/status") == 0) {
    if (strcmp(_payloadBuf, "online") == 0) {
      LOG_I("[MQTT] Home Assistant restarted, re-publishing discovery");
      if (appState.mqttHADiscovery) publishHADiscovery();
      publishMqttSystemStatusStatic();
      publishMqttHardwareStatsStatic();
      publishMqttCrashDiagnosticsStatic();
      publishMqttState();
    }
    return;
  }

  // Handle LED blinking control
  if (suffix && strcmp(suffix, "/led/blinking/set") == 0) {
    bool newState = parseBool(_payloadBuf);
    if (appState.blinkingEnabled != newState) {
      appState.blinkingEnabled = newState;
      LOG_I("[MQTT] Blinking set to %s", appState.blinkingEnabled ? "ON" : "OFF");
      sendBlinkingState();

      if (!appState.blinkingEnabled) {
        appState.ledState = false;
        digitalWrite(LED_PIN, LOW);
        sendLEDState();
      }
    }
    publishMqttBlinkingState();
  }
  // Handle Smart Sensing mode
  else if (suffix && strcmp(suffix, "/smartsensing/mode/set") == 0) {
    SensingMode newMode;
    bool validMode = true;

    if (strcmp(_payloadBuf, "always_on") == 0) {
      newMode = ALWAYS_ON;
    } else if (strcmp(_payloadBuf, "always_off") == 0) {
      newMode = ALWAYS_OFF;
    } else if (strcmp(_payloadBuf, "smart_auto") == 0) {
      newMode = SMART_AUTO;
    } else {
      validMode = false;
      LOG_W("[MQTT] Invalid mode: %s", _payloadBuf);
    }

    if (validMode && appState.currentMode != newMode) {
      appState.currentMode = newMode;
      LOG_I("[MQTT] Mode set to %s", _payloadBuf);

      if (appState.currentMode == SMART_AUTO) {
        appState.timerRemaining = appState.timerDuration * 60;
      }

      saveSmartSensingSettings();
      sendSmartSensingStateInternal();
    }
    publishMqttSmartSensingState();
  }
  // Handle amplifier control
  else if (suffix && strcmp(suffix, "/smartsensing/amplifier/set") == 0) {
    bool newState = parseBool(_payloadBuf);
    setAmplifierState(newState);

    if (appState.currentMode == SMART_AUTO) {
      if (newState) {
        appState.timerRemaining = appState.timerDuration * 60;
        appState.lastTimerUpdate = millis();
      } else {
        appState.timerRemaining = 0;
      }
    }
    publishMqttSmartSensingState();
  }
  // Handle timer duration
  else if (suffix && strcmp(suffix, "/smartsensing/timer_duration/set") == 0) {
    int duration = atoi(_payloadBuf);
    if (duration >= 1 && duration <= 60) {
      appState.timerDuration = duration;

      if (appState.currentMode == SMART_AUTO) {
        appState.timerRemaining = appState.timerDuration * 60;
        if (appState.amplifierState) {
          appState.lastTimerUpdate = millis();
        }
      }

      saveSmartSensingSettings();
      sendSmartSensingStateInternal();
      LOG_I("[MQTT] Timer duration set to %d minutes", duration);
    }
    publishMqttSmartSensingState();
  }
  // Handle audio threshold
  else if (suffix && strcmp(suffix, "/smartsensing/audio_threshold/set") == 0) {
    float threshold = atof(_payloadBuf);
    if (threshold >= -96.0 && threshold <= 0.0) {
      appState.audioThreshold_dBFS = threshold;
      saveSmartSensingSettings();
      sendSmartSensingStateInternal();
      LOG_I("[MQTT] Audio threshold set to %+.0f dBFS", threshold);
    }
    publishMqttSmartSensingState();
  }
  // Handle AP toggle
  else if (suffix && strcmp(suffix, "/ap/enabled/set") == 0) {
    bool enabled = parseBool(_payloadBuf);
    appState.apEnabled = enabled;

    if (enabled) {
      if (!appState.isAPMode) {
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(appState.apSSID, appState.apPassword);
        appState.isAPMode = true;
        LOG_I("[MQTT] Access Point enabled");
      }
    } else {
      if (appState.isAPMode && WiFi.status() == WL_CONNECTED) {
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        appState.isAPMode = false;
        LOG_I("[MQTT] Access Point disabled");
      }
    }

    sendWiFiStatus();
    publishMqttWifiStatus();
  }
  // Handle auto-update setting
  else if (suffix && strcmp(suffix, "/settings/auto_update/set") == 0) {
    bool enabled = parseBool(_payloadBuf);
    if (appState.autoUpdateEnabled != enabled) {
      appState.autoUpdateEnabled = enabled;
      saveSettings();
      LOG_I("[MQTT] Auto-update set to %s", enabled ? "ON" : "OFF");
      sendWiFiStatus(); // Broadcast to web clients
    }
    publishMqttSystemStatus();
  }
  // Handle night mode setting
  else if (suffix && strcmp(suffix, "/settings/dark_mode/set") == 0) {
    bool enabled = parseBool(_payloadBuf);
    if (appState.darkMode != enabled) {
      appState.darkMode = enabled;
      saveSettings();
      LOG_I("[MQTT] Dark mode set to %s", enabled ? "ON" : "OFF");
      sendWiFiStatus(); // Dark mode is part of WiFi status in web UI
    }
    publishMqttSystemStatus();
  }
  // Handle certificate validation setting
  else if (suffix && strcmp(suffix, "/settings/cert_validation/set") == 0) {
    bool enabled = parseBool(_payloadBuf);
    if (appState.enableCertValidation != enabled) {
      appState.enableCertValidation = enabled;
      saveSettings();
      LOG_I("[MQTT] Certificate validation set to %s", enabled ? "ON" : "OFF");
      sendWiFiStatus(); // Broadcast to web clients
    }
    publishMqttSystemStatus();
  }
  // Handle screen timeout setting
  else if (suffix && strcmp(suffix, "/settings/screen_timeout/set") == 0) {
    int timeoutSec = atoi(_payloadBuf);
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
  // Handle dim enabled control
  else if (suffix && strcmp(suffix, "/display/dim_enabled/set") == 0) {
    bool newState = parseBool(_payloadBuf);
    appState.setDimEnabled(newState);
    saveSettings();
    LOG_I("[MQTT] Dim %s", newState ? "enabled" : "disabled");
    publishMqttDisplayState();
  }
  // Handle dim timeout setting
  else if (suffix && strcmp(suffix, "/settings/dim_timeout/set") == 0) {
    int dimSec = atoi(_payloadBuf);
    unsigned long dimMs = (unsigned long)dimSec * 1000UL;
    if (dimMs == 5000 || dimMs == 10000 || dimMs == 15000 ||
        dimMs == 30000 || dimMs == 60000) {
      appState.setDimTimeout(dimMs);
      saveSettings();
      LOG_I("[MQTT] Dim timeout set to %d seconds", dimSec);
    }
    publishMqttDisplayState();
  }
  // Handle dim brightness control
  else if (suffix && strcmp(suffix, "/display/dim_brightness/set") == 0) {
    int pct = atoi(_payloadBuf);
    uint8_t pwm = 26; // default
    if (pct == 10) pwm = 26;
    else if (pct == 25) pwm = 64;
    else if (pct == 50) pwm = 128;
    else if (pct == 75) pwm = 191;
    else {
      publishMqttDisplayState();
      return;
    }
    appState.setDimBrightness(pwm);
    saveSettings();
    LOG_I("[MQTT] Dim brightness set to %d%% (PWM %d)", pct, pwm);
    publishMqttDisplayState();
  }
  // Handle backlight control
  else if (suffix && strcmp(suffix, "/display/backlight/set") == 0) {
    bool newState = parseBool(_payloadBuf);
    appState.setBacklightOn(newState);
    LOG_I("[MQTT] Backlight set to %s", newState ? "ON" : "OFF");
    publishMqttDisplayState();
  }
  // Handle brightness control
  else if (suffix && strcmp(suffix, "/display/brightness/set") == 0) {
    int bright = atoi(_payloadBuf);
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
  else if (suffix && strcmp(suffix, "/settings/buzzer/set") == 0) {
    bool enabled = parseBool(_payloadBuf);
    appState.setBuzzerEnabled(enabled);
    saveSettings();
    LOG_I("[MQTT] Buzzer set to %s", enabled ? "ON" : "OFF");
    publishMqttBuzzerState();
  }
  // Handle buzzer volume
  else if (suffix && strcmp(suffix, "/settings/buzzer_volume/set") == 0) {
    int vol = atoi(_payloadBuf);
    if (vol >= 0 && vol <= 2) {
      appState.setBuzzerVolume(vol);
      saveSettings();
      LOG_I("[MQTT] Buzzer volume set to %d", vol);
      publishMqttBuzzerState();
    }
  }
  // Handle audio update rate
  else if (suffix && strcmp(suffix, "/settings/audio_update_rate/set") == 0) {
    int rate = atoi(_payloadBuf);
    if (rate == 20 || rate == 33 || rate == 50 || rate == 100) {
      appState.audioUpdateRate = (uint16_t)rate;
      saveSettings();
      LOG_I("[MQTT] Audio update rate set to %d ms", rate);
      publishMqttDisplayState();
    }
  }
  // Handle signal generator enable/disable
  else if (suffix && strcmp(suffix, "/signalgenerator/enabled/set") == 0) {
    bool newState = parseBool(_payloadBuf);
    appState.sigGenEnabled = newState;
    siggen_apply_params();
    LOG_I("[MQTT] Signal generator %s", newState ? "enabled" : "disabled");
    publishMqttSignalGenState();
    sendSignalGenState();
  }
  // Handle signal generator waveform
  else if (suffix && strcmp(suffix, "/signalgenerator/waveform/set") == 0) {
    int wf = -1;
    if (strcmp(_payloadBuf, "sine") == 0) wf = 0;
    else if (strcmp(_payloadBuf, "square") == 0) wf = 1;
    else if (strcmp(_payloadBuf, "white_noise") == 0) wf = 2;
    else if (strcmp(_payloadBuf, "sweep") == 0) wf = 3;
    if (wf >= 0) {
      appState.sigGenWaveform = wf;
      siggen_apply_params();
      saveSignalGenSettings();
      LOG_I("[MQTT] Signal generator waveform set to %s", _payloadBuf);
      publishMqttSignalGenState();
      sendSignalGenState();
    }
  }
  // Handle signal generator frequency
  else if (suffix && strcmp(suffix, "/signalgenerator/frequency/set") == 0) {
    float freq = atof(_payloadBuf);
    if (freq >= 1.0f && freq <= 22000.0f) {
      appState.sigGenFrequency = freq;
      siggen_apply_params();
      saveSignalGenSettings();
      LOG_I("[MQTT] Signal generator frequency set to %.0f Hz", freq);
      publishMqttSignalGenState();
      sendSignalGenState();
    }
  }
  // Handle signal generator amplitude
  else if (suffix && strcmp(suffix, "/signalgenerator/amplitude/set") == 0) {
    float amp = atof(_payloadBuf);
    if (amp >= -96.0f && amp <= 0.0f) {
      appState.sigGenAmplitude = amp;
      siggen_apply_params();
      saveSignalGenSettings();
      LOG_I("[MQTT] Signal generator amplitude set to %.0f dBFS", amp);
      publishMqttSignalGenState();
      sendSignalGenState();
    }
  }
  // Handle signal generator channel
  else if (suffix && strcmp(suffix, "/signalgenerator/channel/set") == 0) {
    int ch = -1;
    if (strcmp(_payloadBuf, "ch1") == 0) ch = 0;
    else if (strcmp(_payloadBuf, "ch2") == 0) ch = 1;
    else if (strcmp(_payloadBuf, "both") == 0) ch = 2;
    if (ch >= 0) {
      appState.sigGenChannel = ch;
      siggen_apply_params();
      saveSignalGenSettings();
      LOG_I("[MQTT] Signal generator channel set to %s", _payloadBuf);
      publishMqttSignalGenState();
      sendSignalGenState();
    }
  }
  // Handle signal generator output mode
  else if (suffix && strcmp(suffix, "/signalgenerator/output_mode/set") == 0) {
    int mode = -1;
    if (strcmp(_payloadBuf, "software") == 0) mode = 0;
    else if (strcmp(_payloadBuf, "pwm") == 0) mode = 1;
    if (mode >= 0) {
      appState.sigGenOutputMode = mode;
      siggen_apply_params();
      saveSignalGenSettings();
      LOG_I("[MQTT] Signal generator output mode set to %s", _payloadBuf);
      publishMqttSignalGenState();
      sendSignalGenState();
    }
  }
  // Handle signal generator target ADC
  else if (suffix && strcmp(suffix, "/signalgenerator/target_adc/set") == 0) {
    int target = -1;
    if (strcmp(_payloadBuf, "adc1") == 0) target = 0;
    else if (strcmp(_payloadBuf, "adc2") == 0) target = 1;
    else if (strcmp(_payloadBuf, "both") == 0) target = 2;
    else if (strcmp(_payloadBuf, "usb") == 0) target = 3;
    else if (strcmp(_payloadBuf, "all") == 0) target = 4;
    if (target >= 0) {
      appState.sigGenTargetAdc = target;
      siggen_apply_params();
      saveSignalGenSettings();
      LOG_I("[MQTT] Signal generator target ADC set to %s", _payloadBuf);
      publishMqttSignalGenState();
      sendSignalGenState();
    }
  }
#ifdef DSP_ENABLED
  // Handle emergency limiter enabled
  else if (suffix && strcmp(suffix, "/emergency_limiter/enabled/set") == 0) {
    bool newState = parseBool(_payloadBuf);
    appState.setEmergencyLimiterEnabled(newState);
    saveSettings();
    LOG_I("[MQTT] Emergency limiter set to %s", newState ? "ON" : "OFF");
    publishMqttEmergencyLimiterState();
    sendEmergencyLimiterState();
  }
  // Handle emergency limiter threshold
  else if (suffix && strcmp(suffix, "/emergency_limiter/threshold/set") == 0) {
    float threshold = atof(_payloadBuf);
    if (threshold >= -6.0f && threshold <= 0.0f) {
      appState.setEmergencyLimiterThreshold(threshold);
      saveSettings();
      LOG_I("[MQTT] Emergency limiter threshold set to %.2f dBFS", threshold);
      publishMqttEmergencyLimiterState();
      sendEmergencyLimiterState();
    }
  }
#endif
  // Handle USB auto-priority
  else if (suffix && strcmp(suffix, "/settings/usb_auto_priority/set") == 0) {
    bool newState = parseBool(_payloadBuf);
    appState.usbAutoPriority = newState;
    saveSettings();
    LOG_I("[MQTT] USB auto-priority: %s", newState ? "ON" : "OFF");
    publishMqttUsbAutoPriorityState();
  }
  // Handle DAC source input
  else if (suffix && strcmp(suffix, "/settings/dac_source/set") == 0) {
    uint8_t val = 0;
    if (strcmp(_payloadBuf, "ADC1") == 0 || strcmp(_payloadBuf, "0") == 0) val = 0;
    else if (strcmp(_payloadBuf, "ADC2") == 0 || strcmp(_payloadBuf, "1") == 0) val = 1;
    else if (strcmp(_payloadBuf, "USB") == 0 || strcmp(_payloadBuf, "2") == 0) val = 2;
    else { val = 255; } // invalid
    if (val <= 2) {
      appState.dacSourceInput = val;
      saveSettings();
      LOG_I("[MQTT] DAC source input: %d", val);
      publishMqttUsbAutoPriorityState();
    }
  }
  // Handle ADC reference voltage
  else if (suffix && strcmp(suffix, "/settings/adc_vref/set") == 0) {
    float vref = atof(_payloadBuf);
    if (vref >= 1.0f && vref <= 5.0f) {
      appState.adcVref = vref;
      saveSmartSensingSettings();
      LOG_I("[MQTT] ADC VREF set to %.2f V", vref);
      publishMqttAudioDiagnostics();
    }
  }
  // Handle per-ADC enable/disable
  else if (suffix && strcmp(suffix, "/audio/input1/enabled/set") == 0) {
    bool newState = parseBool(_payloadBuf);
    appState.adcEnabled[0] = newState;
    saveSettings();
    appState.markAdcEnabledDirty();
    LOG_I("[MQTT] ADC1 set to %s", newState ? "ON" : "OFF");
  }
  else if (suffix && strcmp(suffix, "/audio/input2/enabled/set") == 0) {
    bool newState = parseBool(_payloadBuf);
    appState.adcEnabled[1] = newState;
    saveSettings();
    appState.markAdcEnabledDirty();
    LOG_I("[MQTT] ADC2 set to %s", newState ? "ON" : "OFF");
  }
  // Handle VU meter toggle
  else if (suffix && strcmp(suffix, "/audio/vu_meter/set") == 0) {
    bool newState = parseBool(_payloadBuf);
    appState.vuMeterEnabled = newState;
    saveSettings();
    sendAudioGraphState();
    publishMqttAudioGraphState();
    LOG_I("[MQTT] VU meter set to %s", newState ? "ON" : "OFF");
  }
  // Handle waveform toggle
  else if (suffix && strcmp(suffix, "/audio/waveform/set") == 0) {
    bool newState = parseBool(_payloadBuf);
    appState.waveformEnabled = newState;
    saveSettings();
    sendAudioGraphState();
    publishMqttAudioGraphState();
    LOG_I("[MQTT] Waveform set to %s", newState ? "ON" : "OFF");
  }
  // Handle spectrum toggle
  else if (suffix && strcmp(suffix, "/audio/spectrum/set") == 0) {
    bool newState = parseBool(_payloadBuf);
    appState.spectrumEnabled = newState;
    saveSettings();
    sendAudioGraphState();
    publishMqttAudioGraphState();
    LOG_I("[MQTT] Spectrum set to %s", newState ? "ON" : "OFF");
  }
  // Handle FFT window type
  else if (suffix && strcmp(suffix, "/audio/fft_window/set") == 0) {
    // Accept window name strings: hann, blackman, blackman_harris, blackman_nuttall, nuttall, flat_top
    FftWindowType wt = FFT_WINDOW_HANN;
    if (strcmp(_payloadBuf, "blackman") == 0) wt = FFT_WINDOW_BLACKMAN;
    else if (strcmp(_payloadBuf, "blackman_harris") == 0) wt = FFT_WINDOW_BLACKMAN_HARRIS;
    else if (strcmp(_payloadBuf, "blackman_nuttall") == 0) wt = FFT_WINDOW_BLACKMAN_NUTTALL;
    else if (strcmp(_payloadBuf, "nuttall") == 0) wt = FFT_WINDOW_NUTTALL;
    else if (strcmp(_payloadBuf, "flat_top") == 0) wt = FFT_WINDOW_FLAT_TOP;
    appState.fftWindowType = wt;
    saveSettings();
    sendAudioGraphState();
    publishMqttAudioGraphState();
    LOG_I("[MQTT] FFT window set to %d", (int)wt);
  }
  // Handle debug mode toggle
  else if (suffix && strcmp(suffix, "/debug/mode/set") == 0) {
    bool newState = parseBool(_payloadBuf);
    appState.debugMode = newState;
    applyDebugSerialLevel(appState.debugMode, appState.debugSerialLevel);
    saveSettings();
    sendDebugState();
    publishMqttDebugState();
    LOG_I("[MQTT] Debug mode set to %s", newState ? "ON" : "OFF");
  }
  // Handle debug serial level
  else if (suffix && strcmp(suffix, "/debug/serial_level/set") == 0) {
    int level = atoi(_payloadBuf);
    if (level >= 0 && level <= 3) {
      appState.debugSerialLevel = level;
      applyDebugSerialLevel(appState.debugMode, appState.debugSerialLevel);
      saveSettings();
      sendDebugState();
      publishMqttDebugState();
      LOG_I("[MQTT] Debug serial level set to %d", level);
    }
  }
  // Handle debug HW stats toggle
  else if (suffix && strcmp(suffix, "/debug/hw_stats/set") == 0) {
    bool newState = parseBool(_payloadBuf);
    appState.debugHwStats = newState;
    saveSettings();
    sendDebugState();
    publishMqttDebugState();
    LOG_I("[MQTT] Debug HW stats set to %s", newState ? "ON" : "OFF");
  }
  // Handle debug I2S metrics toggle
  else if (suffix && strcmp(suffix, "/debug/i2s_metrics/set") == 0) {
    bool newState = parseBool(_payloadBuf);
    appState.debugI2sMetrics = newState;
    saveSettings();
    sendDebugState();
    publishMqttDebugState();
    LOG_I("[MQTT] Debug I2S metrics set to %s", newState ? "ON" : "OFF");
  }
  // Handle debug task monitor toggle
  else if (suffix && strcmp(suffix, "/debug/task_monitor/set") == 0) {
    bool newState = parseBool(_payloadBuf);
    appState.debugTaskMonitor = newState;
    saveSettings();
    sendDebugState();
    publishMqttDebugState();
    LOG_I("[MQTT] Debug task monitor set to %s", newState ? "ON" : "OFF");
  }
  // Handle timezone offset
  else if (suffix && strcmp(suffix, "/settings/timezone_offset/set") == 0) {
    int offset = atoi(_payloadBuf);
    if (offset >= -12 && offset <= 14) {
      appState.timezoneOffset = offset;
      saveSettings();
      LOG_I("[MQTT] Timezone offset set to %d", offset);
      publishMqttSystemStatus();
    }
  }
  // Handle signal generator sweep speed
  else if (suffix && strcmp(suffix, "/signalgenerator/sweep_speed/set") == 0) {
    float speed = atof(_payloadBuf);
    if (speed >= 0.1f && speed <= 10.0f) {
      appState.sigGenSweepSpeed = speed;
      siggen_apply_params();
      saveSignalGenSettings();
      LOG_I("[MQTT] Signal generator sweep speed set to %.1f Hz/s", speed);
      publishMqttSignalGenState();
      sendSignalGenState();
    }
  }
#ifdef GUI_ENABLED
  // Handle boot animation enabled
  else if (suffix && strcmp(suffix, "/settings/boot_animation/set") == 0) {
    bool newState = parseBool(_payloadBuf);
    appState.bootAnimEnabled = newState;
    saveSettings();
    LOG_I("[MQTT] Boot animation set to %s", newState ? "ON" : "OFF");
    publishMqttBootAnimState();
  }
  // Handle boot animation style
  else if (suffix && strcmp(suffix, "/settings/boot_animation_style/set") == 0) {
    int style = -1;
    if (strcmp(_payloadBuf, "wave_pulse") == 0) style = 0;
    else if (strcmp(_payloadBuf, "speaker_ripple") == 0) style = 1;
    else if (strcmp(_payloadBuf, "waveform") == 0) style = 2;
    else if (strcmp(_payloadBuf, "beat_bounce") == 0) style = 3;
    else if (strcmp(_payloadBuf, "freq_bars") == 0) style = 4;
    else if (strcmp(_payloadBuf, "heartbeat") == 0) style = 5;
    if (style >= 0) {
      appState.bootAnimStyle = style;
      saveSettings();
      LOG_I("[MQTT] Boot animation style set to %s", _payloadBuf);
      publishMqttBootAnimState();
    }
  }
#endif
#ifdef DSP_ENABLED
  // Handle DSP enable/disable
  else if (suffix && strcmp(suffix, "/dsp/enabled/set") == 0) {
    bool newState = parseBool(_payloadBuf);
    appState.dspEnabled = newState;
    saveDspSettingsDebounced();
    appState.markDspConfigDirty();
    LOG_I("[MQTT] DSP set to %s", newState ? "ON" : "OFF");
  }
  // Handle DSP global bypass
  else if (suffix && strcmp(suffix, "/dsp/bypass/set") == 0) {
    bool newState = parseBool(_payloadBuf);
    appState.dspBypass = newState;
    dsp_copy_active_to_inactive();
    DspState *cfg = dsp_get_inactive_config();
    cfg->globalBypass = newState;
    if (!dsp_swap_config()) { appState.dspSwapFailures++; appState.lastDspSwapFailure = millis(); LOG_W("[MQTT] Swap failed, staged for retry"); }
    saveDspSettingsDebounced();
    appState.markDspConfigDirty();
    LOG_I("[MQTT] DSP bypass set to %s", newState ? "ON" : "OFF");
  }
  // Handle per-channel DSP bypass
  else if (suffix && strncmp(suffix, "/dsp/channel_", 13) == 0 && strstr(suffix, "/bypass/set")) {
    int ch = atoi(suffix + 13);  // Skip "/dsp/channel_"
    if (ch >= 0 && ch < DSP_MAX_CHANNELS) {
      bool newState = parseBool(_payloadBuf);
      dsp_copy_active_to_inactive();
      DspState *cfg = dsp_get_inactive_config();
      cfg->channels[ch].bypass = newState;
      if (!dsp_swap_config()) { appState.dspSwapFailures++; appState.lastDspSwapFailure = millis(); LOG_W("[MQTT] Swap failed, staged for retry"); }
      saveDspSettingsDebounced();
      appState.markDspConfigDirty();
      LOG_I("[MQTT] DSP channel %d bypass set to %s", ch, newState ? "ON" : "OFF");
    }
  }
  // PEQ band enable/disable (L1/R1 = channels 0/1)
  else if (suffix && strncmp(suffix, "/dsp/channel_", 13) == 0 && strstr(suffix, "/peq/band")) {
    int ch = atoi(suffix + 13);
    const char* bandStr = strstr(suffix, "/peq/band");
    if (bandStr) {
      int band = atoi(bandStr + 9) - 1;  // "/peq/band" is 9 chars
      if (ch >= 0 && ch < 2 && band >= 0 && band < DSP_PEQ_BANDS) {
        bool newState = parseBool(_payloadBuf);
        dsp_copy_active_to_inactive();
        DspState *cfg = dsp_get_inactive_config();
        cfg->channels[ch].stages[band].enabled = newState;
        if (!dsp_swap_config()) { appState.dspSwapFailures++; appState.lastDspSwapFailure = millis(); LOG_W("[MQTT] Swap failed, staged for retry"); }
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();
        LOG_I("[MQTT] PEQ ch%d band%d set to %s", ch, band + 1, newState ? "ON" : "OFF");
      }
    }
  }
  // PEQ bypass (disable/enable all PEQ bands on all channels)
  else if (suffix && strcmp(suffix, "/dsp/peq/bypass/set") == 0) {
    bool bypass = parseBool(_payloadBuf);
    dsp_copy_active_to_inactive();
    DspState *cfg = dsp_get_inactive_config();
    for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
      for (int b = 0; b < DSP_PEQ_BANDS && b < cfg->channels[ch].stageCount; b++) {
        cfg->channels[ch].stages[b].enabled = !bypass;
      }
    }
    if (!dsp_swap_config()) { appState.dspSwapFailures++; appState.lastDspSwapFailure = millis(); LOG_W("[MQTT] Swap failed, staged for retry"); }
    saveDspSettingsDebounced();
    appState.markDspConfigDirty();
    LOG_I("[MQTT] PEQ bypass set to %s", bypass ? "ON" : "OFF");
  }
  // DSP config preset select
  else if (suffix && strcmp(suffix, "/dsp/preset/set") == 0) {
    int slot = atoi(_payloadBuf);
    if (strcmp(_payloadBuf, "Custom") == 0 || strcmp(_payloadBuf, "-1") == 0) {
      // No action — "Custom" means user manually modified config
    } else if (slot >= 0 && slot < DSP_PRESET_MAX_SLOTS) {
      extern bool dsp_preset_load(int);
      if (dsp_preset_load(slot)) {
        appState.markDspConfigDirty();
        LOG_I("[MQTT] DSP preset %d loaded", slot);
      }
    }
  }
#endif
  // Handle reboot command
  else if (suffix && strcmp(suffix, "/system/reboot") == 0) {
    LOG_W("[MQTT] Reboot command received");
    buzzer_play_blocking(BUZZ_SHUTDOWN, 1200);
    ESP.restart();
  }
  // Handle factory reset command
  else if (suffix && strcmp(suffix, "/system/factory_reset") == 0) {
    LOG_W("[MQTT] Factory reset command received");
    delay(500);
    performFactoryReset();
  }
  // Handle update check command
  else if (suffix && strcmp(suffix, "/system/check_update") == 0) {
    LOG_I("[MQTT] Update check command received");
    checkForFirmwareUpdate();
    publishMqttSystemStatus();
    publishMqttUpdateState();
  }
  // Handle update install command (from HA Update entity)
  else if (suffix && strcmp(suffix, "/system/update/command") == 0) {
    if (strcmp(_payloadBuf, "install") == 0) {
      LOG_I("[MQTT] Firmware install command received from Home Assistant");
      if (appState.updateAvailable && appState.cachedFirmwareUrl.length() > 0) {
        startOTADownloadTask();  // Non-blocking FreeRTOS task
      } else {
        LOG_W("[MQTT] No update available or firmware URL missing");
      }
    }
  }
  // Handle custom device name
  else if (suffix && strcmp(suffix, "/settings/device_name/set") == 0) {
    char name[33];
    strncpy(name, _payloadBuf, 32);
    name[32] = '\0';
    setCharField(appState.customDeviceName, sizeof(appState.customDeviceName), name);
    // Update AP SSID to reflect new custom name
    String apName = strlen(appState.customDeviceName) > 0
                      ? String(appState.customDeviceName)
                      : ("ALX-Nova-" + String(appState.deviceSerialNumber));
    if ((int)apName.length() > 32) apName = apName.substring(0, 32);
    setCharField(appState.apSSID, sizeof(appState.apSSID), apName.c_str());
    saveSettings();
    sendWiFiStatus(); // Broadcast to web clients
    LOG_I("[MQTT] Custom device name set to: '%s'", name);
    mqttClient.publish(mqttTopic("/settings/device_name"), name, true);
  }
}

// Setup MQTT client
void setupMqtt() {
  updateTopicCache();  // Cache base topic before any MQTT operations

  if (!appState.mqttEnabled || strlen(appState.mqttBroker) == 0) {
    LOG_I("[MQTT] Disabled or no broker configured");
    return;
  }

  LOG_I("[MQTT] Setting up...");
  LOG_I("[MQTT] Broker: %s:%d", appState.mqttBroker, appState.mqttPort);
  LOG_I("[MQTT] Base Topic: %s", appState.mqttBaseTopic);
  LOG_I("[MQTT] HA Discovery: %s", appState.mqttHADiscovery ? "enabled" : "disabled");

  mqttClient.setServer(appState.mqttBroker, appState.mqttPort);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(1024); // Increase buffer for HA discovery payloads

  // Attempt initial connection
  mqttReconnect();
}

// Reconnect to MQTT broker with exponential backoff
void mqttReconnect() {
  if (!appState.mqttEnabled || strlen(appState.mqttBroker) == 0) {
    return;
  }

  if (mqttClient.connected()) {
    return;
  }

  // Respect backoff interval (exponential backoff on failures)
  unsigned long currentMillis = millis();
  if (currentMillis - appState.lastMqttReconnect < appState.mqttBackoffDelay) {
    return;
  }
  appState.lastMqttReconnect = currentMillis;

  LOG_I("[MQTT] Connecting to broker (backoff: %lums)...", appState.mqttBackoffDelay);

  // Pre-establish TCP connection with 1s timeout (default is 3s).
  // If the broker is unreachable this limits the main loop block to ~1s.
  // PubSubClient.connect() will reuse the existing TCP connection.
  if (!mqttWifiClient.connected()) {
    if (!mqttWifiClient.connect(appState.mqttBroker, appState.mqttPort, 1000)) {
      LOG_W("[MQTT] TCP connect timeout (1s) to %s:%d", appState.mqttBroker, appState.mqttPort);
      appState.mqttConnected = false;
      appState.increaseMqttBackoff();
      LOG_W("[MQTT] Next retry in %lums", appState.mqttBackoffDelay);
      return;
    }
  }

  String clientId = getMqttDeviceId();
  String lwt = getEffectiveMqttBaseTopic() + "/status";

  bool connected = false;

  if (strlen(appState.mqttUsername) > 0) {
    connected = mqttClient.connect(clientId.c_str(), appState.mqttUsername,
                                   appState.mqttPassword, lwt.c_str(), 0, true,
                                   "offline");
  } else {
    connected =
        mqttClient.connect(clientId.c_str(), lwt.c_str(), 0, true, "offline");
  }

  if (connected) {
    LOG_I("[MQTT] Connected to %s:%d", appState.mqttBroker, appState.mqttPort);
    appState.mqttConnected = true;
    appState.resetMqttBackoff(); // Reset backoff on successful connection

    // Publish online status
    mqttClient.publish(lwt.c_str(), "online", true);

    // Subscribe to command topics
    subscribeToMqttTopics();

    // Publish Home Assistant discovery configs if enabled
    if (appState.mqttHADiscovery) {
      publishHADiscovery();
      LOG_I("[MQTT] Home Assistant discovery published");
    }

    // Publish static info (once per connection — never changes at runtime)
    publishMqttSystemStatusStatic();
    publishMqttHardwareStatsStatic();
    publishMqttCrashDiagnosticsStatic();

    // Publish initial dynamic state
    publishMqttState();
  } else {
    LOG_W("[MQTT] Connection failed (rc=%d)", mqttClient.state());
    appState.mqttConnected = false;
    appState.increaseMqttBackoff(); // Increase backoff on failure
    LOG_W("[MQTT] Next retry in %lums", appState.mqttBackoffDelay);
  }
}

// MQTT loop - call from main loop()
void mqttLoop() {
  if (!appState.mqttEnabled || strlen(appState.mqttBroker) == 0) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (!mqttClient.connected()) {
    appState.mqttConnected = false;
    mqttReconnect();
  }

  mqttClient.loop();

  // Periodic state publishing — per-category selective dispatch
  unsigned long currentMillis = millis();
  if (mqttClient.connected() &&
      (currentMillis - appState.lastMqttPublish >= MQTT_PUBLISH_INTERVAL)) {
    appState.lastMqttPublish = currentMillis;

    // Per-category change detection
    bool audioLevelChanged =
        (fabs(appState.audioLevel_dBFS - appState.prevMqttAudioLevel) > 0.5f);
    bool ledChanged = (appState.ledState != appState.prevMqttLedState);
    bool blinkingChanged = (appState.blinkingEnabled != appState.prevMqttBlinkingEnabled);
    bool sensingChanged =
        (appState.amplifierState != appState.prevMqttAmplifierState) ||
        (appState.currentMode != appState.prevMqttSensingMode) ||
        (appState.timerRemaining != appState.prevMqttTimerRemaining);
    bool displayChanged =
        (appState.backlightOn != appState.prevMqttBacklightOn) ||
        (appState.screenTimeout != appState.prevMqttScreenTimeout) ||
        (appState.backlightBrightness != appState.prevMqttBrightness) ||
        (appState.dimEnabled != appState.prevMqttDimEnabled) ||
        (appState.dimTimeout != appState.prevMqttDimTimeout) ||
        (appState.dimBrightness != appState.prevMqttDimBrightness);
    bool settingsChanged =
        (appState.darkMode != prevMqttDarkMode) ||
        (appState.autoUpdateEnabled != prevMqttAutoUpdate) ||
        (appState.enableCertValidation != prevMqttCertValidation);
    bool buzzerChanged =
        (appState.buzzerEnabled != appState.prevMqttBuzzerEnabled) ||
        (appState.buzzerVolume != appState.prevMqttBuzzerVolume);
    bool siggenChanged =
        (appState.sigGenEnabled != appState.prevMqttSigGenEnabled) ||
        (appState.sigGenWaveform != appState.prevMqttSigGenWaveform) ||
        (fabs(appState.sigGenFrequency - appState.prevMqttSigGenFrequency) > 0.5f) ||
        (fabs(appState.sigGenAmplitude - appState.prevMqttSigGenAmplitude) > 0.5f) ||
        (appState.sigGenOutputMode != appState.prevMqttSigGenOutputMode) ||
        (fabs(appState.sigGenSweepSpeed - appState.prevMqttSigGenSweepSpeed) > 0.05f);
    bool audioGraphChanged =
        (appState.vuMeterEnabled != appState.prevMqttVuMeterEnabled) ||
        (appState.waveformEnabled != appState.prevMqttWaveformEnabled) ||
        (appState.spectrumEnabled != appState.prevMqttSpectrumEnabled) ||
        (appState.fftWindowType != appState.prevMqttFftWindowType);
    bool debugChanged =
        (appState.debugMode != appState.prevMqttDebugMode) ||
        (appState.debugSerialLevel != appState.prevMqttDebugSerialLevel) ||
        (appState.debugHwStats != appState.prevMqttDebugHwStats) ||
        (appState.debugI2sMetrics != appState.prevMqttDebugI2sMetrics) ||
        (appState.debugTaskMonitor != appState.prevMqttDebugTaskMonitor);
    bool usbAutoPriorityChanged =
        (appState.usbAutoPriority != prevMqttUsbAutoPriority) ||
        (appState.dacSourceInput != prevMqttDacSourceInput);

    // Selective dispatch — only publish categories that actually changed
    if (ledChanged) {
      publishMqttLedState();
      appState.prevMqttLedState = appState.ledState;
    }
    if (blinkingChanged) {
      publishMqttBlinkingState();
      appState.prevMqttBlinkingEnabled = appState.blinkingEnabled;
    }
    // Smart sensing + audio level (combined to avoid double-publishing)
    if (sensingChanged || audioLevelChanged) {
      publishMqttSmartSensingState();
      if (audioLevelChanged) {
        publishMqttAudioDiagnostics();
        appState.prevMqttAudioLevel = appState.audioLevel_dBFS;
      }
      if (sensingChanged) {
        appState.prevMqttAmplifierState = appState.amplifierState;
        appState.prevMqttSensingMode = appState.currentMode;
        appState.prevMqttTimerRemaining = appState.timerRemaining;
      }
    }
    if (displayChanged) {
      publishMqttDisplayState();
      appState.prevMqttBacklightOn = appState.backlightOn;
      appState.prevMqttScreenTimeout = appState.screenTimeout;
      appState.prevMqttBrightness = appState.backlightBrightness;
      appState.prevMqttDimEnabled = appState.dimEnabled;
      appState.prevMqttDimTimeout = appState.dimTimeout;
      appState.prevMqttDimBrightness = appState.dimBrightness;
    }
    if (settingsChanged) {
      publishMqttSystemStatus();
      prevMqttDarkMode = appState.darkMode;
      prevMqttAutoUpdate = appState.autoUpdateEnabled;
      prevMqttCertValidation = appState.enableCertValidation;
    }
    if (buzzerChanged) {
      publishMqttBuzzerState();
      appState.prevMqttBuzzerEnabled = appState.buzzerEnabled;
      appState.prevMqttBuzzerVolume = appState.buzzerVolume;
    }
    if (siggenChanged) {
      publishMqttSignalGenState();
      appState.prevMqttSigGenEnabled = appState.sigGenEnabled;
      appState.prevMqttSigGenWaveform = appState.sigGenWaveform;
      appState.prevMqttSigGenFrequency = appState.sigGenFrequency;
      appState.prevMqttSigGenAmplitude = appState.sigGenAmplitude;
      appState.prevMqttSigGenOutputMode = appState.sigGenOutputMode;
      appState.prevMqttSigGenSweepSpeed = appState.sigGenSweepSpeed;
    }
    if (audioGraphChanged) {
      publishMqttAudioGraphState();
      appState.prevMqttVuMeterEnabled = appState.vuMeterEnabled;
      appState.prevMqttWaveformEnabled = appState.waveformEnabled;
      appState.prevMqttSpectrumEnabled = appState.spectrumEnabled;
      appState.prevMqttFftWindowType = appState.fftWindowType;
    }
    if (appState.isAdcEnabledDirty()) {
      publishMqttAdcEnabledState();
      appState.clearAdcEnabledDirty();
    }
    if (debugChanged) {
      publishMqttDebugState();
      appState.prevMqttDebugMode = appState.debugMode;
      appState.prevMqttDebugSerialLevel = appState.debugSerialLevel;
      appState.prevMqttDebugHwStats = appState.debugHwStats;
      appState.prevMqttDebugI2sMetrics = appState.debugI2sMetrics;
      appState.prevMqttDebugTaskMonitor = appState.debugTaskMonitor;
    }
    if (usbAutoPriorityChanged) {
      publishMqttUsbAutoPriorityState();
      prevMqttUsbAutoPriority = appState.usbAutoPriority;
      prevMqttDacSourceInput = appState.dacSourceInput;
    }
#ifdef GUI_ENABLED
    if ((appState.bootAnimEnabled != appState.prevMqttBootAnimEnabled) ||
        (appState.bootAnimStyle != appState.prevMqttBootAnimStyle)) {
      publishMqttBootAnimState();
      appState.prevMqttBootAnimEnabled = appState.bootAnimEnabled;
      appState.prevMqttBootAnimStyle = appState.bootAnimStyle;
    }
#endif
#ifdef DSP_ENABLED
    if ((appState.dspEnabled != appState.prevMqttDspEnabled) ||
        (appState.dspBypass != appState.prevMqttDspBypass) ||
        (appState.dspPresetIndex != appState.prevMqttDspPresetIndex)) {
      publishMqttDspState();
      appState.prevMqttDspEnabled = appState.dspEnabled;
      appState.prevMqttDspBypass = appState.dspBypass;
      appState.prevMqttDspPresetIndex = appState.dspPresetIndex;
    }
#endif
  }

  // 60-second heartbeat — essential status even when nothing changes
  static unsigned long lastMqttHeartbeat = 0;
  if (mqttClient.connected() &&
      (currentMillis - lastMqttHeartbeat >= MQTT_HEARTBEAT_INTERVAL)) {
    lastMqttHeartbeat = currentMillis;
    publishMqttSmartSensingState();
    publishMqttWifiStatus();
    publishMqttCrashDiagnostics();
    publishMqttHardwareStats();
    mqttPubInt("/system/uptime", (int)(millis() / 1000));
  }
}

// ===== MQTT State Publishing Functions =====

// Publish LED state
void publishMqttLedState() {
  if (!mqttClient.connected())
    return;

  mqttPubBool("/led/state", appState.ledState);
}

// Publish blinking state
void publishMqttBlinkingState() {
  if (!mqttClient.connected())
    return;

  mqttPubBool("/led/blinking", appState.blinkingEnabled);
}

// Publish Smart Sensing state
void publishMqttSmartSensingState() {
  if (!mqttClient.connected())
    return;

  // Convert mode enum to string
  const char* modeStr = "smart_auto";
  switch (appState.currentMode) {
  case ALWAYS_ON:
    modeStr = "always_on";
    break;
  case ALWAYS_OFF:
    modeStr = "always_off";
    break;
  default:
    break;
  }

  mqttPubStr("/smartsensing/mode", modeStr);
  mqttPubBool("/smartsensing/amplifier", appState.amplifierState);
  mqttPubInt("/smartsensing/timer_duration", appState.timerDuration);
  mqttPubInt("/smartsensing/timer_remaining", appState.timerRemaining);
  mqttPubFloat("/smartsensing/audio_level", appState.audioLevel_dBFS, 1);
  mqttPubFloat("/smartsensing/audio_threshold", appState.audioThreshold_dBFS, 1);
  mqttClient.publish(mqttTopic("/smartsensing/signal_detected"),
                     (appState.audioLevel_dBFS >= appState.audioThreshold_dBFS) ? "ON" : "OFF",
                     true);

  // Last signal detection timestamp (seconds since boot, 0 if never detected)
  unsigned long lastDetectionSecs =
      appState.lastSignalDetection > 0 ? appState.lastSignalDetection / 1000 : 0;
  mqttPubInt("/smartsensing/last_detection_time", (int)lastDetectionSecs);
}

// Publish WiFi status
void publishMqttWifiStatus() {
  if (!mqttClient.connected())
    return;

  bool connected = (WiFi.status() == WL_CONNECTED);
  mqttPubBool("/wifi/connected", connected);

  if (connected) {
    int rssi = WiFi.RSSI();
    mqttPubInt("/wifi/rssi", rssi);
    mqttPubInt("/wifi/signal_quality", rssiToQuality(rssi));
    mqttPubStr("/wifi/ip", WiFi.localIP().toString().c_str());
    mqttPubStr("/wifi/ssid", WiFi.SSID().c_str());
  }

  mqttPubBool("/ap/enabled", appState.apEnabled);

  if (appState.isAPMode) {
    mqttPubStr("/ap/ip", WiFi.softAPIP().toString().c_str());
    mqttPubStr("/ap/ssid", appState.apSSID);
  }
}

// Publish static system info (called once on MQTT connect)
void publishMqttSystemStatusStatic() {
  if (!mqttClient.connected())
    return;

  mqttPubStr("/system/manufacturer", MANUFACTURER_NAME);
  mqttPubStr("/system/model", MANUFACTURER_MODEL);
  mqttPubStr("/system/serial_number", appState.deviceSerialNumber);
  mqttPubStr("/system/firmware", firmwareVer);
  mqttPubStr("/system/mac", WiFi.macAddress().c_str());
  mqttPubStr("/system/reset_reason", getResetReasonString().c_str());
}

// Publish dynamic system status (on settings change)
void publishMqttSystemStatus() {
  if (!mqttClient.connected())
    return;

  mqttPubBool("/system/update_available", appState.updateAvailable);
  if (appState.cachedLatestVersion.length() > 0) {
    mqttPubStr("/system/latest_version", appState.cachedLatestVersion.c_str());
  }
  mqttPubBool("/settings/auto_update", appState.autoUpdateEnabled);
  mqttPubInt("/settings/timezone_offset", appState.timezoneOffset);
  mqttPubBool("/settings/dark_mode", appState.darkMode);
  mqttPubBool("/settings/cert_validation", appState.enableCertValidation);
}

// Publish update state for Home Assistant Update entity
void publishMqttUpdateState() {
  if (!mqttClient.connected())
    return;

  // Build JSON state for HA Update entity
  JsonDocument doc;
  doc["installed_version"] = firmwareVer;
  doc["latest_version"] =
      appState.cachedLatestVersion.length() > 0 ? appState.cachedLatestVersion : firmwareVer;

  char titleBuf[64];
  snprintf(titleBuf, sizeof(titleBuf), "%s Firmware", MANUFACTURER_MODEL);
  doc["title"] = titleBuf;

  char urlBuf[128];
  snprintf(urlBuf, sizeof(urlBuf), "https://github.com/%s/%s/releases",
           GITHUB_REPO_OWNER, GITHUB_REPO_NAME);
  doc["release_url"] = urlBuf;

  doc["in_progress"] = appState.otaInProgress;
  if (appState.otaInProgress) {
    doc["update_percentage"] = appState.otaProgress;
  } else {
    doc["update_percentage"] = (char*)nullptr; // JSON null
  }

  // Add release summary if update available
  if (appState.updateAvailable && appState.cachedLatestVersion.length() > 0) {
    char summaryBuf[128];
    snprintf(summaryBuf, sizeof(summaryBuf),
             "New firmware version %s is available",
             appState.cachedLatestVersion.c_str());
    doc["release_summary"] = summaryBuf;
  }

  serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
  mqttClient.publish(mqttTopic("/system/update/state"), _jsonBuf, true);

  // Publish separate OTA progress topics for easier monitoring
  mqttPubBool("/system/update/in_progress", appState.otaInProgress);
  mqttPubInt("/system/update/progress", appState.otaProgress);
  mqttPubStr("/system/update/status", appState.otaStatus);

  if (strlen(appState.otaStatusMessage) > 0) {
    mqttPubStr("/system/update/message", appState.otaStatusMessage);
  }

  if (appState.otaTotalBytes > 0) {
    mqttPubInt("/system/update/bytes_downloaded", appState.otaProgressBytes);
    mqttPubInt("/system/update/bytes_total", appState.otaTotalBytes);
  }
}

// Publish static hardware stats (called once on MQTT connect)
void publishMqttHardwareStatsStatic() {
  if (!mqttClient.connected())
    return;

  mqttPubStr("/hardware/cpu_model", ESP.getChipModel());
  mqttPubInt("/hardware/cpu_cores", ESP.getChipCores());
  mqttPubInt("/hardware/cpu_freq", ESP.getCpuFreqMHz());
  mqttPubInt("/hardware/flash_size", (int)ESP.getFlashChipSize());
  mqttPubInt("/hardware/sketch_size", (int)ESP.getSketchSize());
  mqttPubInt("/hardware/sketch_free", (int)ESP.getFreeSketchSpace());
  mqttPubInt("/hardware/heap_total", (int)ESP.getHeapSize());
  mqttPubInt("/hardware/LittleFS_total", (int)LittleFS.totalBytes());
  uint32_t psramSize = ESP.getPsramSize();
  if (psramSize > 0) {
    mqttPubInt("/hardware/psram_total", (int)psramSize);
  }
}

// Publish dynamic hardware stats (gated by debugMode && debugHwStats, called on heartbeat)
void publishMqttHardwareStats() {
  if (!mqttClient.connected())
    return;
  if (!appState.debugMode || !appState.debugHwStats)
    return;

  updateCpuUsage();

  // Dynamic heap
  mqttPubInt("/hardware/heap_free", (int)ESP.getFreeHeap());
  mqttPubInt("/hardware/heap_min_free", (int)ESP.getMinFreeHeap());
  mqttPubInt("/hardware/heap_max_block", (int)ESP.getMaxAllocHeap());

  // Dynamic PSRAM
  if (ESP.getPsramSize() > 0) {
    mqttPubInt("/hardware/psram_free", (int)ESP.getFreePsram());
  }

  // CPU Utilization
  float cpuCore0 = getCpuUsageCore0();
  float cpuCore1 = getCpuUsageCore1();
  float cpuTotal = (cpuCore0 + cpuCore1) / 2.0;
  mqttPubFloat("/hardware/cpu_usage_core0", cpuCore0, 1);
  mqttPubFloat("/hardware/cpu_usage_core1", cpuCore1, 1);
  mqttPubFloat("/hardware/cpu_usage", cpuTotal, 1);

  // Temperature
  float temp = temperatureRead();
  mqttPubFloat("/hardware/temperature", temp, 1);

  // Dynamic storage
  mqttPubInt("/hardware/LittleFS_used", (int)LittleFS.usedBytes());

  // WiFi channel and AP clients
  mqttPubInt("/wifi/channel", WiFi.channel());
  mqttPubInt("/ap/clients", WiFi.softAPgetStationNum());

  // Task monitor
  const TaskMonitorData& tm = task_monitor_get_data();
  mqttPubInt("/hardware/task_count", tm.taskCount);
  mqttPubInt("/hardware/loop_time_us", (int)tm.loopTimeAvgUs);
  mqttPubInt("/hardware/loop_time_max_us", (int)tm.loopTimeMaxUs);

  uint32_t minStackFree = UINT32_MAX;
  for (int i = 0; i < tm.taskCount; i++) {
    if (tm.tasks[i].stackAllocBytes > 0 && tm.tasks[i].stackFreeBytes < minStackFree) {
      minStackFree = tm.tasks[i].stackFreeBytes;
    }
  }
  if (minStackFree == UINT32_MAX) minStackFree = 0;
  mqttPubInt("/hardware/min_stack_free", (int)minStackFree);
}

// Publish buzzer state
void publishMqttBuzzerState() {
  if (!mqttClient.connected())
    return;

  mqttPubBool("/settings/buzzer", appState.buzzerEnabled);
  mqttPubInt("/settings/buzzer_volume", appState.buzzerVolume);
}

// Publish display state (backlight + screen timeout)
void publishMqttDisplayState() {
  if (!mqttClient.connected())
    return;

  mqttPubBool("/display/backlight", appState.backlightOn);
  mqttPubInt("/settings/screen_timeout", (int)(appState.screenTimeout / 1000));

  // Publish brightness as percentage (0-100)
  int brightPct = (int)appState.backlightBrightness * 100 / 255;
  mqttPubInt("/display/brightness", brightPct);

  mqttPubBool("/display/dim_enabled", appState.dimEnabled);
  mqttPubInt("/settings/dim_timeout", (int)(appState.dimTimeout / 1000));

  // Publish dim brightness as percentage
  int dimPct = 10;
  if (appState.dimBrightness >= 191) dimPct = 75;
  else if (appState.dimBrightness >= 128) dimPct = 50;
  else if (appState.dimBrightness >= 64) dimPct = 25;
  else dimPct = 10;
  mqttPubInt("/display/dim_brightness", dimPct);

  mqttPubInt("/settings/audio_update_rate", appState.audioUpdateRate);
}

// Publish signal generator state
void publishMqttSignalGenState() {
  if (!mqttClient.connected())
    return;

  mqttPubBool("/signalgenerator/enabled", appState.sigGenEnabled);

  const char *waveNames[] = {"sine", "square", "white_noise", "sweep"};
  mqttPubStr("/signalgenerator/waveform", waveNames[appState.sigGenWaveform % 4]);
  mqttPubFloat("/signalgenerator/frequency", appState.sigGenFrequency, 0);
  mqttPubFloat("/signalgenerator/amplitude", appState.sigGenAmplitude, 0);

  const char *chanNames[] = {"ch1", "ch2", "both"};
  mqttPubStr("/signalgenerator/channel", chanNames[appState.sigGenChannel % 3]);

  mqttPubStr("/signalgenerator/output_mode",
             appState.sigGenOutputMode == 0 ? "software" : "pwm");

  mqttPubFloat("/signalgenerator/sweep_speed", appState.sigGenSweepSpeed, 0);

  const char *targetNames[] = {"adc1", "adc2", "both", "usb", "all"};
  mqttPubStr("/signalgenerator/target_adc", targetNames[appState.sigGenTargetAdc % 5]);
}

#ifdef DSP_ENABLED
void publishMqttEmergencyLimiterState() {
  if (!mqttClient.connected())
    return;

  // Settings
  mqttPubBool("/emergency_limiter/enabled", appState.emergencyLimiterEnabled);
  mqttPubFloat("/emergency_limiter/threshold", appState.emergencyLimiterThresholdDb, 2);

  // Status (from DSP metrics)
  DspMetrics metrics = dsp_get_metrics();
  mqttPubStr("/emergency_limiter/status",
             metrics.emergencyLimiterActive ? "active" : "idle");
  mqttPubInt("/emergency_limiter/trigger_count", (int)metrics.emergencyLimiterTriggers);
  mqttPubFloat("/emergency_limiter/gain_reduction", metrics.emergencyLimiterGrDb, 2);
}

void publishMqttAudioQualityState() {
  if (!mqttClient.connected())
    return;

  // Settings
  mqttPubBool("/audio_quality/enabled", appState.audioQualityEnabled);
  mqttPubFloat("/audio_quality/glitch_threshold", appState.audioQualityGlitchThreshold, 2);

  // Diagnostics
  AudioQualityDiag diag = audio_quality_get_diagnostics();
  mqttPubInt("/audio_quality/glitches_total", (int)diag.glitchHistory.totalGlitches);
  mqttPubInt("/audio_quality/glitches_last_minute", (int)diag.glitchHistory.glitchesLastMinute);
  mqttPubBool("/audio_quality/correlation_dsp_swap", diag.correlation.dspSwapRelated);
  mqttPubBool("/audio_quality/correlation_wifi", diag.correlation.wifiRelated);
}
#endif

void publishMqttAudioDiagnostics() {
  if (!mqttClient.connected())
    return;

  // Per-input diagnostics (only publish detected inputs)
  const char *inputLabels[] = {"adc1", "adc2", "usb"};
  int adcCount = appState.numInputsDetected < NUM_AUDIO_INPUTS ? appState.numInputsDetected : NUM_AUDIO_INPUTS;
  for (int a = 0; a < adcCount; a++) {
    const AppState::AdcState &adc = appState.audioAdc[a];
    const char *statusStr = "OK";
    switch (adc.healthStatus) {
      case 1: statusStr = "NO_DATA"; break;
      case 2: statusStr = "NOISE_ONLY"; break;
      case 3: statusStr = "CLIPPING"; break;
      case 4: statusStr = "I2S_ERROR"; break;
      case 5: statusStr = "HW_FAULT"; break;
    }
    char inputPrefix[48];
    snprintf(inputPrefix, sizeof(inputPrefix), "/audio/%s", inputLabels[a]);

    snprintf(_topicBuf, sizeof(_topicBuf), "%s%s/adc_status", _mqttTopicBase, inputPrefix);
    mqttClient.publish(_topicBuf, statusStr, true);

    snprintf(_topicBuf, sizeof(_topicBuf), "%s%s/noise_floor", _mqttTopicBase, inputPrefix);
    snprintf(_valBuf, sizeof(_valBuf), "%.1f", (double)adc.noiseFloorDbfs);
    mqttClient.publish(_topicBuf, _valBuf, true);

    snprintf(_topicBuf, sizeof(_topicBuf), "%s%s/vrms", _mqttTopicBase, inputPrefix);
    snprintf(_valBuf, sizeof(_valBuf), "%.3f", (double)adc.vrmsCombined);
    mqttClient.publish(_topicBuf, _valBuf, true);

    snprintf(_topicBuf, sizeof(_topicBuf), "%s%s/level", _mqttTopicBase, inputPrefix);
    snprintf(_valBuf, sizeof(_valBuf), "%.1f", (double)adc.dBFS);
    mqttClient.publish(_topicBuf, _valBuf, true);

    if (appState.debugMode) {
      snprintf(_topicBuf, sizeof(_topicBuf), "%s%s/snr", _mqttTopicBase, inputPrefix);
      snprintf(_valBuf, sizeof(_valBuf), "%.1f", (double)appState.audioSnrDb[a]);
      mqttClient.publish(_topicBuf, _valBuf, true);

      snprintf(_topicBuf, sizeof(_topicBuf), "%s%s/sfdr", _mqttTopicBase, inputPrefix);
      snprintf(_valBuf, sizeof(_valBuf), "%.1f", (double)appState.audioSfdrDb[a]);
      mqttClient.publish(_topicBuf, _valBuf, true);
    }
  }

  // ADC clock sync topics (only when both ADCs are detected)
  if (appState.numAdcsDetected >= 2) {
    mqttPubBool("/audio/adc_sync_ok", appState.adcSyncOk);
    mqttPubFloat("/audio/adc_sync_offset", appState.adcSyncOffsetSamples, 2);
  }

  // Legacy combined topics (ADC 0)
  const char *statusStr = "OK";
  switch (appState.audioHealthStatus) {
    case 1: statusStr = "NO_DATA"; break;
    case 2: statusStr = "NOISE_ONLY"; break;
    case 3: statusStr = "CLIPPING"; break;
    case 4: statusStr = "I2S_ERROR"; break;
    case 5: statusStr = "HW_FAULT"; break;
  }
  mqttPubStr("/audio/adc_status", statusStr);
  mqttPubFloat("/audio/noise_floor", appState.audioNoiseFloorDbfs, 1);
  mqttPubFloat("/audio/input_vrms", appState.audioVrmsCombined, 3);
  mqttPubFloat("/settings/adc_vref", appState.adcVref, 2);
}

// Publish audio graph toggle state
void publishMqttAudioGraphState() {
  if (!mqttClient.connected())
    return;

  mqttPubBool("/audio/vu_meter", appState.vuMeterEnabled);
  mqttPubBool("/audio/waveform", appState.waveformEnabled);
  mqttPubBool("/audio/spectrum", appState.spectrumEnabled);
  mqttPubStr("/audio/fft_window", fftWindowName(appState.fftWindowType));
}

// Publish per-ADC enabled state
void publishMqttAdcEnabledState() {
  if (!mqttClient.connected())
    return;

  mqttPubBool("/audio/input1/enabled", appState.adcEnabled[0]);
  mqttPubBool("/audio/input2/enabled", appState.adcEnabled[1]);
}

// Publish debug mode state
void publishMqttDebugState() {
  if (!mqttClient.connected())
    return;

  mqttPubBool("/debug/mode", appState.debugMode);
  mqttPubInt("/debug/serial_level", appState.debugSerialLevel);
  mqttPubBool("/debug/hw_stats", appState.debugHwStats);
  mqttPubBool("/debug/i2s_metrics", appState.debugI2sMetrics);
  mqttPubBool("/debug/task_monitor", appState.debugTaskMonitor);
}

#ifdef DSP_ENABLED
// Publish DSP pipeline state
void publishMqttDspState() {
  if (!mqttClient.connected())
    return;

  mqttPubBool("/dsp/enabled", appState.dspEnabled);
  mqttPubBool("/dsp/bypass", appState.dspBypass);

  // Preset state
  if (appState.dspPresetIndex >= 0 && appState.dspPresetIndex < DSP_PRESET_MAX_SLOTS) {
    mqttPubStr("/dsp/preset", appState.dspPresetNames[appState.dspPresetIndex]);
  } else {
    mqttPubStr("/dsp/preset", "Custom");
  }

  // Per-channel bypass and stage count
  DspState *cfg = dsp_get_active_config();
  char chPrefix[64];
  for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
    snprintf(chPrefix, sizeof(chPrefix), "/dsp/channel_%d", ch);

    snprintf(_topicBuf, sizeof(_topicBuf), "%s%s/bypass", _mqttTopicBase, chPrefix);
    mqttClient.publish(_topicBuf, cfg->channels[ch].bypass ? "ON" : "OFF", true);

    snprintf(_topicBuf, sizeof(_topicBuf), "%s%s/stage_count", _mqttTopicBase, chPrefix);
    snprintf(_valBuf, sizeof(_valBuf), "%d", cfg->channels[ch].stageCount);
    mqttClient.publish(_topicBuf, _valBuf, true);
  }

  // Global PEQ bypass (derived from all channels)
  bool anyPeqBypassed = true;
  for (int ch = 0; ch < 2; ch++) {
    for (int b = 0; b < DSP_PEQ_BANDS && b < cfg->channels[ch].stageCount; b++) {
      if (cfg->channels[ch].stages[b].enabled) { anyPeqBypassed = false; break; }
    }
    if (!anyPeqBypassed) break;
  }
  mqttPubBool("/dsp/peq/bypass", anyPeqBypassed);

  // DSP metrics
  DspMetrics m = dsp_get_metrics();
  mqttPubFloat("/dsp/cpu_load", m.cpuLoadPercent, 1);
  for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/dsp/channel_%d/limiter_gr", _mqttTopicBase, ch);
    snprintf(_valBuf, sizeof(_valBuf), "%.1f", (double)m.limiterGrDb[ch]);
    mqttClient.publish(_topicBuf, _valBuf, true);
  }
}

#endif

// Publish static crash info (called once on MQTT connect — never changes per boot)
void publishMqttCrashDiagnosticsStatic() {
  if (!mqttClient.connected())
    return;

  mqttPubStr("/diagnostics/reset_reason", getResetReasonString().c_str());
  mqttPubBool("/diagnostics/was_crash", crashlog_last_was_crash());
}

// Publish dynamic crash diagnostics (heap health — called on heartbeat)
void publishMqttCrashDiagnostics() {
  if (!mqttClient.connected())
    return;

  mqttPubInt("/diagnostics/heap_free", (int)ESP.getFreeHeap());
  mqttPubInt("/diagnostics/heap_max_block", (int)ESP.getMaxAllocHeap());
  mqttPubBool("/diagnostics/heap_critical", appState.heapCritical);
  mqttClient.publish(mqttTopic("/diagnostics/heap_warning"),
                     (appState.heapWarning || appState.heapCritical) ? "ON" : "OFF", true);

  // Per-ADC I2S recovery counts
  mqttPubInt("/diagnostics/i2s_recoveries_adc1", (int)appState.audioAdc[0].i2sRecoveries);
  if (appState.numAdcsDetected >= 2) {
    mqttPubInt("/diagnostics/i2s_recoveries_adc2", (int)appState.audioAdc[1].i2sRecoveries);
  }

  // WiFi RX watchdog recovery count
  mqttPubInt("/system/wifi_rx_watchdog_recoveries", (int)appState.wifiRxWatchdogRecoveries);
}

// Publish input names as read-only sensors
void publishMqttInputNames() {
  if (!mqttClient.connected())
    return;

  const char *labels[] = {"input1_name_l", "input1_name_r", "input2_name_l", "input2_name_r", "input3_name_l", "input3_name_r"};
  for (int i = 0; i < NUM_AUDIO_INPUTS * 2; i++) {
    char labelBuf[48];
    snprintf(labelBuf, sizeof(labelBuf), "/audio/%s", labels[i]);
    mqttPubStr(labelBuf, appState.inputNames[i].c_str());
  }
}

#ifdef GUI_ENABLED
// Publish boot animation state
void publishMqttBootAnimState() {
  if (!mqttClient.connected())
    return;

  mqttPubBool("/settings/boot_animation", appState.bootAnimEnabled);

  const char *styleNames[] = {"wave_pulse", "speaker_ripple", "waveform", "beat_bounce", "freq_bars", "heartbeat"};
  mqttPubStr("/settings/boot_animation_style", styleNames[appState.bootAnimStyle % 6]);
}
#endif

// Publish USB auto-priority and DAC source input state
void publishMqttUsbAutoPriorityState() {
  if (!mqttClient.connected())
    return;

  mqttPubBool("/settings/usb_auto_priority", appState.usbAutoPriority);

  const char *sourceNames[] = {"ADC1", "ADC2", "USB"};
  uint8_t src = appState.dacSourceInput < 3 ? appState.dacSourceInput : 0;
  mqttPubStr("/settings/dac_source", sourceNames[src]);
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
  publishMqttSignalGenState();
  publishMqttAudioDiagnostics();
  publishMqttAudioGraphState();
  publishMqttAdcEnabledState();
  publishMqttDebugState();
  publishMqttCrashDiagnostics();
  publishMqttInputNames();
  publishMqttUsbAutoPriorityState();
#ifdef DSP_ENABLED
  publishMqttDspState();
  publishMqttEmergencyLimiterState();
#endif
#ifdef GUI_ENABLED
  publishMqttBootAnimState();
#endif
}

// ===== Home Assistant Auto-Discovery =====

// Helper function to create device info JSON object
void addHADeviceInfo(JsonDocument &doc) {
  const char* deviceId = mqttDeviceId();

  // Extract short ID for display name
  uint64_t chipId = ESP.getEfuseMac();
  uint16_t shortId = (uint16_t)(chipId & 0xFFFF);
  char idBuf[5];
  snprintf(idBuf, sizeof(idBuf), "%04X", shortId);

  char nameBuf[64];
  snprintf(nameBuf, sizeof(nameBuf), "%s %s", MANUFACTURER_MODEL, idBuf);

  JsonObject device = doc["device"].to<JsonObject>();
  JsonArray identifiers = device["identifiers"].to<JsonArray>();
  identifiers.add(deviceId);
  device["name"] = nameBuf;
  device["model"] = MANUFACTURER_MODEL;
  device["manufacturer"] = MANUFACTURER_NAME;
  device["serial_number"] = appState.deviceSerialNumber;
  device["sw_version"] = firmwareVer;

  char configUrl[48];
  snprintf(configUrl, sizeof(configUrl), "http://%s",
           WiFi.localIP().toString().c_str());
  device["configuration_url"] = configUrl;

  // Availability — tells HA which topic indicates online/offline
  char availTopic[160];
  snprintf(availTopic, sizeof(availTopic), "%s/status", _mqttTopicBase);
  JsonArray avail = doc["availability"].to<JsonArray>();
  JsonObject a = avail.add<JsonObject>();
  a["topic"] = availTopic;
  a["payload_available"] = "online";
  a["payload_not_available"] = "offline";
}

// Publish Home Assistant auto-discovery configuration
void publishHADiscovery() {
  if (!mqttClient.connected() || !appState.mqttHADiscovery)
    return;

  LOG_I("[MQTT] Publishing Home Assistant discovery configs...");

  const char* devId = mqttDeviceId();
  // _mqttTopicBase already contains the cached base topic
  char uidBuf[64];  // Reused across all entity blocks

  // ===== LED Blinking Switch =====
  {
    JsonDocument doc;
    doc["name"] = "LED Blinking";
    snprintf(uidBuf, sizeof(uidBuf), "%s_blinking", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/led/blinking", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/led/blinking/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:led-on";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/switch/%s/blinking/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Amplifier Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Amplifier";
    snprintf(uidBuf, sizeof(uidBuf), "%s_amplifier", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/smartsensing/amplifier", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/smartsensing/amplifier/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:amplifier";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/switch/%s/amplifier/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== AP Mode Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Access Point";
    snprintf(uidBuf, sizeof(uidBuf), "%s_ap", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/ap/enabled", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/ap/enabled/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:access-point";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/switch/%s/ap/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Smart Sensing Mode Select =====
  {
    JsonDocument doc;
    doc["name"] = "Smart Sensing Mode";
    snprintf(uidBuf, sizeof(uidBuf), "%s_mode", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/smartsensing/mode", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/smartsensing/mode/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("always_on");
    options.add("always_off");
    options.add("smart_auto");
    doc["icon"] = "mdi:auto-fix";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/select/%s/mode/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Timer Duration Number =====
  {
    JsonDocument doc;
    doc["name"] = "Timer Duration";
    snprintf(uidBuf, sizeof(uidBuf), "%s_timer_duration", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/smartsensing/timer_duration", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/smartsensing/timer_duration/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["min"] = 1;
    doc["max"] = 60;
    doc["step"] = 1;
    doc["unit_of_measurement"] = "min";
    doc["icon"] = "mdi:timer-outline";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/number/%s/timer_duration/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Audio Threshold Number =====
  {
    JsonDocument doc;
    doc["name"] = "Audio Threshold";
    snprintf(uidBuf, sizeof(uidBuf), "%s_audio_threshold", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/smartsensing/audio_threshold", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/smartsensing/audio_threshold/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["min"] = -96;
    doc["max"] = 0;
    doc["step"] = 1;
    doc["unit_of_measurement"] = "dBFS";
    doc["icon"] = "mdi:volume-vibrate";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/number/%s/audio_threshold/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Audio Level Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Audio Level";
    snprintf(uidBuf, sizeof(uidBuf), "%s_audio_level", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/smartsensing/audio_level", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["unit_of_measurement"] = "dBFS";
    doc["state_class"] = "measurement";
    doc["suggested_display_precision"] = 1;
    doc["icon"] = "mdi:volume-vibrate";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/audio_level/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Timer Remaining Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Timer Remaining";
    snprintf(uidBuf, sizeof(uidBuf), "%s_timer_remaining", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/smartsensing/timer_remaining", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["unit_of_measurement"] = "s";
    doc["icon"] = "mdi:timer-sand";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/timer_remaining/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== WiFi RSSI Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "WiFi Signal";
    snprintf(uidBuf, sizeof(uidBuf), "%s_rssi", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/wifi/rssi", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["unit_of_measurement"] = "dBm";
    doc["device_class"] = "signal_strength";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:wifi";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/rssi/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== WiFi Connected Binary Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "WiFi Connected";
    snprintf(uidBuf, sizeof(uidBuf), "%s_wifi_connected", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/wifi/connected", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["device_class"] = "connectivity";
    doc["entity_category"] = "diagnostic";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/binary_sensor/%s/wifi_connected/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Signal Detected Binary Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Detected";
    snprintf(uidBuf, sizeof(uidBuf), "%s_signal_detected", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/smartsensing/signal_detected", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:sine-wave";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/binary_sensor/%s/signal_detected/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }


  // ===== Update Available Binary Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Update Available";
    snprintf(uidBuf, sizeof(uidBuf), "%s_update_available", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/system/update_available", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["device_class"] = "update";
    doc["entity_category"] = "diagnostic";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/binary_sensor/%s/update_available/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Firmware Version Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Firmware Version";
    snprintf(uidBuf, sizeof(uidBuf), "%s_firmware", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/system/firmware", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:tag";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/firmware/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Latest Firmware Version Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Latest Firmware Version";
    snprintf(uidBuf, sizeof(uidBuf), "%s_latest_firmware", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/system/latest_version", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:tag-arrow-up";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/latest_firmware/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Reboot Button =====
  {
    JsonDocument doc;
    doc["name"] = "Reboot";
    snprintf(uidBuf, sizeof(uidBuf), "%s_reboot", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/system/reboot", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_press"] = "REBOOT";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:restart";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/button/%s/reboot/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Check Update Button =====
  {
    JsonDocument doc;
    doc["name"] = "Check for Updates";
    snprintf(uidBuf, sizeof(uidBuf), "%s_check_update", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/system/check_update", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_press"] = "CHECK";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:update";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/button/%s/check_update/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Factory Reset Button =====
  {
    JsonDocument doc;
    doc["name"] = "Factory Reset";
    snprintf(uidBuf, sizeof(uidBuf), "%s_factory_reset", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/system/factory_reset", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_press"] = "RESET";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:factory";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/button/%s/factory_reset/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Auto-Update Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Auto Update";
    snprintf(uidBuf, sizeof(uidBuf), "%s_auto_update", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/auto_update", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/auto_update/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:update";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/switch/%s/auto_update/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Firmware Update Entity =====
  // This provides the native HA Update entity with install capability
  {
    JsonDocument doc;
    doc["name"] = "Firmware";
    snprintf(uidBuf, sizeof(uidBuf), "%s_firmware_update", devId);
    doc["unique_id"] = uidBuf;
    doc["device_class"] = "firmware";
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/system/update/state", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/system/update/command", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_install"] = "install";
    doc["entity_picture"] =
        "https://brands.home-assistant.io/_/esphome/icon.png";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/update/%s/firmware/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== IP Address Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "IP Address";
    snprintf(uidBuf, sizeof(uidBuf), "%s_ip", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/wifi/ip", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:ip-network";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/ip/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }


  // ===== Hardware Diagnostics =====

  // ===== CPU Temperature Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "CPU Temperature";
    snprintf(uidBuf, sizeof(uidBuf), "%s_cpu_temp", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/hardware/temperature", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["unit_of_measurement"] = "\xC2\xB0" "C";
    doc["device_class"] = "temperature";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:thermometer";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/cpu_temp/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== CPU Usage Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "CPU Usage";
    snprintf(uidBuf, sizeof(uidBuf), "%s_cpu_usage", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/hardware/cpu_usage", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["unit_of_measurement"] = "%";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:cpu-64-bit";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/cpu_usage/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Free Heap Memory Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Free Heap Memory";
    snprintf(uidBuf, sizeof(uidBuf), "%s_heap_free", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/hardware/heap_free", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["unit_of_measurement"] = "B";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:memory";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/heap_free/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Uptime Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Uptime";
    snprintf(uidBuf, sizeof(uidBuf), "%s_uptime", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/system/uptime", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["unit_of_measurement"] = "s";
    doc["device_class"] = "duration";
    doc["state_class"] = "total_increasing";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:clock-outline";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/uptime/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== LittleFS Used Storage Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "LittleFS Used";
    snprintf(uidBuf, sizeof(uidBuf), "%s_LittleFS_used", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/hardware/LittleFS_used", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["unit_of_measurement"] = "B";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:harddisk";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/LittleFS_used/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== WiFi Channel Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "WiFi Channel";
    snprintf(uidBuf, sizeof(uidBuf), "%s_wifi_channel", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/wifi/channel", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:wifi";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/wifi_channel/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Dark Mode Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Dark Mode";
    snprintf(uidBuf, sizeof(uidBuf), "%s_dark_mode", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/dark_mode", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/dark_mode/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:weather-night";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/switch/%s/dark_mode/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Certificate Validation Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Certificate Validation";
    snprintf(uidBuf, sizeof(uidBuf), "%s_cert_validation", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/cert_validation", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/cert_validation/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:certificate";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/switch/%s/cert_validation/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Display Backlight Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Display Backlight";
    snprintf(uidBuf, sizeof(uidBuf), "%s_backlight", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/display/backlight", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/display/backlight/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:brightness-6";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/switch/%s/backlight/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Screen Timeout Number =====
  {
    JsonDocument doc;
    doc["name"] = "Screen Timeout";
    snprintf(uidBuf, sizeof(uidBuf), "%s_screen_timeout", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/screen_timeout", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/screen_timeout/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["min"] = 0;
    doc["max"] = 600;
    doc["step"] = 30;
    doc["unit_of_measurement"] = "s";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:timer-off-outline";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/number/%s/screen_timeout/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Dim Enabled Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Dim";
    snprintf(uidBuf, sizeof(uidBuf), "%s_dim_enabled", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/display/dim_enabled", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/display/dim_enabled/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:brightness-auto";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/switch/%s/dim_enabled/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Dim Timeout Number =====
  {
    JsonDocument doc;
    doc["name"] = "Dim Timeout";
    snprintf(uidBuf, sizeof(uidBuf), "%s_dim_timeout", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/dim_timeout", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/dim_timeout/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["min"] = 0;
    doc["max"] = 60;
    doc["step"] = 5;
    doc["unit_of_measurement"] = "s";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:brightness-auto";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/number/%s/dim_timeout/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Display Brightness Number =====
  {
    JsonDocument doc;
    doc["name"] = "Display Brightness";
    snprintf(uidBuf, sizeof(uidBuf), "%s_brightness", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/display/brightness", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/display/brightness/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["min"] = 10;
    doc["max"] = 100;
    doc["step"] = 25;
    doc["unit_of_measurement"] = "%";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:brightness-percent";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/number/%s/brightness/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Dim Brightness Select =====
  {
    JsonDocument doc;
    doc["name"] = "Dim Brightness";
    snprintf(uidBuf, sizeof(uidBuf), "%s_dim_brightness", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/display/dim_brightness", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/display/dim_brightness/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("10");
    options.add("25");
    options.add("50");
    options.add("75");
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:brightness-4";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/select/%s/dim_brightness/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Buzzer Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Buzzer";
    snprintf(uidBuf, sizeof(uidBuf), "%s_buzzer", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/buzzer", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/buzzer/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:volume-high";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/switch/%s/buzzer/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Buzzer Volume Number =====
  {
    JsonDocument doc;
    doc["name"] = "Buzzer Volume";
    snprintf(uidBuf, sizeof(uidBuf), "%s_buzzer_volume", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/buzzer_volume", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/buzzer_volume/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["min"] = 0;
    doc["max"] = 2;
    doc["step"] = 1;
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:volume-medium";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/number/%s/buzzer_volume/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }


  // ===== Audio Update Rate Select =====
  {
    JsonDocument doc;
    doc["name"] = "Audio Update Rate";
    snprintf(uidBuf, sizeof(uidBuf), "%s_audio_update_rate", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/audio_update_rate", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/audio_update_rate/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("20");
    options.add("33");
    options.add("50");
    options.add("100");
    doc["unit_of_measurement"] = "ms";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:update";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/select/%s/audio_update_rate/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Signal Generator Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Generator";
    snprintf(uidBuf, sizeof(uidBuf), "%s_siggen_enabled", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/signalgenerator/enabled", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/signalgenerator/enabled/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:sine-wave";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/switch/%s/siggen_enabled/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Signal Generator Waveform Select =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Waveform";
    snprintf(uidBuf, sizeof(uidBuf), "%s_siggen_waveform", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/signalgenerator/waveform", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/signalgenerator/waveform/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("sine");
    options.add("square");
    options.add("white_noise");
    options.add("sweep");
    doc["icon"] = "mdi:waveform";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/select/%s/siggen_waveform/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Signal Generator Frequency Number =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Frequency";
    snprintf(uidBuf, sizeof(uidBuf), "%s_siggen_frequency", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/signalgenerator/frequency", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/signalgenerator/frequency/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["min"] = 1;
    doc["max"] = 22000;
    doc["step"] = 1;
    doc["unit_of_measurement"] = "Hz";
    doc["icon"] = "mdi:sine-wave";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/number/%s/siggen_frequency/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Signal Generator Amplitude Number =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Amplitude";
    snprintf(uidBuf, sizeof(uidBuf), "%s_siggen_amplitude", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/signalgenerator/amplitude", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/signalgenerator/amplitude/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["min"] = -96;
    doc["max"] = 0;
    doc["step"] = 1;
    doc["unit_of_measurement"] = "dBFS";
    doc["icon"] = "mdi:volume-high";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/number/%s/siggen_amplitude/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Signal Generator Channel Select =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Channel";
    snprintf(uidBuf, sizeof(uidBuf), "%s_siggen_channel", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/signalgenerator/channel", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/signalgenerator/channel/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("ch1");
    options.add("ch2");
    options.add("both");
    doc["icon"] = "mdi:speaker-multiple";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/select/%s/siggen_channel/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Signal Generator Output Mode Select =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Output Mode";
    snprintf(uidBuf, sizeof(uidBuf), "%s_siggen_output_mode", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/signalgenerator/output_mode", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/signalgenerator/output_mode/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("software");
    options.add("pwm");
    doc["icon"] = "mdi:export";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/select/%s/siggen_output_mode/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Signal Generator Target ADC Select =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Target ADC";
    snprintf(uidBuf, sizeof(uidBuf), "%s_siggen_target_adc", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/signalgenerator/target_adc", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/signalgenerator/target_adc/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("adc1");
    options.add("adc2");
    options.add("both");
    options.add("usb");
    options.add("all");
    doc["icon"] = "mdi:audio-input-stereo-minijack";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/select/%s/siggen_target_adc/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }


  // ===== Per-ADC Audio Diagnostic Entities (only detected ADCs) =====
  {
    const char *inputLabels[] = {"adc1", "adc2", "usb"};
    const char *inputNames[] = {"ADC 1", "ADC 2", "USB Audio"};
    int adcCount = appState.numInputsDetected < NUM_AUDIO_INPUTS ? appState.numInputsDetected : NUM_AUDIO_INPUTS;
    char nameBuf[48];
    char prefixBuf[128];
    char idSuffixBuf[32];
    for (int a = 0; a < adcCount; a++) {
      snprintf(prefixBuf, sizeof(prefixBuf), "%s/audio/%s", _mqttTopicBase, inputLabels[a]);
      snprintf(idSuffixBuf, sizeof(idSuffixBuf), "_%s", inputLabels[a]);

      // Per-ADC Level Sensor
      {
        JsonDocument doc;
        snprintf(nameBuf, sizeof(nameBuf), "%s Audio Level", inputNames[a]);
        doc["name"] = nameBuf;
        snprintf(uidBuf, sizeof(uidBuf), "%s%s_level", devId, idSuffixBuf);
        doc["unique_id"] = uidBuf;
        snprintf(_topicBuf, sizeof(_topicBuf), "%s/level", prefixBuf);
        doc["state_topic"] = _topicBuf;
        doc["unit_of_measurement"] = "dBFS";
        doc["state_class"] = "measurement";
        doc["icon"] = "mdi:volume-high";
        addHADeviceInfo(doc);
        serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
        snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/%s_level/config", devId, inputLabels[a]);
        mqttClient.publish(_topicBuf, _jsonBuf, true);
      }

      // Per-ADC Status Sensor
      {
        JsonDocument doc;
        snprintf(nameBuf, sizeof(nameBuf), "%s ADC Status", inputNames[a]);
        doc["name"] = nameBuf;
        snprintf(uidBuf, sizeof(uidBuf), "%s%s_adc_status", devId, idSuffixBuf);
        doc["unique_id"] = uidBuf;
        snprintf(_topicBuf, sizeof(_topicBuf), "%s/adc_status", prefixBuf);
        doc["state_topic"] = _topicBuf;
        doc["entity_category"] = "diagnostic";
        doc["icon"] = "mdi:audio-input-stereo-minijack";
        addHADeviceInfo(doc);
        serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
        snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/%s_adc_status/config", devId, inputLabels[a]);
        mqttClient.publish(_topicBuf, _jsonBuf, true);
      }

      // Per-ADC Noise Floor Sensor
      {
        JsonDocument doc;
        snprintf(nameBuf, sizeof(nameBuf), "%s Noise Floor", inputNames[a]);
        doc["name"] = nameBuf;
        snprintf(uidBuf, sizeof(uidBuf), "%s%s_noise_floor", devId, idSuffixBuf);
        doc["unique_id"] = uidBuf;
        snprintf(_topicBuf, sizeof(_topicBuf), "%s/noise_floor", prefixBuf);
        doc["state_topic"] = _topicBuf;
        doc["unit_of_measurement"] = "dBFS";
        doc["state_class"] = "measurement";
        doc["entity_category"] = "diagnostic";
        doc["icon"] = "mdi:volume-low";
        addHADeviceInfo(doc);
        serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
        snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/%s_noise_floor/config", devId, inputLabels[a]);
        mqttClient.publish(_topicBuf, _jsonBuf, true);
      }

      // Per-ADC Vrms Sensor
      {
        JsonDocument doc;
        snprintf(nameBuf, sizeof(nameBuf), "%s Vrms", inputNames[a]);
        doc["name"] = nameBuf;
        snprintf(uidBuf, sizeof(uidBuf), "%s%s_vrms", devId, idSuffixBuf);
        doc["unique_id"] = uidBuf;
        snprintf(_topicBuf, sizeof(_topicBuf), "%s/vrms", prefixBuf);
        doc["state_topic"] = _topicBuf;
        doc["unit_of_measurement"] = "V";
        doc["device_class"] = "voltage";
        doc["state_class"] = "measurement";
        doc["entity_category"] = "diagnostic";
        doc["suggested_display_precision"] = 3;
        doc["icon"] = "mdi:sine-wave";
        addHADeviceInfo(doc);
        serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
        snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/%s_vrms/config", devId, inputLabels[a]);
        mqttClient.publish(_topicBuf, _jsonBuf, true);
      }

      // SNR/SFDR discovery removed — debug-only data, accessible via REST/WS/GUI.
      // Cleanup of orphaned entities is handled in removeHADiscovery().
    }
  }

  // ===== ADC Clock Sync Binary Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "ADC Clock Sync";
    snprintf(uidBuf, sizeof(uidBuf), "%s_adc_sync_ok", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/audio/adc_sync_ok", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["device_class"] = "connectivity";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:sync";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/binary_sensor/%s/adc_sync_ok/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== ADC Sync Phase Offset Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "ADC Sync Phase Offset";
    snprintf(uidBuf, sizeof(uidBuf), "%s_adc_sync_offset", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/audio/adc_sync_offset", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["unit_of_measurement"] = "samples";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:sine-wave";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/adc_sync_offset/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Legacy Combined Audio ADC Status Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "ADC Status";
    snprintf(uidBuf, sizeof(uidBuf), "%s_adc_status", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/audio/adc_status", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:audio-input-stereo-minijack";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/adc_status/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Legacy Combined Audio Noise Floor Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Audio Noise Floor";
    snprintf(uidBuf, sizeof(uidBuf), "%s_noise_floor", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/audio/noise_floor", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["unit_of_measurement"] = "dBFS";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:volume-low";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/noise_floor/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Legacy Combined Input Voltage (Vrms) Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Input Voltage (Vrms)";
    snprintf(uidBuf, sizeof(uidBuf), "%s_input_vrms", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/audio/input_vrms", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["unit_of_measurement"] = "V";
    doc["device_class"] = "voltage";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["suggested_display_precision"] = 3;
    doc["icon"] = "mdi:sine-wave";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/input_vrms/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== ADC Reference Voltage Number =====
  {
    JsonDocument doc;
    doc["name"] = "ADC Reference Voltage";
    snprintf(uidBuf, sizeof(uidBuf), "%s_adc_vref", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/adc_vref", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/adc_vref/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["min"] = 1.0;
    doc["max"] = 5.0;
    doc["step"] = 0.1;
    doc["unit_of_measurement"] = "V";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:flash-triangle-outline";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/number/%s/adc_vref/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Per-ADC Enable Switches =====
  {
    const char *adcNames[] = {"ADC Input 1", "ADC Input 2"};
    const char *adcIds[] = {"input1_enabled", "input2_enabled"};
    const char *adcTopics[] = {"/audio/input1/enabled", "/audio/input2/enabled"};
    for (int a = 0; a < 2; a++) {
      JsonDocument doc;
      doc["name"] = adcNames[a];
      snprintf(uidBuf, sizeof(uidBuf), "%s_%s", devId, adcIds[a]);
      doc["unique_id"] = uidBuf;
      snprintf(_topicBuf, sizeof(_topicBuf), "%s%s", _mqttTopicBase, adcTopics[a]);
      doc["state_topic"] = _topicBuf;
      snprintf(_topicBuf, sizeof(_topicBuf), "%s%s/set", _mqttTopicBase, adcTopics[a]);
      doc["command_topic"] = _topicBuf;
      doc["payload_on"] = "ON";
      doc["payload_off"] = "OFF";
      doc["entity_category"] = "config";
      doc["icon"] = "mdi:audio-input-stereo-minijack";
      addHADeviceInfo(doc);
      serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
      snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/switch/%s/%s/config", devId, adcIds[a]);
      mqttClient.publish(_topicBuf, _jsonBuf, true);
    }
  }

  // ===== VU Meter Switch =====
  {
    JsonDocument doc;
    doc["name"] = "VU Meter";
    snprintf(uidBuf, sizeof(uidBuf), "%s_vu_meter", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/audio/vu_meter", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/audio/vu_meter/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:chart-bar";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/switch/%s/vu_meter/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Audio Waveform Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Audio Waveform";
    snprintf(uidBuf, sizeof(uidBuf), "%s_waveform", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/audio/waveform", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/audio/waveform/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:waveform";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/switch/%s/waveform/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Frequency Spectrum Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Frequency Spectrum";
    snprintf(uidBuf, sizeof(uidBuf), "%s_spectrum", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/audio/spectrum", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/audio/spectrum/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:equalizer";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/switch/%s/spectrum/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== FFT Window Type Select =====
  {
    JsonDocument doc;
    doc["name"] = "FFT Window";
    snprintf(uidBuf, sizeof(uidBuf), "%s_fft_window", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/audio/fft_window", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/audio/fft_window/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("hann");
    options.add("blackman");
    options.add("blackman_harris");
    options.add("blackman_nuttall");
    options.add("nuttall");
    options.add("flat_top");
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:window-shutter-settings";
    addHADeviceInfo(doc);

    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/select/%s/fft_window/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }


  // ===== Debug Mode Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Debug Mode";
    snprintf(uidBuf, sizeof(uidBuf), "%s_debug_mode", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/debug/mode", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/debug/mode/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:bug";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/switch/%s/debug_mode/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Debug Serial Level Number =====
  {
    JsonDocument doc;
    doc["name"] = "Debug Serial Level";
    snprintf(uidBuf, sizeof(uidBuf), "%s_debug_serial_level", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/debug/serial_level", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/debug/serial_level/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["min"] = 0;
    doc["max"] = 3;
    doc["step"] = 1;
    doc["mode"] = "slider";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:console";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/number/%s/debug_serial_level/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Debug HW Stats Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Debug HW Stats";
    snprintf(uidBuf, sizeof(uidBuf), "%s_debug_hw_stats", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/debug/hw_stats", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/debug/hw_stats/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:chart-line";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/switch/%s/debug_hw_stats/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Debug I2S Metrics Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Debug I2S Metrics";
    snprintf(uidBuf, sizeof(uidBuf), "%s_debug_i2s_metrics", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/debug/i2s_metrics", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/debug/i2s_metrics/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:timer-outline";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/switch/%s/debug_i2s_metrics/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Debug Task Monitor Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Debug Task Monitor";
    snprintf(uidBuf, sizeof(uidBuf), "%s_debug_task_monitor", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/debug/task_monitor", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/debug/task_monitor/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:format-list-bulleted";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/switch/%s/debug_task_monitor/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Task Monitor Diagnostic Sensors =====
  {
    JsonDocument doc;
    doc["name"] = "Task Count";
    snprintf(uidBuf, sizeof(uidBuf), "%s_task_count", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/hardware/task_count", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:format-list-numbered";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/task_count/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }
  {
    JsonDocument doc;
    doc["name"] = "Loop Time";
    snprintf(uidBuf, sizeof(uidBuf), "%s_loop_time", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/hardware/loop_time_us", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["unit_of_measurement"] = "us";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:timer-outline";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/loop_time/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }
  {
    JsonDocument doc;
    doc["name"] = "Loop Time Max";
    snprintf(uidBuf, sizeof(uidBuf), "%s_loop_time_max", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/hardware/loop_time_max_us", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["unit_of_measurement"] = "us";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:timer-alert-outline";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/loop_time_max/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }
  {
    JsonDocument doc;
    doc["name"] = "Min Stack Free";
    snprintf(uidBuf, sizeof(uidBuf), "%s_min_stack_free", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/hardware/min_stack_free", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["unit_of_measurement"] = "B";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:memory";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/min_stack_free/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Crash Diagnostics — Reset Reason (diagnostic sensor) =====
  {
    JsonDocument doc;
    doc["name"] = "Reset Reason";
    snprintf(uidBuf, sizeof(uidBuf), "%s_reset_reason", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/diagnostics/reset_reason", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:restart-alert";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/reset_reason/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Crash Diagnostics — Was Crash (binary sensor) =====
  {
    JsonDocument doc;
    doc["name"] = "Last Boot Was Crash";
    snprintf(uidBuf, sizeof(uidBuf), "%s_was_crash", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/diagnostics/was_crash", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["device_class"] = "problem";
    doc["entity_category"] = "diagnostic";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/binary_sensor/%s/was_crash/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Heap Health — Heap Warning (binary sensor) =====
  {
    JsonDocument doc;
    doc["name"] = "Heap Warning";
    snprintf(uidBuf, sizeof(uidBuf), "%s_heap_warning", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/diagnostics/heap_warning", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["device_class"] = "problem";
    doc["entity_category"] = "diagnostic";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/binary_sensor/%s/heap_warning/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Heap Health — Heap Critical (binary sensor) =====
  {
    JsonDocument doc;
    doc["name"] = "Heap Critical";
    snprintf(uidBuf, sizeof(uidBuf), "%s_heap_critical", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/diagnostics/heap_critical", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["device_class"] = "problem";
    doc["entity_category"] = "diagnostic";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/binary_sensor/%s/heap_critical/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Heap Health — Max Alloc Block (diagnostic sensor) =====
  {
    JsonDocument doc;
    doc["name"] = "Heap Max Block";
    snprintf(uidBuf, sizeof(uidBuf), "%s_heap_max_block", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/diagnostics/heap_max_block", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["unit_of_measurement"] = "B";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:memory";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/heap_max_block/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== WiFi RX Watchdog Recovery Count (diagnostic sensor) =====
  {
    JsonDocument doc;
    doc["name"] = "WiFi RX Watchdog Recoveries";
    snprintf(uidBuf, sizeof(uidBuf), "%s_wifi_rx_watchdog_recoveries", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/system/wifi_rx_watchdog_recoveries", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["state_class"] = "total_increasing";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:wifi-refresh";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/wifi_rx_watchdog_recoveries/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Timezone Offset Number =====
  {
    JsonDocument doc;
    doc["name"] = "Timezone Offset";
    snprintf(uidBuf, sizeof(uidBuf), "%s_timezone_offset", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/timezone_offset", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/timezone_offset/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["min"] = -12;
    doc["max"] = 14;
    doc["step"] = 1;
    doc["unit_of_measurement"] = "h";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:map-clock-outline";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/number/%s/timezone_offset/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Signal Generator Sweep Speed Number =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Sweep Speed";
    snprintf(uidBuf, sizeof(uidBuf), "%s_siggen_sweep_speed", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/signalgenerator/sweep_speed", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/signalgenerator/sweep_speed/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["min"] = 0.1;
    doc["max"] = 10.0;
    doc["step"] = 0.1;
    doc["unit_of_measurement"] = "Hz/s";
    doc["icon"] = "mdi:speedometer";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/number/%s/siggen_sweep_speed/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Input Names (6 read-only sensors) =====
  {
    const char *inputLabels[] = {"input1_name_l", "input1_name_r", "input2_name_l", "input2_name_r", "input3_name_l", "input3_name_r"};
    const char *inputDisplayNames[] = {"Input 1 Left Name", "Input 1 Right Name", "Input 2 Left Name", "Input 2 Right Name", "Input 3 Left Name", "Input 3 Right Name"};
    for (int i = 0; i < NUM_AUDIO_INPUTS * 2; i++) {
      JsonDocument doc;
      doc["name"] = inputDisplayNames[i];
      snprintf(uidBuf, sizeof(uidBuf), "%s_%s", devId, inputLabels[i]);
      doc["unique_id"] = uidBuf;
      snprintf(_topicBuf, sizeof(_topicBuf), "%s/audio/%s", _mqttTopicBase, inputLabels[i]);
      doc["state_topic"] = _topicBuf;
      doc["entity_category"] = "diagnostic";
      doc["icon"] = "mdi:label-outline";
      addHADeviceInfo(doc);
      serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
      snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/%s/config", devId, inputLabels[i]);
      mqttClient.publish(_topicBuf, _jsonBuf, true);
    }
  }


#ifdef DSP_ENABLED
  // ===== DSP Enabled Switch =====
  {
    JsonDocument doc;
    doc["name"] = "DSP";
    snprintf(uidBuf, sizeof(uidBuf), "%s_dsp_enabled", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/dsp/enabled", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/dsp/enabled/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:equalizer";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/switch/%s/dsp_enabled/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== DSP Bypass Switch =====
  {
    JsonDocument doc;
    doc["name"] = "DSP Bypass";
    snprintf(uidBuf, sizeof(uidBuf), "%s_dsp_bypass", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/dsp/bypass", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/dsp/bypass/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:debug-step-over";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/switch/%s/dsp_bypass/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== DSP CPU Load Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "DSP CPU Load";
    snprintf(uidBuf, sizeof(uidBuf), "%s_dsp_cpu_load", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/dsp/cpu_load", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["unit_of_measurement"] = "%";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:cpu-64-bit";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/dsp_cpu_load/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== DSP Preset Select =====
  {
    JsonDocument doc;
    doc["name"] = "DSP Preset";
    snprintf(uidBuf, sizeof(uidBuf), "%s_dsp_preset", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/dsp/preset", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/dsp/preset/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:playlist-music";
    JsonArray opts = doc["options"].to<JsonArray>();
    opts.add("Custom");
    extern bool dsp_preset_exists(int);
    for (int i = 0; i < DSP_PRESET_MAX_SLOTS; i++) {
      if (appState.dspPresetNames[i][0] && dsp_preset_exists(i)) {
        opts.add(appState.dspPresetNames[i]);
      }
    }
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/select/%s/dsp_preset/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Per-Channel DSP Entities =====
  {
    const char *chNames[] = {"L1", "R1", "L2", "R2"};
    char chPrefixBuf[128];
    char chIdSuffixBuf[32];
    char chNameBuf[48];
    for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
      snprintf(chPrefixBuf, sizeof(chPrefixBuf), "%s/dsp/channel_%d", _mqttTopicBase, ch);
      snprintf(chIdSuffixBuf, sizeof(chIdSuffixBuf), "_dsp_ch%d", ch);

      // Per-channel bypass switch
      {
        JsonDocument doc;
        snprintf(chNameBuf, sizeof(chNameBuf), "DSP %s Bypass", chNames[ch]);
        doc["name"] = chNameBuf;
        snprintf(uidBuf, sizeof(uidBuf), "%s%s_bypass", devId, chIdSuffixBuf);
        doc["unique_id"] = uidBuf;
        snprintf(_topicBuf, sizeof(_topicBuf), "%s/bypass", chPrefixBuf);
        doc["state_topic"] = _topicBuf;
        snprintf(_topicBuf, sizeof(_topicBuf), "%s/bypass/set", chPrefixBuf);
        doc["command_topic"] = _topicBuf;
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["entity_category"] = "config";
        doc["icon"] = "mdi:debug-step-over";
        addHADeviceInfo(doc);
        serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
        snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/switch/%s/dsp_ch%d_bypass/config", devId, ch);
        mqttClient.publish(_topicBuf, _jsonBuf, true);
      }

      // Per-channel stage count sensor
      {
        JsonDocument doc;
        snprintf(chNameBuf, sizeof(chNameBuf), "DSP %s Stages", chNames[ch]);
        doc["name"] = chNameBuf;
        snprintf(uidBuf, sizeof(uidBuf), "%s%s_stages", devId, chIdSuffixBuf);
        doc["unique_id"] = uidBuf;
        snprintf(_topicBuf, sizeof(_topicBuf), "%s/stage_count", chPrefixBuf);
        doc["state_topic"] = _topicBuf;
        doc["state_class"] = "measurement";
        doc["entity_category"] = "diagnostic";
        doc["icon"] = "mdi:filter";
        addHADeviceInfo(doc);
        serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
        snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/dsp_ch%d_stages/config", devId, ch);
        mqttClient.publish(_topicBuf, _jsonBuf, true);
      }

      // Per-channel limiter gain reduction sensor
      {
        JsonDocument doc;
        snprintf(chNameBuf, sizeof(chNameBuf), "DSP %s Limiter GR", chNames[ch]);
        doc["name"] = chNameBuf;
        snprintf(uidBuf, sizeof(uidBuf), "%s%s_limiter_gr", devId, chIdSuffixBuf);
        doc["unique_id"] = uidBuf;
        snprintf(_topicBuf, sizeof(_topicBuf), "%s/limiter_gr", chPrefixBuf);
        doc["state_topic"] = _topicBuf;
        doc["unit_of_measurement"] = "dB";
        doc["state_class"] = "measurement";
        doc["entity_category"] = "diagnostic";
        doc["icon"] = "mdi:arrow-collapse-down";
        addHADeviceInfo(doc);
        serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
        snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/dsp_ch%d_limiter_gr/config", devId, ch);
        mqttClient.publish(_topicBuf, _jsonBuf, true);
      }
    }
  }

  // ===== PEQ Bypass Switch =====
  {
    JsonDocument doc;
    doc["name"] = "PEQ Bypass";
    snprintf(uidBuf, sizeof(uidBuf), "%s_peq_bypass", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/dsp/peq/bypass", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/dsp/peq/bypass/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:equalizer";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/switch/%s/peq_bypass/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // PEQ band switches removed — controlled via DSP API / WebSocket only.
  // Cleanup of orphaned PEQ band entities is handled in removeHADiscovery().
#endif

#ifdef GUI_ENABLED
  // ===== Boot Animation Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Boot Animation";
    snprintf(uidBuf, sizeof(uidBuf), "%s_boot_animation", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/boot_animation", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/boot_animation/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:animation-play";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/switch/%s/boot_animation/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Boot Animation Style Select =====
  {
    JsonDocument doc;
    doc["name"] = "Boot Animation Style";
    snprintf(uidBuf, sizeof(uidBuf), "%s_boot_animation_style", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/boot_animation_style", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/boot_animation_style/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("wave_pulse");
    options.add("speaker_ripple");
    options.add("waveform");
    options.add("beat_bounce");
    options.add("freq_bars");
    options.add("heartbeat");
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:animation";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/select/%s/boot_animation_style/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }
#endif

#ifdef DSP_ENABLED
  // ===== Emergency Limiter Enabled Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Emergency Limiter";
    snprintf(uidBuf, sizeof(uidBuf), "%s_emergency_limiter_enabled", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/emergency_limiter/enabled", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/emergency_limiter/enabled/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:shield-alert";
    doc["entity_category"] = "config";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/switch/%s/emergency_limiter_enabled/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Emergency Limiter Threshold Number =====
  {
    JsonDocument doc;
    doc["name"] = "Emergency Limiter Threshold";
    snprintf(uidBuf, sizeof(uidBuf), "%s_emergency_limiter_threshold", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/emergency_limiter/threshold", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/emergency_limiter/threshold/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["min"] = -6.0;
    doc["max"] = 0.0;
    doc["step"] = 0.1;
    doc["unit_of_measurement"] = "dBFS";
    doc["icon"] = "mdi:volume-high";
    doc["entity_category"] = "config";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/number/%s/emergency_limiter_threshold/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Emergency Limiter Status Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Emergency Limiter Status";
    snprintf(uidBuf, sizeof(uidBuf), "%s_emergency_limiter_status", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/emergency_limiter/status", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["icon"] = "mdi:shield-check";
    doc["entity_category"] = "diagnostic";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/emergency_limiter_status/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Emergency Limiter Trigger Count Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Emergency Limiter Triggers";
    snprintf(uidBuf, sizeof(uidBuf), "%s_emergency_limiter_triggers", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/emergency_limiter/trigger_count", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["icon"] = "mdi:counter";
    doc["entity_category"] = "diagnostic";
    doc["state_class"] = "total_increasing";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/emergency_limiter_triggers/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Emergency Limiter Gain Reduction Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Emergency Limiter Gain Reduction";
    snprintf(uidBuf, sizeof(uidBuf), "%s_emergency_limiter_gr", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/emergency_limiter/gain_reduction", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    doc["unit_of_measurement"] = "dB";
    doc["icon"] = "mdi:volume-minus";
    doc["entity_category"] = "diagnostic";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/sensor/%s/emergency_limiter_gr/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }
#endif

  // ===== USB Auto-Priority Switch =====
  {
    JsonDocument doc;
    doc["name"] = "USB Auto-Priority";
    snprintf(uidBuf, sizeof(uidBuf), "%s_usb_auto_priority", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/usb_auto_priority", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/usb_auto_priority/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:usb-flash-drive";
    doc["entity_category"] = "config";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/switch/%s/usb_auto_priority/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
    // Publish current value
    publishMqttUsbAutoPriorityState();
  }

  // ===== DAC Source Select =====
  {
    JsonDocument doc;
    doc["name"] = "DAC Source";
    snprintf(uidBuf, sizeof(uidBuf), "%s_dac_source", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/dac_source", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/dac_source/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("ADC1");
    options.add("ADC2");
    options.add("USB");
    doc["icon"] = "mdi:swap-horizontal";
    doc["entity_category"] = "config";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/select/%s/dac_source/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
  }

  // ===== Custom Device Name (Text Entity) =====
  {
    JsonDocument doc;
    doc["name"] = "Device Name";
    snprintf(uidBuf, sizeof(uidBuf), "%s_device_name", devId);
    doc["unique_id"] = uidBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/device_name", _mqttTopicBase);
    doc["state_topic"] = _topicBuf;
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/device_name/set", _mqttTopicBase);
    doc["command_topic"] = _topicBuf;
    doc["icon"] = "mdi:rename";
    doc["entity_category"] = "config";
    doc["max"] = 32;
    doc["min"] = 0;
    doc["mode"] = "text";
    addHADeviceInfo(doc);
    serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
    snprintf(_topicBuf, sizeof(_topicBuf), "homeassistant/text/%s/device_name/config", devId);
    mqttClient.publish(_topicBuf, _jsonBuf, true);
    // Publish current value
    snprintf(_topicBuf, sizeof(_topicBuf), "%s/settings/device_name", _mqttTopicBase);
    mqttClient.publish(_topicBuf, appState.customDeviceName, true);
  }

  LOG_I("[MQTT] Home Assistant discovery configs published");
}

// Remove Home Assistant auto-discovery configuration
void removeHADiscovery() {
  if (!mqttClient.connected())
    return;

  LOG_I("[MQTT] Removing Home Assistant discovery configs...");

  const char* devId = mqttDeviceId();

  // List of all discovery topics to remove
  const char *topics[] = {
      "homeassistant/switch/%s/blinking/config",
      "homeassistant/switch/%s/amplifier/config",
      "homeassistant/switch/%s/ap/config",
      "homeassistant/switch/%s/auto_update/config",
      "homeassistant/switch/%s/dark_mode/config",
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
      "homeassistant/switch/%s/dim_enabled/config",
      "homeassistant/number/%s/dim_timeout/config",
      "homeassistant/number/%s/brightness/config",
      "homeassistant/select/%s/dim_brightness/config",
      "homeassistant/switch/%s/buzzer/config",
      "homeassistant/number/%s/buzzer_volume/config",
      "homeassistant/switch/%s/siggen_enabled/config",
      "homeassistant/select/%s/siggen_waveform/config",
      "homeassistant/number/%s/siggen_frequency/config",
      "homeassistant/number/%s/siggen_amplitude/config",
      "homeassistant/select/%s/siggen_channel/config",
      "homeassistant/select/%s/siggen_output_mode/config",
      "homeassistant/select/%s/audio_update_rate/config",
      "homeassistant/sensor/%s/adc_status/config",
      "homeassistant/sensor/%s/noise_floor/config",
      "homeassistant/sensor/%s/input_vrms/config",
      "homeassistant/number/%s/adc_vref/config",
      "homeassistant/switch/%s/input1_enabled/config",
      "homeassistant/switch/%s/input2_enabled/config",
      "homeassistant/switch/%s/vu_meter/config",
      "homeassistant/switch/%s/waveform/config",
      "homeassistant/switch/%s/spectrum/config",
      "homeassistant/sensor/%s/task_count/config",
      "homeassistant/sensor/%s/loop_time/config",
      "homeassistant/sensor/%s/loop_time_max/config",
      "homeassistant/sensor/%s/min_stack_free/config",
      "homeassistant/switch/%s/debug_mode/config",
      "homeassistant/number/%s/debug_serial_level/config",
      "homeassistant/switch/%s/debug_hw_stats/config",
      "homeassistant/switch/%s/debug_i2s_metrics/config",
      "homeassistant/switch/%s/debug_task_monitor/config",
      // Per-ADC entities
      "homeassistant/sensor/%s/adc1_level/config",
      "homeassistant/sensor/%s/adc1_adc_status/config",
      "homeassistant/sensor/%s/adc1_noise_floor/config",
      "homeassistant/sensor/%s/adc1_vrms/config",
      "homeassistant/sensor/%s/adc2_level/config",
      "homeassistant/sensor/%s/adc2_adc_status/config",
      "homeassistant/sensor/%s/adc2_noise_floor/config",
      "homeassistant/sensor/%s/adc2_vrms/config",
      "homeassistant/sensor/%s/adc1_snr/config",
      "homeassistant/sensor/%s/adc1_sfdr/config",
      "homeassistant/sensor/%s/adc2_snr/config",
      "homeassistant/sensor/%s/adc2_sfdr/config",
      "homeassistant/select/%s/fft_window/config",
      // Signal generator target ADC
      "homeassistant/select/%s/siggen_target_adc/config",
      // Crash diagnostics
      "homeassistant/sensor/%s/reset_reason/config",
      "homeassistant/binary_sensor/%s/was_crash/config",
      "homeassistant/binary_sensor/%s/heap_critical/config",
      "homeassistant/sensor/%s/heap_max_block/config",
      // Factory reset button
      "homeassistant/button/%s/factory_reset/config",
      // Timezone offset
      "homeassistant/number/%s/timezone_offset/config",
      // Sweep speed
      "homeassistant/number/%s/siggen_sweep_speed/config",
      // Input names
      "homeassistant/sensor/%s/input1_name_l/config",
      "homeassistant/sensor/%s/input1_name_r/config",
      "homeassistant/sensor/%s/input2_name_l/config",
      "homeassistant/sensor/%s/input2_name_r/config",
      // Boot animation
      "homeassistant/switch/%s/boot_animation/config",
      "homeassistant/select/%s/boot_animation_style/config",
      // DSP entities
      "homeassistant/switch/%s/dsp_enabled/config",
      "homeassistant/switch/%s/dsp_bypass/config",
      "homeassistant/sensor/%s/dsp_cpu_load/config",
      "homeassistant/switch/%s/dsp_ch0_bypass/config",
      "homeassistant/switch/%s/dsp_ch1_bypass/config",
      "homeassistant/switch/%s/dsp_ch2_bypass/config",
      "homeassistant/switch/%s/dsp_ch3_bypass/config",
      "homeassistant/sensor/%s/dsp_ch0_stages/config",
      "homeassistant/sensor/%s/dsp_ch1_stages/config",
      "homeassistant/sensor/%s/dsp_ch2_stages/config",
      "homeassistant/sensor/%s/dsp_ch3_stages/config",
      "homeassistant/sensor/%s/dsp_ch0_limiter_gr/config",
      "homeassistant/sensor/%s/dsp_ch1_limiter_gr/config",
      "homeassistant/sensor/%s/dsp_ch2_limiter_gr/config",
      "homeassistant/sensor/%s/dsp_ch3_limiter_gr/config",
      // PEQ bypass
      "homeassistant/switch/%s/peq_bypass/config",
      // Custom device name
      "homeassistant/text/%s/device_name/config",
      // USB auto-priority and DAC source
      "homeassistant/switch/%s/usb_auto_priority/config",
      "homeassistant/select/%s/dac_source/config"};

  char topicBuf[160];
  for (const char *topicTemplate : topics) {
    snprintf(topicBuf, sizeof(topicBuf), topicTemplate, devId);
    mqttClient.publish(topicBuf, "", true); // Empty payload removes the config
  }

  // Remove orphaned PEQ band switch entities (20 = 2 channels x 10 bands)
  for (int ch = 0; ch < 2; ch++) {
    for (int b = 0; b < DSP_PEQ_BANDS; b++) {
      snprintf(topicBuf, sizeof(topicBuf), "homeassistant/switch/%s/peq_ch%d_band%d/config", devId, ch, b + 1);
      mqttClient.publish(topicBuf, "", true);
    }
  }

  LOG_I("[MQTT] Home Assistant discovery configs removed");
}

// ===== MQTT HTTP API Handlers =====

// GET /api/mqtt - Get MQTT settings and status
void handleMqttGet() {
  JsonDocument doc;
  doc["success"] = true;

  // Settings
  doc["enabled"] = appState.mqttEnabled;
  doc["broker"] = appState.mqttBroker;
  doc["port"] = appState.mqttPort;
  doc["username"] = appState.mqttUsername;
  // Don't send password for security, just indicate if set
  doc["hasPassword"] = (strlen(appState.mqttPassword) > 0);
  doc["baseTopic"] = appState.mqttBaseTopic;
  doc["effectiveBaseTopic"] = getEffectiveMqttBaseTopic();
  snprintf(_valBuf, sizeof(_valBuf), "ALX/%s", appState.deviceSerialNumber);
  doc["defaultBaseTopic"] = _valBuf;
  doc["haDiscovery"] = appState.mqttHADiscovery;

  // Status
  doc["connected"] = appState.mqttConnected;
  doc["deviceId"] = mqttDeviceId();

  serializeJson(doc, _jsonBuf, sizeof(_jsonBuf));
  server.send(200, "application/json", _jsonBuf);
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
  bool prevHADiscovery = appState.mqttHADiscovery;

  // Update enabled state
  if (doc["enabled"].is<bool>()) {
    bool newEnabled = doc["enabled"].as<bool>();
    if (appState.mqttEnabled != newEnabled) {
      appState.mqttEnabled = newEnabled;
      settingsChanged = true;
      needReconnect = true;

      if (!appState.mqttEnabled && mqttClient.connected()) {
        // Remove HA discovery before disconnecting
        if (appState.mqttHADiscovery) {
          removeHADiscovery();
        }
        mqttClient.disconnect();
        appState.mqttConnected = false;
      }
    }
  }

  // Update broker
  if (doc["broker"].is<String>()) {
    String newBroker = doc["broker"].as<String>();
    if (strcmp(appState.mqttBroker, newBroker.c_str()) != 0) {
      setCharField(appState.mqttBroker, sizeof(appState.mqttBroker), newBroker.c_str());
      settingsChanged = true;
      needReconnect = true;
    }
  }

  // Update port
  if (doc["port"].is<int>()) {
    int newPort = doc["port"].as<int>();
    if (newPort > 0 && newPort <= 65535 && appState.mqttPort != newPort) {
      appState.mqttPort = newPort;
      settingsChanged = true;
      needReconnect = true;
    }
  }

  // Update username
  if (doc["username"].is<String>()) {
    String newUsername = doc["username"].as<String>();
    if (strcmp(appState.mqttUsername, newUsername.c_str()) != 0) {
      setCharField(appState.mqttUsername, sizeof(appState.mqttUsername), newUsername.c_str());
      settingsChanged = true;
      needReconnect = true;
    }
  }

  // Update password (empty string keeps existing, like WiFi pattern)
  if (!doc["password"].isNull()) {
    String newPassword = doc["password"].as<String>();
    if (newPassword.length() > 0 && strcmp(appState.mqttPassword, newPassword.c_str()) != 0) {
      setCharField(appState.mqttPassword, sizeof(appState.mqttPassword), newPassword.c_str());
      settingsChanged = true;
      needReconnect = true;
    }
  }

  // Update base topic (empty string uses default ALX/{serial})
  if (!doc["baseTopic"].isNull()) {
    String newBaseTopic = doc["baseTopic"].as<String>();
    if (strcmp(appState.mqttBaseTopic, newBaseTopic.c_str()) != 0) {
      // Remove old HA discovery before changing topic
      if (appState.mqttHADiscovery && mqttClient.connected()) {
        removeHADiscovery();
      }
      setCharField(appState.mqttBaseTopic, sizeof(appState.mqttBaseTopic), newBaseTopic.c_str());
      settingsChanged = true;
      needReconnect = true;
      LOG_I("[MQTT] Base topic changed to: %s", newBaseTopic.length() > 0 ? newBaseTopic.c_str() : "(default)");
    }
  }

  // Update HA Discovery
  if (doc["haDiscovery"].is<bool>()) {
    bool newHADiscovery = doc["haDiscovery"].as<bool>();
    if (appState.mqttHADiscovery != newHADiscovery) {
      // If disabling HA discovery, remove existing configs
      if (!newHADiscovery && mqttClient.connected()) {
        removeHADiscovery();
      }
      appState.mqttHADiscovery = newHADiscovery;
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
  if (needReconnect && appState.mqttEnabled && strlen(appState.mqttBroker) > 0) {
    if (mqttClient.connected()) {
      mqttClient.disconnect();
    }
    appState.mqttConnected = false;
    appState.lastMqttReconnect = 0; // Force immediate reconnect attempt
    setupMqtt();
  }

  // Send response
  JsonDocument resp;
  resp["success"] = true;
  resp["connected"] = appState.mqttConnected;
  resp["message"] = settingsChanged ? "Settings updated" : "No changes";

  String json;
  serializeJson(resp, json);
  server.send(200, "application/json", json);
}
