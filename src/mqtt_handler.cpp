#include "mqtt_handler.h"
#include "app_state.h"
#include "buzzer_handler.h"
#include "config.h"
#include "debug_serial.h"
#include "signal_generator.h"
#include "smart_sensing.h"
#include "settings_manager.h"
#include "utils.h"
#include "websocket_handler.h"
#include "wifi_manager.h"
#include "ota_updater.h"
#ifdef DSP_ENABLED
#include "dsp_pipeline.h"
#endif
#ifdef USB_AUDIO_ENABLED
#include "usb_audio.h"
#endif
#include <LittleFS.h>

// External functions from other modules
extern void saveSettings();
extern void saveSettingsDeferred();
extern void performFactoryReset();
extern void checkForFirmwareUpdate();
extern void setAmplifierState(bool state);
#ifdef DSP_ENABLED
extern void saveDspSettingsDebounced();
#endif

// ===== MQTT Settings Functions =====

// Load MQTT settings from LittleFS
bool loadMqttSettings() {
  // Skip legacy file when /config.json already provided MQTT settings.
  if (settingsMqttLoadedFromJson()) {
    LOG_I("[MQTT] MQTT settings already loaded from config.json, skipping mqtt_config.txt");
    return true;
  }

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
  mqttClient.subscribe((base + "/smartsensing/mode/set").c_str());
  mqttClient.subscribe((base + "/smartsensing/amplifier/set").c_str());
  mqttClient.subscribe((base + "/smartsensing/timer_duration/set").c_str());
  mqttClient.subscribe((base + "/smartsensing/audio_threshold/set").c_str());
  mqttClient.subscribe((base + "/ap/enabled/set").c_str());
  mqttClient.subscribe((base + "/settings/auto_update/set").c_str());
  mqttClient.subscribe((base + "/settings/ota_channel/set").c_str());
  mqttClient.subscribe((base + "/settings/dark_mode/set").c_str());
  mqttClient.subscribe((base + "/settings/cert_validation/set").c_str());
  mqttClient.subscribe((base + "/settings/screen_timeout/set").c_str());
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

#ifdef USB_AUDIO_ENABLED
  mqttClient.subscribe((base + "/" + MQTT_TOPIC_USB_ENABLE_SET).c_str());
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

  // Handle Smart Sensing mode
  if (topicStr == base + "/smartsensing/mode/set") {
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

      saveSmartSensingSettingsDeferred();
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

      saveSmartSensingSettingsDeferred();
      LOG_I("[MQTT] Timer duration set to %d minutes", duration);
    }
    publishMqttSmartSensingState();
  }
  // Handle audio threshold
  else if (topicStr == base + "/smartsensing/audio_threshold/set") {
    float threshold = message.toFloat();
    if (threshold >= -96.0 && threshold <= 0.0) {
      appState.audioThreshold_dBFS = threshold;
      saveSmartSensingSettingsDeferred();
      LOG_I("[MQTT] Audio threshold set to %+.0f dBFS", threshold);
    }
    publishMqttSmartSensingState();
  }
  // Handle AP toggle
  else if (topicStr == base + "/ap/enabled/set") {
    bool enabled = (message == "ON" || message == "1" || message == "true");
    appState.apEnabled = enabled;
    // WiFi mode changes are unsafe from mqtt_task — defer to main loop
    appState._pendingApToggle = enabled ? 1 : -1;
    LOG_I("[MQTT] AP toggle requested: %s (deferred to main loop)", enabled ? "enable" : "disable");
    appState.markSettingsDirty();
    publishMqttWifiStatus();
  }
  // Handle auto-update setting
  else if (topicStr == base + "/settings/auto_update/set") {
    bool enabled = (message == "ON" || message == "1" || message == "true");
    if (appState.autoUpdateEnabled != enabled) {
      appState.autoUpdateEnabled = enabled;
      saveSettingsDeferred();
      LOG_I("[MQTT] Auto-update set to %s", enabled ? "ON" : "OFF");
      appState.markSettingsDirty();
    }
    publishMqttSystemStatus();
  }
  // Handle OTA release channel setting
  else if (topicStr == base + "/settings/ota_channel/set") {
    uint8_t newCh = 0;
    if (message == "beta" || message == "1") newCh = 1;
    if (newCh != appState.otaChannel) {
      appState.otaChannel = newCh;
      saveSettingsDeferred();
      LOG_I("[MQTT] OTA channel set to %s", newCh == 0 ? "stable" : "beta");
      appState.markSettingsDirty();
      appState.markOTADirty();
    }
    publishMqttSystemStatus();
  }
  // Handle night mode setting
  else if (topicStr == base + "/settings/dark_mode/set") {
    bool enabled = (message == "ON" || message == "1" || message == "true");
    if (appState.darkMode != enabled) {
      appState.darkMode = enabled;
      saveSettingsDeferred();
      LOG_I("[MQTT] Dark mode set to %s", enabled ? "ON" : "OFF");
      appState.markSettingsDirty();
    }
    publishMqttSystemStatus();
  }
  // Handle certificate validation setting
  else if (topicStr == base + "/settings/cert_validation/set") {
    bool enabled = (message == "ON" || message == "1" || message == "true");
    if (appState.enableCertValidation != enabled) {
      appState.enableCertValidation = enabled;
      saveSettingsDeferred();
      LOG_I("[MQTT] Certificate validation set to %s", enabled ? "ON" : "OFF");
      appState.markSettingsDirty();
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
      saveSettingsDeferred();
      LOG_I("[MQTT] Screen timeout set to %d seconds", timeoutSec);
      appState.markSettingsDirty();
    }
    publishMqttDisplayState();
  }
  // Handle dim enabled control
  else if (topicStr == base + "/display/dim_enabled/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.setDimEnabled(newState);
    saveSettingsDeferred();
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
      saveSettingsDeferred();
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
    saveSettingsDeferred();
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
      saveSettingsDeferred();
      LOG_I("[MQTT] Brightness set to %d%% (PWM %d)", bright, pwm);
      publishMqttDisplayState();
    }
  }
  // Handle buzzer enable/disable
  else if (topicStr == base + "/settings/buzzer/set") {
    bool enabled = (message == "ON" || message == "1" || message == "true");
    appState.setBuzzerEnabled(enabled);
    saveSettingsDeferred();
    LOG_I("[MQTT] Buzzer set to %s", enabled ? "ON" : "OFF");
    publishMqttBuzzerState();
  }
  // Handle buzzer volume
  else if (topicStr == base + "/settings/buzzer_volume/set") {
    int vol = message.toInt();
    if (vol >= 0 && vol <= 2) {
      appState.setBuzzerVolume(vol);
      saveSettingsDeferred();
      LOG_I("[MQTT] Buzzer volume set to %d", vol);
      publishMqttBuzzerState();
    }
  }
  // Handle audio update rate
  else if (topicStr == base + "/settings/audio_update_rate/set") {
    int rate = message.toInt();
    if (rate == 20 || rate == 33 || rate == 50 || rate == 100) {
      appState.audioUpdateRate = (uint16_t)rate;
      saveSettingsDeferred();
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
    appState.markSignalGenDirty();
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
      saveSignalGenSettingsDeferred();
      LOG_I("[MQTT] Signal generator waveform set to %s", message.c_str());
      publishMqttSignalGenState();
      appState.markSignalGenDirty();
    }
  }
  // Handle signal generator frequency
  else if (topicStr == base + "/signalgenerator/frequency/set") {
    float freq = message.toFloat();
    if (freq >= 1.0f && freq <= 22000.0f) {
      appState.sigGenFrequency = freq;
      siggen_apply_params();
      saveSignalGenSettingsDeferred();
      LOG_I("[MQTT] Signal generator frequency set to %.0f Hz", freq);
      publishMqttSignalGenState();
      appState.markSignalGenDirty();
    }
  }
  // Handle signal generator amplitude
  else if (topicStr == base + "/signalgenerator/amplitude/set") {
    float amp = message.toFloat();
    if (amp >= -96.0f && amp <= 0.0f) {
      appState.sigGenAmplitude = amp;
      siggen_apply_params();
      saveSignalGenSettingsDeferred();
      LOG_I("[MQTT] Signal generator amplitude set to %.0f dBFS", amp);
      publishMqttSignalGenState();
      appState.markSignalGenDirty();
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
      saveSignalGenSettingsDeferred();
      LOG_I("[MQTT] Signal generator channel set to %s", message.c_str());
      publishMqttSignalGenState();
      appState.markSignalGenDirty();
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
      saveSignalGenSettingsDeferred();
      LOG_I("[MQTT] Signal generator output mode set to %s", message.c_str());
      publishMqttSignalGenState();
      appState.markSignalGenDirty();
    }
  }
  // Handle ADC reference voltage
  else if (topicStr == base + "/settings/adc_vref/set") {
    float vref = message.toFloat();
    if (vref >= 1.0f && vref <= 5.0f) {
      appState.adcVref = vref;
      saveSmartSensingSettingsDeferred();
      LOG_I("[MQTT] ADC VREF set to %.2f V", vref);
      publishMqttAudioDiagnostics();
    }
  }
  // Handle per-ADC enable/disable
  else if (topicStr == base + "/audio/input1/enabled/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.adcEnabled[0] = newState;
    saveSettingsDeferred();
    appState.markAdcEnabledDirty();
    LOG_I("[MQTT] ADC1 set to %s", newState ? "ON" : "OFF");
  }
  else if (topicStr == base + "/audio/input2/enabled/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.adcEnabled[1] = newState;
    saveSettingsDeferred();
    appState.markAdcEnabledDirty();
    LOG_I("[MQTT] ADC2 set to %s", newState ? "ON" : "OFF");
  }
  // Handle VU meter toggle
  else if (topicStr == base + "/audio/vu_meter/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.vuMeterEnabled = newState;
    saveSettingsDeferred();
    appState.markSettingsDirty();
    publishMqttAudioGraphState();
    LOG_I("[MQTT] VU meter set to %s", newState ? "ON" : "OFF");
  }
  // Handle waveform toggle
  else if (topicStr == base + "/audio/waveform/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.waveformEnabled = newState;
    saveSettingsDeferred();
    appState.markSettingsDirty();
    publishMqttAudioGraphState();
    LOG_I("[MQTT] Waveform set to %s", newState ? "ON" : "OFF");
  }
  // Handle spectrum toggle
  else if (topicStr == base + "/audio/spectrum/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.spectrumEnabled = newState;
    saveSettingsDeferred();
    appState.markSettingsDirty();
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
    saveSettingsDeferred();
    appState.markSettingsDirty();
    publishMqttAudioGraphState();
    LOG_I("[MQTT] FFT window set to %d", (int)wt);
  }
  // Handle debug mode toggle
  else if (topicStr == base + "/debug/mode/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.debugMode = newState;
    applyDebugSerialLevel(appState.debugMode, appState.debugSerialLevel);
    saveSettingsDeferred();
    appState.markSettingsDirty();
    publishMqttDebugState();
    LOG_I("[MQTT] Debug mode set to %s", newState ? "ON" : "OFF");
  }
  // Handle debug serial level
  else if (topicStr == base + "/debug/serial_level/set") {
    int level = message.toInt();
    if (level >= 0 && level <= 3) {
      appState.debugSerialLevel = level;
      applyDebugSerialLevel(appState.debugMode, appState.debugSerialLevel);
      saveSettingsDeferred();
      appState.markSettingsDirty();
      publishMqttDebugState();
      LOG_I("[MQTT] Debug serial level set to %d", level);
    }
  }
  // Handle debug HW stats toggle
  else if (topicStr == base + "/debug/hw_stats/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.debugHwStats = newState;
    saveSettingsDeferred();
    appState.markSettingsDirty();
    publishMqttDebugState();
    LOG_I("[MQTT] Debug HW stats set to %s", newState ? "ON" : "OFF");
  }
  // Handle debug I2S metrics toggle
  else if (topicStr == base + "/debug/i2s_metrics/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.debugI2sMetrics = newState;
    saveSettingsDeferred();
    appState.markSettingsDirty();
    publishMqttDebugState();
    LOG_I("[MQTT] Debug I2S metrics set to %s", newState ? "ON" : "OFF");
  }
  // Handle debug task monitor toggle
  else if (topicStr == base + "/debug/task_monitor/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.debugTaskMonitor = newState;
    saveSettingsDeferred();
    appState.markSettingsDirty();
    publishMqttDebugState();
    LOG_I("[MQTT] Debug task monitor set to %s", newState ? "ON" : "OFF");
  }
  // Handle timezone offset
  else if (topicStr == base + "/settings/timezone_offset/set") {
    int offset = message.toInt();
    if (offset >= -12 && offset <= 14) {
      appState.timezoneOffset = offset;
      saveSettingsDeferred();
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
      saveSignalGenSettingsDeferred();
      LOG_I("[MQTT] Signal generator sweep speed set to %.1f Hz/s", speed);
      publishMqttSignalGenState();
      appState.markSignalGenDirty();
    }
  }
