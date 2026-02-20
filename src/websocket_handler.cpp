#include "websocket_handler.h"
#include "auth_handler.h"
#include "config.h"
#include "app_state.h"
#include "crash_log.h"
#include "settings_manager.h"
#include "wifi_manager.h"
#include "smart_sensing.h"
#include "ota_updater.h"
#include "debug_serial.h"
#include "utils.h"
#include "i2s_audio.h"
#include "signal_generator.h"
#include "task_monitor.h"
#ifdef DSP_ENABLED
#include "dsp_pipeline.h"
#include "dsp_coefficients.h"
#include "dsp_crossover.h"
#endif
#ifdef DAC_ENABLED
#include "dac_hal.h"
#include "dac_registry.h"
#include "dac_eeprom.h"
#endif
#ifdef USB_AUDIO_ENABLED
#include "usb_audio.h"
#endif
#include <WiFi.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#ifndef NATIVE_TEST
#include <esp_heap_caps.h>
#endif
#include "esp_freertos_hooks.h"
#include "esp_timer.h"

// PSRAM-backed WebSocket serialization buffer (eliminates ~23 String heap allocs per broadcast cycle)
// Must be large enough for the largest WS message. DSP state with 6ch × 24 stages (all enabled with
// biquad coefficients) reaches ~16 KB. Default state (all disabled, no coeffs) is ~5.2 KB.
static char *_wsBuf = nullptr;
static const size_t WS_BUF_SIZE = 16384;

