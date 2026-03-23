// test_settings_transactional.cpp
// Tests for the atomic write and boot recovery logic in settings_manager.cpp.
//
// Since settings_manager.cpp depends on WebServer/WiFi/ESP globals that
// cannot be mocked easily, these tests replicate the exact save/load/recovery
// sequences from settings_manager.cpp against the in-memory LittleFS mock,
// validating the transactional guarantees independently of AppState wiring.

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

// ---------------------------------------------------------------------------
// Constants — mirror settings_manager.cpp paths
// ---------------------------------------------------------------------------
static const char* CONFIG_PATH     = "/config.json";
static const char* CONFIG_TMP_PATH = "/config.json.tmp";

// ---------------------------------------------------------------------------
// Helpers — replicate the atomic write / recovery logic from
// settings_manager.cpp so we test the same algorithm against the mock FS.
// ---------------------------------------------------------------------------

// Mirrors saveSettings() atomic write sequence (lines 434-445 of
// settings_manager.cpp): write to .tmp, remove old, rename .tmp to final.
static bool atomicSaveJson(const JsonDocument& doc) {
    std::string json;
    serializeJson(doc, json);

    File file = LittleFS.open(CONFIG_TMP_PATH, "w");
    if (!file) return false;
    file.write((const uint8_t*)json.c_str(), json.size());
    file.close();

    LittleFS.remove(CONFIG_PATH);
    LittleFS.rename(CONFIG_TMP_PATH, CONFIG_PATH);
    return true;
}

// Mirrors the recovery preamble at top of loadSettings() (lines 339-343 of
// settings_manager.cpp): if .tmp exists without the final file, rename it.
static void bootRecovery() {
    if (LittleFS.exists(CONFIG_TMP_PATH) && !LittleFS.exists(CONFIG_PATH)) {
        LittleFS.rename(CONFIG_TMP_PATH, CONFIG_PATH);
    }
}

// Read CONFIG_PATH back from MockFS and parse into a JsonDocument.
// Returns true on success.
static bool readConfigJson(JsonDocument& out) {
    File file = LittleFS.open(CONFIG_PATH, "r");
    if (!file || file.size() == 0) {
        if (file) file.close();
        return false;
    }
    // Read raw bytes via MockFS for reliable deserialization
    file.close();
    std::string content = MockFS::getFile(CONFIG_PATH);
    if (content.empty()) return false;

    DeserializationError err = deserializeJson(out, content);
    return !err;
}

// Build a representative settings document (subset of what saveSettings()
// produces) with controllable field values for verification.
static void buildSettingsDoc(JsonDocument& doc,
                             int version,
                             bool darkMode,
                             int timezone,
                             bool autoUpdate,
                             const char* hostname) {
    doc["version"]    = version;
    doc["darkMode"]   = darkMode;
    doc["timezone"]   = timezone;
    doc["autoUpdate"] = autoUpdate;
    doc["hostname"]   = hostname;
}

// ---------------------------------------------------------------------------
// setUp / tearDown
// ---------------------------------------------------------------------------

void setUp(void) {
    MockFS::reset();
    LittleFS.begin();
}

void tearDown(void) {
    // nothing extra needed — MockFS::reset() in setUp clears state
}

// ===================================================================
// 1. Atomic write: creates .tmp then renames to final path
// ===================================================================

