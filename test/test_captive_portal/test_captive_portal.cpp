#include <unity.h>
#include <string.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// Header-only, pure C functions — no hardware or framework dependencies.
#include "../../src/captive_portal.h"

// ===== Test Setup / Teardown =====

void setUp(void) {
    // Nothing to reset — pure functions, no state.
}

void tearDown(void) {
    // Nothing to clean up.
}

// ===== captive_portal_is_probe_url — known probe URLs =====

void test_probe_url_android_generate_204(void) {
    TEST_ASSERT_TRUE(captive_portal_is_probe_url("/generate_204"));
}

void test_probe_url_apple_hotspot_detect(void) {
    TEST_ASSERT_TRUE(captive_portal_is_probe_url("/hotspot-detect.html"));
}

void test_probe_url_windows_connecttest(void) {
    TEST_ASSERT_TRUE(captive_portal_is_probe_url("/connecttest.txt"));
}

void test_probe_url_windows_redirect(void) {
    TEST_ASSERT_TRUE(captive_portal_is_probe_url("/redirect"));
}

void test_probe_url_windows_legacy_ncsi(void) {
    TEST_ASSERT_TRUE(captive_portal_is_probe_url("/ncsi.txt"));
}

void test_probe_url_firefox_success(void) {
    TEST_ASSERT_TRUE(captive_portal_is_probe_url("/success.txt"));
}

// ===== captive_portal_is_probe_url — non-probe URLs =====

void test_probe_url_root_path_false(void) {
    TEST_ASSERT_FALSE(captive_portal_is_probe_url("/"));
}

void test_probe_url_api_path_false(void) {
    TEST_ASSERT_FALSE(captive_portal_is_probe_url("/api/settings"));
}

void test_probe_url_login_false(void) {
    TEST_ASSERT_FALSE(captive_portal_is_probe_url("/login"));
}

void test_probe_url_favicon_false(void) {
    TEST_ASSERT_FALSE(captive_portal_is_probe_url("/favicon.ico"));
}

void test_probe_url_null_false(void) {
    TEST_ASSERT_FALSE(captive_portal_is_probe_url(nullptr));
}

// ===== captive_portal_is_device_host — matching cases =====

void test_device_host_ap_ip_exact_match(void) {
    TEST_ASSERT_TRUE(captive_portal_is_device_host("192.168.4.1", "192.168.4.1", ""));
}

void test_device_host_sta_ip_exact_match(void) {
    TEST_ASSERT_TRUE(captive_portal_is_device_host("192.168.1.100", "192.168.4.1", "192.168.1.100"));
}

void test_device_host_ap_subnet_match(void) {
    // A host in 192.168.4.x (not matching apIP or staIP) still counts as local.
    TEST_ASSERT_TRUE(captive_portal_is_device_host("192.168.4.2", "192.168.4.1", "10.0.0.1"));
}

void test_device_host_empty_string_true(void) {
    TEST_ASSERT_TRUE(captive_portal_is_device_host("", "192.168.4.1", "10.0.0.1"));
}

void test_device_host_null_true(void) {
    TEST_ASSERT_TRUE(captive_portal_is_device_host(nullptr, "192.168.4.1", "10.0.0.1"));
}

// ===== captive_portal_is_device_host — external hostnames =====

void test_device_host_apple_captive_false(void) {
    TEST_ASSERT_FALSE(captive_portal_is_device_host("captive.apple.com", "192.168.4.1", ""));
}

void test_device_host_android_gstatic_false(void) {
    TEST_ASSERT_FALSE(captive_portal_is_device_host("connectivitycheck.gstatic.com", "192.168.4.1", ""));
}

void test_device_host_windows_msft_false(void) {
    TEST_ASSERT_FALSE(captive_portal_is_device_host("www.msftconnecttest.com", "192.168.4.1", ""));
}

// ===== Test Runner =====

int runUnityTests(void) {
    UNITY_BEGIN();

    // Known probe URLs → true
    RUN_TEST(test_probe_url_android_generate_204);
    RUN_TEST(test_probe_url_apple_hotspot_detect);
    RUN_TEST(test_probe_url_windows_connecttest);
    RUN_TEST(test_probe_url_windows_redirect);
    RUN_TEST(test_probe_url_windows_legacy_ncsi);
    RUN_TEST(test_probe_url_firefox_success);

    // Non-probe URLs → false
    RUN_TEST(test_probe_url_root_path_false);
    RUN_TEST(test_probe_url_api_path_false);
    RUN_TEST(test_probe_url_login_false);
    RUN_TEST(test_probe_url_favicon_false);
    RUN_TEST(test_probe_url_null_false);

    // Device-host matching → true
    RUN_TEST(test_device_host_ap_ip_exact_match);
    RUN_TEST(test_device_host_sta_ip_exact_match);
    RUN_TEST(test_device_host_ap_subnet_match);
    RUN_TEST(test_device_host_empty_string_true);
    RUN_TEST(test_device_host_null_true);

    // External hostnames → false
    RUN_TEST(test_device_host_apple_captive_false);
    RUN_TEST(test_device_host_android_gstatic_false);
    RUN_TEST(test_device_host_windows_msft_false);

    return UNITY_END();
}

#ifdef NATIVE_TEST
int main(void) {
    return runUnityTests();
}
#endif

#ifndef NATIVE_TEST
void setup() {
    delay(2000);
    runUnityTests();
}

void loop() {
    // Do nothing
}
#endif