void ws_init_buffers() {
#ifndef NATIVE_TEST
    if (!_wsBuf) {
        _wsBuf = (char *)heap_caps_malloc(WS_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
#endif
    if (!_wsBuf) {
        _wsBuf = (char *)malloc(WS_BUF_SIZE);
    }
}

// Helper: broadcast a JsonDocument to all WebSocket clients using the PSRAM buffer.
// Falls back to dynamic String allocation if the serialized JSON exceeds WS_BUF_SIZE
// (prevents sending truncated JSON which causes client-side parse failures).
static void wsBroadcastJson(JsonDocument &doc) {
    if (_wsBuf) {
        size_t needed = measureJson(doc) + 1;
        if (needed <= WS_BUF_SIZE) {
            size_t len = serializeJson(doc, _wsBuf, WS_BUF_SIZE);
            webSocket.broadcastTXT(_wsBuf, len);
            return;
        }
        LOG_W("[WebSocket] wsBroadcastJson: JSON %u bytes exceeds buffer (%u), using dynamic alloc", (unsigned)needed, (unsigned)WS_BUF_SIZE);
    }
    String json;
    serializeJson(doc, json);
    webSocket.broadcastTXT(json);
}

// Helper: send a JsonDocument to a single WebSocket client using the PSRAM buffer.
// Falls back to dynamic String allocation if the serialized JSON exceeds WS_BUF_SIZE.
static void wsSendJson(uint8_t num, JsonDocument &doc) {
    if (_wsBuf) {
        size_t needed = measureJson(doc) + 1;
        if (needed <= WS_BUF_SIZE) {
            size_t len = serializeJson(doc, _wsBuf, WS_BUF_SIZE);
            webSocket.sendTXT(num, _wsBuf, len);
            return;
        }
        LOG_W("[WebSocket] wsSendJson: JSON %u bytes exceeds buffer (%u), using dynamic alloc", (unsigned)needed, (unsigned)WS_BUF_SIZE);
    }
    String json;
    serializeJson(doc, json);
    webSocket.sendTXT(num, json.c_str());
}

// ===== WebSocket Authentication Tracking =====
bool wsAuthStatus[MAX_WS_CLIENTS] = {false};
unsigned long wsAuthTimeout[MAX_WS_CLIENTS] = {0};
static String wsSessionId[MAX_WS_CLIENTS];

// ===== Per-client IP Binding =====
// Stores the remote IP captured at connect time. Messages from a different IP
// are rejected and the connection is dropped (Session-IP hijack protection).
IPAddress wsClientIP[MAX_WS_CLIENTS];

// ===== Per-client Audio Streaming Subscription =====
static bool _audioSubscribed[MAX_WS_CLIENTS] = {};

// ===== CPU Utilization Tracking =====
// Uses FreeRTOS idle hooks with microsecond wall-clock timing.
// Each hook accumulates actual wall-clock microseconds spent in idle,
// not iteration counts (which are affected by WiFi interrupt overhead).
static volatile int64_t idleTimeUs0 = 0;   // Accumulated idle microseconds
static volatile int64_t idleTimeUs1 = 0;
static int64_t lastIdleTimeUs0 = 0;        // Previous snapshot
static int64_t lastIdleTimeUs1 = 0;
static int64_t lastCpuMeasureTimeUs = 0;   // Wall clock at last measurement
static float cpuUsageCore0 = -1.0f;
static float cpuUsageCore1 = -1.0f;
static bool cpuHooksInstalled = false;
static int cpuWarmupCycles = 0;            // Skip first 2 measurements for stability

// Track entry time per-core (local to each core, no cross-core cache contention)
static volatile int64_t idleEntryUs0 = 0;
static volatile int64_t idleEntryUs1 = 0;

// Idle hook: measure wall-clock time between calls using esp_timer_get_time().
// Each call = one iteration of the idle task loop. We accumulate the delta
// between consecutive calls, which represents time spent in idle (not in ISRs/tasks).
static bool idleHookCore0() {
  int64_t now = esp_timer_get_time();
  if (idleEntryUs0 > 0) {
    int64_t delta = now - idleEntryUs0;
    // Only count short deltas (<1ms) — longer gaps mean we were preempted
    if (delta < 1000) idleTimeUs0 += delta;
  }
  idleEntryUs0 = now;
  return false;
}

static bool idleHookCore1() {
  int64_t now = esp_timer_get_time();
  if (idleEntryUs1 > 0) {
    int64_t delta = now - idleEntryUs1;
    if (delta < 1000) idleTimeUs1 += delta;
  }
  idleEntryUs1 = now;
  return false;
}

// ===== WebSocket Event Handler =====

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      LOG_I("[WebSocket] Client [%u] disconnected", num);
      wsAuthStatus[num] = false;
      wsAuthTimeout[num] = 0;
      wsSessionId[num] = "";
      _audioSubscribed[num] = false;
      wsClientIP[num] = IPAddress();
      break;

    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        LOG_I("[WebSocket] Client [%u] connected from %d.%d.%d.%d", num, ip[0], ip[1], ip[2], ip[3]);

        // Bind this slot to the connecting client's IP
        wsClientIP[num] = ip;

        // Set auth timeout (5 seconds to authenticate)
        wsAuthStatus[num] = false;
        wsAuthTimeout[num] = millis() + 5000;

        // Request authentication
        webSocket.sendTXT(num, "{\"type\":\"authRequired\"}");
      }
      break;

    case WStype_TEXT:
      {
        LOG_D("[WebSocket] Received from client [%u]: %s", num, payload);

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, (const char*)payload, length);

        if (error) {
          LOG_E("[WebSocket] JSON parsing failed: %s", error.c_str());
          return;
        }

        String msgType = doc["type"].as<String>();

        // Handle authentication message
        if (msgType == "auth") {
          String sessionId = doc["sessionId"].as<String>();

          if (validateSession(sessionId)) {
            wsAuthStatus[num] = true;
            wsAuthTimeout[num] = 0;
            wsSessionId[num] = sessionId;
            // Confirm/update the stored IP binding at auth success
            wsClientIP[num] = webSocket.remoteIP(num);
            webSocket.sendTXT(num, "{\"type\":\"authSuccess\"}");
            LOG_D("[WebSocket] Client [%u] authenticated", num);

            // Send initial state after authentication
            sendWiFiStatus();
            sendSmartSensingStateInternal();
            sendDisplayState();
            sendBuzzerState();
            sendSignalGenState();
            sendAudioGraphState();
            sendDebugState();
            // Send per-ADC enabled state
            {
              JsonDocument adcDoc;
              adcDoc["type"] = "adcState";
              JsonArray arr = adcDoc["enabled"].to<JsonArray>();
              for (int i = 0; i < NUM_AUDIO_INPUTS; i++) arr.add(appState.adcEnabled[i]);
              String adcJson;
              serializeJson(adcDoc, adcJson);
              webSocket.sendTXT(num, adcJson.c_str());
            }
#ifdef DSP_ENABLED
            sendDspState();
#endif
#ifdef DAC_ENABLED
            sendDacState();
#endif
#ifdef USB_AUDIO_ENABLED
            sendUsbAudioState();
#endif
            // If device just updated, notify the client
            if (appState.justUpdated) {
              broadcastJustUpdated();
            }
          } else {
            webSocket.sendTXT(num, "{\"type\":\"authFailed\",\"error\":\"Invalid session\"}");
            webSocket.disconnect(num);
          }
          return;
        }

        // Re-validate session for every non-auth command (catches logout/expiry)
        if (!wsAuthStatus[num] || !validateSession(wsSessionId[num])) {
          wsAuthStatus[num] = false;
          wsSessionId[num] = "";
          webSocket.sendTXT(num, "{\"type\":\"authFailed\",\"error\":\"Session expired or revoked\"}");
          webSocket.disconnect(num);
          return;
        }

        // IP binding check: reject messages arriving from a different IP than
        // the one that was present at connect/auth time (session-IP hijack guard)
        if (webSocket.remoteIP(num) != wsClientIP[num]) {
          LOG_W("[WebSocket] Client [%u] IP mismatch — dropping connection", num);
          webSocket.disconnect(num);
          return;
        }

        if (doc["type"] == "toggleAP") {
          bool enabled = doc["enabled"].as<bool>();
          appState.apEnabled = enabled;
          
          if (enabled) {
            if (!appState.isAPMode) {
              WiFi.mode(WIFI_AP_STA);
              WiFi.softAP(appState.apSSID, appState.apPassword);
              appState.isAPMode = true;
              LOG_I("[WebSocket] Access Point enabled");
              LOG_I("[WebSocket] AP IP: %s", WiFi.softAPIP().toString().c_str());
            }
          } else {
            if (appState.isAPMode && WiFi.status() == WL_CONNECTED) {
              WiFi.softAPdisconnect(true);
              WiFi.mode(WIFI_STA);
              appState.isAPMode = false;
              LOG_I("[WebSocket] Access Point disabled");
            }
          }
          
          sendWiFiStatus();
        } else if (doc["type"] == "getHardwareStats") {
          // Client requesting hardware stats
          sendHardwareStats();
        } else if (msgType == "setBacklight") {
          bool newState = doc["enabled"].as<bool>();
          AppState::getInstance().setBacklightOn(newState);
          LOG_I("[WebSocket] Backlight set to %s", newState ? "ON" : "OFF");
          sendDisplayState();
        } else if (msgType == "setScreenTimeout") {
          int timeoutSec = doc["value"].as<int>();
          unsigned long timeoutMs = (unsigned long)timeoutSec * 1000UL;
          if (timeoutMs == 0 || timeoutMs == 30000 || timeoutMs == 60000 ||
              timeoutMs == 300000 || timeoutMs == 600000) {
            AppState::getInstance().setScreenTimeout(timeoutMs);
            saveSettings();
            LOG_I("[WebSocket] Screen timeout set to %d seconds", timeoutSec);
            sendDisplayState();
          }
        } else if (msgType == "setBrightness") {
          int newBright = doc["value"].as<int>();
          if (newBright >= 1 && newBright <= 255) {
            AppState::getInstance().setBacklightBrightness((uint8_t)newBright);
            saveSettings();
            LOG_I("[WebSocket] Brightness set to %d", newBright);
            sendDisplayState();
          }
        } else if (msgType == "setDimEnabled") {
          bool newState = doc["enabled"].as<bool>();
          AppState::getInstance().setDimEnabled(newState);
          saveSettings();
          LOG_I("[WebSocket] Dim %s", newState ? "enabled" : "disabled");
          sendDisplayState();
        } else if (msgType == "setDimTimeout") {
          int dimSec = doc["value"].as<int>();
          unsigned long dimMs = (unsigned long)dimSec * 1000UL;
          if (dimMs == 5000 || dimMs == 10000 || dimMs == 15000 ||
              dimMs == 30000 || dimMs == 60000) {
            AppState::getInstance().setDimTimeout(dimMs);
            saveSettings();
            LOG_I("[WebSocket] Dim timeout set to %d seconds", dimSec);
            sendDisplayState();
          }
        } else if (msgType == "setDimBrightness") {
          int dimPwm = doc["value"].as<int>();
          if (dimPwm == 26 || dimPwm == 64 || dimPwm == 128 || dimPwm == 191) {
            AppState::getInstance().setDimBrightness((uint8_t)dimPwm);
            saveSettings();
            LOG_I("[WebSocket] Dim brightness set to %d", dimPwm);
            sendDisplayState();
          }
        } else if (msgType == "setBuzzerEnabled") {
          bool newState = doc["enabled"].as<bool>();
          AppState::getInstance().setBuzzerEnabled(newState);
          saveSettings();
          LOG_I("[WebSocket] Buzzer set to %s", newState ? "ON" : "OFF");
          sendBuzzerState();
        } else if (msgType == "setBuzzerVolume") {
          int newVol = doc["value"].as<int>();
          if (newVol >= 0 && newVol <= 2) {
            AppState::getInstance().setBuzzerVolume(newVol);
            saveSettings();
            LOG_I("[WebSocket] Buzzer volume set to %d", newVol);
            sendBuzzerState();
          }
        } else if (msgType == "subscribeAudio") {
          bool enabled = doc["enabled"] | false;
          _audioSubscribed[num] = enabled;
          LOG_I("[WebSocket] Client [%u] audio subscription %s", num, enabled ? "enabled" : "disabled");
        } else if (msgType == "setAudioUpdateRate") {
          int rate = doc["value"].as<int>();
          if (rate == 20 || rate == 33 || rate == 50 || rate == 100) {
            appState.audioUpdateRate = (uint16_t)rate;
            saveSettings();
            LOG_I("[WebSocket] Audio update rate set to %d ms", rate);
          }
        } else if (msgType == "setVuMeterEnabled") {
          appState.vuMeterEnabled = doc["enabled"].as<bool>();
          saveSettings();
          sendAudioGraphState();
          LOG_I("[WebSocket] VU meter %s", appState.vuMeterEnabled ? "enabled" : "disabled");
        } else if (msgType == "setWaveformEnabled") {
          appState.waveformEnabled = doc["enabled"].as<bool>();
          saveSettings();
          sendAudioGraphState();
          LOG_I("[WebSocket] Waveform %s", appState.waveformEnabled ? "enabled" : "disabled");
        } else if (msgType == "setSpectrumEnabled") {
          appState.spectrumEnabled = doc["enabled"].as<bool>();
          saveSettings();
          sendAudioGraphState();
          LOG_I("[WebSocket] Spectrum %s", appState.spectrumEnabled ? "enabled" : "disabled");
        } else if (msgType == "setFftWindowType") {
          int wt = doc["value"].as<int>();
          if (wt >= 0 && wt < FFT_WINDOW_COUNT) {
            appState.fftWindowType = (FftWindowType)wt;
            saveSettings();
            sendAudioGraphState();
            LOG_I("[WebSocket] FFT window type: %d", wt);
          }
        } else if (msgType == "setSignalGen") {
          bool changed = false;
          if (doc["enabled"].is<bool>()) {
            appState.sigGenEnabled = doc["enabled"].as<bool>();
            changed = true;
          }
          if (doc["waveform"].is<int>()) {
            int w = doc["waveform"].as<int>();
            if (w >= 0 && w <= 3) { appState.sigGenWaveform = w; changed = true; }
          }
          if (doc["frequency"].is<float>()) {
            float f = doc["frequency"].as<float>();
            if (f >= 1.0f && f <= 22000.0f) { appState.sigGenFrequency = f; changed = true; }
          }
          if (doc["amplitude"].is<float>()) {
            float a = doc["amplitude"].as<float>();
            if (a >= -96.0f && a <= 0.0f) { appState.sigGenAmplitude = a; changed = true; }
          }
          if (doc["channel"].is<int>()) {
            int c = doc["channel"].as<int>();
            if (c >= 0 && c <= 2) { appState.sigGenChannel = c; changed = true; }
          }
          if (doc["outputMode"].is<int>()) {
            int m = doc["outputMode"].as<int>();
            if (m >= 0 && m <= 1) { appState.sigGenOutputMode = m; changed = true; }
          }
          if (doc["sweepSpeed"].is<float>()) {
            float s = doc["sweepSpeed"].as<float>();
            if (s >= 1.0f && s <= 22000.0f) { appState.sigGenSweepSpeed = s; changed = true; }
          }
          if (doc["targetAdc"].is<int>()) {
            int t = doc["targetAdc"].as<int>();
            if (t >= 0 && t <= 4) { appState.sigGenTargetAdc = t; changed = true; }
          }
          if (changed) {
            siggen_apply_params();
            saveSignalGenSettings();
            sendSignalGenState();
            LOG_I("[WebSocket] Signal generator updated by client [%u]", num);
          }
        } else if (msgType == "setDeviceName") {
          const char* nameRaw = doc["name"].as<const char*>();
          if (!nameRaw) nameRaw = "";
          char nameBuf[33];
          setCharField(nameBuf, sizeof(nameBuf), nameRaw);
          setCharField(appState.customDeviceName, sizeof(appState.customDeviceName), nameBuf);
          // Update AP SSID to reflect new custom name
          char apName[33];
          if (strlen(appState.customDeviceName) > 0) {
            setCharField(apName, sizeof(apName), appState.customDeviceName);
          } else {
            char apNameTmp[33];
            snprintf(apNameTmp, sizeof(apNameTmp), "ALX-Nova-%s", appState.deviceSerialNumber);
            setCharField(apName, sizeof(apName), apNameTmp);
          }
          setCharField(appState.apSSID, sizeof(appState.apSSID), apName);
          saveSettings();
          // Broadcast updated WiFi status so clients see the new name
          sendWiFiStatus();
          LOG_I("[WebSocket] Custom device name set to: '%s'", appState.customDeviceName);
        } else if (msgType == "setInputNames") {
          if (doc["names"].is<JsonArray>()) {
            JsonArray names = doc["names"].as<JsonArray>();
            for (int i = 0; i < NUM_AUDIO_INPUTS * 2 && i < (int)names.size(); i++) {
              String name = names[i].as<String>();
              if (name.length() > 0) appState.inputNames[i] = name;
            }
            saveInputNames();
            // Broadcast updated names
            JsonDocument resp;
            resp["type"] = "inputNames";
            JsonArray outNames = resp["names"].to<JsonArray>();
            for (int i = 0; i < NUM_AUDIO_INPUTS * 2; i++) {
              outNames.add(appState.inputNames[i]);
            }
            wsBroadcastJson(resp);
            LOG_I("[WebSocket] Input names updated by client [%u]", num);
          }
        } else if (msgType == "setDebugMode") {
          appState.debugMode = doc["enabled"].as<bool>();
          applyDebugSerialLevel(appState.debugMode, appState.debugSerialLevel);
          saveSettings();
          sendDebugState();
          LOG_I("[WebSocket] Debug mode %s", appState.debugMode ? "enabled" : "disabled");
        } else if (msgType == "setDebugSerialLevel") {
          int level = doc["level"].as<int>();
          if (level >= 0 && level <= 3) {
            appState.debugSerialLevel = level;
            applyDebugSerialLevel(appState.debugMode, appState.debugSerialLevel);
            saveSettings();
            sendDebugState();
            LOG_I("[WebSocket] Debug serial level set to %d", level);
          }
        } else if (msgType == "setDebugHwStats") {
          appState.debugHwStats = doc["enabled"].as<bool>();
          saveSettings();
          sendDebugState();
          LOG_I("[WebSocket] Debug HW stats %s", appState.debugHwStats ? "enabled" : "disabled");
        } else if (msgType == "setDebugI2sMetrics") {
          appState.debugI2sMetrics = doc["enabled"].as<bool>();
          saveSettings();
          sendDebugState();
          LOG_I("[WebSocket] Debug I2S metrics %s", appState.debugI2sMetrics ? "enabled" : "disabled");
        } else if (msgType == "setDebugTaskMonitor") {
          appState.debugTaskMonitor = doc["enabled"].as<bool>();
          saveSettings();
          sendDebugState();
          LOG_I("[WebSocket] Debug task monitor %s", appState.debugTaskMonitor ? "enabled" : "disabled");
        }
#ifdef DSP_ENABLED
        else if (msgType == "setDspBypass") {
          if (doc["enabled"].is<bool>()) appState.dspEnabled = doc["enabled"].as<bool>();
          if (doc["bypass"].is<bool>()) appState.dspBypass = doc["bypass"].as<bool>();
          // Sync bypass to DSP config (must match appState for UI + pipeline consistency)
          dsp_copy_active_to_inactive();
          DspState *cfg = dsp_get_inactive_config();
          cfg->globalBypass = appState.dspBypass;
          if (!dsp_swap_config()) { appState.dspSwapFailures++; appState.lastDspSwapFailure = millis(); LOG_W("[WebSocket] Swap failed, staged for retry"); }
          extern void saveDspSettingsDebounced();
          saveDspSettingsDebounced();
          appState.markDspConfigDirty();
          LOG_I("[WebSocket] DSP enabled=%d bypass=%d", appState.dspEnabled, appState.dspBypass);
        }
        else if (msgType == "addDspStage") {
          int ch = doc["ch"] | -1;
          int typeInt = doc["stageType"] | (int)DSP_BIQUAD_PEQ;
          if (ch >= 0 && ch < DSP_MAX_CHANNELS) {
            dsp_copy_active_to_inactive();
            int idx = dsp_add_stage(ch, (DspStageType)typeInt);
            if (idx >= 0) {
              // Apply optional overrides (e.g., DC Block: freq=10, label="DC Block")
              DspState *inCfg = dsp_get_inactive_config();
              DspStage &added = inCfg->channels[ch].stages[idx];
              if (dsp_is_biquad_type((DspStageType)typeInt)) {
                if (doc["frequency"].is<float>()) added.biquad.frequency = doc["frequency"].as<float>();
                if (doc["Q"].is<float>()) added.biquad.Q = doc["Q"].as<float>();
                if (doc["gain"].is<float>()) added.biquad.gain = doc["gain"].as<float>();
                dsp_compute_biquad_coeffs(added.biquad, added.type, inCfg->sampleRate);
              }
              if (doc["label"].is<const char*>()) {
                strncpy(added.label, doc["label"].as<const char*>(), sizeof(added.label) - 1);
                added.label[sizeof(added.label) - 1] = '\0';
              }
              if (!dsp_swap_config()) { appState.dspSwapFailures++; appState.lastDspSwapFailure = millis(); LOG_W("[WebSocket] Swap failed, staged for retry"); }
              extern void saveDspSettingsDebounced();
              saveDspSettingsDebounced();
              appState.markDspConfigDirty();
              LOG_I("[WebSocket] DSP stage added ch=%d type=%d idx=%d", ch, typeInt, idx);
            } else {
              JsonDocument errDoc;
              errDoc["type"] = "dspError";
              errDoc["message"] = "Resource pool full (FIR/delay slots exhausted)";
              char errBuf[128];
              serializeJson(errDoc, errBuf, sizeof(errBuf));
              webSocket.sendTXT(num, errBuf);
            }
          }
        }
        else if (msgType == "removeDspStage") {
          int ch = doc["ch"] | -1;
          int si = doc["stage"] | -1;
          if (ch >= 0 && ch < DSP_MAX_CHANNELS) {
            dsp_copy_active_to_inactive();
            if (dsp_remove_stage(ch, si)) {
              if (!dsp_swap_config()) { appState.dspSwapFailures++; appState.lastDspSwapFailure = millis(); LOG_W("[WebSocket] Swap failed, staged for retry"); }
              extern void saveDspSettingsDebounced();
              saveDspSettingsDebounced();
              appState.markDspConfigDirty();
              LOG_I("[WebSocket] DSP stage removed ch=%d stage=%d", ch, si);
            }
          }
        }
        else if (msgType == "updateDspStage") {
          int ch = doc["ch"] | -1;
          int si = doc["stage"] | -1;
          if (ch >= 0 && ch < DSP_MAX_CHANNELS) {
            dsp_copy_active_to_inactive();
            DspState *cfg = dsp_get_inactive_config();
            if (si >= 0 && si < cfg->channels[ch].stageCount) {
              DspStage &s = cfg->channels[ch].stages[si];
              if (doc["enabled"].is<bool>()) s.enabled = doc["enabled"].as<bool>();
              if (dsp_is_biquad_type(s.type)) {
                if (doc["freq"].is<float>()) s.biquad.frequency = doc["freq"].as<float>();
                if (doc["gain"].is<float>()) s.biquad.gain = doc["gain"].as<float>();
                if (doc["Q"].is<float>()) s.biquad.Q = doc["Q"].as<float>();
                if (doc["Q2"].is<float>()) s.biquad.Q2 = doc["Q2"].as<float>();
                dsp_compute_biquad_coeffs(s.biquad, s.type, cfg->sampleRate);
              } else if (s.type == DSP_LIMITER) {
                if (doc["thresholdDb"].is<float>()) s.limiter.thresholdDb = doc["thresholdDb"].as<float>();
                if (doc["attackMs"].is<float>()) s.limiter.attackMs = doc["attackMs"].as<float>();
                if (doc["releaseMs"].is<float>()) s.limiter.releaseMs = doc["releaseMs"].as<float>();
                if (doc["ratio"].is<float>()) s.limiter.ratio = doc["ratio"].as<float>();
              } else if (s.type == DSP_GAIN) {
                if (doc["gainDb"].is<float>()) s.gain.gainDb = doc["gainDb"].as<float>();
                extern void dsp_compute_gain_linear(DspGainParams &p);
                dsp_compute_gain_linear(s.gain);
              } else if (s.type == DSP_DELAY) {
                if (doc["delaySamples"].is<int>()) {
                  uint16_t ds = doc["delaySamples"].as<uint16_t>();
                  s.delay.delaySamples = ds > DSP_MAX_DELAY_SAMPLES ? DSP_MAX_DELAY_SAMPLES : ds;
                }
              } else if (s.type == DSP_POLARITY) {
                if (doc["inverted"].is<bool>()) s.polarity.inverted = doc["inverted"].as<bool>();
              } else if (s.type == DSP_MUTE) {
                if (doc["muted"].is<bool>()) s.mute.muted = doc["muted"].as<bool>();
              } else if (s.type == DSP_COMPRESSOR) {
                if (doc["thresholdDb"].is<float>()) s.compressor.thresholdDb = doc["thresholdDb"].as<float>();
                if (doc["attackMs"].is<float>()) s.compressor.attackMs = doc["attackMs"].as<float>();
                if (doc["releaseMs"].is<float>()) s.compressor.releaseMs = doc["releaseMs"].as<float>();
                if (doc["ratio"].is<float>()) s.compressor.ratio = doc["ratio"].as<float>();
                if (doc["kneeDb"].is<float>()) s.compressor.kneeDb = doc["kneeDb"].as<float>();
                if (doc["makeupGainDb"].is<float>()) s.compressor.makeupGainDb = doc["makeupGainDb"].as<float>();
                extern void dsp_compute_compressor_makeup(DspCompressorParams &p);
                dsp_compute_compressor_makeup(s.compressor);
              } else if (s.type == DSP_NOISE_GATE) {
                if (doc["thresholdDb"].is<float>()) s.noiseGate.thresholdDb = doc["thresholdDb"].as<float>();
                if (doc["attackMs"].is<float>()) s.noiseGate.attackMs = doc["attackMs"].as<float>();
                if (doc["holdMs"].is<float>()) s.noiseGate.holdMs = doc["holdMs"].as<float>();
                if (doc["releaseMs"].is<float>()) s.noiseGate.releaseMs = doc["releaseMs"].as<float>();
                if (doc["ratio"].is<float>()) s.noiseGate.ratio = doc["ratio"].as<float>();
                if (doc["rangeDb"].is<float>()) s.noiseGate.rangeDb = doc["rangeDb"].as<float>();
              } else if (s.type == DSP_TONE_CTRL) {
                if (doc["bassGain"].is<float>()) s.toneCtrl.bassGain = doc["bassGain"].as<float>();
                if (doc["midGain"].is<float>()) s.toneCtrl.midGain = doc["midGain"].as<float>();
                if (doc["trebleGain"].is<float>()) s.toneCtrl.trebleGain = doc["trebleGain"].as<float>();
                extern void dsp_compute_tone_ctrl_coeffs(DspToneCtrlParams &, uint32_t);
                dsp_compute_tone_ctrl_coeffs(s.toneCtrl, cfg->sampleRate);
              } else if (s.type == DSP_SPEAKER_PROT) {
                if (doc["powerRatingW"].is<float>()) s.speakerProt.powerRatingW = doc["powerRatingW"].as<float>();
                if (doc["impedanceOhms"].is<float>()) s.speakerProt.impedanceOhms = doc["impedanceOhms"].as<float>();
                if (doc["thermalTauMs"].is<float>()) s.speakerProt.thermalTauMs = doc["thermalTauMs"].as<float>();
                if (doc["excursionLimitMm"].is<float>()) s.speakerProt.excursionLimitMm = doc["excursionLimitMm"].as<float>();
                if (doc["driverDiameterMm"].is<float>()) s.speakerProt.driverDiameterMm = doc["driverDiameterMm"].as<float>();
                if (doc["maxTempC"].is<float>()) s.speakerProt.maxTempC = doc["maxTempC"].as<float>();
              } else if (s.type == DSP_STEREO_WIDTH) {
                if (doc["width"].is<float>()) s.stereoWidth.width = doc["width"].as<float>();
                if (doc["centerGainDb"].is<float>()) s.stereoWidth.centerGainDb = doc["centerGainDb"].as<float>();
                extern void dsp_compute_stereo_width(DspStereoWidthParams &);
                dsp_compute_stereo_width(s.stereoWidth);
              } else if (s.type == DSP_LOUDNESS) {
                if (doc["referenceLevelDb"].is<float>()) s.loudness.referenceLevelDb = doc["referenceLevelDb"].as<float>();
                if (doc["currentLevelDb"].is<float>()) s.loudness.currentLevelDb = doc["currentLevelDb"].as<float>();
                if (doc["amount"].is<float>()) s.loudness.amount = doc["amount"].as<float>();
                extern void dsp_compute_loudness_coeffs(DspLoudnessParams &, uint32_t);
                dsp_compute_loudness_coeffs(s.loudness, cfg->sampleRate);
              } else if (s.type == DSP_BASS_ENHANCE) {
                if (doc["frequency"].is<float>()) s.bassEnhance.frequency = doc["frequency"].as<float>();
                if (doc["harmonicGainDb"].is<float>()) s.bassEnhance.harmonicGainDb = doc["harmonicGainDb"].as<float>();
                if (doc["mix"].is<float>()) s.bassEnhance.mix = doc["mix"].as<float>();
                if (doc["order"].is<int>()) s.bassEnhance.order = doc["order"].as<uint8_t>();
                extern void dsp_compute_bass_enhance_coeffs(DspBassEnhanceParams &, uint32_t);
                dsp_compute_bass_enhance_coeffs(s.bassEnhance, cfg->sampleRate);
              }
              if (!dsp_swap_config()) { appState.dspSwapFailures++; appState.lastDspSwapFailure = millis(); LOG_W("[WebSocket] Swap failed, staged for retry"); }
              extern void saveDspSettingsDebounced();
              saveDspSettingsDebounced();
              appState.markDspConfigDirty();
            }
          }
        }
        else if (msgType == "reorderDspStage") {
          int ch = doc["ch"] | -1;
          int from = doc["from"] | -1;
          int to = doc["to"] | -1;
          if (ch >= 0 && ch < DSP_MAX_CHANNELS && from >= 0 && to >= 0) {
            dsp_copy_active_to_inactive();
            DspState *cfg = dsp_get_inactive_config();
            int cnt = cfg->channels[ch].stageCount;
            if (from < cnt && to < cnt && from != to) {
              int order[DSP_MAX_STAGES];
              for (int i = 0; i < cnt; i++) order[i] = i;
              // Move 'from' to 'to' position
              int tmp = order[from];
              if (from < to) {
                for (int i = from; i < to; i++) order[i] = order[i+1];
              } else {
                for (int i = from; i > to; i--) order[i] = order[i-1];
              }
              order[to] = tmp;
              if (dsp_reorder_stages(ch, order, cnt)) {
                if (!dsp_swap_config()) { appState.dspSwapFailures++; appState.lastDspSwapFailure = millis(); LOG_W("[WebSocket] Swap failed, staged for retry"); }
                extern void saveDspSettingsDebounced();
                saveDspSettingsDebounced();
                appState.markDspConfigDirty();
                LOG_I("[WebSocket] DSP stage reordered ch=%d from=%d to=%d", ch, from, to);
              }
            }
          }
        }
        else if (msgType == "setDspChannelBypass") {
          int ch = doc["ch"] | -1;
          bool bypass = doc["bypass"] | false;
          if (ch >= 0 && ch < DSP_MAX_CHANNELS) {
            dsp_copy_active_to_inactive();
            DspState *cfg = dsp_get_inactive_config();
            cfg->channels[ch].bypass = bypass;
            if (!dsp_swap_config()) { appState.dspSwapFailures++; appState.lastDspSwapFailure = millis(); LOG_W("[WebSocket] Swap failed, staged for retry"); }
            extern void saveDspSettingsDebounced();
            saveDspSettingsDebounced();
            appState.markDspConfigDirty();
          }
        }
        else if (msgType == "setDspStereoLink") {
          int pair = doc["pair"] | -1;
          bool linked = doc["linked"] | true;
          if (pair >= 0 && pair <= 1) {
            dsp_copy_active_to_inactive();
            DspState *cfg = dsp_get_inactive_config();
            int chA = pair * 2;
            int chB = pair * 2 + 1;
            cfg->channels[chA].stereoLink = linked;
            cfg->channels[chB].stereoLink = linked;
            if (linked) dsp_mirror_channel_config(chA, chB);
            if (!dsp_swap_config()) { appState.dspSwapFailures++; appState.lastDspSwapFailure = millis(); LOG_W("[WebSocket] Swap failed, staged for retry"); }
            extern void saveDspSettingsDebounced();
            saveDspSettingsDebounced();
            appState.markDspConfigDirty();
          }
        }
        // ===== PEQ Band Handlers =====
        else if (msgType == "updatePeqBand") {
          int ch = doc["ch"] | -1;
          int band = doc["band"] | -1;
          if (ch >= 0 && ch < DSP_MAX_CHANNELS && band >= 0 && band < DSP_PEQ_BANDS) {
            dsp_copy_active_to_inactive();
            DspState *cfg = dsp_get_inactive_config();
            if (band < cfg->channels[ch].stageCount) {
              DspStage &s = cfg->channels[ch].stages[band];
              if (doc["freq"].is<float>()) s.biquad.frequency = doc["freq"].as<float>();
              if (doc["gain"].is<float>()) s.biquad.gain = doc["gain"].as<float>();
              if (doc["Q"].is<float>()) s.biquad.Q = doc["Q"].as<float>();
              if (doc["enabled"].is<bool>()) s.enabled = doc["enabled"].as<bool>();
              if (doc["filterType"].is<int>()) {
                int ft = doc["filterType"].as<int>();
                if (ft >= 0 && ft < DSP_STAGE_TYPE_COUNT && dsp_is_biquad_type((DspStageType)ft)) {
                  s.type = (DspStageType)ft;
                }
              } else if (doc["filterType"].is<const char *>()) {
                const char *ft = doc["filterType"].as<const char *>();
                DspStageType newType = DSP_BIQUAD_PEQ;
                if (strcmp(ft, "PEQ") == 0) newType = DSP_BIQUAD_PEQ;
                else if (strcmp(ft, "LOW_SHELF") == 0) newType = DSP_BIQUAD_LOW_SHELF;
                else if (strcmp(ft, "HIGH_SHELF") == 0) newType = DSP_BIQUAD_HIGH_SHELF;
                else if (strcmp(ft, "NOTCH") == 0) newType = DSP_BIQUAD_NOTCH;
                else if (strcmp(ft, "BPF") == 0) newType = DSP_BIQUAD_BPF;
                else if (strcmp(ft, "LPF") == 0) newType = DSP_BIQUAD_LPF;
                else if (strcmp(ft, "HPF") == 0) newType = DSP_BIQUAD_HPF;
                else if (strcmp(ft, "ALLPASS") == 0) newType = DSP_BIQUAD_ALLPASS;
                s.type = newType;
              }
              if (doc["coeffs"].is<JsonArray>() && s.type == DSP_BIQUAD_CUSTOM) {
                JsonArray co = doc["coeffs"].as<JsonArray>();
                for (int j = 0; j < 5 && j < (int)co.size(); j++)
                  s.biquad.coeffs[j] = co[j].as<float>();
              } else {
                dsp_compute_biquad_coeffs(s.biquad, s.type, cfg->sampleRate);
              }
              // Auto-mirror PEQ to linked partner (preserve delay lines — zeroing causes pops)
              int partner = dsp_get_linked_partner(ch);
              if (partner >= 0 && band < cfg->channels[partner].stageCount) {
                // Copy everything except delay lines (biquad state)
                float savedDelay0 = cfg->channels[partner].stages[band].biquad.delay[0];
                float savedDelay1 = cfg->channels[partner].stages[band].biquad.delay[1];
                cfg->channels[partner].stages[band] = cfg->channels[ch].stages[band];
                cfg->channels[partner].stages[band].biquad.delay[0] = savedDelay0;
                cfg->channels[partner].stages[band].biquad.delay[1] = savedDelay1;
              }
              if (!dsp_swap_config()) { appState.dspSwapFailures++; appState.lastDspSwapFailure = millis(); LOG_W("[WebSocket] Swap failed, staged for retry"); }
              extern void saveDspSettingsDebounced();
              saveDspSettingsDebounced();
              appState.markDspConfigDirty();
            }
          }
        }
        else if (msgType == "setPeqBandEnabled") {
          int ch = doc["ch"] | -1;
          int band = doc["band"] | -1;
          bool en = doc["enabled"] | true;
          if (ch >= 0 && ch < DSP_MAX_CHANNELS && band >= 0 && band < DSP_PEQ_BANDS) {
            dsp_copy_active_to_inactive();
            DspState *cfg = dsp_get_inactive_config();
            if (band < cfg->channels[ch].stageCount) {
              cfg->channels[ch].stages[band].enabled = en;
              if (!dsp_swap_config()) { appState.dspSwapFailures++; appState.lastDspSwapFailure = millis(); LOG_W("[WebSocket] Swap failed, staged for retry"); }
              extern void saveDspSettingsDebounced();
              saveDspSettingsDebounced();
              appState.markDspConfigDirty();
            }
          }
        }
        else if (msgType == "setPeqAllEnabled") {
          int ch = doc["ch"] | -1;
          bool en = doc["enabled"] | true;
          if (ch >= 0 && ch < DSP_MAX_CHANNELS) {
            dsp_copy_active_to_inactive();
            DspState *cfg = dsp_get_inactive_config();
            int limit = cfg->channels[ch].stageCount < DSP_PEQ_BANDS ? cfg->channels[ch].stageCount : DSP_PEQ_BANDS;
            for (int b = 0; b < limit; b++) {
              cfg->channels[ch].stages[b].enabled = en;
            }
            if (!dsp_swap_config()) { appState.dspSwapFailures++; appState.lastDspSwapFailure = millis(); LOG_W("[WebSocket] Swap failed, staged for retry"); }
            extern void saveDspSettingsDebounced();
            saveDspSettingsDebounced();
            appState.markDspConfigDirty();
          }
        }
        else if (msgType == "copyPeqChannel") {
          int from = doc["from"] | -1;
          int to = doc["to"] | -1;
          if (from >= 0 && from < DSP_MAX_CHANNELS && to >= 0 && to < DSP_MAX_CHANNELS && from != to) {
            dsp_copy_active_to_inactive();
            dsp_copy_peq_bands(from, to);
            if (!dsp_swap_config()) { appState.dspSwapFailures++; appState.lastDspSwapFailure = millis(); LOG_W("[WebSocket] Swap failed, staged for retry"); }
            extern void saveDspSettingsDebounced();
            saveDspSettingsDebounced();
            appState.markDspConfigDirty();
            LOG_I("[WebSocket] PEQ bands copied ch%d -> ch%d", from, to);
          }
        }
        else if (msgType == "copyChainStages") {
          int from = doc["from"] | -1;
          int to = doc["to"] | -1;
          if (from >= 0 && from < DSP_MAX_CHANNELS && to >= 0 && to < DSP_MAX_CHANNELS && from != to) {
            dsp_copy_active_to_inactive();
            dsp_copy_chain_stages(from, to);
            if (!dsp_swap_config()) { appState.dspSwapFailures++; appState.lastDspSwapFailure = millis(); LOG_W("[WebSocket] Swap failed, staged for retry"); }
            extern void saveDspSettingsDebounced();
            saveDspSettingsDebounced();
            appState.markDspConfigDirty();
            LOG_I("[WebSocket] Chain stages copied ch%d -> ch%d", from, to);
          }
        }
        else if (msgType == "savePeqPreset") {
          const char *name = doc["name"] | (const char *)nullptr;
          int ch = doc["ch"] | 0;
          if (name && strlen(name) > 0 && strlen(name) <= 20 && ch >= 0 && ch < DSP_MAX_CHANNELS) {
            char safeName[24];
            int j = 0;
            for (int i = 0; name[i] && j < 20; i++) {
              char c = name[i];
              if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')
                safeName[j++] = c;
            }
            safeName[j] = '\0';
            if (j > 0) {
              char path[40];
              snprintf(path, sizeof(path), "/peq_%s.json", safeName);
              JsonDocument preset;
              preset["name"] = safeName;
              JsonArray bands = preset["bands"].to<JsonArray>();
              DspState *cfg = dsp_get_active_config();
              for (int b = 0; b < DSP_PEQ_BANDS && b < cfg->channels[ch].stageCount; b++) {
                const DspStage &s = cfg->channels[ch].stages[b];
                JsonObject band = bands.add<JsonObject>();
                band["type"] = (int)s.type;
                band["freq"] = s.biquad.frequency;
                band["gain"] = s.biquad.gain;
                band["Q"] = s.biquad.Q;
                band["enabled"] = s.enabled;
              }
              File f = LittleFS.open(path, "w");
              if (f) {
                if (_wsBuf) {
                  size_t len = serializeJson(preset, _wsBuf, WS_BUF_SIZE);
                  f.write((const uint8_t*)_wsBuf, len);
                } else {
                  serializeJson(preset, f);
                }
                f.close();
              }

              // Respond with success
              JsonDocument resp;
              resp["type"] = "peqPresetSaved";
              resp["name"] = safeName;
              char buf[64];
              serializeJson(resp, buf, sizeof(buf));
              webSocket.sendTXT(num, buf);
              LOG_I("[WebSocket] PEQ preset saved: %s", safeName);
            }
          }
        }
        else if (msgType == "loadPeqPreset") {
          const char *name = doc["name"] | (const char *)nullptr;
          int ch = doc["ch"] | 0;
          if (name && ch >= 0 && ch < DSP_MAX_CHANNELS) {
            char path[40];
            snprintf(path, sizeof(path), "/peq_%s.json", name);
            File f = LittleFS.open(path, "r");
            if (f && f.size() > 0) {
              JsonDocument preset;
              DeserializationError err = deserializeJson(preset, f);
              f.close();
              if (!err && preset["bands"].is<JsonArray>()) {
                dsp_copy_active_to_inactive();
                DspState *cfg = dsp_get_inactive_config();
                JsonArray bands = preset["bands"].as<JsonArray>();
                int b = 0;
                for (JsonObject band : bands) {
                  if (b >= DSP_PEQ_BANDS || b >= cfg->channels[ch].stageCount) break;
                  DspStage &s = cfg->channels[ch].stages[b];
                  if (band["type"].is<int>()) s.type = (DspStageType)band["type"].as<int>();
                  if (band["freq"].is<float>()) s.biquad.frequency = band["freq"].as<float>();
                  if (band["gain"].is<float>()) s.biquad.gain = band["gain"].as<float>();
                  if (band["Q"].is<float>()) s.biquad.Q = band["Q"].as<float>();
                  if (band["enabled"].is<bool>()) s.enabled = band["enabled"].as<bool>();
                  dsp_compute_biquad_coeffs(s.biquad, s.type, cfg->sampleRate);
                  b++;
                }
                if (!dsp_swap_config()) { appState.dspSwapFailures++; appState.lastDspSwapFailure = millis(); LOG_W("[WebSocket] Swap failed, staged for retry"); }
                extern void saveDspSettingsDebounced();
                saveDspSettingsDebounced();
                appState.markDspConfigDirty();
                LOG_I("[WebSocket] PEQ preset loaded: %s to ch%d", name, ch);
              }
            } else {
              if (f) f.close();
            }
          }
        }
        else if (msgType == "deletePeqPreset") {
          const char *name = doc["name"] | (const char *)nullptr;
          if (name) {
            char path[40];
            snprintf(path, sizeof(path), "/peq_%s.json", name);
            LittleFS.remove(path);
            LOG_I("[WebSocket] PEQ preset deleted: %s", name);
          }
        }
        else if (msgType == "listPeqPresets") {
          JsonDocument resp;
          resp["type"] = "peqPresets";
          JsonArray names = resp["presets"].to<JsonArray>();
          File root = LittleFS.open("/");
          if (root && root.isDirectory()) {
            File f = root.openNextFile();
            while (f) {
              String fname = f.name();
              if (fname.startsWith("/")) fname = fname.substring(1);
              if (fname.startsWith("peq_") && fname.endsWith(".json")) {
                names.add(fname.substring(4, fname.length() - 5));
              }
              f = root.openNextFile();
            }
          }
          wsSendJson(num, resp);
        }
        // ===== DSP Config Preset Commands =====
        else if (msgType == "saveDspPreset") {
          int slot = doc["slot"] | -1;
          const char *name = doc["name"] | "";
          if ((slot >= -1) && slot < DSP_PRESET_MAX_SLOTS) {
            extern bool dsp_preset_save(int, const char*);
            if (dsp_preset_save(slot, name)) {
              sendDspState();
              LOG_I("[WebSocket] DSP preset saved: slot=%d name=%s", slot, name);
            }
          }
        }
        else if (msgType == "loadDspPreset") {
          int slot = doc["slot"] | -1;
          if (slot >= 0 && slot < DSP_PRESET_MAX_SLOTS) {
            extern bool dsp_preset_load(int);
            if (dsp_preset_load(slot)) {
              sendDspState();
              LOG_I("[WebSocket] DSP preset loaded: slot=%d", slot);
            }
          }
        }
        else if (msgType == "deleteDspPreset") {
          int slot = doc["slot"] | -1;
          if (slot >= 0 && slot < DSP_PRESET_MAX_SLOTS) {
            extern bool dsp_preset_delete(int);
            dsp_preset_delete(slot);
            extern void saveDspSettings();
            saveDspSettings();
            sendDspState();
            LOG_I("[WebSocket] DSP preset deleted: slot=%d", slot);
          }
        }
        else if (msgType == "renameDspPreset") {
          int slot = doc["slot"] | -1;
          const char *name = doc["name"] | "";
          if (slot >= 0 && slot < DSP_PRESET_MAX_SLOTS && strlen(name) > 0) {
            extern bool dsp_preset_rename(int, const char*);
            if (dsp_preset_rename(slot, name)) {
              sendDspState();
              LOG_I("[WebSocket] DSP preset renamed: slot=%d name=%s", slot, name);
            }
          }
        }
        // measureDelayAlignment and applyDelayAlignment removed in v1.8.3 - incomplete feature
        else if (msgType == "applyBaffleStep") {
          int ch = doc["ch"] | 0;
          float widthMm = doc["baffleWidthMm"] | 250.0f;
          if (ch >= 0 && ch < DSP_MAX_CHANNELS) {
            BaffleStepResult bsr = dsp_baffle_step_correction(widthMm);
            dsp_copy_active_to_inactive();
            int idx = dsp_add_stage(ch, DSP_BIQUAD_HIGH_SHELF);
            if (idx >= 0) {
              DspState *cfg = dsp_get_inactive_config();
              cfg->channels[ch].stages[idx].biquad.frequency = bsr.frequency;
              cfg->channels[ch].stages[idx].biquad.gain = bsr.gainDb;
              cfg->channels[ch].stages[idx].biquad.Q = 0.707f;
              dsp_compute_biquad_coeffs(cfg->channels[ch].stages[idx].biquad, DSP_BIQUAD_HIGH_SHELF, cfg->sampleRate);
              if (!dsp_swap_config()) { appState.dspSwapFailures++; appState.lastDspSwapFailure = millis(); LOG_W("[WebSocket] Swap failed, staged for retry"); }
              extern void saveDspSettingsDebounced();
              saveDspSettingsDebounced();
              appState.markDspConfigDirty();
              LOG_I("[WebSocket] Baffle step: ch=%d width=%.0fmm freq=%.0fHz gain=%.1fdB", ch, widthMm, bsr.frequency, bsr.gainDb);
            }
          }
        }
#endif
#ifdef DAC_ENABLED
        else if (msgType == "setDacEnabled") {
          appState.dacEnabled = doc["enabled"].as<bool>();
          dac_save_settings();  // Save BEFORE init so dac_output_init() loads correct value
          if (appState.dacEnabled && !appState.dacReady) {
            dac_output_init();
          } else if (!appState.dacEnabled) {
            dac_output_deinit();
          }
          appState.markDacDirty();
          LOG_I("[WebSocket] DAC %s", appState.dacEnabled ? "enabled" : "disabled");
        }
        else if (msgType == "setDacVolume") {
          int v = doc["volume"].as<int>();
          if (v >= 0 && v <= 100) {
            appState.dacVolume = (uint8_t)v;
            dac_update_volume(appState.dacVolume);
            dac_save_settings();
            appState.markDacDirty();
          }
        }
        else if (msgType == "setDacMute") {
          bool wasMuted = appState.dacMute;
          appState.dacMute = doc["mute"].as<bool>();
          DacDriver *drv = dac_get_driver();
          if (drv) drv->setMute(appState.dacMute);
          dac_save_settings();
          appState.markDacDirty();
          if (wasMuted != appState.dacMute) {
            LOG_I("[DAC] Mute: %s -> %s", wasMuted ? "ON" : "OFF", appState.dacMute ? "ON" : "OFF");
          }
        }
        else if (msgType == "setDacFilter") {
          uint8_t prevFilter = appState.dacFilterMode;
          int fm = doc["filterMode"].as<int>();
          appState.dacFilterMode = (uint8_t)fm;
          DacDriver *drv = dac_get_driver();
          if (drv) drv->setFilterMode(appState.dacFilterMode);
          dac_save_settings();
          appState.markDacDirty();
          LOG_I("[DAC] Filter mode: %d -> %d", prevFilter, appState.dacFilterMode);
        }
        else if (msgType == "eepromScan") {
          LOG_I("[WebSocket] EEPROM scan requested");
          AppState::EepromDiag& ed = appState.eepromDiag;
          uint8_t eepMask = 0;
          ed.i2cTotalDevices = dac_i2c_scan(&eepMask);
          ed.i2cDevicesMask = eepMask;
          ed.scanned = true;
          ed.lastScanMs = millis();
          DacEepromData eepData;
          if (dac_eeprom_scan(&eepData, eepMask)) {
            ed.found = true;
            ed.eepromAddr = eepData.i2cAddress;
            ed.deviceId = eepData.deviceId;
            ed.hwRevision = eepData.hwRevision;
            strncpy(ed.deviceName, eepData.deviceName, 32);
            ed.deviceName[32] = '\0';
            strncpy(ed.manufacturer, eepData.manufacturer, 32);
            ed.manufacturer[32] = '\0';
            ed.maxChannels = eepData.maxChannels;
            ed.dacI2cAddress = eepData.dacI2cAddress;
            ed.flags = eepData.flags;
            ed.numSampleRates = eepData.numSampleRates;
            for (int i = 0; i < eepData.numSampleRates && i < 4; i++)
              ed.sampleRates[i] = eepData.sampleRates[i];
          } else {
            ed.found = false;
            ed.eepromAddr = 0;
            memset(ed.deviceName, 0, sizeof(ed.deviceName));
            memset(ed.manufacturer, 0, sizeof(ed.manufacturer));
            ed.deviceId = 0;
          }
          appState.markEepromDirty();
        }
        else if (msgType == "eepromProgram") {
          LOG_I("[WebSocket] EEPROM program requested");
          DacEepromData eepData;
          memset(&eepData, 0, sizeof(eepData));
          eepData.deviceId = (uint16_t)doc["deviceId"].as<int>();
          eepData.hwRevision = (uint8_t)doc["hwRevision"].as<int>();
          eepData.maxChannels = (uint8_t)doc["maxChannels"].as<int>();
          eepData.dacI2cAddress = (uint8_t)doc["dacI2cAddress"].as<int>();
          const char* eName = doc["deviceName"] | "";
          strncpy(eepData.deviceName, eName, 32);
          eepData.deviceName[32] = '\0';
          const char* eMfr = doc["manufacturer"] | "";
          strncpy(eepData.manufacturer, eMfr, 32);
          eepData.manufacturer[32] = '\0';
          uint8_t eFlags = 0;
          if (doc["independentClock"].as<bool>()) eFlags |= DAC_FLAG_INDEPENDENT_CLOCK;
          if (doc["hwVolume"].as<bool>()) eFlags |= DAC_FLAG_HW_VOLUME;
          if (doc["filters"].as<bool>()) eFlags |= DAC_FLAG_FILTERS;
          eepData.flags = eFlags;
          JsonArray rArr = doc["sampleRates"].as<JsonArray>();
          if (rArr) {
            int cnt = 0;
            for (JsonVariant v : rArr) {
              if (cnt >= DAC_EEPROM_MAX_RATES) break;
              eepData.sampleRates[cnt++] = v.as<uint32_t>();
            }
            eepData.numSampleRates = cnt;
          }
          uint8_t tAddr = (uint8_t)doc["address"].as<int>();
          if (tAddr < DAC_EEPROM_ADDR_START || tAddr > DAC_EEPROM_ADDR_END) tAddr = DAC_EEPROM_ADDR_START;

          uint8_t buf[DAC_EEPROM_DATA_SIZE];
          int sz = dac_eeprom_serialize(&eepData, buf, sizeof(buf));
          bool ok = (sz > 0) && dac_eeprom_write(tAddr, buf, sz);
          if (!ok) appState.eepromDiag.writeErrors++;

          // Re-scan (use cached mask from prior scan)
          DacEepromData scanned;
          AppState::EepromDiag& ed = appState.eepromDiag;
          if (dac_eeprom_scan(&scanned, ed.i2cDevicesMask)) {
            ed.found = true;
            ed.eepromAddr = scanned.i2cAddress;
            ed.deviceId = scanned.deviceId;
            ed.hwRevision = scanned.hwRevision;
            strncpy(ed.deviceName, scanned.deviceName, 32);
            ed.deviceName[32] = '\0';
            strncpy(ed.manufacturer, scanned.manufacturer, 32);
            ed.manufacturer[32] = '\0';
            ed.maxChannels = scanned.maxChannels;
            ed.dacI2cAddress = scanned.dacI2cAddress;
            ed.flags = scanned.flags;
            ed.numSampleRates = scanned.numSampleRates;
            for (int i = 0; i < scanned.numSampleRates && i < 4; i++)
              ed.sampleRates[i] = scanned.sampleRates[i];
          }
          ed.lastScanMs = millis();
          appState.markEepromDirty();

          // Send result to requesting client
          JsonDocument resp;
          resp["type"] = "eepromProgramResult";
          resp["success"] = ok;
          String rJson;
          serializeJson(resp, rJson);
          webSocket.sendTXT(num, rJson.c_str());
        }
        else if (msgType == "eepromErase") {
          LOG_I("[WebSocket] EEPROM erase requested");
          uint8_t tAddr = appState.eepromDiag.eepromAddr;
          if (doc["address"].is<int>()) tAddr = (uint8_t)doc["address"].as<int>();
          if (tAddr < DAC_EEPROM_ADDR_START || tAddr > DAC_EEPROM_ADDR_END) tAddr = DAC_EEPROM_ADDR_START;

          bool ok = dac_eeprom_erase(tAddr);
          if (!ok) appState.eepromDiag.writeErrors++;

          AppState::EepromDiag& ed = appState.eepromDiag;
          ed.found = false;
          ed.eepromAddr = 0;
          memset(ed.deviceName, 0, sizeof(ed.deviceName));
          memset(ed.manufacturer, 0, sizeof(ed.manufacturer));
          ed.deviceId = 0;
          ed.hwRevision = 0;
          ed.maxChannels = 0;
          ed.dacI2cAddress = 0;
          ed.flags = 0;
          ed.numSampleRates = 0;
          memset(ed.sampleRates, 0, sizeof(ed.sampleRates));
          ed.lastScanMs = millis();
          appState.markEepromDirty();

          JsonDocument resp;
          resp["type"] = "eepromEraseResult";
          resp["success"] = ok;
          String rJson;
          serializeJson(resp, rJson);
          webSocket.sendTXT(num, rJson.c_str());
        }
#endif
        // ===== Per-ADC Enable/Disable =====
        else if (msgType == "setAdcEnabled") {
          int adc = doc["adc"] | -1;
          bool newVal = doc["enabled"].as<bool>();
          if (adc >= 0 && adc < NUM_AUDIO_INPUTS && newVal != appState.adcEnabled[adc]) {
            appState.adcEnabled[adc] = newVal;
            appState.markAdcEnabledDirty();
            saveSettings();
            // Broadcast new state to all clients
            JsonDocument resp;
            resp["type"] = "adcState";
            JsonArray arr = resp["enabled"].to<JsonArray>();
            for (int i = 0; i < NUM_AUDIO_INPUTS; i++) arr.add(appState.adcEnabled[i]);
            String rJson;
            serializeJson(resp, rJson);
            webSocket.broadcastTXT((uint8_t*)rJson.c_str(), rJson.length());
            LOG_I("[WebSocket] %s %s", audioInputLabel(adc), newVal ? "enabled" : "disabled");
          }
        }
#ifdef USB_AUDIO_ENABLED
        // ===== USB Audio Enable/Disable =====
        else if (msgType == "setUsbAudioEnabled") {
          bool newVal = doc["enabled"].as<bool>();
          if (newVal != appState.usbAudioEnabled) {
            appState.usbAudioEnabled = newVal;
            saveSettings();
            if (newVal) {
              usb_audio_init();
            } else {
              usb_audio_deinit();
            }
            appState.markUsbAudioDirty();
            LOG_I("[WebSocket] USB Audio %s", newVal ? "enabled" : "disabled");
          }
        }
#endif
      }
      break;

    default:
      break;
  }
}

