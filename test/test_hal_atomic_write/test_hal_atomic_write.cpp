// test_hal_atomic_write.cpp
// Tests for atomic write protection (tmp+rename) of /hal_config.json.
//
// Strategy: exercise the LittleFS mock directly against the constants and
// logic extracted from hal_device_db.cpp and hal_settings.cpp.  We do NOT
// inline the full HAL device manager here — the atomic-write contract only
// depends on LittleFS operations, so we validate those in isolation using
// the same MockFS that the production code uses.

#include <unity.h>
#include <cstring>
#include <string>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

#include "../test_mocks/LittleFS.h"

// Pull in the path constants without compiling the full hal_device_db.cpp
#define DAC_ENABLED
#include "../../src/hal/hal_device_db.h"

// ---------------------------------------------------------------------------
// Helpers that mirror the production atomic-write pattern
// ---------------------------------------------------------------------------

// Simulates hal_save_device_config / hal_save_all_configs atomic write.
// Writes payload to tmp path then renames to final path.
// Returns false if the tmp open fails (original file left untouched).
static bool atomic_write(const char* finalPath, const char* tmpPath,
                         const std::string& payload) {
    MockFile f = LittleFS.open(tmpPath, "w");
    if (!f) return false;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(payload.data());
    f.write(data, payload.size());
    f.close();
    LittleFS.rename(tmpPath, finalPath);
    return true;
}

// Simulates the boot-recovery block at the top of hal_load_device_configs().
static void boot_recovery(const char* finalPath, const char* tmpPath) {
    if (LittleFS.exists(tmpPath) && !LittleFS.exists(finalPath)) {
        LittleFS.rename(tmpPath, finalPath);
    }
    if (LittleFS.exists(tmpPath)) {
        LittleFS.remove(tmpPath);
    }
}

// ---------------------------------------------------------------------------
// setUp / tearDown
// ---------------------------------------------------------------------------

void setUp(void) {
    MockFS::reset();
    LittleFS.begin();
}

void tearDown(void) {
    MockFS::reset();
}

// ---------------------------------------------------------------------------
// Test 1: atomic_write creates the tmp file before rename
// ---------------------------------------------------------------------------
// We intercept mid-write by manually inspecting the mock after open-but-before
// rename — this is best verified by checking that tmp is absent after a
// successful write (i.e. rename consumed it) and the final file exists.
void test_atomic_write_creates_tmp_file(void) {
    // Manually open tmp as production code does, write, do NOT rename yet
    MockFile f = LittleFS.open(HAL_CONFIG_TMP_PATH, "w");
    TEST_ASSERT_TRUE_MESSAGE((bool)f, "tmp file open should succeed");

    const uint8_t payload[] = "{\"devices\":[]}";
    f.write(payload, sizeof(payload) - 1);
    f.close();

    // At this point only the tmp file should exist
    TEST_ASSERT_TRUE_MESSAGE(LittleFS.exists(HAL_CONFIG_TMP_PATH),
                             "tmp file should exist after write");
    TEST_ASSERT_FALSE_MESSAGE(LittleFS.exists(HAL_CONFIG_FILE_PATH),
                              "final file should not exist yet");
}

// ---------------------------------------------------------------------------
// Test 2: atomic_write renames tmp to final path
// ---------------------------------------------------------------------------
void test_atomic_write_renames_to_final(void) {
    const std::string payload = "{\"devices\":[{\"slot\":0}]}";
    bool ok = atomic_write(HAL_CONFIG_FILE_PATH, HAL_CONFIG_TMP_PATH, payload);

    TEST_ASSERT_TRUE_MESSAGE(ok, "atomic_write should return true on success");
    TEST_ASSERT_TRUE_MESSAGE(LittleFS.exists(HAL_CONFIG_FILE_PATH),
                             "final config file should exist after atomic write");
    TEST_ASSERT_FALSE_MESSAGE(LittleFS.exists(HAL_CONFIG_TMP_PATH),
                              "tmp file should be gone after rename");
    TEST_ASSERT_EQUAL_STRING(payload.c_str(),
                             MockFS::getFile(HAL_CONFIG_FILE_PATH).c_str());
}

// ---------------------------------------------------------------------------
// Test 3: boot recovery completes interrupted write (tmp exists, final absent)
// ---------------------------------------------------------------------------
void test_boot_recovery_completes_interrupted_write(void) {
    // Simulate a power-loss after writing tmp but before rename
    const std::string payload = "{\"devices\":[{\"slot\":1}]}";
    MockFS::injectFile(HAL_CONFIG_TMP_PATH, payload);

    TEST_ASSERT_TRUE_MESSAGE(LittleFS.exists(HAL_CONFIG_TMP_PATH),
                             "tmp should exist before recovery");
    TEST_ASSERT_FALSE_MESSAGE(LittleFS.exists(HAL_CONFIG_FILE_PATH),
                              "final should be absent before recovery");

    boot_recovery(HAL_CONFIG_FILE_PATH, HAL_CONFIG_TMP_PATH);

    TEST_ASSERT_TRUE_MESSAGE(LittleFS.exists(HAL_CONFIG_FILE_PATH),
                             "final config should exist after recovery");
    TEST_ASSERT_FALSE_MESSAGE(LittleFS.exists(HAL_CONFIG_TMP_PATH),
                              "tmp should be removed after recovery rename");
    TEST_ASSERT_EQUAL_STRING(payload.c_str(),
                             MockFS::getFile(HAL_CONFIG_FILE_PATH).c_str());
}

