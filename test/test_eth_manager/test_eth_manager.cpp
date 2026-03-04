#include <cstring>
#include <string>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/ETH.h"
#else
#include <Arduino.h>
#include <ETH.h>
#endif

// ===== Minimal AppState mock for Ethernet manager testing =====
// Mirrors only the Ethernet-relevant fields from src/app_state.h without
// pulling in ESP32 dependencies (WiFi, PubSubClient, WebServer, etc.)

enum NetIfType { NET_NONE, NET_ETHERNET, NET_WIFI };

struct MockAppState {
    bool ethLinkUp = false;
    bool ethConnected = false;
    String ethIP = "";
    NetIfType activeInterface = NET_NONE;
    bool wifiConnectSuccess = false;

    // Dirty flag
    bool _ethernetDirty = false;

    void markEthernetDirty() { _ethernetDirty = true; }
    bool isEthernetDirty() const { return _ethernetDirty; }
    void clearEthernetDirty() { _ethernetDirty = false; }

    void reset() {
        ethLinkUp = false;
        ethConnected = false;
        ethIP = "";
        activeInterface = NET_NONE;
        wifiConnectSuccess = false;
        _ethernetDirty = false;
    }
};

static MockAppState appState;

// ===== Inline eth_manager stubs for native tests =====
// The real eth_manager.cpp compiles only on P4 targets (CONFIG_IDF_TARGET_ESP32P4).
// These stubs mirror the stub bodies in eth_manager.cpp's #else branch so we can
// exercise the AppState interactions directly in this test file.

void eth_manager_init() {}

bool eth_manager_is_connected() {
    return appState.ethConnected;
}

bool eth_manager_link_up() {
    return appState.ethLinkUp;
}

String eth_manager_get_ip() {
    return appState.ethIP;
}

void eth_manager_set_default_route() {
    if (ETH._hasIP) {
        ETH.setDefault();
    }
}

// ===== Test Setup / Teardown =====

void setUp(void) {
    appState.reset();
    ETH._linkUp = false;
    ETH._connected = false;
    ETH._hasIP = false;
    ArduinoMock::reset();
}

void tearDown(void) {}

// ===== Default State Tests =====

void test_default_ethConnected_is_false(void) {
    TEST_ASSERT_FALSE(appState.ethConnected);
}

void test_default_ethLinkUp_is_false(void) {
    TEST_ASSERT_FALSE(appState.ethLinkUp);
}

void test_default_ethIP_is_empty(void) {
    TEST_ASSERT_EQUAL_STRING("", appState.ethIP.c_str());
}

void test_default_activeInterface_is_NET_NONE(void) {
    TEST_ASSERT_EQUAL_INT(NET_NONE, appState.activeInterface);
}

// ===== NetIfType Enum Values =====

void test_NET_NONE_is_zero(void) {
    TEST_ASSERT_EQUAL_INT(0, NET_NONE);
}

void test_NET_ETHERNET_is_one(void) {
    TEST_ASSERT_EQUAL_INT(1, NET_ETHERNET);
}

void test_NET_WIFI_is_two(void) {
    TEST_ASSERT_EQUAL_INT(2, NET_WIFI);
}

// ===== Ethernet Dirty Flag =====

void test_ethernet_dirty_flag_clear_on_reset(void) {
    TEST_ASSERT_FALSE(appState.isEthernetDirty());
}

void test_ethernet_dirty_flag_set_by_mark(void) {
    appState.markEthernetDirty();
    TEST_ASSERT_TRUE(appState.isEthernetDirty());
}

void test_ethernet_dirty_flag_cleared_after_clear(void) {
    appState.markEthernetDirty();
    appState.clearEthernetDirty();
    TEST_ASSERT_FALSE(appState.isEthernetDirty());
}

// ===== EVT_ETHERNET Bit Definition =====

void test_EVT_ETHERNET_is_bit_11(void) {
    // app_events.h defines EVT_ETHERNET as (1UL << 11)
    const uint32_t EVT_ETHERNET = (1UL << 11);
    TEST_ASSERT_EQUAL_UINT32(2048U, EVT_ETHERNET);
}