// ===== State Broadcasting Functions =====

void sendDisplayState() {
  JsonDocument doc;
  doc["type"] = "displayState";
  doc["backlightOn"] = AppState::getInstance().backlightOn;
  doc["screenTimeout"] = AppState::getInstance().screenTimeout / 1000; // Send as seconds
  doc["backlightBrightness"] = AppState::getInstance().backlightBrightness;
  doc["dimEnabled"] = AppState::getInstance().dimEnabled;
  doc["dimTimeout"] = AppState::getInstance().dimTimeout / 1000;
  doc["dimBrightness"] = AppState::getInstance().dimBrightness;
  wsBroadcastJson(doc);
}

void sendFactoryResetProgress(unsigned long secondsHeld, bool resetTriggered) {
  JsonDocument doc;
  doc["type"] = "factoryResetProgress";
  doc["secondsHeld"] = secondsHeld;
  doc["secondsRequired"] = BTN_VERY_LONG_PRESS_MIN / 1000;
  doc["resetTriggered"] = resetTriggered;
  doc["progress"] = (secondsHeld * 100) / (BTN_VERY_LONG_PRESS_MIN / 1000);
  wsBroadcastJson(doc);
}

void sendRebootProgress(unsigned long secondsHeld, bool rebootTriggered) {
  JsonDocument doc;
  doc["type"] = "rebootProgress";
  doc["secondsHeld"] = secondsHeld;
  doc["secondsRequired"] = BTN_VERY_LONG_PRESS_MIN / 1000;
  doc["rebootTriggered"] = rebootTriggered;
  doc["progress"] = (secondsHeld * 100) / (BTN_VERY_LONG_PRESS_MIN / 1000);
  wsBroadcastJson(doc);
}