void test_atomic_write_creates_tmp_then_renames(void) {
    // Arrange
    JsonDocument doc;
    buildSettingsDoc(doc, 1, true, 3600, true, "nova-test");

    // Serialize to string so we can do the steps manually and inspect
    // intermediate state.
    std::string json;
    serializeJson(doc, json);

    // Act — step 1: write to .tmp
    File file = LittleFS.open(CONFIG_TMP_PATH, "w");
    TEST_ASSERT_TRUE_MESSAGE(file, "Failed to open .tmp for writing");
    file.write((const uint8_t*)json.c_str(), json.size());
    file.close();

    // Assert — .tmp exists, final does NOT yet
    TEST_ASSERT_TRUE_MESSAGE(LittleFS.exists(CONFIG_TMP_PATH),
                             ".tmp should exist after write");
    TEST_ASSERT_FALSE_MESSAGE(LittleFS.exists(CONFIG_PATH),
                              "Final config should not exist yet");

    // Act — step 2: remove old (no-op here) + rename
    LittleFS.remove(CONFIG_PATH);
    LittleFS.rename(CONFIG_TMP_PATH, CONFIG_PATH);

    // Assert — final exists, .tmp gone
    TEST_ASSERT_TRUE_MESSAGE(LittleFS.exists(CONFIG_PATH),
                             "Final config should exist after rename");
    TEST_ASSERT_FALSE_MESSAGE(LittleFS.exists(CONFIG_TMP_PATH),
                              ".tmp should be gone after rename");

    // Verify content
    std::string raw = MockFS::getFile(CONFIG_PATH);
    TEST_ASSERT_TRUE(raw.length() > 0);

    JsonDocument verify;
    DeserializationError err = deserializeJson(verify, raw.c_str());
    TEST_ASSERT_FALSE_MESSAGE(err, "Saved JSON should parse cleanly");
    TEST_ASSERT_EQUAL_INT(1, verify["version"].as<int>());
    TEST_ASSERT_TRUE(verify["darkMode"].as<bool>());
    TEST_ASSERT_EQUAL_INT(3600, verify["timezone"].as<int>());
    TEST_ASSERT_EQUAL_STRING("nova-test", verify["hostname"].as<const char*>());
}

// Verify that when an old config.json already exists, atomic save replaces it
// cleanly without leaving stale data.
void test_atomic_write_replaces_existing_config(void) {
    // Arrange — pre-populate an existing config
    JsonDocument oldDoc;
    buildSettingsDoc(oldDoc, 1, false, 0, false, "old-host");
    atomicSaveJson(oldDoc);
    TEST_ASSERT_TRUE(LittleFS.exists(CONFIG_PATH));

    // Act — save a new config with different values
    JsonDocument newDoc;
    buildSettingsDoc(newDoc, 1, true, 7200, true, "new-host");
    bool ok = atomicSaveJson(newDoc);
    TEST_ASSERT_TRUE(ok);

    // Assert — final file contains the NEW values
    JsonDocument verify;
    TEST_ASSERT_TRUE(readConfigJson(verify));
    TEST_ASSERT_TRUE(verify["darkMode"].as<bool>());
    TEST_ASSERT_EQUAL_INT(7200, verify["timezone"].as<int>());
    TEST_ASSERT_EQUAL_STRING("new-host", verify["hostname"].as<const char*>());

    // .tmp must not linger
    TEST_ASSERT_FALSE(LittleFS.exists(CONFIG_TMP_PATH));
}

// ===================================================================
// 2. Boot recovery: orphaned .tmp -> rename to final
// ===================================================================

void test_boot_recovery_from_orphaned_tmp(void) {
    // Arrange — simulate a crash after .tmp was written but before rename.
    // Only the .tmp file exists; the final config.json does not.
    JsonDocument doc;
    buildSettingsDoc(doc, 1, true, -3600, false, "recovered");
    std::string json;
    serializeJson(doc, json);
    MockFS::injectFile(CONFIG_TMP_PATH, json);

    TEST_ASSERT_TRUE(LittleFS.exists(CONFIG_TMP_PATH));
    TEST_ASSERT_FALSE(LittleFS.exists(CONFIG_PATH));

    // Act
    bootRecovery();

    // Assert — .tmp renamed to final
    TEST_ASSERT_TRUE_MESSAGE(LittleFS.exists(CONFIG_PATH),
                             "Recovery should rename .tmp to config.json");
    TEST_ASSERT_FALSE_MESSAGE(LittleFS.exists(CONFIG_TMP_PATH),
                              ".tmp should be gone after recovery");

    // Verify content survived the rename
    JsonDocument verify;
    TEST_ASSERT_TRUE(readConfigJson(verify));
    TEST_ASSERT_EQUAL_INT(1, verify["version"].as<int>());
    TEST_ASSERT_TRUE(verify["darkMode"].as<bool>());
    TEST_ASSERT_EQUAL_INT(-3600, verify["timezone"].as<int>());
    TEST_ASSERT_EQUAL_STRING("recovered", verify["hostname"].as<const char*>());
}

