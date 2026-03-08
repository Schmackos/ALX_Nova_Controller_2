// mqtt_publish.cpp — MQTT state publishing functions
// Extracted from mqtt_handler.cpp as part of a 3-file split.
// This file owns all publish functions and their per-category change-detection statics.

#include "mqtt_handler.h"
#include "app_state.h"
#include "config.h"
#include "debug_serial.h"
#include "diag_journal.h"
#include "diag_event.h"
#include "crash_log.h"
#include "task_monitor.h"
#include "utils.h"
#include "websocket_handler.h"
#ifdef DSP_ENABLED
#include "dsp_pipeline.h"
#endif
#ifdef USB_AUDIO_ENABLED
#include "usb_audio.h"
#endif
#include <LittleFS.h>
#include <WiFi.h>
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

// Per-category change-detection statics extracted from AppState.
// These are ONLY used inside mqttPublishPendingState() and its callees in this file.
static bool prevMqttAmplifierState = false;
static unsigned long prevMqttTimerRemaining = 0;
static float prevMqttAudioLevel = -96.0f;
static bool prevMqttBacklightOn = true;
static unsigned long prevMqttScreenTimeout = 60000;
static bool prevMqttBuzzerEnabled = true;
static int prevMqttBuzzerVolume = 1;
static uint8_t prevMqttBrightness = 255;
static bool prevMqttDimEnabled = false;
static unsigned long prevMqttDimTimeout = 10000;
static uint8_t prevMqttDimBrightness = 26;
static bool prevMqttVuMeterEnabled = true;
static bool prevMqttWaveformEnabled = true;
static bool prevMqttSpectrumEnabled = true;
static bool prevMqttDebugMode = true;
static int prevMqttDebugSerialLevel = 2;
static bool prevMqttDebugHwStats = true;
static bool prevMqttDebugI2sMetrics = true;
static bool prevMqttDebugTaskMonitor = true;
static bool prevMqttSigGenEnabled = false;
static int prevMqttSigGenWaveform = 0;
static float prevMqttSigGenFrequency = 1000.0f;
static float prevMqttSigGenAmplitude = -6.0f;
static int prevMqttSigGenOutputMode = 0;
static float prevMqttSigGenSweepSpeed = 1000.0f;
static bool prevMqttDspEnabled = false;
static bool prevMqttDspBypass = false;
static bool prevMqttDspChBypass[DSP_MAX_CHANNELS] = {};
static bool prevMqttDacEnabled = false;
static uint8_t prevMqttDacVolume = 80;
static bool prevMqttDacMute = false;
static uint8_t prevMqttOtaChannel = 0;
static bool prevMqttBootAnimEnabled = true;
static int prevMqttBootAnimStyle = 0;
static SensingMode prevMqttSensingMode = ALWAYS_ON;
static FftWindowType prevMqttFftWindowType = FFT_WINDOW_HANN;
static int8_t prevMqttDspPresetIndex = -1;

// ===== MQTT State Publishing Functions =====

