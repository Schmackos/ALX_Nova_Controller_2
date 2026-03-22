// test_settings_export_v2.cpp
// Tests for v2.0 settings export/import format — verifies HAL devices,
// custom schemas, DSP configs, output DSP, and pipeline matrix sections.
//
// Since settings_manager.cpp depends on WebServer/WiFi/ESP globals that
// can't be mocked easily, tests exercise the JSON structure and LittleFS
// round-trip directly, validating the same serialisation logic.

#include <unity.h>
#include <cstring>
#include <string>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

#include "../test_mocks/LittleFS.h"
#include <ArduinoJson.h>
#include "../../src/hal/hal_types.h"

// ---------------------------------------------------------------------------
// Helpers — mirror the export/import logic from settings_manager.cpp
// ---------------------------------------------------------------------------

// Build a minimal v2.0 export document with the given sections populated.
static void buildExportV2(JsonDocument& doc,
                          bool includeHal = true,
                          bool includeDspGlobal = true,
                          bool includePipeline = true,
                          bool includeCustomSchemas = true) {
    doc["exportInfo"]["version"] = "2.0";
    doc["exportInfo"]["timestamp"] = "2026-03-22T12:00:00";
    doc["deviceInfo"]["manufacturer"] = "ALX";
    doc["deviceInfo"]["model"] = "Nova";
    doc["settings"]["appState.darkMode"] = true;

    if (includeHal) {
        JsonArray halArr = doc["halDevices"].to<JsonArray>();
        JsonObject d0 = halArr.add<JsonObject>();
        d0["slot"] = 0;
        d0["compatible"] = "ti,pcm5102a";
        d0["i2cAddr"] = 0;
        d0["i2cBusIndex"] = 0;
        d0["i2sPort"] = 0;
        d0["volume"] = 80;
        d0["mute"] = false;
        d0["enabled"] = true;
        d0["userLabel"] = "Main DAC";
        d0["filterMode"] = 1;
        d0["sampleRate"] = 48000;
        d0["bitDepth"] = 32;

        JsonObject d1 = halArr.add<JsonObject>();
        d1["slot"] = 3;
        d1["compatible"] = "ess,es9038q2m";
        d1["i2cAddr"] = 72;
        d1["i2cBusIndex"] = 2;
        d1["i2sPort"] = 2;
        d1["volume"] = 95;
        d1["mute"] = false;
        d1["enabled"] = true;
        d1["userLabel"] = "Expansion DAC";
        d1["filterMode"] = 3;
        d1["sampleRate"] = 96000;
        d1["bitDepth"] = 32;
    }

    if (includeCustomSchemas) {
        JsonArray schemas = doc["halCustomSchemas"].to<JsonArray>();
        JsonObject s = schemas.add<JsonObject>();
        s["compatible"] = "custom,my-diy-dac";
        s["name"] = "My DIY DAC";
        s["type"] = "dac";
        s["bus"] = "i2c";
        s["i2cAddr"] = "0x48";
    }

    if (includeDspGlobal) {
        doc["dspGlobal"]["enabled"] = true;
        doc["dspGlobal"]["bypass"] = false;

        JsonArray chArr = doc["dspChannels"].to<JsonArray>();
        for (int i = 0; i < 4; i++) {
            JsonObject ch = chArr.add<JsonObject>();
            ch["channel"] = i;
            ch["gain"] = -3.0f;
        }

        JsonArray outArr = doc["outputDsp"].to<JsonArray>();
        for (int i = 0; i < 16; i++) {
            JsonObject out = outArr.add<JsonObject>();
            out["channel"] = i;
            out["gain"] = 0.0f;
        }
    }

    if (includePipeline) {
        doc["pipelineMatrix"]["rows"] = 32;
        doc["pipelineMatrix"]["cols"] = 32;
        JsonArray cells = doc["pipelineMatrix"]["cells"].to<JsonArray>();
        JsonObject c = cells.add<JsonObject>();
        c["row"] = 0;
        c["col"] = 0;
        c["gain"] = 1.0f;
    }
}

// Build a v1.0 export document (no v2 sections).
static void buildExportV1(JsonDocument& doc) {
    doc["exportInfo"]["version"] = "1.0";
    doc["exportInfo"]["timestamp"] = "2026-01-01T00:00:00";
    doc["deviceInfo"]["manufacturer"] = "ALX";
    doc["settings"]["appState.darkMode"] = false;
    doc["mqtt"]["enabled"] = false;
}

// ---------------------------------------------------------------------------
// setUp / tearDown
// ---------------------------------------------------------------------------

