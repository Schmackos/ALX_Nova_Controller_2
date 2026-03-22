#include <unity.h>
#include "../../src/config.h"

void setUp(void) {}
void tearDown(void) {}

// Test skip factor calculation logic (replicated from websocket_handler.cpp)
static uint8_t calcSkipFactor(uint8_t authCount) {
    if (authCount >= 8) return WS_BINARY_SKIP_8PLUS;
    if (authCount >= 5) return WS_BINARY_SKIP_5PLUS;
    if (authCount >= 3) return WS_BINARY_SKIP_3PLUS;
    if (authCount == 2) return WS_BINARY_SKIP_2_CLIENTS;
    return 1;
}

void test_skip_factor_0_clients(void) {
    TEST_ASSERT_EQUAL_UINT8(1, calcSkipFactor(0));
}

void test_skip_factor_1_client(void) {
    TEST_ASSERT_EQUAL_UINT8(1, calcSkipFactor(1));
}

void test_skip_factor_2_clients(void) {
    TEST_ASSERT_EQUAL_UINT8(WS_BINARY_SKIP_2_CLIENTS, calcSkipFactor(2));
}

void test_skip_factor_3_clients(void) {
    TEST_ASSERT_EQUAL_UINT8(WS_BINARY_SKIP_3PLUS, calcSkipFactor(3));
}

void test_skip_factor_5_clients(void) {
    TEST_ASSERT_EQUAL_UINT8(WS_BINARY_SKIP_5PLUS, calcSkipFactor(5));
}

void test_skip_factor_7_clients(void) {
    TEST_ASSERT_EQUAL_UINT8(WS_BINARY_SKIP_5PLUS, calcSkipFactor(7));
}

void test_skip_factor_8_clients(void) {
    TEST_ASSERT_EQUAL_UINT8(WS_BINARY_SKIP_8PLUS, calcSkipFactor(8));
}

void test_skip_factor_12_clients(void) {
    TEST_ASSERT_EQUAL_UINT8(WS_BINARY_SKIP_8PLUS, calcSkipFactor(12));
}

void test_frame_allow_pattern_1_client(void) {
    // With skip factor 1, every frame is allowed
    uint8_t skipFactor = calcSkipFactor(1);
    int allowed = 0;
    for (uint8_t i = 1; i <= 10; i++) {
        if ((i % skipFactor) == 0) allowed++;
    }
    TEST_ASSERT_EQUAL_INT(10, allowed); // all 10 frames allowed
}

void test_frame_allow_pattern_2_clients(void) {
    // With skip factor 2, every other frame
    uint8_t skipFactor = calcSkipFactor(2);
    int allowed = 0;
    for (uint8_t i = 1; i <= 10; i++) {
        if ((i % skipFactor) == 0) allowed++;
    }
    TEST_ASSERT_EQUAL_INT(5, allowed); // 5 of 10 frames
}

void test_frame_allow_pattern_3_clients(void) {
    // With skip factor 4, every 4th frame
    uint8_t skipFactor = calcSkipFactor(3);
    int allowed = 0;
    for (uint8_t i = 1; i <= 16; i++) {
        if ((i % skipFactor) == 0) allowed++;
    }
    TEST_ASSERT_EQUAL_INT(4, allowed); // 4 of 16 frames
}

void test_skip_counter_wraps(void) {
    // uint8_t wraps at 255 -> 0. Verify modulo still works
    uint8_t counter = 255;
    counter++; // wraps to 0
    TEST_ASSERT_EQUAL_UINT8(0, counter);
    // 0 % any_non_zero == 0, which means "allow" — correct behavior
    TEST_ASSERT_EQUAL_UINT8(0, counter % WS_BINARY_SKIP_3PLUS);
}

void test_recount_interval_reasonable(void) {
    TEST_ASSERT_GREATER_THAN(1000, WS_AUTH_RECOUNT_INTERVAL_MS);
    TEST_ASSERT_LESS_THAN(60000, WS_AUTH_RECOUNT_INTERVAL_MS);
}

void test_constants_are_positive(void) {
    TEST_ASSERT_GREATER_THAN(0, WS_BINARY_SKIP_2_CLIENTS);
    TEST_ASSERT_GREATER_THAN(0, WS_BINARY_SKIP_3PLUS);
    TEST_ASSERT_GREATER_THAN(0, WS_BINARY_SKIP_5PLUS);
    TEST_ASSERT_GREATER_THAN(0, WS_BINARY_SKIP_8PLUS);
    TEST_ASSERT_GREATER_OR_EQUAL(WS_BINARY_SKIP_2_CLIENTS, WS_BINARY_SKIP_3PLUS);
    TEST_ASSERT_GREATER_OR_EQUAL(WS_BINARY_SKIP_3PLUS, WS_BINARY_SKIP_5PLUS);
    TEST_ASSERT_GREATER_OR_EQUAL(WS_BINARY_SKIP_5PLUS, WS_BINARY_SKIP_8PLUS);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_skip_factor_0_clients);
    RUN_TEST(test_skip_factor_1_client);
    RUN_TEST(test_skip_factor_2_clients);
    RUN_TEST(test_skip_factor_3_clients);
    RUN_TEST(test_skip_factor_5_clients);
    RUN_TEST(test_skip_factor_7_clients);
    RUN_TEST(test_skip_factor_8_clients);
    RUN_TEST(test_skip_factor_12_clients);
    RUN_TEST(test_frame_allow_pattern_1_client);
    RUN_TEST(test_frame_allow_pattern_2_clients);
    RUN_TEST(test_frame_allow_pattern_3_clients);
    RUN_TEST(test_skip_counter_wraps);
    RUN_TEST(test_recount_interval_reasonable);
    RUN_TEST(test_constants_are_positive);
    return UNITY_END();
}