void sendBuzzerState() {
  JsonDocument doc;
  doc["type"] = "buzzerState";
  doc["enabled"] = AppState::getInstance().buzzerEnabled;
  doc["volume"] = AppState::getInstance().buzzerVolume;
  wsBroadcastJson(doc);
}

void sendSignalGenState() {
  JsonDocument doc;
  doc["type"] = "signalGenerator";
  doc["enabled"] = appState.sigGenEnabled;
  doc["waveform"] = appState.sigGenWaveform;
  doc["frequency"] = appState.sigGenFrequency;
  doc["amplitude"] = appState.sigGenAmplitude;
  doc["channel"] = appState.sigGenChannel;
  doc["outputMode"] = appState.sigGenOutputMode;
  doc["sweepSpeed"] = appState.sigGenSweepSpeed;
  doc["targetAdc"] = appState.sigGenTargetAdc;
  wsBroadcastJson(doc);
}

void sendAudioGraphState() {
  JsonDocument doc;
  doc["type"] = "audioGraphState";
  doc["vuMeterEnabled"] = appState.vuMeterEnabled;
  doc["waveformEnabled"] = appState.waveformEnabled;
  doc["spectrumEnabled"] = appState.spectrumEnabled;
  doc["fftWindowType"] = (int)appState.fftWindowType;
  wsBroadcastJson(doc);
}

