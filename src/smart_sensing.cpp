#include "smart_sensing.h"
#include "app_state.h"
#include "config.h"
#include "debug_serial.h"
#include "i2s_audio.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <cmath>

// Smoothed audio level for stable signal detection (EMA, α=0.15, τ≈308ms)
static float _smoothedAudioLevel = -96.0f;

static const char *audioHealthName(uint8_t status) {
    switch (status) {
    case AUDIO_OK:         return "OK";
    case AUDIO_NO_DATA:    return "NO_DATA";
    case AUDIO_NOISE_ONLY: return "NOISE_ONLY";
    case AUDIO_CLIPPING:   return "CLIPPING";
    case AUDIO_I2S_ERROR:  return "I2S_ERROR";
    case AUDIO_HW_FAULT:   return "HW_FAULT";
    default:               return "UNKNOWN";
    }
}

// ===== Smart Sensing HTTP API Handlers =====

void handleSmartSensingGet() {
  JsonDocument doc;
  doc["success"] = true;

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
  doc["mode"] = modeStr;

  doc["timerDuration"] = appState.timerDuration;
  doc["timerRemaining"] = appState.timerRemaining;
  doc["timerActive"] = (appState.timerRemaining > 0);
  doc["amplifierState"] = appState.amplifierState;
  doc["audioThreshold"] = appState.audioThreshold_dBFS;
  doc["audioLevel"] = appState.audioLevel_dBFS;
  doc["signalDetected"] = (_smoothedAudioLevel >= appState.audioThreshold_dBFS);
  doc["audioSampleRate"] = appState.audioSampleRate;
  doc["adcVref"] = appState.adcVref;
  doc["numAdcsDetected"] = appState.numAdcsDetected;
  // Per-ADC data
  JsonArray adcArr = doc["adc"].to<JsonArray>();
  for (int a = 0; a < NUM_AUDIO_INPUTS; a++) {
    JsonObject adcObj = adcArr.add<JsonObject>();
    const AppState::AdcState &adc = appState.audioAdc[a];
    adcObj["rms1"] = adc.rms1;
    adcObj["rms2"] = adc.rms2;
    adcObj["vu1"] = adc.vu1;
    adcObj["vu2"] = adc.vu2;
    adcObj["peak1"] = adc.peak1;
    adcObj["peak2"] = adc.peak2;
    adcObj["vrms1"] = adc.vrms1;
    adcObj["vrms2"] = adc.vrms2;
    adcObj["vrms"] = adc.vrmsCombined;
    adcObj["dBFS"] = adc.dBFS;
  }
  // Legacy flat fields (ADC 0)
  doc["audioRms1"] = appState.audioRmsLeft;
  doc["audioRms2"] = appState.audioRmsRight;
  doc["audioVu1"] = appState.audioVuLeft;
  doc["audioVu2"] = appState.audioVuRight;
  doc["audioPeak1"] = appState.audioPeakLeft;
  doc["audioPeak2"] = appState.audioPeakRight;
  doc["audioPeak"] = appState.audioPeakCombined;
  doc["audioVrms1"] = appState.audioVrms1;
  doc["audioVrms2"] = appState.audioVrms2;
  doc["audioVrms"] = appState.audioVrmsCombined;

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleSmartSensingUpdate() {
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

  // Update mode
  if (doc["mode"].is<String>()) {
    String modeStr = doc["mode"].as<String>();
    SensingMode newMode;

    if (modeStr == "always_on") {
      newMode = ALWAYS_ON;
    } else if (modeStr == "always_off") {
      newMode = ALWAYS_OFF;
    } else if (modeStr == "smart_auto") {
      newMode = SMART_AUTO;
    } else {
      server.send(400, "application/json",
                  "{\"success\": false, \"message\": \"Invalid mode\"}");
      return;
    }

    if (appState.currentMode != newMode) {
      appState.currentMode = newMode;
      settingsChanged = true;
      LOG_I("[Sensing] Mode changed to: %s", modeStr.c_str());

      // When switching to SMART_AUTO mode, immediately evaluate signal state
      if (appState.currentMode == SMART_AUTO) {
        bool signalDetected = detectSignal();
        _smoothedAudioLevel = appState.audioLevel_dBFS; // Initialize to current level

        if (signalDetected) {
          // Signal is above threshold - turn ON and set timer to full value
          appState.timerRemaining = appState.timerDuration * 60;
          appState.lastTimerUpdate = millis();
          appState.lastSignalDetection = millis();
          setAmplifierState(true);
          appState.previousSignalState = true;
          LOG_I("[Sensing] Smart Auto activated: signal detected, amp ON");
        } else {
          // Signal is below threshold - turn OFF
          appState.timerRemaining = 0;
          setAmplifierState(false);
          appState.previousSignalState = false;
          LOG_I("[Sensing] Smart Auto activated: no signal, amp OFF");
        }
      }
    }
  }

  // Update timer duration
  if (doc["appState.timerDuration"].is<int>()) {
    int duration = doc["appState.timerDuration"].as<int>();

    if (duration >= 1 && duration <= 60) {
      appState.timerDuration = duration;
      settingsChanged = true;

      // Update timer remaining in SMART_AUTO mode
      if (appState.currentMode == SMART_AUTO) {
        // Always update appState.timerRemaining to show the new duration
        appState.timerRemaining = appState.timerDuration * 60;

        if (appState.amplifierState) {
          // Amplifier is ON - update timer to new duration
          appState.lastTimerUpdate = millis();
          LOG_I("[Sensing] Timer duration changed to %d min (timer updated)", duration);
        } else {
          // Amplifier is OFF - just display new duration, countdown won't start
          // until signal disappears
          LOG_I("[Sensing] Timer duration changed to %d min (countdown starts when signal disappears)", duration);
        }
      }

      LOG_I("[Sensing] Timer duration set to %d min", duration);
    } else {
      server.send(400, "application/json",
                  "{\"success\": false, \"message\": \"Timer duration must be "
                  "between 1 and 60 minutes\"}");
      return;
    }
  }

  // Update audio threshold
  if (doc["audioThreshold"].is<float>() || doc["audioThreshold"].is<int>()) {
    float threshold = doc["audioThreshold"].as<float>();
    if (threshold >= -96.0f && threshold <= 0.0f) {
      appState.audioThreshold_dBFS = threshold;
      settingsChanged = true;
      LOG_I("[Sensing] Audio threshold set to %+.0f dBFS", threshold);
    } else {
      server.send(400, "application/json",
                  "{\"success\": false, \"message\": \"Audio threshold must "
                  "be between -96 and 0 dBFS\"}");
      return;
    }
  }

  // Update ADC reference voltage
  if (doc["adcVref"].is<float>() || doc["adcVref"].is<int>()) {
    float vref = doc["adcVref"].as<float>();
    if (vref >= 1.0f && vref <= 5.0f) {
      appState.adcVref = vref;
      settingsChanged = true;
      LOG_I("[Sensing] ADC VREF set to %.2f V", vref);
    }
  }

  // Update sample rate
  if (doc["audioSampleRate"].is<int>()) {
    uint32_t rate = doc["audioSampleRate"].as<uint32_t>();
    if (audio_validate_sample_rate(rate)) {
      appState.audioSampleRate = rate;
      i2s_audio_set_sample_rate(rate);
      settingsChanged = true;
      LOG_I("[Sensing] Sample rate set to %lu Hz", rate);
    }
  }

  // Manual override
  if (doc["manualOverride"].is<bool>()) {
    bool state = doc["manualOverride"].as<bool>();
    setAmplifierState(state);
    LOG_I("[Sensing] Manual override: amplifier %s", state ? "ON" : "OFF");

    if (appState.currentMode == SMART_AUTO) {
      if (state) {
        // If turning on manually in SMART_AUTO mode, set timer to full value
        appState.timerRemaining = appState.timerDuration * 60;
        appState.lastTimerUpdate = millis();
        LOG_D("[Sensing] Manual ON: timer set to full value");
      } else {
        // If turning off manually in SMART_AUTO mode, reset timer to 0
        appState.timerRemaining = 0;
        LOG_D("[Sensing] Manual OFF: timer reset to 0");
      }
    }
  }

  // Save settings if changed
  if (settingsChanged) {
    saveSmartSensingSettings();
  }

  // Broadcast updated state immediately (force broadcast)
  sendSmartSensingStateInternal();

  JsonDocument resp;
  resp["success"] = true;
  String json;
  serializeJson(resp, json);
  server.send(200, "application/json", json);
}

// ===== Smart Sensing Core Functions =====

bool detectSignal() {
  AudioAnalysis analysis = i2s_audio_get_analysis();
  AudioDiagnostics diag = i2s_audio_get_diagnostics();

  // Copy per-input analysis and diagnostics into AppState
  for (int a = 0; a < NUM_AUDIO_INPUTS; a++) {
    AppState::AdcState &dst = appState.audioAdc[a];
    const AdcAnalysis &src = analysis.adc[a];
    dst.rms1 = src.rms1;
    dst.rms2 = src.rms2;
    dst.rmsCombined = src.rmsCombined;
    if (appState.vuMeterEnabled) {
      dst.vu1 = src.vu1;
      dst.vu2 = src.vu2;
      dst.vuCombined = src.vuCombined;
      dst.peak1 = src.peak1;
      dst.peak2 = src.peak2;
      dst.peakCombined = src.peakCombined;
    }
    dst.vrms1 = audio_rms_to_vrms(src.rms1, appState.adcVref);
    dst.vrms2 = audio_rms_to_vrms(src.rms2, appState.adcVref);
    dst.vrmsCombined = audio_rms_to_vrms(src.rmsCombined, appState.adcVref);
    dst.dBFS = src.dBFS;

    // Diagnostics
    const AdcDiagnostics &dsrc = diag.adc[a];
    dst.healthStatus = (uint8_t)dsrc.status;
    // Debounced health transition logging: only log after new state is stable for 3 seconds.
    // Prevents 33K+ log lines/session from ADC2 oscillating at the CLIPPING boundary.
    static uint8_t prevHealth[NUM_AUDIO_INPUTS] = {0xFF, 0xFF, 0xFF};
    static uint8_t pendingHealth[NUM_AUDIO_INPUTS] = {0xFF, 0xFF, 0xFF};
    static unsigned long pendingSince[NUM_AUDIO_INPUTS] = {0, 0, 0};
    if (dst.healthStatus != prevHealth[a]) {
        if (dst.healthStatus != pendingHealth[a]) {
            // New candidate state — start hold timer
            pendingHealth[a] = dst.healthStatus;
            pendingSince[a] = millis();
        } else if (millis() - pendingSince[a] >= 3000) {
            // Candidate held stable for 3s — commit the transition
            LOG_I("[Sensing] %s health: %s -> %s", audioInputLabel(a),
                  audioHealthName(prevHealth[a]), audioHealthName(dst.healthStatus));
            prevHealth[a] = dst.healthStatus;
        }
    } else {
        // Status returned to current — cancel any pending transition
        pendingHealth[a] = dst.healthStatus;
    }
    dst.i2sErrors = dsrc.i2sReadErrors;
    dst.allZeroBuffers = dsrc.allZeroBuffers;
    dst.consecutiveZeros = dsrc.consecutiveZeros;
    dst.noiseFloorDbfs = dsrc.noiseFloorDbfs;
    dst.lastNonZeroMs = dsrc.lastNonZeroMs;
    dst.totalBuffers = dsrc.totalBuffersRead;
    dst.clippedSamples = dsrc.clippedSamples;
    dst.clipRate = dsrc.clipRate;
    dst.dcOffset = dsrc.dcOffset;
  }

  // Overall level = max dBFS across all ADCs
  appState.audioLevel_dBFS = analysis.dBFS;
  appState.numAdcsDetected = diag.numAdcsDetected;
  appState.numInputsDetected = diag.numInputsDetected;

  // Copy ADC clock sync diagnostics into AppState for WS/MQTT broadcast
  AdcSyncDiag sync = i2s_audio_get_sync_diag();
  appState.adcSyncOffsetSamples = sync.phaseOffsetSamples;
  appState.adcSyncCorrelation   = sync.correlationPeak;
  appState.adcSyncOk            = sync.inSync;

  return analysis.signalDetected;
}

// Set amplifier state and update pin
void setAmplifierState(bool state) {
  if (appState.amplifierState != state) {
    appState.amplifierState = state;
    digitalWrite(AMPLIFIER_PIN, state ? HIGH : LOW);
    LOG_I("[Sensing] Amplifier %s", state ? "ON" : "OFF");
  }
}

// Main Smart Sensing logic - called from loop()
void updateSmartSensingLogic() {
  unsigned long currentMillis = millis();

  // Rate limit signal reading to reduce CPU usage (every 50ms is sufficient)
  static unsigned long lastSignalRead = 0;

  // Match rate to audioUpdateRate so VU/peak data is fresh every WS send
  const uint16_t detectInterval = appState.audioUpdateRate;
  if (currentMillis - lastSignalRead >= detectInterval) {
    lastSignalRead = currentMillis;
    // Read audio level for real-time display, regardless of mode
    detectSignal();
    // Smoothing alpha scaled to maintain ~308ms time constant regardless of interval
    // α = 1 - exp(-interval / 308) — precomputed for common rates
    float alpha;
    switch (detectInterval) {
      case 20:  alpha = 0.063f; break;
      case 33:  alpha = 0.102f; break;
      case 50:  alpha = 0.15f;  break;
      case 100: alpha = 0.278f; break;
      default:  alpha = 1.0f - expf(-(float)detectInterval / 308.0f); break;
    }
    _smoothedAudioLevel += (appState.audioLevel_dBFS - _smoothedAudioLevel) * alpha;
  }

  // Use smoothed level for signal presence decision
  bool signalPresent = (_smoothedAudioLevel >= appState.audioThreshold_dBFS);

  switch (appState.currentMode) {
  case ALWAYS_ON:
    // Always keep amplifier ON, timer disabled
    setAmplifierState(true);
    appState.timerRemaining = 0;
    appState.previousSignalState = signalPresent; // Update state tracking
    break;

  case ALWAYS_OFF:
    // Always keep amplifier OFF, timer disabled
    setAmplifierState(false);
    appState.timerRemaining = 0;
    appState.previousSignalState = signalPresent; // Update state tracking
    break;

  case SMART_AUTO: {
    if (signalPresent) {
      // Signal is currently detected - keep timer at full value and amplifier
      // ON
      appState.timerRemaining = appState.timerDuration * 60; // Keep timer at full value
      appState.lastSignalDetection = currentMillis;
      appState.lastTimerUpdate = currentMillis;

      // Ensure amplifier is ON
      if (!appState.amplifierState) {
        setAmplifierState(true);
        LOG_D("[Sensing] Signal detected, amp ON, timer reset");
      }
    } else {
      // No signal detected - countdown timer if amplifier is ON
      if (appState.amplifierState && appState.timerRemaining > 0) {
        if (currentMillis - appState.lastTimerUpdate >= 1000) {
          appState.lastTimerUpdate = currentMillis;
          appState.timerRemaining--;

          if (appState.timerRemaining == 0) {
            setAmplifierState(false);
            LOG_I("[Sensing] Timer expired, amplifier OFF");
          }
        }
      }
    }

    // Update previous signal state for next iteration
    appState.previousSignalState = signalPresent;
    break;
  }
  }
}

// ===== Smart Sensing State Broadcasting =====

// Send Smart Sensing state via WebSocket (internal function, always sends)
void sendSmartSensingStateInternal() {
  JsonDocument doc;
  doc["type"] = "smartSensing";

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
  doc["mode"] = modeStr;

  doc["timerDuration"] = appState.timerDuration;
  doc["timerRemaining"] = appState.timerRemaining;
  doc["timerActive"] = (appState.timerRemaining > 0);
  doc["amplifierState"] = appState.amplifierState;
  doc["audioThreshold"] = appState.audioThreshold_dBFS;
  doc["audioLevel"] = appState.audioLevel_dBFS;
  doc["signalDetected"] = (_smoothedAudioLevel >= appState.audioThreshold_dBFS);
  doc["audioSampleRate"] = appState.audioSampleRate;
  doc["audioVrms"] = appState.audioVrmsCombined;

  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t *)json.c_str(), json.length());

  // Update tracked state after broadcast
  appState.prevBroadcastMode = appState.currentMode;
  appState.prevBroadcastAmplifierState = appState.amplifierState;
  appState.prevBroadcastTimerRemaining = appState.timerRemaining;
  appState.lastSmartSensingHeartbeat = millis();
}

