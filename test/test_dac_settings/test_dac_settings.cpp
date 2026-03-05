// test_dac_settings.cpp
// Phase 0 unit tests for ES8311 DAC settings persistence.
//
// These tests exercise three things:
//   1. AppState default values for the ES8311 fields.
//   2. Backward-compat: a JSON that omits ES8311 fields leaves defaults intact.
//   3. Forward-compat: the JSON doc built for saving includes all three ES8311 keys.
//
// dac_load_settings() / dac_save_settings() are wrapped in #ifndef NATIVE_TEST,
// so they never compile here.  We test the same logic inline using ArduinoJson
// directly — the same library the production code uses.

#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

#include <ArduinoJson.h>

// Pull in AppState — guards inside app_state.h / config.h / app_events.h
// handle NATIVE_TEST / UNIT_TEST automatically.
#include "../../src/app_state.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Reset the singleton's ES8311 (and primary-DAC) fields to their compile-time
// defaults so each test starts clean.  The singleton cannot be destroyed, so we
// write every member directly.
static void resetDacFields() {
    AppState& as = AppState::getInstance();

    // Primary DAC fields
    as.dacEnabled    = false;
    as.dacVolume     = 80;
    as.dacMute       = false;
    as.dacDeviceId   = 0x0001;   // DAC_ID_PCM5102A
    strncpy(as.dacModelName, "PCM5102A", sizeof(as.dacModelName) - 1);
    as.dacModelName[sizeof(as.dacModelName) - 1] = '\0';
    as.dacFilterMode = 0;

    // ES8311 secondary DAC fields
    as.es8311Enabled = false;
    as.es8311Volume  = 80;
    as.es8311Mute    = false;
    as.es8311Ready   = false;

    // Dirty flags
    as.clearDacDirty();
    as.clearEs8311Dirty();
}

// ---------------------------------------------------------------------------
// setUp / tearDown
// ---------------------------------------------------------------------------

void setUp(void) {
    resetDacFields();
}

void tearDown(void) {}

// ---------------------------------------------------------------------------
// Test 1: AppState defaults for ES8311 fields
// ---------------------------------------------------------------------------

void test_es8311_settings_default(void) {
    // Arrange — setUp() already reset to defaults.
    AppState& as = AppState::getInstance();

    // Act — read the values as-is (no load has occurred).

    // Assert
    TEST_ASSERT_FALSE(as.es8311Enabled);
    TEST_ASSERT_EQUAL_UINT8(80, as.es8311Volume);
    TEST_ASSERT_FALSE(as.es8311Mute);
}

// ---------------------------------------------------------------------------
// Test 2: Loading a JSON that has no ES8311 fields preserves defaults
//
// Mirrors the guard logic in dac_load_settings():
//   if (doc["es8311Enabled"].is<bool>())  as.es8311Enabled = ...
//   if (doc["es8311Volume"].is<int>())    as.es8311Volume  = ...
//   if (doc["es8311Mute"].is<bool>())     as.es8311Mute    = ...
// When the keys are absent the is<T>() check returns false and no assignment
// is made — fields stay at their defaults.
// ---------------------------------------------------------------------------