void sendDebugState() {
  JsonDocument doc;
  doc["type"] = "debugState";
  doc["debugMode"] = appState.debugMode;
  doc["debugSerialLevel"] = appState.debugSerialLevel;
  doc["debugHwStats"] = appState.debugHwStats;
  doc["debugI2sMetrics"] = appState.debugI2sMetrics;
  doc["debugTaskMonitor"] = appState.debugTaskMonitor;
  wsBroadcastJson(doc);
}

#ifdef DSP_ENABLED
void sendDspState() {
  JsonDocument doc;
  doc["type"] = "dspState";
  doc["dspEnabled"] = appState.dspEnabled;
  doc["dspBypass"] = appState.dspBypass;
  doc["presetIndex"] = appState.dspPresetIndex;

  // Send preset list (index, name, exists)
  JsonArray presets = doc["presets"].to<JsonArray>();
  extern bool dsp_preset_exists(int);
  for (int i = 0; i < DSP_PRESET_MAX_SLOTS; i++) {
    JsonObject preset = presets.add<JsonObject>();
    preset["index"] = i;
    preset["name"] = appState.dspPresetNames[i];
    preset["exists"] = dsp_preset_exists(i);
  }

  DspState *cfg = dsp_get_active_config();
  doc["globalBypass"] = cfg->globalBypass;
  doc["sampleRate"] = cfg->sampleRate;
  JsonArray channels = doc["channels"].to<JsonArray>();
  for (int c = 0; c < DSP_MAX_CHANNELS; c++) {
    JsonObject ch = channels.add<JsonObject>();
    ch["bypass"] = cfg->channels[c].bypass;
    ch["stereoLink"] = cfg->channels[c].stereoLink;
    ch["stageCount"] = cfg->channels[c].stageCount;
    JsonArray stages = ch["stages"].to<JsonArray>();
    for (int s = 0; s < cfg->channels[c].stageCount; s++) {
      const DspStage &st = cfg->channels[c].stages[s];
      JsonObject so = stages.add<JsonObject>();
      so["enabled"] = st.enabled;
      so["type"] = (int)st.type;
      if (st.label[0]) so["label"] = st.label;
      if (dsp_is_biquad_type(st.type)) {
        so["freq"] = st.biquad.frequency;
        so["gain"] = st.biquad.gain;
        so["Q"] = st.biquad.Q;
        if (st.type == DSP_BIQUAD_LINKWITZ) so["Q2"] = st.biquad.Q2;
        // Only send coefficients for enabled stages (saves ~3KB for 40 disabled PEQ bands)
        if (st.enabled) {
          JsonArray co = so["coeffs"].to<JsonArray>();
          for (int j = 0; j < 5; j++) co.add(st.biquad.coeffs[j]);
        }
      } else if (st.type == DSP_LIMITER) {
        so["thresholdDb"] = st.limiter.thresholdDb;
        so["attackMs"] = st.limiter.attackMs;
        so["releaseMs"] = st.limiter.releaseMs;
        so["ratio"] = st.limiter.ratio;
        so["gr"] = st.limiter.gainReduction;
      } else if (st.type == DSP_GAIN) {
        so["gainDb"] = st.gain.gainDb;
      } else if (st.type == DSP_FIR) {
        so["numTaps"] = st.fir.numTaps;
      } else if (st.type == DSP_DELAY) {
        so["delaySamples"] = st.delay.delaySamples;
      } else if (st.type == DSP_POLARITY) {
        so["inverted"] = st.polarity.inverted;
      } else if (st.type == DSP_MUTE) {
        so["muted"] = st.mute.muted;
      } else if (st.type == DSP_COMPRESSOR) {
        so["thresholdDb"] = st.compressor.thresholdDb;
        so["attackMs"] = st.compressor.attackMs;
        so["releaseMs"] = st.compressor.releaseMs;
        so["ratio"] = st.compressor.ratio;
        so["kneeDb"] = st.compressor.kneeDb;
        so["makeupGainDb"] = st.compressor.makeupGainDb;
        so["gr"] = st.compressor.gainReduction;
      } else if (st.type == DSP_NOISE_GATE) {
        so["thresholdDb"] = st.noiseGate.thresholdDb;
        so["attackMs"] = st.noiseGate.attackMs;
        so["holdMs"] = st.noiseGate.holdMs;
        so["releaseMs"] = st.noiseGate.releaseMs;
        so["ratio"] = st.noiseGate.ratio;
        so["rangeDb"] = st.noiseGate.rangeDb;
        so["gr"] = st.noiseGate.gainReduction;
      } else if (st.type == DSP_TONE_CTRL) {
        so["bassGain"] = st.toneCtrl.bassGain;
        so["midGain"] = st.toneCtrl.midGain;
        so["trebleGain"] = st.toneCtrl.trebleGain;
      } else if (st.type == DSP_SPEAKER_PROT) {
        so["powerRatingW"] = st.speakerProt.powerRatingW;
        so["impedanceOhms"] = st.speakerProt.impedanceOhms;
        so["thermalTauMs"] = st.speakerProt.thermalTauMs;
        so["excursionLimitMm"] = st.speakerProt.excursionLimitMm;
        so["driverDiameterMm"] = st.speakerProt.driverDiameterMm;
        so["maxTempC"] = st.speakerProt.maxTempC;
        so["currentTempC"] = st.speakerProt.currentTempC;
        so["gr"] = st.speakerProt.gainReduction;
      } else if (st.type == DSP_STEREO_WIDTH) {
        so["width"] = st.stereoWidth.width;
        so["centerGainDb"] = st.stereoWidth.centerGainDb;
      } else if (st.type == DSP_LOUDNESS) {
        so["referenceLevelDb"] = st.loudness.referenceLevelDb;
        so["currentLevelDb"] = st.loudness.currentLevelDb;
        so["amount"] = st.loudness.amount;
      } else if (st.type == DSP_BASS_ENHANCE) {
        so["frequency"] = st.bassEnhance.frequency;
        so["harmonicGainDb"] = st.bassEnhance.harmonicGainDb;
        so["mix"] = st.bassEnhance.mix;
        so["order"] = st.bassEnhance.order;
      } else if (st.type == DSP_MULTIBAND_COMP) {
        so["numBands"] = st.multibandComp.numBands;
      }
    }
  }
  wsBroadcastJson(doc);
}

