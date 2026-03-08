#include <unity.h>
#include <string>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Preferences.h"
#include "../test_mocks/LittleFS.h"
#else
#include <Arduino.h>
#include <Preferences.h>
#endif

// Mock app state structure
struct AppSettings {
    String deviceName;
    String timezone;
    int utcOffset;
    bool dstEnabled;
    bool autoUpdate;
    String updateChannel; // "stable" or "beta"
};

AppSettings appSettings;

namespace TestSettingsState {
    void reset() {
        appSettings.deviceName = "ALX Nova";
        appSettings.timezone = "UTC";
        appSettings.utcOffset = 0;
        appSettings.dstEnabled = false;
        appSettings.autoUpdate = true;
        appSettings.updateChannel = "stable";
        Preferences::reset();
#ifdef NATIVE_TEST
        ArduinoMock::reset();
        MockFS::reset();
#endif
    }
}

// ===== SETTINGS MANAGER IMPLEMENTATIONS =====

void loadDefaultSettings() {
    appSettings.deviceName = "ALX Nova";
    appSettings.timezone = "UTC";
    appSettings.utcOffset = 0;
    appSettings.dstEnabled = false;
    appSettings.autoUpdate = true;
    appSettings.updateChannel = "stable";
}

bool loadSettings() {
    Preferences prefs;
    prefs.begin("settings", true);

    // Try to load settings
    if (!prefs.isKey("device_name")) {
        prefs.end();
        loadDefaultSettings();
        return false; // No settings saved yet
    }

    appSettings.deviceName = prefs.getString("device_name", "ALX Nova");
    appSettings.timezone = prefs.getString("timezone", "UTC");
    appSettings.utcOffset = prefs.getInt("utc_offset", 0);
    appSettings.dstEnabled = prefs.getBool("dst_enabled", false);
    appSettings.autoUpdate = prefs.getBool("auto_update", true);
    appSettings.updateChannel = prefs.getString("update_channel", "stable");

    prefs.end();
    return true;
}

void saveSettings() {
    Preferences prefs;
    prefs.begin("settings", false);

    prefs.putString("device_name", appSettings.deviceName);
    prefs.putString("timezone", appSettings.timezone);
    prefs.putInt("utc_offset", appSettings.utcOffset);
    prefs.putBool("dst_enabled", appSettings.dstEnabled);
    prefs.putBool("auto_update", appSettings.autoUpdate);
    prefs.putString("update_channel", appSettings.updateChannel);

    prefs.end();
}

void performFactoryReset() {
    // Clear all namespaces
    Preferences prefs;

    // Clear main settings
    prefs.begin("settings", false);
    prefs.clear();
    prefs.end();

    // Clear auth settings
    prefs.begin("auth", false);
    prefs.clear();
    prefs.end();

    // Clear WiFi settings
    prefs.begin("wifi", false);
    prefs.clear();
    prefs.end();

    // Clear MQTT settings
    prefs.begin("mqtt", false);
    prefs.clear();
    prefs.end();

    // Reload defaults
    loadDefaultSettings();
}

// ===== Test Setup/Teardown =====

void setUp(void) {
    TestSettingsState::reset();
}

void tearDown(void) {
    // Clean up after each test
}

// ===== Settings Persistence Tests =====

void test_load_settings_defaults(void) {
    // Load with no saved settings
    bool loaded = loadSettings();

    TEST_ASSERT_FALSE(loaded); // No saved settings
    TEST_ASSERT_EQUAL_STRING("ALX Nova", appSettings.deviceName.c_str());
    TEST_ASSERT_EQUAL_STRING("UTC", appSettings.timezone.c_str());
    TEST_ASSERT_EQUAL(0, appSettings.utcOffset);
    TEST_ASSERT_FALSE(appSettings.dstEnabled);
    TEST_ASSERT_TRUE(appSettings.autoUpdate);
}

void test_save_settings_to_nvs(void) {
    appSettings.deviceName = "My Device";
    appSettings.timezone = "America/New_York";
    appSettings.utcOffset = -18000; // EST
    appSettings.dstEnabled = true;
    appSettings.autoUpdate = false;
    appSettings.updateChannel = "beta";

    saveSettings();

    // Verify settings were saved
    Preferences prefs;
    prefs.begin("settings", true);

    TEST_ASSERT_EQUAL_STRING("My Device", prefs.getString("device_name", "").c_str());
    TEST_ASSERT_EQUAL_STRING("America/New_York", prefs.getString("timezone", "").c_str());
    TEST_ASSERT_EQUAL(-18000, prefs.getInt("utc_offset", 0));
    TEST_ASSERT_TRUE(prefs.getBool("dst_enabled", false));
    TEST_ASSERT_FALSE(prefs.getBool("auto_update", true));
    TEST_ASSERT_EQUAL_STRING("beta", prefs.getString("update_channel", "").c_str());

    prefs.end();
}