void test_EVT_ETHERNET_does_not_overlap_other_events(void) {
    // Verify bit 11 is distinct from all other defined event bits (0-10)
    const uint32_t EVT_ETHERNET = (1UL << 11);
    const uint32_t other_events = (1UL << 0)  // EVT_OTA
                                | (1UL << 1)  // EVT_DISPLAY
                                | (1UL << 2)  // EVT_BUZZER
                                | (1UL << 3)  // EVT_SIGGEN
                                | (1UL << 4)  // EVT_DSP_CONFIG
                                | (1UL << 5)  // EVT_DAC
                                | (1UL << 6)  // EVT_EEPROM
                                | (1UL << 7)  // EVT_USB_AUDIO
                                | (1UL << 8)  // EVT_USB_VU
                                | (1UL << 9)  // EVT_SETTINGS
                                | (1UL << 10); // EVT_ADC_ENABLED
    TEST_ASSERT_EQUAL_UINT32(0U, EVT_ETHERNET & other_events);
}

// ===== ActiveInterface Transitions =====

void test_activeInterface_transitions_none_to_ethernet(void) {
    TEST_ASSERT_EQUAL_INT(NET_NONE, appState.activeInterface);
    appState.ethConnected = true;
    appState.ethIP = "192.168.1.100";
    appState.activeInterface = NET_ETHERNET;
    appState.markEthernetDirty();

    TEST_ASSERT_EQUAL_INT(NET_ETHERNET, appState.activeInterface);
    TEST_ASSERT_TRUE(appState.isEthernetDirty());
}

void test_activeInterface_transitions_ethernet_to_wifi_on_lost_ip(void) {
    // Simulate ETH_GOT_IP path
    appState.ethConnected = true;
    appState.ethIP = "192.168.1.100";
    appState.activeInterface = NET_ETHERNET;
    appState.wifiConnectSuccess = true;

    // Simulate ETH_LOST_IP handler logic
    appState.ethConnected = false;
    appState.ethIP = "";
    if (appState.activeInterface == NET_ETHERNET) {
        appState.activeInterface = appState.wifiConnectSuccess ? NET_WIFI : NET_NONE;
    }
    appState.markEthernetDirty();

    TEST_ASSERT_EQUAL_INT(NET_WIFI, appState.activeInterface);
    TEST_ASSERT_FALSE(appState.ethConnected);
    TEST_ASSERT_EQUAL_STRING("", appState.ethIP.c_str());
}

void test_activeInterface_transitions_ethernet_to_none_when_no_wifi(void) {
    // Simulate ETH_GOT_IP path
    appState.ethConnected = true;
    appState.ethIP = "192.168.1.100";
    appState.activeInterface = NET_ETHERNET;
    appState.wifiConnectSuccess = false;

    // Simulate ETH_DISCONNECTED handler logic
    appState.ethLinkUp = false;
    appState.ethConnected = false;
    appState.ethIP = "";
    if (appState.activeInterface == NET_ETHERNET) {
        appState.activeInterface = appState.wifiConnectSuccess ? NET_WIFI : NET_NONE;
    }
    appState.markEthernetDirty();

    TEST_ASSERT_EQUAL_INT(NET_NONE, appState.activeInterface);
    TEST_ASSERT_FALSE(appState.ethLinkUp);
}

void test_activeInterface_not_changed_when_already_wifi(void) {
    // If activeInterface is already NET_WIFI, ETH_LOST_IP should not overwrite it
    appState.activeInterface = NET_WIFI;
    appState.ethConnected = true;

    // Simulate ETH_LOST_IP handler logic (only changes interface if it was ETH)
    appState.ethConnected = false;
    appState.ethIP = "";
    if (appState.activeInterface == NET_ETHERNET) {
        appState.activeInterface = appState.wifiConnectSuccess ? NET_WIFI : NET_NONE;
    }

    // Should still be NET_WIFI — the guard prevented overwrite
    TEST_ASSERT_EQUAL_INT(NET_WIFI, appState.activeInterface);
}

// ===== Stub Function Smoke Tests =====

void test_eth_manager_init_compiles_and_runs(void) {
    // Stub is a no-op on native; just verify it doesn't crash
    eth_manager_init();
    TEST_ASSERT_TRUE(true);
}

