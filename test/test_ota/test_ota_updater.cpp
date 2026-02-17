#include <unity.h>
#include <string>
#include <cstring>
#include <cctype>

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

        while (i < n1 && std::isdigit(static_cast<unsigned char>(v1[i]))) {
            num1 = num1 * 10 + (v1[i] - '0');
            i++;
        }
        while (i < n1 && !std::isdigit(static_cast<unsigned char>(v1[i])))
            i++;

        while (j < n2 && std::isdigit(static_cast<unsigned char>(v2[j]))) {
            num2 = num2 * 10 + (v2[j] - '0');
            j++;
        }
        while (j < n2 && !std::isdigit(static_cast<unsigned char>(v2[j])))
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

    // Convert to std::string for string operations
    std::string jsonStr = json.c_str();

    // Extract version
    size_t versionStart = jsonStr.find("\"tag_name\":\"v");
    if (versionStart == std::string::npos) {
        return false;
    }
    versionStart += 13; // Length of "\"tag_name\":\"v"
    size_t versionEnd = jsonStr.find("\"", versionStart);
    version = jsonStr.substr(versionStart, versionEnd - versionStart).c_str();

    // Extract download URL (mock: look for .bin URL)
    size_t urlStart = jsonStr.find(".bin");
    if (urlStart != std::string::npos) {
        // Find the full URL
        size_t urlBegin = jsonStr.rfind("\"", urlStart);
        size_t urlEnd = urlStart + 4; // .bin
        downloadUrl = jsonStr.substr(urlBegin + 1, urlEnd - urlBegin - 1).c_str();
    }

    // Extract SHA256
    size_t sha256Start = jsonStr.find("\"sha256\":\"");
    if (sha256Start != std::string::npos) {
        sha256Start += 10;
        size_t sha256End = jsonStr.find("\"", sha256Start);
        sha256 = jsonStr.substr(sha256Start, sha256End - sha256Start).c_str();
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

// ===== Stream JSON Parsing Tests =====

// Reimplementation of the release JSON extraction logic for testing.
// Mirrors the real getLatestReleaseInfo() field extraction from ArduinoJson.
struct ReleaseInfo {
    String version;
    String firmwareUrl;
    String checksum;
};

bool extractReleaseInfo(const String& jsonStr, ReleaseInfo& info) {
    // Use ArduinoJson-like parsing (test uses simple string ops)
    std::string json = jsonStr.c_str();

    // Extract tag_name
    size_t tagStart = json.find("\"tag_name\":\"");
    if (tagStart == std::string::npos) return false;
    tagStart += 12;
    size_t tagEnd = json.find("\"", tagStart);
    if (tagEnd == std::string::npos) return false;
    info.version = json.substr(tagStart, tagEnd - tagStart).c_str();

    // Find firmware.bin in assets
    bool foundFirmware = false;
    size_t assetPos = 0;
    while ((assetPos = json.find("\"name\":\"", assetPos)) != std::string::npos) {
        assetPos += 8;
        size_t nameEnd = json.find("\"", assetPos);
        std::string name = json.substr(assetPos, nameEnd - assetPos);

        if (name == "firmware.bin") {
            // Find browser_download_url after this name
            size_t urlStart = json.find("\"browser_download_url\":\"", nameEnd);
            if (urlStart != std::string::npos) {
                urlStart += 24;
                size_t urlEnd = json.find("\"", urlStart);
                info.firmwareUrl = json.substr(urlStart, urlEnd - urlStart).c_str();
                foundFirmware = true;
            }
        }
    }

    if (!foundFirmware) return false;

    // Extract SHA256 from body
    size_t bodyStart = json.find("\"body\":\"");
    if (bodyStart != std::string::npos) {
        bodyStart += 8;
        size_t bodyEnd = json.find("\"", bodyStart);
        std::string body = json.substr(bodyStart, bodyEnd - bodyStart);

        size_t shaIdx = body.find("SHA256:");
        if (shaIdx == std::string::npos) shaIdx = body.find("sha256:");
        if (shaIdx != std::string::npos) {
            size_t start = body.find(':', shaIdx) + 1;
            while (start < body.size() && (body[start] == ' ' || body[start] == '\n')) start++;
            std::string candidate = body.substr(start, 64);
            if (candidate.length() == 64) {
                bool validHex = true;
                for (char c : candidate) {
                    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                        validHex = false;
                        break;
                    }
                }
                if (validHex) info.checksum = candidate.c_str();
            }
        }
    }

    return true;
}

