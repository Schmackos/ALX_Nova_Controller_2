#include "smart_sensing.h"
#include "app_state.h"
#include "config.h"
#include "debug_serial.h"
#include "i2s_audio.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <cmath>

// ===== Smart Sensing HTTP API Handlers =====

void handleSmartSensingGet() {
  JsonDocument doc;
  doc["success"] = true;

  // Convert mode enum to string
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
  doc["mode"] = modeStr;

  doc["timerDuration"] = timerDuration;
  doc["timerRemaining"] = timerRemaining;
  doc["timerActive"] = (timerRemaining > 0);
  doc["amplifierState"] = amplifierState;
  doc["audioThreshold"] = audioThreshold_dBFS;
  doc["audioLevel"] = audioLevel_dBFS;
  doc["signalDetected"] = (audioLevel_dBFS >= audioThreshold_dBFS);
  doc["audioRmsL"] = appState.audioRmsLeft;
  doc["audioRmsR"] = appState.audioRmsRight;
  doc["audioVuL"] = appState.audioVuLeft;
  doc["audioVuR"] = appState.audioVuRight;
  doc["audioPeakL"] = appState.audioPeakLeft;
  doc["audioPeakR"] = appState.audioPeakRight;
  doc["audioPeak"] = appState.audioPeakCombined;
  doc["audioSampleRate"] = appState.audioSampleRate;

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

    if (currentMode != newMode) {
      currentMode = newMode;
      settingsChanged = true;
      LOG_I("[Sensing] Mode changed to: %s", modeStr.c_str());

      // When switching to SMART_AUTO mode, immediately evaluate signal state
      if (currentMode == SMART_AUTO) {
        bool signalDetected = detectSignal();

        if (signalDetected) {
          // Signal is above threshold - turn ON and set timer to full value
          timerRemaining = timerDuration * 60;
          lastTimerUpdate = millis();
          lastSignalDetection = millis();
          setAmplifierState(true);
          previousSignalState = true;
          LOG_I("[Sensing] Smart Auto activated: signal detected, amp ON");
        } else {
          // Signal is below threshold - turn OFF
          timerRemaining = 0;
          setAmplifierState(false);
          previousSignalState = false;
          LOG_I("[Sensing] Smart Auto activated: no signal, amp OFF");
        }
      }
    }
  }

  // Update timer duration
  if (doc["timerDuration"].is<int>()) {
    int duration = doc["timerDuration"].as<int>();

    if (duration >= 1 && duration <= 60) {
      timerDuration = duration;
      settingsChanged = true;

      // Update timer remaining in SMART_AUTO mode
      if (currentMode == SMART_AUTO) {
        // Always update timerRemaining to show the new duration
        timerRemaining = timerDuration * 60;

        if (amplifierState) {
          // Amplifier is ON - update timer to new duration
          lastTimerUpdate = millis();
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
      audioThreshold_dBFS = threshold;
      settingsChanged = true;
      LOG_I("[Sensing] Audio threshold set to %+.0f dBFS", threshold);
    } else {
      server.send(400, "application/json",
                  "{\"success\": false, \"message\": \"Audio threshold must "
                  "be between -96 and 0 dBFS\"}");
      return;
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

    if (currentMode == SMART_AUTO) {
      if (state) {
        // If turning on manually in SMART_AUTO mode, set timer to full value
        timerRemaining = timerDuration * 60;
        lastTimerUpdate = millis();
        LOG_D("[Sensing] Manual ON: timer set to full value");
      } else {
        // If turning off manually in SMART_AUTO mode, reset timer to 0
        timerRemaining = 0;
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
  audioLevel_dBFS = analysis.dBFS;
  appState.audioRmsLeft = analysis.rmsLeft;
  appState.audioRmsRight = analysis.rmsRight;
  appState.audioRmsCombined = analysis.rmsCombined;
  appState.audioVuLeft = analysis.vuLeft;
  appState.audioVuRight = analysis.vuRight;
  appState.audioVuCombined = analysis.vuCombined;
  appState.audioPeakLeft = analysis.peakLeft;
  appState.audioPeakRight = analysis.peakRight;
  appState.audioPeakCombined = analysis.peakCombined;
  return analysis.signalDetected;
}

// Set amplifier state and update pin
void setAmplifierState(bool state) {
  if (amplifierState != state) {
    amplifierState = state;
    digitalWrite(AMPLIFIER_PIN, state ? HIGH : LOW);
    LOG_I("[Sensing] Amplifier %s", state ? "ON" : "OFF");
  }
}

// Main Smart Sensing logic - called from loop()
void updateSmartSensingLogic() {
  unsigned long currentMillis = millis();

  // Rate limit signal reading to reduce CPU usage (every 50ms is sufficient)
  static unsigned long lastSignalRead = 0;
  static bool signalDetected = false;

  if (currentMillis - lastSignalRead >= 50) {
    lastSignalRead = currentMillis;
    // Read audio level for real-time display, regardless of mode
    signalDetected = detectSignal();
  }

  switch (currentMode) {
  case ALWAYS_ON:
    // Always keep amplifier ON, timer disabled
    setAmplifierState(true);
    timerRemaining = 0;
    previousSignalState = signalDetected; // Update state tracking
    break;

  case ALWAYS_OFF:
    // Always keep amplifier OFF, timer disabled
    setAmplifierState(false);
    timerRemaining = 0;
    previousSignalState = signalDetected; // Update state tracking
    break;

  case SMART_AUTO: {
    if (signalDetected) {
      // Signal is currently detected - keep timer at full value and amplifier
      // ON
      timerRemaining = timerDuration * 60; // Keep timer at full value
      lastSignalDetection = currentMillis;
      lastTimerUpdate = currentMillis;

      // Ensure amplifier is ON
      if (!amplifierState) {
        setAmplifierState(true);
        LOG_D("[Sensing] Signal detected, amp ON, timer reset");
      }
    } else {
      // No signal detected - countdown timer if amplifier is ON
      if (amplifierState && timerRemaining > 0) {
        if (currentMillis - lastTimerUpdate >= 1000) {
          lastTimerUpdate = currentMillis;
          timerRemaining--;

          if (timerRemaining == 0) {
            setAmplifierState(false);
            LOG_I("[Sensing] Timer expired, amplifier OFF");
          }
        }
      }
    }

    // Update previous signal state for next iteration
    previousSignalState = signalDetected;
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
  doc["mode"] = modeStr;

  doc["timerDuration"] = timerDuration;
  doc["timerRemaining"] = timerRemaining;
  doc["timerActive"] = (timerRemaining > 0);
  doc["amplifierState"] = amplifierState;
  doc["audioThreshold"] = audioThreshold_dBFS;
  doc["audioLevel"] = audioLevel_dBFS;
  doc["signalDetected"] = (audioLevel_dBFS >= audioThreshold_dBFS);
  doc["audioRmsL"] = appState.audioRmsLeft;
  doc["audioRmsR"] = appState.audioRmsRight;
  doc["audioVuL"] = appState.audioVuLeft;
  doc["audioVuR"] = appState.audioVuRight;
  doc["audioPeakL"] = appState.audioPeakLeft;
  doc["audioPeakR"] = appState.audioPeakRight;
  doc["audioPeak"] = appState.audioPeakCombined;

  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t *)json.c_str(), json.length());

  // Update tracked state after broadcast
  prevBroadcastMode = currentMode;
  prevBroadcastAmplifierState = amplifierState;
  prevBroadcastTimerRemaining = timerRemaining;
  prevBroadcastAudioLevel = audioLevel_dBFS;
  lastSmartSensingHeartbeat = millis();
}

// Send Smart Sensing state - only broadcasts if state changed or heartbeat
// interval elapsed
void sendSmartSensingState() {
  unsigned long currentMillis = millis();

  // Check if any state has changed
  bool stateChanged = (currentMode != prevBroadcastMode) ||
                      (amplifierState != prevBroadcastAmplifierState) ||
                      (timerRemaining != prevBroadcastTimerRemaining) ||
                      (fabs(audioLevel_dBFS - prevBroadcastAudioLevel) >
                       0.5); // 0.5 dBFS tolerance for audio level changes

  // Check if heartbeat interval has elapsed
  bool heartbeatDue = (currentMillis - lastSmartSensingHeartbeat >=
                       SMART_SENSING_HEARTBEAT_INTERVAL);

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
  file.close();

  line1.trim();
  line2.trim();
  line3.trim();
  line4.trim();

  if (line1.length() > 0) {
    int mode = line1.toInt();
    if (mode >= 0 && mode <= 2) {
      currentMode = static_cast<SensingMode>(mode);
    }
  }

  if (line2.length() > 0) {
    int duration = line2.toInt();
    if (duration >= 1 && duration <= 60) {
      timerDuration = duration;
    }
  }

  if (line3.length() > 0) {
    float threshold = line3.toFloat();
    // Auto-migrate: if value > 0, it's old voltage format
    audioThreshold_dBFS = audio_migrate_voltage_threshold(threshold);
  }

  if (line4.length() > 0) {
    uint32_t rate = (uint32_t)line4.toInt();
    if (audio_validate_sample_rate(rate)) {
      appState.audioSampleRate = rate;
    }
  }

  LOG_I("[Sensing] Settings loaded");
  LOG_D("[Sensing]   Mode: %d, Timer: %lu min, Threshold: %+.0f dBFS, Sample Rate: %lu Hz", currentMode,
        timerDuration, audioThreshold_dBFS, appState.audioSampleRate);

  return true;
}

// Save Smart Sensing settings to LittleFS
void saveSmartSensingSettings() {
  File file = LittleFS.open("/smartsensing.txt", "w");
  if (!file) {
    LOG_E("[Sensing] Failed to open settings file for writing");
    return;
  }

  file.println(String(currentMode));
  file.println(String(timerDuration));
  file.println(String(audioThreshold_dBFS, 1));
  file.println(String(appState.audioSampleRate));
  file.close();

  LOG_I("[Sensing] Settings saved");
}