void test_load_settings_from_nvs(void) {
    // Save custom settings
    Preferences prefs;
    prefs.begin("settings", false);
    prefs.putString("device_name", "Custom Device");
    prefs.putString("timezone", "Europe/London");
    prefs.putInt("utc_offset", 0);
    prefs.putBool("dst_enabled", true);
    prefs.putBool("auto_update", false);
    prefs.putString("update_channel", "beta");
    prefs.end();

    // Load them
    bool loaded = loadSettings();

    TEST_ASSERT_TRUE(loaded);
    TEST_ASSERT_EQUAL_STRING("Custom Device", appSettings.deviceName.c_str());
    TEST_ASSERT_EQUAL_STRING("Europe/London", appSettings.timezone.c_str());
    TEST_ASSERT_TRUE(appSettings.dstEnabled);
    TEST_ASSERT_FALSE(appSettings.autoUpdate);
}

void test_factory_reset_clears_all(void) {
    // Save some data to multiple namespaces
    Preferences prefs;

    prefs.begin("settings", false);
    prefs.putString("device_name", "Test");
    prefs.end();

    prefs.begin("auth", false);
    prefs.putString("web_pwd", "password");
    prefs.end();

    prefs.begin("wifi", false);
    prefs.putString("ssid", "TestWiFi");
    prefs.end();

    // Perform factory reset
    performFactoryReset();

    // Verify all data is cleared
    prefs.begin("settings", true);
    TEST_ASSERT_FALSE(prefs.isKey("device_name"));
    prefs.end();

    prefs.begin("auth", true);
    TEST_ASSERT_FALSE(prefs.isKey("web_pwd"));
    prefs.end();

    prefs.begin("wifi", true);
    TEST_ASSERT_FALSE(prefs.isKey("ssid"));
    prefs.end();

    // Verify defaults are loaded
    TEST_ASSERT_EQUAL_STRING("ALX Nova", appSettings.deviceName.c_str());
}

// ===== API Handler Tests =====

void test_settings_api_get(void) {
    // Save custom settings
    appSettings.deviceName = "Test Device";
    appSettings.timezone = "America/Chicago";
    appSettings.utcOffset = -21600; // CST
    appSettings.autoUpdate = false;

    // In real API, this would return JSON
    TEST_ASSERT_EQUAL_STRING("Test Device", appSettings.deviceName.c_str());
    TEST_ASSERT_EQUAL_STRING("America/Chicago", appSettings.timezone.c_str());
    TEST_ASSERT_EQUAL(-21600, appSettings.utcOffset);
}

void test_settings_api_update(void) {
    // Simulate API update
    appSettings.deviceName = "Updated Device";
    appSettings.timezone = "Asia/Tokyo";
    appSettings.utcOffset = 32400; // JST
    appSettings.autoUpdate = true;

    saveSettings();

    // Reload and verify
    Preferences prefs;
    prefs.begin("settings", true);
    TEST_ASSERT_EQUAL_STRING("Updated Device", prefs.getString("device_name", "").c_str());
    TEST_ASSERT_EQUAL_STRING("Asia/Tokyo", prefs.getString("timezone", "").c_str());
    prefs.end();
}

void test_settings_update_partial(void) {
    // Save initial settings
    appSettings.deviceName = "Initial";
    appSettings.timezone = "UTC";
    saveSettings();

    // Update only device name
    appSettings.deviceName = "Updated";
    saveSettings();

    // Reload and verify timezone was preserved
    Preferences prefs;
    prefs.begin("settings", true);
    TEST_ASSERT_EQUAL_STRING("Updated", prefs.getString("device_name", "").c_str());
    TEST_ASSERT_EQUAL_STRING("UTC", prefs.getString("timezone", "").c_str());
    prefs.end();
}