void mqttPublishPendingState() {
  if (!mqttClient.connected()) return;

  unsigned long currentMillis = millis();
  if (currentMillis - appState.lastMqttPublish < MQTT_PUBLISH_INTERVAL) return;
  appState.lastMqttPublish = currentMillis;

  // Per-category change detection
  bool audioLevelChanged =
      (fabs(appState.audioLevel_dBFS - prevMqttAudioLevel) > 0.5f);
  bool sensingChanged =
      (appState.amplifierState != prevMqttAmplifierState) ||
      (appState.currentMode != prevMqttSensingMode) ||
      (appState.timerRemaining != prevMqttTimerRemaining);
  bool displayChanged =
      (appState.backlightOn != prevMqttBacklightOn) ||
      (appState.screenTimeout != prevMqttScreenTimeout) ||
      (appState.backlightBrightness != prevMqttBrightness) ||
      (appState.dimEnabled != prevMqttDimEnabled) ||
      (appState.dimTimeout != prevMqttDimTimeout) ||
      (appState.dimBrightness != prevMqttDimBrightness);
  bool settingsChanged =
      (appState.darkMode != prevMqttDarkMode) ||
      (appState.autoUpdateEnabled != prevMqttAutoUpdate) ||
      (appState.enableCertValidation != prevMqttCertValidation) ||
      (appState.otaChannel != prevMqttOtaChannel);
  bool buzzerChanged =
      (appState.buzzerEnabled != prevMqttBuzzerEnabled) ||
      (appState.buzzerVolume != prevMqttBuzzerVolume);
  bool siggenChanged =
      (appState.sigGenEnabled != prevMqttSigGenEnabled) ||
      (appState.sigGenWaveform != prevMqttSigGenWaveform) ||
      (fabs(appState.sigGenFrequency - prevMqttSigGenFrequency) > 0.5f) ||
      (fabs(appState.sigGenAmplitude - prevMqttSigGenAmplitude) > 0.5f) ||
      (appState.sigGenOutputMode != prevMqttSigGenOutputMode) ||
      (fabs(appState.sigGenSweepSpeed - prevMqttSigGenSweepSpeed) > 0.05f);
  bool audioGraphChanged =
      (appState.vuMeterEnabled != prevMqttVuMeterEnabled) ||
      (appState.waveformEnabled != prevMqttWaveformEnabled) ||
      (appState.spectrumEnabled != prevMqttSpectrumEnabled) ||
      (appState.fftWindowType != prevMqttFftWindowType);
  bool debugChanged =
      (appState.debugMode != prevMqttDebugMode) ||
      (appState.debugSerialLevel != prevMqttDebugSerialLevel) ||
      (appState.debugHwStats != prevMqttDebugHwStats) ||
      (appState.debugI2sMetrics != prevMqttDebugI2sMetrics) ||
      (appState.debugTaskMonitor != prevMqttDebugTaskMonitor);

  // Selective dispatch — only publish categories that actually changed
  // Smart sensing + audio level (combined to avoid double-publishing)
  if (sensingChanged || audioLevelChanged) {
    publishMqttSmartSensingState();
    if (audioLevelChanged) {
      publishMqttAudioDiagnostics();
      prevMqttAudioLevel = appState.audioLevel_dBFS;
    }
    if (sensingChanged) {
      prevMqttAmplifierState = appState.amplifierState;
      prevMqttSensingMode = appState.currentMode;
      prevMqttTimerRemaining = appState.timerRemaining;
    }
  }
  if (displayChanged) {
    publishMqttDisplayState();
    prevMqttBacklightOn = appState.backlightOn;
    prevMqttScreenTimeout = appState.screenTimeout;
    prevMqttBrightness = appState.backlightBrightness;
    prevMqttDimEnabled = appState.dimEnabled;
    prevMqttDimTimeout = appState.dimTimeout;
    prevMqttDimBrightness = appState.dimBrightness;
  }
  if (settingsChanged) {
    publishMqttSystemStatus();
    prevMqttDarkMode = appState.darkMode;
    prevMqttAutoUpdate = appState.autoUpdateEnabled;
    prevMqttCertValidation = appState.enableCertValidation;
    prevMqttOtaChannel = appState.otaChannel;
  }
  if (buzzerChanged) {
    publishMqttBuzzerState();
    prevMqttBuzzerEnabled = appState.buzzerEnabled;
    prevMqttBuzzerVolume = appState.buzzerVolume;
  }
  if (siggenChanged) {
    publishMqttSignalGenState();
    prevMqttSigGenEnabled = appState.sigGenEnabled;
    prevMqttSigGenWaveform = appState.sigGenWaveform;
    prevMqttSigGenFrequency = appState.sigGenFrequency;
    prevMqttSigGenAmplitude = appState.sigGenAmplitude;
    prevMqttSigGenOutputMode = appState.sigGenOutputMode;
    prevMqttSigGenSweepSpeed = appState.sigGenSweepSpeed;
  }
  if (audioGraphChanged) {
    publishMqttAudioGraphState();
    prevMqttVuMeterEnabled = appState.vuMeterEnabled;
    prevMqttWaveformEnabled = appState.waveformEnabled;
    prevMqttSpectrumEnabled = appState.spectrumEnabled;
    prevMqttFftWindowType = appState.fftWindowType;
  }
  if (appState.isAdcEnabledDirty()) {
    publishMqttAdcEnabledState();
    appState.clearAdcEnabledDirty();
  }
#ifdef USB_AUDIO_ENABLED
  if (appState.isUsbAudioDirty()) {
    publishMqttUsbAudioState();
    // Note: clearUsbAudioDirty() is handled by main loop WS handler — don't clear here
  }
#endif
  if (debugChanged) {
    publishMqttDebugState();
    prevMqttDebugMode = appState.debugMode;
    prevMqttDebugSerialLevel = appState.debugSerialLevel;
    prevMqttDebugHwStats = appState.debugHwStats;
    prevMqttDebugI2sMetrics = appState.debugI2sMetrics;
    prevMqttDebugTaskMonitor = appState.debugTaskMonitor;
  }
#ifdef GUI_ENABLED
  if ((appState.bootAnimEnabled != prevMqttBootAnimEnabled) ||
      (appState.bootAnimStyle != prevMqttBootAnimStyle)) {
    publishMqttBootAnimState();
    prevMqttBootAnimEnabled = appState.bootAnimEnabled;
    prevMqttBootAnimStyle = appState.bootAnimStyle;
  }
#endif
#ifdef DSP_ENABLED
  if ((appState.dspEnabled != prevMqttDspEnabled) ||
      (appState.dspBypass != prevMqttDspBypass) ||
      (appState.dspPresetIndex != prevMqttDspPresetIndex)) {
    publishMqttDspState();
    prevMqttDspEnabled = appState.dspEnabled;
    prevMqttDspBypass = appState.dspBypass;
    prevMqttDspPresetIndex = appState.dspPresetIndex;
  }
#endif
}