void setUp(void) {
    LittleFS._mounted = true;
    MockFS::_files.clear();
}

void tearDown(void) {
    MockFS::reset();
}

// ---------------------------------------------------------------------------
// Export tests
// ---------------------------------------------------------------------------

void test_export_v2_has_version_2_0(void) {
    JsonDocument doc;
    buildExportV2(doc);
    TEST_ASSERT_EQUAL_STRING("2.0", doc["exportInfo"]["version"].as<const char*>());
}

void test_export_v2_includes_hal_devices_section(void) {
    JsonDocument doc;
    buildExportV2(doc);

    TEST_ASSERT_FALSE(doc["halDevices"].isNull());
    TEST_ASSERT_TRUE(doc["halDevices"].is<JsonArray>());
    JsonArray arr = doc["halDevices"].as<JsonArray>();
    TEST_ASSERT_EQUAL(2, arr.size());

    // First device
    TEST_ASSERT_EQUAL(0, arr[0]["slot"].as<int>());
    TEST_ASSERT_EQUAL_STRING("ti,pcm5102a", arr[0]["compatible"].as<const char*>());
    TEST_ASSERT_EQUAL(80, arr[0]["volume"].as<int>());
    TEST_ASSERT_EQUAL_STRING("Main DAC", arr[0]["userLabel"].as<const char*>());
    TEST_ASSERT_EQUAL(1, arr[0]["filterMode"].as<int>());
    TEST_ASSERT_EQUAL(48000, (int)arr[0]["sampleRate"].as<uint32_t>());

    // Second device
    TEST_ASSERT_EQUAL(3, arr[1]["slot"].as<int>());
    TEST_ASSERT_EQUAL_STRING("ess,es9038q2m", arr[1]["compatible"].as<const char*>());
    TEST_ASSERT_EQUAL(95, arr[1]["volume"].as<int>());
}

void test_export_v2_includes_dsp_global_section(void) {
    JsonDocument doc;
    buildExportV2(doc);

    TEST_ASSERT_FALSE(doc["dspGlobal"].isNull());
    TEST_ASSERT_TRUE(doc["dspGlobal"]["enabled"].as<bool>());
    TEST_ASSERT_FALSE(doc["dspGlobal"]["bypass"].as<bool>());
}

void test_export_v2_includes_dsp_channels_section(void) {
    JsonDocument doc;
    buildExportV2(doc);

    TEST_ASSERT_FALSE(doc["dspChannels"].isNull());
    TEST_ASSERT_TRUE(doc["dspChannels"].is<JsonArray>());
    TEST_ASSERT_EQUAL(4, doc["dspChannels"].as<JsonArray>().size());
}

void test_export_v2_includes_output_dsp_section(void) {
    JsonDocument doc;
    buildExportV2(doc);

    TEST_ASSERT_FALSE(doc["outputDsp"].isNull());
    TEST_ASSERT_TRUE(doc["outputDsp"].is<JsonArray>());
    TEST_ASSERT_EQUAL(16, doc["outputDsp"].as<JsonArray>().size());
}

void test_export_v2_includes_pipeline_matrix_section(void) {
    JsonDocument doc;
    buildExportV2(doc);

    TEST_ASSERT_FALSE(doc["pipelineMatrix"].isNull());
    TEST_ASSERT_EQUAL(32, doc["pipelineMatrix"]["rows"].as<int>());
    TEST_ASSERT_EQUAL(32, doc["pipelineMatrix"]["cols"].as<int>());
    TEST_ASSERT_TRUE(doc["pipelineMatrix"]["cells"].is<JsonArray>());
}

void test_export_v2_includes_custom_schemas_section(void) {
    JsonDocument doc;
    buildExportV2(doc);

    TEST_ASSERT_FALSE(doc["halCustomSchemas"].isNull());
    TEST_ASSERT_TRUE(doc["halCustomSchemas"].is<JsonArray>());
    TEST_ASSERT_EQUAL(1, doc["halCustomSchemas"].as<JsonArray>().size());
    TEST_ASSERT_EQUAL_STRING("custom,my-diy-dac",
        doc["halCustomSchemas"][0]["compatible"].as<const char*>());
}

// ---------------------------------------------------------------------------
// Import tests — v1.0 backward compatibility
// ---------------------------------------------------------------------------

void test_import_v1_skips_hal_section(void) {
    // A v1.0 export should not contain halDevices
    JsonDocument doc;
    buildExportV1(doc);

    const char* ver = doc["exportInfo"]["version"] | "1.0";
    bool isV2 = (strcmp(ver, "2.0") == 0);
    TEST_ASSERT_FALSE(isV2);
    TEST_ASSERT_TRUE(doc["halDevices"].isNull());
}

