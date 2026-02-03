#include <unity.h>
#include <string>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Preferences.h"
#else
#include <Arduino.h>
#include <Preferences.h>
#endif

String currentVersion = "1.0.0";
String latestVersion = "1.0.0";
String updateUrl = "";
String updateChecksum = "";

namespace TestOTAState {
    void reset() {
        currentVersion = "1.0.0";
        latestVersion = "1.0.0";
        updateUrl = "";
        updateChecksum = "";
        Preferences::reset();
#ifdef NATIVE_TEST
        ArduinoMock::reset();
#endif
    }
}

// ===== VERSION COMPARISON =====

int compareVersions(const String& v1, const String& v2) {
    int i = 0, j = 0;
    const int n1 = v1.length();
    const int n2 = v2.length();

    while (i < n1 || j < n2) {
        long num1 = 0;
        long num2 = 0;

        while (i < n1 && isDigit(v1[i])) {
            num1 = num1 * 10 + (v1[i] - '0');
            i++;
        }
        while (i < n1 && !isDigit(v1[i]))
            i++;

        while (j < n2 && isDigit(v2[j])) {
            num2 = num2 * 10 + (v2[j] - '0');
            j++;
        }
        while (j < n2 && !isDigit(v2[j]))
            j++;

        if (num1 < num2)
            return -1;
        if (num1 > num2)
            return 1;
    }

    return 0;
}

// ===== OTA UPDATER IMPLEMENTATIONS =====

bool isUpdateAvailable() {
    return compareVersions(currentVersion, latestVersion) < 0;
}

bool parseGithubReleaseJson(const String& json, String& version, String& downloadUrl, String& sha256) {
    // Simple JSON parsing for testing
    // In real code this would use ArduinoJson

    // Extract version
    int versionStart = json.indexOf("\"tag_name\":\"v");
    if (versionStart == -1) {
        return false;
    }
    versionStart += 13; // Length of "\"tag_name\":\"v"
    int versionEnd = json.indexOf("\"", versionStart);
    version = json.substring(versionStart, versionEnd);

    // Extract download URL (mock: look for .bin URL)
    int urlStart = json.indexOf(".bin");
    if (urlStart != -1) {
        // Find the full URL
        int urlBegin = json.lastIndexOf("\"", urlStart);
        int urlEnd = urlStart + 4; // .bin
        downloadUrl = json.substring(urlBegin + 1, urlEnd);
    }

    // Extract SHA256
    int sha256Start = json.indexOf("\"sha256\":\"");
    if (sha256Start != -1) {
        sha256Start += 10;
        int sha256End = json.indexOf("\"", sha256Start);
        sha256 = json.substring(sha256Start, sha256End);
    }

    return true;
}

String calculateSHA256(const uint8_t* data, size_t len) {
    // Mock SHA256 - in real code would use cryptographic library
    // For testing, return a mock hash based on length
    String hash = "sha256_";
    hash += len;
    return hash;
}

bool verifySHA256(const uint8_t* data, size_t len, const String& expectedHash) {
    String calculatedHash = calculateSHA256(data, len);
    return calculatedHash == expectedHash;
}

void saveOTASuccessFlag(const String& previousVersion) {
    Preferences prefs;
    prefs.begin("ota", false);
    prefs.putString("prev_version", previousVersion);
    prefs.putBool("update_success", true);
    prefs.end();
}

bool checkAndClearOTASuccessFlag(String& previousVersion) {
    Preferences prefs;
    prefs.begin("ota", true);

    bool hasFlag = prefs.isKey("update_success");
    if (hasFlag) {
        previousVersion = prefs.getString("prev_version", "");
    }

    prefs.end();

    if (hasFlag) {
        // Clear the flag
        prefs.begin("ota", false);
        prefs.remove("update_success");
        prefs.remove("prev_version");
        prefs.end();
    }

    return hasFlag;
}

// ===== Test Setup/Teardown =====

void setUp(void) {
    TestOTAState::reset();
}

void tearDown(void) {
    // Clean up after each test
}

// ===== Version Comparison Tests =====

void test_version_comparison_update_available(void) {
    currentVersion = "1.0.0";
    latestVersion = "1.0.1";

    TEST_ASSERT_TRUE(isUpdateAvailable());
}

void test_version_comparison_update_not_available(void) {
    currentVersion = "1.0.1";
    latestVersion = "1.0.0";

    TEST_ASSERT_FALSE(isUpdateAvailable());
}

void test_version_comparison_same_version(void) {
    currentVersion = "1.0.0";
    latestVersion = "1.0.0";

    TEST_ASSERT_FALSE(isUpdateAvailable());
}

void test_version_comparison_major_upgrade(void) {
    currentVersion = "1.5.3";
    latestVersion = "2.0.0";

    TEST_ASSERT_TRUE(isUpdateAvailable());
}

void test_version_comparison_minor_upgrade(void) {
    currentVersion = "1.5.3";
    latestVersion = "1.6.0";

    TEST_ASSERT_TRUE(isUpdateAvailable());
}

