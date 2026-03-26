// websocket_command.cpp — WebSocket event handler (connect/disconnect/auth/commands)
// and deferred initial-state drain logic.
// Broadcast functions live in websocket_broadcast.cpp.
// Auth state lives in websocket_auth.cpp.
// CPU monitoring lives in websocket_cpu_monitor.cpp.
// Public API declared in websocket_handler.h.

#include <cmath>
#include "websocket_handler.h"
#include "websocket_internal.h"
#include "auth_handler.h"
#include "config.h"
#include "app_state.h"
#include "globals.h"
#include "settings_manager.h"
#include "wifi_manager.h"
#include "smart_sensing.h"
#include "ota_updater.h"
#include "debug_serial.h"
#include "utils.h"
#include "i2s_audio.h"
#include "signal_generator.h"
#include "audio_pipeline.h"
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
#endif
#ifdef USB_AUDIO_ENABLED
#include "usb_audio.h"
#endif
#include "eth_manager.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

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
    INIT_USB_AUDIO   = (1u << 10),
    INIT_UPDATED     = (1u << 11),
    INIT_HAL_DEVICE    = (1u << 13),
    INIT_CHANNEL_MAP   = (1u << 14),
    INIT_HEALTH_CHECK  = (1u << 15),
    INIT_ALL           = 0xFFFFu,
};

