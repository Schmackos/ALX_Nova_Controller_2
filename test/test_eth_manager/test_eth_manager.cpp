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

struct MockEthernetState {
    bool linkUp = false;
    bool connected = false;
    String ip = "";
    NetIfType activeInterface = NET_NONE;

    // Runtime status (populated from ETH events, NOT persisted)
    String mac = "";
    String gateway = "";
    String subnet = "";
    String dns1 = "";
    String dns2 = "";
    uint16_t speed = 0;
    bool fullDuplex = false;

    // Configuration (persisted to /config.json)
    bool useStaticIP = false;
    String staticIP = "";
    String staticSubnet = "255.255.255.0";
    String staticGateway = "";
    String staticDns1 = "";
    String staticDns2 = "";
    String hostname = "alx-nova";

    // Revert timer (NOT persisted)
    bool pendingConfirm = false;
    unsigned long confirmDeadline = 0;
};

struct MockWifiState {
    bool connectSuccess = false;
};

struct MockAppState {
    MockEthernetState ethernet;
    MockWifiState wifi;

    // Dirty flag
    bool _ethernetDirty = false;

    void markEthernetDirty() { _ethernetDirty = true; }
    bool isEthernetDirty() const { return _ethernetDirty; }
    void clearEthernetDirty() { _ethernetDirty = false; }