// When BOTH .tmp and config.json exist, recovery must NOT overwrite the final
// file — this means the rename succeeded but the .tmp delete was lost (or a
// new save was in progress). The real code only acts when final is MISSING.
void test_boot_recovery_skips_when_both_exist(void) {
    // Arrange
    JsonDocument finalDoc;
    buildSettingsDoc(finalDoc, 1, false, 0, true, "final-host");
    atomicSaveJson(finalDoc);

    // Manually inject a stale .tmp alongside the final
    JsonDocument staleDoc;
    buildSettingsDoc(staleDoc, 1, true, 9999, false, "stale-host");
    std::string staleJson;
    serializeJson(staleDoc, staleJson);
    MockFS::injectFile(CONFIG_TMP_PATH, staleJson);

    TEST_ASSERT_TRUE(LittleFS.exists(CONFIG_PATH));
    TEST_ASSERT_TRUE(LittleFS.exists(CONFIG_TMP_PATH));

    // Act
    bootRecovery();

    // Assert — final file is untouched (still has original values)
    JsonDocument verify;
    TEST_ASSERT_TRUE(readConfigJson(verify));
    TEST_ASSERT_EQUAL_STRING("final-host", verify["hostname"].as<const char*>());
    TEST_ASSERT_EQUAL_INT(0, verify["timezone"].as<int>());
}

// When neither .tmp nor config.json exist, recovery is a no-op.
void test_boot_recovery_noop_when_no_files(void) {
    TEST_ASSERT_FALSE(LittleFS.exists(CONFIG_PATH));
    TEST_ASSERT_FALSE(LittleFS.exists(CONFIG_TMP_PATH));

    // Act — should not crash or create files
    bootRecovery();

    TEST_ASSERT_FALSE(LittleFS.exists(CONFIG_PATH));
    TEST_ASSERT_FALSE(LittleFS.exists(CONFIG_TMP_PATH));
}

// ===================================================================
// 3. Normal save produces valid JSON
// ===================================================================

void test_normal_save_produces_valid_json(void) {
    // Arrange
    JsonDocument doc;
    buildSettingsDoc(doc, 1, true, 3600, true, "valid-json-test");
    doc["buzzer"]    = true;
    doc["buzzerVol"] = 2;
    doc["audioRate"] = 100;

    // Act
    bool ok = atomicSaveJson(doc);
    TEST_ASSERT_TRUE(ok);

    // Assert — read raw bytes, parse, verify structure
    std::string raw = MockFS::getFile(CONFIG_PATH);
    TEST_ASSERT_TRUE(raw.length() > 2);  // At least "{}"

    JsonDocument verify;
    DeserializationError err = deserializeJson(verify, raw.c_str());
    TEST_ASSERT_FALSE_MESSAGE(err, "Saved JSON must be parseable");

    TEST_ASSERT_EQUAL_INT(1, verify["version"].as<int>());
    TEST_ASSERT_TRUE(verify["darkMode"].as<bool>());
    TEST_ASSERT_TRUE(verify["autoUpdate"].as<bool>());
    TEST_ASSERT_TRUE(verify["buzzer"].as<bool>());
    TEST_ASSERT_EQUAL_INT(2, verify["buzzerVol"].as<int>());
    TEST_ASSERT_EQUAL_INT(100, verify["audioRate"].as<int>());
    TEST_ASSERT_EQUAL_STRING("valid-json-test", verify["hostname"].as<const char*>());
}

// Verify that an empty document still produces valid (albeit minimal) JSON.
void test_save_empty_doc_produces_valid_json(void) {
    JsonDocument doc;  // no fields

    bool ok = atomicSaveJson(doc);
    TEST_ASSERT_TRUE(ok);

    std::string raw = MockFS::getFile(CONFIG_PATH);
    JsonDocument verify;
    DeserializationError err = deserializeJson(verify, raw.c_str());
    TEST_ASSERT_FALSE_MESSAGE(err, "Even an empty doc should produce valid JSON");
}

// Verify that a document with nested objects (like MQTT) round-trips cleanly,
// mirroring the real settings_manager.cpp MQTT section.
void test_save_with_nested_mqtt_section(void) {
    JsonDocument doc;
    buildSettingsDoc(doc, 1, true, 0, true, "mqtt-test");

    JsonObject mqtt = doc["mqtt"].to<JsonObject>();
    mqtt["broker"]      = "192.168.1.10";
    mqtt["port"]        = 1883;
    mqtt["user"]        = "admin";
    mqtt["pass"]        = "secret";
    mqtt["baseTopic"]   = "alx/nova";
    mqtt["haDiscovery"] = true;

    bool ok = atomicSaveJson(doc);
    TEST_ASSERT_TRUE(ok);

    JsonDocument verify;
    TEST_ASSERT_TRUE(readConfigJson(verify));
    TEST_ASSERT_TRUE(verify["mqtt"].is<JsonObject>());
    TEST_ASSERT_EQUAL_STRING("192.168.1.10",
                             verify["mqtt"]["broker"].as<const char*>());
    TEST_ASSERT_EQUAL_INT(1883, verify["mqtt"]["port"].as<int>());
    TEST_ASSERT_TRUE(verify["mqtt"]["haDiscovery"].as<bool>());
}

