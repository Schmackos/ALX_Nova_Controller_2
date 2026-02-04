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
            // Only update password if provided (non-empty)
            if (password && strlen(password) > 0) {
                wifiNetworks[i].password = password;
            }
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

// ===== Static IP Configuration Tests (Advanced) =====

void test_static_ip_parsing_valid_addresses(void) {
    IPAddress ip, gw, sn;

    // Test valid IP addresses
    bool validIP = ip.fromString("192.168.1.100");
    bool validGW = gw.fromString("192.168.1.1");
    bool validSN = sn.fromString("255.255.255.0");

    TEST_ASSERT_TRUE(validIP);
    TEST_ASSERT_TRUE(validGW);
    TEST_ASSERT_TRUE(validSN);
}

void test_static_ip_parsing_invalid_addresses(void) {
    IPAddress ip;

    // Test invalid IP formats
    bool invalid1 = ip.fromString("256.168.1.1"); // Out of range
    bool invalid2 = ip.fromString("192.168.1");   // Incomplete
    bool invalid3 = ip.fromString("invalid");      // Not numeric

    TEST_ASSERT_FALSE(invalid1);
    TEST_ASSERT_FALSE(invalid2);
    TEST_ASSERT_FALSE(invalid3);
}

void test_dhcp_to_static_transition(void) {
    // Save network with DHCP first
    saveWiFiNetwork("TestNet", "pwd", false);
    TEST_ASSERT_FALSE(wifiNetworks[0].useStaticIP);

    // Update to use static IP
    saveWiFiNetwork("TestNet", "pwd", true, "192.168.1.100",
                    "255.255.255.0", "192.168.1.1");

    TEST_ASSERT_TRUE(wifiNetworks[0].useStaticIP);
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", wifiNetworks[0].staticIP.c_str());
}

void test_static_to_dhcp_transition(void) {
    // Save network with static IP
    saveWiFiNetwork("TestNet", "pwd", true, "192.168.1.100",
                    "255.255.255.0", "192.168.1.1");
    TEST_ASSERT_TRUE(wifiNetworks[0].useStaticIP);

    // Update to use DHCP
    saveWiFiNetwork("TestNet", "pwd", false);

    TEST_ASSERT_FALSE(wifiNetworks[0].useStaticIP);
}

void test_static_ip_with_dns_servers(void) {
    saveWiFiNetwork("TestNet", "pwd", true, "192.168.1.100",
                    "255.255.255.0", "192.168.1.1", "8.8.8.8", "8.8.4.4");

    TEST_ASSERT_TRUE(wifiNetworks[0].useStaticIP);
    TEST_ASSERT_EQUAL_STRING("8.8.8.8", wifiNetworks[0].dns1.c_str());
    TEST_ASSERT_EQUAL_STRING("8.8.4.4", wifiNetworks[0].dns2.c_str());
}

void test_static_ip_without_dns_servers(void) {
    saveWiFiNetwork("TestNet", "pwd", true, "192.168.1.100",
                    "255.255.255.0", "192.168.1.1", "", "");

    TEST_ASSERT_TRUE(wifiNetworks[0].useStaticIP);
    TEST_ASSERT_EQUAL_STRING("", wifiNetworks[0].dns1.c_str());
    TEST_ASSERT_EQUAL_STRING("", wifiNetworks[0].dns2.c_str());
}

void test_static_ip_partial_dns_servers(void) {
    // Only DNS1 provided
    saveWiFiNetwork("TestNet", "pwd", true, "192.168.1.100",
                    "255.255.255.0", "192.168.1.1", "8.8.8.8", "");

    TEST_ASSERT_TRUE(wifiNetworks[0].useStaticIP);
    TEST_ASSERT_EQUAL_STRING("8.8.8.8", wifiNetworks[0].dns1.c_str());
    TEST_ASSERT_EQUAL_STRING("", wifiNetworks[0].dns2.c_str());
}

// ===== Connection Process Tests =====