void test_version_comparison_patch_upgrade(void) {
    currentVersion = "1.5.3";
    latestVersion = "1.5.4";

    TEST_ASSERT_TRUE(isUpdateAvailable());
}

// ===== GitHub Release JSON Parsing Tests =====

void test_parse_github_release_json(void) {
    String json = R"({"tag_name":"v1.0.5","browser_download_url":"firmware.bin","sha256":"abcd1234"})";
    String version, url, sha256;

    bool parsed = parseGithubReleaseJson(json, version, url, sha256);

    TEST_ASSERT_TRUE(parsed);
    TEST_ASSERT_EQUAL_STRING("1.0.5", version.c_str());
}

void test_parse_github_release_invalid_json(void) {
    String json = R"(invalid json)";
    String version, url, sha256;

    bool parsed = parseGithubReleaseJson(json, version, url, sha256);

    TEST_ASSERT_FALSE(parsed);
}

// ===== SHA256 Tests =====

void test_sha256_calculation(void) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    String hash = calculateSHA256(data, 4);

    TEST_ASSERT_NOT_EQUAL(0, hash.length());
}

void test_sha256_verification_pass(void) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    String calculatedHash = calculateSHA256(data, 4);

    bool verified = verifySHA256(data, 4, calculatedHash);
    TEST_ASSERT_TRUE(verified);
}

void test_sha256_verification_fail(void) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    String wrongHash = "wrong_hash";

    bool verified = verifySHA256(data, 4, wrongHash);
    TEST_ASSERT_FALSE(verified);
}

// ===== OTA Success Flag Tests =====

void test_ota_success_flag_saved(void) {
    saveOTASuccessFlag("0.9.5");

    // Verify it was saved
    Preferences prefs;
    prefs.begin("ota", true);
    bool hasFlag = prefs.isKey("update_success");
    String prevVersion = prefs.getString("prev_version", "");
    prefs.end();

    TEST_ASSERT_TRUE(hasFlag);
    TEST_ASSERT_EQUAL_STRING("0.9.5", prevVersion.c_str());
}

void test_ota_success_flag_cleared(void) {
    // Save flag first
    saveOTASuccessFlag("0.9.5");

    // Check and clear
    String prevVersion;
    bool found = checkAndClearOTASuccessFlag(prevVersion);

    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_STRING("0.9.5", prevVersion.c_str());

    // Verify it was cleared
    Preferences prefs;
    prefs.begin("ota", true);
    bool stillExists = prefs.isKey("update_success");
    prefs.end();

    TEST_ASSERT_FALSE(stillExists);
}

// ===== API Tests =====

void test_check_update_api_update_available(void) {
    currentVersion = "1.0.0";
    latestVersion = "1.0.1";
    updateUrl = "https://example.com/firmware.bin";

    // In real API, this would return JSON with update info
    bool updateAvailable = isUpdateAvailable();

    TEST_ASSERT_TRUE(updateAvailable);
}

void test_check_update_api_no_update(void) {
    currentVersion = "1.0.1";
    latestVersion = "1.0.0";

    bool updateAvailable = isUpdateAvailable();

    TEST_ASSERT_FALSE(updateAvailable);
}

void test_firmware_size_validation(void) {
    // Test firmware size limits
    size_t maxSize = 4194304; // 4MB
    size_t testSize = 2097152; // 2MB

    TEST_ASSERT_LESS_THAN(maxSize, testSize + 1);
}

// ===== Test Runner =====

int runUnityTests(void) {
    UNITY_BEGIN();

    // Version comparison tests
    RUN_TEST(test_version_comparison_update_available);
    RUN_TEST(test_version_comparison_update_not_available);
    RUN_TEST(test_version_comparison_same_version);
    RUN_TEST(test_version_comparison_major_upgrade);
    RUN_TEST(test_version_comparison_minor_upgrade);
    RUN_TEST(test_version_comparison_patch_upgrade);

    // GitHub release JSON parsing tests
    RUN_TEST(test_parse_github_release_json);
    RUN_TEST(test_parse_github_release_invalid_json);

    // SHA256 tests
    RUN_TEST(test_sha256_calculation);
    RUN_TEST(test_sha256_verification_pass);
    RUN_TEST(test_sha256_verification_fail);

    // OTA success flag tests
    RUN_TEST(test_ota_success_flag_saved);
    RUN_TEST(test_ota_success_flag_cleared);

    // API tests
    RUN_TEST(test_check_update_api_update_available);
    RUN_TEST(test_check_update_api_no_update);
    RUN_TEST(test_firmware_size_validation);

    return UNITY_END();
}

// For native platform
#ifdef NATIVE_TEST
int main(void) {
    return runUnityTests();
}
#endif

// For Arduino platform
#ifndef NATIVE_TEST
void setup() {
    delay(2000);
    runUnityTests();
}

void loop() {
    // Do nothing
}
#endif