#ifdef GUI_ENABLED
  // Handle boot animation enabled
  else if (topicStr == base + "/settings/boot_animation/set") {
    bool newState = (message == "ON" || message == "1" || message == "true");
    appState.bootAnimEnabled = newState;
    saveSettingsDeferred();
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
      saveSettingsDeferred();
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
    if (!dsp_swap_config()) { dsp_log_swap_failure("MQTT"); }
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
        if (!dsp_swap_config()) { dsp_log_swap_failure("MQTT"); }
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
        if (!dsp_swap_config()) { dsp_log_swap_failure("MQTT"); }
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
    if (!dsp_swap_config()) { dsp_log_swap_failure("MQTT"); }
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
#ifdef USB_AUDIO_ENABLED
  // Handle USB audio enable/disable
  else if (topicStr == base + "/" + MQTT_TOPIC_USB_ENABLE_SET) {
    bool newVal = (message == "true" || message == "1" || message == "ON");
    AppState::getInstance().usbAudioEnabled = newVal;
    AppState::getInstance().pipelineInputBypass[3] = !newVal;
    AppState::getInstance().markUsbAudioDirty();
    LOG_I("[MQTT] USB audio set to %s", newVal ? "enabled" : "disabled");
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

  mqttPublishPendingState();
  mqttPublishHeartbeat();
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

  // Reconnect if needed — signal mqtt_task to handle reconnection
  if (needReconnect && appState.mqttEnabled && appState.mqttBroker.length() > 0) {
    appState.mqttConnected = false;
    appState.lastMqttReconnect = 0; // Force immediate reconnect attempt
    appState._mqttReconfigPending = true;
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