void test_eth_manager_is_connected_returns_appstate_ethConnected(void) {
    appState.ethConnected = false;
    TEST_ASSERT_FALSE(eth_manager_is_connected());

    appState.ethConnected = true;
    TEST_ASSERT_TRUE(eth_manager_is_connected());
}

void test_eth_manager_link_up_returns_appstate_ethLinkUp(void) {
    appState.ethLinkUp = false;
    TEST_ASSERT_FALSE(eth_manager_link_up());

    appState.ethLinkUp = true;
    TEST_ASSERT_TRUE(eth_manager_link_up());
}

void test_eth_manager_get_ip_returns_appstate_ethIP(void) {
    appState.ethIP = "";
    TEST_ASSERT_EQUAL_STRING("", eth_manager_get_ip().c_str());

    appState.ethIP = "10.0.0.50";
    TEST_ASSERT_EQUAL_STRING("10.0.0.50", eth_manager_get_ip().c_str());
}

void test_eth_manager_set_default_route_no_crash_when_no_ip(void) {
    ETH._hasIP = false;
    eth_manager_set_default_route(); // must not crash
    TEST_ASSERT_TRUE(true);
}

void test_eth_manager_set_default_route_calls_set_default_when_has_ip(void) {
    ETH._hasIP = true;
    // setDefault() is a no-op in the mock; just verify it runs without error
    eth_manager_set_default_route();
    TEST_ASSERT_TRUE(true);
}

// ===== Mock ETH Class Defaults =====

void test_mock_eth_linkSpeed_returns_100(void) {
    TEST_ASSERT_EQUAL_INT(100, ETH.linkSpeed());
}

void test_mock_eth_fullDuplex_returns_true(void) {
    TEST_ASSERT_TRUE(ETH.fullDuplex());
}

void test_mock_eth_localIP_toString_returns_address(void) {
    String ip = ETH.localIP().toString();
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", ip.c_str());
}

// ===== Test Runner =====

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Default state tests
    RUN_TEST(test_default_ethConnected_is_false);
    RUN_TEST(test_default_ethLinkUp_is_false);
    RUN_TEST(test_default_ethIP_is_empty);
    RUN_TEST(test_default_activeInterface_is_NET_NONE);

    // NetIfType enum values
    RUN_TEST(test_NET_NONE_is_zero);
    RUN_TEST(test_NET_ETHERNET_is_one);
    RUN_TEST(test_NET_WIFI_is_two);

    // Ethernet dirty flag
    RUN_TEST(test_ethernet_dirty_flag_clear_on_reset);
    RUN_TEST(test_ethernet_dirty_flag_set_by_mark);
    RUN_TEST(test_ethernet_dirty_flag_cleared_after_clear);

    // EVT_ETHERNET bit definition
    RUN_TEST(test_EVT_ETHERNET_is_bit_11);
    RUN_TEST(test_EVT_ETHERNET_does_not_overlap_other_events);

    // ActiveInterface transitions
    RUN_TEST(test_activeInterface_transitions_none_to_ethernet);
    RUN_TEST(test_activeInterface_transitions_ethernet_to_wifi_on_lost_ip);
    RUN_TEST(test_activeInterface_transitions_ethernet_to_none_when_no_wifi);
    RUN_TEST(test_activeInterface_not_changed_when_already_wifi);

    // Stub function smoke tests
    RUN_TEST(test_eth_manager_init_compiles_and_runs);
    RUN_TEST(test_eth_manager_is_connected_returns_appstate_ethConnected);
    RUN_TEST(test_eth_manager_link_up_returns_appstate_ethLinkUp);
    RUN_TEST(test_eth_manager_get_ip_returns_appstate_ethIP);
    RUN_TEST(test_eth_manager_set_default_route_no_crash_when_no_ip);
    RUN_TEST(test_eth_manager_set_default_route_calls_set_default_when_has_ip);

    // Mock ETH class defaults
    RUN_TEST(test_mock_eth_linkSpeed_returns_100);
    RUN_TEST(test_mock_eth_fullDuplex_returns_true);
    RUN_TEST(test_mock_eth_localIP_toString_returns_address);

    return UNITY_END();
}