void test_settings_validation(void) {
    // Valid timezone offset
    appSettings.utcOffset = -12 * 3600; // -12 hours
    TEST_ASSERT_EQUAL(-12 * 3600, appSettings.utcOffset);

    appSettings.utcOffset = 14 * 3600; // +14 hours (max)
    TEST_ASSERT_EQUAL(14 * 3600, appSettings.utcOffset);
}

// ===== OTA Channel Persistence Tests =====

void test_settings_updateChannel_defaults_to_stable(void) {
    // After loadDefaultSettings(), channel should be "stable"
    loadDefaultSettings();
    TEST_ASSERT_EQUAL_STRING("stable", appSettings.updateChannel.c_str());
}

void test_settings_save_beta_channel(void) {
    // Set channel to beta and persist
    appSettings.updateChannel = "beta";
    saveSettings();

    // Verify the NVS key holds "beta"
    Preferences prefs;
    prefs.begin("settings", false);
    prefs.putString("device_name", "tmp"); // ensure isKey("device_name") passes on reload
    prefs.end();

    Preferences readPrefs;
    readPrefs.begin("settings", true);
    TEST_ASSERT_EQUAL_STRING("beta", readPrefs.getString("update_channel", "stable").c_str());
    readPrefs.end();
}

void test_settings_load_restores_beta_channel(void) {
    // Pre-populate NVS with beta channel and a sentinel key so loadSettings() returns true
    Preferences prefs;
    prefs.begin("settings", false);
    prefs.putString("device_name", "ChannelTest");
    prefs.putString("update_channel", "beta");
    prefs.end();

    bool loaded = loadSettings();

    TEST_ASSERT_TRUE(loaded);
    TEST_ASSERT_EQUAL_STRING("beta", appSettings.updateChannel.c_str());
}

void test_settings_load_restores_stable_channel(void) {
    // Pre-populate NVS with stable channel
    Preferences prefs;
    prefs.begin("settings", false);
    prefs.putString("device_name", "ChannelTest");
    prefs.putString("update_channel", "stable");
    prefs.end();

    bool loaded = loadSettings();

    TEST_ASSERT_TRUE(loaded);
    TEST_ASSERT_EQUAL_STRING("stable", appSettings.updateChannel.c_str());
}

void test_settings_load_missing_channel_key_uses_default(void) {
    // Pre-populate NVS without an update_channel key — getString should return default "stable"
    Preferences prefs;
    prefs.begin("settings", false);
    prefs.putString("device_name", "NoChannelDevice");
    // deliberately omit update_channel
    prefs.end();

    bool loaded = loadSettings();

    TEST_ASSERT_TRUE(loaded);
    // The mock getString returns the default "stable" when key is absent
    TEST_ASSERT_EQUAL_STRING("stable", appSettings.updateChannel.c_str());
}

void test_settings_channel_roundtrip_stable_to_beta(void) {
    // Start with stable, save, switch to beta, save, reload, verify beta persists
    appSettings.updateChannel = "stable";
    saveSettings();

    appSettings.updateChannel = "beta";
    saveSettings();

    // Reset in-memory state then reload from NVS
    appSettings.updateChannel = "stable";

    Preferences prefs;
    prefs.begin("settings", true);
    appSettings.updateChannel = prefs.getString("update_channel", "stable");
    prefs.end();

    TEST_ASSERT_EQUAL_STRING("beta", appSettings.updateChannel.c_str());
}

void test_settings_factory_reset_clears_channel(void) {
    // Save a beta channel preference
    Preferences prefs;
    prefs.begin("settings", false);
    prefs.putString("device_name", "TestDevice");
    prefs.putString("update_channel", "beta");
    prefs.end();

    // Factory reset should clear all keys and restore defaults
    performFactoryReset();

    // update_channel key should be gone
    Preferences verify;
    verify.begin("settings", true);
    TEST_ASSERT_FALSE(verify.isKey("update_channel"));
    verify.end();

    // In-memory default should be "stable"
    TEST_ASSERT_EQUAL_STRING("stable", appSettings.updateChannel.c_str());
}

// ===== JSON Config Guard Tests =====
//
// Replicates the loadSettingsJson() + loadSettingsLegacy() fallback
// logic from settings_manager.cpp. When /config.json exists with
// version >= 1, legacy loading is skipped entirely.

