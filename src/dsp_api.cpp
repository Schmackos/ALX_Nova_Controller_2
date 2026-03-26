#ifdef DSP_ENABLED

#include "dsp_api.h"
#include "http_security.h"
#include "dsp_pipeline.h"
#include "dsp_coefficients.h"
#include "dsp_rew_parser.h"
#include "dsp_crossover.h"
#include "dsp_convolution.h"
#include "thd_measurement.h"
#include "app_state.h"
#include "globals.h"
#include "auth_handler.h"
#include "debug_serial.h"
#include "psram_alloc.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <sys/stat.h>
#include <esp_heap_caps.h>


// Check if a LittleFS file exists without triggering VFS "no permits" error log.
// Arduino's dspFileExists() internally calls open("r") which logs the error.
static bool dspFileExists(const char *path) {
    struct stat st;
    String fullPath = String("/littlefs") + path;
    return (stat(fullPath.c_str(), &st) == 0);
}

// ===== DSP Settings Persistence =====

static unsigned long _lastDspSaveRequest = 0;
static bool _dspSavePending = false;
static const unsigned long DSP_SAVE_DEBOUNCE_MS = 5000;

void loadDspSettings() {
    // Load global settings (skip open if file missing to avoid VFS error log)
    if (dspFileExists("/dsp_global.json")) {
        File f = LittleFS.open("/dsp_global.json", "r");
        if (f && f.size() > 0) {
            String json = f.readString();
            f.close();

JsonDocument doc;
            if (!deserializeJson(doc, json)) {
                DspState *cfg = dsp_get_inactive_config();
                if (doc["globalBypass"].is<bool>()) cfg->globalBypass = doc["globalBypass"].as<bool>();
                if (doc["sampleRate"].is<unsigned int>()) cfg->sampleRate = doc["sampleRate"].as<uint32_t>();
                if (doc["dspEnabled"].is<bool>()) appState.dsp.enabled = doc["dspEnabled"].as<bool>();
                if (doc["presetIndex"].is<int>()) appState.dsp.presetIndex = doc["presetIndex"].as<int8_t>();
                if (doc["presetNames"].is<JsonArray>()) {
                    JsonArray names = doc["presetNames"].as<JsonArray>();
                    for (int i = 0; i < 4 && i < (int)names.size(); i++) {
                        const char *n = names[i] | "";
                        strncpy(appState.dsp.presetNames[i], n, 20);
                        appState.dsp.presetNames[i][20] = '\0';
                    }
                }
            }
        } else if (f) {
            f.close();
        }
    }

    // Load per-channel configs
    for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
        char path[24];
        snprintf(path, sizeof(path), "/dsp_ch%d.json", ch);
        if (dspFileExists(path)) {
            File cf = LittleFS.open(path, "r");
            if (cf && cf.size() > 0) {
                String json = cf.readString();
                cf.close();
                dsp_load_config_from_json(json.c_str(), ch);
            } else if (cf) {
                cf.close();
            }
        }

        // Load FIR binary data if present
        snprintf(path, sizeof(path), "/dsp_fir%d.bin", ch);
        if (!dspFileExists(path)) continue;
        File ff = LittleFS.open(path, "r");
        if (ff && ff.size() > 0) {
            DspState *cfg = dsp_get_inactive_config();
            DspChannelConfig &chCfg = cfg->channels[ch];
            for (int s = 0; s < chCfg.stageCount; s++) {
                if (chCfg.stages[s].type == DSP_FIR && chCfg.stages[s].fir.firSlot >= 0) {
                    int slot = chCfg.stages[s].fir.firSlot;
                    int taps = ff.size() / sizeof(float);
                    if (taps > DSP_MAX_FIR_TAPS) taps = DSP_MAX_FIR_TAPS;
                    // Write to both states since we're loading at boot
                    float *tapsBuf0 = dsp_fir_get_taps(0, slot);
                    float *tapsBuf1 = dsp_fir_get_taps(1, slot);
                    if (tapsBuf0) {
                        ff.read((uint8_t *)tapsBuf0, taps * sizeof(float));
                        if (tapsBuf1) memcpy(tapsBuf1, tapsBuf0, taps * sizeof(float));
                        chCfg.stages[s].fir.numTaps = (uint16_t)taps;
                    }
                    break;
                }
            }
            ff.close();
        } else if (ff) {
            ff.close();
        }
    }

    // Recompute all coefficients and swap to make loaded config active
    DspState *cfg = dsp_get_inactive_config();
    for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
        dsp_recompute_channel_coeffs(cfg->channels[ch], cfg->sampleRate);
    }
    if (!dsp_swap_config()) { dsp_log_swap_failure("DSP API"); }

    LOG_I("[DSP] Settings loaded from LittleFS");
}

void saveDspSettings() {
    // Save global settings
    JsonDocument globalDoc;
    DspState *cfg = dsp_get_active_config();
    globalDoc["globalBypass"] = cfg->globalBypass;
    globalDoc["sampleRate"] = cfg->sampleRate;
    globalDoc["dspEnabled"] = appState.dsp.enabled;
    globalDoc["presetIndex"] = appState.dsp.presetIndex;
    JsonArray names = globalDoc["presetNames"].to<JsonArray>();
    for (int i = 0; i < DSP_PRESET_MAX_SLOTS; i++) names.add(appState.dsp.presetNames[i]);

    String globalJson;
    serializeJson(globalDoc, globalJson);
    File f = LittleFS.open("/dsp_global.json", "w");
    if (f) { f.print(globalJson); f.close(); }

    // Save per-channel configs
    for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
        char path[24];
        snprintf(path, sizeof(path), "/dsp_ch%d.json", ch);

        char *buf = (char *)psram_alloc(4096, 1, "dsp_save");
        if (!buf) continue;
        dsp_export_config_to_json(ch, buf, 4096);
        File cf = LittleFS.open(path, "w");
        if (cf) { cf.print(buf); cf.close(); }
        psram_free(buf, "dsp_save");

        // Save FIR binary if channel has a FIR stage
        DspChannelConfig &chCfg = cfg->channels[ch];
        for (int s = 0; s < chCfg.stageCount; s++) {
            if (chCfg.stages[s].type == DSP_FIR && chCfg.stages[s].fir.numTaps > 0
                && chCfg.stages[s].fir.firSlot >= 0) {
                snprintf(path, sizeof(path), "/dsp_fir%d.bin", ch);
                // Taps are identical in both pool states (written to both on load/import)
                float *tapsBuf = dsp_fir_get_taps(0, chCfg.stages[s].fir.firSlot);
                File ff = LittleFS.open(path, "w");
                if (ff && tapsBuf) {
                    ff.write((const uint8_t *)tapsBuf,
                             chCfg.stages[s].fir.numTaps * sizeof(float));
                    ff.close();
                } else if (ff) {
                    ff.close();
                }
                break;
            }
        }
    }

    _dspSavePending = false;
    LOG_I("[DSP] Settings saved to LittleFS");
}

void saveDspSettingsDebounced() {
    _dspSavePending = true;
    _lastDspSaveRequest = millis();
}

// Called from main loop to check if a debounced save is due
static void checkDspSave() {
    if (_dspSavePending && (millis() - _lastDspSaveRequest >= DSP_SAVE_DEBOUNCE_MS)) {
        saveDspSettings();
    }
}

// ===== DSP Preset Management =====

bool dsp_preset_exists(int slot) {
    if (slot < 0 || slot >= DSP_PRESET_MAX_SLOTS) return false;
    char path[24];
    snprintf(path, sizeof(path), "/dsp_preset_%d.json", slot);
    return dspFileExists(path);
}

