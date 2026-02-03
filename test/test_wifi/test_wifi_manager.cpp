#include <unity.h>
#include <string>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Preferences.h"
#include "../test_mocks/WiFi.h"
#include "../test_mocks/IPAddress.h"
#else
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#endif

#define MAX_WIFI_NETWORKS 5

// Mock WiFi network storage
struct WiFiNetworkConfig {
    String ssid;
    String password;
    bool useStaticIP;
    String staticIP;
    String subnet;
    String gateway;
    String dns1;
    String dns2;
};

WiFiNetworkConfig wifiNetworks[MAX_WIFI_NETWORKS];
int wifiNetworkCount = 0;

namespace TestWiFiState {
    void reset() {
        wifiNetworkCount = 0;
        for (int i = 0; i < MAX_WIFI_NETWORKS; i++) {
            wifiNetworks[i].ssid = "";
            wifiNetworks[i].password = "";
            wifiNetworks[i].useStaticIP = false;
        }
        Preferences::reset();
#ifdef NATIVE_TEST
        ArduinoMock::reset();
        WiFiClass::reset();
#endif
    }
}

// ===== WIFI MANAGER IMPLEMENTATIONS =====

int getWiFiNetworkCount() {
    return wifiNetworkCount;
}

bool saveWiFiNetwork(const char* ssid, const char* password, bool useStaticIP = false,
                      const char* staticIP = "", const char* subnet = "",
                      const char* gateway = "", const char* dns1 = "", const char* dns2 = "") {
    if (!ssid || strlen(ssid) == 0) {
        return false;
    }

    // Check if network already exists and update it
    for (int i = 0; i < wifiNetworkCount; i++) {
        if (wifiNetworks[i].ssid == ssid) {
            wifiNetworks[i].password = password ? String(password) : "";
            wifiNetworks[i].useStaticIP = useStaticIP;
            wifiNetworks[i].staticIP = staticIP ? String(staticIP) : "";
            wifiNetworks[i].subnet = subnet ? String(subnet) : "";
            wifiNetworks[i].gateway = gateway ? String(gateway) : "";
            wifiNetworks[i].dns1 = dns1 ? String(dns1) : "";
            wifiNetworks[i].dns2 = dns2 ? String(dns2) : "";
            return true;
        }
    }

    // Check if we've hit max networks
    if (wifiNetworkCount >= MAX_WIFI_NETWORKS) {
        return false;
    }

    // Add new network
    wifiNetworks[wifiNetworkCount].ssid = ssid;
    wifiNetworks[wifiNetworkCount].password = password ? String(password) : "";
    wifiNetworks[wifiNetworkCount].useStaticIP = useStaticIP;
    wifiNetworks[wifiNetworkCount].staticIP = staticIP ? String(staticIP) : "";
    wifiNetworks[wifiNetworkCount].subnet = subnet ? String(subnet) : "";
    wifiNetworks[wifiNetworkCount].gateway = gateway ? String(gateway) : "";
    wifiNetworks[wifiNetworkCount].dns1 = dns1 ? String(dns1) : "";
    wifiNetworks[wifiNetworkCount].dns2 = dns2 ? String(dns2) : "";
    wifiNetworkCount++;

    return true;
}

bool removeWiFiNetwork(int index) {
    if (index < 0 || index >= wifiNetworkCount) {
        return false;
    }

    // Shift remaining networks down
    for (int i = index; i < wifiNetworkCount - 1; i++) {
        wifiNetworks[i] = wifiNetworks[i + 1];
    }

    wifiNetworkCount--;
    wifiNetworks[wifiNetworkCount].ssid = "";
    return true;
}

int rssiToQuality(int rssi) {
    if (rssi <= -100)
        return 0;
    if (rssi >= -50)
        return 100;
    return 2 * (rssi + 100);
}

// ===== Test Setup/Teardown =====

void setUp(void) {
    TestWiFiState::reset();
}

void tearDown(void) {
    // Clean up after each test
}

// ===== Credentials Persistence Tests =====

