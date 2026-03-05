#ifdef DAC_ENABLED
#ifndef NATIVE_TEST

#include "pipeline_api.h"
#include "audio_pipeline.h"
#include "audio_output_sink.h"
#include "debug_serial.h"
#include "config.h"
#include <WebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

extern WebServer server;

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
        server.send(200, "application/json", json);
    });

    // POST /api/pipeline/matrix/cell — set a single cell: {out, in, gainDb}
    server.on("/api/pipeline/matrix/cell", HTTP_POST, []() {
        if (!server.hasArg("plain")) {
            server.send(400, "application/json", "{\"error\":\"no body\"}");
            return;
        }

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, server.arg("plain"));
        if (err) {
            server.send(400, "application/json", "{\"error\":\"parse error\"}");
            return;
        }

        int out_ch = doc["out"] | -1;
        int in_ch  = doc["in"]  | -1;
        float gainDb = doc["gainDb"] | -96.0f;

        if (out_ch < 0 || out_ch >= AUDIO_PIPELINE_MATRIX_SIZE ||
            in_ch  < 0 || in_ch  >= AUDIO_PIPELINE_MATRIX_SIZE) {
            server.send(400, "application/json", "{\"error\":\"channel out of range\"}");
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
        server.send(200, "application/json", json);
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
        server.send(200, "application/json", json);
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