// Publish 60-second heartbeat — essential status even when nothing changes.
// Called from mqtt_task (or mqttLoop for backward compat).
void mqttPublishHeartbeat() {
  if (!mqttClient.connected()) return;

  static unsigned long lastMqttHeartbeat = 0;
  unsigned long currentMillis = millis();
  if (currentMillis - lastMqttHeartbeat < MQTT_HEARTBEAT_INTERVAL) return;
  lastMqttHeartbeat = currentMillis;

  publishMqttSmartSensingState();
  publishMqttWifiStatus();
  publishMqttCrashDiagnostics();
  publishMqttHardwareStats();
  String base = getEffectiveMqttBaseTopic();
  mqttClient.publish((base + "/system/uptime").c_str(),
                     String(millis() / 1000).c_str(), true);
}

// ===== MQTT State Publishing Functions =====

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
  mqttClient.publish((base + "/settings/ota_channel").c_str(),
                     appState.otaChannel == 0 ? "stable" : "beta", true);
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
}

void publishMqttAudioDiagnostics() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();

  // Per-ADC diagnostics — iterate all active inputs dynamically.
  int adcCount = appState.activeInputCount;
  if (adcCount <= 0) adcCount = appState.numAdcsDetected;
  if (adcCount > AUDIO_PIPELINE_MAX_INPUTS) adcCount = AUDIO_PIPELINE_MAX_INPUTS;
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
    char labelBuf[8];
    snprintf(labelBuf, sizeof(labelBuf), "adc%d", a + 1);
    String prefix = base + "/audio/" + labelBuf;
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

  // Legacy combined topics — worst-case aggregation across all active inputs.
  // C1: worst health status (highest enum value = most severe).
  // C2: worst noise floor (highest dBFS = most noise), highest vrms (loudest input).
  int worstHealth = 0;
  float worstNoise = appState.audioAdc[0].noiseFloorDbfs;
  float maxVrms = appState.audioAdc[0].vrmsCombined;
  {
    int laneCount = appState.activeInputCount;
    if (laneCount <= 0) laneCount = adcCount; // fall back to per-ADC loop bound
    if (laneCount > AUDIO_PIPELINE_MAX_INPUTS) laneCount = AUDIO_PIPELINE_MAX_INPUTS;
    for (int a = 0; a < laneCount; a++) {
      const AppState::AdcState &lane = appState.audioAdc[a];
      if (lane.healthStatus > worstHealth) worstHealth = lane.healthStatus;
      if (lane.noiseFloorDbfs > worstNoise) worstNoise = lane.noiseFloorDbfs;
      if (lane.vrmsCombined > maxVrms) maxVrms = lane.vrmsCombined;
    }
  }
  const char *statusStr = "OK";
  switch (worstHealth) {
    case 1: statusStr = "NO_DATA"; break;
    case 2: statusStr = "NOISE_ONLY"; break;
    case 3: statusStr = "CLIPPING"; break;
    case 4: statusStr = "I2S_ERROR"; break;
    case 5: statusStr = "HW_FAULT"; break;
  }
  mqttClient.publish((base + "/audio/adc_status").c_str(), statusStr, true);
  mqttClient.publish((base + "/audio/noise_floor").c_str(),
                     String(worstNoise, 1).c_str(), true);
  mqttClient.publish((base + "/audio/input_vrms").c_str(),
                     String(maxVrms, 3).c_str(), true);
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

