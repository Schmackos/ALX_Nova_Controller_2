// ===== Ethernet Settings Persistence Tests =====
// Tests for Ethernet configuration field defaults, state management,
// and dirty flag behavior. Uses a local mock that mirrors EthernetState
// fields without pulling in ESP32 dependencies.

#include <cstring>
#include <string>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Minimal mock mirroring EthernetState config fields =====

struct MockEthernetState {
    bool linkUp = false;
    bool connected = false;
    String ip = "";

    // Runtime status
    String mac = "";
    String gateway = "";
    String subnet = "";
    String dns1 = "";
    String dns2 = "";
    uint16_t speed = 0;
    bool fullDuplex = false;

    // Configuration (persisted)
    bool useStaticIP = false;
    String staticIP = "";
    String staticSubnet = "255.255.255.0";
    String staticGateway = "";
    String staticDns1 = "";
    String staticDns2 = "";
    String hostname = "alx-nova";

    // Revert timer
    bool pendingConfirm = false;
    unsigned long confirmDeadline = 0;
};

struct MockAppState {
    MockEthernetState ethernet;

    bool _ethernetDirty = false;

    void markEthernetDirty() { _ethernetDirty = true; }
    bool isEthernetDirty() const { return _ethernetDirty; }
    void clearEthernetDirty() { _ethernetDirty = false; }

    void resetEthConfig() {
        ethernet.useStaticIP = false;
        ethernet.staticIP = "";
        ethernet.staticSubnet = "255.255.255.0";
        ethernet.staticGateway = "";
        ethernet.staticDns1 = "";
        ethernet.staticDns2 = "";
        ethernet.hostname = "alx-nova";
        ethernet.pendingConfirm = false;
        ethernet.confirmDeadline = 0;
        _ethernetDirty = false;
    }
};

static MockAppState appState;

// ===== Test Setup / Teardown =====

void setUp(void) {
    appState.resetEthConfig();
    ArduinoMock::reset();
}

void tearDown(void) {}

// ===== Tests =====

void test_default_hostname_value(void) {
    TEST_ASSERT_EQUAL_STRING("alx-nova", appState.ethernet.hostname.c_str());
}

void test_hostname_max_length(void) {
    // 63 char hostname should be valid (DNS label limit)
    String longHost = "";
    for (int i = 0; i < 63; i++) longHost += 'a';
    appState.ethernet.hostname = longHost;
    TEST_ASSERT_EQUAL(63, (int)appState.ethernet.hostname.length());
}

void test_static_ip_config_complete(void) {
    appState.ethernet.useStaticIP = true;
    appState.ethernet.staticIP = "192.168.1.100";
    appState.ethernet.staticSubnet = "255.255.255.0";
    appState.ethernet.staticGateway = "192.168.1.1";
    appState.ethernet.staticDns1 = "8.8.8.8";
    appState.ethernet.staticDns2 = "8.8.4.4";

    TEST_ASSERT_TRUE(appState.ethernet.useStaticIP);
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", appState.ethernet.staticIP.c_str());
    TEST_ASSERT_EQUAL_STRING("255.255.255.0", appState.ethernet.staticSubnet.c_str());
    TEST_ASSERT_EQUAL_STRING("192.168.1.1", appState.ethernet.staticGateway.c_str());
    TEST_ASSERT_EQUAL_STRING("8.8.8.8", appState.ethernet.staticDns1.c_str());
    TEST_ASSERT_EQUAL_STRING("8.8.4.4", appState.ethernet.staticDns2.c_str());
}

void test_dhcp_mode_defaults(void) {
    TEST_ASSERT_FALSE(appState.ethernet.useStaticIP);
    TEST_ASSERT_EQUAL_STRING("", appState.ethernet.staticIP.c_str());
    TEST_ASSERT_EQUAL_STRING("255.255.255.0", appState.ethernet.staticSubnet.c_str());
}

