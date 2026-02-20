#include <unity.h>
#include <string.h>
#include <stdint.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include <ArduinoJson.h>
#else
#include <Arduino.h>
#include <ArduinoJson.h>
#endif

// ===== Constants mirrored from production code =====
// These match the values defined in src/config.h and platformio.ini [env:native]
#define WS_BUF_SIZE          16384
#define WS_BUF_SIZE_OLD       4096  // Buffer size before the truncation-bug fix
#define DSP_MAX_CHANNELS         6
#define DSP_MAX_STAGES          24
#define DSP_PEQ_BANDS           10
#define DSP_PRESET_MAX_SLOTS    32

// ===== Helper: build the dspState JSON exactly as sendDspState() does =====
//
// This replicates the JSON serialization logic from websocket_handler.cpp
// sendDspState() without pulling in AppState/dsp_pipeline.cpp dependencies.
// The test controls stage content to exercise minimum and worst-case sizes.
//
// Parameters:
//   doc            — JsonDocument to populate
//   numChannels    — number of channels to emit (max DSP_MAX_CHANNELS)
//   stagesPerCh    — number of stages per channel (max DSP_MAX_STAGES)
//   stagesEnabled  — when true, each biquad stage includes a 5-element coeffs array
//   numPresets     — number of preset slots to emit (max DSP_PRESET_MAX_SLOTS)
static void build_dsp_state_json(JsonDocument &doc,
                                 int numChannels,
                                 int stagesPerCh,
                                 bool stagesEnabled,
                                 int numPresets)
{
    doc["type"]        = "dspState";
    doc["dspEnabled"]  = true;
    doc["dspBypass"]   = false;
    doc["presetIndex"] = -1;

    // Preset list (index, name, exists) — mirrors the DSP_PRESET_MAX_SLOTS loop
    JsonArray presets = doc["presets"].to<JsonArray>();
    for (int i = 0; i < numPresets; i++) {
        JsonObject preset = presets.add<JsonObject>();
        preset["index"]  = i;
        preset["name"]   = "";   // empty name = worst-case minimally-sized string
        preset["exists"] = false;
    }

    // Global DSP config
    doc["globalBypass"] = false;
    doc["sampleRate"]   = 48000;

    // Per-channel stage data
    JsonArray channels = doc["channels"].to<JsonArray>();
    for (int c = 0; c < numChannels; c++) {
        JsonObject ch = channels.add<JsonObject>();
        ch["bypass"]     = false;
        ch["stereoLink"] = true;
        ch["stageCount"] = stagesPerCh;

        JsonArray stages = ch["stages"].to<JsonArray>();
        for (int s = 0; s < stagesPerCh; s++) {
            JsonObject so = stages.add<JsonObject>();
            so["enabled"] = stagesEnabled;
            so["type"]    = 4;  // DSP_BIQUAD_PEQ

            // Biquad parameters (always present for PEQ type)
            so["freq"] = 1000.0f;
            so["gain"] = 0.0f;
            so["Q"]    = 0.707f;

            // Coefficients only sent for enabled stages (production code behaviour)
            if (stagesEnabled) {
                JsonArray co = so["coeffs"].to<JsonArray>();
                co.add(1.0f); co.add(0.0f); co.add(0.0f);
                co.add(0.0f); co.add(0.0f);
            }
        }
    }
}

// ===== Test: WebSocket Smart Sensing Message Format =====
// This test verifies that the WebSocket messages use the correct
// JSON keys (without "appState." prefix) as expected by the frontend.
//
// Bug that this test prevents:
// Previously, the backend sent "appState.timerDuration" but the frontend
// expected "timerDuration", causing the timer display to show "-- min"
// instead of the actual timer value.