void sendDspMetrics() {
  DspMetrics m = dsp_get_metrics();
  JsonDocument doc;
  doc["type"] = "dspMetrics";
  doc["processTimeUs"] = m.processTimeUs;
  doc["cpuLoad"] = m.cpuLoadPercent;
  JsonArray gr = doc["limiterGr"].to<JsonArray>();
  for (int i = 0; i < DSP_MAX_CHANNELS; i++) gr.add(m.limiterGrDb[i]);
  wsBroadcastJson(doc);
}
#endif

#ifdef DAC_ENABLED
void sendDacState() {
  JsonDocument doc;
  doc["type"] = "dacState";
  doc["enabled"] = appState.dacEnabled;
  doc["volume"] = appState.dacVolume;
  doc["mute"] = appState.dacMute;
  doc["deviceId"] = appState.dacDeviceId;
  doc["modelName"] = appState.dacModelName;
  doc["outputChannels"] = appState.dacOutputChannels;
  doc["detected"] = appState.dacDetected;
  doc["ready"] = appState.dacReady;
  doc["filterMode"] = appState.dacFilterMode;
  doc["txUnderruns"] = appState.dacTxUnderruns;
  // TX diagnostics snapshot
  {
    DacTxDiag txd = dac_get_tx_diagnostics();
    JsonObject tx = doc["tx"].to<JsonObject>();
    tx["i2sTxEnabled"] = txd.i2sTxEnabled;
    tx["volumeGain"] = serialized(String(txd.volumeGain, 4));
    tx["writeCount"] = txd.writeCount;
    tx["bytesWritten"] = txd.bytesWritten;
    tx["bytesExpected"] = txd.bytesExpected;
    tx["peakSample"] = txd.peakSample;
    tx["zeroFrames"] = txd.zeroFrames;
  }
  // Include available drivers
  JsonArray drivers = doc["drivers"].to<JsonArray>();
  const DacRegistryEntry* entries = dac_registry_get_entries();
  int count = dac_registry_get_count();
  for (int i = 0; i < count; i++) {
    JsonObject drv = drivers.add<JsonObject>();
    drv["id"] = entries[i].deviceId;
    drv["name"] = entries[i].name;
  }
  // Filter modes from current driver
  DacDriver* drv = dac_get_driver();
  if (drv && drv->getCapabilities().hasFilterModes) {
    JsonArray filters = doc["filterModes"].to<JsonArray>();
    for (uint8_t f = 0; f < drv->getCapabilities().numFilterModes; f++) {
      const char* name = drv->getFilterModeName(f);
      filters.add(name ? name : "Unknown");
    }
  }
  // EEPROM diagnostics
  {
    const AppState::EepromDiag& ed = appState.eepromDiag;
    JsonObject eep = doc["eeprom"].to<JsonObject>();
    eep["scanned"] = ed.scanned;
    eep["found"] = ed.found;
    eep["addr"] = ed.eepromAddr;
    eep["i2cDevices"] = ed.i2cTotalDevices;
    eep["i2cMask"] = ed.i2cDevicesMask;
    eep["readErrors"] = ed.readErrors;
    eep["writeErrors"] = ed.writeErrors;
    if (ed.found) {
      eep["deviceName"] = ed.deviceName;
      eep["manufacturer"] = ed.manufacturer;
      eep["deviceId"] = ed.deviceId;
      eep["hwRevision"] = ed.hwRevision;
      eep["maxChannels"] = ed.maxChannels;
      eep["dacI2cAddress"] = ed.dacI2cAddress;
      eep["flags"] = ed.flags;
      JsonArray rates = eep["sampleRates"].to<JsonArray>();
      for (int i = 0; i < ed.numSampleRates; i++) {
        rates.add(ed.sampleRates[i]);
      }
    }
  }
  wsBroadcastJson(doc);
}
#endif

