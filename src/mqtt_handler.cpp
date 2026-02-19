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
    appState.mqttBroker = line2;
  }

  if (line3.length() > 0) {
    int port = line3.toInt();
    if (port > 0 && port <= 65535) {
      appState.mqttPort = port;
    }
  }

  if (line4.length() > 0) {
    appState.mqttUsername = line4;
  }

  if (line5.length() > 0) {
    appState.mqttPassword = line5;
  }

  if (line6.length() > 0) {
    appState.mqttBaseTopic = line6;
  }

  if (line7.length() > 0) {
    appState.mqttHADiscovery = (line7.toInt() != 0);
  }

  LOG_I("[MQTT] Settings loaded - Enabled: %s, Broker: %s:%d", appState.mqttEnabled ? "true" : "false", appState.mqttBroker.c_str(), appState.mqttPort);
  LOG_I("[MQTT] Base Topic: %s, HA Discovery: %s", appState.mqttBaseTopic.c_str(), appState.mqttHADiscovery ? "true" : "false");

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
  uint64_t chipId = ESP.getEfuseMac();
  uint16_t shortId = (uint16_t)(chipId & 0xFFFF);
  char idBuf[5];
  snprintf(idBuf, sizeof(idBuf), "%04X", shortId);
  return String("esp32_audio_") + idBuf;
}

// Get effective MQTT base topic (falls back to ALX/{serialNumber} if not
// configured)
String getEffectiveMqttBaseTopic() {
  if (appState.mqttBaseTopic.length() > 0) {
    return appState.mqttBaseTopic;
  }
  // Default: ALX/{appState.deviceSerialNumber}
  return String("ALX/") + appState.deviceSerialNumber;
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
  mqttClient.subscribe((base + "/settings/dark_mode/set").c_str());
  mqttClient.subscribe((base + "/settings/cert_validation/set").c_str());
  mqttClient.subscribe((base + "/settings/screen_timeout/set").c_str());
  mqttClient.subscribe((base + "/settings/device_name/set").c_str());
  mqttClient.subscribe((base + "/display/dim_enabled/set").c_str());
  mqttClient.subscribe((base + "/settings/dim_timeout/set").c_str());
  mqttClient.subscribe((base + "/display/backlight/set").c_str());
  mqttClient.subscribe((base + "/display/brightness/set").c_str());
  mqttClient.subscribe((base + "/display/dim_brightness/set").c_str());
  mqttClient.subscribe((base + "/settings/buzzer/set").c_str());
  mqttClient.subscribe((base + "/settings/buzzer_volume/set").c_str());
  mqttClient.subscribe((base + "/settings/audio_update_rate/set").c_str());
  mqttClient.subscribe((base + "/system/reboot").c_str());
  mqttClient.subscribe((base + "/system/factory_reset").c_str());
  mqttClient.subscribe((base + "/system/check_update").c_str());
  mqttClient.subscribe((base + "/system/update/command").c_str());
  mqttClient.subscribe((base + "/signalgenerator/enabled/set").c_str());
  mqttClient.subscribe((base + "/signalgenerator/waveform/set").c_str());
  mqttClient.subscribe((base + "/signalgenerator/frequency/set").c_str());
  mqttClient.subscribe((base + "/signalgenerator/amplitude/set").c_str());
  mqttClient.subscribe((base + "/signalgenerator/channel/set").c_str());
  mqttClient.subscribe((base + "/signalgenerator/output_mode/set").c_str());
  mqttClient.subscribe((base + "/signalgenerator/target_adc/set").c_str());
#ifdef DSP_ENABLED
  mqttClient.subscribe((base + "/emergency_limiter/enabled/set").c_str());
  mqttClient.subscribe((base + "/emergency_limiter/threshold/set").c_str());
#endif
  mqttClient.subscribe((base + "/settings/adc_vref/set").c_str());
  mqttClient.subscribe((base + "/audio/input1/enabled/set").c_str());
  mqttClient.subscribe((base + "/audio/input2/enabled/set").c_str());
  mqttClient.subscribe((base + "/audio/vu_meter/set").c_str());
  mqttClient.subscribe((base + "/audio/waveform/set").c_str());
  mqttClient.subscribe((base + "/audio/spectrum/set").c_str());
  mqttClient.subscribe((base + "/audio/fft_window/set").c_str());
  mqttClient.subscribe((base + "/debug/mode/set").c_str());
  mqttClient.subscribe((base + "/debug/serial_level/set").c_str());
  mqttClient.subscribe((base + "/debug/hw_stats/set").c_str());
  mqttClient.subscribe((base + "/debug/i2s_metrics/set").c_str());
  mqttClient.subscribe((base + "/debug/task_monitor/set").c_str());
  mqttClient.subscribe((base + "/signalgenerator/sweep_speed/set").c_str());
  mqttClient.subscribe((base + "/settings/timezone_offset/set").c_str());
#ifdef GUI_ENABLED
  mqttClient.subscribe((base + "/settings/boot_animation/set").c_str());
  mqttClient.subscribe((base + "/settings/boot_animation_style/set").c_str());
#endif
#ifdef DSP_ENABLED
  mqttClient.subscribe((base + "/dsp/enabled/set").c_str());
  mqttClient.subscribe((base + "/dsp/bypass/set").c_str());
  for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
    mqttClient.subscribe((base + "/dsp/channel_" + String(ch) + "/bypass/set").c_str());
  }
  mqttClient.subscribe((base + "/dsp/peq/bypass/set").c_str());
  mqttClient.subscribe((base + "/dsp/preset/set").c_str());