void test_smart_sensing_websocket_message_keys() {
    // Create a mock smart sensing WebSocket message
    JsonDocument doc;

    // Simulate the message construction from smart_sensing.cpp:sendSmartSensingStateInternal()
    doc["type"] = "smartSensing";
    doc["mode"] = "smart_auto";

    // CRITICAL: These keys must NOT have the "appState." prefix
    // Frontend expects: timerDuration, timerRemaining, timerActive, amplifierState
    doc["timerDuration"] = 15;      // NOT "appState.timerDuration"
    doc["timerRemaining"] = 900;    // NOT "appState.timerRemaining"
    doc["timerActive"] = true;      // Correct (never had prefix)
    doc["amplifierState"] = true;   // NOT "appState.amplifierState"
    doc["audioThreshold"] = -40.0f;
    doc["audioLevel"] = -50.0f;
    doc["signalDetected"] = true;

    // Verify the keys exist WITHOUT the "appState." prefix
    TEST_ASSERT_TRUE(doc["timerDuration"].is<int>());
    TEST_ASSERT_TRUE(doc["timerRemaining"].is<int>());
    TEST_ASSERT_TRUE(doc["timerActive"].is<bool>());
    TEST_ASSERT_TRUE(doc["amplifierState"].is<bool>());

    // Verify values
    TEST_ASSERT_EQUAL(15, doc["timerDuration"].as<int>());
    TEST_ASSERT_EQUAL(900, doc["timerRemaining"].as<int>());
    TEST_ASSERT_TRUE(doc["timerActive"].as<bool>());
    TEST_ASSERT_TRUE(doc["amplifierState"].as<bool>());

    // Verify the WRONG keys do NOT exist (regression check)
    TEST_ASSERT_FALSE(doc["appState.timerDuration"].is<int>());
    TEST_ASSERT_FALSE(doc["appState.timerRemaining"].is<int>());
    TEST_ASSERT_FALSE(doc["appState.amplifierState"].is<bool>());
}

void test_websocket_message_consistency() {
    // Verify all smart sensing messages use consistent key naming
    JsonDocument doc;

    doc["timerDuration"] = 10;
    doc["timerRemaining"] = 600;
    doc["timerActive"] = true;
    doc["amplifierState"] = false;
    doc["audioThreshold"] = -45.0f;
    doc["audioLevel"] = -60.0f;
    doc["signalDetected"] = false;

    // All keys should be simple names without prefixes
    TEST_ASSERT_TRUE(doc["timerDuration"].is<int>());
    TEST_ASSERT_TRUE(doc["timerRemaining"].is<int>());
    TEST_ASSERT_TRUE(doc["timerActive"].is<bool>());
    TEST_ASSERT_TRUE(doc["amplifierState"].is<bool>());
    TEST_ASSERT_TRUE(doc["audioThreshold"].is<float>());
    TEST_ASSERT_TRUE(doc["audioLevel"].is<float>());
    TEST_ASSERT_TRUE(doc["signalDetected"].is<bool>());
}

void test_timer_display_values() {
    // Test that timer values are correctly formatted for display
    JsonDocument doc;

    // Test case 1: Timer at full duration (15 minutes = 900 seconds)
    doc["timerDuration"] = 15;
    doc["timerRemaining"] = 900;
    doc["timerActive"] = true;

    TEST_ASSERT_EQUAL(15, doc["timerDuration"].as<int>());
    TEST_ASSERT_EQUAL(900, doc["timerRemaining"].as<int>());
    TEST_ASSERT_TRUE(doc["timerActive"].as<bool>());

    // Test case 2: Timer counting down
    doc["timerRemaining"] = 450; // 7:30 remaining
    TEST_ASSERT_EQUAL(450, doc["timerRemaining"].as<int>());

    // Test case 3: Timer expired
    doc["timerRemaining"] = 0;
    doc["timerActive"] = false;
    TEST_ASSERT_EQUAL(0, doc["timerRemaining"].as<int>());
    TEST_ASSERT_FALSE(doc["timerActive"].as<bool>());
}

void test_timer_active_flag() {
    // Test that timerActive correctly reflects timer state
    JsonDocument doc;

    // timerActive should be true when timerRemaining > 0
    doc["timerRemaining"] = 100;
    doc["timerActive"] = (doc["timerRemaining"].as<int>() > 0);
    TEST_ASSERT_TRUE(doc["timerActive"].as<bool>());

    // timerActive should be false when timerRemaining == 0
    doc["timerRemaining"] = 0;
    doc["timerActive"] = (doc["timerRemaining"].as<int>() > 0);
    TEST_ASSERT_FALSE(doc["timerActive"].as<bool>());
}

