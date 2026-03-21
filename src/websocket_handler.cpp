#include "websocket_handler.h"
#include "auth_handler.h"
#include "config.h"
#include "app_state.h"
#include "globals.h"
#include "diag_journal.h"
#include "diag_event.h"
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
#include "audio_pipeline.h"
#include "audio_input_source.h"
#include "audio_output_sink.h"
#include "heap_budget.h"
#ifdef DSP_ENABLED
#include "dsp_pipeline.h"
#include "dsp_coefficients.h"
#include "dsp_crossover.h"
#include "thd_measurement.h"
#endif
#ifdef DAC_ENABLED
#include "output_dsp.h"
#include "dac_hal.h"
#include "dac_eeprom.h"
#include "hal/hal_device_manager.h"
#include "hal/hal_audio_device.h"
#include "hal/hal_pipeline_bridge.h"
#include "hal/hal_settings.h"
#include "hal/hal_types.h"
#include "hal/hal_temp_sensor.h"
#endif
#ifdef USB_AUDIO_ENABLED
#include "usb_audio.h"
#endif
#include <WiFi.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "esp_freertos_hooks.h"
#include "esp_timer.h"

// ===== WebSocket Authentication Tracking =====
bool wsAuthStatus[MAX_WS_CLIENTS] = {false};
unsigned long wsAuthTimeout[MAX_WS_CLIENTS] = {0};
static String wsSessionId[MAX_WS_CLIENTS];

// ===== HTTP Page Serving Flag =====
volatile bool httpServingPage = false;

// ===== Per-client Audio Streaming Subscription =====
static bool _audioSubscribed[MAX_WS_CLIENTS] = {};

// ===== Authenticated Client Counter =====
// Tracked via webSocketEvent() connect/disconnect callbacks.
// Used by broadcast functions to skip JSON serialization when no clients are listening.
static uint8_t _wsAuthCount = 0;
static inline bool _wsAnyAuth() { return _wsAuthCount > 0; }
bool wsAnyClientAuthenticated() { return _wsAuthCount > 0; }

// Deferred initial-state queue — spreads the auth-success broadcast burst
// across multiple main-loop iterations to prevent WiFi TX saturation that
// causes cross-core audio pipeline interference.
static uint32_t _pendingInitState[MAX_WS_CLIENTS] = {};

enum InitStateBit : uint32_t {
    INIT_WIFI        = (1u << 0),
    INIT_SENSING     = (1u << 1),
    INIT_DISPLAY     = (1u << 2),
    INIT_BUZZER      = (1u << 3),
    INIT_SIGGEN      = (1u << 4),
    INIT_AUDIO_GRAPH = (1u << 5),
    INIT_DEBUG       = (1u << 6),
    INIT_ADC_STATE   = (1u << 7),
    INIT_DSP         = (1u << 8),
    INIT_DAC         = (1u << 9),
    INIT_USB_AUDIO   = (1u << 10),
    INIT_UPDATED     = (1u << 11),
    INIT_HAL_DEVICE  = (1u << 13),
    INIT_CHANNEL_MAP = (1u << 14),
    INIT_ALL         = 0x7FFFu,
};

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

// ===== HAL Device Lookup Helpers =====
#ifdef DAC_ENABLED
// Find HAL slot for a device by compatible string. Returns 0xFF if not found.
static uint8_t _halSlotForCompatible(const char* compat) {
    HalDevice* dev = HalDeviceManager::instance().findByCompatible(compat);
    return dev ? dev->getSlot() : 0xFF;
}
// Get HalAudioDevice* for a given pipeline sink slot (nullptr if not found or not audio device)
static HalAudioDevice* _audioDeviceForSinkSlot(uint8_t sinkSlot) {
    int8_t halSlot = hal_pipeline_get_slot_for_sink(sinkSlot);
    if (halSlot < 0) return nullptr;
    HalDevice* dev = HalDeviceManager::instance().getDevice((uint8_t)halSlot);
    if (!dev) return nullptr;
    // Only DAC/CODEC types are audio devices
    if (dev->getType() != HAL_DEV_DAC && dev->getType() != HAL_DEV_CODEC) return nullptr;
    return static_cast<HalAudioDevice*>(dev);
}
#endif