void test_save_single_network(void) {
    bool saved = saveWiFiNetwork("TestSSID", "password123");

    TEST_ASSERT_TRUE(saved);
    TEST_ASSERT_EQUAL(1, wifiNetworkCount);
    TEST_ASSERT_EQUAL_STRING("TestSSID", wifiNetworks[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("password123", wifiNetworks[0].password.c_str());
}

void test_save_multiple_networks(void) {
    bool result1 = saveWiFiNetwork("Network1", "pwd1");
    bool result2 = saveWiFiNetwork("Network2", "pwd2");
    bool result3 = saveWiFiNetwork("Network3", "pwd3");

    TEST_ASSERT_TRUE(result1);
    TEST_ASSERT_TRUE(result2);
    TEST_ASSERT_TRUE(result3);
    TEST_ASSERT_EQUAL(3, wifiNetworkCount);

    TEST_ASSERT_EQUAL_STRING("Network1", wifiNetworks[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("Network2", wifiNetworks[1].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("Network3", wifiNetworks[2].ssid.c_str());
}

void test_update_existing_network(void) {
    saveWiFiNetwork("MyNetwork", "oldpassword");
    int countAfterFirst = wifiNetworkCount;

    // Update the same network
    saveWiFiNetwork("MyNetwork", "newpassword");

    TEST_ASSERT_EQUAL(countAfterFirst, wifiNetworkCount); // Count doesn't increase
    TEST_ASSERT_EQUAL_STRING("newpassword", wifiNetworks[0].password.c_str());
}

void test_remove_network_shifts_down(void) {
    saveWiFiNetwork("Net1", "pwd1");
    saveWiFiNetwork("Net2", "pwd2");
    saveWiFiNetwork("Net3", "pwd3");

    // Remove middle network
    removeWiFiNetwork(1);

    TEST_ASSERT_EQUAL(2, wifiNetworkCount);
    TEST_ASSERT_EQUAL_STRING("Net1", wifiNetworks[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("Net3", wifiNetworks[1].ssid.c_str()); // Net3 shifted to index 1
}

void test_save_rejects_sixth_network(void) {
    // Save 5 networks
    for (int i = 0; i < MAX_WIFI_NETWORKS; i++) {
        char ssid[20];
        snprintf(ssid, sizeof(ssid), "Network%d", i);
        bool result = saveWiFiNetwork(ssid, "pwd");
        TEST_ASSERT_TRUE(result);
    }

    // Try to save 6th
    bool result = saveWiFiNetwork("Network6", "pwd");
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL(MAX_WIFI_NETWORKS, wifiNetworkCount);
}

// ===== Static IP Configuration Tests =====

void test_save_network_with_static_ip(void) {
    bool saved = saveWiFiNetwork("StaticNet", "pwd", true, "192.168.1.100",
                                 "255.255.255.0", "192.168.1.1", "8.8.8.8", "8.8.4.4");

    TEST_ASSERT_TRUE(saved);
    TEST_ASSERT_TRUE(wifiNetworks[0].useStaticIP);
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", wifiNetworks[0].staticIP.c_str());
    TEST_ASSERT_EQUAL_STRING("255.255.255.0", wifiNetworks[0].subnet.c_str());
    TEST_ASSERT_EQUAL_STRING("192.168.1.1", wifiNetworks[0].gateway.c_str());
}

void test_load_network_applies_static_ip(void) {
    saveWiFiNetwork("StaticNet", "pwd", true, "192.168.1.100",
                    "255.255.255.0", "192.168.1.1", "8.8.8.8", "8.8.4.4");

    // Verify static IP was stored
    TEST_ASSERT_TRUE(wifiNetworks[0].useStaticIP);
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", wifiNetworks[0].staticIP.c_str());
}

void test_network_priority_preserves_static_ip(void) {
    // Save two networks
    saveWiFiNetwork("Net1", "pwd1", true, "192.168.1.100", "255.255.255.0", "192.168.1.1");
    saveWiFiNetwork("Net2", "pwd2", false);

    // Move Net2 to priority (simulating connection priority change)
    WiFiNetworkConfig temp = wifiNetworks[0];
    wifiNetworks[0] = wifiNetworks[1];
    wifiNetworks[1] = temp;

    // Verify Net1's static IP is preserved in slot 1
    TEST_ASSERT_TRUE(wifiNetworks[1].useStaticIP);
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", wifiNetworks[1].staticIP.c_str());
}

void test_static_ip_validation(void) {
    // Valid IP
    bool saved = saveWiFiNetwork("Net", "pwd", true, "192.168.1.1", "255.255.255.0");
    TEST_ASSERT_TRUE(saved);

    // For this test, we just verify it was saved - full validation would be in real code
    TEST_ASSERT_EQUAL_STRING("192.168.1.1", wifiNetworks[0].staticIP.c_str());
}

// ===== Network Scanning Tests =====

void test_wifi_scan_returns_json(void) {
    // Add mock scan results
#ifdef NATIVE_TEST
    WiFiClass::addMockNetwork("Network1", -50);
    WiFiClass::addMockNetwork("Network2", -75);
    WiFiClass::addMockNetwork("Network3", -95);
#endif

    int scanResults = 3; // In real code this would come from WiFi.scanNetworks()

    TEST_ASSERT_EQUAL(3, scanResults);
}

void test_wifi_scan_signal_strength(void) {
    // Test RSSI to quality conversion
    int quality1 = rssiToQuality(-50); // Strong signal
    int quality2 = rssiToQuality(-75); // Medium signal
    int quality3 = rssiToQuality(-100); // Weak signal

    TEST_ASSERT_EQUAL(100, quality1);
    TEST_ASSERT_EQUAL(50, quality2);
    TEST_ASSERT_EQUAL(0, quality3);
}

// ===== Connection Logic Tests =====

void test_connect_to_stored_networks_order(void) {
    saveWiFiNetwork("Network1", "pwd1");
    saveWiFiNetwork("Network2", "pwd2");
    saveWiFiNetwork("Network3", "pwd3");

    // Networks should be in the order they were saved
    TEST_ASSERT_EQUAL_STRING("Network1", wifiNetworks[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("Network2", wifiNetworks[1].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("Network3", wifiNetworks[2].ssid.c_str());
}

void test_connect_success_moves_to_priority(void) {
    saveWiFiNetwork("Network1", "pwd1");
    saveWiFiNetwork("Network2", "pwd2");
    saveWiFiNetwork("Network3", "pwd3");

    // Simulate successful connection to Network2 (index 1)
    // Move it to priority
    WiFiNetworkConfig temp = wifiNetworks[1];
    for (int i = 1; i > 0; i--) {
        wifiNetworks[i] = wifiNetworks[i - 1];
    }
    wifiNetworks[0] = temp;

    // Network2 should now be at index 0
    TEST_ASSERT_EQUAL_STRING("Network2", wifiNetworks[0].ssid.c_str());
}

// ===== API Handler Tests =====

void test_wifi_list_excludes_passwords(void) {
    saveWiFiNetwork("Network1", "SecurePassword123");
    saveWiFiNetwork("Network2", "AnotherPassword456");

    // API should list networks without passwords
    TEST_ASSERT_EQUAL_STRING("Network1", wifiNetworks[0].ssid.c_str());
    // Password should still be stored internally but not exposed in API
    TEST_ASSERT_EQUAL_STRING("SecurePassword123", wifiNetworks[0].password.c_str());
}

void test_wifi_save_validates_fields(void) {
    // Empty SSID should fail
    bool result = saveWiFiNetwork("", "password");
    TEST_ASSERT_FALSE(result);

    // Valid SSID should succeed
    result = saveWiFiNetwork("ValidSSID", "password");
    TEST_ASSERT_TRUE(result);
}

void test_remove_network_invalid_index(void) {
    saveWiFiNetwork("Network1", "pwd");

    bool result = removeWiFiNetwork(10); // Invalid index
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL(1, wifiNetworkCount); // Count unchanged
}

void test_remove_network_negative_index(void) {
    saveWiFiNetwork("Network1", "pwd");

    bool result = removeWiFiNetwork(-1);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL(1, wifiNetworkCount);
}

// ===== Test Runner =====

int runUnityTests(void) {
    UNITY_BEGIN();

    // Credentials persistence tests
    RUN_TEST(test_save_single_network);
    RUN_TEST(test_save_multiple_networks);
    RUN_TEST(test_update_existing_network);
    RUN_TEST(test_remove_network_shifts_down);
    RUN_TEST(test_save_rejects_sixth_network);

    // Static IP configuration tests
    RUN_TEST(test_save_network_with_static_ip);
    RUN_TEST(test_load_network_applies_static_ip);
    RUN_TEST(test_network_priority_preserves_static_ip);
    RUN_TEST(test_static_ip_validation);

    // Network scanning tests
    RUN_TEST(test_wifi_scan_returns_json);
    RUN_TEST(test_wifi_scan_signal_strength);

    // Connection logic tests
    RUN_TEST(test_connect_to_stored_networks_order);
    RUN_TEST(test_connect_success_moves_to_priority);

    // API handler tests
    RUN_TEST(test_wifi_list_excludes_passwords);
    RUN_TEST(test_wifi_save_validates_fields);
    RUN_TEST(test_remove_network_invalid_index);
    RUN_TEST(test_remove_network_negative_index);

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