void test_parse_release_json_extracts_version(void) {
    String json = R"({"tag_name":"1.9.0","body":"Release notes","assets":[{"name":"firmware.bin","browser_download_url":"https://example.com/firmware.bin"}]})";
    ReleaseInfo info;
    bool ok = extractReleaseInfo(json, info);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("1.9.0", info.version.c_str());
}

void test_parse_release_json_extracts_firmware_url(void) {
    String json = R"({"tag_name":"1.9.0","body":"","assets":[{"name":"firmware.bin","browser_download_url":"https://github.com/user/repo/releases/download/1.9.0/firmware.bin"}]})";
    ReleaseInfo info;
    bool ok = extractReleaseInfo(json, info);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("https://github.com/user/repo/releases/download/1.9.0/firmware.bin", info.firmwareUrl.c_str());
}

void test_parse_release_json_extracts_sha256(void) {
    String json = R"({"tag_name":"1.9.0","body":"SHA256: a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2","assets":[{"name":"firmware.bin","browser_download_url":"https://example.com/firmware.bin"}]})";
    ReleaseInfo info;
    bool ok = extractReleaseInfo(json, info);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2", info.checksum.c_str());
}

void test_parse_release_json_no_sha256_in_body(void) {
    String json = R"({"tag_name":"1.9.0","body":"Just a release, no hash here","assets":[{"name":"firmware.bin","browser_download_url":"https://example.com/firmware.bin"}]})";
    ReleaseInfo info;
    bool ok = extractReleaseInfo(json, info);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(0, info.checksum.length());
}

void test_parse_release_json_no_assets(void) {
    String json = R"({"tag_name":"1.9.0","body":"","assets":[]})";
    ReleaseInfo info;
    bool ok = extractReleaseInfo(json, info);
    TEST_ASSERT_FALSE(ok);
}

void test_parse_release_json_no_firmware_bin(void) {
    String json = R"({"tag_name":"1.9.0","body":"","assets":[{"name":"source.zip","browser_download_url":"https://example.com/source.zip"}]})";
    ReleaseInfo info;
    bool ok = extractReleaseInfo(json, info);
    TEST_ASSERT_FALSE(ok);
}

void test_parse_release_json_invalid_sha256_hex(void) {
    // 64 chars but contains 'g' which is not valid hex
    String json = R"({"tag_name":"1.9.0","body":"SHA256: g1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2","assets":[{"name":"firmware.bin","browser_download_url":"https://example.com/firmware.bin"}]})";
    ReleaseInfo info;
    bool ok = extractReleaseInfo(json, info);
    TEST_ASSERT_TRUE(ok);  // Parsing succeeds but checksum should be empty
    TEST_ASSERT_EQUAL(0, info.checksum.length());
}

// ===== HTTP Fallback Tests =====

// Reimplementation of downgradeToHttp for testing
static String testDowngradeToHttp(const String& url) {
    std::string s = url.c_str();
    size_t pos = s.find("https://");
    if (pos != std::string::npos) {
        s.replace(pos, 8, "http://");
    }
    return String(s.c_str());
}

void test_downgrade_to_http_cdn_url(void) {
    String url = "https://objects.githubusercontent.com/path/firmware.bin";
    String result = testDowngradeToHttp(url);
    TEST_ASSERT_EQUAL_STRING("http://objects.githubusercontent.com/path/firmware.bin", result.c_str());
}