bool dsp_preset_save(int slot, const char *name) {
    // Auto-assign slot if -1
    if (slot == -1) {
        bool found = false;
        for (int i = 0; i < DSP_PRESET_MAX_SLOTS; i++) {
            if (!dsp_preset_exists(i) || appState.dsp.presetNames[i][0] == '\0') {
                slot = i;
                found = true;
                break;
            }
        }
        if (!found) {
            LOG_E("[DSP] Preset save: all %d slots full", DSP_PRESET_MAX_SLOTS);
            return false;
        }
    }

    if (slot < 0 || slot >= DSP_PRESET_MAX_SLOTS) return false;

    // Export full config from active state (heap-allocated to avoid stack overflow)
    char *configBuf = (char *)psram_alloc(8192, 1, "dsp_export");
    if (!configBuf) {
        LOG_E("[DSP] Preset save: heap alloc failed");
        return false;
    }
    dsp_export_full_config_json(configBuf, 8192);

    // Parse and augment with name
JsonDocument doc;
    if (deserializeJson(doc, configBuf)) { psram_free(configBuf, "dsp_export"); return false; }

    doc["name"] = name ? name : "";
    doc["dspEnabled"] = appState.dsp.enabled;

    // Write to file
    char path[24];
    snprintf(path, sizeof(path), "/dsp_preset_%d.json", slot);
    String json;
    serializeJson(doc, json);
    File f = LittleFS.open(path, "w");
    if (!f) { psram_free(configBuf, "dsp_export"); return false; }
    f.print(json);
    f.close();
    psram_free(configBuf, "dsp_export");

    // Update AppState
    if (name) {
        strncpy(appState.dsp.presetNames[slot], name, 20);
        appState.dsp.presetNames[slot][20] = '\0';
    }
    appState.dsp.presetIndex = slot;
    appState.markDspConfigDirty();

    // Persist preset index in global settings (debounced to avoid stacking another 4KB alloc)
    saveDspSettingsDebounced();

    LOG_I("[DSP] Preset %d saved: %s", slot, name ? name : "");
    return true;
}

bool dsp_preset_load(int slot) {
    if (slot < 0 || slot >= DSP_PRESET_MAX_SLOTS) return false;

    char path[24];
    snprintf(path, sizeof(path), "/dsp_preset_%d.json", slot);
    if (!dspFileExists(path)) return false;

    File f = LittleFS.open(path, "r");
    if (!f || f.size() == 0) { if (f) f.close(); return false; }
    String json = f.readString();
    f.close();

JsonDocument doc;
    if (deserializeJson(doc, json)) return false;

    // Load full config into inactive buffer
    dsp_copy_active_to_inactive();
    dsp_import_full_config_json(json.c_str());

    // Load dspEnabled
    if (doc["dspEnabled"].is<bool>()) appState.dsp.enabled = doc["dspEnabled"].as<bool>();

    // Recompute all coefficients
    DspState *cfg = dsp_get_inactive_config();
    for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
        extern void dsp_recompute_channel_coeffs(DspChannelConfig &ch, uint32_t sampleRate);
        dsp_recompute_channel_coeffs(cfg->channels[ch], cfg->sampleRate);
    }

    if (!dsp_swap_config()) { dsp_log_swap_failure("DSP API"); }

    // Mark config dirty first (this invalidates preset to -1), then restore
    appState.markDspConfigDirty();

    // Update AppState — set preset index AFTER markDspConfigDirty (which resets to -1)
    const char *name = doc["name"] | "";
    strncpy(appState.dsp.presetNames[slot], name, 20);
    appState.dsp.presetNames[slot][20] = '\0';
    appState.dsp.presetIndex = slot;
    appState.markDspConfigDirty();

    // Save as active config + persist preset index
    saveDspSettings();

    LOG_I("[DSP] Preset %d loaded: %s", slot, name);
    return true;
}

bool dsp_preset_delete(int slot) {
    if (slot < 0 || slot >= DSP_PRESET_MAX_SLOTS) return false;

    char path[24];
    snprintf(path, sizeof(path), "/dsp_preset_%d.json", slot);
    if (dspFileExists(path)) {
        LittleFS.remove(path);
    }

    appState.dsp.presetNames[slot][0] = '\0';
    if (appState.dsp.presetIndex == slot) {
        appState.dsp.presetIndex = -1;
    }
    appState.markDspConfigDirty();

    LOG_I("[DSP] Preset %d deleted", slot);
    return true;
}

bool dsp_preset_rename(int slot, const char *newName) {
    if (slot < 0 || slot >= DSP_PRESET_MAX_SLOTS) return false;
    if (!newName || strlen(newName) == 0) return false;

    char path[24];
    snprintf(path, sizeof(path), "/dsp_preset_%d.json", slot);
    if (!dspFileExists(path)) return false;

    // Read existing preset file
    File f = LittleFS.open(path, "r");
    if (!f || f.size() == 0) { if (f) f.close(); return false; }
    String json = f.readString();
    f.close();

    // Parse and update name field
JsonDocument doc;
    if (deserializeJson(doc, json)) return false;

    doc["name"] = newName;

    // Write back to file
    json.clear();
    serializeJson(doc, json);
    f = LittleFS.open(path, "w");
    if (!f) return false;
    f.print(json);
    f.close();

    // Update AppState
    strncpy(appState.dsp.presetNames[slot], newName, 20);
    appState.dsp.presetNames[slot][20] = '\0';
    appState.markDspConfigDirty();

    // Persist to global settings
    saveDspSettingsDebounced();

    LOG_I("[DSP] Preset %d renamed: %s", slot, newName);
    return true;
}

// ===== Helpers =====

static int parseChannelParam() {
    if (!server.hasArg("ch")) return -1;
    int ch = server.arg("ch").toInt();
    if (ch < 0 || ch >= DSP_MAX_CHANNELS) return -1;
    return ch;
}

static int parseStageParam() {
    if (!server.hasArg("stage")) return -1;
    return server.arg("stage").toInt();
}

static void sendJsonError(int code, const char *msg) {
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"success\":false,\"message\":\"%s\"}", msg);
    server_send(code, "application/json", buf);
}

static DspStageType typeFromString(const char *name) {
    if (!name) return DSP_BIQUAD_PEQ;
    if (strcmp(name, "LPF") == 0) return DSP_BIQUAD_LPF;
    if (strcmp(name, "HPF") == 0) return DSP_BIQUAD_HPF;
    if (strcmp(name, "BPF") == 0) return DSP_BIQUAD_BPF;
    if (strcmp(name, "NOTCH") == 0) return DSP_BIQUAD_NOTCH;
    if (strcmp(name, "PEQ") == 0) return DSP_BIQUAD_PEQ;
    if (strcmp(name, "LOW_SHELF") == 0) return DSP_BIQUAD_LOW_SHELF;
    if (strcmp(name, "HIGH_SHELF") == 0) return DSP_BIQUAD_HIGH_SHELF;
    if (strcmp(name, "ALLPASS") == 0) return DSP_BIQUAD_ALLPASS;
    if (strcmp(name, "ALLPASS_360") == 0) return DSP_BIQUAD_ALLPASS_360;
    if (strcmp(name, "ALLPASS_180") == 0) return DSP_BIQUAD_ALLPASS_180;
    if (strcmp(name, "BPF_0DB") == 0) return DSP_BIQUAD_BPF_0DB;
    if (strcmp(name, "CUSTOM") == 0) return DSP_BIQUAD_CUSTOM;
    if (strcmp(name, "LIMITER") == 0) return DSP_LIMITER;
    if (strcmp(name, "FIR") == 0) return DSP_FIR;
    if (strcmp(name, "GAIN") == 0) return DSP_GAIN;
    if (strcmp(name, "DELAY") == 0) return DSP_DELAY;
    if (strcmp(name, "POLARITY") == 0) return DSP_POLARITY;
    if (strcmp(name, "MUTE") == 0) return DSP_MUTE;
    if (strcmp(name, "COMPRESSOR") == 0) return DSP_COMPRESSOR;
    if (strcmp(name, "LPF_1ST") == 0) return DSP_BIQUAD_LPF_1ST;
    if (strcmp(name, "HPF_1ST") == 0) return DSP_BIQUAD_HPF_1ST;
    if (strcmp(name, "LINKWITZ") == 0) return DSP_BIQUAD_LINKWITZ;
    if (strcmp(name, "NOISE_GATE") == 0) return DSP_NOISE_GATE;
    if (strcmp(name, "TONE_CTRL") == 0) return DSP_TONE_CTRL;
    if (strcmp(name, "STEREO_WIDTH") == 0) return DSP_STEREO_WIDTH;
    if (strcmp(name, "LOUDNESS") == 0) return DSP_LOUDNESS;
    if (strcmp(name, "BASS_ENHANCE") == 0) return DSP_BASS_ENHANCE;
    if (strcmp(name, "MULTIBAND_COMP") == 0) return DSP_MULTIBAND_COMP;
    return DSP_BIQUAD_PEQ;
}

// ===== Stereo Link Helper =====