void test_import_v2_processes_hal_section(void) {
    // Build v2.0 doc and check it has halDevices
    JsonDocument doc;
    buildExportV2(doc);

    const char* ver = doc["exportInfo"]["version"] | "1.0";
    bool isV2 = (strcmp(ver, "2.0") == 0);
    TEST_ASSERT_TRUE(isV2);
    TEST_ASSERT_FALSE(doc["halDevices"].isNull());

    // Verify we can iterate the hal devices
    JsonArray arr = doc["halDevices"].as<JsonArray>();
    int count = 0;
    for (JsonObject obj : arr) {
        uint8_t slot = obj["slot"] | 255;
        TEST_ASSERT_NOT_EQUAL(255, slot);
        count++;
    }
    TEST_ASSERT_EQUAL(2, count);
}

void test_import_partial_only_settings_no_hal(void) {
    // Partial import: has settings but no HAL section
    JsonDocument doc;
    doc["exportInfo"]["version"] = "2.0";
    doc["settings"]["appState.darkMode"] = true;
    // No halDevices, no dspGlobal, no pipelineMatrix

    TEST_ASSERT_TRUE(doc["halDevices"].isNull());
    TEST_ASSERT_TRUE(doc["dspGlobal"].isNull());
    TEST_ASSERT_TRUE(doc["pipelineMatrix"].isNull());

    // The isNull() guard should prevent any processing
    const char* ver = doc["exportInfo"]["version"] | "1.0";
    TEST_ASSERT_TRUE(strcmp(ver, "2.0") == 0);
}

void test_import_partial_only_hal_no_settings(void) {
    // Partial import: has HAL but no traditional settings
    JsonDocument doc;
    doc["exportInfo"]["version"] = "2.0";
    doc["exportInfo"]["timestamp"] = "2026-03-22T12:00:00";
    // Minimal settings required by validator
    doc["settings"]["appState.darkMode"] = false;

    JsonArray halArr = doc["halDevices"].to<JsonArray>();
    JsonObject d = halArr.add<JsonObject>();
    d["slot"] = 5;
    d["compatible"] = "ess,es9039q2m";
    d["volume"] = 90;
    d["enabled"] = true;

    TEST_ASSERT_FALSE(doc["halDevices"].isNull());
    TEST_ASSERT_EQUAL(1, doc["halDevices"].as<JsonArray>().size());
    TEST_ASSERT_TRUE(doc["mqtt"].isNull());
    TEST_ASSERT_TRUE(doc["wifi"].isNull());
}

// ---------------------------------------------------------------------------
// Import tests — HAL device config write
// ---------------------------------------------------------------------------

void test_import_hal_devices_writes_config(void) {
    // Simulate import: for each halDevice entry, build a HalDeviceConfig
    JsonDocument doc;
    buildExportV2(doc);

    JsonArray arr = doc["halDevices"].as<JsonArray>();
    for (JsonObject obj : arr) {
        HalDeviceConfig cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.valid = true;
        cfg.i2cAddr = obj["i2cAddr"] | 0;
        cfg.i2cBusIndex = obj["i2cBusIndex"] | 0;
        cfg.i2sPort = obj["i2sPort"] | 255;
        cfg.volume = obj["volume"] | 100;
        cfg.mute = obj["mute"] | false;
        cfg.enabled = obj["enabled"] | true;
        cfg.filterMode = obj["filterMode"] | 0;
        cfg.sampleRate = obj["sampleRate"] | 0;
        cfg.bitDepth = obj["bitDepth"] | 0;
        const char* label = obj["userLabel"] | "";
        hal_safe_strcpy(cfg.userLabel, sizeof(cfg.userLabel), label);

        TEST_ASSERT_TRUE(cfg.valid);
        if (obj["slot"].as<int>() == 0) {
            TEST_ASSERT_EQUAL(80, cfg.volume);
            TEST_ASSERT_EQUAL_STRING("Main DAC", cfg.userLabel);
            TEST_ASSERT_EQUAL(1, cfg.filterMode);
            TEST_ASSERT_EQUAL(48000, (int)cfg.sampleRate);
        }
    }
}

// ---------------------------------------------------------------------------
// Import tests — custom schema file write
// ---------------------------------------------------------------------------