#ifdef USB_AUDIO_ENABLED
// Publish USB audio state
void publishMqttUsbAudioState() {
    if (!mqttClient.connected()) return;
    String base = getEffectiveMqttBaseTopic();

    mqttClient.publish((base + "/" + MQTT_TOPIC_USB_CONNECTED).c_str(),
                       appState.usbAudioConnected ? "true" : "false", true);

    mqttClient.publish((base + "/" + MQTT_TOPIC_USB_STREAMING).c_str(),
                       appState.usbAudioStreaming ? "true" : "false", true);

    mqttClient.publish((base + "/" + MQTT_TOPIC_USB_ENABLED).c_str(),
                       appState.usbAudioEnabled ? "true" : "false", true);

    char val[16];
    snprintf(val, sizeof(val), "%u", (unsigned)appState.usbAudioSampleRate);
    mqttClient.publish((base + "/" + MQTT_TOPIC_USB_RATE).c_str(), val, true);

    snprintf(val, sizeof(val), "%.1f", (float)appState.usbAudioVolume / 256.0f);
    mqttClient.publish((base + "/" + MQTT_TOPIC_USB_VOLUME).c_str(), val, true);

    snprintf(val, sizeof(val), "%u", (unsigned)usb_audio_get_overruns());
    mqttClient.publish((base + "/" + MQTT_TOPIC_USB_OVERRUNS).c_str(), val, true);

    snprintf(val, sizeof(val), "%u", (unsigned)usb_audio_get_underruns());
    mqttClient.publish((base + "/" + MQTT_TOPIC_USB_UNDERRUNS).c_str(), val, true);
}
#endif

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

// ===== Diagnostic Event Publishing =====
void publishMqttDiagEvent() {
  if (!mqttClient.connected()) return;

  DiagEvent ev;
  if (!diag_journal_latest(&ev)) return;

  // Severity filtering: skip events below threshold
  if (ev.severity < appState.mqttErrorThreshold) return;

  String base = getEffectiveMqttBaseTopic();

  JsonDocument doc;
  doc["seq"]   = ev.seq;
  doc["boot"]  = ev.bootId;
  doc["t"]     = ev.timestamp;
  doc["heap"]  = ev.heapFree;

  char codeBuf[8];
  snprintf(codeBuf, sizeof(codeBuf), "0x%04X", ev.code);
  doc["c"]     = codeBuf;

  doc["corr"]  = ev.corrId;
  doc["sub"]   = diag_subsystem_name(diag_subsystem_from_code((DiagErrorCode)ev.code));
  doc["dev"]   = ev.device;
  doc["slot"]  = ev.slot;
  doc["msg"]   = ev.message;
  doc["sev"]   = diag_severity_char((DiagSeverity)ev.severity);
  doc["retry"] = ev.retryCount;

  String json;
  serializeJson(doc, json);
  mqttClient.publish((base + "/diagnostics/errors").c_str(), json.c_str());
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

  // Per-ADC I2S recovery counts — iterate all active inputs dynamically (C3).
  {
    int adcCount = appState.activeInputCount;
    if (adcCount <= 0) adcCount = appState.numAdcsDetected;
    if (adcCount > AUDIO_PIPELINE_MAX_INPUTS) adcCount = AUDIO_PIPELINE_MAX_INPUTS;
    for (int a = 0; a < adcCount; a++) {
      char topic[80];
      // Topic uses 1-based index to match legacy adc1/adc2 naming convention.
      snprintf(topic, sizeof(topic), "%s/diagnostics/i2s_recoveries_adc%d",
               base.c_str(), a + 1);
      mqttClient.publish(topic, String(appState.audioAdc[a].i2sRecoveries).c_str(), true);
    }
  }
}

// Publish input names as read-only sensors
void publishMqttInputNames() {
  if (!mqttClient.connected())
    return;

  String base = getEffectiveMqttBaseTopic();

  const char *labels[] = {
      "input1_name_l", "input1_name_r", "input2_name_l", "input2_name_r",
      "input3_name_l", "input3_name_r", "input4_name_l", "input4_name_r",
      "input5_name_l", "input5_name_r", "input6_name_l", "input6_name_r",
      "input7_name_l", "input7_name_r", "input8_name_l", "input8_name_r"};
  for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS * 2; i++) {
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
#ifdef USB_AUDIO_ENABLED
  publishMqttUsbAudioState();
#endif
#ifdef DSP_ENABLED
  publishMqttDspState();
#endif
#ifdef GUI_ENABLED
  publishMqttBootAnimState();
#endif
}