#endif

  // Subscribe to HA birth message for re-discovery after HA restart
  mqttClient.subscribe("homeassistant/status");

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

  // Handle Home Assistant restart — re-publish discovery so HA picks up current device info
  if (topicStr == "homeassistant/status") {
    if (message == "online") {
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
  if (topicStr == base + "/led/blinking/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
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

    if (validMode && appState.currentMode != newMode) {
      appState.currentMode = newMode;
      LOG_I("[MQTT] Mode set to %s", message.c_str());

      if (appState.currentMode == SMART_AUTO) {
        appState.timerRemaining = appState.timerDuration * 60;
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
  else if (topicStr == base + "/smartsensing/timer_duration/set") {
    int duration = message.toInt();
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
  else if (topicStr == base + "/smartsensing/audio_threshold/set") {
    float threshold = message.toFloat();
    if (threshold >= -96.0 && threshold <= 0.0) {
      appState.audioThreshold_dBFS = threshold;
      saveSmartSensingSettings();
      sendSmartSensingStateInternal();
      LOG_I("[MQTT] Audio threshold set to %+.0f dBFS", threshold);
    }
    publishMqttSmartSensingState();
  }
  // Handle AP toggle
  else if (topicStr == base + "/ap/enabled/set") {
    bool enabled = (message == "ON" || message == "1" || message == "true");
    appState.apEnabled = enabled;

    if (enabled) {
      if (!appState.isAPMode) {
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(appState.apSSID.c_str(), appState.apPassword);
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
  else if (topicStr == base + "/settings/auto_update/set") {
    bool enabled = (message == "ON" || message == "1" || message == "true");
    if (appState.autoUpdateEnabled != enabled) {
      appState.autoUpdateEnabled = enabled;
      saveSettings();
      LOG_I("[MQTT] Auto-update set to %s", enabled ? "ON" : "OFF");
      sendWiFiStatus(); // Broadcast to web clients
    }
    publishMqttSystemStatus();
  }
  // Handle night mode setting
  else if (topicStr == base + "/settings/dark_mode/set") {
    bool enabled = (message == "ON" || message == "1" || message == "true");
    if (appState.darkMode != enabled) {
      appState.darkMode = enabled;
      saveSettings();
      LOG_I("[MQTT] Dark mode set to %s", enabled ? "ON" : "OFF");
      sendWiFiStatus(); // Dark mode is part of WiFi status in web UI
    }
    publishMqttSystemStatus();
  }
  // Handle certificate validation setting
  else if (topicStr == base + "/settings/cert_validation/set") {
    bool enabled = (message == "ON" || message == "1" || message == "true");
    if (appState.enableCertValidation != enabled) {
      appState.enableCertValidation = enabled;
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
  // Handle dim enabled control
  else if (topicStr == base + "/display/dim_enabled/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.setDimEnabled(newState);
    saveSettings();
    LOG_I("[MQTT] Dim %s", newState ? "enabled" : "disabled");
    publishMqttDisplayState();
  }
  // Handle dim timeout setting
  else if (topicStr == base + "/settings/dim_timeout/set") {
    int dimSec = message.toInt();
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
  else if (topicStr == base + "/display/dim_brightness/set") {
    int pct = message.toInt();
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
  // Handle audio update rate
  else if (topicStr == base + "/settings/audio_update_rate/set") {
    int rate = message.toInt();
    if (rate == 20 || rate == 33 || rate == 50 || rate == 100) {
      appState.audioUpdateRate = (uint16_t)rate;
      saveSettings();
      LOG_I("[MQTT] Audio update rate set to %d ms", rate);
      publishMqttDisplayState();
    }
  }
  // Handle signal generator enable/disable
  else if (topicStr == base + "/signalgenerator/enabled/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.sigGenEnabled = newState;
    siggen_apply_params();
    LOG_I("[MQTT] Signal generator %s", newState ? "enabled" : "disabled");
    publishMqttSignalGenState();
    sendSignalGenState();
  }
  // Handle signal generator waveform
  else if (topicStr == base + "/signalgenerator/waveform/set") {
    int wf = -1;
    if (message == "sine") wf = 0;
    else if (message == "square") wf = 1;
    else if (message == "white_noise") wf = 2;
    else if (message == "sweep") wf = 3;
    if (wf >= 0) {
      appState.sigGenWaveform = wf;
      siggen_apply_params();
      saveSignalGenSettings();
      LOG_I("[MQTT] Signal generator waveform set to %s", message.c_str());
      publishMqttSignalGenState();
      sendSignalGenState();
    }
  }
  // Handle signal generator frequency
  else if (topicStr == base + "/signalgenerator/frequency/set") {
    float freq = message.toFloat();
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
  else if (topicStr == base + "/signalgenerator/amplitude/set") {
    float amp = message.toFloat();
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
  else if (topicStr == base + "/signalgenerator/channel/set") {
    int ch = -1;
    if (message == "ch1") ch = 0;
    else if (message == "ch2") ch = 1;
    else if (message == "both") ch = 2;
    if (ch >= 0) {
      appState.sigGenChannel = ch;
      siggen_apply_params();
      saveSignalGenSettings();
      LOG_I("[MQTT] Signal generator channel set to %s", message.c_str());
      publishMqttSignalGenState();
      sendSignalGenState();
    }
  }
  // Handle signal generator output mode
  else if (topicStr == base + "/signalgenerator/output_mode/set") {
    int mode = -1;
    if (message == "software") mode = 0;
    else if (message == "pwm") mode = 1;
    if (mode >= 0) {
      appState.sigGenOutputMode = mode;
      siggen_apply_params();
      saveSignalGenSettings();
      LOG_I("[MQTT] Signal generator output mode set to %s", message.c_str());
      publishMqttSignalGenState();
      sendSignalGenState();
    }
  }
  // Handle signal generator target ADC
  else if (topicStr == base + "/signalgenerator/target_adc/set") {
    int target = -1;
    if (message == "adc1") target = 0;
    else if (message == "adc2") target = 1;
    else if (message == "both") target = 2;
    if (target >= 0) {
      appState.sigGenTargetAdc = target;
      siggen_apply_params();
      saveSignalGenSettings();
      LOG_I("[MQTT] Signal generator target ADC set to %s", message.c_str());
      publishMqttSignalGenState();
      sendSignalGenState();
    }
  }
#ifdef DSP_ENABLED
  // Handle emergency limiter enabled
  else if (topicStr == base + "/emergency_limiter/enabled/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.setEmergencyLimiterEnabled(newState);
    saveSettings();
    LOG_I("[MQTT] Emergency limiter set to %s", newState ? "ON" : "OFF");
    publishMqttEmergencyLimiterState();
    sendEmergencyLimiterState();
  }
  // Handle emergency limiter threshold
  else if (topicStr == base + "/emergency_limiter/threshold/set") {
    float threshold = message.toFloat();
    if (threshold >= -6.0f && threshold <= 0.0f) {
      appState.setEmergencyLimiterThreshold(threshold);
      saveSettings();
      LOG_I("[MQTT] Emergency limiter threshold set to %.2f dBFS", threshold);
      publishMqttEmergencyLimiterState();
      sendEmergencyLimiterState();
    }
  }
#endif
  // Handle ADC reference voltage
  else if (topicStr == base + "/settings/adc_vref/set") {
    float vref = message.toFloat();
    if (vref >= 1.0f && vref <= 5.0f) {
      appState.adcVref = vref;
      saveSmartSensingSettings();
      LOG_I("[MQTT] ADC VREF set to %.2f V", vref);
      publishMqttAudioDiagnostics();
    }
  }
  // Handle per-ADC enable/disable
  else if (topicStr == base + "/audio/input1/enabled/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.adcEnabled[0] = newState;
    saveSettings();
    appState.markAdcEnabledDirty();
    LOG_I("[MQTT] ADC1 set to %s", newState ? "ON" : "OFF");
  }
  else if (topicStr == base + "/audio/input2/enabled/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.adcEnabled[1] = newState;
    saveSettings();
    appState.markAdcEnabledDirty();
    LOG_I("[MQTT] ADC2 set to %s", newState ? "ON" : "OFF");
  }
  // Handle VU meter toggle
  else if (topicStr == base + "/audio/vu_meter/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.vuMeterEnabled = newState;
    saveSettings();
    sendAudioGraphState();
    publishMqttAudioGraphState();
    LOG_I("[MQTT] VU meter set to %s", newState ? "ON" : "OFF");
  }
  // Handle waveform toggle
  else if (topicStr == base + "/audio/waveform/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.waveformEnabled = newState;
    saveSettings();
    sendAudioGraphState();
    publishMqttAudioGraphState();
    LOG_I("[MQTT] Waveform set to %s", newState ? "ON" : "OFF");
  }
  // Handle spectrum toggle
  else if (topicStr == base + "/audio/spectrum/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.spectrumEnabled = newState;
    saveSettings();
    sendAudioGraphState();
    publishMqttAudioGraphState();
    LOG_I("[MQTT] Spectrum set to %s", newState ? "ON" : "OFF");
  }
  // Handle FFT window type
  else if (topicStr == base + "/audio/fft_window/set") {
    // Accept window name strings: hann, blackman, blackman_harris, blackman_nuttall, nuttall, flat_top
    FftWindowType wt = FFT_WINDOW_HANN;
    if (message == "blackman") wt = FFT_WINDOW_BLACKMAN;
    else if (message == "blackman_harris") wt = FFT_WINDOW_BLACKMAN_HARRIS;
    else if (message == "blackman_nuttall") wt = FFT_WINDOW_BLACKMAN_NUTTALL;
    else if (message == "nuttall") wt = FFT_WINDOW_NUTTALL;
    else if (message == "flat_top") wt = FFT_WINDOW_FLAT_TOP;
    appState.fftWindowType = wt;
    saveSettings();
    sendAudioGraphState();
    publishMqttAudioGraphState();
    LOG_I("[MQTT] FFT window set to %d", (int)wt);
  }
  // Handle debug mode toggle
  else if (topicStr == base + "/debug/mode/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.debugMode = newState;
    applyDebugSerialLevel(appState.debugMode, appState.debugSerialLevel);
    saveSettings();
    sendDebugState();
    publishMqttDebugState();
    LOG_I("[MQTT] Debug mode set to %s", newState ? "ON" : "OFF");
  }
  // Handle debug serial level
  else if (topicStr == base + "/debug/serial_level/set") {
    int level = message.toInt();
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
  else if (topicStr == base + "/debug/hw_stats/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.debugHwStats = newState;
    saveSettings();
    sendDebugState();
    publishMqttDebugState();
    LOG_I("[MQTT] Debug HW stats set to %s", newState ? "ON" : "OFF");
  }
  // Handle debug I2S metrics toggle
  else if (topicStr == base + "/debug/i2s_metrics/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.debugI2sMetrics = newState;
    saveSettings();
    sendDebugState();
    publishMqttDebugState();
    LOG_I("[MQTT] Debug I2S metrics set to %s", newState ? "ON" : "OFF");
  }
  // Handle debug task monitor toggle
  else if (topicStr == base + "/debug/task_monitor/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.debugTaskMonitor = newState;
    saveSettings();
    sendDebugState();
    publishMqttDebugState();
    LOG_I("[MQTT] Debug task monitor set to %s", newState ? "ON" : "OFF");
  }
  // Handle timezone offset
  else if (topicStr == base + "/settings/timezone_offset/set") {
    int offset = message.toInt();
    if (offset >= -12 && offset <= 14) {
      appState.timezoneOffset = offset;
      saveSettings();
      LOG_I("[MQTT] Timezone offset set to %d", offset);
      publishMqttSystemStatus();
    }
  }
  // Handle signal generator sweep speed
  else if (topicStr == base + "/signalgenerator/sweep_speed/set") {
    float speed = message.toFloat();
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
  else if (topicStr == base + "/settings/boot_animation/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.bootAnimEnabled = newState;
    saveSettings();
    LOG_I("[MQTT] Boot animation set to %s", newState ? "ON" : "OFF");
    publishMqttBootAnimState();
  }
  // Handle boot animation style
  else if (topicStr == base + "/settings/boot_animation_style/set") {
    int style = -1;
    if (message == "wave_pulse") style = 0;
    else if (message == "speaker_ripple") style = 1;
    else if (message == "waveform") style = 2;
    else if (message == "beat_bounce") style = 3;
    else if (message == "freq_bars") style = 4;
    else if (message == "heartbeat") style = 5;
    if (style >= 0) {
      appState.bootAnimStyle = style;
      saveSettings();
      LOG_I("[MQTT] Boot animation style set to %s", message.c_str());
      publishMqttBootAnimState();
    }
  }
#endif
#ifdef DSP_ENABLED
  // Handle DSP enable/disable
  else if (topicStr == base + "/dsp/enabled/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.dspEnabled = newState;
    saveDspSettingsDebounced();
    appState.markDspConfigDirty();
    LOG_I("[MQTT] DSP set to %s", newState ? "ON" : "OFF");
  }
  // Handle DSP global bypass
  else if (topicStr == base + "/dsp/bypass/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
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
  else if (topicStr.startsWith(base + "/dsp/channel_") && topicStr.endsWith("/bypass/set")) {
    // Extract channel number from topic
    int chStart = (base + "/dsp/channel_").length();
    int chEnd = topicStr.indexOf("/bypass/set");
    if (chEnd > chStart) {
      int ch = topicStr.substring(chStart, chEnd).toInt();
      if (ch >= 0 && ch < DSP_MAX_CHANNELS) {
        bool newState = (message == "ON" || message == "1" || message == "true");
        dsp_copy_active_to_inactive();
        DspState *cfg = dsp_get_inactive_config();
        cfg->channels[ch].bypass = newState;
        if (!dsp_swap_config()) { appState.dspSwapFailures++; appState.lastDspSwapFailure = millis(); LOG_W("[MQTT] Swap failed, staged for retry"); }
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();
        LOG_I("[MQTT] DSP channel %d bypass set to %s", ch, newState ? "ON" : "OFF");
      }
    }
  }
  // PEQ band enable/disable (L1/R1 = channels 0/1)
  else if (topicStr.startsWith(base + "/dsp/channel_") && topicStr.indexOf("/peq/band") > 0 && topicStr.endsWith("/set")) {
    int chStart = (base + "/dsp/channel_").length();
    int chEnd = topicStr.indexOf("/peq/band");
    int bandStart = chEnd + 9;
    int bandEnd = topicStr.indexOf("/set", bandStart);
    if (chEnd > chStart && bandEnd > bandStart) {
      int ch = topicStr.substring(chStart, chEnd).toInt();
      int band = topicStr.substring(bandStart, bandEnd).toInt() - 1;
      if (ch >= 0 && ch < 2 && band >= 0 && band < DSP_PEQ_BANDS) {
        bool newState = (message == "ON" || message == "1" || message == "true");
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
  else if (topicStr == base + "/dsp/peq/bypass/set") {
    bool bypass = (message == "ON" || message == "1" || message == "true");
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
  else if (topicStr == base + "/dsp/preset/set") {
    int slot = message.toInt();
    if (message == "Custom" || message == "-1") {
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
      if (appState.updateAvailable && appState.cachedFirmwareUrl.length() > 0) {
        startOTADownloadTask();  // Non-blocking FreeRTOS task
      } else {
        LOG_W("[MQTT] No update available or firmware URL missing");
      }
    }
  }
  // Handle custom device name
  else if (topicStr == base + "/settings/device_name/set") {
    String name = message;
    if ((int)name.length() > 32) name = name.substring(0, 32);
    appState.customDeviceName = name;
    // Update AP SSID to reflect new custom name
    String apName = appState.customDeviceName.length() > 0
                      ? appState.customDeviceName
                      : ("ALX-Nova-" + appState.deviceSerialNumber);
    if ((int)apName.length() > 32) apName = apName.substring(0, 32);
    appState.apSSID = apName;
    saveSettings();
    sendWiFiStatus(); // Broadcast to web clients
    LOG_I("[MQTT] Custom device name set to: '%s'", name.c_str());
    mqttClient.publish((base + "/settings/device_name").c_str(), name.c_str(), true);
  }
}

// Setup MQTT client
void setupMqtt() {
  if (!appState.mqttEnabled || appState.mqttBroker.length() == 0) {
    LOG_I("[MQTT] Disabled or no broker configured");
    return;
  }

  LOG_I("[MQTT] Setting up...");
  LOG_I("[MQTT] Broker: %s:%d", appState.mqttBroker.c_str(), appState.mqttPort);
  LOG_I("[MQTT] Base Topic: %s", appState.mqttBaseTopic.c_str());
  LOG_I("[MQTT] HA Discovery: %s", appState.mqttHADiscovery ? "enabled" : "disabled");

  mqttClient.setServer(appState.mqttBroker.c_str(), appState.mqttPort);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(1024); // Increase buffer for HA discovery payloads

  // Attempt initial connection
  mqttReconnect();
}

// Reconnect to MQTT broker with exponential backoff
void mqttReconnect() {
  if (!appState.mqttEnabled || appState.mqttBroker.length() == 0) {
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
    if (!mqttWifiClient.connect(appState.mqttBroker.c_str(), appState.mqttPort, 1000)) {
      LOG_W("[MQTT] TCP connect timeout (1s) to %s:%d", appState.mqttBroker.c_str(), appState.mqttPort);
      appState.mqttConnected = false;
      appState.increaseMqttBackoff();
      LOG_W("[MQTT] Next retry in %lums", appState.mqttBackoffDelay);
      return;
    }
  }

  String clientId = getMqttDeviceId();
  String lwt = getEffectiveMqttBaseTopic() + "/status";

  bool connected = false;

  if (appState.mqttUsername.length() > 0) {
    connected = mqttClient.connect(clientId.c_str(), appState.mqttUsername.c_str(),
                                   appState.mqttPassword.c_str(), lwt.c_str(), 0, true,
                                   "offline");
  } else {
    connected =
        mqttClient.connect(clientId.c_str(), lwt.c_str(), 0, true, "offline");
  }

  if (connected) {
    LOG_I("[MQTT] Connected to %s:%d", appState.mqttBroker.c_str(), appState.mqttPort);
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
  if (!appState.mqttEnabled || appState.mqttBroker.length() == 0) {
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
    String base = getEffectiveMqttBaseTopic();
    mqttClient.publish((base + "/system/uptime").c_str(),
                       String(millis() / 1000).c_str(), true);
  }
}

// ===== MQTT State Publishing Functions =====

// Publish LED state
void publishMqttLedState() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();
  mqttClient.publish((base + "/led/state").c_str(), appState.ledState ? "ON" : "OFF",
                     true);
}

// Publish blinking state
void publishMqttBlinkingState() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();
  mqttClient.publish((base + "/led/blinking").c_str(),
                     appState.blinkingEnabled ? "ON" : "OFF", true);
}

// Publish Smart Sensing state
void publishMqttSmartSensingState() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();

  // Convert mode enum to string
  String modeStr;
  switch (appState.currentMode) {
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
                     appState.amplifierState ? "ON" : "OFF", true);
  mqttClient.publish((base + "/smartsensing/timer_duration").c_str(),
                     String(appState.timerDuration).c_str(), true);
  mqttClient.publish((base + "/smartsensing/timer_remaining").c_str(),
                     String(appState.timerRemaining).c_str(), true);
  mqttClient.publish((base + "/smartsensing/audio_level").c_str(),
                     String(appState.audioLevel_dBFS, 1).c_str(), true);
  mqttClient.publish((base + "/smartsensing/audio_threshold").c_str(),
                     String(appState.audioThreshold_dBFS, 1).c_str(), true);
  mqttClient.publish((base + "/smartsensing/signal_detected").c_str(),
                     (appState.audioLevel_dBFS >= appState.audioThreshold_dBFS) ? "ON" : "OFF",
                     true);

  // Last signal detection timestamp (seconds since boot, 0 if never detected)
  unsigned long lastDetectionSecs =
      appState.lastSignalDetection > 0 ? appState.lastSignalDetection / 1000 : 0;
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

  mqttClient.publish((base + "/ap/enabled").c_str(), appState.apEnabled ? "ON" : "OFF",
                     true);

  if (appState.isAPMode) {
    mqttClient.publish((base + "/ap/ip").c_str(),
                       WiFi.softAPIP().toString().c_str(), true);
    mqttClient.publish((base + "/ap/ssid").c_str(), appState.apSSID.c_str(), true);
  }
}

// Publish static system info (called once on MQTT connect)
void publishMqttSystemStatusStatic() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();

  mqttClient.publish((base + "/system/manufacturer").c_str(), MANUFACTURER_NAME,
                     true);
  mqttClient.publish((base + "/system/model").c_str(), MANUFACTURER_MODEL,
                     true);
  mqttClient.publish((base + "/system/serial_number").c_str(),
                     appState.deviceSerialNumber.c_str(), true);
  mqttClient.publish((base + "/system/firmware").c_str(), firmwareVer, true);
  mqttClient.publish((base + "/system/mac").c_str(), WiFi.macAddress().c_str(),
                     true);
  mqttClient.publish((base + "/system/reset_reason").c_str(),
                     getResetReasonString().c_str(), true);
}

// Publish dynamic system status (on settings change)
void publishMqttSystemStatus() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();

  mqttClient.publish((base + "/system/update_available").c_str(),
                     appState.updateAvailable ? "ON" : "OFF", true);
  if (appState.cachedLatestVersion.length() > 0) {
    mqttClient.publish((base + "/system/latest_version").c_str(),
                       appState.cachedLatestVersion.c_str(), true);
  }
  mqttClient.publish((base + "/settings/auto_update").c_str(),
                     appState.autoUpdateEnabled ? "ON" : "OFF", true);
  mqttClient.publish((base + "/settings/timezone_offset").c_str(),
                     String(appState.timezoneOffset).c_str(), true);
  mqttClient.publish((base + "/settings/dark_mode").c_str(),
                     appState.darkMode ? "ON" : "OFF", true);
  mqttClient.publish((base + "/settings/cert_validation").c_str(),
                     appState.enableCertValidation ? "ON" : "OFF", true);
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
      appState.cachedLatestVersion.length() > 0 ? appState.cachedLatestVersion : firmwareVer;
  doc["title"] = String(MANUFACTURER_MODEL) + " Firmware";
  doc["release_url"] = String("https://github.com/") + GITHUB_REPO_OWNER + "/" +
                       GITHUB_REPO_NAME + "/releases";
  doc["in_progress"] = appState.otaInProgress;
  if (appState.otaInProgress) {
    doc["update_percentage"] = appState.otaProgress;
  } else {
    doc["update_percentage"] = (char*)nullptr; // JSON null
  }

  // Add release summary if update available
  if (appState.updateAvailable && appState.cachedLatestVersion.length() > 0) {
    doc["release_summary"] =
        "New firmware version " + appState.cachedLatestVersion + " is available";
  }

  String json;
  serializeJson(doc, json);
  mqttClient.publish((base + "/system/update/state").c_str(), json.c_str(),
                     true);

  // Publish separate OTA progress topics for easier monitoring
  mqttClient.publish((base + "/system/update/in_progress").c_str(),
                     appState.otaInProgress ? "ON" : "OFF", true);
  mqttClient.publish((base + "/system/update/progress").c_str(),
                     String(appState.otaProgress).c_str(), true);
  mqttClient.publish((base + "/system/update/status").c_str(),
                     appState.otaStatus.c_str(), true);

  if (appState.otaStatusMessage.length() > 0) {
    mqttClient.publish((base + "/system/update/message").c_str(),
                       appState.otaStatusMessage.c_str(), true);
  }

  if (appState.otaTotalBytes > 0) {
    mqttClient.publish((base + "/system/update/bytes_downloaded").c_str(),
                       String(appState.otaProgressBytes).c_str(), true);
    mqttClient.publish((base + "/system/update/bytes_total").c_str(),
                       String(appState.otaTotalBytes).c_str(), true);
  }
}

// Publish static hardware stats (called once on MQTT connect)
void publishMqttHardwareStatsStatic() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();

  mqttClient.publish((base + "/hardware/cpu_model").c_str(), ESP.getChipModel(),
                     true);
  mqttClient.publish((base + "/hardware/cpu_cores").c_str(),
                     String(ESP.getChipCores()).c_str(), true);
  mqttClient.publish((base + "/hardware/cpu_freq").c_str(),
                     String(ESP.getCpuFreqMHz()).c_str(), true);
  mqttClient.publish((base + "/hardware/flash_size").c_str(),
                     String(ESP.getFlashChipSize()).c_str(), true);
  mqttClient.publish((base + "/hardware/sketch_size").c_str(),
                     String(ESP.getSketchSize()).c_str(), true);
  mqttClient.publish((base + "/hardware/sketch_free").c_str(),
                     String(ESP.getFreeSketchSpace()).c_str(), true);
  mqttClient.publish((base + "/hardware/heap_total").c_str(),
                     String(ESP.getHeapSize()).c_str(), true);
  mqttClient.publish((base + "/hardware/LittleFS_total").c_str(),
                     String(LittleFS.totalBytes()).c_str(), true);
  uint32_t psramSize = ESP.getPsramSize();
  if (psramSize > 0) {
    mqttClient.publish((base + "/hardware/psram_total").c_str(),
                       String(psramSize).c_str(), true);
  }
}

// Publish dynamic hardware stats (gated by debugMode && debugHwStats, called on heartbeat)
void publishMqttHardwareStats() {
  if (!mqttClient.connected())
    return;
  if (!appState.debugMode || !appState.debugHwStats)
    return;

  updateCpuUsage();

  String base = getEffectiveMqttBaseTopic();

  // Dynamic heap
  mqttClient.publish((base + "/hardware/heap_free").c_str(),
                     String(ESP.getFreeHeap()).c_str(), true);
  mqttClient.publish((base + "/hardware/heap_min_free").c_str(),
                     String(ESP.getMinFreeHeap()).c_str(), true);
  mqttClient.publish((base + "/hardware/heap_max_block").c_str(),
                     String(ESP.getMaxAllocHeap()).c_str(), true);

  // Dynamic PSRAM
  if (ESP.getPsramSize() > 0) {
    mqttClient.publish((base + "/hardware/psram_free").c_str(),
                       String(ESP.getFreePsram()).c_str(), true);
  }

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

  // Dynamic storage
  mqttClient.publish((base + "/hardware/LittleFS_used").c_str(),
                     String(LittleFS.usedBytes()).c_str(), true);

  // WiFi channel and AP clients
  mqttClient.publish((base + "/wifi/channel").c_str(),
                     String(WiFi.channel()).c_str(), true);
  mqttClient.publish((base + "/ap/clients").c_str(),
                     String(WiFi.softAPgetStationNum()).c_str(), true);

  // Task monitor
  const TaskMonitorData& tm = task_monitor_get_data();
  mqttClient.publish((base + "/hardware/task_count").c_str(),
                     String(tm.taskCount).c_str(), true);
  mqttClient.publish((base + "/hardware/loop_time_us").c_str(),
                     String(tm.loopTimeAvgUs).c_str(), true);
  mqttClient.publish((base + "/hardware/loop_time_max_us").c_str(),
                     String(tm.loopTimeMaxUs).c_str(), true);

  uint32_t minStackFree = UINT32_MAX;
  for (int i = 0; i < tm.taskCount; i++) {
    if (tm.tasks[i].stackAllocBytes > 0 && tm.tasks[i].stackFreeBytes < minStackFree) {
      minStackFree = tm.tasks[i].stackFreeBytes;
    }
  }
  if (minStackFree == UINT32_MAX) minStackFree = 0;
  mqttClient.publish((base + "/hardware/min_stack_free").c_str(),
                     String(minStackFree).c_str(), true);
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

  mqttClient.publish((base + "/display/dim_enabled").c_str(),
                     appState.dimEnabled ? "ON" : "OFF", true);
  mqttClient.publish((base + "/settings/dim_timeout").c_str(),
                     String(appState.dimTimeout / 1000).c_str(), true);

  // Publish dim brightness as percentage
  int dimPct = 10;
  if (appState.dimBrightness >= 191) dimPct = 75;
  else if (appState.dimBrightness >= 128) dimPct = 50;
  else if (appState.dimBrightness >= 64) dimPct = 25;
  else dimPct = 10;
  mqttClient.publish((base + "/display/dim_brightness").c_str(),
                     String(dimPct).c_str(), true);

  mqttClient.publish((base + "/settings/audio_update_rate").c_str(),
                     String(appState.audioUpdateRate).c_str(), true);
}

// Publish signal generator state
void publishMqttSignalGenState() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();

  mqttClient.publish((base + "/signalgenerator/enabled").c_str(),
                     appState.sigGenEnabled ? "ON" : "OFF", true);

  const char *waveNames[] = {"sine", "square", "white_noise", "sweep"};
  mqttClient.publish((base + "/signalgenerator/waveform").c_str(),
                     waveNames[appState.sigGenWaveform % 4], true);
  mqttClient.publish((base + "/signalgenerator/frequency").c_str(),
                     String(appState.sigGenFrequency, 0).c_str(), true);
  mqttClient.publish((base + "/signalgenerator/amplitude").c_str(),
                     String(appState.sigGenAmplitude, 0).c_str(), true);

  const char *chanNames[] = {"ch1", "ch2", "both"};
  mqttClient.publish((base + "/signalgenerator/channel").c_str(),
                     chanNames[appState.sigGenChannel % 3], true);

  mqttClient.publish((base + "/signalgenerator/output_mode").c_str(),
                     appState.sigGenOutputMode == 0 ? "software" : "pwm", true);

  mqttClient.publish((base + "/signalgenerator/sweep_speed").c_str(),
                     String(appState.sigGenSweepSpeed, 0).c_str(), true);

  const char *targetNames[] = {"adc1", "adc2", "both"};
  mqttClient.publish((base + "/signalgenerator/target_adc").c_str(),
                     targetNames[appState.sigGenTargetAdc % 3], true);
}

#ifdef DSP_ENABLED
void publishMqttEmergencyLimiterState() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();

  // Settings
  mqttClient.publish((base + "/emergency_limiter/enabled").c_str(),
                     appState.emergencyLimiterEnabled ? "ON" : "OFF", true);
  mqttClient.publish((base + "/emergency_limiter/threshold").c_str(),
                     String(appState.emergencyLimiterThresholdDb, 2).c_str(), true);

  // Status (from DSP metrics)
  DspMetrics metrics = dsp_get_metrics();
  mqttClient.publish((base + "/emergency_limiter/status").c_str(),
                     metrics.emergencyLimiterActive ? "active" : "idle", true);
  mqttClient.publish((base + "/emergency_limiter/trigger_count").c_str(),
                     String(metrics.emergencyLimiterTriggers).c_str(), true);
  mqttClient.publish((base + "/emergency_limiter/gain_reduction").c_str(),
                     String(metrics.emergencyLimiterGrDb, 2).c_str(), true);
}

void publishMqttAudioQualityState() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();

  // Settings
  mqttClient.publish((base + "/audio_quality/enabled").c_str(),
                     appState.audioQualityEnabled ? "ON" : "OFF", true);
  mqttClient.publish((base + "/audio_quality/glitch_threshold").c_str(),
                     String(appState.audioQualityGlitchThreshold, 2).c_str(), true);

  // Diagnostics
  AudioQualityDiag diag = audio_quality_get_diagnostics();
  mqttClient.publish((base + "/audio_quality/glitches_total").c_str(),
                     String(diag.glitchHistory.totalGlitches).c_str(), true);
  mqttClient.publish((base + "/audio_quality/glitches_last_minute").c_str(),
                     String(diag.glitchHistory.glitchesLastMinute).c_str(), true);
  mqttClient.publish((base + "/audio_quality/correlation_dsp_swap").c_str(),
                     diag.correlation.dspSwapRelated ? "ON" : "OFF", true);
  mqttClient.publish((base + "/audio_quality/correlation_wifi").c_str(),
                     diag.correlation.wifiRelated ? "ON" : "OFF", true);
}
#endif

void publishMqttAudioDiagnostics() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();

  // Per-ADC diagnostics (only publish detected ADCs)
  const char *inputLabels[] = {"adc1", "adc2"};
  int adcCount = appState.numAdcsDetected < NUM_AUDIO_ADCS ? appState.numAdcsDetected : NUM_AUDIO_ADCS;
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
    String prefix = base + "/audio/" + inputLabels[a];
    mqttClient.publish((prefix + "/adc_status").c_str(), statusStr, true);
    mqttClient.publish((prefix + "/noise_floor").c_str(),
                       String(adc.noiseFloorDbfs, 1).c_str(), true);
    mqttClient.publish((prefix + "/vrms").c_str(),
                       String(adc.vrmsCombined, 3).c_str(), true);
    mqttClient.publish((prefix + "/level").c_str(),
                       String(adc.dBFS, 1).c_str(), true);
    if (appState.debugMode) {
      mqttClient.publish((prefix + "/snr").c_str(),
                         String(appState.audioSnrDb[a], 1).c_str(), true);
      mqttClient.publish((prefix + "/sfdr").c_str(),
                         String(appState.audioSfdrDb[a], 1).c_str(), true);
    }
  }

  // ADC clock sync topics (only when both ADCs are detected)
  if (appState.numAdcsDetected >= 2) {
    mqttClient.publish((base + "/audio/adc_sync_ok").c_str(),
                       appState.adcSyncOk ? "ON" : "OFF", true);
    mqttClient.publish((base + "/audio/adc_sync_offset").c_str(),
                       String(appState.adcSyncOffsetSamples, 2).c_str(), true);
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
  mqttClient.publish((base + "/audio/adc_status").c_str(), statusStr, true);
  mqttClient.publish((base + "/audio/noise_floor").c_str(),
                     String(appState.audioNoiseFloorDbfs, 1).c_str(), true);
  mqttClient.publish((base + "/audio/input_vrms").c_str(),
                     String(appState.audioVrmsCombined, 3).c_str(), true);
  mqttClient.publish((base + "/settings/adc_vref").c_str(),
                     String(appState.adcVref, 2).c_str(), true);
}

// Publish audio graph toggle state
void publishMqttAudioGraphState() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();

  mqttClient.publish((base + "/audio/vu_meter").c_str(),
                     appState.vuMeterEnabled ? "ON" : "OFF", true);
  mqttClient.publish((base + "/audio/waveform").c_str(),
                     appState.waveformEnabled ? "ON" : "OFF", true);
  mqttClient.publish((base + "/audio/spectrum").c_str(),
                     appState.spectrumEnabled ? "ON" : "OFF", true);
  mqttClient.publish((base + "/audio/fft_window").c_str(),
                     fftWindowName(appState.fftWindowType), true);
}

// Publish per-ADC enabled state
void publishMqttAdcEnabledState() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();
  mqttClient.publish((base + "/audio/input1/enabled").c_str(),
                     appState.adcEnabled[0] ? "ON" : "OFF", true);
  mqttClient.publish((base + "/audio/input2/enabled").c_str(),
                     appState.adcEnabled[1] ? "ON" : "OFF", true);
}

// Publish debug mode state
void publishMqttDebugState() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();

  mqttClient.publish((base + "/debug/mode").c_str(),
                     appState.debugMode ? "ON" : "OFF", true);
  mqttClient.publish((base + "/debug/serial_level").c_str(),
                     String(appState.debugSerialLevel).c_str(), true);
  mqttClient.publish((base + "/debug/hw_stats").c_str(),
                     appState.debugHwStats ? "ON" : "OFF", true);
  mqttClient.publish((base + "/debug/i2s_metrics").c_str(),
                     appState.debugI2sMetrics ? "ON" : "OFF", true);
  mqttClient.publish((base + "/debug/task_monitor").c_str(),
                     appState.debugTaskMonitor ? "ON" : "OFF", true);
}

#ifdef DSP_ENABLED
// Publish DSP pipeline state
void publishMqttDspState() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();

  mqttClient.publish((base + "/dsp/enabled").c_str(),
                     appState.dspEnabled ? "ON" : "OFF", true);
  mqttClient.publish((base + "/dsp/bypass").c_str(),
                     appState.dspBypass ? "ON" : "OFF", true);

  // Preset state
  if (appState.dspPresetIndex >= 0 && appState.dspPresetIndex < DSP_PRESET_MAX_SLOTS) {
    mqttClient.publish((base + "/dsp/preset").c_str(),
                       appState.dspPresetNames[appState.dspPresetIndex], true);
  } else {
    mqttClient.publish((base + "/dsp/preset").c_str(), "Custom", true);
  }

  // Per-channel bypass and stage count
  DspState *cfg = dsp_get_active_config();
  for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
    String prefix = base + "/dsp/channel_" + String(ch);
    mqttClient.publish((prefix + "/bypass").c_str(),
                       cfg->channels[ch].bypass ? "ON" : "OFF", true);
    mqttClient.publish((prefix + "/stage_count").c_str(),
                       String(cfg->channels[ch].stageCount).c_str(), true);
  }

  // Global PEQ bypass (derived from all channels)
  bool anyPeqBypassed = true;
  for (int ch = 0; ch < 2; ch++) {
    for (int b = 0; b < DSP_PEQ_BANDS && b < cfg->channels[ch].stageCount; b++) {
      if (cfg->channels[ch].stages[b].enabled) { anyPeqBypassed = false; break; }
    }
    if (!anyPeqBypassed) break;
  }
  mqttClient.publish((base + "/dsp/peq/bypass").c_str(), anyPeqBypassed ? "ON" : "OFF", true);

  // DSP metrics
  DspMetrics m = dsp_get_metrics();
  mqttClient.publish((base + "/dsp/cpu_load").c_str(),
                     String(m.cpuLoadPercent, 1).c_str(), true);
  for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
    mqttClient.publish((base + "/dsp/channel_" + String(ch) + "/limiter_gr").c_str(),
                       String(m.limiterGrDb[ch], 1).c_str(), true);
  }
}

#endif

// Publish static crash info (called once on MQTT connect — never changes per boot)
void publishMqttCrashDiagnosticsStatic() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();

  mqttClient.publish((base + "/diagnostics/reset_reason").c_str(),
                     getResetReasonString().c_str(), true);
  mqttClient.publish((base + "/diagnostics/was_crash").c_str(),
                     crashlog_last_was_crash() ? "ON" : "OFF", true);
}

// Publish dynamic crash diagnostics (heap health — called on heartbeat)
void publishMqttCrashDiagnostics() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();

  mqttClient.publish((base + "/diagnostics/heap_free").c_str(),
                     String(ESP.getFreeHeap()).c_str(), true);
  mqttClient.publish((base + "/diagnostics/heap_max_block").c_str(),
                     String(ESP.getMaxAllocHeap()).c_str(), true);
  mqttClient.publish((base + "/diagnostics/heap_critical").c_str(),
                     appState.heapCritical ? "ON" : "OFF", true);
  mqttClient.publish((base + "/diagnostics/heap_warning").c_str(),
                     (appState.heapWarning || appState.heapCritical) ? "ON" : "OFF", true);

  // Per-ADC I2S recovery counts
  mqttClient.publish((base + "/diagnostics/i2s_recoveries_adc1").c_str(),
                     String(appState.audioAdc[0].i2sRecoveries).c_str(), true);
  if (appState.numAdcsDetected >= 2) {
    mqttClient.publish((base + "/diagnostics/i2s_recoveries_adc2").c_str(),
                       String(appState.audioAdc[1].i2sRecoveries).c_str(), true);
  }
}

// Publish input names as read-only sensors
void publishMqttInputNames() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();

  const char *labels[] = {"input1_name_l", "input1_name_r", "input2_name_l", "input2_name_r"};
  for (int i = 0; i < NUM_AUDIO_ADCS * 2; i++) {
    mqttClient.publish((base + "/audio/" + labels[i]).c_str(),
                       appState.inputNames[i].c_str(), true);
  }
}

#ifdef GUI_ENABLED
// Publish boot animation state
void publishMqttBootAnimState() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();

  mqttClient.publish((base + "/settings/boot_animation").c_str(),
                     appState.bootAnimEnabled ? "ON" : "OFF", true);

  const char *styleNames[] = {"wave_pulse", "speaker_ripple", "waveform", "beat_bounce", "freq_bars", "heartbeat"};
  mqttClient.publish((base + "/settings/boot_animation_style").c_str(),
                     styleNames[appState.bootAnimStyle % 6], true);
}
#endif

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
  device["serial_number"] = appState.deviceSerialNumber;
  device["sw_version"] = firmwareVer;
  device["configuration_url"] = "http://" + WiFi.localIP().toString();

  // Availability — tells HA which topic indicates online/offline
  String availBase = getEffectiveMqttBaseTopic();
  JsonArray avail = doc["availability"].to<JsonArray>();
  JsonObject a = avail.add<JsonObject>();
  a["topic"] = availBase + "/status";
  a["payload_available"] = "online";
  a["payload_not_available"] = "offline";
}

// Publish Home Assistant auto-discovery configuration
void publishHADiscovery() {
  if (!mqttClient.connected() || !appState.mqttHADiscovery)
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

  // ===== Factory Reset Button =====
  {
    JsonDocument doc;
    doc["name"] = "Factory Reset";
    doc["unique_id"] = deviceId + "_factory_reset";
    doc["command_topic"] = base + "/system/factory_reset";
    doc["payload_press"] = "RESET";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:factory";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/button/" + deviceId + "/factory_reset/config";
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

  // ===== Dark Mode Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Dark Mode";
    doc["unique_id"] = deviceId + "_dark_mode";
    doc["state_topic"] = base + "/settings/dark_mode";
    doc["command_topic"] = base + "/settings/dark_mode/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:weather-night";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/dark_mode/config";
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

  // ===== Dim Enabled Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Dim";
    doc["unique_id"] = deviceId + "_dim_enabled";
    doc["state_topic"] = base + "/display/dim_enabled";
    doc["command_topic"] = base + "/display/dim_enabled/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:brightness-auto";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/switch/" + deviceId + "/dim_enabled/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Dim Timeout Number =====
  {
    JsonDocument doc;
    doc["name"] = "Dim Timeout";
    doc["unique_id"] = deviceId + "_dim_timeout";
    doc["state_topic"] = base + "/settings/dim_timeout";
    doc["command_topic"] = base + "/settings/dim_timeout/set";
    doc["min"] = 0;
    doc["max"] = 60;
    doc["step"] = 5;
    doc["unit_of_measurement"] = "s";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:brightness-auto";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/number/" + deviceId + "/dim_timeout/config";
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

  // ===== Dim Brightness Select =====
  {
    JsonDocument doc;
    doc["name"] = "Dim Brightness";
    doc["unique_id"] = deviceId + "_dim_brightness";
    doc["state_topic"] = base + "/display/dim_brightness";
    doc["command_topic"] = base + "/display/dim_brightness/set";
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("10");
    options.add("25");
    options.add("50");
    options.add("75");
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:brightness-4";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/select/" + deviceId + "/dim_brightness/config";
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


  // ===== Audio Update Rate Select =====
  {
    JsonDocument doc;
    doc["name"] = "Audio Update Rate";
    doc["unique_id"] = deviceId + "_audio_update_rate";
    doc["state_topic"] = base + "/settings/audio_update_rate";
    doc["command_topic"] = base + "/settings/audio_update_rate/set";
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("20");
    options.add("33");
    options.add("50");
    options.add("100");
    doc["unit_of_measurement"] = "ms";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:update";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic =
        "homeassistant/select/" + deviceId + "/audio_update_rate/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Signal Generator Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Generator";
    doc["unique_id"] = deviceId + "_siggen_enabled";
    doc["state_topic"] = base + "/signalgenerator/enabled";
    doc["command_topic"] = base + "/signalgenerator/enabled/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:sine-wave";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/siggen_enabled/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Signal Generator Waveform Select =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Waveform";
    doc["unique_id"] = deviceId + "_siggen_waveform";
    doc["state_topic"] = base + "/signalgenerator/waveform";
    doc["command_topic"] = base + "/signalgenerator/waveform/set";
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("sine");
    options.add("square");
    options.add("white_noise");
    options.add("sweep");
    doc["icon"] = "mdi:waveform";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/select/" + deviceId + "/siggen_waveform/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Signal Generator Frequency Number =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Frequency";
    doc["unique_id"] = deviceId + "_siggen_frequency";
    doc["state_topic"] = base + "/signalgenerator/frequency";
    doc["command_topic"] = base + "/signalgenerator/frequency/set";
    doc["min"] = 1;
    doc["max"] = 22000;
    doc["step"] = 1;
    doc["unit_of_measurement"] = "Hz";
    doc["icon"] = "mdi:sine-wave";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/number/" + deviceId + "/siggen_frequency/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Signal Generator Amplitude Number =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Amplitude";
    doc["unique_id"] = deviceId + "_siggen_amplitude";
    doc["state_topic"] = base + "/signalgenerator/amplitude";
    doc["command_topic"] = base + "/signalgenerator/amplitude/set";
    doc["min"] = -96;
    doc["max"] = 0;
    doc["step"] = 1;
    doc["unit_of_measurement"] = "dBFS";
    doc["icon"] = "mdi:volume-high";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/number/" + deviceId + "/siggen_amplitude/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Signal Generator Channel Select =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Channel";
    doc["unique_id"] = deviceId + "_siggen_channel";
    doc["state_topic"] = base + "/signalgenerator/channel";
    doc["command_topic"] = base + "/signalgenerator/channel/set";
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("ch1");
    options.add("ch2");
    options.add("both");
    doc["icon"] = "mdi:speaker-multiple";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/select/" + deviceId + "/siggen_channel/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Signal Generator Output Mode Select =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Output Mode";
    doc["unique_id"] = deviceId + "_siggen_output_mode";
    doc["state_topic"] = base + "/signalgenerator/output_mode";
    doc["command_topic"] = base + "/signalgenerator/output_mode/set";
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("software");
    options.add("pwm");
    doc["icon"] = "mdi:export";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/select/" + deviceId + "/siggen_output_mode/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Signal Generator Target ADC Select =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Target ADC";
    doc["unique_id"] = deviceId + "_siggen_target_adc";
    doc["state_topic"] = base + "/signalgenerator/target_adc";
    doc["command_topic"] = base + "/signalgenerator/target_adc/set";
    JsonArray options = doc["options"].to<JsonArray>();
    options.add("adc1");
    options.add("adc2");
    options.add("both");
    doc["icon"] = "mdi:audio-input-stereo-minijack";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/select/" + deviceId + "/siggen_target_adc/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }


  // ===== Per-ADC Audio Diagnostic Entities (only detected ADCs) =====
  {
    const char *inputLabels[] = {"adc1", "adc2"};
    const char *inputNames[] = {"ADC 1", "ADC 2"};
    int adcCount = appState.numAdcsDetected < NUM_AUDIO_ADCS ? appState.numAdcsDetected : NUM_AUDIO_ADCS;
    for (int a = 0; a < adcCount; a++) {
      String prefix = base + "/audio/" + inputLabels[a];
      String idSuffix = String("_") + inputLabels[a];

      // Per-ADC Level Sensor
      {
        JsonDocument doc;
        doc["name"] = String(inputNames[a]) + " Audio Level";
        doc["unique_id"] = deviceId + idSuffix + "_level";
        doc["state_topic"] = prefix + "/level";
        doc["unit_of_measurement"] = "dBFS";
        doc["state_class"] = "measurement";
        doc["icon"] = "mdi:volume-high";
        addHADeviceInfo(doc);
        String payload;
        serializeJson(doc, payload);
        mqttClient.publish(("homeassistant/sensor/" + deviceId + "/" + inputLabels[a] + "_level/config").c_str(), payload.c_str(), true);
      }

      // Per-ADC Status Sensor
      {
        JsonDocument doc;
        doc["name"] = String(inputNames[a]) + " ADC Status";
        doc["unique_id"] = deviceId + idSuffix + "_adc_status";
        doc["state_topic"] = prefix + "/adc_status";
        doc["entity_category"] = "diagnostic";
        doc["icon"] = "mdi:audio-input-stereo-minijack";
        addHADeviceInfo(doc);
        String payload;
        serializeJson(doc, payload);
        mqttClient.publish(("homeassistant/sensor/" + deviceId + "/" + inputLabels[a] + "_adc_status/config").c_str(), payload.c_str(), true);
      }

      // Per-ADC Noise Floor Sensor
      {
        JsonDocument doc;
        doc["name"] = String(inputNames[a]) + " Noise Floor";
        doc["unique_id"] = deviceId + idSuffix + "_noise_floor";
        doc["state_topic"] = prefix + "/noise_floor";
        doc["unit_of_measurement"] = "dBFS";
        doc["state_class"] = "measurement";
        doc["entity_category"] = "diagnostic";
        doc["icon"] = "mdi:volume-low";
        addHADeviceInfo(doc);
        String payload;
        serializeJson(doc, payload);
        mqttClient.publish(("homeassistant/sensor/" + deviceId + "/" + inputLabels[a] + "_noise_floor/config").c_str(), payload.c_str(), true);
      }

      // Per-ADC Vrms Sensor
      {
        JsonDocument doc;
        doc["name"] = String(inputNames[a]) + " Vrms";
        doc["unique_id"] = deviceId + idSuffix + "_vrms";
        doc["state_topic"] = prefix + "/vrms";
        doc["unit_of_measurement"] = "V";
        doc["device_class"] = "voltage";
        doc["state_class"] = "measurement";
        doc["entity_category"] = "diagnostic";
        doc["suggested_display_precision"] = 3;
        doc["icon"] = "mdi:sine-wave";
        addHADeviceInfo(doc);
        String payload;
        serializeJson(doc, payload);
        mqttClient.publish(("homeassistant/sensor/" + deviceId + "/" + inputLabels[a] + "_vrms/config").c_str(), payload.c_str(), true);
      }

      // SNR/SFDR discovery removed — debug-only data, accessible via REST/WS/GUI.
      // Cleanup of orphaned entities is handled in removeHADiscovery().
    }
  }

  // ===== ADC Clock Sync Binary Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "ADC Clock Sync";
    doc["unique_id"] = deviceId + "_adc_sync_ok";
    doc["state_topic"] = base + "/audio/adc_sync_ok";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["device_class"] = "connectivity";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:sync";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/binary_sensor/" + deviceId + "/adc_sync_ok/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== ADC Sync Phase Offset Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "ADC Sync Phase Offset";
    doc["unique_id"] = deviceId + "_adc_sync_offset";
    doc["state_topic"] = base + "/audio/adc_sync_offset";
    doc["unit_of_measurement"] = "samples";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:sine-wave";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/adc_sync_offset/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Legacy Combined Audio ADC Status Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "ADC Status";
    doc["unique_id"] = deviceId + "_adc_status";
    doc["state_topic"] = base + "/audio/adc_status";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:audio-input-stereo-minijack";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/adc_status/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Legacy Combined Audio Noise Floor Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Audio Noise Floor";
    doc["unique_id"] = deviceId + "_noise_floor";
    doc["state_topic"] = base + "/audio/noise_floor";
    doc["unit_of_measurement"] = "dBFS";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:volume-low";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/noise_floor/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Legacy Combined Input Voltage (Vrms) Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Input Voltage (Vrms)";
    doc["unique_id"] = deviceId + "_input_vrms";
    doc["state_topic"] = base + "/audio/input_vrms";
    doc["unit_of_measurement"] = "V";
    doc["device_class"] = "voltage";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["suggested_display_precision"] = 3;
    doc["icon"] = "mdi:sine-wave";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + deviceId + "/input_vrms/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== ADC Reference Voltage Number =====
  {
    JsonDocument doc;
    doc["name"] = "ADC Reference Voltage";
    doc["unique_id"] = deviceId + "_adc_vref";
    doc["state_topic"] = base + "/settings/adc_vref";
    doc["command_topic"] = base + "/settings/adc_vref/set";
    doc["min"] = 1.0;
    doc["max"] = 5.0;
    doc["step"] = 0.1;
    doc["unit_of_measurement"] = "V";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:flash-triangle-outline";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/number/" + deviceId + "/adc_vref/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Per-ADC Enable Switches =====
  {
    const char *adcNames[] = {"ADC Input 1", "ADC Input 2"};
    const char *adcIds[] = {"input1_enabled", "input2_enabled"};
    const char *adcTopics[] = {"/audio/input1/enabled", "/audio/input2/enabled"};
    for (int a = 0; a < 2; a++) {
      JsonDocument doc;
      doc["name"] = adcNames[a];
      doc["unique_id"] = deviceId + "_" + adcIds[a];
      doc["state_topic"] = base + adcTopics[a];
      doc["command_topic"] = base + String(adcTopics[a]) + "/set";
      doc["payload_on"] = "ON";
      doc["payload_off"] = "OFF";
      doc["entity_category"] = "config";
      doc["icon"] = "mdi:audio-input-stereo-minijack";
      addHADeviceInfo(doc);
      String payload;
      serializeJson(doc, payload);
      String topic = "homeassistant/switch/" + deviceId + "/" + adcIds[a] + "/config";
      mqttClient.publish(topic.c_str(), payload.c_str(), true);
    }
  }

  // ===== VU Meter Switch =====
  {
    JsonDocument doc;
    doc["name"] = "VU Meter";
    doc["unique_id"] = deviceId + "_vu_meter";
    doc["state_topic"] = base + "/audio/vu_meter";
    doc["command_topic"] = base + "/audio/vu_meter/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:chart-bar";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/vu_meter/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Audio Waveform Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Audio Waveform";
    doc["unique_id"] = deviceId + "_waveform";
    doc["state_topic"] = base + "/audio/waveform";
    doc["command_topic"] = base + "/audio/waveform/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:waveform";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/waveform/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== Frequency Spectrum Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Frequency Spectrum";
    doc["unique_id"] = deviceId + "_spectrum";
    doc["state_topic"] = base + "/audio/spectrum";
    doc["command_topic"] = base + "/audio/spectrum/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:equalizer";
    addHADeviceInfo(doc);

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/switch/" + deviceId + "/spectrum/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }

  // ===== FFT Window Type Select =====
  {
    JsonDocument doc;
    doc["name"] = "FFT Window";
    doc["unique_id"] = deviceId + "_fft_window";
    doc["state_topic"] = base + "/audio/fft_window";
    doc["command_topic"] = base + "/audio/fft_window/set";
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

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/select/" + deviceId + "/fft_window/config";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
  }


  // ===== Debug Mode Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Debug Mode";
    doc["unique_id"] = deviceId + "_debug_mode";
    doc["state_topic"] = base + "/debug/mode";
    doc["command_topic"] = base + "/debug/mode/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:bug";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/debug_mode/config").c_str(), payload.c_str(), true);
  }

  // ===== Debug Serial Level Number =====
  {
    JsonDocument doc;
    doc["name"] = "Debug Serial Level";
    doc["unique_id"] = deviceId + "_debug_serial_level";
    doc["state_topic"] = base + "/debug/serial_level";
    doc["command_topic"] = base + "/debug/serial_level/set";
    doc["min"] = 0;
    doc["max"] = 3;
    doc["step"] = 1;
    doc["mode"] = "slider";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:console";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/number/" + deviceId + "/debug_serial_level/config").c_str(), payload.c_str(), true);
  }

  // ===== Debug HW Stats Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Debug HW Stats";
    doc["unique_id"] = deviceId + "_debug_hw_stats";
    doc["state_topic"] = base + "/debug/hw_stats";
    doc["command_topic"] = base + "/debug/hw_stats/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:chart-line";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/debug_hw_stats/config").c_str(), payload.c_str(), true);
  }

  // ===== Debug I2S Metrics Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Debug I2S Metrics";
    doc["unique_id"] = deviceId + "_debug_i2s_metrics";
    doc["state_topic"] = base + "/debug/i2s_metrics";
    doc["command_topic"] = base + "/debug/i2s_metrics/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:timer-outline";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/debug_i2s_metrics/config").c_str(), payload.c_str(), true);
  }

  // ===== Debug Task Monitor Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Debug Task Monitor";
    doc["unique_id"] = deviceId + "_debug_task_monitor";
    doc["state_topic"] = base + "/debug/task_monitor";
    doc["command_topic"] = base + "/debug/task_monitor/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:format-list-bulleted";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/debug_task_monitor/config").c_str(), payload.c_str(), true);
  }

  // ===== Task Monitor Diagnostic Sensors =====
  {
    JsonDocument doc;
    doc["name"] = "Task Count";
    doc["unique_id"] = deviceId + "_task_count";
    doc["state_topic"] = base + "/hardware/task_count";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:format-list-numbered";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/task_count/config").c_str(), payload.c_str(), true);
  }
  {
    JsonDocument doc;
    doc["name"] = "Loop Time";
    doc["unique_id"] = deviceId + "_loop_time";
    doc["state_topic"] = base + "/hardware/loop_time_us";
    doc["unit_of_measurement"] = "us";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:timer-outline";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/loop_time/config").c_str(), payload.c_str(), true);
  }
  {
    JsonDocument doc;
    doc["name"] = "Loop Time Max";
    doc["unique_id"] = deviceId + "_loop_time_max";
    doc["state_topic"] = base + "/hardware/loop_time_max_us";
    doc["unit_of_measurement"] = "us";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:timer-alert-outline";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/loop_time_max/config").c_str(), payload.c_str(), true);
  }
  {
    JsonDocument doc;
    doc["name"] = "Min Stack Free";
    doc["unique_id"] = deviceId + "_min_stack_free";
    doc["state_topic"] = base + "/hardware/min_stack_free";
    doc["unit_of_measurement"] = "B";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:memory";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/min_stack_free/config").c_str(), payload.c_str(), true);
  }

  // ===== Crash Diagnostics — Reset Reason (diagnostic sensor) =====
  {
    JsonDocument doc;
    doc["name"] = "Reset Reason";
    doc["unique_id"] = deviceId + "_reset_reason";
    doc["state_topic"] = base + "/diagnostics/reset_reason";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:restart-alert";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/reset_reason/config").c_str(), payload.c_str(), true);
  }

  // ===== Crash Diagnostics — Was Crash (binary sensor) =====
  {
    JsonDocument doc;
    doc["name"] = "Last Boot Was Crash";
    doc["unique_id"] = deviceId + "_was_crash";
    doc["state_topic"] = base + "/diagnostics/was_crash";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["device_class"] = "problem";
    doc["entity_category"] = "diagnostic";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/binary_sensor/" + deviceId + "/was_crash/config").c_str(), payload.c_str(), true);
  }

  // ===== Heap Health — Heap Warning (binary sensor) =====
  {
    JsonDocument doc;
    doc["name"] = "Heap Warning";
    doc["unique_id"] = deviceId + "_heap_warning";
    doc["state_topic"] = base + "/diagnostics/heap_warning";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["device_class"] = "problem";
    doc["entity_category"] = "diagnostic";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/binary_sensor/" + deviceId + "/heap_warning/config").c_str(), payload.c_str(), true);
  }

  // ===== Heap Health — Heap Critical (binary sensor) =====
  {
    JsonDocument doc;
    doc["name"] = "Heap Critical";
    doc["unique_id"] = deviceId + "_heap_critical";
    doc["state_topic"] = base + "/diagnostics/heap_critical";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["device_class"] = "problem";
    doc["entity_category"] = "diagnostic";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/binary_sensor/" + deviceId + "/heap_critical/config").c_str(), payload.c_str(), true);
  }

  // ===== Heap Health — Max Alloc Block (diagnostic sensor) =====
  {
    JsonDocument doc;
    doc["name"] = "Heap Max Block";
    doc["unique_id"] = deviceId + "_heap_max_block";
    doc["state_topic"] = base + "/diagnostics/heap_max_block";
    doc["unit_of_measurement"] = "B";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:memory";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/heap_max_block/config").c_str(), payload.c_str(), true);
  }

  // ===== Timezone Offset Number =====
  {
    JsonDocument doc;
    doc["name"] = "Timezone Offset";
    doc["unique_id"] = deviceId + "_timezone_offset";
    doc["state_topic"] = base + "/settings/timezone_offset";
    doc["command_topic"] = base + "/settings/timezone_offset/set";
    doc["min"] = -12;
    doc["max"] = 14;
    doc["step"] = 1;
    doc["unit_of_measurement"] = "h";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:map-clock-outline";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/number/" + deviceId + "/timezone_offset/config").c_str(), payload.c_str(), true);
  }

  // ===== Signal Generator Sweep Speed Number =====
  {
    JsonDocument doc;
    doc["name"] = "Signal Sweep Speed";
    doc["unique_id"] = deviceId + "_siggen_sweep_speed";
    doc["state_topic"] = base + "/signalgenerator/sweep_speed";
    doc["command_topic"] = base + "/signalgenerator/sweep_speed/set";
    doc["min"] = 0.1;
    doc["max"] = 10.0;
    doc["step"] = 0.1;
    doc["unit_of_measurement"] = "Hz/s";
    doc["icon"] = "mdi:speedometer";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/number/" + deviceId + "/siggen_sweep_speed/config").c_str(), payload.c_str(), true);
  }

  // ===== Input Names (4 read-only sensors) =====
  {
    const char *inputLabels[] = {"input1_name_l", "input1_name_r", "input2_name_l", "input2_name_r"};
    const char *inputDisplayNames[] = {"Input 1 Left Name", "Input 1 Right Name", "Input 2 Left Name", "Input 2 Right Name"};
    for (int i = 0; i < NUM_AUDIO_ADCS * 2; i++) {
      JsonDocument doc;
      doc["name"] = inputDisplayNames[i];
      doc["unique_id"] = deviceId + "_" + inputLabels[i];
      doc["state_topic"] = base + "/audio/" + inputLabels[i];
      doc["entity_category"] = "diagnostic";
      doc["icon"] = "mdi:label-outline";
      addHADeviceInfo(doc);
      String payload;
      serializeJson(doc, payload);
      mqttClient.publish(("homeassistant/sensor/" + deviceId + "/" + inputLabels[i] + "/config").c_str(), payload.c_str(), true);
    }
  }