// ===== WebSocket Event Handler =====

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      LOG_I("[WebSocket] Client [%u] disconnected", num);
      if (wsAuthStatus[num] && _wsAuthCount > 0) _wsAuthCount--;
      wsAuthStatus[num] = false;
      wsAuthTimeout[num] = 0;
      wsSessionId[num] = "";
      _audioSubscribed[num] = false;
      _pendingInitState[num] = 0;
      break;

    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        LOG_I("[WebSocket] Client [%u] connected from %d.%d.%d.%d", num, ip[0], ip[1], ip[2], ip[3]);

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

        // Handle authentication message — token-based (preferred) or session fallback
        if (msgType == "auth") {
          String sessionId;
          bool authenticated = false;

          // Prefer one-time WS token (from /api/ws-token endpoint)
          if (doc.containsKey("token")) {
            String token = doc["token"].as<String>();
            authenticated = validateWsToken(token, sessionId);
          }
          // Fallback: direct session ID (legacy clients)
          if (!authenticated && doc.containsKey("sessionId")) {
            sessionId = doc["sessionId"].as<String>();
            authenticated = validateSession(sessionId);
          }

          if (authenticated) {
            wsAuthStatus[num] = true;
            _wsAuthCount++;
            wsAuthTimeout[num] = 0;
            wsSessionId[num] = sessionId;
            webSocket.sendTXT(num, "{\"type\":\"authSuccess\"}");
            LOG_D("[WebSocket] Client [%u] authenticated (total: %u)", num, _wsAuthCount);

            // Defer initial state sends — drainPendingInitState() will send
            // 3 per main-loop iteration to avoid WiFi TX burst audio pops.
            _pendingInitState[num] = INIT_ALL;
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

        if (doc["type"] == "toggleAP") {
          bool enabled = doc["enabled"].as<bool>();
          appState.wifi.apEnabled = enabled;
          
          if (enabled) {
            if (!appState.wifi.isAPMode) {
              WiFi.mode(WIFI_AP_STA);
              WiFi.softAP(appState.wifi.apSSID.c_str(), appState.wifi.apPassword);
              appState.wifi.isAPMode = true;
              LOG_I("[WebSocket] Access Point enabled");
              LOG_I("[WebSocket] AP IP: %s", WiFi.softAPIP().toString().c_str());
            }
          } else {
            if (appState.wifi.isAPMode && WiFi.status() == WL_CONNECTED) {
              WiFi.softAPdisconnect(true);
              WiFi.mode(WIFI_STA);
              appState.wifi.isAPMode = false;
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
            saveSettingsDeferred();
            LOG_I("[WebSocket] Screen timeout set to %d seconds", timeoutSec);
            sendDisplayState();
          }
        } else if (msgType == "setBrightness") {
          int newBright = doc["value"].as<int>();
          if (newBright >= 1 && newBright <= 255) {
            AppState::getInstance().setBacklightBrightness((uint8_t)newBright);
            saveSettingsDeferred();
            LOG_I("[WebSocket] Brightness set to %d", newBright);
            sendDisplayState();
          }
        } else if (msgType == "setDimEnabled") {
          bool newState = doc["enabled"].as<bool>();
          AppState::getInstance().setDimEnabled(newState);
          saveSettingsDeferred();
          LOG_I("[WebSocket] Dim %s", newState ? "enabled" : "disabled");
          sendDisplayState();
        } else if (msgType == "setDimTimeout") {
          int dimSec = doc["value"].as<int>();
          unsigned long dimMs = (unsigned long)dimSec * 1000UL;
          if (dimMs == 5000 || dimMs == 10000 || dimMs == 15000 ||
              dimMs == 30000 || dimMs == 60000) {
            AppState::getInstance().setDimTimeout(dimMs);
            saveSettingsDeferred();
            LOG_I("[WebSocket] Dim timeout set to %d seconds", dimSec);
            sendDisplayState();
          }
        } else if (msgType == "setDimBrightness") {
          int dimPwm = doc["value"].as<int>();
          if (dimPwm == 26 || dimPwm == 64 || dimPwm == 128 || dimPwm == 191) {
            AppState::getInstance().setDimBrightness((uint8_t)dimPwm);
            saveSettingsDeferred();
            LOG_I("[WebSocket] Dim brightness set to %d", dimPwm);
            sendDisplayState();
          }
        } else if (msgType == "setBuzzerEnabled") {
          bool newState = doc["enabled"].as<bool>();
          AppState::getInstance().setBuzzerEnabled(newState);
          saveSettingsDeferred();
          LOG_I("[WebSocket] Buzzer set to %s", newState ? "ON" : "OFF");
          sendBuzzerState();
        } else if (msgType == "setBuzzerVolume") {
          int newVol = doc["value"].as<int>();
          if (newVol >= 0 && newVol <= 2) {
            AppState::getInstance().setBuzzerVolume(newVol);
            saveSettingsDeferred();
            LOG_I("[WebSocket] Buzzer volume set to %d", newVol);
            sendBuzzerState();
          }
        } else if (msgType == "subscribeAudio") {
          bool enabled = doc["enabled"] | false;
          _audioSubscribed[num] = enabled;
          LOG_I("[WebSocket] Client [%u] audio subscription %s", num, enabled ? "enabled" : "disabled");
        } else if (msgType == "setAudioUpdateRate") {
          int rate = doc["value"].as<int>();
          if (rate == 33 || rate == 50 || rate == 100) {
            appState.audio.updateRate = (uint16_t)rate;
            saveSettingsDeferred();
            LOG_I("[WebSocket] Audio update rate set to %d ms", rate);
          }
        } else if (msgType == "setVuMeterEnabled") {
          appState.audio.vuMeterEnabled = doc["enabled"].as<bool>();
          saveSettingsDeferred();
          sendAudioGraphState();
          LOG_I("[WebSocket] VU meter %s", appState.audio.vuMeterEnabled ? "enabled" : "disabled");
        } else if (msgType == "setWaveformEnabled") {
          appState.audio.waveformEnabled = doc["enabled"].as<bool>();
          saveSettingsDeferred();
          sendAudioGraphState();
          LOG_I("[WebSocket] Waveform %s", appState.audio.waveformEnabled ? "enabled" : "disabled");
        } else if (msgType == "setSpectrumEnabled") {
          appState.audio.spectrumEnabled = doc["enabled"].as<bool>();
          saveSettingsDeferred();
          sendAudioGraphState();
          LOG_I("[WebSocket] Spectrum %s", appState.audio.spectrumEnabled ? "enabled" : "disabled");
        } else if (msgType == "setFftWindowType") {
          int wt = doc["value"].as<int>();
          if (wt >= 0 && wt < FFT_WINDOW_COUNT) {
            appState.audio.fftWindowType = (FftWindowType)wt;
            saveSettingsDeferred();
            sendAudioGraphState();
            LOG_I("[WebSocket] FFT window type: %d", wt);
          }
        } else if (msgType == "setSignalGen") {
          bool changed = false;
          if (doc["enabled"].is<bool>()) {
            appState.sigGen.enabled = doc["enabled"].as<bool>();
            changed = true;
          }
          if (doc["waveform"].is<int>()) {
            int w = doc["waveform"].as<int>();
            if (w >= 0 && w <= 3) { appState.sigGen.waveform = w; changed = true; }
          }
          if (doc["frequency"].is<float>()) {
            float f = doc["frequency"].as<float>();
            if (f >= 1.0f && f <= 22000.0f) { appState.sigGen.frequency = f; changed = true; }
          }
          if (doc["amplitude"].is<float>()) {
            float a = doc["amplitude"].as<float>();
            if (a >= -96.0f && a <= 0.0f) { appState.sigGen.amplitude = a; changed = true; }
          }
          if (doc["channel"].is<int>()) {
            int c = doc["channel"].as<int>();
            if (c >= 0 && c <= 2) { appState.sigGen.channel = c; changed = true; }
          }
          if (doc["outputMode"].is<int>()) {
            int m = doc["outputMode"].as<int>();
            if (m >= 0 && m <= 1) { appState.sigGen.outputMode = m; changed = true; }
          }
          if (doc["sweepSpeed"].is<float>()) {
            float s = doc["sweepSpeed"].as<float>();
            if (s >= 1.0f && s <= 22000.0f) { appState.sigGen.sweepSpeed = s; changed = true; }
          }
          if (changed) {
            siggen_apply_params();
            saveSignalGenSettings();
            sendSignalGenState();
            LOG_I("[WebSocket] Signal generator updated by client [%u]", num);
          }
        } else if (msgType == "setInputNames") {
          if (doc["names"].is<JsonArray>()) {
            JsonArray names = doc["names"].as<JsonArray>();
            for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS * 2 && i < (int)names.size(); i++) {
              String name = names[i].as<String>();
              if (name.length() > 0) appState.audio.inputNames[i] = name;
            }
            saveInputNames();
            // Broadcast updated names
            JsonDocument resp;
            resp["type"] = "inputNames";
            JsonArray outNames = resp["names"].to<JsonArray>();
            for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS * 2; i++) {
              outNames.add(appState.audio.inputNames[i]);
            }
            String json;
            serializeJson(resp, json);
            webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
            LOG_I("[WebSocket] Input names updated by client [%u]", num);
          }
        } else if (msgType == "setDebugMode") {
          appState.debug.debugMode = doc["enabled"].as<bool>();
          applyDebugSerialLevel(appState.debug.debugMode, appState.debug.serialLevel);
          saveSettingsDeferred();
          sendDebugState();
          LOG_I("[WebSocket] Debug mode %s", appState.debug.debugMode ? "enabled" : "disabled");
        } else if (msgType == "setDebugSerialLevel") {
          int level = doc["level"].as<int>();
          if (level >= 0 && level <= 3) {
            appState.debug.serialLevel = level;
            applyDebugSerialLevel(appState.debug.debugMode, appState.debug.serialLevel);
            saveSettingsDeferred();
            sendDebugState();
            LOG_I("[WebSocket] Debug serial level set to %d", level);
          }
        } else if (msgType == "setDebugHwStats") {
          appState.debug.hwStats = doc["enabled"].as<bool>();
          saveSettingsDeferred();
          sendDebugState();
          LOG_I("[WebSocket] Debug HW stats %s", appState.debug.hwStats ? "enabled" : "disabled");
        } else if (msgType == "setDebugI2sMetrics") {
          appState.debug.i2sMetrics = doc["enabled"].as<bool>();
          saveSettingsDeferred();
          sendDebugState();
          LOG_I("[WebSocket] Debug I2S metrics %s", appState.debug.i2sMetrics ? "enabled" : "disabled");
        } else if (msgType == "setDebugTaskMonitor") {
          appState.debug.taskMonitor = doc["enabled"].as<bool>();
          saveSettingsDeferred();
          sendDebugState();
          LOG_I("[WebSocket] Debug task monitor %s", appState.debug.taskMonitor ? "enabled" : "disabled");
        }
#ifdef DSP_ENABLED
        else if (msgType == "setDspBypass") {
          if (doc["enabled"].is<bool>()) {
            appState.dsp.enabled = doc["enabled"].as<bool>();
            // Wire enable to pipeline-level lane bypass: disabled → skip DSP entirely for ADC1+ADC2
            audio_pipeline_bypass_dsp(0, !appState.dsp.enabled);
            audio_pipeline_bypass_dsp(1, !appState.dsp.enabled);
          }
          if (doc["bypass"].is<bool>()) appState.dsp.bypass = doc["bypass"].as<bool>();
          // Sync global bypass to DSP config (in-DSP bypass, independent of lane enable)
          dsp_copy_active_to_inactive();
          DspState *cfg = dsp_get_inactive_config();
          cfg->globalBypass = appState.dsp.bypass;
          if (!dsp_swap_config()) { dsp_log_swap_failure("WebSocket"); }
          extern void saveDspSettingsDebounced();
          saveDspSettingsDebounced();
          appState.markDspConfigDirty();
          LOG_I("[WebSocket] DSP enabled=%d bypass=%d", appState.dsp.enabled, appState.dsp.bypass);
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
              if (!dsp_swap_config()) { dsp_log_swap_failure("WebSocket"); }
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
              if (!dsp_swap_config()) { dsp_log_swap_failure("WebSocket"); }
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
              if (!dsp_swap_config()) { dsp_log_swap_failure("WebSocket"); }
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
                if (!dsp_swap_config()) { dsp_log_swap_failure("WebSocket"); }
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
            if (!dsp_swap_config()) { dsp_log_swap_failure("WebSocket"); }
            extern void saveDspSettingsDebounced();
            saveDspSettingsDebounced();
            appState.markDspConfigDirty();
          }
        }
        else if (msgType == "setDspStereoLink") {
          int pair = doc["pair"] | -1;
          bool linked = doc["linked"] | true; // cppcheck-suppress badBitmaskCheck
          if (pair >= 0 && pair <= 1) {
            dsp_copy_active_to_inactive();
            DspState *cfg = dsp_get_inactive_config();
            int chA = pair * 2;
            int chB = pair * 2 + 1;
            cfg->channels[chA].stereoLink = linked;
            cfg->channels[chB].stereoLink = linked;
            if (linked) dsp_mirror_channel_config(chA, chB);
            if (!dsp_swap_config()) { dsp_log_swap_failure("WebSocket"); }
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
              if (!dsp_swap_config()) { dsp_log_swap_failure("WebSocket"); }
              extern void saveDspSettingsDebounced();
              saveDspSettingsDebounced();
              appState.markDspConfigDirty();
            }
          }
        }
        else if (msgType == "setPeqBandEnabled") {
          int ch = doc["ch"] | -1;
          int band = doc["band"] | -1;
          bool en = doc["enabled"] | true; // cppcheck-suppress badBitmaskCheck
          if (ch >= 0 && ch < DSP_MAX_CHANNELS && band >= 0 && band < DSP_PEQ_BANDS) {
            dsp_copy_active_to_inactive();
            DspState *cfg = dsp_get_inactive_config();
            if (band < cfg->channels[ch].stageCount) {
              cfg->channels[ch].stages[band].enabled = en;
              if (!dsp_swap_config()) { dsp_log_swap_failure("WebSocket"); }
              extern void saveDspSettingsDebounced();
              saveDspSettingsDebounced();
              appState.markDspConfigDirty();
            }
          }
        }
        else if (msgType == "setPeqAllEnabled") {
          int ch = doc["ch"] | -1;
          bool en = doc["enabled"] | true; // cppcheck-suppress badBitmaskCheck
          if (ch >= 0 && ch < DSP_MAX_CHANNELS) {
            dsp_copy_active_to_inactive();
            DspState *cfg = dsp_get_inactive_config();
            int limit = cfg->channels[ch].stageCount < DSP_PEQ_BANDS ? cfg->channels[ch].stageCount : DSP_PEQ_BANDS;
            for (int b = 0; b < limit; b++) {
              cfg->channels[ch].stages[b].enabled = en;
            }
            if (!dsp_swap_config()) { dsp_log_swap_failure("WebSocket"); }
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
            if (!dsp_swap_config()) { dsp_log_swap_failure("WebSocket"); }
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
            if (!dsp_swap_config()) { dsp_log_swap_failure("WebSocket"); }
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
              String json;
              serializeJson(preset, json);
              File f = LittleFS.open(path, "w");
              if (f) { f.print(json); f.close(); }

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
              String json = f.readString();
              f.close();
              JsonDocument preset;
              if (!deserializeJson(preset, json) && preset["bands"].is<JsonArray>()) {
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
                if (!dsp_swap_config()) { dsp_log_swap_failure("WebSocket"); }
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
          String json;
          serializeJson(resp, json);
          webSocket.sendTXT(num, json.c_str());
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
        else if (msgType == "startThdMeasurement") {
          float freq = doc["freq"] | 1000.0f;
          int avg = doc["averages"] | 8;
          extern void thd_start_measurement(float, uint16_t);
          thd_start_measurement(freq, (uint16_t)avg);
          LOG_I("[WebSocket] THD measurement started: %.0f Hz, %d avg", freq, avg);
        }
        else if (msgType == "stopThdMeasurement") {
          extern void thd_stop_measurement();
          thd_stop_measurement();
          LOG_I("[WebSocket] THD measurement stopped");
        }
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
              if (!dsp_swap_config()) { dsp_log_swap_failure("WebSocket"); }
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
          LOG_W("[WebSocket] DEPRECATED: '%s' — use PUT /api/hal/devices", msgType.c_str());
          uint8_t halSlot = _halSlotForCompatible("ti,pcm5102a");
          if (halSlot == 0xFF) {
            LOG_W("[WebSocket] setDacEnabled: PCM5102A not found in HAL");
          } else {
            HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(halSlot);
            bool was = cfg ? cfg->enabled : false;
            bool en = doc["enabled"].as<bool>();
            if (cfg) cfg->enabled = en;
            // Defer init/deinit to main loop — I2C EEPROM scan + I2S driver
            // setup is too heavy for the WebSocket handler context (blocks SDIO)
            HalDevice* dev = HalDeviceManager::instance().getDevice(halSlot);
            if (en && !was && dev && !dev->_ready) {
              if (!appState.halCoord.requestDeviceToggle(halSlot, 1)) {
                LOG_W("[WebSocket] Toggle queue full for slot %u (setDacEnabled)", halSlot);
              }
            } else if (!en && was) {
              if (!appState.halCoord.requestDeviceToggle(halSlot, -1)) {
                LOG_W("[WebSocket] Toggle queue full for slot %u (setDacEnabled)", halSlot);
              }
            }
            hal_save_device_config_deferred(halSlot);
            appState.markDacDirty();
            LOG_I("[WebSocket] DAC %s (deferred)", en ? "enabled" : "disabled");
          }
        }
        else if (msgType == "setDacVolume") {
          LOG_W("[WebSocket] DEPRECATED: '%s' — use PUT /api/hal/devices", msgType.c_str());
          uint8_t halSlot = _halSlotForCompatible("ti,pcm5102a");
          int v = doc["volume"].as<int>();
          if (v >= 0 && v <= 100) {
            if (halSlot != 0xFF) {
              HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(halSlot);
              if (cfg) cfg->volume = (uint8_t)v;
              int8_t sinkSlot = hal_pipeline_get_sink_slot(halSlot);
              if (sinkSlot >= 0) {
                audio_pipeline_set_sink_volume((uint8_t)sinkSlot, dac_volume_to_linear((uint8_t)v));
                HalAudioDevice* audioDev = _audioDeviceForSinkSlot((uint8_t)sinkSlot);
                if (audioDev && audioDev->hasHardwareVolume()) audioDev->setVolume((uint8_t)v);
              }
              hal_save_device_config_deferred(halSlot);
            } else {
              LOG_W("[WebSocket] setDacVolume: PCM5102A not found in HAL");
            }
            appState.markDacDirty();
          }
        }
        else if (msgType == "setDacMute") {
          LOG_W("[WebSocket] DEPRECATED: '%s' — use PUT /api/hal/devices", msgType.c_str());
          uint8_t halSlot = _halSlotForCompatible("ti,pcm5102a");
          bool newMute = doc["mute"].as<bool>();
          // Apply software mute via slot-indexed API
          if (halSlot != 0xFF) {
            int8_t sinkSlot = hal_pipeline_get_sink_slot(halSlot);
            if (sinkSlot >= 0) {
              audio_pipeline_set_sink_muted((uint8_t)sinkSlot, newMute);
              HalAudioDevice* audioDev = _audioDeviceForSinkSlot((uint8_t)sinkSlot);
              if (audioDev) audioDev->setMute(newMute);
            }
            HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(halSlot);
            if (cfg) cfg->mute = newMute;
            hal_save_device_config_deferred(halSlot);
          } else {
              // PCM5102A not in HAL — mute request ignored (device not registered)
            LOG_W("[WebSocket] setDacMute: PCM5102A not found in HAL");
          }
          appState.markDacDirty();
          LOG_I("[DAC] (PCM5102A) mute: %s", newMute ? "ON" : "OFF");
        }
        else if (msgType == "setDacFilter") {
          LOG_W("[WebSocket] DEPRECATED: '%s' — use PUT /api/hal/devices", msgType.c_str());
          uint8_t halSlot = _halSlotForCompatible("ti,pcm5102a");
          HalDeviceConfig* fCfg = (halSlot != 0xFF) ? HalDeviceManager::instance().getConfig(halSlot) : nullptr;
          uint8_t prevFilter = fCfg ? fCfg->filterMode : 0;
          int fm = doc["filterMode"].as<int>();
          if (fCfg) fCfg->filterMode = (uint8_t)fm;
          HalAudioDevice* audioDev = _audioDeviceForSinkSlot(0);
          if (audioDev) audioDev->setFilterMode((uint8_t)fm);
          if (halSlot != 0xFF) {
            hal_save_device_config_deferred(halSlot);
          } else {
            LOG_W("[WebSocket] setDacFilter: PCM5102A not found in HAL");
          }
          appState.markDacDirty();
          LOG_I("[DAC] Filter mode: %d -> %d", prevFilter, (uint8_t)fm);
        }
        else if (msgType == "setEs8311Enabled") {
          LOG_W("[WebSocket] DEPRECATED: '%s' — use PUT /api/hal/devices", msgType.c_str());
          uint8_t halSlot = _halSlotForCompatible("everest-semi,es8311");
          if (halSlot == 0xFF) {
            LOG_W("[WebSocket] setEs8311Enabled: ES8311 not found in HAL");
          } else {
            HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(halSlot);
            bool was = cfg ? cfg->enabled : false;
            bool en = doc["enabled"].as<bool>();
            if (cfg) cfg->enabled = en;
            // Defer init/deinit to main loop — ES8311 I2C + I2S2 setup is too heavy
            // for the WebSocket handler context (blocks SDIO -> WiFi crash)
            if (en && !was) {
              if (!appState.halCoord.requestDeviceToggle(halSlot, 1)) {
                LOG_W("[WebSocket] Toggle queue full for slot %u (setEs8311Enabled)", halSlot);
              }
            } else if (!en && was) {
              if (!appState.halCoord.requestDeviceToggle(halSlot, -1)) {
                LOG_W("[WebSocket] Toggle queue full for slot %u (setEs8311Enabled)", halSlot);
              }
            }
            hal_save_device_config_deferred(halSlot);
            appState.markDacDirty();
            LOG_I("[WebSocket] ES8311 %s (deferred)", en ? "enabled" : "disabled");
          }
        }
        else if (msgType == "setEs8311Volume") {
          LOG_W("[WebSocket] DEPRECATED: '%s' — use PUT /api/hal/devices", msgType.c_str());
          uint8_t halSlot = _halSlotForCompatible("everest-semi,es8311");
          int v = doc["volume"].as<int>();
          if (v >= 0 && v <= 100) {
            if (halSlot != 0xFF) {
              HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(halSlot);
              if (cfg) cfg->volume = (uint8_t)v;
              int8_t sinkSlot = hal_pipeline_get_sink_slot(halSlot);
              if (sinkSlot >= 0) {
                audio_pipeline_set_sink_volume((uint8_t)sinkSlot, dac_volume_to_linear((uint8_t)v));
                HalAudioDevice* audioDev = _audioDeviceForSinkSlot((uint8_t)sinkSlot);
                if (audioDev && audioDev->hasHardwareVolume()) audioDev->setVolume((uint8_t)v);
              }
              hal_save_device_config_deferred(halSlot);
            } else {
              LOG_W("[WebSocket] setEs8311Volume: ES8311 not found in HAL");
            }
            appState.markDacDirty();
          }
        }
        else if (msgType == "setEs8311Mute") {
          LOG_W("[WebSocket] DEPRECATED: '%s' — use PUT /api/hal/devices", msgType.c_str());
          uint8_t halSlot = _halSlotForCompatible("everest-semi,es8311");
          bool newMute = doc["mute"].as<bool>();
          if (halSlot != 0xFF) {
            int8_t sinkSlot = hal_pipeline_get_sink_slot(halSlot);
            if (sinkSlot >= 0) {
              audio_pipeline_set_sink_muted((uint8_t)sinkSlot, newMute);
              HalAudioDevice* audioDev = _audioDeviceForSinkSlot((uint8_t)sinkSlot);
              if (audioDev) audioDev->setMute(newMute);
            }
            HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(halSlot);
            if (cfg) cfg->mute = newMute;
            hal_save_device_config_deferred(halSlot);
          } else {
            // ES8311 not in HAL — mute request ignored (device not registered)
            LOG_W("[WebSocket] setEs8311Mute: ES8311 not found in HAL");
          }
          appState.markDacDirty();
          LOG_I("[DAC] (ES8311) mute: %s", newMute ? "ON" : "OFF");
        }
        else if (msgType == "eepromScan") {
          LOG_I("[WebSocket] EEPROM scan requested");
          EepromDiag& ed = appState.dac.eepromDiag;
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
          if (!ok) appState.dac.eepromDiag.writeErrors++;

          // Re-scan (use cached mask from prior scan)
          DacEepromData scanned;
          EepromDiag& ed = appState.dac.eepromDiag;
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
          uint8_t tAddr = appState.dac.eepromDiag.eepromAddr;
          if (doc["address"].is<int>()) tAddr = (uint8_t)doc["address"].as<int>();
          if (tAddr < DAC_EEPROM_ADDR_START || tAddr > DAC_EEPROM_ADDR_END) tAddr = DAC_EEPROM_ADDR_START;

          bool ok = dac_eeprom_erase(tAddr);
          if (!ok) appState.dac.eepromDiag.writeErrors++;

          EepromDiag& ed = appState.dac.eepromDiag;
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
          if (adc >= 0 && adc < AUDIO_PIPELINE_MAX_INPUTS && newVal != appState.audio.adcEnabled[adc]) {
            appState.audio.adcEnabled[adc] = newVal;
            appState.markAdcEnabledDirty();
            saveSettingsDeferred();
            // Broadcast new state to all clients
            JsonDocument resp;
            resp["type"] = "adcState";
            JsonArray arr = resp["enabled"].to<JsonArray>();
            for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) arr.add(appState.audio.adcEnabled[i]);
            String rJson;
            serializeJson(resp, rJson);
            webSocket.broadcastTXT((uint8_t*)rJson.c_str(), rJson.length());
            LOG_I("[WebSocket] ADC%d %s", adc + 1, newVal ? "enabled" : "disabled");
          }
        }
#ifdef USB_AUDIO_ENABLED
        // ===== USB Audio Enable/Disable =====
        else if (msgType == "setUsbAudioEnabled") {
          bool newVal = doc["enabled"].as<bool>();
          if (newVal != appState.usbAudio.enabled) {
            appState.usbAudio.enabled = newVal;
            saveSettingsDeferred();
            if (newVal) {
              usb_audio_init();
              appState.pipelineInputBypass[3] = false;
              appState.pipelineDspBypass[3] = false;
            } else {
              usb_audio_deinit();
              appState.pipelineInputBypass[3] = true;
            }
            appState.markUsbAudioDirty();
            LOG_I("[WebSocket] USB Audio %s", newVal ? "enabled" : "disabled");
          }
        }
#endif
        // ===== Audio Tab Channel Controls =====
        else if (msgType == "setInputGain") {
          int lane = doc["lane"] | -1;
          float db = doc["db"] | 0.0f;
          if (lane >= 0 && lane < AUDIO_PIPELINE_MAX_INPUTS) {
            audio_pipeline_bypass_input(lane, false);
            LOG_I("[WebSocket] Input gain lane=%d db=%.1f (gain not yet applied)", lane, db);
          }
        }
        else if (msgType == "setInputMute") {
          int lane = doc["lane"] | -1;
          bool muted = doc["muted"] | false;
          if (lane >= 0 && lane < AUDIO_PIPELINE_MAX_INPUTS) {
            audio_pipeline_bypass_input(lane, muted);
            LOG_I("[WebSocket] Input mute lane=%d muted=%d", lane, muted);
          }
        }
        else if (msgType == "setInputPhase") {
          int lane = doc["lane"] | -1;
          bool inverted = doc["inverted"] | false;
          if (lane >= 0 && lane < AUDIO_PIPELINE_MAX_INPUTS) {
            // Per-input polarity — set via DSP polarity stage
            LOG_I("[WebSocket] Input phase lane=%d inverted=%d", lane, inverted);
          }
        }
#ifdef DAC_ENABLED
        else if (msgType == "setOutputMute") {
          int ch = doc["channel"] | -1;
          bool muted = doc["muted"] | false;
          if (ch >= 0 && ch < AUDIO_PIPELINE_MATRIX_SIZE) {
            // Apply mute via output DSP mute stage
            output_dsp_copy_active_to_inactive();
            OutputDspState *cfg = output_dsp_get_inactive_config();
            // Find or create mute stage
            OutputDspChannelConfig &chCfg = cfg->channels[ch];
            bool found = false;
            for (int s = 0; s < chCfg.stageCount; s++) {
              if (chCfg.stages[s].type == DSP_MUTE) {
                chCfg.stages[s].mute.muted = muted;
                found = true;
                break;
              }
            }
            if (!found && chCfg.stageCount < OUTPUT_DSP_MAX_STAGES) {
              output_dsp_init_stage(chCfg.stages[chCfg.stageCount], DSP_MUTE);
              chCfg.stages[chCfg.stageCount].mute.muted = muted;
              chCfg.stageCount++;
            }
            output_dsp_swap_config();
            output_dsp_save_channel(ch);
            LOG_I("[WebSocket] Output mute ch=%d muted=%d", ch, muted);
          }
        }
        else if (msgType == "setOutputPhase") {
          int ch = doc["channel"] | -1;
          bool inverted = doc["inverted"] | false;
          if (ch >= 0 && ch < AUDIO_PIPELINE_MATRIX_SIZE) {
            output_dsp_copy_active_to_inactive();
            OutputDspState *cfg = output_dsp_get_inactive_config();
            OutputDspChannelConfig &chCfg = cfg->channels[ch];
            bool found = false;
            for (int s = 0; s < chCfg.stageCount; s++) {
              if (chCfg.stages[s].type == DSP_POLARITY) {
                chCfg.stages[s].polarity.inverted = inverted;
                found = true;
                break;
              }
            }
            if (!found && chCfg.stageCount < OUTPUT_DSP_MAX_STAGES) {
              output_dsp_init_stage(chCfg.stages[chCfg.stageCount], DSP_POLARITY);
              chCfg.stages[chCfg.stageCount].polarity.inverted = inverted;
              chCfg.stageCount++;
            }
            output_dsp_swap_config();
            output_dsp_save_channel(ch);
            LOG_I("[WebSocket] Output phase ch=%d inverted=%d", ch, inverted);
          }
        }
        else if (msgType == "setOutputGain") {
          int ch = doc["channel"] | -1;
          float db = doc["db"] | 0.0f;
          if (ch >= 0 && ch < AUDIO_PIPELINE_MATRIX_SIZE) {
            output_dsp_copy_active_to_inactive();
            OutputDspState *cfg = output_dsp_get_inactive_config();
            OutputDspChannelConfig &chCfg = cfg->channels[ch];
            bool found = false;
            for (int s = 0; s < chCfg.stageCount; s++) {
              if (chCfg.stages[s].type == DSP_GAIN) {
                chCfg.stages[s].gain.gainDb = db;
                found = true;
                break;
              }
            }
            if (!found && chCfg.stageCount < OUTPUT_DSP_MAX_STAGES) {
              output_dsp_init_stage(chCfg.stages[chCfg.stageCount], DSP_GAIN);
              chCfg.stages[chCfg.stageCount].gain.gainDb = db;
              chCfg.stageCount++;
            }
            output_dsp_swap_config();
            output_dsp_save_channel(ch);
            LOG_I("[WebSocket] Output gain ch=%d db=%.1f", ch, db);
          }
        }
        else if (msgType == "setOutputDelay") {
          int ch = doc["channel"] | -1;
          float ms = doc["ms"] | 0.0f;
          if (ch >= 0 && ch < AUDIO_PIPELINE_MATRIX_SIZE) {
            // Delay in ms → samples: delay = ms * sampleRate / 1000
            LOG_I("[WebSocket] Output delay ch=%d ms=%.2f", ch, ms);
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
  doc["backlightOn"] = AppState::getInstance().display.backlightOn;
  doc["screenTimeout"] = AppState::getInstance().display.screenTimeout / 1000; // Send as seconds
  doc["backlightBrightness"] = AppState::getInstance().display.backlightBrightness;
  doc["dimEnabled"] = AppState::getInstance().display.dimEnabled;
  doc["dimTimeout"] = AppState::getInstance().display.dimTimeout / 1000;
  doc["dimBrightness"] = AppState::getInstance().display.dimBrightness;
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}

void sendFactoryResetProgress(unsigned long secondsHeld, bool resetTriggered) {
  JsonDocument doc;
  doc["type"] = "factoryResetProgress";
  doc["secondsHeld"] = secondsHeld;
  doc["secondsRequired"] = BTN_VERY_LONG_PRESS_MIN / 1000;
  doc["resetTriggered"] = resetTriggered;
  doc["progress"] = (secondsHeld * 100) / (BTN_VERY_LONG_PRESS_MIN / 1000);
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}

void sendRebootProgress(unsigned long secondsHeld, bool rebootTriggered) {
  JsonDocument doc;
  doc["type"] = "rebootProgress";
  doc["secondsHeld"] = secondsHeld;
  doc["secondsRequired"] = BTN_VERY_LONG_PRESS_MIN / 1000;
  doc["rebootTriggered"] = rebootTriggered;
  doc["progress"] = (secondsHeld * 100) / (BTN_VERY_LONG_PRESS_MIN / 1000);
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}

void sendBuzzerState() {
  JsonDocument doc;
  doc["type"] = "buzzerState";
  doc["enabled"] = AppState::getInstance().buzzer.enabled;
  doc["volume"] = AppState::getInstance().buzzer.volume;
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}

void sendSignalGenState() {
  JsonDocument doc;
  doc["type"] = "signalGenerator";
  doc["enabled"] = appState.sigGen.enabled;
  doc["waveform"] = appState.sigGen.waveform;
  doc["frequency"] = appState.sigGen.frequency;
  doc["amplitude"] = appState.sigGen.amplitude;
  doc["channel"] = appState.sigGen.channel;
  doc["outputMode"] = appState.sigGen.outputMode;
  doc["sweepSpeed"] = appState.sigGen.sweepSpeed;
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}

void sendAudioGraphState() {
  JsonDocument doc;
  doc["type"] = "audioGraphState";
  doc["vuMeterEnabled"] = appState.audio.vuMeterEnabled;
  doc["waveformEnabled"] = appState.audio.waveformEnabled;
  doc["spectrumEnabled"] = appState.audio.spectrumEnabled;
  doc["fftWindowType"] = (int)appState.audio.fftWindowType;
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}

void sendDebugState() {
  JsonDocument doc;
  doc["type"] = "debugState";
  doc["debugMode"] = appState.debug.debugMode;
  doc["debugSerialLevel"] = appState.debug.serialLevel;
  doc["debugHwStats"] = appState.debug.hwStats;
  doc["debugI2sMetrics"] = appState.debug.i2sMetrics;
  doc["debugTaskMonitor"] = appState.debug.taskMonitor;
  // Pin configuration — static board info, always sent once on connect
  {
    JsonArray pins = doc["pins"].to<JsonArray>();
    struct { const char *d; const char *f; int g; const char *c; } pm[] = {
      {"I2S ADC (shared)", "BCK",   I2S_BCK_PIN,      "audio"},
      {"I2S ADC Lane 0",   "DOUT",  I2S_DOUT_PIN,     "audio"},
      {"I2S ADC Lane 1",   "DOUT2", I2S_DOUT2_PIN,    "audio"},
      {"I2S ADC (shared)", "LRC",   I2S_LRC_PIN,      "audio"},
      {"I2S ADC (shared)", "MCLK",  I2S_MCLK_PIN,     "audio"},
#ifdef DAC_ENABLED
      {"DAC Output",      "DOUT",  I2S_TX_DATA_PIN,   "audio"},
      {"DAC I2C",         "SDA",   DAC_I2C_SDA_PIN,   "audio"},
      {"DAC I2C",         "SCL",   DAC_I2C_SCL_PIN,   "audio"},
#endif
      {"ST7735S TFT",     "CS",    TFT_CS_PIN,        "display"},
      {"ST7735S TFT",     "MOSI",  TFT_MOSI_PIN,     "display"},
      {"ST7735S TFT",     "CLK",   TFT_SCLK_PIN,     "display"},
      {"ST7735S TFT",     "DC",    TFT_DC_PIN,        "display"},
      {"ST7735S TFT",     "RST",   TFT_RST_PIN,      "display"},
      {"ST7735S TFT",     "BL",    TFT_BL_PIN,        "display"},
      {"EC11 Encoder",    "A",     ENCODER_A_PIN,      "input"},
      {"EC11 Encoder",    "B",     ENCODER_B_PIN,      "input"},
      {"EC11 Encoder",    "SW",    ENCODER_SW_PIN,     "input"},
      {"Piezo Buzzer",    "IO",    BUZZER_PIN,         "core"},
      {"Status LED",      "LED",   LED_PIN,            "core"},
      {"Relay Module",    "Amp",   AMPLIFIER_PIN,      "core"},
      {"Tactile Switch",  "Btn",   RESET_BUTTON_PIN,   "core"},
      {"Signal Generator","PWM",   SIGGEN_PWM_PIN,     "core"},
#if CONFIG_IDF_TARGET_ESP32P4
      {"ES8311 DAC",      "I2S TX",  9,               "audio"},
      {"ES8311 DAC",      "PA Ctrl", 53,              "audio"},
#endif
    };
    for (auto &p : pm) {
      JsonObject pin = pins.add<JsonObject>();
      pin["g"] = p.g;
      pin["f"] = p.f;
      pin["d"] = p.d;
      pin["c"] = p.c;
    }
  }
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}

// ===== Diagnostic Event Broadcast =====
void sendDiagEvent() {
  DiagEvent ev;
  if (!diag_journal_latest(&ev)) return;

  JsonDocument doc;
  doc["type"]  = "diagEvent";
  doc["seq"]   = ev.seq;
  doc["boot"]  = ev.bootId;
  doc["t"]     = ev.timestamp;
  doc["heap"]  = ev.heapFree;

  // Error code as hex string "0x1001"
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
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}

#ifdef DSP_ENABLED
void sendDspState() {
  if (!_wsAnyAuth()) return;
  JsonDocument doc;
  doc["type"] = "dspState";
  doc["dspEnabled"] = appState.dsp.enabled;
  doc["dspBypass"] = appState.dsp.bypass;
  doc["presetIndex"] = appState.dsp.presetIndex;

  // Send preset list (index, name, exists)
  JsonArray presets = doc["presets"].to<JsonArray>();
  extern bool dsp_preset_exists(int);
  for (int i = 0; i < DSP_PRESET_MAX_SLOTS; i++) {
    JsonObject preset = presets.add<JsonObject>();
    preset["index"] = i;
    preset["name"] = appState.dsp.presetNames[i];
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
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}

void sendDspMetrics() {
  if (!_wsAnyAuth()) return;
  DspMetrics m = dsp_get_metrics();
  JsonDocument doc;
  doc["type"] = "dspMetrics";
  doc["processTimeUs"] = m.processTimeUs;
  doc["cpuLoad"] = m.cpuLoadPercent;
  JsonArray gr = doc["limiterGr"].to<JsonArray>();
  for (int i = 0; i < DSP_MAX_CHANNELS; i++) gr.add(m.limiterGrDb[i]);
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}
#endif

#ifdef DAC_ENABLED
void sendDacState() {
  if (!_wsAnyAuth()) return;
  JsonDocument doc;
  doc["type"] = "dacState";

  // Query primary DAC (PCM5102A) via HAL
  {
    HalDevice* dev = HalDeviceManager::instance().findByCompatible("ti,pcm5102a");
    HalDeviceConfig* cfg = dev ? HalDeviceManager::instance().getConfig(dev->getSlot()) : nullptr;
    doc["enabled"] = cfg ? cfg->enabled : false;
    doc["volume"] = cfg ? cfg->volume : 80;
    doc["mute"] = cfg ? cfg->mute : false;
    doc["deviceId"] = dev ? dev->getDescriptor().legacyId : 0x0001;
    doc["modelName"] = dev ? dev->getDescriptor().name : "PCM5102A";
    doc["outputChannels"] = dev ? dev->getDescriptor().channelCount : 2;
    doc["detected"] = (dev != nullptr);
    doc["ready"] = dev ? dev->_ready : false;
    doc["filterMode"] = cfg ? cfg->filterMode : 0;
    doc["txUnderruns"] = appState.dac.txUnderruns;  // Diagnostic counter stays in AppState
  }

  // Query ES8311 via HAL
  {
    HalDevice* esDev = HalDeviceManager::instance().findByCompatible("everest-semi,es8311");
    HalDeviceConfig* esCfg = esDev ? HalDeviceManager::instance().getConfig(esDev->getSlot()) : nullptr;
    doc["es8311Enabled"] = esCfg ? esCfg->enabled : false;
    doc["es8311Volume"] = esCfg ? esCfg->volume : 80;
    doc["es8311Mute"] = esCfg ? esCfg->mute : false;
    doc["es8311Ready"] = esDev ? esDev->_ready : false;
  }
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
  // Include available DAC-path devices from HAL with pipeline sink state
  {
    JsonArray drivers = doc["drivers"].to<JsonArray>();
    HalDeviceManager& mgr = HalDeviceManager::instance();
    for (uint8_t s = 0; s < HAL_MAX_DEVICES; s++) {
        HalDevice* dev = mgr.getDevice(s);
        if (!dev) continue;
        if (!(dev->getDescriptor().capabilities & HAL_CAP_DAC_PATH)) continue;
        const HalDeviceDescriptor& desc = dev->getDescriptor();
        JsonObject drv = drivers.add<JsonObject>();
        drv["compatible"] = desc.compatible;
        drv["name"] = desc.name;
        drv["halSlot"] = s;
        drv["ready"] = dev->_ready;
        // Add enabled/volume/mute state from pipeline sink
        int8_t sinkSlot = hal_pipeline_get_sink_slot(s);
        if (sinkSlot >= 0) {
            drv["enabled"] = true;
            drv["volume"] = (int)(audio_pipeline_get_sink_volume(sinkSlot) * 100);
            drv["muted"] = audio_pipeline_is_sink_muted(sinkSlot);
        } else {
            drv["enabled"] = false;
            drv["volume"] = 0;
            drv["muted"] = false;
        }
    }
  }
  // EEPROM diagnostics
  {
    const EepromDiag& ed = appState.dac.eepromDiag;
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
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}

void sendHalDeviceState() {
    if (!_wsAnyAuth()) return;
    JsonDocument doc;
    doc["type"] = "halDeviceState";
    doc["scanning"] = appState._halScanInProgress;

    JsonArray arr = doc["devices"].to<JsonArray>();
    HalDeviceManager::instance().forEach([](HalDevice* dev, void* ctx) {
        JsonArray* a = static_cast<JsonArray*>(ctx);
        const HalDeviceDescriptor& desc = dev->getDescriptor();
        JsonObject obj = a->add<JsonObject>();
        obj["slot"] = dev->getSlot();
        obj["compatible"] = desc.compatible;
        obj["name"] = desc.name;
        obj["type"] = desc.type;
        obj["state"] = dev->_state;
        obj["discovery"] = dev->getDiscovery();
        obj["ready"] = (bool)dev->_ready;
        obj["i2cAddr"] = desc.i2cAddr;
        obj["channels"] = desc.channelCount;
        obj["capabilities"] = desc.capabilities;
        obj["manufacturer"] = desc.manufacturer;
        obj["busType"] = desc.bus.type;
        obj["busIndex"] = desc.bus.index;
        obj["pinA"] = desc.bus.pinA;
        obj["pinB"] = desc.bus.pinB;
        obj["busFreq"] = desc.bus.freqHz;
        obj["sampleRates"] = desc.sampleRatesMask;
        obj["legacyId"] = desc.legacyId;

        // For sensor devices, include live readings
        if (desc.type == HAL_DEV_SENSOR) {
            HalTempSensor* ts = static_cast<HalTempSensor*>(dev);
            obj["temperature"] = ts->getTemperature();
        }

        // Include per-device runtime config if available
        HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(dev->getSlot());
        if (cfg && cfg->valid) {
            obj["userLabel"] = cfg->userLabel;
            obj["cfgEnabled"] = cfg->enabled;
            obj["cfgI2sPort"] = cfg->i2sPort;
            obj["cfgVolume"] = cfg->volume;
            obj["cfgMute"] = cfg->mute;
            obj["cfgPinSda"] = cfg->pinSda;
            obj["cfgPinScl"] = cfg->pinScl;
        }
    }, &arr);

    String json;
    serializeJson(doc, json);
    webSocket.broadcastTXT(json.c_str());
}

void sendAudioChannelMap() {
    if (!_wsAnyAuth()) return;
    JsonDocument doc;
    doc["type"] = "audioChannelMap";

    // --- Input lanes (from pipeline registered sources) ---
    JsonArray inputs = doc["inputs"].to<JsonArray>();
    for (int lane = 0; lane < AUDIO_PIPELINE_MAX_INPUTS; lane++) {
        const AudioInputSource* src = audio_pipeline_get_source(lane);
        if (!src) continue;  // Skip empty/unregistered lanes

        JsonObject inp = inputs.add<JsonObject>();
        inp["lane"] = lane;
        inp["name"] = src->name ? src->name : "Unknown";
        inp["channels"] = 2;  // All input lanes are stereo
        inp["matrixCh"] = lane * 2;  // First mono channel in matrix

        // Enrich with HAL device info via reverse lookup
        int8_t halSlot = hal_pipeline_get_slot_for_adc_lane(lane);
        if (halSlot >= 0) {
            HalDevice* dev = HalDeviceManager::instance().getDevice(halSlot);
            if (dev) {
                const HalDeviceDescriptor& desc = dev->getDescriptor();
                inp["deviceName"] = desc.name;
                inp["compatible"] = desc.compatible;
                inp["manufacturer"] = desc.manufacturer;
                inp["capabilities"] = desc.capabilities;
                inp["ready"] = (bool)dev->_ready;
                inp["deviceType"] = (int)desc.type;
            }
        } else {
            // Software source not in bridge mapping (SigGen, USB, etc.)
            inp["deviceName"] = src->name ? src->name : "Unknown";
            inp["manufacturer"] = "";
            inp["capabilities"] = 0;
            inp["ready"] = src->isActive ? src->isActive() : false;
            inp["deviceType"] = (int)HAL_DEV_ADC;
        }
    }

    // --- Output sinks (from pipeline registered sinks) ---
    JsonArray outputs = doc["outputs"].to<JsonArray>();
    int sinkCount = audio_pipeline_get_sink_count();
    for (int s = 0; s < sinkCount; s++) {
        const AudioOutputSink* sink = audio_pipeline_get_sink(s);
        if (!sink) continue;
        JsonObject out = outputs.add<JsonObject>();
        out["index"] = s;
        out["name"] = sink->name;
        out["firstChannel"] = sink->firstChannel;
        out["channels"] = sink->channelCount;
        out["muted"] = sink->muted;

        // Find matching HAL device via O(1) halSlot lookup
        if (sink->halSlot != 0xFF) {
            HalDevice* dev = HalDeviceManager::instance().getDevice(sink->halSlot);
            if (dev) {
                const HalDeviceDescriptor& desc = dev->getDescriptor();
                out["compatible"] = desc.compatible;
                out["manufacturer"] = desc.manufacturer;
                out["capabilities"] = desc.capabilities;
                out["ready"] = (bool)dev->_ready;
                out["deviceType"] = (int)desc.type;
                out["i2cAddr"] = desc.i2cAddr;
            }
        }

        // Default ready state if no HAL device is bound to this sink
        if (!out.containsKey("ready")) {
            out["ready"] = (sink->isReady ? sink->isReady() : false);
            out["capabilities"] = 0;
            out["deviceType"] = (int)HAL_DEV_DAC;
        }
    }

    // --- Matrix dimensions ---
    doc["matrixInputs"] = AUDIO_PIPELINE_MATRIX_SIZE;
    doc["matrixOutputs"] = AUDIO_PIPELINE_MATRIX_SIZE;
    doc["matrixBypass"] = audio_pipeline_is_matrix_bypass();

    // --- Matrix gains ---
    JsonArray matrix = doc["matrix"].to<JsonArray>();
    for (int o = 0; o < AUDIO_PIPELINE_MATRIX_SIZE; o++) {
        JsonArray row = matrix.add<JsonArray>();
        for (int i = 0; i < AUDIO_PIPELINE_MATRIX_SIZE; i++) {
            row.add(serialized(String(audio_pipeline_get_matrix_gain(o, i), 4)));
        }
    }

    String json;
    serializeJson(doc, json);
    webSocket.broadcastTXT(json.c_str());
}
#endif

#ifdef USB_AUDIO_ENABLED
void sendUsbAudioState() {
  if (!_wsAnyAuth()) return;
  JsonDocument doc;
  doc["type"] = "usbAudioState";
  doc["enabled"] = appState.usbAudio.enabled;
  doc["connected"] = appState.usbAudio.connected;
  doc["streaming"] = appState.usbAudio.streaming;
  doc["sampleRate"] = appState.usbAudio.sampleRate;
  doc["bitDepth"] = appState.usbAudio.bitDepth;
  doc["channels"] = appState.usbAudio.channels;
  doc["volume"] = appState.usbAudio.volume;
  doc["volumeLinear"] = usb_audio_get_volume_linear();
  doc["mute"] = appState.usbAudio.mute;
  doc["overruns"]  = usb_audio_get_overruns();
  doc["underruns"] = usb_audio_get_underruns();
  doc["vuL"]             = appState.usbAudio.vuL;
  doc["vuR"]             = appState.usbAudio.vuR;
  doc["negotiatedRate"]  = appState.usbAudio.negotiatedRate;
  doc["negotiatedDepth"] = appState.usbAudio.negotiatedDepth;
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}
#endif

void sendMqttSettingsState() {
  if (!_wsAnyAuth()) return;
  JsonDocument doc;
  doc["type"] = "mqttSettings";
  doc["enabled"] = appState.mqtt.enabled;
  doc["broker"] = appState.mqtt.broker;
  doc["port"] = appState.mqtt.port;
  doc["username"] = appState.mqtt.username;
  doc["hasPassword"] = (appState.mqtt.password.length() > 0);
  doc["baseTopic"] = appState.mqtt.baseTopic;
  doc["haDiscovery"] = appState.mqtt.haDiscovery;
  doc["connected"] = appState.mqtt.connected;
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t *)json.c_str(), json.length());
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
  if (!appState.debug.debugMode) {
    if (cpuHooksInstalled) deinitCpuUsageMonitoring();
    return;
  }
  if (!_wsAnyAuth()) return;

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
  if (appState.debug.hwStats) {
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
    doc["audio"]["sampleRate"] = appState.audio.sampleRate;
    doc["audio"]["adcVref"] = appState.audio.adcVref;
    doc["audio"]["numAdcsDetected"] = appState.audio.numAdcsDetected;
    JsonArray adcArr = doc["audio"]["adcs"].to<JsonArray>();
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
      adcObj["status"] = statusStr;
      adcObj["noiseFloorDbfs"] = adc.noiseFloorDbfs;
      adcObj["i2sErrors"] = adc.i2sErrors;
      adcObj["consecutiveZeros"] = adc.consecutiveZeros;
      adcObj["totalBuffers"] = adc.totalBuffers;
      adcObj["vrms"] = adc.vrmsCombined;
      adcObj["snrDb"] = appState.audio.snrDb[a];
      adcObj["sfdrDb"] = appState.audio.sfdrDb[a];
    }
    doc["audio"]["fftWindowType"] = (int)appState.audio.fftWindowType;
    // DEPRECATED v1.14: flat fields — use audio.adcs[] array. Kept for backward compat.
    doc["audio"]["adcStatus"] = adcArr[0]["status"];
    doc["audio"]["noiseFloorDbfs"] = appState.audio.adc[0].noiseFloorDbfs;
    doc["audio"]["vrms"] = appState.audio.adc[0].vrmsCombined;

    // Uptime (milliseconds since boot)
    doc["uptime"] = millis();

    // Reset reason
    doc["resetReason"] = getResetReasonString();

    // Heap health
    doc["heapCritical"] = appState.debug.heapCritical;
    doc["heapWarning"] = appState.debug.heapWarning;

    // Heap budget breakdown
    {
      JsonArray budget = doc["heapBudget"].to<JsonArray>();
      for (int i = 0; i < heap_budget_count(); i++) {
          const HeapBudgetEntry* e = heap_budget_entry(i);
          if (!e) continue;
          JsonObject entry = budget.add<JsonObject>();
          entry["label"] = e->label;
          entry["bytes"] = e->bytes;
          entry["psram"] = e->isPsram;
      }
      doc["heapBudgetPsram"] = heap_budget_total_psram();
      doc["heapBudgetSram"]  = heap_budget_total_sram();
    }

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
    for (int a = 0; a < AUDIO_PIPELINE_MAX_INPUTS; a++) {
      doc["audio"]["adcs"][a]["i2sRecoveries"] = appState.audio.adc[a].i2sRecoveries;
    }

#ifdef DAC_ENABLED
    // DAC Output diagnostics (query HAL)
    {
      JsonObject dac = doc["dac"].to<JsonObject>();
      HalDevice* pcmDev = HalDeviceManager::instance().findByCompatible("ti,pcm5102a");
      HalDeviceConfig* pcmCfg = pcmDev ? HalDeviceManager::instance().getConfig(pcmDev->getSlot()) : nullptr;
      dac["enabled"] = pcmCfg ? pcmCfg->enabled : false;
      dac["ready"] = pcmDev ? pcmDev->_ready : false;
      dac["detected"] = (pcmDev != nullptr);
      dac["model"] = pcmDev ? pcmDev->getDescriptor().name : "PCM5102A";
      dac["deviceId"] = pcmDev ? pcmDev->getDescriptor().legacyId : 0x0001;
      dac["volume"] = pcmCfg ? pcmCfg->volume : 80;
      dac["mute"] = pcmCfg ? pcmCfg->mute : false;
      dac["filterMode"] = pcmCfg ? pcmCfg->filterMode : 0;
      dac["outputChannels"] = pcmDev ? pcmDev->getDescriptor().channelCount : 2;
      dac["txUnderruns"] = appState.dac.txUnderruns;
      // Device capabilities from HAL descriptor
      if (pcmDev) {
        const HalDeviceDescriptor& desc = pcmDev->getDescriptor();
        dac["manufacturer"] = desc.manufacturer;
        HalAudioDevice* audioDev = _audioDeviceForSinkSlot(0);
        dac["hwVolume"] = audioDev ? audioDev->hasHardwareVolume() : false;
        dac["i2cControl"] = (desc.i2cAddr != 0);
        dac["independentClock"] = false;
        dac["hasFilters"] = false;
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
      const EepromDiag& ed = appState.dac.eepromDiag;
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

    // Pin configuration table — firmware-defined, correct per target board
    {
      JsonArray pins = doc["pins"].to<JsonArray>();
      struct { const char *d; const char *f; int g; const char *c; } pm[] = {
        {"PCM1808 ADC 1&2", "BCK",   I2S_BCK_PIN,      "audio"},
        {"PCM1808 ADC 1",   "DOUT",  I2S_DOUT_PIN,     "audio"},
        {"PCM1808 ADC 2",   "DOUT2", I2S_DOUT2_PIN,    "audio"},
        {"PCM1808 ADC 1&2", "LRC",   I2S_LRC_PIN,      "audio"},
        {"PCM1808 ADC 1&2", "MCLK",  I2S_MCLK_PIN,     "audio"},
#ifdef DAC_ENABLED
        {"DAC Output",      "DOUT",  I2S_TX_DATA_PIN,   "audio"},
        {"DAC I2C",         "SDA",   DAC_I2C_SDA_PIN,   "audio"},
        {"DAC I2C",         "SCL",   DAC_I2C_SCL_PIN,   "audio"},
#endif
        {"ST7735S TFT",     "CS",    TFT_CS_PIN,        "display"},
        {"ST7735S TFT",     "MOSI",  TFT_MOSI_PIN,     "display"},
        {"ST7735S TFT",     "CLK",   TFT_SCLK_PIN,     "display"},
        {"ST7735S TFT",     "DC",    TFT_DC_PIN,        "display"},
        {"ST7735S TFT",     "RST",   TFT_RST_PIN,      "display"},
        {"ST7735S TFT",     "BL",    TFT_BL_PIN,        "display"},
        {"EC11 Encoder",    "A",     ENCODER_A_PIN,      "input"},
        {"EC11 Encoder",    "B",     ENCODER_B_PIN,      "input"},
        {"EC11 Encoder",    "SW",    ENCODER_SW_PIN,     "input"},
        {"Piezo Buzzer",    "IO",    BUZZER_PIN,         "core"},
        {"Status LED",      "LED",   LED_PIN,            "core"},
        {"Relay Module",    "Amp",   AMPLIFIER_PIN,      "core"},
        {"Tactile Switch",  "Btn",   RESET_BUTTON_PIN,   "core"},
        {"Signal Generator","PWM",   SIGGEN_PWM_PIN,     "core"},
#if CONFIG_IDF_TARGET_ESP32P4
        {"ES8311 DAC",      "I2S TX",  9,               "audio"},
        {"ES8311 DAC",      "PA Ctrl", 53,              "audio"},
#endif
      };
      for (auto &p : pm) {
        JsonObject pin = pins.add<JsonObject>();
        pin["g"] = p.g;
        pin["f"] = p.f;
        pin["d"] = p.d;
        pin["c"] = p.c;
      }
    }

#ifdef DSP_ENABLED
    // DSP diagnostics
    {
      JsonObject dsp = doc["dsp"].to<JsonObject>();
      dsp["swapFailures"] = appState.dsp.swapFailures;
      dsp["swapSuccesses"] = appState.dsp.swapSuccesses;
      unsigned long timeSinceFailure = appState.dsp.lastSwapFailure > 0 ? (millis() - appState.dsp.lastSwapFailure) : 0;
      dsp["lastSwapFailureAgo"] = timeSinceFailure;
    }
#endif
  }

  // === I2S Metrics sections (gated by debugI2sMetrics) ===
  if (appState.debug.i2sMetrics) {
    // I2S Static Config
    I2sStaticConfig i2sCfg = i2s_audio_get_static_config();
    JsonArray i2sCfgArr = doc["audio"]["i2sConfig"].to<JsonArray>();
    for (int a = 0; a < AUDIO_PIPELINE_MAX_INPUTS; a++) {
      JsonObject c = i2sCfgArr.add<JsonObject>();
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

    // I2S Runtime Metrics
    JsonObject i2sRt = doc["audio"]["i2sRuntime"].to<JsonObject>();
    i2sRt["stackFree"] = appState.audio.i2sMetrics.audioTaskStackFree;
    JsonArray bpsArr = i2sRt["buffersPerSec"].to<JsonArray>();
    JsonArray latArr = i2sRt["avgReadLatencyUs"].to<JsonArray>();
    for (int a = 0; a < AUDIO_PIPELINE_MAX_INPUTS; a++) {
      bpsArr.add(serialized(String(appState.audio.i2sMetrics.buffersPerSec[a], 1)));
      latArr.add(serialized(String(appState.audio.i2sMetrics.avgReadLatencyUs[a], 0)));
    }
  }

  // === Task Monitor section (gated by debugTaskMonitor) ===
  // Note: task_monitor_update() runs on its own 5s timer in main loop
  if (appState.debug.taskMonitor) {
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
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}

// ===== Deferred Initial-State Drain =====
// Sends up to 2 state messages per call, one client at a time.
// Called from main loop after webSocket.loop() to spread WiFi TX load.

void drainPendingInitState() {
    for (int c = 0; c < MAX_WS_CLIENTS; c++) {
        uint32_t &pending = _pendingInitState[c];
        if (!pending || !wsAuthStatus[c]) continue;

        int sent = 0;
        const int MAX_PER_ITER = 1;

        if (sent < MAX_PER_ITER && (pending & INIT_WIFI))        { sendWiFiStatus();                  pending &= ~INIT_WIFI;        sent++; }
        if (sent < MAX_PER_ITER && (pending & INIT_SENSING))     { sendSmartSensingStateInternal();   pending &= ~INIT_SENSING;     sent++; }
        if (sent < MAX_PER_ITER && (pending & INIT_DISPLAY))     { sendDisplayState();                pending &= ~INIT_DISPLAY;     sent++; }
        if (sent < MAX_PER_ITER && (pending & INIT_BUZZER))      { sendBuzzerState();                 pending &= ~INIT_BUZZER;      sent++; }
        if (sent < MAX_PER_ITER && (pending & INIT_SIGGEN))      { sendSignalGenState();              pending &= ~INIT_SIGGEN;      sent++; }
        if (sent < MAX_PER_ITER && (pending & INIT_AUDIO_GRAPH)) { sendAudioGraphState();             pending &= ~INIT_AUDIO_GRAPH; sent++; }
        if (sent < MAX_PER_ITER && (pending & INIT_DEBUG))       { sendDebugState();                  pending &= ~INIT_DEBUG;       sent++; }
        if (sent < MAX_PER_ITER && (pending & INIT_ADC_STATE)) {
            JsonDocument adcDoc;
            adcDoc["type"] = "adcState";
            JsonArray arr = adcDoc["enabled"].to<JsonArray>();
            for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) arr.add(appState.audio.adcEnabled[i]);
            String adcJson;
            serializeJson(adcDoc, adcJson);
            webSocket.sendTXT(c, adcJson.c_str());
            pending &= ~INIT_ADC_STATE;
            sent++;
        }
#ifdef DSP_ENABLED
        if (sent < MAX_PER_ITER && (pending & INIT_DSP))         { sendDspState();                    pending &= ~INIT_DSP;         sent++; }
#else
        pending &= ~INIT_DSP;
#endif
#ifdef DAC_ENABLED
        if (sent < MAX_PER_ITER && (pending & INIT_DAC))         { sendDacState();                    pending &= ~INIT_DAC;         sent++; }
        if (sent < MAX_PER_ITER && (pending & INIT_HAL_DEVICE))  { sendHalDeviceState();              pending &= ~INIT_HAL_DEVICE;  sent++; }
        if (sent < MAX_PER_ITER && (pending & INIT_CHANNEL_MAP)) { sendAudioChannelMap();             pending &= ~INIT_CHANNEL_MAP; sent++; }
#else
        pending &= ~INIT_DAC;
        pending &= ~INIT_HAL_DEVICE;
        pending &= ~INIT_CHANNEL_MAP;
#endif
#ifdef USB_AUDIO_ENABLED
        if (sent < MAX_PER_ITER && (pending & INIT_USB_AUDIO))   { sendUsbAudioState();               pending &= ~INIT_USB_AUDIO;   sent++; }
#else
        pending &= ~INIT_USB_AUDIO;
#endif
        if (sent < MAX_PER_ITER && (pending & INIT_UPDATED)) {
            if (appState.ota.justUpdated) broadcastJustUpdated();
            pending &= ~INIT_UPDATED;
            sent++;
        }

        break; // Only drain one client per call to avoid starving the loop
    }
}

// ===== Audio Streaming to Subscribed Clients =====

void sendAudioData() {
  if (httpServingPage) return;
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
    doc["audioLevel"] = appState.audio.level_dBFS;
    doc["signalDetected"] = (appState.audio.level_dBFS >= appState.audio.threshold_dBFS);
    doc["numAdcsDetected"] = appState.audio.numAdcsDetected;
    // Per-ADC data array
    JsonArray adcArr = doc["adc"].to<JsonArray>();
    JsonArray adcStatusArr = doc["adcStatus"].to<JsonArray>();
    JsonArray adcNoiseArr = doc["adcNoiseFloor"].to<JsonArray>();
    for (int a = 0; a < AUDIO_PIPELINE_MAX_INPUTS; a++) {
      const AdcState &adc = appState.audio.adc[a];
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
    // DEPRECATED v1.14: flat fields — use adcs[] array. Kept for backward compat.
    doc["audioRms1"] = appState.audio.adc[0].rms1;
    doc["audioRms2"] = appState.audio.adc[0].rms2;
    doc["audioVu1"] = appState.audio.adc[0].vu1;
    doc["audioVu2"] = appState.audio.adc[0].vu2;
    doc["audioPeak1"] = appState.audio.adc[0].peak1;
    doc["audioPeak2"] = appState.audio.adc[0].peak2;
    doc["audioPeak"] = appState.audio.adc[0].peakCombined;
    doc["audioVrms1"] = appState.audio.adc[0].vrms1;
    doc["audioVrms2"] = appState.audio.adc[0].vrms2;
    doc["audioVrms"] = appState.audio.adc[0].vrmsCombined;
    // Output sink VU data
    JsonArray sinkArr = doc["sinks"].to<JsonArray>();
    int sinkCnt = audio_pipeline_get_sink_count();
    for (int s = 0; s < sinkCnt; s++) {
        const AudioOutputSink* sk = audio_pipeline_get_sink(s);
        if (!sk) continue;
        JsonObject sinkObj = sinkArr.add<JsonObject>();
        sinkObj["vuL"] = sk->vuL;
        sinkObj["vuR"] = sk->vuR;
        sinkObj["name"] = sk->name ? sk->name : "";
        sinkObj["ch"] = sk->firstChannel;
    }
    String json;
    serializeJson(doc, json);
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
      if (_audioSubscribed[i]) {
        webSocket.sendTXT(i, (uint8_t*)json.c_str(), json.length());
      }
    }
  }

  // --- Waveform/Spectrum data — alternated each call to reduce WiFi TX burst ---
  // One call sends waveform, the next sends spectrum (audio levels always sent).
  static bool _sendWaveformNext = true;

  // Graduated heap pressure: warning = halve binary rate, critical = suppress entirely
  static bool _heapSkipBinaryFrame = false;
  if (appState.debug.heapWarning) { _heapSkipBinaryFrame = !_heapSkipBinaryFrame; }
  else { _heapSkipBinaryFrame = false; }
  const bool heapAllowBinary = !appState.debug.heapCritical && !_heapSkipBinaryFrame;

  if (_sendWaveformNext) {
    // --- Waveform data (per-ADC) — binary: [type:1][adc:1][samples:256] ---
    if (appState.audio.waveformEnabled && heapAllowBinary) {
      uint8_t wfBin[2 + WAVEFORM_BUFFER_SIZE]; // 258 bytes
      wfBin[0] = WS_BIN_WAVEFORM;
      for (int a = 0; a < appState.audio.numAdcsDetected; a++) {
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
  } else {
    // --- Spectrum data (per-ADC) — binary: [type:1][adc:1][freq:f32LE][bands:Nxf32LE] ---
    if (appState.audio.spectrumEnabled && heapAllowBinary) {
      uint8_t spBin[2 + sizeof(float) + SPECTRUM_BANDS * sizeof(float)]; // 70 bytes
      spBin[0] = WS_BIN_SPECTRUM;
      float bands[SPECTRUM_BANDS];
      float freq = 0.0f;
      for (int a = 0; a < appState.audio.numAdcsDetected; a++) {
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
  _sendWaveformNext = !_sendWaveformNext;
}