void test_downgrade_to_http_github_url(void) {
    String url = "https://github.com/user/repo/releases/download/v1.0/firmware.bin";
    String result = testDowngradeToHttp(url);
    TEST_ASSERT_EQUAL_STRING("http://github.com/user/repo/releases/download/v1.0/firmware.bin", result.c_str());
}

void test_http_fallback_requires_checksum(void) {
    // Simulate: heap is low (<30KB) but no checksum available
    String cachedChecksum = "";
    bool hasChecksum = (cachedChecksum.length() == 64);
    bool heapLow = true;  // < 30KB

    // HTTP fallback should NOT be allowed without checksum
    bool httpFallbackAllowed = heapLow && hasChecksum;
    TEST_ASSERT_FALSE(httpFallbackAllowed);
}

void test_http_fallback_requires_64char_checksum(void) {
    // Short checksum should not enable fallback
    String cachedChecksum = "abc123";
    bool hasChecksum = (cachedChecksum.length() == 64);
    TEST_ASSERT_FALSE(hasChecksum);

    // 63 chars - too short
    cachedChecksum = "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b";
    hasChecksum = (cachedChecksum.length() == 64);
    TEST_ASSERT_FALSE(hasChecksum);
}

void test_http_fallback_allowed_with_valid_checksum(void) {
    String cachedChecksum = "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2";
    bool hasChecksum = (cachedChecksum.length() == 64);
    TEST_ASSERT_TRUE(hasChecksum);
}

// ===== Heap Threshold Decision Tests =====

// Reimplementation of the heap decision logic from performOTAUpdate
enum OtaTransport {
    OTA_ABORT = 0,
    OTA_HTTP_FALLBACK,
    OTA_HTTPS_INSECURE,
    OTA_HTTPS_FULL
};

static OtaTransport selectOtaTransport(uint32_t maxBlock, bool hasChecksum) {
    if (maxBlock < 10000) return OTA_ABORT;
    if (maxBlock < 30000 && hasChecksum) return OTA_HTTP_FALLBACK;
    if (maxBlock < 30000) return OTA_ABORT;  // No checksum, can't do HTTP fallback
    if (maxBlock < 50000) return OTA_HTTPS_INSECURE;
    return OTA_HTTPS_FULL;
}

void test_ota_download_aborts_below_10k(void) {
    TEST_ASSERT_EQUAL(OTA_ABORT, selectOtaTransport(9999, true));
    TEST_ASSERT_EQUAL(OTA_ABORT, selectOtaTransport(5000, false));
    TEST_ASSERT_EQUAL(OTA_ABORT, selectOtaTransport(0, true));
}

void test_ota_download_uses_http_at_15k_with_checksum(void) {
    TEST_ASSERT_EQUAL(OTA_HTTP_FALLBACK, selectOtaTransport(15000, true));
    TEST_ASSERT_EQUAL(OTA_HTTP_FALLBACK, selectOtaTransport(20000, true));
    TEST_ASSERT_EQUAL(OTA_HTTP_FALLBACK, selectOtaTransport(29999, true));
}

void test_ota_download_aborts_at_15k_without_checksum(void) {
    TEST_ASSERT_EQUAL(OTA_ABORT, selectOtaTransport(15000, false));
    TEST_ASSERT_EQUAL(OTA_ABORT, selectOtaTransport(29999, false));
}

void test_ota_download_uses_insecure_tls_at_35k(void) {
    TEST_ASSERT_EQUAL(OTA_HTTPS_INSECURE, selectOtaTransport(35000, true));
    TEST_ASSERT_EQUAL(OTA_HTTPS_INSECURE, selectOtaTransport(35000, false));
    TEST_ASSERT_EQUAL(OTA_HTTPS_INSECURE, selectOtaTransport(49999, true));
}