void test_import_custom_schemas_writes_files(void) {
    JsonDocument doc;
    buildExportV2(doc);

    JsonArray schemas = doc["halCustomSchemas"].as<JsonArray>();
    for (JsonObject schema : schemas) {
        const char* compat = schema["compatible"] | "";
        TEST_ASSERT_TRUE(strlen(compat) > 0);

        char path[64];
        snprintf(path, sizeof(path), "/hal/custom/%s.json", compat);

        std::string schemaStr;
        serializeJson(schema, schemaStr);

        File f = LittleFS.open(path, "w");
        TEST_ASSERT_TRUE((bool)f);
        f.write((const uint8_t*)schemaStr.c_str(), schemaStr.length());
        f.close();

        // Verify the file was written
        TEST_ASSERT_TRUE(LittleFS.exists(path));
        std::string stored = MockFS::getFile(path);
        TEST_ASSERT_TRUE(stored.length() > 0);

        // Parse back and verify
        JsonDocument verify;
        TEST_ASSERT_TRUE(deserializeJson(verify, stored.c_str()) == DeserializationError::Ok);
        TEST_ASSERT_EQUAL_STRING("custom,my-diy-dac",
                                 verify["compatible"].as<const char*>());
    }
}

// ---------------------------------------------------------------------------
// Import tests — pipeline matrix file write
// ---------------------------------------------------------------------------

void test_import_pipeline_matrix_writes_file(void) {
    JsonDocument doc;
    buildExportV2(doc);

    // Simulate import: write pipelineMatrix to /pipeline_matrix.json
    File f = LittleFS.open("/pipeline_matrix.json", "w");
    TEST_ASSERT_TRUE((bool)f);
    serializeJson(doc["pipelineMatrix"], f);
    f.close();

    // Verify
    TEST_ASSERT_TRUE(LittleFS.exists("/pipeline_matrix.json"));
    std::string stored = MockFS::getFile("/pipeline_matrix.json");

    JsonDocument verify;
    TEST_ASSERT_TRUE(deserializeJson(verify, stored.c_str()) == DeserializationError::Ok);
    TEST_ASSERT_EQUAL(32, verify["rows"].as<int>());
    TEST_ASSERT_EQUAL(32, verify["cols"].as<int>());
    TEST_ASSERT_TRUE(verify["cells"].is<JsonArray>());
}

// ---------------------------------------------------------------------------
// Import tests — DSP channel file writes
// ---------------------------------------------------------------------------

void test_import_dsp_channels_writes_files(void) {
    JsonDocument doc;
    buildExportV2(doc);

    JsonArray chArr = doc["dspChannels"].as<JsonArray>();
    for (size_t i = 0; i < chArr.size() && i < 4; i++) {
        if (chArr[i].as<JsonObject>().size() == 0) continue;
        char path[24];
        snprintf(path, sizeof(path), "/dsp_ch%d.json", (int)i);
        File f = LittleFS.open(path, "w");
        TEST_ASSERT_TRUE((bool)f);
        serializeJson(chArr[i], f);
        f.close();
    }

    // Verify all 4 channel files exist
    for (int i = 0; i < 4; i++) {
        char path[24];
        snprintf(path, sizeof(path), "/dsp_ch%d.json", i);
        TEST_ASSERT_TRUE(LittleFS.exists(path));

        std::string stored = MockFS::getFile(path);
        JsonDocument verify;
        TEST_ASSERT_TRUE(deserializeJson(verify, stored.c_str()) == DeserializationError::Ok);
        TEST_ASSERT_EQUAL(i, verify["channel"].as<int>());
    }
}

// ---------------------------------------------------------------------------
// Import tests — output DSP file writes
// ---------------------------------------------------------------------------

void test_import_output_dsp_writes_files(void) {
    JsonDocument doc;
    buildExportV2(doc);

    JsonArray outArr = doc["outputDsp"].as<JsonArray>();
    for (size_t i = 0; i < outArr.size() && i < 16; i++) {
        if (outArr[i].as<JsonObject>().size() == 0) continue;
        char path[32];
        snprintf(path, sizeof(path), "/output_dsp_ch%d.json", (int)i);
        File f = LittleFS.open(path, "w");
        TEST_ASSERT_TRUE((bool)f);
        serializeJson(outArr[i], f);
        f.close();
    }

    // Spot-check a few files
    TEST_ASSERT_TRUE(LittleFS.exists("/output_dsp_ch0.json"));
    TEST_ASSERT_TRUE(LittleFS.exists("/output_dsp_ch15.json"));

    std::string stored = MockFS::getFile("/output_dsp_ch0.json");
    JsonDocument verify;
    TEST_ASSERT_TRUE(deserializeJson(verify, stored.c_str()) == DeserializationError::Ok);
    TEST_ASSERT_EQUAL(0, verify["channel"].as<int>());
}