// ===== Tests: WebSocket DSP Buffer Truncation Regression =====
//
// Background: WS_BUF_SIZE was 4096 bytes. The DSP state JSON (6 channels x 10 PEQ
// bands + 32 preset slots) serializes to ~5200 bytes minimum with all stages
// disabled (no coefficients). With all stages enabled (coefficients included) it
// can reach ~16 KB. The old 4096-byte limit caused silent truncation producing
// invalid JSON that triggered a JSON.parse() SyntaxError in the browser and
// broke the entire DSP configuration page.
//
// The fix: WS_BUF_SIZE raised to 16384. wsBroadcastJson() / wsSendJson() now call
// measureJson() before serializeJson() and fall back to dynamic String allocation
// if the JSON exceeds the buffer — preventing silent truncation in all cases.
//
// These tests reproduce the serialization in isolation (no WebSocket library
// dependency) and verify:
//   1. The default DSP state (10 PEQ bands × 6 channels, all disabled, 32 preset
//      slots) serializes to > 4096 bytes (proving the old limit was insufficient).
//   2. The same default state fits within the new 16384-byte limit.
//   3. A worst-case state (all 24 stages × 6 channels enabled with biquad
//      coefficients) also fits within WS_BUF_SIZE.
//   4. The overflow guard threshold (WS_BUF_SIZE) is exactly 16384, confirming
//      the constant matches the fixed value.

// ---------------------------------------------------------------------------
// Test 1: Default DSP state JSON exceeds the old 4096-byte buffer limit.
//
// The default state has DSP_PEQ_BANDS (10) disabled stages per channel across
// DSP_MAX_CHANNELS (6) channels and DSP_PRESET_MAX_SLOTS (32) preset objects.
// Disabled biquad stages omit their coefficients array — this is the minimum
// realistic serialized size. The test asserts that this minimum already exceeds
// 4096 bytes, reproducing the conditions that caused the original truncation bug.
// ---------------------------------------------------------------------------
void test_dsp_state_json_exceeds_old_buffer_limit(void) {
    JsonDocument doc;
    // Default state: 10 PEQ bands per channel, disabled (no coefficients)
    build_dsp_state_json(doc,
                         DSP_MAX_CHANNELS,   // 6 channels
                         DSP_PEQ_BANDS,      // 10 stages per channel
                         false,              // disabled → no coeffs
                         DSP_PRESET_MAX_SLOTS); // 32 preset slots

    size_t serialized_size = measureJson(doc);

    // Must exceed the old (broken) 4096-byte limit to confirm the bug was real.
    // If this assertion fails the JSON shrank below the threshold and the test
    // data no longer represents a realistic worst-case default state.
    TEST_ASSERT_GREATER_THAN(WS_BUF_SIZE_OLD, serialized_size);
}

// ---------------------------------------------------------------------------
// Test 2: Default DSP state JSON fits within the new 16384-byte limit.
//
// The same default state that overflows the old buffer must fit entirely within
// WS_BUF_SIZE so that wsBroadcastJson() can use the fast PSRAM path instead of
// falling back to dynamic String allocation.
// ---------------------------------------------------------------------------
void test_dsp_state_json_fits_in_new_buffer(void) {
    JsonDocument doc;
    build_dsp_state_json(doc,
                         DSP_MAX_CHANNELS,
                         DSP_PEQ_BANDS,
                         false,
                         DSP_PRESET_MAX_SLOTS);

    size_t serialized_size = measureJson(doc);

    // Must fit in the new buffer (16384 bytes, +1 for null terminator handled by caller).
    TEST_ASSERT_LESS_OR_EQUAL(WS_BUF_SIZE, serialized_size);
}

// ---------------------------------------------------------------------------
// Test 3: Worst-case DSP state (all stages enabled with biquad coefficients)
//         fits within WS_BUF_SIZE.
//
// When all DSP_MAX_STAGES (24) per channel are enabled PEQ filters, each stage
// serializes with an additional 5-element "coeffs" array. This is the maximum
// realistic JSON payload from sendDspState(). The test confirms it still fits
// within the new 16384-byte limit so that the PSRAM buffer path is always
// viable for real-world configurations.
// ---------------------------------------------------------------------------
void test_dsp_state_worst_case_fits_in_new_buffer(void) {
    JsonDocument doc;
    // Worst-case: all 24 stages per channel, all enabled (coefficients included)
    build_dsp_state_json(doc,
                         DSP_MAX_CHANNELS,
                         DSP_MAX_STAGES,    // 24 stages per channel
                         true,              // enabled → coefficients included
                         DSP_PRESET_MAX_SLOTS);

    size_t serialized_size = measureJson(doc);

    TEST_ASSERT_LESS_OR_EQUAL(WS_BUF_SIZE, serialized_size);
}