// ---------------------------------------------------------------------------
// Test 4: boot recovery cleans stale tmp when both files exist
// ---------------------------------------------------------------------------
void test_boot_recovery_cleans_stale_tmp(void) {
    // Both exist: previous atomic write succeeded but stale tmp was left
    const std::string finalPayload = "{\"devices\":[{\"slot\":2}]}";
    const std::string stalePayload = "{\"devices\":[{\"slot\":99}]}";
    MockFS::injectFile(HAL_CONFIG_FILE_PATH, finalPayload);
    MockFS::injectFile(HAL_CONFIG_TMP_PATH, stalePayload);

    boot_recovery(HAL_CONFIG_FILE_PATH, HAL_CONFIG_TMP_PATH);

    // Final must be unchanged; tmp must be gone
    TEST_ASSERT_TRUE_MESSAGE(LittleFS.exists(HAL_CONFIG_FILE_PATH),
                             "final config should still exist");
    TEST_ASSERT_FALSE_MESSAGE(LittleFS.exists(HAL_CONFIG_TMP_PATH),
                              "stale tmp should have been removed");
    TEST_ASSERT_EQUAL_STRING(finalPayload.c_str(),
                             MockFS::getFile(HAL_CONFIG_FILE_PATH).c_str());
}

// ---------------------------------------------------------------------------
// Test 5: failed tmp open leaves original file untouched
// ---------------------------------------------------------------------------
void test_atomic_write_failure_preserves_original(void) {
    // Pre-populate the final file with known-good data
    const std::string original = "{\"devices\":[{\"slot\":0,\"enabled\":true}]}";
    MockFS::injectFile(HAL_CONFIG_FILE_PATH, original);

    // Simulate open failure by unmounting the filesystem
    LittleFS.end();

    // atomic_write must return false and must not corrupt the original
    const std::string newPayload = "{\"devices\":[]}";
    bool ok = atomic_write(HAL_CONFIG_FILE_PATH, HAL_CONFIG_TMP_PATH, newPayload);

    TEST_ASSERT_FALSE_MESSAGE(ok, "atomic_write should fail when FS is unmounted");

    // Remount to inspect files
    LittleFS.begin();
    TEST_ASSERT_TRUE_MESSAGE(LittleFS.exists(HAL_CONFIG_FILE_PATH),
                             "original file should still exist");
    TEST_ASSERT_FALSE_MESSAGE(LittleFS.exists(HAL_CONFIG_TMP_PATH),
                              "tmp should not have been created");
    TEST_ASSERT_EQUAL_STRING(original.c_str(),
                             MockFS::getFile(HAL_CONFIG_FILE_PATH).c_str());
}

// ---------------------------------------------------------------------------
// Test 6: multiple consecutive writes produce correct final content
// ---------------------------------------------------------------------------
void test_atomic_write_multiple_consecutive(void) {
    for (int i = 1; i <= 3; i++) {
        std::string payload = "{\"version\":" + std::to_string(i) + "}";
        bool ok = atomic_write(HAL_CONFIG_FILE_PATH, HAL_CONFIG_TMP_PATH, payload);
        TEST_ASSERT_TRUE_MESSAGE(ok, "each write should succeed");
        TEST_ASSERT_FALSE_MESSAGE(LittleFS.exists(HAL_CONFIG_TMP_PATH),
                                  "tmp should not linger between writes");
        TEST_ASSERT_EQUAL_STRING(payload.c_str(),
                                 MockFS::getFile(HAL_CONFIG_FILE_PATH).c_str());
    }
}

// ---------------------------------------------------------------------------
// Test 7: boot recovery is a no-op when neither tmp nor final exist
// ---------------------------------------------------------------------------
void test_boot_recovery_noop_when_no_files(void) {
    TEST_ASSERT_FALSE(LittleFS.exists(HAL_CONFIG_FILE_PATH));
    TEST_ASSERT_FALSE(LittleFS.exists(HAL_CONFIG_TMP_PATH));

    boot_recovery(HAL_CONFIG_FILE_PATH, HAL_CONFIG_TMP_PATH);

    TEST_ASSERT_FALSE_MESSAGE(LittleFS.exists(HAL_CONFIG_FILE_PATH),
                              "final should remain absent");
    TEST_ASSERT_FALSE_MESSAGE(LittleFS.exists(HAL_CONFIG_TMP_PATH),
                              "tmp should remain absent");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_atomic_write_creates_tmp_file);
    RUN_TEST(test_atomic_write_renames_to_final);
    RUN_TEST(test_boot_recovery_completes_interrupted_write);
    RUN_TEST(test_boot_recovery_cleans_stale_tmp);
    RUN_TEST(test_atomic_write_failure_preserves_original);
    RUN_TEST(test_atomic_write_multiple_consecutive);
    RUN_TEST(test_boot_recovery_noop_when_no_files);
    return UNITY_END();
}