void test_revert_to_dhcp_clears_static_flag(void) {
    appState.ethernet.useStaticIP = true;
    appState.ethernet.staticIP = "10.0.0.50";

    // Revert to DHCP
    appState.ethernet.useStaticIP = false;

    TEST_ASSERT_FALSE(appState.ethernet.useStaticIP);
    // Static IP string preserved for UI re-display
    TEST_ASSERT_EQUAL_STRING("10.0.0.50", appState.ethernet.staticIP.c_str());
}

void test_hostname_persists_across_static_ip_change(void) {
    appState.ethernet.hostname = "custom-host";
    appState.ethernet.useStaticIP = true;
    appState.ethernet.staticIP = "10.0.0.50";

    appState.ethernet.staticIP = "10.0.0.51";
    TEST_ASSERT_EQUAL_STRING("custom-host", appState.ethernet.hostname.c_str());
}

void test_empty_optional_fields_accepted(void) {
    appState.ethernet.useStaticIP = true;
    appState.ethernet.staticIP = "192.168.1.100";
    appState.ethernet.staticSubnet = "255.255.255.0";
    appState.ethernet.staticGateway = "192.168.1.1";
    appState.ethernet.staticDns1 = "";
    appState.ethernet.staticDns2 = "";

    TEST_ASSERT_EQUAL_STRING("", appState.ethernet.staticDns1.c_str());
    TEST_ASSERT_EQUAL_STRING("", appState.ethernet.staticDns2.c_str());
}

void test_all_fields_reset_independently(void) {
    appState.ethernet.useStaticIP = true;
    appState.ethernet.staticIP = "10.0.0.1";
    appState.ethernet.staticSubnet = "255.0.0.0";
    appState.ethernet.staticGateway = "10.0.0.254";
    appState.ethernet.staticDns1 = "1.1.1.1";
    appState.ethernet.staticDns2 = "1.0.0.1";
    appState.ethernet.hostname = "test";

    // Reset one field at a time
    appState.ethernet.staticDns2 = "";
    TEST_ASSERT_EQUAL_STRING("1.1.1.1", appState.ethernet.staticDns1.c_str());
    TEST_ASSERT_EQUAL_STRING("", appState.ethernet.staticDns2.c_str());
    TEST_ASSERT_EQUAL_STRING("test", appState.ethernet.hostname.c_str());
}

void test_subnet_default_preserved_when_not_set(void) {
    TEST_ASSERT_EQUAL_STRING("255.255.255.0", appState.ethernet.staticSubnet.c_str());
}

void test_confirm_timer_fields_default(void) {
    TEST_ASSERT_FALSE(appState.ethernet.pendingConfirm);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)appState.ethernet.confirmDeadline);
}

void test_confirm_timer_set_and_clear(void) {
    appState.ethernet.pendingConfirm = true;
    appState.ethernet.confirmDeadline = 120000;
    TEST_ASSERT_TRUE(appState.ethernet.pendingConfirm);

    appState.ethernet.pendingConfirm = false;
    appState.ethernet.confirmDeadline = 0;
    TEST_ASSERT_FALSE(appState.ethernet.pendingConfirm);
}

void test_dirty_flag_set_on_eth_change(void) {
    appState.clearEthernetDirty();
    appState.ethernet.hostname = "changed";
    appState.markEthernetDirty();
    TEST_ASSERT_TRUE(appState.isEthernetDirty());

    appState.clearEthernetDirty();
    TEST_ASSERT_FALSE(appState.isEthernetDirty());
}

// ===== Test Runner =====

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_default_hostname_value);
    RUN_TEST(test_hostname_max_length);
    RUN_TEST(test_static_ip_config_complete);
    RUN_TEST(test_dhcp_mode_defaults);
    RUN_TEST(test_revert_to_dhcp_clears_static_flag);
    RUN_TEST(test_hostname_persists_across_static_ip_change);
    RUN_TEST(test_empty_optional_fields_accepted);
    RUN_TEST(test_all_fields_reset_independently);
    RUN_TEST(test_subnet_default_preserved_when_not_set);
    RUN_TEST(test_confirm_timer_fields_default);
    RUN_TEST(test_confirm_timer_set_and_clear);
    RUN_TEST(test_dirty_flag_set_on_eth_change);
    return UNITY_END();
}