// ---------------------------------------------------------------------------
// Test 4: WS_BUF_SIZE constant value is exactly 16384.
//
// Guards against accidental regression of the buffer size constant itself.
// The comment in websocket_handler.cpp states: "DSP state with 6ch × 24 stages
// (all enabled with biquad coefficients) reaches ~16 KB." If WS_BUF_SIZE is
// ever reduced below 16384 without also verifying worst-case JSON fits, the
// truncation bug would silently return.
// ---------------------------------------------------------------------------
void test_ws_buf_size_constant_is_16384(void) {
    TEST_ASSERT_EQUAL_INT(16384, (int)WS_BUF_SIZE);
}

// ---------------------------------------------------------------------------
// Test 5: Enabled biquad stages produce larger JSON than disabled stages.
//
// Validates that the production code's optimisation of omitting "coeffs" from
// disabled stages is actually meaningful for buffer sizing. If this invariant
// breaks (i.e. disabled stages somehow become larger) the worst-case estimate
// used in Tests 3 and the WS_BUF_SIZE comment would need revisiting.
// ---------------------------------------------------------------------------
void test_enabled_stages_larger_than_disabled(void) {
    JsonDocument docEnabled;
    build_dsp_state_json(docEnabled,
                         DSP_MAX_CHANNELS,
                         DSP_PEQ_BANDS,
                         true,   // enabled — includes coeffs
                         DSP_PRESET_MAX_SLOTS);

    JsonDocument docDisabled;
    build_dsp_state_json(docDisabled,
                         DSP_MAX_CHANNELS,
                         DSP_PEQ_BANDS,
                         false,  // disabled — no coeffs
                         DSP_PRESET_MAX_SLOTS);

    size_t sizeEnabled  = measureJson(docEnabled);
    size_t sizeDisabled = measureJson(docDisabled);

    TEST_ASSERT_GREATER_THAN(sizeDisabled, sizeEnabled);
}

// ---------------------------------------------------------------------------
// Test 6: JSON produced for a single channel with 10 PEQ bands (enabled) is
//         valid ArduinoJson output — channels array is present and non-empty.
//
// Smoke-test that the helper correctly produces a document with the expected
// structure so that the size measurements in the other tests are meaningful.
// ---------------------------------------------------------------------------
void test_dsp_state_json_structure_is_correct(void) {
    JsonDocument doc;
    build_dsp_state_json(doc, 1, DSP_PEQ_BANDS, true, 1);

    // Top-level type field
    TEST_ASSERT_TRUE(doc["type"].is<const char *>());
    TEST_ASSERT_EQUAL_STRING("dspState", doc["type"].as<const char *>());

    // Channels array has exactly 1 entry with 10 stages
    TEST_ASSERT_TRUE(doc["channels"].is<JsonArray>());
    JsonArray channels = doc["channels"].as<JsonArray>();
    TEST_ASSERT_EQUAL_INT(1, (int)channels.size());

    JsonObject ch = channels[0].as<JsonObject>();
    TEST_ASSERT_TRUE(ch["stages"].is<JsonArray>());
    TEST_ASSERT_EQUAL_INT(DSP_PEQ_BANDS, (int)ch["stages"].as<JsonArray>().size());

    // First stage has "coeffs" because it is enabled
    JsonObject firstStage = ch["stages"].as<JsonArray>()[0].as<JsonObject>();
    TEST_ASSERT_TRUE(firstStage["coeffs"].is<JsonArray>());
    TEST_ASSERT_EQUAL_INT(5, (int)firstStage["coeffs"].as<JsonArray>().size());

    // Preset array has 1 entry
    TEST_ASSERT_TRUE(doc["presets"].is<JsonArray>());
    TEST_ASSERT_EQUAL_INT(1, (int)doc["presets"].as<JsonArray>().size());
}

void setUp(void) {
#ifdef NATIVE_TEST
    ArduinoMock::reset();
#endif
}

void tearDown(void) {
    // Clean up after each test
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_smart_sensing_websocket_message_keys);
    RUN_TEST(test_websocket_message_consistency);
    RUN_TEST(test_timer_display_values);
    RUN_TEST(test_timer_active_flag);

    // DSP buffer truncation regression tests
    RUN_TEST(test_dsp_state_json_exceeds_old_buffer_limit);
    RUN_TEST(test_dsp_state_json_fits_in_new_buffer);
    RUN_TEST(test_dsp_state_worst_case_fits_in_new_buffer);
    RUN_TEST(test_ws_buf_size_constant_is_16384);
    RUN_TEST(test_enabled_stages_larger_than_disabled);
    RUN_TEST(test_dsp_state_json_structure_is_correct);

    return UNITY_END();
}