// ===================================================================
// 4. Save -> read -> save -> read consistency (no corruption)
// ===================================================================

void test_save_does_not_corrupt_on_reread(void) {
    // Arrange
    JsonDocument doc;
    buildSettingsDoc(doc, 1, true, -18000, false, "round-trip");
    doc["buzzer"]    = false;
    doc["buzzerVol"] = 1;
    doc["audioRate"] = 50;

    // Act — first save + read
    TEST_ASSERT_TRUE(atomicSaveJson(doc));

    JsonDocument read1;
    TEST_ASSERT_TRUE(readConfigJson(read1));

    // Act — save read1 back and read again
    TEST_ASSERT_TRUE(atomicSaveJson(read1));

    JsonDocument read2;
    TEST_ASSERT_TRUE(readConfigJson(read2));

    // Assert — both reads produce identical values
    TEST_ASSERT_EQUAL_INT(read1["version"].as<int>(),
                          read2["version"].as<int>());
    TEST_ASSERT_EQUAL(read1["darkMode"].as<bool>(),
                      read2["darkMode"].as<bool>());
    TEST_ASSERT_EQUAL_INT(read1["timezone"].as<int>(),
                          read2["timezone"].as<int>());
    TEST_ASSERT_EQUAL(read1["autoUpdate"].as<bool>(),
                      read2["autoUpdate"].as<bool>());
    TEST_ASSERT_EQUAL_STRING(read1["hostname"].as<const char*>(),
                             read2["hostname"].as<const char*>());
    TEST_ASSERT_EQUAL(read1["buzzer"].as<bool>(),
                      read2["buzzer"].as<bool>());
    TEST_ASSERT_EQUAL_INT(read1["buzzerVol"].as<int>(),
                          read2["buzzerVol"].as<int>());
    TEST_ASSERT_EQUAL_INT(read1["audioRate"].as<int>(),
                          read2["audioRate"].as<int>());
}

// Multiple sequential saves should each fully replace the previous content,
// and the final read should reflect only the last save.
void test_multiple_sequential_saves(void) {
    for (int i = 0; i < 5; i++) {
        JsonDocument doc;
        char host[32];
        snprintf(host, sizeof(host), "save-%d", i);
        buildSettingsDoc(doc, 1, (i % 2 == 0), i * 100, (i % 2 != 0), host);
        TEST_ASSERT_TRUE(atomicSaveJson(doc));
    }

    // Only the last save's values should survive
    JsonDocument verify;
    TEST_ASSERT_TRUE(readConfigJson(verify));
    TEST_ASSERT_EQUAL_STRING("save-4", verify["hostname"].as<const char*>());
    TEST_ASSERT_EQUAL_INT(400, verify["timezone"].as<int>());
    TEST_ASSERT_TRUE(verify["darkMode"].as<bool>());   // 4 % 2 == 0
    TEST_ASSERT_FALSE(verify["autoUpdate"].as<bool>()); // 4 % 2 != 0 is false

    // No .tmp remnants
    TEST_ASSERT_FALSE(LittleFS.exists(CONFIG_TMP_PATH));
}

// ===================================================================
// 5. Edge cases
// ===================================================================

// If LittleFS is not mounted, open should fail gracefully.
void test_save_fails_gracefully_when_fs_unmounted(void) {
    LittleFS.end();  // unmount

    JsonDocument doc;
    buildSettingsDoc(doc, 1, true, 0, true, "no-fs");

    bool ok = atomicSaveJson(doc);
    TEST_ASSERT_FALSE_MESSAGE(ok, "Save should fail when FS is unmounted");

    // No files should exist
    TEST_ASSERT_FALSE(LittleFS.exists(CONFIG_PATH));
    TEST_ASSERT_FALSE(LittleFS.exists(CONFIG_TMP_PATH));
}

// Read from a non-existent config should fail cleanly.
void test_read_returns_false_when_no_config(void) {
    JsonDocument doc;
    TEST_ASSERT_FALSE(readConfigJson(doc));
}

