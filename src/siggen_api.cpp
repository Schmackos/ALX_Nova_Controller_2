#ifndef NATIVE_TEST

#include "siggen_api.h"
#include "http_security.h"
#include "signal_generator.h"
#include "settings_manager.h"
#include "app_state.h"
#include "globals.h"
#include "auth_handler.h"
#include <ArduinoJson.h>

extern bool requireAuth();

void registerSignalGenApiEndpoints() {
  // GET /api/signalgenerator — read current signal generator state
  server_on_versioned("/api/signalgenerator", HTTP_GET, []() {
    if (!requireAuth()) return;
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
    server_send(200, "application/json", json);
  });

  // POST /api/signalgenerator — update signal generator parameters
  server_on_versioned("/api/signalgenerator", HTTP_POST, []() {
    if (!requireAuth()) return;
    if (!server.hasArg("plain")) {
      server_send(400, "application/json",
                  "{\"success\":false,\"message\":\"No data\"}");
      return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, server.arg("plain"))) {
      server_send(400, "application/json",
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
    server_send(200, "application/json", "{\"success\":true}");
  });
}

#endif // NATIVE_TEST
