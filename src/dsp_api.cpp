#ifdef DSP_ENABLED

#include "dsp_api.h"
#include "dsp_pipeline.h"
#include "dsp_coefficients.h"
#include "dsp_rew_parser.h"
#include "dsp_crossover.h"
#include "app_state.h"
#include "auth_handler.h"
#include "debug_serial.h"
#include <ArduinoJson.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <sys/stat.h>

extern WebServer server;

// Check if a LittleFS file exists without triggering VFS "no permits" error log.
// Arduino's dspFileExists() internally calls open("r") which logs the error.
static bool dspFileExists(const char *path) {
    struct stat st;
    String fullPath = String("/littlefs") + path;
    return (stat(fullPath.c_str(), &st) == 0);
}

// ===== Global Routing Matrix =====
static DspRoutingMatrix _routingMatrix;
static bool _routingMatrixLoaded = false;

DspRoutingMatrix* dsp_get_routing_matrix() {
    if (!_routingMatrixLoaded) {
        dsp_routing_init(_routingMatrix);
        _routingMatrixLoaded = true;
    }
    return &_routingMatrix;
}

// ===== Routing Matrix Persistence =====

static void loadRoutingMatrix() {
    if (!dspFileExists("/dsp_routing.json")) {
        dsp_routing_init(_routingMatrix);
        _routingMatrixLoaded = true;
        return;
    }
    File f = LittleFS.open("/dsp_routing.json", "r");
    if (f && f.size() > 0) {
        String json = f.readString();
        f.close();

        JsonDocument doc;
        if (!deserializeJson(doc, json)) {
            JsonArray mat = doc["matrix"].as<JsonArray>();
            if (!mat.isNull()) {
                for (int o = 0; o < DSP_MAX_CHANNELS && o < (int)mat.size(); o++) {
                    JsonArray row = mat[o].as<JsonArray>();
                    if (row.isNull()) continue;
                    for (int i = 0; i < DSP_MAX_CHANNELS && i < (int)row.size(); i++) {
                        _routingMatrix.matrix[o][i] = row[i].as<float>();
                    }
                }
            }
        }
        _routingMatrixLoaded = true;
        LOG_I("[DSP] Routing matrix loaded from LittleFS");
    } else {
        if (f) f.close();
        dsp_routing_init(_routingMatrix);
        _routingMatrixLoaded = true;
    }
}

static void saveRoutingMatrix() {
    JsonDocument doc;
    JsonArray mat = doc["matrix"].to<JsonArray>();
    for (int o = 0; o < DSP_MAX_CHANNELS; o++) {
        JsonArray row = mat.add<JsonArray>();
        for (int i = 0; i < DSP_MAX_CHANNELS; i++) {
            row.add(_routingMatrix.matrix[o][i]);
        }
    }

    String json;
    serializeJson(doc, json);
    File f = LittleFS.open("/dsp_routing.json", "w");
    if (f) { f.print(json); f.close(); }
    LOG_I("[DSP] Routing matrix saved to LittleFS");
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
                if (doc["dspEnabled"].is<bool>()) appState.dspEnabled = doc["dspEnabled"].as<bool>();
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

    // Load routing matrix
    loadRoutingMatrix();

    // Recompute all coefficients and swap to make loaded config active
    DspState *cfg = dsp_get_inactive_config();
    for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
        dsp_recompute_channel_coeffs(cfg->channels[ch], cfg->sampleRate);
    }
    dsp_swap_config();

    LOG_I("[DSP] Settings loaded from LittleFS");
}

void saveDspSettings() {
    // Save global settings
    JsonDocument globalDoc;
    DspState *cfg = dsp_get_active_config();
    globalDoc["globalBypass"] = cfg->globalBypass;
    globalDoc["sampleRate"] = cfg->sampleRate;
    globalDoc["dspEnabled"] = appState.dspEnabled;

    String globalJson;
    serializeJson(globalDoc, globalJson);
    File f = LittleFS.open("/dsp_global.json", "w");
    if (f) { f.print(globalJson); f.close(); }

    // Save per-channel configs
    for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
        char path[24];
        snprintf(path, sizeof(path), "/dsp_ch%d.json", ch);

        char buf[4096];
        dsp_export_config_to_json(ch, buf, sizeof(buf));
        File cf = LittleFS.open(path, "w");
        if (cf) { cf.print(buf); cf.close(); }

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
    server.send(code, "application/json", buf);
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
    return DSP_BIQUAD_PEQ;
}