// Read from a corrupt (non-JSON) file should fail cleanly.
void test_read_returns_false_on_corrupt_json(void) {
    MockFS::injectFile(CONFIG_PATH, "this is not json {{{{");

    JsonDocument doc;
    TEST_ASSERT_FALSE_MESSAGE(readConfigJson(doc),
                              "Corrupt JSON should cause parse failure");
}

// Verify recovery works even when .tmp contains a large document.
void test_boot_recovery_with_large_document(void) {
    JsonDocument doc;
    buildSettingsDoc(doc, 1, true, 0, true, "big-doc");

    // Add an array with many entries to increase size
    JsonArray arr = doc["adcEnabled"].to<JsonArray>();
    for (int i = 0; i < 8; i++) arr.add(true);

    JsonObject mqtt = doc["mqtt"].to<JsonObject>();
    mqtt["broker"]      = "10.0.0.1";
    mqtt["port"]        = 8883;
    mqtt["user"]        = "longusername";
    mqtt["pass"]        = "longpassword123456";
    mqtt["baseTopic"]   = "home/audio/alx/nova/controller";
    mqtt["haDiscovery"] = true;

    std::string json;
    serializeJson(doc, json);
    MockFS::injectFile(CONFIG_TMP_PATH, json);

    TEST_ASSERT_TRUE(LittleFS.exists(CONFIG_TMP_PATH));
    TEST_ASSERT_FALSE(LittleFS.exists(CONFIG_PATH));

    // Act
    bootRecovery();

    // Assert
    TEST_ASSERT_TRUE(LittleFS.exists(CONFIG_PATH));
    TEST_ASSERT_FALSE(LittleFS.exists(CONFIG_TMP_PATH));

    JsonDocument verify;
    TEST_ASSERT_TRUE(readConfigJson(verify));
    TEST_ASSERT_EQUAL_STRING("big-doc", verify["hostname"].as<const char*>());
    TEST_ASSERT_EQUAL_INT(8, verify["adcEnabled"].as<JsonArrayConst>().size());
    TEST_ASSERT_EQUAL_STRING("home/audio/alx/nova/controller",
                             verify["mqtt"]["baseTopic"].as<const char*>());
}

// Verify that .tmp left behind from a previous save cycle does not leak
// into the next save (atomicSaveJson always removes it via rename).
void test_stale_tmp_cleaned_up_by_next_save(void) {
    // Arrange — leave a stale .tmp
    MockFS::injectFile(CONFIG_TMP_PATH, "{\"hostname\":\"stale\"}");

    // Act — normal save
    JsonDocument doc;
    buildSettingsDoc(doc, 1, true, 0, true, "fresh");
    TEST_ASSERT_TRUE(atomicSaveJson(doc));

    // Assert — .tmp gone, final has fresh data
    TEST_ASSERT_FALSE(LittleFS.exists(CONFIG_TMP_PATH));
    JsonDocument verify;
    TEST_ASSERT_TRUE(readConfigJson(verify));
    TEST_ASSERT_EQUAL_STRING("fresh", verify["hostname"].as<const char*>());
}

// ===================================================================
// main
// ===================================================================

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // 1. Atomic write
    RUN_TEST(test_atomic_write_creates_tmp_then_renames);
    RUN_TEST(test_atomic_write_replaces_existing_config);

    // 2. Boot recovery
    RUN_TEST(test_boot_recovery_from_orphaned_tmp);
    RUN_TEST(test_boot_recovery_skips_when_both_exist);
    RUN_TEST(test_boot_recovery_noop_when_no_files);

    // 3. Normal save produces valid JSON
    RUN_TEST(test_normal_save_produces_valid_json);
    RUN_TEST(test_save_empty_doc_produces_valid_json);
    RUN_TEST(test_save_with_nested_mqtt_section);

    // 4. Round-trip consistency
    RUN_TEST(test_save_does_not_corrupt_on_reread);
    RUN_TEST(test_multiple_sequential_saves);

    // 5. Edge cases
    RUN_TEST(test_save_fails_gracefully_when_fs_unmounted);
    RUN_TEST(test_read_returns_false_when_no_config);
    RUN_TEST(test_read_returns_false_on_corrupt_json);
    RUN_TEST(test_boot_recovery_with_large_document);
    RUN_TEST(test_stale_tmp_cleaned_up_by_next_save);

    return UNITY_END();
}