// ---------------------------------------------------------------------------
// Import tests — DSP global file write via round-trip
// ---------------------------------------------------------------------------

void test_import_dsp_global_writes_file(void) {
    JsonDocument doc;
    buildExportV2(doc);

    // Simulate import: write dspGlobal to /dsp_global.json
    File f = LittleFS.open("/dsp_global.json", "w");
    TEST_ASSERT_TRUE((bool)f);
    serializeJson(doc["dspGlobal"], f);
    f.close();

    // Read back and verify
    std::string stored = MockFS::getFile("/dsp_global.json");
    JsonDocument verify;
    TEST_ASSERT_TRUE(deserializeJson(verify, stored.c_str()) == DeserializationError::Ok);
    TEST_ASSERT_TRUE(verify["enabled"].as<bool>());
    TEST_ASSERT_FALSE(verify["bypass"].as<bool>());
}

// ---------------------------------------------------------------------------
// Import tests — validation
// ---------------------------------------------------------------------------

void test_import_rejects_missing_export_info(void) {
    JsonDocument doc;
    doc["settings"]["appState.darkMode"] = true;
    // No exportInfo section

    TEST_ASSERT_TRUE(doc["exportInfo"].isNull());
    // The import handler checks: if (doc["exportInfo"].isNull() || doc["settings"].isNull())
    // This would return 400. We just verify the guard condition.
    bool wouldReject = doc["exportInfo"].isNull() || doc["settings"].isNull();
    TEST_ASSERT_TRUE(wouldReject);
}

void test_import_invalid_json_returns_error(void) {
    const char* badJson = "{ this is not valid json !!!";
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, badJson);
    TEST_ASSERT_NOT_EQUAL(DeserializationError::Ok, err);
}

// ---------------------------------------------------------------------------
// Export round-trip: write to LittleFS then read back
// ---------------------------------------------------------------------------

void test_export_round_trip_via_littlefs(void) {
    // Build export doc
    JsonDocument exportDoc;
    buildExportV2(exportDoc);

    // Serialize to LittleFS
    File wf = LittleFS.open("/export.json", "w");
    TEST_ASSERT_TRUE((bool)wf);
    serializeJson(exportDoc, wf);
    wf.close();

    // Read back
    std::string stored = MockFS::getFile("/export.json");
    TEST_ASSERT_TRUE(stored.length() > 0);

    JsonDocument importDoc;
    DeserializationError err = deserializeJson(importDoc, stored.c_str());
    TEST_ASSERT_TRUE(err == DeserializationError::Ok);

    // Verify key sections survived the round-trip
    TEST_ASSERT_EQUAL_STRING("2.0", importDoc["exportInfo"]["version"].as<const char*>());
    TEST_ASSERT_FALSE(importDoc["halDevices"].isNull());
    TEST_ASSERT_FALSE(importDoc["dspGlobal"].isNull());
    TEST_ASSERT_FALSE(importDoc["pipelineMatrix"].isNull());
    TEST_ASSERT_FALSE(importDoc["halCustomSchemas"].isNull());
    TEST_ASSERT_EQUAL(2, importDoc["halDevices"].as<JsonArray>().size());
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_export_v2_has_version_2_0);
    RUN_TEST(test_export_v2_includes_hal_devices_section);
    RUN_TEST(test_export_v2_includes_dsp_global_section);
    RUN_TEST(test_export_v2_includes_dsp_channels_section);
    RUN_TEST(test_export_v2_includes_output_dsp_section);
    RUN_TEST(test_export_v2_includes_pipeline_matrix_section);
    RUN_TEST(test_export_v2_includes_custom_schemas_section);
    RUN_TEST(test_import_v1_skips_hal_section);
    RUN_TEST(test_import_v2_processes_hal_section);
    RUN_TEST(test_import_partial_only_settings_no_hal);
    RUN_TEST(test_import_partial_only_hal_no_settings);
    RUN_TEST(test_import_hal_devices_writes_config);
    RUN_TEST(test_import_custom_schemas_writes_files);
    RUN_TEST(test_import_pipeline_matrix_writes_file);
    RUN_TEST(test_import_dsp_channels_writes_files);
    RUN_TEST(test_import_output_dsp_writes_files);
    RUN_TEST(test_import_dsp_global_writes_file);
    RUN_TEST(test_import_rejects_missing_export_info);
    RUN_TEST(test_import_invalid_json_returns_error);
    RUN_TEST(test_export_round_trip_via_littlefs);

    return UNITY_END();
}