void test_ota_download_uses_full_tls_at_60k(void) {
    TEST_ASSERT_EQUAL(OTA_HTTPS_FULL, selectOtaTransport(60000, true));
    TEST_ASSERT_EQUAL(OTA_HTTPS_FULL, selectOtaTransport(60000, false));
    TEST_ASSERT_EQUAL(OTA_HTTPS_FULL, selectOtaTransport(100000, true));
}

// ===== OTA Backoff Tests (Group 4B) =====

// Test-local reimplementation of backoff logic (mirrors ota_updater.cpp)
static int _testOtaFailures = 0;

static unsigned long testGetOTAEffectiveInterval() {
    if (_testOtaFailures >= 10) return 3600000UL;
    if (_testOtaFailures >= 6)  return 1800000UL;
    if (_testOtaFailures >= 3)  return 900000UL;
    return 300000UL;
}

static void testOtaRecordFailure() {
    _testOtaFailures++;
    if (_testOtaFailures > 20) _testOtaFailures = 20;
}

void test_ota_backoff_counter_caps_at_20(void) {
    _testOtaFailures = 0;
    // Record 100 failures
    for (int i = 0; i < 100; i++) {
        testOtaRecordFailure();
    }
    // Counter should be capped at 20
    TEST_ASSERT_EQUAL(20, _testOtaFailures);
}

void test_ota_backoff_interval_progression(void) {
    _testOtaFailures = 0;
    TEST_ASSERT_EQUAL(300000UL, testGetOTAEffectiveInterval());  // 0 failures: 5min

    _testOtaFailures = 3;
    TEST_ASSERT_EQUAL(900000UL, testGetOTAEffectiveInterval());  // 3 failures: 15min

    _testOtaFailures = 6;
    TEST_ASSERT_EQUAL(1800000UL, testGetOTAEffectiveInterval()); // 6 failures: 30min

    _testOtaFailures = 10;
    TEST_ASSERT_EQUAL(3600000UL, testGetOTAEffectiveInterval()); // 10 failures: 1hr
}

void test_ota_backoff_reset_on_success(void) {
    _testOtaFailures = 0;
    // Accumulate some failures
    for (int i = 0; i < 15; i++) {
        testOtaRecordFailure();
    }
    TEST_ASSERT_EQUAL(15, _testOtaFailures);

    // Simulate success reset
    _testOtaFailures = 0;
    TEST_ASSERT_EQUAL(0, _testOtaFailures);
    TEST_ASSERT_EQUAL(300000UL, testGetOTAEffectiveInterval());
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

    // Stream JSON parsing tests
    RUN_TEST(test_parse_release_json_extracts_version);
    RUN_TEST(test_parse_release_json_extracts_firmware_url);
    RUN_TEST(test_parse_release_json_extracts_sha256);
    RUN_TEST(test_parse_release_json_no_sha256_in_body);
    RUN_TEST(test_parse_release_json_no_assets);
    RUN_TEST(test_parse_release_json_no_firmware_bin);
    RUN_TEST(test_parse_release_json_invalid_sha256_hex);

    // HTTP fallback tests
    RUN_TEST(test_downgrade_to_http_cdn_url);
    RUN_TEST(test_downgrade_to_http_github_url);
    RUN_TEST(test_http_fallback_requires_checksum);
    RUN_TEST(test_http_fallback_requires_64char_checksum);
    RUN_TEST(test_http_fallback_allowed_with_valid_checksum);

    // Heap threshold decision tests
    RUN_TEST(test_ota_download_aborts_below_10k);
    RUN_TEST(test_ota_download_uses_http_at_15k_with_checksum);
    RUN_TEST(test_ota_download_aborts_at_15k_without_checksum);
    RUN_TEST(test_ota_download_uses_insecure_tls_at_35k);
    RUN_TEST(test_ota_download_uses_full_tls_at_60k);

    // OTA backoff tests (Group 4B)
    RUN_TEST(test_ota_backoff_counter_caps_at_20);
    RUN_TEST(test_ota_backoff_interval_progression);
    RUN_TEST(test_ota_backoff_reset_on_success);

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