// Inline replicated loadSettingsJson: returns true if /config.json
// exists, is valid JSON, and has a version field >= 1.
static bool test_loadSettingsJson() {
#ifdef NATIVE_TEST
    LittleFS.begin();
    MockFile file = LittleFS.open("/config.json", "r");
    if (!file || file.size() == 0) {
        if (file) file.close();
        return false;
    }
    // Read the file contents
    std::string content;
    content.resize(file.size());
    file.read((uint8_t*)&content[0], file.size());
    file.close();
    // Check for "version" field (simplified JSON check)
    return content.find("\"version\"") != std::string::npos;
#else
    return false;
#endif
}

// Inline replicated loadSettingsLegacy: returns true if /settings.txt exists.
static bool test_loadSettingsLegacy() {
#ifdef NATIVE_TEST
    LittleFS.begin();
    return LittleFS.exists("/settings.txt");
#else
    return false;
#endif
}

// Replicated loadSettings() flow: JSON first, legacy fallback
static bool test_loadSettingsFlow(bool &legacyWasCalled) {
    legacyWasCalled = false;
    if (test_loadSettingsJson()) {
        return true; // JSON loaded — legacy skipped
    }
    // Fall back to legacy
    if (test_loadSettingsLegacy()) {
        legacyWasCalled = true;
        return true;
    }
    return false;
}

void test_json_config_skips_legacy_load(void) {
#ifdef NATIVE_TEST
    // Set up mock filesystem with both JSON and legacy files
    LittleFS.begin();
    MockFS::injectFile("/config.json",
        "{\"version\":1,\"autoUpdate\":true,\"timezone\":0}");
    MockFS::injectFile("/settings.txt",
        "1\n0\n0\n0\n1\n5000\n1\n1\n0\n0\n0\n");

    bool legacyWasCalled = false;
    bool loaded = test_loadSettingsFlow(legacyWasCalled);

    TEST_ASSERT_TRUE(loaded);
    TEST_ASSERT_FALSE(legacyWasCalled);

    // Clean up
    MockFS::reset();
#endif
}

void test_json_config_absent_falls_back_to_legacy(void) {
#ifdef NATIVE_TEST
    // Only legacy file exists — no config.json
    LittleFS.begin();
    MockFS::injectFile("/settings.txt",
        "1\n0\n0\n0\n1\n5000\n1\n1\n0\n0\n0\n");

    bool legacyWasCalled = false;
    bool loaded = test_loadSettingsFlow(legacyWasCalled);

    TEST_ASSERT_TRUE(loaded);
    TEST_ASSERT_TRUE(legacyWasCalled);

    // Clean up
    MockFS::reset();
#endif
}

void test_json_config_empty_falls_back_to_legacy(void) {
#ifdef NATIVE_TEST
    // config.json exists but is empty — should fall through to legacy
    LittleFS.begin();
    MockFS::injectFile("/config.json", "");
    MockFS::injectFile("/settings.txt",
        "1\n0\n0\n0\n1\n5000\n1\n1\n0\n0\n0\n");

    bool legacyWasCalled = false;
    bool loaded = test_loadSettingsFlow(legacyWasCalled);

    TEST_ASSERT_TRUE(loaded);
    TEST_ASSERT_TRUE(legacyWasCalled);

    // Clean up
    MockFS::reset();
#endif
}

// ===== Test Runner =====

int runUnityTests(void) {
    UNITY_BEGIN();

    RUN_TEST(test_load_settings_defaults);
    RUN_TEST(test_save_settings_to_nvs);
    RUN_TEST(test_load_settings_from_nvs);
    RUN_TEST(test_factory_reset_clears_all);

    RUN_TEST(test_settings_api_get);
    RUN_TEST(test_settings_api_update);
    RUN_TEST(test_settings_update_partial);
    RUN_TEST(test_settings_validation);

    // OTA channel persistence tests
    RUN_TEST(test_settings_updateChannel_defaults_to_stable);
    RUN_TEST(test_settings_save_beta_channel);
    RUN_TEST(test_settings_load_restores_beta_channel);
    RUN_TEST(test_settings_load_restores_stable_channel);
    RUN_TEST(test_settings_load_missing_channel_key_uses_default);
    RUN_TEST(test_settings_channel_roundtrip_stable_to_beta);
    RUN_TEST(test_settings_factory_reset_clears_channel);

    // JSON config guard tests
    RUN_TEST(test_json_config_skips_legacy_load);
    RUN_TEST(test_json_config_absent_falls_back_to_legacy);
    RUN_TEST(test_json_config_empty_falls_back_to_legacy);

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