void test_es8311_settings_missing_fields(void) {
    // Arrange — build a JSON that only has primary DAC fields (no ES8311 keys).
    const char* legacyJson =
        "{\"enabled\":true,\"volume\":75,\"mute\":false,"
        "\"deviceId\":1,\"modelName\":\"PCM5102A\",\"filterMode\":0}";

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, legacyJson);
    TEST_ASSERT_EQUAL_INT(DeserializationError::Ok, err.code());

    AppState& as = AppState::getInstance();

    // Act — apply the same guarded-assignment logic used by dac_load_settings().
    if (doc["enabled"].is<bool>())    as.dacEnabled    = doc["enabled"].as<bool>();
    if (doc["volume"].is<int>()) {
        int v = doc["volume"].as<int>();
        if (v >= 0 && v <= 100) as.dacVolume = (uint8_t)v;
    }
    if (doc["mute"].is<bool>())       as.dacMute       = doc["mute"].as<bool>();
    if (doc["deviceId"].is<int>())    as.dacDeviceId   = (uint16_t)doc["deviceId"].as<int>();
    if (doc["filterMode"].is<int>())  as.dacFilterMode = (uint8_t)doc["filterMode"].as<int>();

    // ES8311 keys deliberately absent — should NOT touch the fields.
    if (doc["es8311Enabled"].is<bool>()) as.es8311Enabled = doc["es8311Enabled"].as<bool>();
    if (doc["es8311Volume"].is<int>()) {
        int v = doc["es8311Volume"].as<int>();
        if (v >= 0 && v <= 100) as.es8311Volume = (uint8_t)v;
    }
    if (doc["es8311Mute"].is<bool>()) as.es8311Mute = doc["es8311Mute"].as<bool>();

    // Assert — primary fields were updated from JSON.
    TEST_ASSERT_TRUE(as.dacEnabled);
    TEST_ASSERT_EQUAL_UINT8(75, as.dacVolume);
    TEST_ASSERT_FALSE(as.dacMute);

    // Assert — ES8311 fields kept their compile-time defaults.
    TEST_ASSERT_FALSE(as.es8311Enabled);
    TEST_ASSERT_EQUAL_UINT8(80, as.es8311Volume);
    TEST_ASSERT_FALSE(as.es8311Mute);
}

// ---------------------------------------------------------------------------
// Test 3: The JSON document built for saving includes all three ES8311 keys
//
// Mirrors the serialization block in dac_save_settings():
//   doc["es8311Enabled"] = as.es8311Enabled;
//   doc["es8311Volume"]  = as.es8311Volume;
//   doc["es8311Mute"]    = as.es8311Mute;
// We set non-default values first so we can also verify the round-trip values.
// ---------------------------------------------------------------------------

void test_dac_settings_json_contains_es8311_fields(void) {
    // Arrange — set non-default ES8311 values so round-trip is meaningful.
    AppState& as = AppState::getInstance();
    as.es8311Enabled = true;
    as.es8311Volume  = 65;
    as.es8311Mute    = true;

    // Also set primary DAC fields to something deterministic.
    as.dacEnabled    = true;
    as.dacVolume     = 90;
    as.dacMute       = false;
    as.dacDeviceId   = 0x0001;
    strncpy(as.dacModelName, "PCM5102A", sizeof(as.dacModelName) - 1);
    as.dacModelName[sizeof(as.dacModelName) - 1] = '\0';
    as.dacFilterMode = 0;

    // Act — build the JSON doc exactly as dac_save_settings() does.
    JsonDocument doc;
    doc["enabled"]      = as.dacEnabled;
    doc["volume"]       = as.dacVolume;
    doc["mute"]         = as.dacMute;
    doc["deviceId"]     = as.dacDeviceId;
    doc["modelName"]    = as.dacModelName;
    doc["filterMode"]   = as.dacFilterMode;
    doc["es8311Enabled"] = as.es8311Enabled;
    doc["es8311Volume"]  = as.es8311Volume;
    doc["es8311Mute"]    = as.es8311Mute;

    // Assert — all three ES8311 keys are present in the document.
    TEST_ASSERT_TRUE(doc["es8311Enabled"].is<bool>());
    TEST_ASSERT_TRUE(doc["es8311Volume"].is<int>());
    TEST_ASSERT_TRUE(doc["es8311Mute"].is<bool>());

    // Assert — values round-trip correctly.
    TEST_ASSERT_TRUE(doc["es8311Enabled"].as<bool>());
    TEST_ASSERT_EQUAL_INT(65, doc["es8311Volume"].as<int>());
    TEST_ASSERT_TRUE(doc["es8311Mute"].as<bool>());

    // Assert — primary DAC fields are also present (regression guard).
    TEST_ASSERT_TRUE(doc["enabled"].is<bool>());
    TEST_ASSERT_TRUE(doc["volume"].is<int>());
    TEST_ASSERT_TRUE(doc["mute"].is<bool>());
    TEST_ASSERT_TRUE(doc["deviceId"].is<int>());
    TEST_ASSERT_TRUE(doc["modelName"].is<const char*>());
    TEST_ASSERT_TRUE(doc["filterMode"].is<int>());
}

