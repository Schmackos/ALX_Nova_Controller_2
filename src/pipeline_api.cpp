#ifdef DAC_ENABLED
#ifndef NATIVE_TEST

#include "pipeline_api.h"
#include "http_security.h"
#include "audio_pipeline.h"
#include "audio_output_sink.h"
#include "output_dsp.h"
#include "debug_serial.h"
#include "globals.h"
#include "config.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

// Deferred matrix save: 2 seconds after last cell change
static unsigned long _matrixSavePending = 0;
static const unsigned long MATRIX_SAVE_DELAY_MS = 2000;

void registerPipelineApiEndpoints(WebServer& /*srv*/) {
    // GET /api/pipeline/matrix — returns 8x8 gain array + input/output names
    server.on("/api/pipeline/matrix", HTTP_GET, []() {
        JsonDocument doc;

        // Matrix gains
        JsonArray matrix = doc["matrix"].to<JsonArray>();
        for (int o = 0; o < AUDIO_PIPELINE_MATRIX_SIZE; o++) {
            JsonArray row = matrix.add<JsonArray>();
            for (int i = 0; i < AUDIO_PIPELINE_MATRIX_SIZE; i++) {
                row.add(audio_pipeline_get_matrix_gain(o, i));
            }
        }

        // Input channel names
        JsonArray inputs = doc["inputs"].to<JsonArray>();
        inputs.add("ADC1 L"); inputs.add("ADC1 R");
        inputs.add("ADC2 L"); inputs.add("ADC2 R");
        inputs.add("SigGen L"); inputs.add("SigGen R");
        inputs.add("USB L"); inputs.add("USB R");

        // Output channel names
        JsonArray outputs = doc["outputs"].to<JsonArray>();
        outputs.add("Out 0 (L)"); outputs.add("Out 1 (R)");
        outputs.add("Out 2"); outputs.add("Out 3");
        outputs.add("Out 4"); outputs.add("Out 5");
        outputs.add("Out 6"); outputs.add("Out 7");

        doc["bypass"] = audio_pipeline_is_matrix_bypass();

        String json;
        serializeJson(doc, json);
        server_send(200, "application/json", json);
    });

    // POST /api/pipeline/matrix/cell — set a single cell: {out, in, gainDb}
    server.on("/api/pipeline/matrix/cell", HTTP_POST, []() {
        if (!server.hasArg("plain")) {
            server_send(400, "application/json", "{\"error\":\"no body\"}");
            return;
        }

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, server.arg("plain"));
        if (err) {
            server_send(400, "application/json", "{\"error\":\"parse error\"}");
            return;
        }

        int out_ch = doc["out"] | -1;
        int in_ch  = doc["in"]  | -1;
        float gainDb = doc["gainDb"] | -96.0f;

        if (out_ch < 0 || out_ch >= AUDIO_PIPELINE_MATRIX_SIZE ||
            in_ch  < 0 || in_ch  >= AUDIO_PIPELINE_MATRIX_SIZE) {
            server_send(400, "application/json", "{\"error\":\"channel out of range\"}");
            return;
        }

        audio_pipeline_set_matrix_gain_db(out_ch, in_ch, gainDb);

        // Schedule deferred save
        _matrixSavePending = millis() + MATRIX_SAVE_DELAY_MS;

        JsonDocument resp;
        resp["status"] = "ok";
        resp["out"] = out_ch;
        resp["in"] = in_ch;
        resp["gainDb"] = gainDb;
        resp["gainLinear"] = audio_pipeline_get_matrix_gain(out_ch, in_ch);
        String json;
        serializeJson(resp, json);
        server_send(200, "application/json", json);
    });

    // GET /api/pipeline/sinks — registered output sinks with VU/ready state
    server.on("/api/pipeline/sinks", HTTP_GET, []() {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();

        int count = audio_pipeline_get_sink_count();
        for (int s = 0; s < count; s++) {
            const AudioOutputSink* sink = audio_pipeline_get_sink(s);
            if (!sink) continue;
            JsonObject obj = arr.add<JsonObject>();
            obj["name"] = sink->name ? sink->name : "?";
            obj["firstChannel"] = sink->firstChannel;
            obj["channelCount"] = sink->channelCount;
            obj["gainLinear"] = sink->gainLinear;
            obj["muted"] = sink->muted;
            obj["ready"] = (sink->isReady && sink->isReady());
            obj["vuL"] = sink->vuL;
            obj["vuR"] = sink->vuR;
        }

        String json;
        serializeJson(doc, json);
        server_send(200, "application/json", json);
    });

    // GET /api/output/dsp?ch=N — get output DSP channel config
    server.on("/api/output/dsp", HTTP_GET, []() {
        int ch = server.hasArg("ch") ? server.arg("ch").toInt() : -1;
        if (ch < 0 || ch >= OUTPUT_DSP_MAX_CHANNELS) {
            server_send(400, "application/json", "{\"error\":\"invalid channel\"}");
            return;
        }

        OutputDspState *cfg = output_dsp_get_active_config();
        OutputDspChannelConfig &channel = cfg->channels[ch];

        JsonDocument doc;
        doc["channel"] = ch;
        doc["bypass"] = channel.bypass;
        doc["globalBypass"] = cfg->globalBypass;
        doc["sampleRate"] = cfg->sampleRate;

        JsonArray stages = doc["stages"].to<JsonArray>();
        for (int i = 0; i < channel.stageCount; i++) {
            OutputDspStage &s = channel.stages[i];
            JsonObject obj = stages.add<JsonObject>();
            obj["index"] = i;
            obj["enabled"] = s.enabled;
            obj["type"] = stage_type_name(s.type);
            if (s.label[0]) obj["label"] = s.label;

            if (dsp_is_biquad_type(s.type)) {
                obj["frequency"] = s.biquad.frequency;
                obj["gain"] = s.biquad.gain;
                obj["Q"] = s.biquad.Q;
            } else if (s.type == DSP_GAIN) {
                obj["gainDb"] = s.gain.gainDb;
            } else if (s.type == DSP_LIMITER) {
                obj["thresholdDb"] = s.limiter.thresholdDb;
                obj["attackMs"] = s.limiter.attackMs;
                obj["releaseMs"] = s.limiter.releaseMs;
                obj["ratio"] = s.limiter.ratio;
                obj["gainReduction"] = s.limiter.gainReduction;
            } else if (s.type == DSP_COMPRESSOR) {
                obj["thresholdDb"] = s.compressor.thresholdDb;
                obj["ratio"] = s.compressor.ratio;
                obj["gainReduction"] = s.compressor.gainReduction;
            } else if (s.type == DSP_POLARITY) {
                obj["inverted"] = s.polarity.inverted;
            } else if (s.type == DSP_MUTE) {
                obj["muted"] = s.mute.muted;
            }
        }

        String json;
        serializeJson(doc, json);
        server_send(200, "application/json", json);
    });

    // POST /api/output/dsp/stage — add stage: {ch, type, position?}
    server.on("/api/output/dsp/stage", HTTP_POST, []() {
        if (!server.hasArg("plain")) { server_send(400, "application/json", "{\"error\":\"no body\"}"); return; }
        JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain"))) { server_send(400, "application/json", "{\"error\":\"parse\"}"); return; }

        int ch = doc["ch"] | -1;
        const char *typeName = doc["type"] | "";
        int position = doc["position"] | -1;

        if (ch < 0 || ch >= OUTPUT_DSP_MAX_CHANNELS) {
            server_send(400, "application/json", "{\"error\":\"invalid channel\"}");
            return;
        }

        // Map type name to enum — reuse the type_from_name pattern
        DspStageType type = DSP_BIQUAD_PEQ;
        if (strcmp(typeName, "LPF") == 0) type = DSP_BIQUAD_LPF;
        else if (strcmp(typeName, "HPF") == 0) type = DSP_BIQUAD_HPF;
        else if (strcmp(typeName, "BPF") == 0) type = DSP_BIQUAD_BPF;
        else if (strcmp(typeName, "PEQ") == 0) type = DSP_BIQUAD_PEQ;
        else if (strcmp(typeName, "NOTCH") == 0) type = DSP_BIQUAD_NOTCH;
        else if (strcmp(typeName, "LOW_SHELF") == 0) type = DSP_BIQUAD_LOW_SHELF;
        else if (strcmp(typeName, "HIGH_SHELF") == 0) type = DSP_BIQUAD_HIGH_SHELF;
        else if (strcmp(typeName, "LIMITER") == 0) type = DSP_LIMITER;
        else if (strcmp(typeName, "GAIN") == 0) type = DSP_GAIN;
        else if (strcmp(typeName, "POLARITY") == 0) type = DSP_POLARITY;
        else if (strcmp(typeName, "MUTE") == 0) type = DSP_MUTE;
        else if (strcmp(typeName, "COMPRESSOR") == 0) type = DSP_COMPRESSOR;

        output_dsp_copy_active_to_inactive();
        int idx = output_dsp_add_stage(ch, type, position);
        if (idx < 0) {
            server_send(400, "application/json", "{\"error\":\"add failed\"}");
            return;
        }
        output_dsp_swap_config();
        output_dsp_save_channel(ch);

        JsonDocument resp;
        resp["status"] = "ok";
        resp["ch"] = ch;
        resp["index"] = idx;
        resp["type"] = typeName;
        String json;
        serializeJson(resp, json);
        server_send(200, "application/json", json);
    });

    // DELETE /api/output/dsp/stage — remove stage: {ch, index}
    server.on("/api/output/dsp/stage", HTTP_DELETE, []() {
        if (!server.hasArg("plain")) { server_send(400, "application/json", "{\"error\":\"no body\"}"); return; }
        JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain"))) { server_send(400, "application/json", "{\"error\":\"parse\"}"); return; }

        int ch = doc["ch"] | -1;
        int idx = doc["index"] | -1;

        if (ch < 0 || ch >= OUTPUT_DSP_MAX_CHANNELS) {
            server_send(400, "application/json", "{\"error\":\"invalid channel\"}");
            return;
        }

        output_dsp_copy_active_to_inactive();
        bool ok = output_dsp_remove_stage(ch, idx);
        if (!ok) {
            server_send(400, "application/json", "{\"error\":\"remove failed\"}");
            return;
        }
        output_dsp_swap_config();
        output_dsp_save_channel(ch);

        server_send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // PUT /api/output/dsp — set channel config: {ch, bypass, stages:[...]}
    server.on("/api/output/dsp", HTTP_PUT, []() {
        if (!server.hasArg("plain")) { server_send(400, "application/json", "{\"error\":\"no body\"}"); return; }
        JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain"))) { server_send(400, "application/json", "{\"error\":\"parse\"}"); return; }

        int ch = doc["ch"] | -1;
        if (ch < 0 || ch >= OUTPUT_DSP_MAX_CHANNELS) {
            server_send(400, "application/json", "{\"error\":\"invalid channel\"}");
            return;
        }

        output_dsp_copy_active_to_inactive();
        OutputDspState *cfg = output_dsp_get_inactive_config();
        OutputDspChannelConfig &channel = cfg->channels[ch];

        if (doc.containsKey("bypass")) {
            channel.bypass = doc["bypass"].as<bool>();
        }
        if (doc.containsKey("globalBypass")) {
            cfg->globalBypass = doc["globalBypass"].as<bool>();
        }
        if (doc.containsKey("stageIndex") && doc.containsKey("stageEnabled")) {
            int idx = doc["stageIndex"].as<int>();
            if (idx >= 0 && idx < channel.stageCount) {
                channel.stages[idx].enabled = doc["stageEnabled"].as<bool>();
            }
        }

        output_dsp_swap_config();
        output_dsp_save_channel(ch);

        server_send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // POST /api/output/dsp/crossover — setup crossover: {subCh, mainCh, freqHz, order}
    server.on("/api/output/dsp/crossover", HTTP_POST, []() {
        if (!server.hasArg("plain")) { server_send(400, "application/json", "{\"error\":\"no body\"}"); return; }
        JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain"))) { server_send(400, "application/json", "{\"error\":\"parse\"}"); return; }

        int subCh = doc["subCh"] | -1;
        int mainCh = doc["mainCh"] | -1;
        float freqHz = doc["freqHz"] | 80.0f;
        int order = doc["order"] | 4;

        output_dsp_copy_active_to_inactive();
        int result = output_dsp_setup_crossover(subCh, mainCh, freqHz, order);
        if (result < 0) {
            server_send(400, "application/json", "{\"error\":\"crossover setup failed\"}");
            return;
        }
        output_dsp_swap_config();
        output_dsp_save_channel(subCh);
        output_dsp_save_channel(mainCh);

        JsonDocument resp;
        resp["status"] = "ok";
        resp["stagesAdded"] = result;
        String json;
        serializeJson(resp, json);
        server_send(200, "application/json", json);
    });

    LOG_I("[Pipeline API] Registered pipeline REST endpoints");
}

// Call from main loop to flush deferred matrix save
void pipeline_api_check_deferred_save() {
    if (_matrixSavePending > 0 && millis() >= _matrixSavePending) {
        _matrixSavePending = 0;
        audio_pipeline_save_matrix();
    }
}

#endif // NATIVE_TEST
#endif // DAC_ENABLED
