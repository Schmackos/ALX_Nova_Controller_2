// websocket_broadcast.cpp — WebSocket state broadcasting functions
// Extracted from websocket_handler.cpp for modularity.
// Functions declared in websocket_handler.h.

#include "websocket_handler.h"
#include "websocket_internal.h"
#include "config.h"
#include "app_state.h"
#include "globals.h"
#include "diag_journal.h"
#include "diag_event.h"
#include "crash_log.h"
#include "debug_serial.h"
#include "utils.h"
#include "i2s_audio.h"
#include "task_monitor.h"
#include "audio_pipeline.h"
#include "audio_input_source.h"
#include "audio_output_sink.h"
#include "heap_budget.h"
#include "psram_alloc.h"
#ifdef DSP_ENABLED
#include "dsp_pipeline.h"
#endif
#ifdef DAC_ENABLED
#include "dac_hal.h"
#include "dac_eeprom.h"
#include "hal/hal_device_manager.h"
#include "hal/hal_audio_device.h"
#include "hal/hal_pipeline_bridge.h"
#include "hal/hal_types.h"
#include "hal/hal_temp_sensor.h"
#include "hal/hal_driver_registry.h"
#endif
#ifdef USB_AUDIO_ENABLED
#include "usb_audio.h"
#endif
#include <WiFi.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// ===== HAL Device Lookup Helpers =====
#ifdef DAC_ENABLED
HalAudioDevice* ws_audio_device_for_sink_slot(uint8_t sinkSlot) {
    int8_t halSlot = hal_pipeline_get_slot_for_sink(sinkSlot);
    if (halSlot < 0) return nullptr;
    HalDevice* dev = HalDeviceManager::instance().getDevice((uint8_t)halSlot);
    if (!dev) return nullptr;
    if (dev->getType() != HAL_DEV_DAC && dev->getType() != HAL_DEV_CODEC) return nullptr;
    return static_cast<HalAudioDevice*>(dev);
}
#endif

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
  if (!ws_any_auth()) return;
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
  if (!ws_any_auth()) return;
  DspMetrics m = dsp_get_metrics();
  PipelineTimingMetrics timing = audio_pipeline_get_timing();
  JsonDocument doc;
  doc["type"] = "dspMetrics";
  doc["processTimeUs"] = m.processTimeUs;
  doc["cpuLoad"] = m.cpuLoadPercent;
  JsonArray gr = doc["limiterGr"].to<JsonArray>();
  for (int i = 0; i < DSP_MAX_CHANNELS; i++) gr.add(m.limiterGrDb[i]);
  // Pipeline-wide timing metrics (from audio_pipeline_task, Core 1)
  doc["pipelineCpu"]    = timing.totalCpuPercent;
  doc["pipelineFrameUs"] = timing.totalFrameUs;
  doc["matrixUs"]       = timing.matrixMixUs;
  doc["outputDspUs"]    = timing.outputDspUs;
  // DSP threshold flags and FIR bypass counter
  doc["dspCpuWarn"]     = m.cpuWarning;
  doc["dspCpuCrit"]     = m.cpuCritical;
  doc["firBypassCount"] = m.firBypassCount;
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}
#endif

void sendHalDeviceState() {
    if (!ws_any_auth()) return;
    JsonDocument doc;
    doc["type"] = "halDeviceState";
    doc["scanning"] = appState._halScanInProgress;
    doc["deviceCount"] = HalDeviceManager::instance().getCount();
    doc["deviceMax"] = HAL_MAX_DEVICES;
    doc["driverCount"] = hal_registry_count();
    doc["driverMax"] = hal_registry_max();

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

        // Surface last init error for devices in error/unavailable state
        if (dev->_state == HAL_STATE_ERROR || dev->_state == HAL_STATE_UNAVAILABLE) {
            if (dev->_lastError[0]) {
                obj["errorReason"] = dev->getLastError();
            }
        }

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
    if (!ws_any_auth()) return;
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

#ifdef USB_AUDIO_ENABLED
void sendUsbAudioState() {
  if (!ws_any_auth()) return;
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
  if (!ws_any_auth()) return;
  JsonDocument doc;
  doc["type"] = "mqttSettings";
  doc["enabled"] = appState.mqtt.enabled;
  doc["broker"] = appState.mqtt.broker;
  doc["port"] = appState.mqtt.port;
  doc["username"] = appState.mqtt.username;
  doc["hasPassword"] = (appState.mqtt.password.length() > 0);
  doc["baseTopic"] = appState.mqtt.baseTopic;
  doc["haDiscovery"] = appState.mqtt.haDiscovery;
  doc["useTls"] = appState.mqtt.useTls;
  doc["verifyCert"] = appState.mqtt.verifyCert;
  doc["connected"] = appState.mqtt.connected;
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t *)json.c_str(), json.length());
}