// ===== WebSocket Event Handler =====

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      LOG_I("[WebSocket] Client [%u] disconnected", num);
      if (wsAuthStatus[num]) ws_auth_decrement();
      wsAuthStatus[num] = false;
      wsAuthTimeout[num] = 0;
      ws_clear_session_id(num);
      ws_set_audio_subscribed(num, false);
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
        if (length > 4096) {
            LOG_W("[WebSocket] Message too large (%u bytes), rejected", length);
            return;
        }

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
            ws_auth_increment();
            wsAuthTimeout[num] = 0;
            ws_set_session_id(num, sessionId);
            webSocket.sendTXT(num, "{\"type\":\"authSuccess\"}");
            webSocket.sendTXT(num, "{\"type\":\"protocolVersion\",\"version\":\"" WS_PROTOCOL_VERSION "\"}");
            LOG_D("[WebSocket] Client [%u] authenticated (total: %u)", num, ws_auth_count());

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
        if (!wsAuthStatus[num] || !validateSession(ws_get_session_id(num))) {
          wsAuthStatus[num] = false;
          ws_clear_session_id(num);
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
        } else if (msgType == "getHealthCheck") {
          sendHealthCheckState();
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
          ws_set_audio_subscribed(num, enabled);
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
              const char* name = names[i] | "";
              if (name[0] != '\0') strlcpy(appState.audio.inputNames[i], name, sizeof(appState.audio.inputNames[i]));
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
        else if (msgType == "setMultibandComp") {
          // Update multiband compressor per-band params and/or crossover frequencies.
          // Message fields:
          //   ch (int)         — DSP channel index (0-3)
          //   stage (int)      — Stage index of the DSP_MULTIBAND_COMP stage
          //   numBands (int)   — Optional: set number of active bands (2-4)
          //   bands (array)    — Optional: per-band objects with thresholdDb, ratio, attackMs,
          //                      releaseMs, kneeDb, makeupGainDb
          //   crossoverFreqs (array) — Optional: crossover boundary frequencies in Hz (up to 3)
          int ch = doc["ch"] | -1;
          int si = doc["stage"] | -1;
          if (ch >= 0 && ch < DSP_MAX_CHANNELS) {
            dsp_copy_active_to_inactive();
            DspState *cfg = dsp_get_inactive_config();
            bool changed = false;
            if (si >= 0 && si < cfg->channels[ch].stageCount) {
              DspStage &s = cfg->channels[ch].stages[si];
              if (s.type == DSP_MULTIBAND_COMP) {
                // Update numBands in the stage struct (double-buffered)
                if (doc["numBands"].is<int>()) {
                  uint8_t nb = doc["numBands"].as<uint8_t>();
                  if (nb >= 2 && nb <= 4) { s.multibandComp.numBands = nb; changed = true; }
                }
                int mbSlot = s.multibandComp.mbSlot;
                // Pool writes happen after swap to avoid data race with audio task.
                // First collect numBands change into inactive config, swap, then write pool.
                if (changed) {
                  if (!dsp_swap_config()) { dsp_log_swap_failure("WebSocket"); }
                }
                // Now safe to write pool — audio task reads the newly-active config
                // Update per-band params in the pool
                if (mbSlot >= 0 && doc["bands"].is<JsonArray>()) {
                  JsonArray bands = doc["bands"].as<JsonArray>();
                  int bIdx = 0;
                  for (JsonObject band : bands) {
                    if (bIdx >= 4) break;
                    float thresh = band["thresholdDb"] | -12.0f;
                    float attack = band["attackMs"] | 10.0f;
                    float release = band["releaseMs"] | 100.0f;
                    float ratio   = band["ratio"] | 4.0f;
                    float knee    = band["kneeDb"] | 6.0f;
                    float makeup  = band["makeupGainDb"] | 0.0f;
                    dsp_mb_set_band_params(mbSlot, bIdx, thresh, attack, release, ratio, knee, makeup);
                    bIdx++;
                    changed = true;
                  }
                }
                // Update crossover frequencies in the pool
                if (mbSlot >= 0 && doc["crossoverFreqs"].is<JsonArray>()) {
                  JsonArray freqs = doc["crossoverFreqs"].as<JsonArray>();
                  int fIdx = 0;
                  for (JsonVariant freq : freqs) {
                    if (fIdx >= 3) break;
                    dsp_mb_set_crossover_freq(mbSlot, fIdx, freq.as<float>(), cfg->sampleRate);
                    fIdx++;
                    changed = true;
                  }
                }
              }
            }
            if (changed) {
              extern void saveDspSettingsDebounced();
              saveDspSettingsDebounced();
              appState.markDspConfigDirty();
              LOG_I("[WebSocket] setMultibandComp ch=%d stage=%d", ch, si);
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
            if (db < -60.0f) db = -60.0f;
            if (db > 12.0f)  db = 12.0f;
            float gainLinear = powf(10.0f, db / 20.0f);
            audio_pipeline_set_source_gain(lane, gainLinear);
            LOG_I("[WebSocket] Input gain lane=%d db=%.1f gainLinear=%.4f", lane, db, gainLinear);
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
#ifdef DSP_ENABLED
          int chL = lane * 2;
          int chR = lane * 2 + 1;
          if (lane >= 0 && chR < DSP_MAX_CHANNELS) {
            dsp_copy_active_to_inactive();
            DspState *cfg = dsp_get_inactive_config();
            // Find-or-create DSP_POLARITY stage in chain region for both L and R channels
            for (int ch = chL; ch <= chR; ch++) {
              DspChannelConfig &chCfg = cfg->channels[ch];
              bool found = false;
              for (int s = DSP_PEQ_BANDS; s < chCfg.stageCount; s++) {
                if (chCfg.stages[s].type == DSP_POLARITY) {
                  chCfg.stages[s].polarity.inverted = inverted;
                  chCfg.stages[s].enabled = true;
                  found = true;
                  break;
                }
              }
              if (!found) {
                int idx = dsp_add_chain_stage(ch, DSP_POLARITY);
                if (idx >= 0) {
                  cfg->channels[ch].stages[idx].polarity.inverted = inverted;
                  cfg->channels[ch].stages[idx].enabled = true;
                }
              }
            }
            if (!dsp_swap_config()) { dsp_log_swap_failure("WebSocket"); }
            extern void saveDspSettingsDebounced();
            saveDspSettingsDebounced();
            appState.markDspConfigDirty();
            LOG_I("[WebSocket] Input phase lane=%d inverted=%d", lane, inverted);
          }
#else
          LOG_I("[WebSocket] Input phase lane=%d inverted=%d (DSP not enabled)", lane, inverted);
#endif
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
            output_dsp_copy_active_to_inactive();
            OutputDspState *cfg = output_dsp_get_inactive_config();
            uint32_t sampleRate = cfg->sampleRate > 0 ? cfg->sampleRate : 48000;
            uint16_t delaySamples = (uint16_t)((ms * sampleRate) / 1000.0f);
            if (delaySamples > OUTPUT_DSP_MAX_DELAY_SAMPLES)
              delaySamples = OUTPUT_DSP_MAX_DELAY_SAMPLES;
            OutputDspChannelConfig &chCfg = cfg->channels[ch];
            bool found = false;
            for (int s = 0; s < chCfg.stageCount; s++) {
              if (chCfg.stages[s].type == DSP_DELAY) {
                chCfg.stages[s].delay.delaySamples = delaySamples;
                found = true;
                break;
              }
            }
            if (!found) {
              int idx = output_dsp_add_stage(ch, DSP_DELAY);
              if (idx >= 0) {
                cfg->channels[ch].stages[idx].delay.delaySamples = delaySamples;
              }
            }
            output_dsp_swap_config();
            output_dsp_save_channel(ch);
            LOG_I("[WebSocket] Output delay ch=%d ms=%.2f samples=%u", ch, ms, delaySamples);
          }
        }
        else if (msgType == "setOutputCrossover") {
          int subCh = doc["subCh"] | -1;
          int mainCh = doc["mainCh"] | -1;
          float freqHz = doc["freqHz"] | 80.0f;
          int order = doc["order"] | 4;
          if (subCh >= 0 && subCh < OUTPUT_DSP_MAX_CHANNELS &&
              mainCh >= 0 && mainCh < OUTPUT_DSP_MAX_CHANNELS &&
              freqHz > 0.0f) {
            output_dsp_copy_active_to_inactive();
            int result = output_dsp_setup_crossover(subCh, mainCh, freqHz, order);
            if (result >= 0) {
              output_dsp_swap_config();
              output_dsp_save_channel(subCh);
              output_dsp_save_channel(mainCh);
              LOG_I("[WebSocket] Output crossover: LR%d %.0fHz sub=ch%d main=ch%d stagesAdded=%d",
                    order, freqHz, subCh, mainCh, result);
            } else {
              LOG_I("[WebSocket] Output crossover failed: invalid params subCh=%d mainCh=%d freqHz=%.0f order=%d",
                    subCh, mainCh, freqHz, order);
            }
          }
        }
#endif

        // ===== Ethernet Configuration =====
        else if (msgType == "setEthConfig") {
          bool useStatic = doc["useStaticIP"] | false;

          // Apply hostname if provided (RFC 1123: [a-zA-Z0-9-], no leading/trailing hyphen)
          if (doc["hostname"].is<const char*>()) {
            String h = doc["hostname"].as<String>();
            bool valid = h.length() >= 1 && h.length() <= 63;
            if (valid && (h[0] == '-' || h[h.length() - 1] == '-')) valid = false;
            if (valid) {
              for (unsigned int i = 0; i < h.length(); i++) {
                char c = h[i];
                if (!isalnum(c) && c != '-') { valid = false; break; }
              }
            }
            if (valid) {
              appState.ethernet.hostname = h;
            } else {
              LOG_W("[WebSocket] setEthConfig: invalid hostname");
            }
          }

          if (useStatic) {
            if (!doc["staticIP"].is<const char*>()) {
              LOG_W("[WebSocket] setEthConfig: missing staticIP");
              return;
            }
            IPAddress test;
            if (!test.fromString(doc["staticIP"].as<const char*>())) {
              LOG_W("[WebSocket] setEthConfig: invalid staticIP");
              return;
            }
            appState.ethernet.useStaticIP = true;
            appState.ethernet.staticIP = doc["staticIP"].as<String>();
            if (doc["subnet"].is<const char*>()) appState.ethernet.staticSubnet = doc["subnet"].as<String>();
            if (doc["gateway"].is<const char*>()) appState.ethernet.staticGateway = doc["gateway"].as<String>();
            if (doc["dns1"].is<const char*>()) appState.ethernet.staticDns1 = doc["dns1"].as<String>();
            if (doc["dns2"].is<const char*>()) appState.ethernet.staticDns2 = doc["dns2"].as<String>();
          } else {
            appState.ethernet.useStaticIP = false;
          }

          eth_manager_apply_config();
          if (useStatic) {
            eth_manager_start_confirm_timer();
          } else {
            saveSettings();
          }
          appState.markEthernetDirty();
          LOG_I("[WebSocket] Ethernet config updated");
          sendWiFiStatus();
        }
        else if (msgType == "confirmEthConfig") {
          eth_manager_confirm_config();
          LOG_I("[WebSocket] Ethernet config confirmed");
          sendWiFiStatus();
        }
        else if (msgType == "setHostname") {
          if (!doc["hostname"].is<const char*>()) {
            LOG_W("[WebSocket] setHostname: missing hostname");
            return;
          }
          String h = doc["hostname"].as<String>();
          // RFC 1123: 1-63 chars, [a-zA-Z0-9-], no leading/trailing hyphen
          bool valid = h.length() >= 1 && h.length() <= 63;
          if (valid && (h[0] == '-' || h[h.length() - 1] == '-')) valid = false;
          if (valid) {
            for (unsigned int i = 0; i < h.length(); i++) {
              char c = h[i];
              if (!isalnum(c) && c != '-') { valid = false; break; }
            }
          }
          if (!valid) {
            LOG_W("[WebSocket] setHostname: invalid hostname");
            return;
          }
          appState.ethernet.hostname = h;
          saveSettings();
          appState.markEthernetDirty();
          LOG_I("[WebSocket] Hostname set to: %s", h.c_str());
          sendWiFiStatus();
        }
      }
      break;

    default:
      break;
  }
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
        if (sent < MAX_PER_ITER && (pending & INIT_HAL_DEVICE))  { sendHalDeviceState();              pending &= ~INIT_HAL_DEVICE;  sent++; }
        if (sent < MAX_PER_ITER && (pending & INIT_CHANNEL_MAP)) { sendAudioChannelMap();             pending &= ~INIT_CHANNEL_MAP; sent++; }
#else
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
        if (sent < MAX_PER_ITER && (pending & INIT_HEALTH_CHECK)) { sendHealthCheckState();            pending &= ~INIT_HEALTH_CHECK; sent++; }

        break; // Only drain one client per call to avoid starving the loop
    }
}