#ifdef USB_AUDIO_ENABLED
void sendUsbAudioState() {
  JsonDocument doc;
  doc["type"] = "usbAudioState";
  doc["enabled"] = appState.usbAudioEnabled;
  doc["connected"] = appState.usbAudioConnected;
  doc["streaming"] = appState.usbAudioStreaming;
  doc["sampleRate"] = appState.usbAudioSampleRate;
  doc["bitDepth"] = appState.usbAudioBitDepth;
  doc["channels"] = appState.usbAudioChannels;
  doc["volume"] = appState.usbAudioVolume;
  doc["volumeLinear"] = usb_audio_get_volume_linear();
  doc["mute"] = appState.usbAudioMute;
  doc["overruns"] = appState.usbAudioBufferOverruns;
  doc["underruns"] = appState.usbAudioBufferUnderruns;
  doc["bufferLevel"] = usb_audio_get_buffer_fill();
  doc["framesAvailable"] = usb_audio_available_frames();
  doc["bufferCapacity"] = 1024;
  wsBroadcastJson(doc);
}
#endif

void sendMqttSettingsState() {
  JsonDocument doc;
  doc["type"] = "mqttSettings";
  doc["enabled"] = appState.mqttEnabled;
  doc["broker"] = appState.mqttBroker;
  doc["port"] = appState.mqttPort;
  doc["username"] = appState.mqttUsername;
  doc["hasPassword"] = (strlen(appState.mqttPassword) > 0);
  doc["baseTopic"] = appState.mqttBaseTopic;
  doc["haDiscovery"] = appState.mqttHADiscovery;
  doc["connected"] = appState.mqttConnected;
  wsBroadcastJson(doc);
}

// ===== CPU Utilization Functions =====

void initCpuUsageMonitoring() {
  if (!cpuHooksInstalled) {
    esp_register_freertos_idle_hook_for_cpu(idleHookCore0, 0);
    esp_register_freertos_idle_hook_for_cpu(idleHookCore1, 1);
    cpuHooksInstalled = true;
    cpuWarmupCycles = 0;
    idleTimeUs0 = 0;
    idleTimeUs1 = 0;
    idleEntryUs0 = 0;
    idleEntryUs1 = 0;
    lastIdleTimeUs0 = 0;
    lastIdleTimeUs1 = 0;
    lastCpuMeasureTimeUs = esp_timer_get_time();
    cpuUsageCore0 = -1.0f;
    cpuUsageCore1 = -1.0f;
  }
}

void deinitCpuUsageMonitoring() {
  if (cpuHooksInstalled) {
    esp_deregister_freertos_idle_hook_for_cpu(idleHookCore0, 0);
    esp_deregister_freertos_idle_hook_for_cpu(idleHookCore1, 1);
    cpuHooksInstalled = false;
    cpuUsageCore0 = -1.0f;
    cpuUsageCore1 = -1.0f;
  }
}

void updateCpuUsage() {
  if (!cpuHooksInstalled) {
    initCpuUsageMonitoring();
    return;
  }

  int64_t nowUs = esp_timer_get_time();
  int64_t elapsedUs = nowUs - lastCpuMeasureTimeUs;

  // Only update every 2 seconds for stable readings
  if (elapsedUs < 2000000) return;

  // Snapshot idle accumulations (volatile reads)
  int64_t curIdle0 = idleTimeUs0;
  int64_t curIdle1 = idleTimeUs1;

  // Delta idle microseconds since last measurement
  int64_t deltaIdle0 = curIdle0 - lastIdleTimeUs0;
  int64_t deltaIdle1 = curIdle1 - lastIdleTimeUs1;

  lastIdleTimeUs0 = curIdle0;
  lastIdleTimeUs1 = curIdle1;
  lastCpuMeasureTimeUs = nowUs;

  // Skip the first 2 cycles — hooks need time to accumulate stable data
  if (cpuWarmupCycles < 2) {
    cpuWarmupCycles++;
    cpuUsageCore0 = -1.0f;
    cpuUsageCore1 = -1.0f;
    return;
  }

  // CPU = 100% - (idle_time / total_time * 100%)
  // idle_time is actual microseconds the idle task ran (not counting ISR time)
  // total_time is wall-clock elapsed microseconds
  if (elapsedUs > 0) {
    float idlePct0 = (float)deltaIdle0 / (float)elapsedUs * 100.0f;
    float idlePct1 = (float)deltaIdle1 / (float)elapsedUs * 100.0f;
    cpuUsageCore0 = 100.0f - idlePct0;
    cpuUsageCore1 = 100.0f - idlePct1;

    // Clamp
    if (cpuUsageCore0 < 0) cpuUsageCore0 = 0;
    if (cpuUsageCore0 > 100) cpuUsageCore0 = 100;
    if (cpuUsageCore1 < 0) cpuUsageCore1 = 0;
    if (cpuUsageCore1 > 100) cpuUsageCore1 = 100;
  }
}

float getCpuUsageCore0() {
  return cpuUsageCore0;
}

float getCpuUsageCore1() {
  return cpuUsageCore1;
}