void sendHardwareStats() {
  // Master debug gate — if debug mode is off, deregister hooks and send nothing
  if (!appState.debug.debugMode) {
    deinitCpuUsageMonitoring();
    return;
  }
  if (!ws_any_auth()) return;

  JsonDocument doc;
  doc["type"] = "hardware_stats";

  // === CPU stats — always included when debugMode is on ===
  updateCpuUsage();
  doc["cpu"]["freqMHz"] = ESP.getCpuFreqMHz();
  doc["cpu"]["model"] = ESP.getChipModel();
  doc["cpu"]["revision"] = ESP.getChipRevision();
  doc["cpu"]["cores"] = ESP.getChipCores();
  // During warm-up, report -1 (UI shows "Calibrating...")
  float cpu0 = getCpuUsageCore0();
  float cpu1 = getCpuUsageCore1();
  bool cpuValid = (cpu0 >= 0 && cpu1 >= 0);
  doc["cpu"]["usageCore0"] = cpuValid ? cpu0 : -1;
  doc["cpu"]["usageCore1"] = cpuValid ? cpu1 : -1;
  doc["cpu"]["usageTotal"] = cpuValid ? (cpu0 + cpu1) / 2.0f : -1;
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

    // WebSocket client count
    doc["wsClientCount"] = ws_auth_count();
    doc["wsClientMax"] = (uint8_t)MAX_WS_CLIENTS;

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

    // Uptime (milliseconds since boot)
    doc["uptime"] = millis();

    // Reset reason
    doc["resetReason"] = getResetReasonString();

    // Heap health
    doc["heapCritical"] = appState.debug.heapCritical;
    doc["heapWarning"] = appState.debug.heapWarning;
    if (appState.audio.dmaAllocFailed) {
        doc["dmaAllocFailed"] = true;
        doc["dmaAllocFailMask"] = appState.audio.dmaAllocFailMask;
    }

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

    // PSRAM allocation tracker stats
    {
        PsramAllocStats ps = psram_get_stats();
        doc["psramFallbackCount"] = ps.fallbackCount;
        doc["psramFailedCount"]   = ps.failedCount;
        doc["psramAllocPsram"]    = ps.activePsramBytes;
        doc["psramAllocSram"]     = ps.activeSramBytes;
    }
    doc["psramWarning"]  = appState.debug.psramWarning;
    doc["psramCritical"] = appState.debug.psramCritical;

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
        HalAudioDevice* audioDev = ws_audio_device_for_sink_slot(0);
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
// ===== Audio Streaming to Subscribed Clients =====

void sendAudioData() {
  if (httpServingPage) return;
  // Early return if no clients are subscribed
  bool anySubscribed = false;
  for (int i = 0; i < MAX_WS_CLIENTS; i++) {
    if (ws_is_audio_subscribed(i)) {
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
      if (ws_is_audio_subscribed(i)) {
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

  // Client-count adaptive rate: skip binary frames based on authenticated client count
  ws_auth_recalibrate();
  static uint8_t _clientSkipCounter = 0;
  _clientSkipCounter++;
  uint8_t skipFactor = 1;
  uint8_t authCount = ws_auth_count();
  if (authCount >= 8) skipFactor = WS_BINARY_SKIP_8PLUS;
  else if (authCount >= 5) skipFactor = WS_BINARY_SKIP_5PLUS;
  else if (authCount >= 3) skipFactor = WS_BINARY_SKIP_3PLUS;
  else if (authCount == 2) skipFactor = WS_BINARY_SKIP_2_CLIENTS;
  const bool clientAllowBinary = (_clientSkipCounter % skipFactor) == 0;

  // Combined gate: both heap pressure AND client count must allow binary
  const bool allowBinary = heapAllowBinary && clientAllowBinary;

  if (_sendWaveformNext) {
    // --- Waveform data (per-ADC) — binary: [type:1][adc:1][samples:256] ---
    if (appState.audio.waveformEnabled && allowBinary) {
      uint8_t wfBin[2 + WAVEFORM_BUFFER_SIZE]; // 258 bytes
      wfBin[0] = WS_BIN_WAVEFORM;
      for (int a = 0; a < appState.audio.numAdcsDetected; a++) {
        if (i2s_audio_get_waveform(wfBin + 2, a)) {
          wfBin[1] = (uint8_t)a;
          for (int i = 0; i < MAX_WS_CLIENTS; i++) {
            if (ws_is_audio_subscribed(i)) {
              webSocket.sendBIN(i, wfBin, sizeof(wfBin));
            }
          }
        }
      }
    }
  } else {
    // --- Spectrum data (per-ADC) — binary: [type:1][adc:1][freq:f32LE][bands:Nxf32LE] ---
    if (appState.audio.spectrumEnabled && allowBinary) {
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
            if (ws_is_audio_subscribed(i)) {
              webSocket.sendBIN(i, spBin, sizeof(spBin));
            }
          }
        }
      }
    }
  }
  _sendWaveformNext = !_sendWaveformNext;
}

void sendHealthCheckState() {
  JsonDocument doc;
  doc["type"] = "healthCheckState";
  const auto& hc = appState.healthCheck;
  doc["pass"] = hc.lastPassCount;
  doc["fail"] = hc.lastFailCount;
  doc["skip"] = hc.lastSkipCount;
  doc["durationMs"] = hc.lastCheckDurationMs;
  doc["timestamp"] = hc.lastCheckTimestamp;
  doc["deferredComplete"] = hc.deferredComplete;
  doc["overall"] = (hc.lastFailCount == 0) ? "pass" : "fail";
  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT((uint8_t*)json.c_str(), json.length());
}
