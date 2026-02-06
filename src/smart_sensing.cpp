#include "smart_sensing.h"
#include "app_state.h"
#include "config.h"
#include "debug_serial.h"
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
  doc["voltageThreshold"] = voltageThreshold;
  doc["voltageReading"] = lastVoltageReading;
  doc["voltageDetected"] = (lastVoltageReading >= voltageThreshold);

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

      // When switching to SMART_AUTO mode, immediately evaluate voltage state
      if (currentMode == SMART_AUTO) {
        bool voltageDetected = detectVoltage();

        if (voltageDetected) {
          // Voltage is above threshold - turn ON and set timer to full value
          timerRemaining = timerDuration * 60;
          lastTimerUpdate = millis();
          lastVoltageDetection = millis();
          setAmplifierState(true);
          previousVoltageState = true;
          LOG_I("[Sensing] Smart Auto activated: voltage detected, amp ON");
        } else {
          // Voltage is below threshold - turn OFF
          timerRemaining = 0;
          setAmplifierState(false);
          previousVoltageState = false;
          LOG_I("[Sensing] Smart Auto activated: no voltage, amp OFF");
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
          // until voltage disappears
          LOG_I("[Sensing] Timer duration changed to %d min (countdown starts when voltage disappears)", duration);
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

  // Update voltage threshold
  if (doc["voltageThreshold"].is<float>() ||
      doc["voltageThreshold"].is<int>()) {
    float threshold = doc["voltageThreshold"].as<float>();

    if (threshold >= 0.1 && threshold <= 3.3) {
      voltageThreshold = threshold;
      settingsChanged = true;
      LOG_I("[Sensing] Voltage threshold set to %.2fV", threshold);
    } else {
      server.send(400, "application/json",
                  "{\"success\": false, \"message\": \"Voltage threshold must "
                  "be between 0.1 and 3.3 volts\"}");
      return;
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

// Detect voltage on the analog input pin
bool detectVoltage() {
  // Read analog value from GPIO 1 (ADC1_CH0 on ESP32-S3)
  int rawValue = analogRead(VOLTAGE_SENSE_PIN);

  // Convert to voltage (ESP32 ADC is 12-bit: 0-4095 maps to 0-3.3V)
  // Note: ESP32 ADC has non-linearity; this is a basic conversion
  lastVoltageReading = (rawValue / 4095.0) * 3.3;

  // Compare with threshold
  return (lastVoltageReading >= voltageThreshold);
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

  // Rate limit voltage reading to reduce CPU usage (every 50ms is sufficient)
  static unsigned long lastVoltageRead = 0;
  static bool voltageDetected = false;

  if (currentMillis - lastVoltageRead >= 50) {
    lastVoltageRead = currentMillis;
    // Read voltage for real-time display, regardless of mode
    voltageDetected = detectVoltage();
  }

  switch (currentMode) {
  case ALWAYS_ON:
    // Always keep amplifier ON, timer disabled
    setAmplifierState(true);
    timerRemaining = 0;
    previousVoltageState = voltageDetected; // Update state tracking
    break;

  case ALWAYS_OFF:
    // Always keep amplifier OFF, timer disabled
    setAmplifierState(false);
    timerRemaining = 0;
    previousVoltageState = voltageDetected; // Update state tracking
    break;

  case SMART_AUTO: {
    if (voltageDetected) {
      // Voltage is currently detected - keep timer at full value and amplifier
      // ON
      timerRemaining = timerDuration * 60; // Keep timer at full value
      lastVoltageDetection = currentMillis;
      lastTimerUpdate = currentMillis;

      // Ensure amplifier is ON
      if (!amplifierState) {
        setAmplifierState(true);
        LOG_D("[Sensing] Voltage detected, amp ON, timer reset");
      }
    } else {
      // No voltage detected - countdown timer if amplifier is ON
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

    // Update previous voltage state for next iteration
    previousVoltageState = voltageDetected;
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
  doc["voltageThreshold"] = voltageThreshold;
  doc["voltageReading"] = lastVoltageReading;
  doc["voltageDetected"] = (lastVoltageReading >= voltageThreshold);

  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t *)json.c_str(), json.length());

  // Update tracked state after broadcast
  prevBroadcastMode = currentMode;
  prevBroadcastAmplifierState = amplifierState;
  prevBroadcastTimerRemaining = timerRemaining;
  prevBroadcastVoltageReading = lastVoltageReading;
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
                      (fabs(lastVoltageReading - prevBroadcastVoltageReading) >
                       0.02); // 0.02V tolerance for more frequent updates

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
  String line3 = file.readStringUntil('\n'); // voltage threshold
  file.close();

  line1.trim();
  line2.trim();
  line3.trim();

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
    if (threshold >= 0.1 && threshold <= 3.3) {
      voltageThreshold = threshold;
    }
  }

  LOG_I("[Sensing] Settings loaded");
  LOG_D("[Sensing]   Mode: %d, Timer: %lu min, Threshold: %.2fV", currentMode,
        timerDuration, voltageThreshold);

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
  file.println(String(voltageThreshold, 2));
  file.close();

  LOG_I("[Sensing] Settings saved");
}