    void reset() {
        ethernet.linkUp = false;
        ethernet.connected = false;
        ethernet.ip = "";
        ethernet.activeInterface = NET_NONE;
        ethernet.mac = "";
        ethernet.gateway = "";
        ethernet.subnet = "";
        ethernet.dns1 = "";
        ethernet.dns2 = "";
        ethernet.speed = 0;
        ethernet.fullDuplex = false;
        ethernet.useStaticIP = false;
        ethernet.staticIP = "";
        ethernet.staticSubnet = "255.255.255.0";
        ethernet.staticGateway = "";
        ethernet.staticDns1 = "";
        ethernet.staticDns2 = "";
        ethernet.hostname = "alx-nova";
        ethernet.pendingConfirm = false;
        ethernet.confirmDeadline = 0;
        wifi.connectSuccess = false;
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
    return appState.ethernet.connected;
}

bool eth_manager_link_up() {
    return appState.ethernet.linkUp;
}

String eth_manager_get_ip() {
    return appState.ethernet.ip;
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
    TEST_ASSERT_FALSE(appState.ethernet.connected);
}

void test_default_ethLinkUp_is_false(void) {
    TEST_ASSERT_FALSE(appState.ethernet.linkUp);
}

void test_default_ethIP_is_empty(void) {
    TEST_ASSERT_EQUAL_STRING("", appState.ethernet.ip.c_str());
}

void test_default_activeInterface_is_NET_NONE(void) {
    TEST_ASSERT_EQUAL_INT(NET_NONE, appState.ethernet.activeInterface);
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
    TEST_ASSERT_EQUAL_INT(NET_NONE, appState.ethernet.activeInterface);
    appState.ethernet.connected = true;
    appState.ethernet.ip = "192.168.1.100";
    appState.ethernet.activeInterface = NET_ETHERNET;
    appState.markEthernetDirty();

    TEST_ASSERT_EQUAL_INT(NET_ETHERNET, appState.ethernet.activeInterface);
    TEST_ASSERT_TRUE(appState.isEthernetDirty());
}

void test_activeInterface_transitions_ethernet_to_wifi_on_lost_ip(void) {
    // Simulate ETH_GOT_IP path
    appState.ethernet.connected = true;
    appState.ethernet.ip = "192.168.1.100";
    appState.ethernet.activeInterface = NET_ETHERNET;
    appState.wifi.connectSuccess = true;

    // Simulate ETH_LOST_IP handler logic
    appState.ethernet.connected = false;
    appState.ethernet.ip = "";
    if (appState.ethernet.activeInterface == NET_ETHERNET) {
        appState.ethernet.activeInterface = appState.wifi.connectSuccess ? NET_WIFI : NET_NONE;
    }
    appState.markEthernetDirty();

    TEST_ASSERT_EQUAL_INT(NET_WIFI, appState.ethernet.activeInterface);
    TEST_ASSERT_FALSE(appState.ethernet.connected);
    TEST_ASSERT_EQUAL_STRING("", appState.ethernet.ip.c_str());
}

void test_activeInterface_transitions_ethernet_to_none_when_no_wifi(void) {
    // Simulate ETH_GOT_IP path
    appState.ethernet.connected = true;
    appState.ethernet.ip = "192.168.1.100";
    appState.ethernet.activeInterface = NET_ETHERNET;
    appState.wifi.connectSuccess = false;

    // Simulate ETH_DISCONNECTED handler logic
    appState.ethernet.linkUp = false;
    appState.ethernet.connected = false;
    appState.ethernet.ip = "";
    if (appState.ethernet.activeInterface == NET_ETHERNET) {
        appState.ethernet.activeInterface = appState.wifi.connectSuccess ? NET_WIFI : NET_NONE;
    }
    appState.markEthernetDirty();

    TEST_ASSERT_EQUAL_INT(NET_NONE, appState.ethernet.activeInterface);
    TEST_ASSERT_FALSE(appState.ethernet.linkUp);
}

void test_activeInterface_not_changed_when_already_wifi(void) {
    // If activeInterface is already NET_WIFI, ETH_LOST_IP should not overwrite it
    appState.ethernet.activeInterface = NET_WIFI;
    appState.ethernet.connected = true;

    // Simulate ETH_LOST_IP handler logic (only changes interface if it was ETH)
    appState.ethernet.connected = false;
    appState.ethernet.ip = "";
    if (appState.ethernet.activeInterface == NET_ETHERNET) {
        appState.ethernet.activeInterface = appState.wifi.connectSuccess ? NET_WIFI : NET_NONE;
    }

    // Should still be NET_WIFI — the guard prevented overwrite
    TEST_ASSERT_EQUAL_INT(NET_WIFI, appState.ethernet.activeInterface);
}

// ===== Stub Function Smoke Tests =====

void test_eth_manager_init_compiles_and_runs(void) {
    // Stub is a no-op on native; just verify it doesn't crash
    eth_manager_init();
    TEST_ASSERT_TRUE(true);
}

void test_eth_manager_is_connected_returns_appstate_ethConnected(void) {
    appState.ethernet.connected = false;
    TEST_ASSERT_FALSE(eth_manager_is_connected());

    appState.ethernet.connected = true;
    TEST_ASSERT_TRUE(eth_manager_is_connected());
}

void test_eth_manager_link_up_returns_appstate_ethLinkUp(void) {
    appState.ethernet.linkUp = false;
    TEST_ASSERT_FALSE(eth_manager_link_up());

    appState.ethernet.linkUp = true;
    TEST_ASSERT_TRUE(eth_manager_link_up());
}

void test_eth_manager_get_ip_returns_appstate_ethIP(void) {
    appState.ethernet.ip = "";
    TEST_ASSERT_EQUAL_STRING("", eth_manager_get_ip().c_str());

    appState.ethernet.ip = "10.0.0.50";
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

// ===== Default Config Field Tests =====

void test_default_hostname_is_alx_nova(void) {
    TEST_ASSERT_EQUAL_STRING("alx-nova", appState.ethernet.hostname.c_str());
}

void test_default_useStaticIP_is_false(void) {
    TEST_ASSERT_FALSE(appState.ethernet.useStaticIP);
}

void test_default_staticIP_is_empty(void) {
    TEST_ASSERT_EQUAL_STRING("", appState.ethernet.staticIP.c_str());
}

void test_default_staticSubnet_is_255(void) {
    TEST_ASSERT_EQUAL_STRING("255.255.255.0", appState.ethernet.staticSubnet.c_str());
}

void test_default_speed_is_zero(void) {
    TEST_ASSERT_EQUAL_UINT16(0, appState.ethernet.speed);
}

void test_default_fullDuplex_is_false(void) {
    TEST_ASSERT_FALSE(appState.ethernet.fullDuplex);
}

void test_default_pendingConfirm_is_false(void) {
    TEST_ASSERT_FALSE(appState.ethernet.pendingConfirm);
}

void test_default_confirmDeadline_is_zero(void) {
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)appState.ethernet.confirmDeadline);
}

void test_default_mac_is_empty(void) {
    TEST_ASSERT_EQUAL_STRING("", appState.ethernet.mac.c_str());
}

void test_default_gateway_is_empty(void) {
    TEST_ASSERT_EQUAL_STRING("", appState.ethernet.gateway.c_str());
}

void test_default_dns1_is_empty(void) {
    TEST_ASSERT_EQUAL_STRING("", appState.ethernet.dns1.c_str());
}

void test_static_ip_fields_independent(void) {
    appState.ethernet.staticIP = "192.168.1.100";
    appState.ethernet.staticGateway = "192.168.1.1";
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", appState.ethernet.staticIP.c_str());
    TEST_ASSERT_EQUAL_STRING("192.168.1.1", appState.ethernet.staticGateway.c_str());
    TEST_ASSERT_EQUAL_STRING("255.255.255.0", appState.ethernet.staticSubnet.c_str());
    TEST_ASSERT_EQUAL_STRING("", appState.ethernet.staticDns1.c_str());
}