#ifdef DSP_ENABLED
  // ===== DSP Enabled Switch =====
  {
    JsonDocument doc;
    doc["name"] = "DSP";
    doc["unique_id"] = deviceId + "_dsp_enabled";
    doc["state_topic"] = base + "/dsp/enabled";
    doc["command_topic"] = base + "/dsp/enabled/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:equalizer";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/dsp_enabled/config").c_str(), payload.c_str(), true);
  }

  // ===== DSP Bypass Switch =====
  {
    JsonDocument doc;
    doc["name"] = "DSP Bypass";
    doc["unique_id"] = deviceId + "_dsp_bypass";
    doc["state_topic"] = base + "/dsp/bypass";
    doc["command_topic"] = base + "/dsp/bypass/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:debug-step-over";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/dsp_bypass/config").c_str(), payload.c_str(), true);
  }

  // ===== DSP CPU Load Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "DSP CPU Load";
    doc["unique_id"] = deviceId + "_dsp_cpu_load";
    doc["state_topic"] = base + "/dsp/cpu_load";
    doc["unit_of_measurement"] = "%";
    doc["state_class"] = "measurement";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:cpu-64-bit";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/dsp_cpu_load/config").c_str(), payload.c_str(), true);
  }

  // ===== DSP Preset Select =====
  {
    JsonDocument doc;
    doc["name"] = "DSP Preset";
    doc["unique_id"] = deviceId + "_dsp_preset";
    doc["state_topic"] = base + "/dsp/preset";
    doc["command_topic"] = base + "/dsp/preset/set";
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
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/select/" + deviceId + "/dsp_preset/config").c_str(), payload.c_str(), true);
  }

  // ===== Per-Channel DSP Entities =====
  {
    const char *chNames[] = {"L1", "R1", "L2", "R2"};
    for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
      String chPrefix = base + "/dsp/channel_" + String(ch);
      String idSuffix = String("_dsp_ch") + String(ch);

      // Per-channel bypass switch
      {
        JsonDocument doc;
        doc["name"] = String("DSP ") + chNames[ch] + " Bypass";
        doc["unique_id"] = deviceId + idSuffix + "_bypass";
        doc["state_topic"] = chPrefix + "/bypass";
        doc["command_topic"] = chPrefix + "/bypass/set";
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["entity_category"] = "config";
        doc["icon"] = "mdi:debug-step-over";
        addHADeviceInfo(doc);
        String payload;
        serializeJson(doc, payload);
        mqttClient.publish(("homeassistant/switch/" + deviceId + "/dsp_ch" + String(ch) + "_bypass/config").c_str(), payload.c_str(), true);
      }

      // Per-channel stage count sensor
      {
        JsonDocument doc;
        doc["name"] = String("DSP ") + chNames[ch] + " Stages";
        doc["unique_id"] = deviceId + idSuffix + "_stages";
        doc["state_topic"] = chPrefix + "/stage_count";
        doc["state_class"] = "measurement";
        doc["entity_category"] = "diagnostic";
        doc["icon"] = "mdi:filter";
        addHADeviceInfo(doc);
        String payload;
        serializeJson(doc, payload);
        mqttClient.publish(("homeassistant/sensor/" + deviceId + "/dsp_ch" + String(ch) + "_stages/config").c_str(), payload.c_str(), true);
      }

      // Per-channel limiter gain reduction sensor
      {
        JsonDocument doc;
        doc["name"] = String("DSP ") + chNames[ch] + " Limiter GR";
        doc["unique_id"] = deviceId + idSuffix + "_limiter_gr";
        doc["state_topic"] = chPrefix + "/limiter_gr";
        doc["unit_of_measurement"] = "dB";
        doc["state_class"] = "measurement";
        doc["entity_category"] = "diagnostic";
        doc["icon"] = "mdi:arrow-collapse-down";
        addHADeviceInfo(doc);
        String payload;
        serializeJson(doc, payload);
        mqttClient.publish(("homeassistant/sensor/" + deviceId + "/dsp_ch" + String(ch) + "_limiter_gr/config").c_str(), payload.c_str(), true);
      }
    }
  }

  // ===== PEQ Bypass Switch =====
  {
    JsonDocument doc;
    doc["name"] = "PEQ Bypass";
    doc["unique_id"] = deviceId + "_peq_bypass";
    doc["state_topic"] = base + "/dsp/peq/bypass";
    doc["command_topic"] = base + "/dsp/peq/bypass/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:equalizer";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/peq_bypass/config").c_str(), payload.c_str(), true);
  }

  // PEQ band switches removed — controlled via DSP API / WebSocket only.
  // Cleanup of orphaned PEQ band entities is handled in removeHADiscovery().
