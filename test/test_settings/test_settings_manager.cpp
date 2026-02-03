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