// ===== API Endpoint Registration =====

void registerDspApiEndpoints() {
    // GET /api/dsp — full config
    server.on("/api/dsp", HTTP_GET, []() {
        if (!requireAuth()) return;
        char buf[8192];
        dsp_export_full_config_json(buf, sizeof(buf));

        // Wrap with dspEnabled
        JsonDocument doc;
        deserializeJson(doc, buf);
        doc["dspEnabled"] = appState.dspEnabled;

        String json;
        serializeJson(doc, json);
        server.send(200, "application/json", json);
    });

    // PUT /api/dsp — replace full config
    server.on("/api/dsp", HTTP_PUT, []() {
        if (!requireAuth()) return;
        if (!server.hasArg("plain")) { sendJsonError(400, "No data"); return; }

        dsp_import_full_config_json(server.arg("plain").c_str());

        JsonDocument doc;
        if (!deserializeJson(doc, server.arg("plain"))) {
            if (doc["dspEnabled"].is<bool>()) appState.dspEnabled = doc["dspEnabled"].as<bool>();
        }

        dsp_swap_config();
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();
        server.send(200, "application/json", "{\"success\":true}");
        LOG_I("[DSP] Full config replaced via API");
    });

    // POST /api/dsp/bypass — toggle global bypass
    server.on("/api/dsp/bypass", HTTP_POST, []() {
        if (!requireAuth()) return;
        dsp_copy_active_to_inactive();
        DspState *cfg = dsp_get_inactive_config();
        if (server.hasArg("plain")) {
            JsonDocument doc;
            if (!deserializeJson(doc, server.arg("plain"))) {
                if (doc["bypass"].is<bool>()) cfg->globalBypass = doc["bypass"].as<bool>();
                if (doc["enabled"].is<bool>()) appState.dspEnabled = doc["enabled"].as<bool>();
            }
        } else {
            cfg->globalBypass = !cfg->globalBypass;
        }
        dsp_swap_config();
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();
        server.send(200, "application/json", "{\"success\":true}");
    });

    // GET /api/dsp/metrics
    server.on("/api/dsp/metrics", HTTP_GET, []() {
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
        server.send(200, "application/json", json);
    });

    // GET /api/dsp/channel?ch=N — get channel config
    server.on("/api/dsp/channel", HTTP_GET, []() {
        if (!requireAuth()) return;
        int ch = parseChannelParam();
        if (ch < 0) { sendJsonError(400, "Invalid channel"); return; }

        char buf[4096];
        dsp_export_config_to_json(ch, buf, sizeof(buf));
        server.send(200, "application/json", buf);
    });

    // POST /api/dsp/channel/bypass?ch=N — toggle channel bypass
    server.on("/api/dsp/channel/bypass", HTTP_POST, []() {
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
        dsp_swap_config();
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();
        server.send(200, "application/json", "{\"success\":true}");
    });

    // POST /api/dsp/stage?ch=N — add stage
    server.on("/api/dsp/stage", HTTP_POST, []() {
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
        }

        dsp_swap_config();
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();

        char resp[64];
        snprintf(resp, sizeof(resp), "{\"success\":true,\"index\":%d}", idx);
        server.send(200, "application/json", resp);
        LOG_I("[DSP] Stage added: ch=%d type=%d pos=%d", ch, type, idx);
    });

    // PUT /api/dsp/stage?ch=N&stage=M — update stage params
    server.on("/api/dsp/stage", HTTP_PUT, []() {
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
        }

        dsp_swap_config();
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();
        server.send(200, "application/json", "{\"success\":true}");
    });

    // DELETE /api/dsp/stage?ch=N&stage=M — remove stage
    server.on("/api/dsp/stage", HTTP_DELETE, []() {
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

        dsp_swap_config();
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();
        server.send(200, "application/json", "{\"success\":true}");
        LOG_I("[DSP] Stage removed: ch=%d stage=%d", ch, si);
    });

    // POST /api/dsp/stage/reorder?ch=N — reorder stages
    server.on("/api/dsp/stage/reorder", HTTP_POST, []() {
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

        dsp_swap_config();
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();
        server.send(200, "application/json", "{\"success\":true}");
    });

    // POST /api/dsp/stage/enable?ch=N&stage=M — toggle stage enable
    server.on("/api/dsp/stage/enable", HTTP_POST, []() {
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

        dsp_swap_config();
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();
        server.send(200, "application/json", "{\"success\":true}");
    });

    // ===== Import/Export Endpoints =====

    // POST /api/dsp/import/apo?ch=N
    server.on("/api/dsp/import/apo", HTTP_POST, []() {
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

        dsp_swap_config();
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();

        char resp[64];
        snprintf(resp, sizeof(resp), "{\"success\":true,\"added\":%d}", added);
        server.send(200, "application/json", resp);
        LOG_I("[DSP] APO import: %d filters added to ch=%d", added, ch);
    });

    // POST /api/dsp/import/minidsp?ch=N
    server.on("/api/dsp/import/minidsp", HTTP_POST, []() {
        if (!requireAuth()) return;
        int ch = parseChannelParam();
        if (ch < 0) { sendJsonError(400, "Invalid channel"); return; }
        if (!server.hasArg("plain")) { sendJsonError(400, "No data"); return; }

        DspState *inactive = dsp_get_inactive_config();
        dsp_copy_active_to_inactive();

        int added = dsp_parse_minidsp_biquads(server.arg("plain").c_str(),
                                                inactive->channels[ch]);

        if (added < 0) { sendJsonError(400, "Parse error"); return; }

        dsp_swap_config();
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();

        char resp[64];
        snprintf(resp, sizeof(resp), "{\"success\":true,\"added\":%d}", added);
        server.send(200, "application/json", resp);
    });

    // POST /api/dsp/import/fir?ch=N — import FIR text coefficients
    server.on("/api/dsp/import/fir", HTTP_POST, []() {
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

        dsp_swap_config();
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();

        char resp[64];
        snprintf(resp, sizeof(resp), "{\"success\":true,\"taps\":%d}", taps);
        server.send(200, "application/json", resp);
        LOG_I("[DSP] FIR import: %d taps to ch=%d", taps, ch);
    });

    // GET /api/dsp/export/apo?ch=N
    server.on("/api/dsp/export/apo", HTTP_GET, []() {
        if (!requireAuth()) return;
        int ch = parseChannelParam();
        if (ch < 0) { sendJsonError(400, "Invalid channel"); return; }

        DspState *cfg = dsp_get_active_config();
        char buf[2048];
        dsp_export_apo(cfg->channels[ch], cfg->sampleRate, buf, sizeof(buf));
        server.send(200, "text/plain", buf);
    });

    // GET /api/dsp/export/minidsp?ch=N
    server.on("/api/dsp/export/minidsp", HTTP_GET, []() {
        if (!requireAuth()) return;
        int ch = parseChannelParam();
        if (ch < 0) { sendJsonError(400, "Invalid channel"); return; }

        DspState *cfg = dsp_get_active_config();
        char buf[2048];
        dsp_export_minidsp(cfg->channels[ch], buf, sizeof(buf));
        server.send(200, "text/plain", buf);
    });

    // GET /api/dsp/export/json
    server.on("/api/dsp/export/json", HTTP_GET, []() {
        if (!requireAuth()) return;
        char buf[8192];
        dsp_export_full_config_json(buf, sizeof(buf));
        server.send(200, "application/json", buf);
    });

    // ===== Crossover & Bass Management Endpoints =====

    // POST /api/dsp/crossover?ch=N — apply crossover filter
    server.on("/api/dsp/crossover", HTTP_POST, []() {
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

        int result = -1;
        if (strncmp(typeStr, "lr", 2) == 0) {
            int order = atoi(typeStr + 2);
            result = dsp_insert_crossover_lr(ch, freq, order, role);
        } else if (strncmp(typeStr, "bw", 2) == 0) {
            int order = atoi(typeStr + 2);
            result = dsp_insert_crossover_butterworth(ch, freq, order, role);
        } else {
            sendJsonError(400, "Unknown crossover type");
            return;
        }

        if (result < 0) { sendJsonError(400, "Failed to insert crossover"); return; }

        dsp_swap_config();
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();

        char resp[64];
        snprintf(resp, sizeof(resp), "{\"success\":true,\"firstStage\":%d}", result);
        server.send(200, "application/json", resp);
        LOG_I("[DSP] Crossover applied: ch=%d type=%s freq=%.0f role=%d", ch, typeStr, freq, role);
    });

    // POST /api/dsp/bassmanagement — setup bass management
    server.on("/api/dsp/bassmanagement", HTTP_POST, []() {
        if (!requireAuth()) return;
        if (!server.hasArg("plain")) { sendJsonError(400, "No data"); return; }

        JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain"))) { sendJsonError(400, "Invalid JSON"); return; }

        int subChannel = doc["subChannel"] | 0;
        float crossoverFreq = doc["freq"] | 80.0f;

        if (!doc["mainChannels"].is<JsonArray>()) {
            sendJsonError(400, "Missing mainChannels array");
            return;
        }

        JsonArray mains = doc["mainChannels"].as<JsonArray>();
        int mainChannels[DSP_MAX_CHANNELS];
        int numMains = 0;
        for (JsonVariant v : mains) {
            if (numMains >= DSP_MAX_CHANNELS) break;
            mainChannels[numMains++] = v.as<int>();
        }

        dsp_copy_active_to_inactive();

        int result = dsp_setup_bass_management(subChannel, mainChannels, numMains, crossoverFreq);
        if (result < 0) { sendJsonError(400, "Failed to setup bass management"); return; }

        dsp_swap_config();
        saveDspSettingsDebounced();
        appState.markDspConfigDirty();
        server.send(200, "application/json", "{\"success\":true}");
        LOG_I("[DSP] Bass management: sub=%d mains=%d freq=%.0f", subChannel, numMains, crossoverFreq);
    });

    // ===== Routing Matrix Endpoints =====

    // GET /api/dsp/routing — get routing matrix
    server.on("/api/dsp/routing", HTTP_GET, []() {
        if (!requireAuth()) return;

        DspRoutingMatrix *rm = dsp_get_routing_matrix();
        JsonDocument doc;
        JsonArray mat = doc["matrix"].to<JsonArray>();
        for (int o = 0; o < DSP_MAX_CHANNELS; o++) {
            JsonArray row = mat.add<JsonArray>();
            for (int i = 0; i < DSP_MAX_CHANNELS; i++) {
                row.add(rm->matrix[o][i]);
            }
        }

        String json;
        serializeJson(doc, json);
        server.send(200, "application/json", json);
    });

    // PUT /api/dsp/routing — set routing matrix
    server.on("/api/dsp/routing", HTTP_PUT, []() {
        if (!requireAuth()) return;
        if (!server.hasArg("plain")) { sendJsonError(400, "No data"); return; }

        JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain"))) { sendJsonError(400, "Invalid JSON"); return; }

        DspRoutingMatrix *rm = dsp_get_routing_matrix();

        // Check for preset shortcut
        const char *preset = doc["preset"] | (const char *)nullptr;
        if (preset) {
            if (strcmp(preset, "identity") == 0) dsp_routing_preset_identity(*rm);
            else if (strcmp(preset, "mono_sum") == 0) dsp_routing_preset_mono_sum(*rm);
            else if (strcmp(preset, "swap_lr") == 0) dsp_routing_preset_swap_lr(*rm);
            else if (strcmp(preset, "sub_sum") == 0) dsp_routing_preset_sub_sum(*rm);
            else { sendJsonError(400, "Unknown preset"); return; }
        } else if (doc["matrix"].is<JsonArray>()) {
            JsonArray mat = doc["matrix"].as<JsonArray>();
            for (int o = 0; o < DSP_MAX_CHANNELS && o < (int)mat.size(); o++) {
                JsonArray row = mat[o].as<JsonArray>();
                if (row.isNull()) continue;
                for (int i = 0; i < DSP_MAX_CHANNELS && i < (int)row.size(); i++) {
                    rm->matrix[o][i] = row[i].as<float>();
                }
            }
        } else if (doc["output"].is<int>() && doc["input"].is<int>() && doc["gainDb"].is<float>()) {
            // Single cell update
            dsp_routing_set_gain_db(*rm, doc["output"].as<int>(), doc["input"].as<int>(), doc["gainDb"].as<float>());
        } else {
            sendJsonError(400, "Provide preset, matrix, or output/input/gainDb");
            return;
        }

        saveRoutingMatrix();
        appState.markDspConfigDirty();
        server.send(200, "application/json", "{\"success\":true}");
        LOG_I("[DSP] Routing matrix updated");
    });

    LOG_I("[DSP] REST API endpoints registered");
}

// ===== Public: call from main loop for debounced save =====
// This is exposed to the main loop for periodic save checking
void dsp_check_debounced_save() {
    checkDspSave();
}

#endif // DSP_ENABLED