// ---------------------------------------------------------------------------
// Bonus test: ES8311 volume out-of-range is rejected (matches load guard)
// ---------------------------------------------------------------------------

void test_es8311_volume_out_of_range_rejected(void) {
    // Arrange — JSON with volume > 100.
    const char* badJson =
        "{\"enabled\":false,\"volume\":80,\"mute\":false,"
        "\"deviceId\":1,\"modelName\":\"PCM5102A\",\"filterMode\":0,"
        "\"es8311Enabled\":false,\"es8311Volume\":255,\"es8311Mute\":false}";

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, badJson);
    TEST_ASSERT_EQUAL_INT(DeserializationError::Ok, err.code());

    AppState& as = AppState::getInstance();

    // Act — apply ES8311 volume with the same range guard used in production.
    if (doc["es8311Volume"].is<int>()) {
        int v = doc["es8311Volume"].as<int>();
        if (v >= 0 && v <= 100) as.es8311Volume = (uint8_t)v;
        // else: silently ignored, default stays
    }

    // Assert — default volume (80) is preserved because 255 > 100.
    TEST_ASSERT_EQUAL_UINT8(80, as.es8311Volume);
}

// ---------------------------------------------------------------------------
// Bonus test: JSON round-trip — save then load restores all ES8311 fields
// ---------------------------------------------------------------------------

void test_es8311_json_roundtrip(void) {
    // Arrange — write non-default values into AppState.
    AppState& as = AppState::getInstance();
    as.es8311Enabled = true;
    as.es8311Volume  = 42;
    as.es8311Mute    = true;

    // Act — serialize (save).
    JsonDocument saveDoc;
    saveDoc["es8311Enabled"] = as.es8311Enabled;
    saveDoc["es8311Volume"]  = as.es8311Volume;
    saveDoc["es8311Mute"]    = as.es8311Mute;

    char buf[128];
    serializeJson(saveDoc, buf, sizeof(buf));

    // Reset to defaults (simulates device restart).
    as.es8311Enabled = false;
    as.es8311Volume  = 80;
    as.es8311Mute    = false;

    // Act — deserialize (load).
    JsonDocument loadDoc;
    deserializeJson(loadDoc, buf);

    if (loadDoc["es8311Enabled"].is<bool>()) as.es8311Enabled = loadDoc["es8311Enabled"].as<bool>();
    if (loadDoc["es8311Volume"].is<int>()) {
        int v = loadDoc["es8311Volume"].as<int>();
        if (v >= 0 && v <= 100) as.es8311Volume = (uint8_t)v;
    }
    if (loadDoc["es8311Mute"].is<bool>()) as.es8311Mute = loadDoc["es8311Mute"].as<bool>();

    // Assert — fields restored from JSON.
    TEST_ASSERT_TRUE(as.es8311Enabled);
    TEST_ASSERT_EQUAL_UINT8(42, as.es8311Volume);
    TEST_ASSERT_TRUE(as.es8311Mute);
}

// ---------------------------------------------------------------------------
// Bonus test: ES8311 dirty flag starts clear and can be set / cleared
// ---------------------------------------------------------------------------

void test_es8311_dirty_flag_lifecycle(void) {
    AppState& as = AppState::getInstance();

    // Arrange — setUp() already calls clearEs8311Dirty().
    TEST_ASSERT_FALSE(as.isEs8311Dirty());

    // Act — mark dirty.
    as.markEs8311Dirty();
    TEST_ASSERT_TRUE(as.isEs8311Dirty());

    // Act — clear.
    as.clearEs8311Dirty();
    TEST_ASSERT_FALSE(as.isEs8311Dirty());
}

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    RUN_TEST(test_es8311_settings_default);
    RUN_TEST(test_es8311_settings_missing_fields);
    RUN_TEST(test_dac_settings_json_contains_es8311_fields);
    RUN_TEST(test_es8311_volume_out_of_range_rejected);
    RUN_TEST(test_es8311_json_roundtrip);
    RUN_TEST(test_es8311_dirty_flag_lifecycle);

    return UNITY_END();
}