static void autoMirrorIfLinked(int ch) {
    int partner = dsp_get_linked_partner(ch);
    if (partner >= 0) {
        dsp_mirror_channel_config(ch, partner);
    }
}

// ===== API Endpoint Registration =====

void registerDspApiEndpoints() {
    // GET /api/dsp — full config
    server_on_versioned("/api/dsp", HTTP_GET, []() {
        if (!requireAuth()) return;
        const int bufSize = 8192;
        char *buf = (char *)psram_alloc(bufSize, 1, "dsp_api_get");
        if (!buf) { server_send(503, "application/json", "{\"error\":\"Out of memory\"}"); return; }

        dsp_export_full_config_json(buf, bufSize);

        // Wrap with dspEnabled
JsonDocument doc;
        deserializeJson(doc, buf);
        doc["dspEnabled"] = appState.dsp.enabled;
        psram_free(buf, "dsp_api_get");

        String json;
        serializeJson(doc, json);
        server_send(200, "application/json", json);
    });

    // PUT /api/dsp — replace full config
    server_on_versioned("/api/dsp", HTTP_PUT, []() {
        if (!requireAuth()) return;
        if (!server.hasArg("plain")) { sendJsonError(400, "No data"); return; }

        dsp_import_full_config_json(server.arg("plain").c_str());

JsonDocument doc;
        if (!deserializeJson(doc, server.arg("plain"))) {
            if (doc["dspEnabled"].is<bool>()) appState.dsp.enabled = doc["dspEnabled"].as<bool>();
        }

        if (!dsp_swap_config()) { dsp_log_swap_failure("DSP API"); sendJsonError(503, "DSP busy, retry"); return; }
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();
        server_send(200, "application/json", "{\"success\":true}");
        LOG_I("[DSP] Full config replaced via API");
    });

    // POST /api/dsp/bypass — toggle global bypass
    server_on_versioned("/api/dsp/bypass", HTTP_POST, []() {
        if (!requireAuth()) return;
        dsp_copy_active_to_inactive();
        DspState *cfg = dsp_get_inactive_config();
        if (server.hasArg("plain")) {
JsonDocument doc;
            if (!deserializeJson(doc, server.arg("plain"))) {
                if (doc["bypass"].is<bool>()) cfg->globalBypass = doc["bypass"].as<bool>();
                if (doc["enabled"].is<bool>()) appState.dsp.enabled = doc["enabled"].as<bool>();
            }
        } else {
            cfg->globalBypass = !cfg->globalBypass;
        }
        if (!dsp_swap_config()) { dsp_log_swap_failure("DSP API"); sendJsonError(503, "DSP busy, retry"); return; }
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();
        server_send(200, "application/json", "{\"success\":true}");
    });

    // GET /api/dsp/metrics
    server_on_versioned("/api/dsp/metrics", HTTP_GET, []() {
        if (!requireAuth()) return;
        DspMetrics m = dsp_get_metrics();
JsonDocument doc;
        doc["processTimeUs"] = m.processTimeUs;
        doc["maxProcessTimeUs"] = m.maxProcessTimeUs;
        doc["cpuLoad"] = m.cpuLoadPercent;
        JsonArray gr = doc["limiterGr"].to<JsonArray>();
        for (int i = 0; i < DSP_MAX_CHANNELS; i++) gr.add(m.limiterGrDb[i]);

        String json;
        serializeJson(doc, json);
        server_send(200, "application/json", json);
    });

    // GET /api/dsp/channel?ch=N — get channel config
    server_on_versioned("/api/dsp/channel", HTTP_GET, []() {
        if (!requireAuth()) return;
        int ch = parseChannelParam();
        if (ch < 0) { sendJsonError(400, "Invalid channel"); return; }

        const int bufSize = 4096;
        char *buf = (char *)psram_alloc(bufSize, 1, "dsp_api_ch");
        if (!buf) { server_send(503, "application/json", "{\"error\":\"Out of memory\"}"); return; }

        dsp_export_config_to_json(ch, buf, bufSize);
        server_send(200, "application/json", buf);
        psram_free(buf, "dsp_api_ch");
    });

    // POST /api/dsp/channel/bypass?ch=N — toggle channel bypass
    server_on_versioned("/api/dsp/channel/bypass", HTTP_POST, []() {
        if (!requireAuth()) return;
        int ch = parseChannelParam();
        if (ch < 0) { sendJsonError(400, "Invalid channel"); return; }

        dsp_copy_active_to_inactive();
        DspState *cfg = dsp_get_inactive_config();
        if (server.hasArg("plain")) {
JsonDocument doc;
            if (!deserializeJson(doc, server.arg("plain"))) {
                if (doc["bypass"].is<bool>()) cfg->channels[ch].bypass = doc["bypass"].as<bool>();
            }
        } else {
            cfg->channels[ch].bypass = !cfg->channels[ch].bypass;
        }
        if (!dsp_swap_config()) { dsp_log_swap_failure("DSP API"); sendJsonError(503, "DSP busy, retry"); return; }
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();
        server_send(200, "application/json", "{\"success\":true}");
    });

    // POST /api/dsp/stage?ch=N — add stage
    server_on_versioned("/api/dsp/stage", HTTP_POST, []() {
        if (!requireAuth()) return;
        int ch = parseChannelParam();
        if (ch < 0) { sendJsonError(400, "Invalid channel"); return; }
        if (!server.hasArg("plain")) { sendJsonError(400, "No data"); return; }

JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain"))) { sendJsonError(400, "Invalid JSON"); return; }

        DspStageType type = typeFromString(doc["type"].as<const char *>());
        int pos = doc["position"] | -1;

        // Copy active config to inactive, then modify
        DspState *inactive = dsp_get_inactive_config();
        dsp_copy_active_to_inactive();

        int idx = dsp_add_stage(ch, type, pos);
        if (idx < 0) { sendJsonError(400, "Max stages reached"); return; }

        // Apply params if provided
        DspStage &s = inactive->channels[ch].stages[idx];
        if (doc["enabled"].is<bool>()) s.enabled = doc["enabled"].as<bool>();
        if (doc["label"].is<const char *>()) {
            strncpy(s.label, doc["label"].as<const char *>(), sizeof(s.label) - 1);
            s.label[sizeof(s.label) - 1] = '\0';
        }

        JsonObject params = doc["params"];
        if (dsp_is_biquad_type(type) && !params.isNull()) {
            if (params["frequency"].is<float>()) s.biquad.frequency = params["frequency"].as<float>();
            if (params["gain"].is<float>()) s.biquad.gain = params["gain"].as<float>();
            if (params["Q"].is<float>()) s.biquad.Q = params["Q"].as<float>();
            if (params["Q2"].is<float>()) s.biquad.Q2 = params["Q2"].as<float>();
            dsp_compute_biquad_coeffs(s.biquad, type, inactive->sampleRate);
        } else if (type == DSP_LIMITER && !params.isNull()) {
            if (params["thresholdDb"].is<float>()) s.limiter.thresholdDb = params["thresholdDb"].as<float>();
            if (params["attackMs"].is<float>()) s.limiter.attackMs = params["attackMs"].as<float>();
            if (params["releaseMs"].is<float>()) s.limiter.releaseMs = params["releaseMs"].as<float>();
            if (params["ratio"].is<float>()) s.limiter.ratio = params["ratio"].as<float>();
        } else if (type == DSP_GAIN && !params.isNull()) {
            if (params["gainDb"].is<float>()) s.gain.gainDb = params["gainDb"].as<float>();
            dsp_compute_gain_linear(s.gain);
        } else if (type == DSP_DELAY && !params.isNull()) {
            if (params["delaySamples"].is<int>()) {
                uint16_t ds = params["delaySamples"].as<uint16_t>();
                s.delay.delaySamples = ds > DSP_MAX_DELAY_SAMPLES ? DSP_MAX_DELAY_SAMPLES : ds;
            }
        } else if (type == DSP_POLARITY && !params.isNull()) {
            if (params["inverted"].is<bool>()) s.polarity.inverted = params["inverted"].as<bool>();
        } else if (type == DSP_MUTE && !params.isNull()) {
            if (params["muted"].is<bool>()) s.mute.muted = params["muted"].as<bool>();
        } else if (type == DSP_COMPRESSOR && !params.isNull()) {
            if (params["thresholdDb"].is<float>()) s.compressor.thresholdDb = params["thresholdDb"].as<float>();
            if (params["attackMs"].is<float>()) s.compressor.attackMs = params["attackMs"].as<float>();
            if (params["releaseMs"].is<float>()) s.compressor.releaseMs = params["releaseMs"].as<float>();
            if (params["ratio"].is<float>()) s.compressor.ratio = params["ratio"].as<float>();
            if (params["kneeDb"].is<float>()) s.compressor.kneeDb = params["kneeDb"].as<float>();
            if (params["makeupGainDb"].is<float>()) s.compressor.makeupGainDb = params["makeupGainDb"].as<float>();
            dsp_compute_compressor_makeup(s.compressor);
        } else if (type == DSP_NOISE_GATE && !params.isNull()) {
            if (params["thresholdDb"].is<float>()) s.noiseGate.thresholdDb = params["thresholdDb"].as<float>();
            if (params["attackMs"].is<float>()) s.noiseGate.attackMs = params["attackMs"].as<float>();
            if (params["holdMs"].is<float>()) s.noiseGate.holdMs = params["holdMs"].as<float>();
            if (params["releaseMs"].is<float>()) s.noiseGate.releaseMs = params["releaseMs"].as<float>();
            if (params["ratio"].is<float>()) s.noiseGate.ratio = params["ratio"].as<float>();
            if (params["rangeDb"].is<float>()) s.noiseGate.rangeDb = params["rangeDb"].as<float>();
        } else if (type == DSP_TONE_CTRL && !params.isNull()) {
            if (params["bassGain"].is<float>()) s.toneCtrl.bassGain = params["bassGain"].as<float>();
            if (params["midGain"].is<float>()) s.toneCtrl.midGain = params["midGain"].as<float>();
            if (params["trebleGain"].is<float>()) s.toneCtrl.trebleGain = params["trebleGain"].as<float>();
            dsp_compute_tone_ctrl_coeffs(s.toneCtrl, inactive->sampleRate);
        } else if (type == DSP_STEREO_WIDTH && !params.isNull()) {
            if (params["width"].is<float>()) s.stereoWidth.width = params["width"].as<float>();
            if (params["centerGainDb"].is<float>()) s.stereoWidth.centerGainDb = params["centerGainDb"].as<float>();
            dsp_compute_stereo_width(s.stereoWidth);
        } else if (type == DSP_LOUDNESS && !params.isNull()) {
            if (params["referenceLevelDb"].is<float>()) s.loudness.referenceLevelDb = params["referenceLevelDb"].as<float>();
            if (params["currentLevelDb"].is<float>()) s.loudness.currentLevelDb = params["currentLevelDb"].as<float>();
            if (params["amount"].is<float>()) s.loudness.amount = params["amount"].as<float>();
            dsp_compute_loudness_coeffs(s.loudness, inactive->sampleRate);
        } else if (type == DSP_BASS_ENHANCE && !params.isNull()) {
            if (params["frequency"].is<float>()) s.bassEnhance.frequency = params["frequency"].as<float>();
            if (params["harmonicGainDb"].is<float>()) s.bassEnhance.harmonicGainDb = params["harmonicGainDb"].as<float>();
            if (params["mix"].is<float>()) s.bassEnhance.mix = params["mix"].as<float>();
            if (params["order"].is<int>()) s.bassEnhance.order = params["order"].as<uint8_t>();
            dsp_compute_bass_enhance_coeffs(s.bassEnhance, inactive->sampleRate);
        } else if (type == DSP_MULTIBAND_COMP && !params.isNull()) {
            if (params["numBands"].is<int>()) s.multibandComp.numBands = params["numBands"].as<uint8_t>();
        }

        autoMirrorIfLinked(ch);
        if (!dsp_swap_config()) { dsp_log_swap_failure("DSP API"); sendJsonError(503, "DSP busy, retry"); return; }
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();

        char resp[64];
        snprintf(resp, sizeof(resp), "{\"success\":true,\"index\":%d}", idx);
        server_send(200, "application/json", resp);
        LOG_I("[DSP] Stage added: ch=%d type=%d pos=%d", ch, type, idx);
    });

    // PUT /api/dsp/stage?ch=N&stage=M — update stage params
    server_on_versioned("/api/dsp/stage", HTTP_PUT, []() {
        if (!requireAuth()) return;
        int ch = parseChannelParam();
        int si = parseStageParam();
        if (ch < 0) { sendJsonError(400, "Invalid channel"); return; }
        if (!server.hasArg("plain")) { sendJsonError(400, "No data"); return; }

JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain"))) { sendJsonError(400, "Invalid JSON"); return; }

        DspState *inactive = dsp_get_inactive_config();
        dsp_copy_active_to_inactive();

        if (si < 0 || si >= inactive->channels[ch].stageCount) {
            sendJsonError(400, "Invalid stage index");
            return;
        }

        DspStage &s = inactive->channels[ch].stages[si];
        if (doc["enabled"].is<bool>()) s.enabled = doc["enabled"].as<bool>();
        if (doc["label"].is<const char *>()) {
            strncpy(s.label, doc["label"].as<const char *>(), sizeof(s.label) - 1);
            s.label[sizeof(s.label) - 1] = '\0';
        }

        JsonObject params = doc["params"];
        if (dsp_is_biquad_type(s.type) && !params.isNull()) {
            if (params["frequency"].is<float>()) s.biquad.frequency = params["frequency"].as<float>();
            if (params["gain"].is<float>()) s.biquad.gain = params["gain"].as<float>();
            if (params["Q"].is<float>()) s.biquad.Q = params["Q"].as<float>();
            if (params["Q2"].is<float>()) s.biquad.Q2 = params["Q2"].as<float>();
            if (s.type == DSP_BIQUAD_CUSTOM && params["coeffs"].is<JsonArray>()) {
                JsonArray c = params["coeffs"].as<JsonArray>();
                for (int j = 0; j < 5 && j < (int)c.size(); j++)
                    s.biquad.coeffs[j] = c[j].as<float>();
            } else {
                dsp_compute_biquad_coeffs(s.biquad, s.type, inactive->sampleRate);
            }
        } else if (s.type == DSP_LIMITER && !params.isNull()) {
            if (params["thresholdDb"].is<float>()) s.limiter.thresholdDb = params["thresholdDb"].as<float>();
            if (params["attackMs"].is<float>()) s.limiter.attackMs = params["attackMs"].as<float>();
            if (params["releaseMs"].is<float>()) s.limiter.releaseMs = params["releaseMs"].as<float>();
            if (params["ratio"].is<float>()) s.limiter.ratio = params["ratio"].as<float>();
        } else if (s.type == DSP_GAIN && !params.isNull()) {
            if (params["gainDb"].is<float>()) s.gain.gainDb = params["gainDb"].as<float>();
            dsp_compute_gain_linear(s.gain);
        } else if (s.type == DSP_DELAY && !params.isNull()) {
            if (params["delaySamples"].is<int>()) {
                uint16_t ds = params["delaySamples"].as<uint16_t>();
                s.delay.delaySamples = ds > DSP_MAX_DELAY_SAMPLES ? DSP_MAX_DELAY_SAMPLES : ds;
            }
        } else if (s.type == DSP_POLARITY && !params.isNull()) {
            if (params["inverted"].is<bool>()) s.polarity.inverted = params["inverted"].as<bool>();
        } else if (s.type == DSP_MUTE && !params.isNull()) {
            if (params["muted"].is<bool>()) s.mute.muted = params["muted"].as<bool>();
        } else if (s.type == DSP_COMPRESSOR && !params.isNull()) {
            if (params["thresholdDb"].is<float>()) s.compressor.thresholdDb = params["thresholdDb"].as<float>();
            if (params["attackMs"].is<float>()) s.compressor.attackMs = params["attackMs"].as<float>();
            if (params["releaseMs"].is<float>()) s.compressor.releaseMs = params["releaseMs"].as<float>();
            if (params["ratio"].is<float>()) s.compressor.ratio = params["ratio"].as<float>();
            if (params["kneeDb"].is<float>()) s.compressor.kneeDb = params["kneeDb"].as<float>();
            if (params["makeupGainDb"].is<float>()) s.compressor.makeupGainDb = params["makeupGainDb"].as<float>();
            dsp_compute_compressor_makeup(s.compressor);
        } else if (s.type == DSP_NOISE_GATE && !params.isNull()) {
            if (params["thresholdDb"].is<float>()) s.noiseGate.thresholdDb = params["thresholdDb"].as<float>();
            if (params["attackMs"].is<float>()) s.noiseGate.attackMs = params["attackMs"].as<float>();
            if (params["holdMs"].is<float>()) s.noiseGate.holdMs = params["holdMs"].as<float>();
            if (params["releaseMs"].is<float>()) s.noiseGate.releaseMs = params["releaseMs"].as<float>();
            if (params["ratio"].is<float>()) s.noiseGate.ratio = params["ratio"].as<float>();
            if (params["rangeDb"].is<float>()) s.noiseGate.rangeDb = params["rangeDb"].as<float>();
        } else if (s.type == DSP_TONE_CTRL && !params.isNull()) {
            if (params["bassGain"].is<float>()) s.toneCtrl.bassGain = params["bassGain"].as<float>();
            if (params["midGain"].is<float>()) s.toneCtrl.midGain = params["midGain"].as<float>();
            if (params["trebleGain"].is<float>()) s.toneCtrl.trebleGain = params["trebleGain"].as<float>();
            dsp_compute_tone_ctrl_coeffs(s.toneCtrl, inactive->sampleRate);
        } else if (s.type == DSP_STEREO_WIDTH && !params.isNull()) {
            if (params["width"].is<float>()) s.stereoWidth.width = params["width"].as<float>();
            if (params["centerGainDb"].is<float>()) s.stereoWidth.centerGainDb = params["centerGainDb"].as<float>();
            dsp_compute_stereo_width(s.stereoWidth);
        } else if (s.type == DSP_LOUDNESS && !params.isNull()) {
            if (params["referenceLevelDb"].is<float>()) s.loudness.referenceLevelDb = params["referenceLevelDb"].as<float>();
            if (params["currentLevelDb"].is<float>()) s.loudness.currentLevelDb = params["currentLevelDb"].as<float>();
            if (params["amount"].is<float>()) s.loudness.amount = params["amount"].as<float>();
            dsp_compute_loudness_coeffs(s.loudness, inactive->sampleRate);
        } else if (s.type == DSP_BASS_ENHANCE && !params.isNull()) {
            if (params["frequency"].is<float>()) s.bassEnhance.frequency = params["frequency"].as<float>();
            if (params["harmonicGainDb"].is<float>()) s.bassEnhance.harmonicGainDb = params["harmonicGainDb"].as<float>();
            if (params["mix"].is<float>()) s.bassEnhance.mix = params["mix"].as<float>();
            if (params["order"].is<int>()) s.bassEnhance.order = params["order"].as<uint8_t>();
            dsp_compute_bass_enhance_coeffs(s.bassEnhance, inactive->sampleRate);
        } else if (s.type == DSP_MULTIBAND_COMP && !params.isNull()) {
            if (params["numBands"].is<int>()) s.multibandComp.numBands = params["numBands"].as<uint8_t>();
        }

        autoMirrorIfLinked(ch);
        if (!dsp_swap_config()) { dsp_log_swap_failure("DSP API"); sendJsonError(503, "DSP busy, retry"); return; }
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();
        server_send(200, "application/json", "{\"success\":true}");
    });

    // DELETE /api/dsp/stage?ch=N&stage=M — remove stage
    server_on_versioned("/api/dsp/stage", HTTP_DELETE, []() {
        if (!requireAuth()) return;
        int ch = parseChannelParam();
        int si = parseStageParam();
        if (ch < 0) { sendJsonError(400, "Invalid channel"); return; }

        DspState *inactive = dsp_get_inactive_config();
        dsp_copy_active_to_inactive();

        if (!dsp_remove_stage(ch, si)) {
            sendJsonError(400, "Invalid stage index");
            return;
        }

        autoMirrorIfLinked(ch);
        if (!dsp_swap_config()) { dsp_log_swap_failure("DSP API"); sendJsonError(503, "DSP busy, retry"); return; }
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();
        server_send(200, "application/json", "{\"success\":true}");
        LOG_I("[DSP] Stage removed: ch=%d stage=%d", ch, si);
    });

    // POST /api/dsp/stage/reorder?ch=N — reorder stages
    server_on_versioned("/api/dsp/stage/reorder", HTTP_POST, []() {
        if (!requireAuth()) return;
        int ch = parseChannelParam();
        if (ch < 0) { sendJsonError(400, "Invalid channel"); return; }
        if (!server.hasArg("plain")) { sendJsonError(400, "No data"); return; }

JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain"))) { sendJsonError(400, "Invalid JSON"); return; }

        if (!doc["order"].is<JsonArray>()) { sendJsonError(400, "Missing order array"); return; }
        JsonArray order = doc["order"].as<JsonArray>();

        DspState *inactive = dsp_get_inactive_config();
        dsp_copy_active_to_inactive();

        int newOrder[DSP_MAX_STAGES];
        int count = 0;
        for (JsonVariant v : order) {
            if (count >= DSP_MAX_STAGES) break;
            newOrder[count++] = v.as<int>();
        }

        if (!dsp_reorder_stages(ch, newOrder, count)) {
            sendJsonError(400, "Invalid order");
            return;
        }

        if (!dsp_swap_config()) { dsp_log_swap_failure("DSP API"); sendJsonError(503, "DSP busy, retry"); return; }
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();
        server_send(200, "application/json", "{\"success\":true}");
    });

    // POST /api/dsp/stage/enable?ch=N&stage=M — toggle stage enable
    server_on_versioned("/api/dsp/stage/enable", HTTP_POST, []() {
        if (!requireAuth()) return;
        int ch = parseChannelParam();
        int si = parseStageParam();
        if (ch < 0) { sendJsonError(400, "Invalid channel"); return; }

        DspState *inactive = dsp_get_inactive_config();
        dsp_copy_active_to_inactive();

        bool newState = true;
        if (server.hasArg("plain")) {
JsonDocument doc;
            if (!deserializeJson(doc, server.arg("plain"))) {
                if (doc["enabled"].is<bool>()) newState = doc["enabled"].as<bool>();
            }
        } else {
            // Toggle
            if (si >= 0 && si < inactive->channels[ch].stageCount) {
                newState = !inactive->channels[ch].stages[si].enabled;
            }
        }

        if (!dsp_set_stage_enabled(ch, si, newState)) {
            sendJsonError(400, "Invalid stage index");
            return;
        }

        if (!dsp_swap_config()) { dsp_log_swap_failure("DSP API"); sendJsonError(503, "DSP busy, retry"); return; }
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();
        server_send(200, "application/json", "{\"success\":true}");
    });

    // ===== Import/Export Endpoints =====

    // POST /api/dsp/import/apo?ch=N
    server_on_versioned("/api/dsp/import/apo", HTTP_POST, []() {
        if (!requireAuth()) return;
        int ch = parseChannelParam();
        if (ch < 0) { sendJsonError(400, "Invalid channel"); return; }
        if (!server.hasArg("plain")) { sendJsonError(400, "No data"); return; }

        DspState *inactive = dsp_get_inactive_config();
        dsp_copy_active_to_inactive();

        int added = dsp_parse_apo_filters(server.arg("plain").c_str(),
                                           inactive->channels[ch],
                                           inactive->sampleRate);

        if (added < 0) { sendJsonError(400, "Parse error"); return; }

        if (!dsp_swap_config()) { dsp_log_swap_failure("DSP API"); sendJsonError(503, "DSP busy, retry"); return; }
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();

        char resp[64];
        snprintf(resp, sizeof(resp), "{\"success\":true,\"added\":%d}", added);
        server_send(200, "application/json", resp);
        LOG_I("[DSP] APO import: %d filters added to ch=%d", added, ch);
    });

    // POST /api/dsp/import/minidsp?ch=N
    server_on_versioned("/api/dsp/import/minidsp", HTTP_POST, []() {
        if (!requireAuth()) return;
        int ch = parseChannelParam();
        if (ch < 0) { sendJsonError(400, "Invalid channel"); return; }
        if (!server.hasArg("plain")) { sendJsonError(400, "No data"); return; }

        DspState *inactive = dsp_get_inactive_config();
        dsp_copy_active_to_inactive();

        int added = dsp_parse_minidsp_biquads(server.arg("plain").c_str(),
                                                inactive->channels[ch]);

        if (added < 0) { sendJsonError(400, "Parse error"); return; }

        if (!dsp_swap_config()) { dsp_log_swap_failure("DSP API"); sendJsonError(503, "DSP busy, retry"); return; }
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();

        char resp[64];
        snprintf(resp, sizeof(resp), "{\"success\":true,\"added\":%d}", added);
        server_send(200, "application/json", resp);
    });

    // POST /api/dsp/import/fir?ch=N — import FIR text coefficients
    server_on_versioned("/api/dsp/import/fir", HTTP_POST, []() {
        if (!requireAuth()) return;
        int ch = parseChannelParam();
        if (ch < 0) { sendJsonError(400, "Invalid channel"); return; }
        if (!server.hasArg("plain")) { sendJsonError(400, "No data"); return; }

        dsp_copy_active_to_inactive();
        DspState *inactive = dsp_get_inactive_config();

        DspChannelConfig &chCfg = inactive->channels[ch];
        if (chCfg.stageCount >= DSP_MAX_STAGES) { sendJsonError(400, "Max stages reached"); return; }

        // Allocate FIR slot
        int slot = dsp_fir_alloc_slot();
        if (slot < 0) { sendJsonError(400, "No FIR slots available"); return; }

        // Parse taps into pool (write to both states since slot is newly allocated)
        float *tapsBuf0 = dsp_fir_get_taps(0, slot);
        if (!tapsBuf0) { dsp_fir_free_slot(slot); sendJsonError(500, "FIR pool error"); return; }

        int taps = dsp_parse_fir_text(server.arg("plain").c_str(), tapsBuf0, DSP_MAX_FIR_TAPS);
        if (taps <= 0) { dsp_fir_free_slot(slot); sendJsonError(400, "No valid FIR taps"); return; }

        // Copy taps to other state's pool
        float *tapsBuf1 = dsp_fir_get_taps(1, slot);
        if (tapsBuf1) memcpy(tapsBuf1, tapsBuf0, taps * sizeof(float));

        // Add FIR stage
        DspStage &s = chCfg.stages[chCfg.stageCount];
        dsp_init_stage(s, DSP_FIR);
        s.fir.firSlot = (int8_t)slot;
        s.fir.numTaps = (uint16_t)taps;
        chCfg.stageCount++;

        if (!dsp_swap_config()) { dsp_log_swap_failure("DSP API"); sendJsonError(503, "DSP busy, retry"); return; }
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();

        char resp[64];
        snprintf(resp, sizeof(resp), "{\"success\":true,\"taps\":%d}", taps);
        server_send(200, "application/json", resp);
        LOG_I("[DSP] FIR import: %d taps to ch=%d", taps, ch);
    });

    // GET /api/dsp/export/apo?ch=N
    server_on_versioned("/api/dsp/export/apo", HTTP_GET, []() {
        if (!requireAuth()) return;
        int ch = parseChannelParam();
        if (ch < 0) { sendJsonError(400, "Invalid channel"); return; }

        DspState *cfg = dsp_get_active_config();
        char buf[2048];
        dsp_export_apo(cfg->channels[ch], cfg->sampleRate, buf, sizeof(buf));
        server_send(200, "text/plain", buf);
    });

    // GET /api/dsp/export/minidsp?ch=N
    server_on_versioned("/api/dsp/export/minidsp", HTTP_GET, []() {
        if (!requireAuth()) return;
        int ch = parseChannelParam();
        if (ch < 0) { sendJsonError(400, "Invalid channel"); return; }

        DspState *cfg = dsp_get_active_config();
        char buf[2048];
        dsp_export_minidsp(cfg->channels[ch], buf, sizeof(buf));
        server_send(200, "text/plain", buf);
    });

    // GET /api/dsp/export/json
    server_on_versioned("/api/dsp/export/json", HTTP_GET, []() {
        if (!requireAuth()) return;
        const int bufSize = 8192;
        char *buf = (char *)psram_alloc(bufSize, 1, "dsp_api_export");
        if (!buf) { server_send(503, "application/json", "{\"error\":\"Out of memory\"}"); return; }

        dsp_export_full_config_json(buf, bufSize);
        server_send(200, "application/json", buf);
        psram_free(buf, "dsp_api_export");
    });

    // ===== Crossover & Bass Management Endpoints =====

    // POST /api/dsp/crossover?ch=N — apply crossover filter
    server_on_versioned("/api/dsp/crossover", HTTP_POST, []() {
        if (!requireAuth()) return;
        int ch = parseChannelParam();
        if (ch < 0) { sendJsonError(400, "Invalid channel"); return; }
        if (!server.hasArg("plain")) { sendJsonError(400, "No data"); return; }

JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain"))) { sendJsonError(400, "Invalid JSON"); return; }

        float freq = doc["freq"] | 1000.0f;
        int role = doc["role"] | 0; // 0 = LPF, 1 = HPF
        const char *typeStr = doc["type"] | "lr4";

        dsp_copy_active_to_inactive();
        dsp_clear_crossover_stages(ch);

        int result = -1;
        if (strncmp(typeStr, "lr", 2) == 0) {
            int order = atoi(typeStr + 2);
            result = dsp_insert_crossover_lr(ch, freq, order, role);
        } else if (strncmp(typeStr, "bw", 2) == 0) {
            int order = atoi(typeStr + 2);
            result = dsp_insert_crossover_butterworth(ch, freq, order, role);
        } else if (strncmp(typeStr, "bessel", 6) == 0) {
            int order = atoi(typeStr + 6);
            if (order == 0) order = 4;
            result = dsp_insert_crossover_bessel(ch, freq, order, role);
        } else {
            sendJsonError(400, "Unknown crossover type");
            return;
        }

        if (result < 0) { sendJsonError(400, "Failed to insert crossover"); return; }

        if (!dsp_swap_config()) { dsp_log_swap_failure("DSP API"); sendJsonError(503, "DSP busy, retry"); return; }
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();

        char resp[64];
        snprintf(resp, sizeof(resp), "{\"success\":true,\"firstStage\":%d}", result);
        server_send(200, "application/json", resp);
        LOG_I("[DSP] Crossover applied: ch=%d type=%s freq=%.0f role=%d", ch, typeStr, freq, role);
    });

    // ===== Baffle Step & THD Endpoints =====

    // POST /api/dsp/bafflestep?ch=N — apply baffle step correction
    server_on_versioned("/api/dsp/bafflestep", HTTP_POST, []() {
        if (!requireAuth()) return;
        int ch = parseChannelParam();
        if (ch < 0) { sendJsonError(400, "Invalid channel"); return; }
        if (!server.hasArg("plain")) { sendJsonError(400, "No data"); return; }

JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain"))) { sendJsonError(400, "Invalid JSON"); return; }

        float widthMm = doc["baffleWidthMm"] | 250.0f;
        BaffleStepResult bsr = dsp_baffle_step_correction(widthMm);

        dsp_copy_active_to_inactive();
        DspState *inactive = dsp_get_inactive_config();
        int idx = dsp_add_stage(ch, DSP_BIQUAD_HIGH_SHELF);
        if (idx < 0) { sendJsonError(400, "No room for stage"); return; }

        DspStage &s = inactive->channels[ch].stages[idx];
        s.biquad.frequency = bsr.frequency;
        s.biquad.gain = bsr.gainDb;
        s.biquad.Q = 0.707f;
        dsp_compute_biquad_coeffs(s.biquad, DSP_BIQUAD_HIGH_SHELF, inactive->sampleRate);

        autoMirrorIfLinked(ch);
        if (!dsp_swap_config()) { dsp_log_swap_failure("DSP API"); sendJsonError(503, "DSP busy, retry"); return; }
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();

        char resp[128];
        snprintf(resp, sizeof(resp), "{\"success\":true,\"frequency\":%.1f,\"gainDb\":%.1f,\"index\":%d}", bsr.frequency, bsr.gainDb, idx);
        server_send(200, "application/json", resp);
        LOG_I("[DSP] Baffle step: ch=%d width=%.0fmm freq=%.0fHz", ch, widthMm, bsr.frequency);
    });

    // GET /api/thd — get THD measurement result
    server_on_versioned("/api/thd", HTTP_GET, []() {
        if (!requireAuth()) return;
        ThdResult r = thd_get_result();
JsonDocument doc;
        doc["measuring"] = thd_is_measuring();
        doc["testFreq"] = thd_get_test_freq();
        doc["valid"] = r.valid;
        doc["thdPlusNPercent"] = r.thdPlusNPercent;
        doc["thdPlusNDb"] = r.thdPlusNDb;
        doc["fundamentalDbfs"] = r.fundamentalDbfs;
        doc["framesProcessed"] = r.framesProcessed;
        doc["framesTarget"] = r.framesTarget;
        JsonArray harmonics = doc["harmonicLevels"].to<JsonArray>();
        for (int h = 0; h < THD_MAX_HARMONICS; h++) harmonics.add(r.harmonicLevels[h]);
        String json;
        serializeJson(doc, json);
        server_send(200, "application/json", json);
    });

    // POST /api/thd — start THD measurement
    server_on_versioned("/api/thd", HTTP_POST, []() {
        if (!requireAuth()) return;
        if (!server.hasArg("plain")) { sendJsonError(400, "No data"); return; }
JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain"))) { sendJsonError(400, "Invalid JSON"); return; }
        float freq = doc["freq"] | 1000.0f;
        int avg = doc["averages"] | 8;
        thd_start_measurement(freq, (uint16_t)avg);
        server_send(200, "application/json", "{\"success\":true}");
        LOG_I("[DSP] THD measurement started: %.0f Hz, %d avg", freq, avg);
    });

    // DELETE /api/thd — stop THD measurement
    server_on_versioned("/api/thd", HTTP_DELETE, []() {
        if (!requireAuth()) return;
        thd_stop_measurement();
        server_send(200, "application/json", "{\"success\":true}");
    });

    // ===== PEQ Preset Endpoints =====

    // GET /api/dsp/peq/presets — list preset names
    server_on_versioned("/api/dsp/peq/presets", HTTP_GET, []() {
        if (!requireAuth()) return;
JsonDocument doc;
        JsonArray names = doc["presets"].to<JsonArray>();

        File root = LittleFS.open("/");
        if (root && root.isDirectory()) {
            File f = root.openNextFile();
            while (f) {
                String name = f.name();
                // LittleFS may return name with or without leading /
                if (name.startsWith("/")) name = name.substring(1);
                if (name.startsWith("peq_") && name.endsWith(".json")) {
                    // Extract preset name: peq_MyPreset.json → MyPreset
                    String presetName = name.substring(4, name.length() - 5);
                    names.add(presetName);
                }
                f = root.openNextFile();
            }
        }

        String json;
        serializeJson(doc, json);
        server_send(200, "application/json", json);
    });

    // POST /api/dsp/peq/presets — save preset
    server_on_versioned("/api/dsp/peq/presets", HTTP_POST, []() {
        if (!requireAuth()) return;
        if (!server.hasArg("plain")) { sendJsonError(400, "No data"); return; }

JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain"))) { sendJsonError(400, "Invalid JSON"); return; }

        const char *name = doc["name"] | (const char *)nullptr;
        if (!name || strlen(name) == 0 || strlen(name) > 20) {
            sendJsonError(400, "Name required (max 20 chars)");
            return;
        }

        // Sanitize name for filesystem
        char safeName[24];
        int j = 0;
        for (int i = 0; name[i] && j < 20; i++) {
            char c = name[i];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '_' || c == '-') {
                safeName[j++] = c;
            }
        }
        safeName[j] = '\0';
        if (j == 0) { sendJsonError(400, "Invalid name"); return; }

        // Check preset limit (max 10)
        int presetCount = 0;
        File root = LittleFS.open("/");
        if (root && root.isDirectory()) {
            File f = root.openNextFile();
            while (f) {
                String fname = f.name();
                if (fname.startsWith("/")) fname = fname.substring(1);
                if (fname.startsWith("peq_") && fname.endsWith(".json")) presetCount++;
                f = root.openNextFile();
            }
        }

        // Check if we're overwriting an existing preset
        char path[40];
        snprintf(path, sizeof(path), "/peq_%s.json", safeName);
        bool overwriting = dspFileExists(path);

        if (!overwriting && presetCount >= 10) {
            sendJsonError(400, "Max 10 presets");
            return;
        }

        // Build preset from current PEQ bands if no bands provided
        JsonDocument preset;
        preset["name"] = safeName;
        if (doc["bands"].is<JsonArray>()) {
            preset["bands"] = doc["bands"];
        } else {
            // Save current PEQ bands from specified channel (default ch 0)
            int ch = doc["ch"] | 0;
            if (ch < 0 || ch >= DSP_MAX_CHANNELS) ch = 0;
            DspState *cfg = dsp_get_active_config();
            JsonArray bands = preset["bands"].to<JsonArray>();
            for (int b = 0; b < DSP_PEQ_BANDS && b < cfg->channels[ch].stageCount; b++) {
                const DspStage &s = cfg->channels[ch].stages[b];
                JsonObject band = bands.add<JsonObject>();
                band["type"] = stage_type_name(s.type);
                band["freq"] = s.biquad.frequency;
                band["gain"] = s.biquad.gain;
                band["Q"] = s.biquad.Q;
                band["enabled"] = s.enabled;
                if (s.label[0]) band["label"] = s.label;
            }
        }

        String json;
        serializeJson(preset, json);
        File f = LittleFS.open(path, "w");
        if (f) { f.print(json); f.close(); }
        server_send(200, "application/json", "{\"success\":true}");
        LOG_I("[DSP] PEQ preset saved: %s", safeName);
    });

    // GET /api/dsp/peq/preset?name=X — load preset
    server_on_versioned("/api/dsp/peq/preset", HTTP_GET, []() {
        if (!requireAuth()) return;
        if (!server.hasArg("name")) { sendJsonError(400, "Name required"); return; }

        char safeName[36];
        if (!sanitize_filename(server.arg("name").c_str(), safeName, sizeof(safeName))) {
            sendJsonError(400, "Invalid name");
            return;
        }
        char path[40];
        snprintf(path, sizeof(path), "/peq_%s.json", safeName);
        if (!dspFileExists(path)) { sendJsonError(404, "Preset not found"); return; }

        File f = LittleFS.open(path, "r");
        if (!f) { sendJsonError(500, "Read error"); return; }
        String json = f.readString();
        f.close();
        server_send(200, "application/json", json);
    });

    // DELETE /api/dsp/peq/preset?name=X — delete preset
    server_on_versioned("/api/dsp/peq/preset", HTTP_DELETE, []() {
        if (!requireAuth()) return;
        if (!server.hasArg("name")) { sendJsonError(400, "Name required"); return; }

        char safeName[36];
        if (!sanitize_filename(server.arg("name").c_str(), safeName, sizeof(safeName))) {
            sendJsonError(400, "Invalid name");
            return;
        }
        char path[40];
        snprintf(path, sizeof(path), "/peq_%s.json", safeName);
        if (!dspFileExists(path)) { sendJsonError(404, "Preset not found"); return; }

        LittleFS.remove(path);
        server_send(200, "application/json", "{\"success\":true}");
        LOG_I("[DSP] PEQ preset deleted: %s", safeName);
    });

    // ===== DSP Config Preset Endpoints (up to 32 slots) =====

    // GET /api/dsp/presets — list all preset slots
    server_on_versioned("/api/dsp/presets", HTTP_GET, []() {
        if (!requireAuth()) return;
JsonDocument doc;
        doc["activeIndex"] = appState.dsp.presetIndex;
        JsonArray slots = doc["slots"].to<JsonArray>();
        for (int i = 0; i < DSP_PRESET_MAX_SLOTS; i++) {
            JsonObject slot = slots.add<JsonObject>();
            slot["index"] = i;
            slot["name"] = appState.dsp.presetNames[i];
            slot["exists"] = dsp_preset_exists(i);
        }
        String json;
        serializeJson(doc, json);
        server_send(200, "application/json", json);
    });

    // POST /api/dsp/presets/save?slot=N — save current config to slot
    server_on_versioned("/api/dsp/presets/save", HTTP_POST, []() {
        if (!requireAuth()) return;
        if (!server.hasArg("slot")) { sendJsonError(400, "Slot required"); return; }
        int slot = server.arg("slot").toInt();
        if (slot < 0 || slot >= DSP_PRESET_MAX_SLOTS) { sendJsonError(400, "Invalid slot (0-31)"); return; }

        const char *name = "";
JsonDocument doc;
        if (server.hasArg("plain") && !deserializeJson(doc, server.arg("plain"))) {
            name = doc["name"] | "";
        }

        if (dsp_preset_save(slot, name)) {
            server_send(200, "application/json", "{\"success\":true}");
        } else {
            sendJsonError(500, "Failed to save preset");
        }
    });

    // POST /api/dsp/presets/load?slot=N — load preset into active config
    server_on_versioned("/api/dsp/presets/load", HTTP_POST, []() {
        if (!requireAuth()) return;
        if (!server.hasArg("slot")) { sendJsonError(400, "Slot required"); return; }
        int slot = server.arg("slot").toInt();
        if (slot < 0 || slot >= DSP_PRESET_MAX_SLOTS) { sendJsonError(400, "Invalid slot (0-31)"); return; }

        if (dsp_preset_load(slot)) {
            server_send(200, "application/json", "{\"success\":true}");
        } else {
            sendJsonError(404, "Preset not found or load failed");
        }
    });

    // DELETE /api/dsp/presets?slot=N — delete preset
    server_on_versioned("/api/dsp/presets", HTTP_DELETE, []() {
        if (!requireAuth()) return;
        if (!server.hasArg("slot")) { sendJsonError(400, "Slot required"); return; }
        int slot = server.arg("slot").toInt();
        if (slot < 0 || slot >= DSP_PRESET_MAX_SLOTS) { sendJsonError(400, "Invalid slot (0-31)"); return; }

        if (dsp_preset_delete(slot)) {
            saveDspSettings();
            server_send(200, "application/json", "{\"success\":true}");
        } else {
            sendJsonError(500, "Failed to delete preset");
        }
    });

    // POST /api/dsp/presets/rename — rename preset
    server_on_versioned("/api/dsp/presets/rename", HTTP_POST, []() {
        if (!requireAuth()) return;
        if (!server.hasArg("slot")) { sendJsonError(400, "Slot required"); return; }
        int slot = server.arg("slot").toInt();
        if (slot < 0 || slot >= DSP_PRESET_MAX_SLOTS) { sendJsonError(400, "Invalid slot (0-31)"); return; }

        String name = "";
JsonDocument doc;
        if (server.hasArg("plain") && !deserializeJson(doc, server.arg("plain"))) {
            name = doc["name"] | "";
        }

        if (name.length() == 0) { sendJsonError(400, "Name required"); return; }

        if (dsp_preset_rename(slot, name.c_str())) {
            server_send(200, "application/json", "{\"success\":true}");
        } else {
            sendJsonError(500, "Failed to rename preset");
        }
    });

    // Delay alignment API endpoints removed in v1.8.3 - incomplete feature, never functional

    // POST /api/dsp/channel/stereolink — toggle stereo link for a channel pair
    server_on_versioned("/api/dsp/channel/stereolink", HTTP_POST, []() {
        if (!requireAuth()) return;
        if (!server.hasArg("plain")) { sendJsonError(400, "No data"); return; }

JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain"))) { sendJsonError(400, "Invalid JSON"); return; }

        int pair = doc["pair"] | -1; // 0 = ch0+1, 1 = ch2+3
        if (pair < 0 || pair > 1) { sendJsonError(400, "Invalid pair (0 or 1)"); return; }
        bool linked = doc["linked"] | true; // cppcheck-suppress badBitmaskCheck

        dsp_copy_active_to_inactive();
        DspState *inactive = dsp_get_inactive_config();
        int chA = pair * 2;
        int chB = pair * 2 + 1;
        inactive->channels[chA].stereoLink = linked;
        inactive->channels[chB].stereoLink = linked;

        if (linked) {
            // Mirror A → B on link enable
            dsp_mirror_channel_config(chA, chB);
        }

        if (!dsp_swap_config()) { dsp_log_swap_failure("DSP API"); sendJsonError(503, "DSP busy, retry"); return; }
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();
        server_send(200, "application/json", "{\"success\":true}");
        LOG_I("[DSP] Stereo link pair %d: %s", pair, linked ? "linked" : "unlinked");
    });

    // POST /api/dsp/convolution/upload?ch=N — upload WAV impulse response for convolution
    server_on_versioned("/api/dsp/convolution/upload", HTTP_POST, []() {
        if (!requireAuth()) return;
        int ch = parseChannelParam();
        if (ch < 0) { sendJsonError(400, "Invalid channel"); return; }
        if (!server.hasArg("plain")) { sendJsonError(400, "No data"); return; }

        const String &body = server.arg("plain");
        if (body.length() == 0) { sendJsonError(400, "Empty body"); return; }
        const size_t maxIrBytes = CONV_MAX_PARTITIONS * CONV_PARTITION_SIZE * sizeof(float);
        if (body.length() > maxIrBytes) { sendJsonError(413, "IR too large"); return; }

        dsp_copy_active_to_inactive();
        DspState *inactive = dsp_get_inactive_config();
        DspChannelConfig &chCfg = inactive->channels[ch];

        // Allocate convolution slot (pool has CONV_MAX_IR_SLOTS=2 slots)
        // Check if channel already has a DSP_CONVOLUTION stage to reuse its slot
        int existingSlot = -1;
        int existingStageIdx = -1;
        for (int s = 0; s < chCfg.stageCount; s++) {
            if (chCfg.stages[s].type == DSP_CONVOLUTION) {
                existingSlot = chCfg.stages[s].convolution.convSlot;
                existingStageIdx = s;
                break;
            }
        }

        // Allocate temp IR buffer for WAV parsing (up to CONV_MAX_PARTITIONS * CONV_PARTITION_SIZE floats)
        const int maxTaps = CONV_MAX_PARTITIONS * CONV_PARTITION_SIZE;
        float *irBuf = (float *)psram_alloc(maxTaps, sizeof(float), "conv_ir_upload");
        if (!irBuf) { sendJsonError(503, "Out of memory"); return; }

        uint32_t sampleRate = inactive->sampleRate > 0 ? inactive->sampleRate : 48000;
        int taps = dsp_parse_wav_ir((const uint8_t *)body.c_str(), (int)body.length(),
                                    irBuf, maxTaps, sampleRate);
        if (taps <= 0) {
            psram_free(irBuf, "conv_ir_upload");
            sendJsonError(400, "Invalid WAV: must be mono PCM/float at matching sample rate");
            return;
        }

        // Free old convolution slot if reusing
        if (existingSlot >= 0) {
            dsp_conv_free_slot(existingSlot);
        }

        // Allocate new convolution slot
        // Find first available slot (0 or 1)
        int convSlot = -1;
        for (int i = 0; i < CONV_MAX_IR_SLOTS; i++) {
            if (!dsp_conv_is_active(i)) { convSlot = i; break; }
        }
        if (convSlot < 0) {
            psram_free(irBuf, "conv_ir_upload");
            sendJsonError(400, "No convolution slots available");
            return;
        }

        int rc = dsp_conv_init_slot(convSlot, irBuf, taps);
        psram_free(irBuf, "conv_ir_upload");
        if (rc < 0) { sendJsonError(500, "Convolution init failed"); return; }

        // Update or add DSP_CONVOLUTION stage
        if (existingStageIdx >= 0) {
            chCfg.stages[existingStageIdx].convolution.convSlot = (int8_t)convSlot;
            chCfg.stages[existingStageIdx].enabled = true;
        } else {
            if (chCfg.stageCount >= DSP_MAX_STAGES) {
                dsp_conv_free_slot(convSlot);
                sendJsonError(400, "Max stages reached");
                return;
            }
            DspStage &s = chCfg.stages[chCfg.stageCount];
            dsp_init_stage(s, DSP_CONVOLUTION);
            s.convolution.convSlot = (int8_t)convSlot;
            chCfg.stageCount++;
        }

        if (!dsp_swap_config()) {
            dsp_conv_free_slot(convSlot);
            dsp_log_swap_failure("DSP API");
            sendJsonError(503, "DSP busy, retry");
            return;
        }
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();

        char resp[80];
        snprintf(resp, sizeof(resp), "{\"success\":true,\"irLength\":%d,\"sampleRate\":%lu}",
                 taps, (unsigned long)sampleRate);
        server_send(200, "application/json", resp);
        LOG_I("[DSP] Convolution upload: ch=%d slot=%d irLength=%d sampleRate=%lu",
              ch, convSlot, taps, (unsigned long)sampleRate);
    });

    LOG_I("[DSP] REST API endpoints registered");
}

// ===== Public: call from main loop for debounced save =====
// This is exposed to the main loop for periodic save checking
void dsp_check_debounced_save() {
    checkDspSave();
}

#endif // DSP_ENABLED