#endif

#ifdef GUI_ENABLED
  // ===== Boot Animation Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Boot Animation";
    doc["unique_id"] = deviceId + "_boot_animation";
    doc["state_topic"] = base + "/settings/boot_animation";
    doc["command_topic"] = base + "/settings/boot_animation/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["entity_category"] = "config";
    doc["icon"] = "mdi:animation-play";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/boot_animation/config").c_str(), payload.c_str(), true);
  }

  // ===== Boot Animation Style Select =====
  {
    JsonDocument doc;
    doc["name"] = "Boot Animation Style";
    doc["unique_id"] = deviceId + "_boot_animation_style";
    doc["state_topic"] = base + "/settings/boot_animation_style";
    doc["command_topic"] = base + "/settings/boot_animation_style/set";
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
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/select/" + deviceId + "/boot_animation_style/config").c_str(), payload.c_str(), true);
  }
#endif

#ifdef DSP_ENABLED
  // ===== Emergency Limiter Enabled Switch =====
  {
    JsonDocument doc;
    doc["name"] = "Emergency Limiter";
    doc["unique_id"] = deviceId + "_emergency_limiter_enabled";
    doc["state_topic"] = base + "/emergency_limiter/enabled";
    doc["command_topic"] = base + "/emergency_limiter/enabled/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:shield-alert";
    doc["entity_category"] = "config";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/switch/" + deviceId + "/emergency_limiter_enabled/config").c_str(), payload.c_str(), true);
  }

  // ===== Emergency Limiter Threshold Number =====
  {
    JsonDocument doc;
    doc["name"] = "Emergency Limiter Threshold";
    doc["unique_id"] = deviceId + "_emergency_limiter_threshold";
    doc["state_topic"] = base + "/emergency_limiter/threshold";
    doc["command_topic"] = base + "/emergency_limiter/threshold/set";
    doc["min"] = -6.0;
    doc["max"] = 0.0;
    doc["step"] = 0.1;
    doc["unit_of_measurement"] = "dBFS";
    doc["icon"] = "mdi:volume-high";
    doc["entity_category"] = "config";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/number/" + deviceId + "/emergency_limiter_threshold/config").c_str(), payload.c_str(), true);
  }

  // ===== Emergency Limiter Status Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Emergency Limiter Status";
    doc["unique_id"] = deviceId + "_emergency_limiter_status";
    doc["state_topic"] = base + "/emergency_limiter/status";
    doc["icon"] = "mdi:shield-check";
    doc["entity_category"] = "diagnostic";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/emergency_limiter_status/config").c_str(), payload.c_str(), true);
  }

  // ===== Emergency Limiter Trigger Count Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Emergency Limiter Triggers";
    doc["unique_id"] = deviceId + "_emergency_limiter_triggers";
    doc["state_topic"] = base + "/emergency_limiter/trigger_count";
    doc["icon"] = "mdi:counter";
    doc["entity_category"] = "diagnostic";
    doc["state_class"] = "total_increasing";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/emergency_limiter_triggers/config").c_str(), payload.c_str(), true);
  }

  // ===== Emergency Limiter Gain Reduction Sensor =====
  {
    JsonDocument doc;
    doc["name"] = "Emergency Limiter Gain Reduction";
    doc["unique_id"] = deviceId + "_emergency_limiter_gr";
    doc["state_topic"] = base + "/emergency_limiter/gain_reduction";
    doc["unit_of_measurement"] = "dB";
    doc["icon"] = "mdi:volume-minus";
    doc["entity_category"] = "diagnostic";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/sensor/" + deviceId + "/emergency_limiter_gr/config").c_str(), payload.c_str(), true);
  }