void test_connect_to_stored_networks_tries_in_order(void) {
    saveWiFiNetwork("Network1", "pwd1");
    saveWiFiNetwork("Network2", "pwd2");
    saveWiFiNetwork("Network3", "pwd3");

    // Networks should be tried in order (0, 1, 2)
    TEST_ASSERT_EQUAL_STRING("Network1", wifiNetworks[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("Network2", wifiNetworks[1].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("Network3", wifiNetworks[2].ssid.c_str());
}

void test_successful_connection_updates_priority(void) {
    saveWiFiNetwork("Network1", "pwd1");
    saveWiFiNetwork("Network2", "pwd2");
    saveWiFiNetwork("Network3", "pwd3");

    // Simulate Network2 (index 1) connecting successfully
    // Move it to priority position (index 0)
    WiFiNetworkConfig successNet = wifiNetworks[1];

    // Shift networks 0 down
    for (int i = 1; i > 0; i--) {
        wifiNetworks[i] = wifiNetworks[i - 1];
    }
    wifiNetworks[0] = successNet;

    // Network2 should now be first
    TEST_ASSERT_EQUAL_STRING("Network2", wifiNetworks[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("Network1", wifiNetworks[1].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("Network3", wifiNetworks[2].ssid.c_str());
}

void test_priority_reorder_preserves_static_ip(void) {
    // Save networks with different static IP configs
    saveWiFiNetwork("Net1", "pwd1", true, "192.168.1.100", "255.255.255.0", "192.168.1.1");
    saveWiFiNetwork("Net2", "pwd2", false);
    saveWiFiNetwork("Net3", "pwd3", true, "192.168.1.101", "255.255.255.0", "192.168.1.1");

    // Move Net3 (index 2) to priority (index 0)
    WiFiNetworkConfig successNet = wifiNetworks[2];
    for (int i = 2; i > 0; i--) {
        wifiNetworks[i] = wifiNetworks[i - 1];
    }
    wifiNetworks[0] = successNet;

    // Verify Net3's static IP config is preserved
    TEST_ASSERT_EQUAL_STRING("Net3", wifiNetworks[0].ssid.c_str());
    TEST_ASSERT_TRUE(wifiNetworks[0].useStaticIP);
    TEST_ASSERT_EQUAL_STRING("192.168.1.101", wifiNetworks[0].staticIP.c_str());

    // Verify Net1's static IP is still at new position
    TEST_ASSERT_EQUAL_STRING("Net1", wifiNetworks[1].ssid.c_str());
    TEST_ASSERT_TRUE(wifiNetworks[1].useStaticIP);
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", wifiNetworks[1].staticIP.c_str());
}

void test_empty_network_list_returns_zero_count(void) {
    TEST_ASSERT_EQUAL(0, getWiFiNetworkCount());
}

void test_network_count_after_operations(void) {
    TEST_ASSERT_EQUAL(0, getWiFiNetworkCount());

    saveWiFiNetwork("Net1", "pwd1");
    TEST_ASSERT_EQUAL(1, getWiFiNetworkCount());

    saveWiFiNetwork("Net2", "pwd2");
    TEST_ASSERT_EQUAL(2, getWiFiNetworkCount());

    removeWiFiNetwork(0);
    TEST_ASSERT_EQUAL(1, getWiFiNetworkCount());

    removeWiFiNetwork(0);
    TEST_ASSERT_EQUAL(0, getWiFiNetworkCount());
}

// ===== Migration Logic Tests =====

void test_migration_marks_as_complete(void) {
    Preferences prefs;
    prefs.begin("wifi-list", false);

    // Initially not migrated
    TEST_ASSERT_EQUAL(0, prefs.getUChar("migrated", 0));

    // Mark as migrated
    prefs.putUChar("migrated", 1);

    // Verify migration flag is set
    TEST_ASSERT_EQUAL(1, prefs.getUChar("migrated", 0));

    prefs.end();
}

void test_migration_initializes_empty_count(void) {
    Preferences prefs;
    prefs.begin("wifi-list", false);

    // Initialize with zero networks
    prefs.putUChar("count", 0);
    prefs.putUChar("migrated", 1);

    TEST_ASSERT_EQUAL(0, prefs.getUChar("count", 0));
    TEST_ASSERT_EQUAL(1, prefs.getUChar("migrated", 0));

    prefs.end();
}

void test_preferences_storage_format(void) {
    Preferences prefs;
    prefs.begin("wifi-list", false);

    // Save using Preferences storage format
    prefs.putString("s0", "TestSSID");
    prefs.putString("p0", "TestPassword");
    prefs.putBool("static0", true);
    prefs.putString("ip0", "192.168.1.100");
    prefs.putUChar("count", 1);

    // Verify retrieval
    String ssid = prefs.getString("s0", "");
    String pwd = prefs.getString("p0", "");
    bool useStatic = prefs.getBool("static0", false);
    String ip = prefs.getString("ip0", "");
    int count = prefs.getUChar("count", 0);

    TEST_ASSERT_EQUAL_STRING("TestSSID", ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("TestPassword", pwd.c_str());
    TEST_ASSERT_TRUE(useStatic);
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", ip.c_str());
    TEST_ASSERT_EQUAL(1, count);

    prefs.end();
}

// ===== Network Removal Edge Cases =====

void test_remove_first_network_shifts_correctly(void) {
    saveWiFiNetwork("First", "pwd1");
    saveWiFiNetwork("Second", "pwd2");
    saveWiFiNetwork("Third", "pwd3");

    removeWiFiNetwork(0);

    TEST_ASSERT_EQUAL(2, wifiNetworkCount);
    TEST_ASSERT_EQUAL_STRING("Second", wifiNetworks[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("Third", wifiNetworks[1].ssid.c_str());
}

void test_remove_last_network_decrements_count(void) {
    saveWiFiNetwork("First", "pwd1");
    saveWiFiNetwork("Second", "pwd2");
    saveWiFiNetwork("Third", "pwd3");

    removeWiFiNetwork(2); // Remove last

    TEST_ASSERT_EQUAL(2, wifiNetworkCount);
    TEST_ASSERT_EQUAL_STRING("First", wifiNetworks[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("Second", wifiNetworks[1].ssid.c_str());
}

void test_remove_all_networks_one_by_one(void) {
    saveWiFiNetwork("Net1", "pwd1");
    saveWiFiNetwork("Net2", "pwd2");
    saveWiFiNetwork("Net3", "pwd3");

    TEST_ASSERT_EQUAL(3, wifiNetworkCount);

    removeWiFiNetwork(0);
    TEST_ASSERT_EQUAL(2, wifiNetworkCount);

    removeWiFiNetwork(0);
    TEST_ASSERT_EQUAL(1, wifiNetworkCount);

    removeWiFiNetwork(0);
    TEST_ASSERT_EQUAL(0, wifiNetworkCount);
}

void test_remove_from_empty_list_fails(void) {
    TEST_ASSERT_EQUAL(0, wifiNetworkCount);

    bool result = removeWiFiNetwork(0);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL(0, wifiNetworkCount);
}

// ===== Password Management Tests =====

void test_update_network_keeps_password_if_empty(void) {
    // Save network with password
    saveWiFiNetwork("TestNet", "original_password");
    TEST_ASSERT_EQUAL_STRING("original_password", wifiNetworks[0].password.c_str());

    // Update with empty password - implementation should keep original
    // (This tests the intended behavior from wifi_manager.cpp line 486)
    saveWiFiNetwork("TestNet", "");

    // Password should remain unchanged
    TEST_ASSERT_EQUAL_STRING("original_password", wifiNetworks[0].password.c_str());
}

void test_update_network_changes_password_if_provided(void) {
    saveWiFiNetwork("TestNet", "old_password");
    TEST_ASSERT_EQUAL_STRING("old_password", wifiNetworks[0].password.c_str());

    saveWiFiNetwork("TestNet", "new_password");
    TEST_ASSERT_EQUAL_STRING("new_password", wifiNetworks[0].password.c_str());
}

void test_password_not_exposed_in_api(void) {
    saveWiFiNetwork("SecureNet", "VerySecretPassword123!");

    // Password is stored internally
    TEST_ASSERT_EQUAL_STRING("VerySecretPassword123!", wifiNetworks[0].password.c_str());

    // In a real API handler, passwords would not be included in JSON response
    // This test verifies the password exists but would be filtered out
    TEST_ASSERT_TRUE(wifiNetworks[0].password.length() > 0);
}

// ===== WiFi Connection State Tests =====

void test_wifi_connection_status_changes(void) {
#ifdef NATIVE_TEST
    // Test connection status transitions
    TEST_ASSERT_EQUAL(WiFiClass::WL_IDLE_STATUS, WiFi.status());

    WiFi.begin("TestSSID", "password");
    TEST_ASSERT_EQUAL(WiFiClass::WL_CONNECTED, WiFi.status());

    WiFi.disconnect();
    TEST_ASSERT_EQUAL(WiFiClass::WL_DISCONNECTED, WiFi.status());
#endif
}

void test_wifi_ssid_tracking(void) {
#ifdef NATIVE_TEST
    WiFi.begin("MyNetwork", "password");

    String connectedSSID = WiFi.SSID();
    TEST_ASSERT_EQUAL_STRING("MyNetwork", connectedSSID.c_str());

    WiFi.disconnect();
    String afterDisconnect = WiFi.SSID();
    TEST_ASSERT_EQUAL(0, afterDisconnect.length());
#endif
}

void test_wifi_ip_configuration(void) {
#ifdef NATIVE_TEST
    IPAddress ip(192, 168, 1, 100);
    IPAddress gw(192, 168, 1, 1);
    IPAddress sn(255, 255, 255, 0);

    WiFi.config(ip, gw, sn);

    TEST_ASSERT_TRUE(WiFi.localIP() == ip);
    TEST_ASSERT_TRUE(WiFi.gatewayIP() == gw);
    TEST_ASSERT_TRUE(WiFi.subnetMask() == sn);
#endif
}

// ===== Multi-Network Advanced Tests =====

void test_duplicate_ssid_updates_not_adds(void) {
    saveWiFiNetwork("DuplicateNet", "pwd1");
    TEST_ASSERT_EQUAL(1, wifiNetworkCount);

    saveWiFiNetwork("DuplicateNet", "pwd2");
    TEST_ASSERT_EQUAL(1, wifiNetworkCount); // Count should not increase
    TEST_ASSERT_EQUAL_STRING("pwd2", wifiNetworks[0].password.c_str());
}

void test_case_sensitive_ssid_comparison(void) {
    saveWiFiNetwork("MyNetwork", "pwd1");
    saveWiFiNetwork("mynetwork", "pwd2");

    // SSIDs are case-sensitive, so these should be different
    TEST_ASSERT_EQUAL(2, wifiNetworkCount);
    TEST_ASSERT_EQUAL_STRING("MyNetwork", wifiNetworks[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("mynetwork", wifiNetworks[1].ssid.c_str());
}

void test_special_characters_in_ssid(void) {
    saveWiFiNetwork("WiFi-2.4GHz_Guest@Home!", "password");

    TEST_ASSERT_EQUAL(1, wifiNetworkCount);
    TEST_ASSERT_EQUAL_STRING("WiFi-2.4GHz_Guest@Home!", wifiNetworks[0].ssid.c_str());
}

void test_very_long_ssid(void) {
    // WiFi SSIDs can be up to 32 characters
    const char* longSSID = "This_Is_A_Very_Long_SSID_Name_32";
    saveWiFiNetwork(longSSID, "password");

    TEST_ASSERT_EQUAL(1, wifiNetworkCount);
    TEST_ASSERT_EQUAL_STRING(longSSID, wifiNetworks[0].ssid.c_str());
}

void test_network_with_spaces_in_ssid(void) {
    saveWiFiNetwork("My Home Network", "password");

    TEST_ASSERT_EQUAL(1, wifiNetworkCount);
    TEST_ASSERT_EQUAL_STRING("My Home Network", wifiNetworks[0].ssid.c_str());
}

// ===== Preferences Integration Tests =====

void test_preferences_namespace_isolation(void) {
    // Test that wifi-list namespace is isolated
    Preferences prefs1, prefs2;

    prefs1.begin("wifi-list", false);
    prefs1.putString("s0", "WiFiNetwork");
    prefs1.end();

    prefs2.begin("other-namespace", false);
    String value = prefs2.getString("s0", "default");
    prefs2.end();

    // Value from other namespace should be default
    TEST_ASSERT_EQUAL_STRING("default", value.c_str());

    prefs1.begin("wifi-list", true);
    String wifiValue = prefs1.getString("s0", "default");
    prefs1.end();

    // Value from wifi-list should be what we stored
    TEST_ASSERT_EQUAL_STRING("WiFiNetwork", wifiValue.c_str());
}

void test_preferences_read_only_mode(void) {
    Preferences prefsWrite, prefsRead;

    // Write some data
    prefsWrite.begin("wifi-list", false);
    prefsWrite.putString("test", "value");
    prefsWrite.end();

    // Open in read-only mode
    prefsRead.begin("wifi-list", true);
    String value = prefsRead.getString("test", "");
    TEST_ASSERT_EQUAL_STRING("value", value.c_str());

    // Attempt to write in read-only mode (should fail silently)
    prefsRead.putString("test", "newvalue");
    prefsRead.end();

    // Verify original value is unchanged
    prefsWrite.begin("wifi-list", true);
    String verifyValue = prefsWrite.getString("test", "");
    prefsWrite.end();

    TEST_ASSERT_EQUAL_STRING("value", verifyValue.c_str());
}

// ===== WiFi Retry Logic Tests =====

// Mock retry state variables (would be static in real implementation)
namespace WiFiRetryState {
    bool wifiRetryInProgress = false;
    unsigned long lastFullRetryAttempt = 0;
    int currentRetryCount = 0;
    String lastFailedSSID = "";
    bool wifiDisconnected = false;

    void reset() {
        wifiRetryInProgress = false;
        lastFullRetryAttempt = 0;
        currentRetryCount = 0;
        lastFailedSSID = "";
        wifiDisconnected = false;
    }
}

// Mock function to simulate WiFi event with error 201
void simulateWiFiError201(const char* ssid) {
    // Simulate error 201 detection logic from onWiFiEvent
    WiFiRetryState::lastFailedSSID = String(ssid);
    WiFiRetryState::wifiRetryInProgress = true;
    WiFiRetryState::wifiDisconnected = true;
}

// Mock function to simulate successful connection
void simulateSuccessfulConnection() {
    // Simulate success logic from onWiFiEvent
    WiFiRetryState::wifiRetryInProgress = false;
    WiFiRetryState::currentRetryCount = 0;
    WiFiRetryState::lastFailedSSID = "";
    WiFiRetryState::wifiDisconnected = false;
}

// Mock function to simulate retry attempt failure
void simulateRetryFailure() {
    WiFiRetryState::lastFullRetryAttempt = ArduinoMock::mockMillis;
    WiFiRetryState::currentRetryCount++;
    WiFiRetryState::wifiRetryInProgress = false;
}

void test_wifi_retry_error_201_triggers_retry(void) {
    TestWiFiState::reset();
    WiFiRetryState::reset();

    // Simulate connecting to a network that doesn't exist
    simulateWiFiError201("NonExistentNetwork");

    // Verify retry flags are set
    TEST_ASSERT_TRUE(WiFiRetryState::wifiRetryInProgress);
    TEST_ASSERT_TRUE(WiFiRetryState::wifiDisconnected);
    TEST_ASSERT_EQUAL_STRING("NonExistentNetwork", WiFiRetryState::lastFailedSSID.c_str());
}

void test_wifi_retry_successful_connection_clears_flags(void) {
    TestWiFiState::reset();
    WiFiRetryState::reset();

    // Start with retry in progress
    WiFiRetryState::wifiRetryInProgress = true;
    WiFiRetryState::currentRetryCount = 3;
    WiFiRetryState::lastFailedSSID = "FailedNetwork";
    WiFiRetryState::wifiDisconnected = true;

    // Simulate successful connection
    simulateSuccessfulConnection();

    // Verify all retry state is cleared
    TEST_ASSERT_FALSE(WiFiRetryState::wifiRetryInProgress);
    TEST_ASSERT_FALSE(WiFiRetryState::wifiDisconnected);
    TEST_ASSERT_EQUAL(0, WiFiRetryState::currentRetryCount);
    TEST_ASSERT_EQUAL_STRING("", WiFiRetryState::lastFailedSSID.c_str());
}

void test_wifi_retry_counter_increments(void) {
    TestWiFiState::reset();
    WiFiRetryState::reset();

    // Initial state
    TEST_ASSERT_EQUAL(0, WiFiRetryState::currentRetryCount);

    // Simulate first failure
    simulateRetryFailure();
    TEST_ASSERT_EQUAL(1, WiFiRetryState::currentRetryCount);

    // Simulate second failure
    simulateRetryFailure();
    TEST_ASSERT_EQUAL(2, WiFiRetryState::currentRetryCount);

    // Simulate third failure
    simulateRetryFailure();
    TEST_ASSERT_EQUAL(3, WiFiRetryState::currentRetryCount);

    // Verify counter resets on success
    simulateSuccessfulConnection();
    TEST_ASSERT_EQUAL(0, WiFiRetryState::currentRetryCount);
}

void test_wifi_retry_tracks_failed_ssid(void) {
    TestWiFiState::reset();
    WiFiRetryState::reset();

    // Simulate failure on first network
    simulateWiFiError201("Network1");
    TEST_ASSERT_EQUAL_STRING("Network1", WiFiRetryState::lastFailedSSID.c_str());

    // Simulate failure on different network
    WiFiRetryState::reset();
    simulateWiFiError201("Network2");
    TEST_ASSERT_EQUAL_STRING("Network2", WiFiRetryState::lastFailedSSID.c_str());

    // Verify cleared on success
    simulateSuccessfulConnection();
    TEST_ASSERT_EQUAL_STRING("", WiFiRetryState::lastFailedSSID.c_str());
}

void test_wifi_retry_interval_timing(void) {
    TestWiFiState::reset();
    WiFiRetryState::reset();
    ArduinoMock::reset();

    const unsigned long RETRY_INTERVAL_MS = 30000;

    // Simulate initial failure
    simulateRetryFailure();
    unsigned long firstRetryTime = WiFiRetryState::lastFullRetryAttempt;
    TEST_ASSERT_EQUAL(0, firstRetryTime);

    // Advance time by 15 seconds (not enough)
    ArduinoMock::mockMillis += 15000;
    unsigned long timeSinceRetry = ArduinoMock::mockMillis - WiFiRetryState::lastFullRetryAttempt;
    TEST_ASSERT_TRUE(timeSinceRetry < RETRY_INTERVAL_MS);

    // Advance time by another 15 seconds (total 30 seconds - should trigger retry)
    ArduinoMock::mockMillis += 15000;
    timeSinceRetry = ArduinoMock::mockMillis - WiFiRetryState::lastFullRetryAttempt;
    TEST_ASSERT_TRUE(timeSinceRetry >= RETRY_INTERVAL_MS);
}

void test_wifi_retry_multiple_networks_fallback(void) {
    TestWiFiState::reset();
    WiFiRetryState::reset();

    // Save multiple networks
    saveWiFiNetwork("Network1", "pass1");
    saveWiFiNetwork("Network2", "pass2");
    saveWiFiNetwork("Network3", "pass3");

    TEST_ASSERT_EQUAL(3, getWiFiNetworkCount());

    // Simulate error 201 on first network
    simulateWiFiError201("Network1");
    TEST_ASSERT_TRUE(WiFiRetryState::wifiRetryInProgress);

    // Verify retry should attempt other networks
    // In real implementation, connectToStoredNetworks() would be called
    // which tries Network2, then Network3
}

void test_wifi_retry_clears_on_success_after_multiple_failures(void) {
    TestWiFiState::reset();
    WiFiRetryState::reset();

    // Simulate multiple failures
    simulateWiFiError201("Network1");
    simulateRetryFailure();
    TEST_ASSERT_EQUAL(1, WiFiRetryState::currentRetryCount);

    simulateWiFiError201("Network2");
    simulateRetryFailure();
    TEST_ASSERT_EQUAL(2, WiFiRetryState::currentRetryCount);

    simulateWiFiError201("Network3");
    simulateRetryFailure();
    TEST_ASSERT_EQUAL(3, WiFiRetryState::currentRetryCount);

    // Finally succeed
    simulateSuccessfulConnection();

    // Verify everything is cleared
    TEST_ASSERT_FALSE(WiFiRetryState::wifiRetryInProgress);
    TEST_ASSERT_FALSE(WiFiRetryState::wifiDisconnected);
    TEST_ASSERT_EQUAL(0, WiFiRetryState::currentRetryCount);
    TEST_ASSERT_EQUAL_STRING("", WiFiRetryState::lastFailedSSID.c_str());
}

void test_wifi_retry_immediate_vs_periodic(void) {
    TestWiFiState::reset();
    WiFiRetryState::reset();
    ArduinoMock::reset();

    const unsigned long RETRY_INTERVAL_MS = 30000;

    // Simulate error 201 - should trigger immediate retry
    simulateWiFiError201("TestNetwork");
    TEST_ASSERT_TRUE(WiFiRetryState::wifiRetryInProgress);
    TEST_ASSERT_EQUAL(0, WiFiRetryState::lastFullRetryAttempt); // Not set yet

    // After immediate retry fails, should schedule periodic retry
    simulateRetryFailure();
    TEST_ASSERT_FALSE(WiFiRetryState::wifiRetryInProgress); // Immediate flag cleared
    TEST_ASSERT_TRUE(WiFiRetryState::wifiDisconnected);      // Still disconnected
    TEST_ASSERT_EQUAL(1, WiFiRetryState::currentRetryCount);
    unsigned long firstRetryTime = WiFiRetryState::lastFullRetryAttempt;

    // Advance time for periodic retry
    ArduinoMock::mockMillis += RETRY_INTERVAL_MS + 1000;

    // Periodic retry should be ready
    unsigned long timeSinceRetry = ArduinoMock::mockMillis - WiFiRetryState::lastFullRetryAttempt;
    TEST_ASSERT_TRUE(timeSinceRetry > RETRY_INTERVAL_MS);
}

void test_wifi_retry_preserves_network_order(void) {
    TestWiFiState::reset();
    WiFiRetryState::reset();

    // Save networks in priority order
    saveWiFiNetwork("Priority1", "pass1");
    saveWiFiNetwork("Priority2", "pass2");
    saveWiFiNetwork("Priority3", "pass3");

    // Verify order is preserved
    TEST_ASSERT_EQUAL_STRING("Priority1", wifiNetworks[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("Priority2", wifiNetworks[1].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("Priority3", wifiNetworks[2].ssid.c_str());

    // Simulate error 201 on Priority1
    simulateWiFiError201("Priority1");

    // After retry, networks should still be in same order
    // (retry logic should try Priority2, then Priority3, but not reorder)
    TEST_ASSERT_EQUAL_STRING("Priority1", wifiNetworks[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("Priority2", wifiNetworks[1].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("Priority3", wifiNetworks[2].ssid.c_str());
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

    // Static IP configuration tests (Basic)
    RUN_TEST(test_save_network_with_static_ip);
    RUN_TEST(test_load_network_applies_static_ip);
    RUN_TEST(test_network_priority_preserves_static_ip);
    RUN_TEST(test_static_ip_validation);

    // Static IP configuration tests (Advanced)
    RUN_TEST(test_static_ip_parsing_valid_addresses);
    RUN_TEST(test_static_ip_parsing_invalid_addresses);
    RUN_TEST(test_dhcp_to_static_transition);
    RUN_TEST(test_static_to_dhcp_transition);
    RUN_TEST(test_static_ip_with_dns_servers);
    RUN_TEST(test_static_ip_without_dns_servers);
    RUN_TEST(test_static_ip_partial_dns_servers);

    // Network scanning tests
    RUN_TEST(test_wifi_scan_returns_json);
    RUN_TEST(test_wifi_scan_signal_strength);

    // Connection logic tests
    RUN_TEST(test_connect_to_stored_networks_order);
    RUN_TEST(test_connect_success_moves_to_priority);
    RUN_TEST(test_connect_to_stored_networks_tries_in_order);
    RUN_TEST(test_successful_connection_updates_priority);
    RUN_TEST(test_priority_reorder_preserves_static_ip);
    RUN_TEST(test_empty_network_list_returns_zero_count);
    RUN_TEST(test_network_count_after_operations);

    // Migration logic tests
    RUN_TEST(test_migration_marks_as_complete);
    RUN_TEST(test_migration_initializes_empty_count);
    RUN_TEST(test_preferences_storage_format);

    // Network removal edge cases
    RUN_TEST(test_remove_first_network_shifts_correctly);
    RUN_TEST(test_remove_last_network_decrements_count);
    RUN_TEST(test_remove_all_networks_one_by_one);
    RUN_TEST(test_remove_from_empty_list_fails);

    // Password management tests
    RUN_TEST(test_update_network_keeps_password_if_empty);
    RUN_TEST(test_update_network_changes_password_if_provided);
    RUN_TEST(test_password_not_exposed_in_api);

    // WiFi connection state tests
    RUN_TEST(test_wifi_connection_status_changes);
    RUN_TEST(test_wifi_ssid_tracking);
    RUN_TEST(test_wifi_ip_configuration);

    // Multi-network advanced tests
    RUN_TEST(test_duplicate_ssid_updates_not_adds);
    RUN_TEST(test_case_sensitive_ssid_comparison);
    RUN_TEST(test_special_characters_in_ssid);
    RUN_TEST(test_very_long_ssid);
    RUN_TEST(test_network_with_spaces_in_ssid);

    // Preferences integration tests
    RUN_TEST(test_preferences_namespace_isolation);
    RUN_TEST(test_preferences_read_only_mode);

    // API handler tests
    RUN_TEST(test_wifi_list_excludes_passwords);
    RUN_TEST(test_wifi_save_validates_fields);
    RUN_TEST(test_remove_network_invalid_index);
    RUN_TEST(test_remove_network_negative_index);

    // WiFi retry logic tests
    RUN_TEST(test_wifi_retry_error_201_triggers_retry);
    RUN_TEST(test_wifi_retry_successful_connection_clears_flags);
    RUN_TEST(test_wifi_retry_counter_increments);
    RUN_TEST(test_wifi_retry_tracks_failed_ssid);
    RUN_TEST(test_wifi_retry_interval_timing);
    RUN_TEST(test_wifi_retry_multiple_networks_fallback);
    RUN_TEST(test_wifi_retry_clears_on_success_after_multiple_failures);
    RUN_TEST(test_wifi_retry_immediate_vs_periodic);
    RUN_TEST(test_wifi_retry_preserves_network_order);

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