// Send Smart Sensing state - only broadcasts if state changed or heartbeat
// interval elapsed
void sendSmartSensingState() {
  unsigned long currentMillis = millis();

  // Check if any state has changed
  bool stateChanged = (appState.currentMode != appState.prevBroadcastMode) ||
                      (appState.amplifierState != appState.prevBroadcastAmplifierState) ||
                      (appState.timerRemaining != appState.prevBroadcastTimerRemaining);

  // Check if heartbeat interval has elapsed (1s, matches main loop call rate)
  bool heartbeatDue = (currentMillis - appState.lastSmartSensingHeartbeat >= 1000);

  // Only broadcast if state changed or heartbeat is due
  if (stateChanged || heartbeatDue) {
    sendSmartSensingStateInternal();
  }
}

// ===== Smart Sensing Settings =====

// Load Smart Sensing settings from LittleFS
bool loadSmartSensingSettings() {
  // Use create=true to avoid "no permits for creation" error log if file is
  // missing
  File file = LittleFS.open("/smartsensing.txt", "r", true);
  if (!file || file.size() == 0) {
    if (file)
      file.close();
    return false;
  }

  String line1 = file.readStringUntil('\n'); // mode
  String line2 = file.readStringUntil('\n'); // timer duration
  String line3 = file.readStringUntil('\n'); // audio threshold
  String line4 = file.readStringUntil('\n'); // sample rate
  String line5 = file.readStringUntil('\n'); // ADC VREF
  file.close();

  line1.trim();
  line2.trim();
  line3.trim();
  line4.trim();
  line5.trim();

  if (line1.length() > 0) {
    int mode = line1.toInt();
    if (mode >= 0 && mode <= 2) {
      appState.currentMode = static_cast<SensingMode>(mode);
    }
  }

  if (line2.length() > 0) {
    int duration = line2.toInt();
    if (duration >= 1 && duration <= 60) {
      appState.timerDuration = duration;
    }
  }

  if (line3.length() > 0) {
    float threshold = line3.toFloat();
    // Auto-migrate: if value > 0, it's old voltage format
    appState.audioThreshold_dBFS = audio_migrate_voltage_threshold(threshold);
  }

  if (line4.length() > 0) {
    uint32_t rate = (uint32_t)line4.toInt();
    if (audio_validate_sample_rate(rate)) {
      appState.audioSampleRate = rate;
    }
  }

  if (line5.length() > 0) {
    float vref = line5.toFloat();
    if (vref >= 1.0f && vref <= 5.0f) {
      appState.adcVref = vref;
    }
  }

  LOG_I("[Sensing] Settings loaded");
  LOG_D("[Sensing]   Mode: %d, Timer: %lu min, Threshold: %+.0f dBFS, Sample Rate: %lu Hz", appState.currentMode,
        appState.timerDuration, appState.audioThreshold_dBFS, appState.audioSampleRate);

  return true;
}

// Save Smart Sensing settings to LittleFS
void saveSmartSensingSettings() {
  File file = LittleFS.open("/smartsensing.txt", "w");
  if (!file) {
    LOG_E("[Sensing] Failed to open settings file for writing");
    return;
  }

  file.println(String(appState.currentMode));
  file.println(String(appState.timerDuration));
  file.println(String(appState.audioThreshold_dBFS, 1));
  file.println(String(appState.audioSampleRate));
  file.println(String(appState.adcVref, 2));
  file.close();

  LOG_I("[Sensing] Settings saved");
}