void sendHardwareStats() {
  // Master debug gate — if debug mode is off, deregister hooks and send nothing
  if (!appState.debugMode) {
    if (cpuHooksInstalled) deinitCpuUsageMonitoring();
    return;
  }

  JsonDocument doc;
  doc["type"] = "hardware_stats";

  // === CPU stats — always included when debugMode is on ===
  updateCpuUsage();
  doc["cpu"]["freqMHz"] = ESP.getCpuFreqMHz();
  doc["cpu"]["model"] = ESP.getChipModel();
  doc["cpu"]["revision"] = ESP.getChipRevision();
  doc["cpu"]["cores"] = ESP.getChipCores();
  // During warm-up, report -1 (UI shows "Calibrating...")
  bool cpuValid = (cpuUsageCore0 >= 0 && cpuUsageCore1 >= 0);
  doc["cpu"]["usageCore0"] = cpuValid ? cpuUsageCore0 : -1;
  doc["cpu"]["usageCore1"] = cpuValid ? cpuUsageCore1 : -1;
  doc["cpu"]["usageTotal"] = cpuValid ? (cpuUsageCore0 + cpuUsageCore1) / 2.0f : -1;
  // temperatureRead() uses SAR ADC spinlock which can deadlock with I2S ADC,
  // causing interrupt WDT on Core 1. Cache the value on a slow timer instead.
  {
    static float cachedTemp = 0.0f;
    static unsigned long lastTempRead = 0;
    unsigned long now = millis();
    if (now - lastTempRead > 10000 || lastTempRead == 0) {
      lastTempRead = now;
      cachedTemp = temperatureRead();
    }
    doc["cpu"]["temperature"] = cachedTemp;
  }

  // === Hardware Stats sections (gated by debugHwStats) ===
  if (appState.debugHwStats) {
    // Memory - Internal Heap
    doc["memory"]["heapTotal"] = ESP.getHeapSize();
    doc["memory"]["heapFree"] = ESP.getFreeHeap();
    doc["memory"]["heapMinFree"] = ESP.getMinFreeHeap();
    doc["memory"]["heapMaxBlock"] = ESP.getMaxAllocHeap();

    // Memory - PSRAM (external, may not be available)
    doc["memory"]["psramTotal"] = ESP.getPsramSize();
    doc["memory"]["psramFree"] = ESP.getFreePsram();

    // Storage - Flash
    doc["storage"]["flashSize"] = ESP.getFlashChipSize();
    doc["storage"]["sketchSize"] = ESP.getSketchSize();
    doc["storage"]["sketchFree"] = ESP.getFreeSketchSpace();

    // Storage - LittleFS
    doc["storage"]["LittleFSTotal"] = LittleFS.totalBytes();
    doc["storage"]["LittleFSUsed"] = LittleFS.usedBytes();

    // WiFi Information
    doc["wifi"]["rssi"] = WiFi.RSSI();
    doc["wifi"]["channel"] = WiFi.channel();
    doc["wifi"]["apClients"] = WiFi.softAPgetStationNum();
    doc["wifi"]["connected"] = (WiFi.status() == WL_CONNECTED);

    // Audio ADC diagnostics (per-ADC)
    doc["audio"]["sampleRate"] = appState.audioSampleRate;
    doc["audio"]["adcVref"] = appState.adcVref;
    doc["audio"]["numAdcsDetected"] = appState.numAdcsDetected;
    JsonArray adcArr = doc["audio"]["adcs"].to<JsonArray>();
    for (int a = 0; a < NUM_AUDIO_INPUTS; a++) {
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
      adcObj["status"] = statusStr;
      adcObj["noiseFloorDbfs"] = adc.noiseFloorDbfs;
      adcObj["i2sErrors"] = adc.i2sErrors;
      adcObj["consecutiveZeros"] = adc.consecutiveZeros;
      adcObj["totalBuffers"] = adc.totalBuffers;
      adcObj["vrms"] = adc.vrmsCombined;
      adcObj["snrDb"] = appState.audioSnrDb[a];
      adcObj["sfdrDb"] = appState.audioSfdrDb[a];
    }
    doc["audio"]["fftWindowType"] = (int)appState.fftWindowType;
    // ADC clock sync diagnostics
    doc["audio"]["syncOk"]            = appState.adcSyncOk;
    doc["audio"]["syncOffsetSamples"] = appState.adcSyncOffsetSamples;
    doc["audio"]["syncCorrelation"]   = appState.adcSyncCorrelation;
    // Legacy flat fields for backward compat
    doc["audio"]["adcStatus"] = adcArr[0]["status"];
    doc["audio"]["noiseFloorDbfs"] = appState.audioNoiseFloorDbfs;
    doc["audio"]["vrms"] = appState.audioVrmsCombined;

    // Uptime (milliseconds since boot)
    doc["uptime"] = millis();

    // Reset reason
    doc["resetReason"] = getResetReasonString();

    // Heap health
    doc["heapCritical"]     = appState.heapCritical;
    doc["heapWarning"]      = appState.heapWarning;
    doc["heapMaxBlockBytes"] = appState.heapMaxBlockBytes;
    doc["wifiRxWatchdogRecoveries"] = appState.wifiRxWatchdogRecoveries;

    // Crash history (ring buffer, most recent first)
    const CrashLogData &clog = crashlog_get();
    JsonArray crashArr = doc["crashHistory"].to<JsonArray>();
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

    // Per-ADC I2S recovery counts
    for (int a = 0; a < NUM_AUDIO_ADCS; a++) {
      doc["audio"]["adcs"][a]["i2sRecoveries"] = appState.audioAdc[a].i2sRecoveries;
    }

#ifdef DAC_ENABLED
    // DAC Output diagnostics
    {
      JsonObject dac = doc["dac"].to<JsonObject>();
      dac["enabled"] = appState.dacEnabled;
      dac["ready"] = appState.dacReady;
      dac["detected"] = appState.dacDetected;
      dac["model"] = appState.dacModelName;
      dac["deviceId"] = appState.dacDeviceId;
      dac["volume"] = appState.dacVolume;
      dac["mute"] = appState.dacMute;
      dac["filterMode"] = appState.dacFilterMode;
      dac["outputChannels"] = appState.dacOutputChannels;
      dac["txUnderruns"] = appState.dacTxUnderruns;
      DacDriver* drv = dac_get_driver();
      if (drv) {
        const DacCapabilities& caps = drv->getCapabilities();
        dac["manufacturer"] = caps.manufacturer;
        dac["hwVolume"] = caps.hasHardwareVolume;
        dac["i2cControl"] = caps.hasI2cControl;
        dac["independentClock"] = caps.needsIndependentClock;
        dac["hasFilters"] = caps.hasFilterModes;
      }
      // TX diagnostics snapshot
      {
        DacTxDiag txd = dac_get_tx_diagnostics();
        JsonObject tx = dac["tx"].to<JsonObject>();
        tx["i2sTxEnabled"] = txd.i2sTxEnabled;
        tx["volumeGain"] = serialized(String(txd.volumeGain, 4));
        tx["writeCount"] = txd.writeCount;
        tx["bytesWritten"] = txd.bytesWritten;
        tx["bytesExpected"] = txd.bytesExpected;
        tx["peakSample"] = txd.peakSample;
        tx["zeroFrames"] = txd.zeroFrames;
      }
      // EEPROM diagnostics
      const AppState::EepromDiag& ed = appState.eepromDiag;
      JsonObject eep = dac["eeprom"].to<JsonObject>();
      eep["scanned"] = ed.scanned;
      eep["found"] = ed.found;
      eep["addr"] = ed.eepromAddr;
      eep["i2cMask"] = ed.i2cDevicesMask;
      eep["i2cDevices"] = ed.i2cTotalDevices;
      eep["readErrors"] = ed.readErrors;
      eep["writeErrors"] = ed.writeErrors;
    }
#endif

#ifdef DSP_ENABLED
    // DSP diagnostics
    {
      JsonObject dsp = doc["dsp"].to<JsonObject>();
      dsp["swapFailures"] = appState.dspSwapFailures;
      dsp["swapSuccesses"] = appState.dspSwapSuccesses;
      unsigned long timeSinceFailure = appState.lastDspSwapFailure > 0 ? (millis() - appState.lastDspSwapFailure) : 0;
      dsp["lastSwapFailureAgo"] = timeSinceFailure;
    }
#endif
  }

  // === I2S Metrics sections (gated by debugI2sMetrics) ===
  if (appState.debugI2sMetrics) {
    // I2S Static Config
    I2sStaticConfig i2sCfg = i2s_audio_get_static_config();
    JsonArray i2sCfgArr = doc["audio"]["i2sConfig"].to<JsonArray>();
    for (int a = 0; a < NUM_AUDIO_ADCS; a++) {
      JsonObject c = i2sCfgArr.add<JsonObject>();
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

    // I2S Runtime Metrics
    JsonObject i2sRt = doc["audio"]["i2sRuntime"].to<JsonObject>();
    i2sRt["stackFree"] = appState.i2sMetrics.audioTaskStackFree;
    JsonArray bpsArr = i2sRt["buffersPerSec"].to<JsonArray>();
    JsonArray latArr = i2sRt["avgReadLatencyUs"].to<JsonArray>();
    for (int a = 0; a < NUM_AUDIO_ADCS; a++) {
      bpsArr.add(serialized(String(appState.i2sMetrics.buffersPerSec[a], 1)));
      latArr.add(serialized(String(appState.i2sMetrics.avgReadLatencyUs[a], 0)));
    }
  }

  // === Task Monitor section (gated by debugTaskMonitor) ===
  // Note: task_monitor_update() runs on its own 5s timer in main loop
  if (appState.debugTaskMonitor) {
    const TaskMonitorData& tm = task_monitor_get_data();
    doc["tasks"]["count"] = tm.taskCount;
    doc["tasks"]["loopUs"] = tm.loopTimeUs;
    doc["tasks"]["loopMaxUs"] = tm.loopTimeMaxUs;
    doc["tasks"]["loopAvgUs"] = tm.loopTimeAvgUs;
    JsonArray taskArr = doc["tasks"]["list"].to<JsonArray>();
    for (int i = 0; i < tm.taskCount; i++) {
      JsonObject t = taskArr.add<JsonObject>();
      t["name"] = tm.tasks[i].name;
      t["stackFree"] = tm.tasks[i].stackFreeBytes;
      t["stackAlloc"] = tm.tasks[i].stackAllocBytes;
      t["pri"] = tm.tasks[i].priority;
      t["state"] = tm.tasks[i].state;
      t["core"] = tm.tasks[i].coreId;
    }
  }

  // Broadcast to all WebSocket clients
  wsBroadcastJson(doc);
}

// ===== Audio Streaming to Subscribed Clients =====

void sendAudioData() {
  // Early return if no clients are subscribed
  bool anySubscribed = false;
  for (int i = 0; i < MAX_WS_CLIENTS; i++) {
    if (_audioSubscribed[i]) {
      anySubscribed = true;
      break;
    }
  }
  if (!anySubscribed) return;

  // --- Audio levels (VU, peak, RMS, diagnostics) ---
  {
    JsonDocument doc;
    doc["type"] = "audioLevels";
    doc["audioLevel"] = appState.audioLevel_dBFS;
    doc["signalDetected"] = (appState.audioLevel_dBFS >= appState.audioThreshold_dBFS);
    doc["numAdcsDetected"] = appState.numAdcsDetected;
    // Per-ADC data array
    JsonArray adcArr = doc["adc"].to<JsonArray>();
    JsonArray adcStatusArr = doc["adcStatus"].to<JsonArray>();
    JsonArray adcNoiseArr = doc["adcNoiseFloor"].to<JsonArray>();
    for (int a = 0; a < NUM_AUDIO_INPUTS; a++) {
      const AppState::AdcState &adc = appState.audioAdc[a];
      JsonObject adcObj = adcArr.add<JsonObject>();
      adcObj["vu1"] = adc.vu1;
      adcObj["vu2"] = adc.vu2;
      adcObj["peak1"] = adc.peak1;
      adcObj["peak2"] = adc.peak2;
      adcObj["rms1"] = adc.rms1;
      adcObj["rms2"] = adc.rms2;
      adcObj["vrms1"] = adc.vrms1;
      adcObj["vrms2"] = adc.vrms2;
      adcObj["dBFS"] = adc.dBFS;
      const char *statusStr = "OK";
      switch (adc.healthStatus) {
        case 1: statusStr = "NO_DATA"; break;
        case 2: statusStr = "NOISE_ONLY"; break;
        case 3: statusStr = "CLIPPING"; break;
        case 4: statusStr = "I2S_ERROR"; break;
        case 5: statusStr = "HW_FAULT"; break;
      }
      adcStatusArr.add(statusStr);
      adcNoiseArr.add(adc.noiseFloorDbfs);
    }
    // DAC output metering
    {
      JsonObject dacOut = doc["dacOutput"].to<JsonObject>();
      dacOut["vuL"] = appState.dacOutputVuL;
      dacOut["vuR"] = appState.dacOutputVuR;
      dacOut["dbfsL"] = appState.dacOutputDbfsL;
      dacOut["dbfsR"] = appState.dacOutputDbfsR;
      dacOut["peakL"] = appState.dacOutputPeakL;
      dacOut["peakR"] = appState.dacOutputPeakR;
    }
    // Legacy flat fields for backward compat (ADC 0)
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
    if (_wsBuf) {
      size_t len = serializeJson(doc, _wsBuf, WS_BUF_SIZE);
      for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (_audioSubscribed[i]) {
          webSocket.sendTXT(i, _wsBuf, len);
        }
      }
    } else {
      String json;
      serializeJson(doc, json);
      for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (_audioSubscribed[i]) {
          webSocket.sendTXT(i, (uint8_t*)json.c_str(), json.length());
        }
      }
    }
  }

  // --- Waveform data (per-ADC) — binary: [type:1][adc:1][samples:256] ---
  if (appState.waveformEnabled && !appState.heapCritical) {
    uint8_t wfBin[2 + WAVEFORM_BUFFER_SIZE]; // 258 bytes
    wfBin[0] = WS_BIN_WAVEFORM;
    for (int a = 0; a < appState.numInputsDetected; a++) {
      if (i2s_audio_get_waveform(wfBin + 2, a)) {
        wfBin[1] = (uint8_t)a;
        for (int i = 0; i < MAX_WS_CLIENTS; i++) {
          if (_audioSubscribed[i]) {
            webSocket.sendBIN(i, wfBin, sizeof(wfBin));
          }
        }
      }
    }
  }

  // --- Spectrum data (per-ADC) — binary: [type:1][adc:1][freq:f32LE][bands:Nxf32LE] ---
  if (appState.spectrumEnabled && !appState.heapCritical) {
    uint8_t spBin[2 + sizeof(float) + SPECTRUM_BANDS * sizeof(float)]; // 70 bytes
    spBin[0] = WS_BIN_SPECTRUM;
    float bands[SPECTRUM_BANDS];
    float freq = 0.0f;
    for (int a = 0; a < appState.numInputsDetected; a++) {
      if (i2s_audio_get_spectrum(bands, &freq, a)) {
        spBin[1] = (uint8_t)a;
        memcpy(spBin + 2, &freq, sizeof(float));
        memcpy(spBin + 2 + sizeof(float), bands, SPECTRUM_BANDS * sizeof(float));
        for (int i = 0; i < MAX_WS_CLIENTS; i++) {
          if (_audioSubscribed[i]) {
            webSocket.sendBIN(i, spBin, sizeof(spBin));
          }
        }
      }
    }
  }
}