#endif

  // ===== Custom Device Name (Text Entity) =====
  {
    JsonDocument doc;
    doc["name"] = "Device Name";
    doc["unique_id"] = deviceId + "_device_name";
    doc["state_topic"] = base + "/settings/device_name";
    doc["command_topic"] = base + "/settings/device_name/set";
    doc["icon"] = "mdi:rename";
    doc["entity_category"] = "config";
    doc["max"] = 32;
    doc["min"] = 0;
    doc["mode"] = "text";
    addHADeviceInfo(doc);
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(("homeassistant/text/" + deviceId + "/device_name/config").c_str(), payload.c_str(), true);
    // Publish current value
    mqttClient.publish((base + "/settings/device_name").c_str(), appState.customDeviceName.c_str(), true);
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
      "homeassistant/text/%s/device_name/config"};

  char topicBuf[160];
  for (const char *topicTemplate : topics) {
    snprintf(topicBuf, sizeof(topicBuf), topicTemplate, deviceId.c_str());
    mqttClient.publish(topicBuf, "", true); // Empty payload removes the config
  }

  // Remove orphaned PEQ band switch entities (20 = 2 channels x 10 bands)
  for (int ch = 0; ch < 2; ch++) {
    for (int b = 0; b < DSP_PEQ_BANDS; b++) {
      String topic = "homeassistant/switch/" + deviceId + "/peq_ch" + String(ch) + "_band" + String(b + 1) + "/config";
      mqttClient.publish(topic.c_str(), "", true);
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
  doc["hasPassword"] = (appState.mqttPassword.length() > 0);
  doc["baseTopic"] = appState.mqttBaseTopic;
  doc["effectiveBaseTopic"] = getEffectiveMqttBaseTopic();
  doc["defaultBaseTopic"] = String("ALX/") + appState.deviceSerialNumber;
  doc["haDiscovery"] = appState.mqttHADiscovery;

  // Status
  doc["connected"] = appState.mqttConnected;
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
    if (appState.mqttBroker != newBroker) {
      appState.mqttBroker = newBroker;
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
    if (appState.mqttUsername != newUsername) {
      appState.mqttUsername = newUsername;
      settingsChanged = true;
      needReconnect = true;
    }
  }

  // Update password (empty string keeps existing, like WiFi pattern)
  if (!doc["password"].isNull()) {
    String newPassword = doc["password"].as<String>();
    if (newPassword.length() > 0 && appState.mqttPassword != newPassword) {
      appState.mqttPassword = newPassword;
      settingsChanged = true;
      needReconnect = true;
    }
  }

  // Update base topic (empty string uses default ALX/{serial})
  if (!doc["baseTopic"].isNull()) {
    String newBaseTopic = doc["baseTopic"].as<String>();
    if (appState.mqttBaseTopic != newBaseTopic) {
      // Remove old HA discovery before changing topic
      if (appState.mqttHADiscovery && mqttClient.connected()) {
        removeHADiscovery();
      }
      appState.mqttBaseTopic = newBaseTopic;
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
  if (needReconnect && appState.mqttEnabled && appState.mqttBroker.length() > 0) {
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