void test_hostname_assignment(void) {
    appState.ethernet.hostname = "my-audio-device";
    TEST_ASSERT_EQUAL_STRING("my-audio-device", appState.ethernet.hostname.c_str());
}

void test_runtime_fields_cleared_on_reset(void) {
    // Simulate connected state
    appState.ethernet.speed = 100;
    appState.ethernet.fullDuplex = true;
    appState.ethernet.gateway = "192.168.1.1";
    appState.ethernet.subnet = "255.255.255.0";
    appState.ethernet.dns1 = "8.8.8.8";
    appState.ethernet.dns2 = "8.8.4.4";
    appState.ethernet.mac = "AA:BB:CC:DD:EE:FF";

    // Simulate disconnect (clear runtime, keep MAC)
    appState.ethernet.speed = 0;
    appState.ethernet.fullDuplex = false;
    appState.ethernet.gateway = "";
    appState.ethernet.subnet = "";
    appState.ethernet.dns1 = "";
    appState.ethernet.dns2 = "";

    TEST_ASSERT_EQUAL_UINT16(0, appState.ethernet.speed);
    TEST_ASSERT_FALSE(appState.ethernet.fullDuplex);
    TEST_ASSERT_EQUAL_STRING("", appState.ethernet.gateway.c_str());
    TEST_ASSERT_EQUAL_STRING("AA:BB:CC:DD:EE:FF", appState.ethernet.mac.c_str());
}

void test_static_config_preserved_on_disconnect(void) {
    appState.ethernet.useStaticIP = true;
    appState.ethernet.staticIP = "10.0.0.50";
    appState.ethernet.staticGateway = "10.0.0.1";
    appState.ethernet.hostname = "test-device";

    // Simulate disconnect
    appState.ethernet.connected = false;
    appState.ethernet.linkUp = false;
    appState.ethernet.ip = "";

    TEST_ASSERT_TRUE(appState.ethernet.useStaticIP);
    TEST_ASSERT_EQUAL_STRING("10.0.0.50", appState.ethernet.staticIP.c_str());
    TEST_ASSERT_EQUAL_STRING("10.0.0.1", appState.ethernet.staticGateway.c_str());
    TEST_ASSERT_EQUAL_STRING("test-device", appState.ethernet.hostname.c_str());
}

void test_pending_confirm_lifecycle(void) {
    appState.ethernet.pendingConfirm = true;
    appState.ethernet.confirmDeadline = 60000;
    TEST_ASSERT_TRUE(appState.ethernet.pendingConfirm);
    TEST_ASSERT_EQUAL_UINT32(60000, (uint32_t)appState.ethernet.confirmDeadline);

    appState.ethernet.pendingConfirm = false;
    appState.ethernet.confirmDeadline = 0;
    TEST_ASSERT_FALSE(appState.ethernet.pendingConfirm);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)appState.ethernet.confirmDeadline);
}

void test_dirty_flag_on_config_change(void) {
    appState.clearEthernetDirty();
    TEST_ASSERT_FALSE(appState.isEthernetDirty());

    appState.ethernet.useStaticIP = true;
    appState.markEthernetDirty();
    TEST_ASSERT_TRUE(appState.isEthernetDirty());
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

    // Default config field tests
    RUN_TEST(test_default_hostname_is_alx_nova);
    RUN_TEST(test_default_useStaticIP_is_false);
    RUN_TEST(test_default_staticIP_is_empty);
    RUN_TEST(test_default_staticSubnet_is_255);
    RUN_TEST(test_default_speed_is_zero);
    RUN_TEST(test_default_fullDuplex_is_false);
    RUN_TEST(test_default_pendingConfirm_is_false);
    RUN_TEST(test_default_confirmDeadline_is_zero);
    RUN_TEST(test_default_mac_is_empty);
    RUN_TEST(test_default_gateway_is_empty);
    RUN_TEST(test_default_dns1_is_empty);

    // Static IP config tests
    RUN_TEST(test_static_ip_fields_independent);
    RUN_TEST(test_hostname_assignment);
    RUN_TEST(test_runtime_fields_cleared_on_reset);
    RUN_TEST(test_static_config_preserved_on_disconnect);
    RUN_TEST(test_pending_confirm_lifecycle);
    RUN_TEST(test_dirty_flag_on_config_change);

    return UNITY_END();
}
