// test_health_check.cpp
// Native unit tests for the health check module data structures and AppState
// integration.
//
// The health_check.cpp implementation is guarded by #ifndef NATIVE_TEST, so
// the actual check functions (system, I2C, HAL, etc.) are not available here.
// This file tests:
//   - HealthCheckResult enum values
//   - HealthCheckItem struct layout and field sizes
//   - HealthCheckReport struct: manual item addition, counts, capacity limits
//   - HealthCheckState struct defaults and field assignment
//   - AppState dirty flag integration (markHealthCheckDirty / isHealthCheckDirty
//     / clearHealthCheckDirty)

#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

#include "../../src/health_check.h"
#include "../../src/state/health_check_state.h"
#include "../../src/config.h"
#include "../../src/app_state.h"

// Convenience macro
#define appState AppState::getInstance()

// ---------------------------------------------------------------------------
// Helper: replicates the _add_item logic from health_check.cpp so we can test
// report population without the hardware-guarded implementation.
// ---------------------------------------------------------------------------

static void add_item(HealthCheckReport* r, const char* name,
                     HealthCheckResult result, const char* detail) {
    if (r->count >= HEALTH_CHECK_MAX_ITEMS) return;
    HealthCheckItem& item = r->items[r->count++];
    strncpy(item.name, name, sizeof(item.name) - 1);
    item.name[sizeof(item.name) - 1] = '\0';
    strncpy(item.detail, detail, sizeof(item.detail) - 1);
    item.detail[sizeof(item.detail) - 1] = '\0';
    item.result = result;
    switch (result) {
        case HC_PASS: r->passCount++; break;
        case HC_WARN: r->warnCount++; break;
        case HC_FAIL: r->failCount++; break;
        case HC_SKIP: r->skipCount++; break;
    }
}

// ---------------------------------------------------------------------------
// setUp / tearDown
// ---------------------------------------------------------------------------

void setUp(void) {
    // Reset AppState health check fields
    appState.healthCheck = HealthCheckState{};
    appState.clearHealthCheckDirty();
}

void tearDown(void) {}

// ===========================================================================
// Section 1: HealthCheckResult enum values
// ===========================================================================

void test_enum_hc_pass_is_zero(void) {
    TEST_ASSERT_EQUAL_UINT8(0, HC_PASS);
}

void test_enum_hc_warn_is_one(void) {
    TEST_ASSERT_EQUAL_UINT8(1, HC_WARN);
}

void test_enum_hc_fail_is_two(void) {
    TEST_ASSERT_EQUAL_UINT8(2, HC_FAIL);
}

void test_enum_hc_skip_is_three(void) {
    TEST_ASSERT_EQUAL_UINT8(3, HC_SKIP);
}

// ===========================================================================
// Section 2: HealthCheckItem struct layout
// ===========================================================================

void test_item_name_field_size(void) {
    HealthCheckItem item;
    TEST_ASSERT_EQUAL(24, sizeof(item.name));
}

void test_item_detail_field_size(void) {
    HealthCheckItem item;
    TEST_ASSERT_EQUAL(40, sizeof(item.detail));
}

void test_item_result_field_size(void) {
    // HealthCheckResult is uint8_t
    TEST_ASSERT_EQUAL(1, sizeof(HealthCheckResult));
}

void test_item_zeroed_struct(void) {
    HealthCheckItem item;
    memset(&item, 0, sizeof(item));
    TEST_ASSERT_EQUAL_UINT8(HC_PASS, item.result);  // 0 == HC_PASS
    TEST_ASSERT_EQUAL_STRING("", item.name);
    TEST_ASSERT_EQUAL_STRING("", item.detail);
}

// ===========================================================================
// Section 3: HealthCheckReport — capacity and counts
// ===========================================================================

void test_max_items_constant(void) {
    TEST_ASSERT_EQUAL(32, HEALTH_CHECK_MAX_ITEMS);
}

void test_report_zero_initialized(void) {
    HealthCheckReport report;
    memset(&report, 0, sizeof(report));

    TEST_ASSERT_EQUAL_UINT8(0, report.count);
    TEST_ASSERT_EQUAL_UINT8(0, report.passCount);
    TEST_ASSERT_EQUAL_UINT8(0, report.warnCount);
    TEST_ASSERT_EQUAL_UINT8(0, report.failCount);
    TEST_ASSERT_EQUAL_UINT8(0, report.skipCount);
    TEST_ASSERT_EQUAL_UINT32(0, report.durationMs);
    TEST_ASSERT_EQUAL_UINT32(0, report.timestamp);
    TEST_ASSERT_FALSE(report.deferredPhase);
}

void test_add_single_pass_item(void) {
    HealthCheckReport report;
    memset(&report, 0, sizeof(report));

    add_item(&report, "heap_free", HC_PASS, "82KB free");

    TEST_ASSERT_EQUAL_UINT8(1, report.count);
    TEST_ASSERT_EQUAL_UINT8(1, report.passCount);
    TEST_ASSERT_EQUAL_UINT8(0, report.failCount);
    TEST_ASSERT_EQUAL_STRING("heap_free", report.items[0].name);
    TEST_ASSERT_EQUAL_STRING("82KB free", report.items[0].detail);
    TEST_ASSERT_EQUAL_UINT8(HC_PASS, report.items[0].result);
}

void test_add_single_fail_item(void) {
    HealthCheckReport report;
    memset(&report, 0, sizeof(report));

    add_item(&report, "storage", HC_FAIL, "LittleFS not mounted");

    TEST_ASSERT_EQUAL_UINT8(1, report.count);
    TEST_ASSERT_EQUAL_UINT8(0, report.passCount);
    TEST_ASSERT_EQUAL_UINT8(1, report.failCount);
    TEST_ASSERT_EQUAL_UINT8(HC_FAIL, report.items[0].result);
}

void test_add_single_warn_item(void) {
    HealthCheckReport report;
    memset(&report, 0, sizeof(report));

    add_item(&report, "psram", HC_WARN, "PSRAM not detected");

    TEST_ASSERT_EQUAL_UINT8(1, report.count);
    TEST_ASSERT_EQUAL_UINT8(1, report.warnCount);
    TEST_ASSERT_EQUAL_UINT8(HC_WARN, report.items[0].result);
}

void test_add_single_skip_item(void) {
    HealthCheckReport report;
    memset(&report, 0, sizeof(report));

    add_item(&report, "mqtt", HC_SKIP, "not configured");

    TEST_ASSERT_EQUAL_UINT8(1, report.count);
    TEST_ASSERT_EQUAL_UINT8(1, report.skipCount);
    TEST_ASSERT_EQUAL_UINT8(HC_SKIP, report.items[0].result);
}

void test_add_mixed_items_counts(void) {
    HealthCheckReport report;
    memset(&report, 0, sizeof(report));

    add_item(&report, "heap_free",  HC_PASS, "82KB");
    add_item(&report, "psram",      HC_WARN, "low");
    add_item(&report, "dma_alloc",  HC_FAIL, "failed");
    add_item(&report, "storage",    HC_PASS, "ok");
    add_item(&report, "mqtt",       HC_SKIP, "not configured");
    add_item(&report, "network",    HC_FAIL, "disconnected");

    TEST_ASSERT_EQUAL_UINT8(6, report.count);
    TEST_ASSERT_EQUAL_UINT8(2, report.passCount);
    TEST_ASSERT_EQUAL_UINT8(1, report.warnCount);
    TEST_ASSERT_EQUAL_UINT8(2, report.failCount);
    TEST_ASSERT_EQUAL_UINT8(1, report.skipCount);
}

void test_add_items_up_to_max(void) {
    HealthCheckReport report;
    memset(&report, 0, sizeof(report));

    for (int i = 0; i < HEALTH_CHECK_MAX_ITEMS; i++) {
        char name[24];
        snprintf(name, sizeof(name), "check_%02d", i);
        add_item(&report, name, HC_PASS, "ok");
    }

    TEST_ASSERT_EQUAL_UINT8(HEALTH_CHECK_MAX_ITEMS, report.count);
    TEST_ASSERT_EQUAL_UINT8(HEALTH_CHECK_MAX_ITEMS, report.passCount);
}

void test_add_items_beyond_max_is_capped(void) {
    HealthCheckReport report;
    memset(&report, 0, sizeof(report));

    // Fill to max
    for (int i = 0; i < HEALTH_CHECK_MAX_ITEMS; i++) {
        char name[24];
        snprintf(name, sizeof(name), "check_%02d", i);
        add_item(&report, name, HC_PASS, "ok");
    }

    // Try to add one more — should be silently ignored
    add_item(&report, "overflow", HC_FAIL, "should not appear");

    TEST_ASSERT_EQUAL_UINT8(HEALTH_CHECK_MAX_ITEMS, report.count);
    TEST_ASSERT_EQUAL_UINT8(HEALTH_CHECK_MAX_ITEMS, report.passCount);
    TEST_ASSERT_EQUAL_UINT8(0, report.failCount);  // overflow item was not added
}

void test_add_items_beyond_max_does_not_corrupt(void) {
    HealthCheckReport report;
    memset(&report, 0, sizeof(report));

    // Fill to max with alternating pass/fail
    for (int i = 0; i < HEALTH_CHECK_MAX_ITEMS; i++) {
        HealthCheckResult r = (i % 2 == 0) ? HC_PASS : HC_FAIL;
        char name[24];
        snprintf(name, sizeof(name), "check_%02d", i);
        add_item(&report, name, r, "detail");
    }

    uint8_t passBefore = report.passCount;
    uint8_t failBefore = report.failCount;

    // Attempt overflow with different result types
    add_item(&report, "extra1", HC_WARN, "nope");
    add_item(&report, "extra2", HC_SKIP, "nope");

    // Counts should not change
    TEST_ASSERT_EQUAL_UINT8(passBefore, report.passCount);
    TEST_ASSERT_EQUAL_UINT8(failBefore, report.failCount);
    TEST_ASSERT_EQUAL_UINT8(0, report.warnCount);
    TEST_ASSERT_EQUAL_UINT8(0, report.skipCount);
}

void test_name_truncation(void) {
    HealthCheckReport report;
    memset(&report, 0, sizeof(report));

    // Name longer than 24 chars should be truncated
    add_item(&report, "this_name_is_way_too_long_for_the_field", HC_PASS, "ok");

    // Should be truncated to 23 chars + null terminator
    TEST_ASSERT_EQUAL_UINT8(23, strlen(report.items[0].name));
    TEST_ASSERT_EQUAL_UINT8(1, report.count);
}

void test_detail_truncation(void) {
    HealthCheckReport report;
    memset(&report, 0, sizeof(report));

    // Detail longer than 40 chars should be truncated
    add_item(&report, "check", HC_PASS,
             "this detail string is definitely way too long to fit in the 40 char field");

    // Should be truncated to 39 chars + null terminator
    TEST_ASSERT_EQUAL_UINT8(39, strlen(report.items[0].detail));
}

void test_report_metadata_fields(void) {
    HealthCheckReport report;
    memset(&report, 0, sizeof(report));

    report.timestamp     = 12345;
    report.durationMs    = 42;
    report.deferredPhase = true;

    TEST_ASSERT_EQUAL_UINT32(12345, report.timestamp);
    TEST_ASSERT_EQUAL_UINT32(42, report.durationMs);
    TEST_ASSERT_TRUE(report.deferredPhase);
}

// ===========================================================================
// Section 4: HealthCheckState struct defaults
// ===========================================================================

void test_state_default_values(void) {
    HealthCheckState state = {};

    TEST_ASSERT_EQUAL_UINT8(0, state.lastPassCount);
    TEST_ASSERT_EQUAL_UINT8(0, state.lastFailCount);
    TEST_ASSERT_EQUAL_UINT8(0, state.lastSkipCount);
    TEST_ASSERT_EQUAL_UINT32(0, state.lastCheckDurationMs);
    TEST_ASSERT_EQUAL_UINT32(0, state.lastCheckTimestamp);
    TEST_ASSERT_FALSE(state.deferredComplete);
}

void test_state_field_assignment(void) {
    HealthCheckState state = {};

    state.lastPassCount       = 10;
    state.lastFailCount       = 2;
    state.lastSkipCount       = 3;
    state.lastCheckDurationMs = 150;
    state.lastCheckTimestamp  = 98765;
    state.deferredComplete    = true;

    TEST_ASSERT_EQUAL_UINT8(10, state.lastPassCount);
    TEST_ASSERT_EQUAL_UINT8(2, state.lastFailCount);
    TEST_ASSERT_EQUAL_UINT8(3, state.lastSkipCount);
    TEST_ASSERT_EQUAL_UINT32(150, state.lastCheckDurationMs);
    TEST_ASSERT_EQUAL_UINT32(98765, state.lastCheckTimestamp);
    TEST_ASSERT_TRUE(state.deferredComplete);
}

void test_state_maxval_uint8(void) {
    HealthCheckState state = {};
    state.lastPassCount = 255;
    state.lastFailCount = 255;
    state.lastSkipCount = 255;
    TEST_ASSERT_EQUAL_UINT8(255, state.lastPassCount);
    TEST_ASSERT_EQUAL_UINT8(255, state.lastFailCount);
    TEST_ASSERT_EQUAL_UINT8(255, state.lastSkipCount);
}

// ===========================================================================
// Section 5: AppState integration — dirty flags
// ===========================================================================

void test_dirty_flag_initially_clear(void) {
    TEST_ASSERT_FALSE(appState.isHealthCheckDirty());
}

void test_mark_dirty_sets_flag(void) {
    appState.markHealthCheckDirty();
    TEST_ASSERT_TRUE(appState.isHealthCheckDirty());
}

void test_clear_dirty_resets_flag(void) {
    appState.markHealthCheckDirty();
    TEST_ASSERT_TRUE(appState.isHealthCheckDirty());

    appState.clearHealthCheckDirty();
    TEST_ASSERT_FALSE(appState.isHealthCheckDirty());
}

void test_clear_dirty_idempotent(void) {
    appState.clearHealthCheckDirty();
    appState.clearHealthCheckDirty();
    TEST_ASSERT_FALSE(appState.isHealthCheckDirty());
}

void test_mark_dirty_idempotent(void) {
    appState.markHealthCheckDirty();
    appState.markHealthCheckDirty();
    TEST_ASSERT_TRUE(appState.isHealthCheckDirty());
}

// ===========================================================================
// Section 6: AppState healthCheck fields read/write
// ===========================================================================

void test_appstate_healthcheck_default(void) {
    TEST_ASSERT_EQUAL_UINT8(0, appState.healthCheck.lastPassCount);
    TEST_ASSERT_EQUAL_UINT8(0, appState.healthCheck.lastFailCount);
    TEST_ASSERT_EQUAL_UINT8(0, appState.healthCheck.lastSkipCount);
    TEST_ASSERT_EQUAL_UINT32(0, appState.healthCheck.lastCheckDurationMs);
    TEST_ASSERT_EQUAL_UINT32(0, appState.healthCheck.lastCheckTimestamp);
    TEST_ASSERT_FALSE(appState.healthCheck.deferredComplete);
}

void test_appstate_healthcheck_set_from_report(void) {
    // Simulate what _update_appstate() does in health_check.cpp
    HealthCheckReport report;
    memset(&report, 0, sizeof(report));

    add_item(&report, "heap",    HC_PASS, "ok");
    add_item(&report, "storage", HC_PASS, "ok");
    add_item(&report, "network", HC_FAIL, "disconnected");
    add_item(&report, "mqtt",    HC_SKIP, "not configured");
    report.durationMs    = 42;
    report.timestamp     = 55555;
    report.deferredPhase = true;

    // Replicate _update_appstate logic
    appState.healthCheck.lastPassCount       = report.passCount;
    appState.healthCheck.lastFailCount       = report.failCount;
    appState.healthCheck.lastSkipCount       = report.skipCount;
    appState.healthCheck.lastCheckDurationMs = report.durationMs;
    appState.healthCheck.lastCheckTimestamp  = report.timestamp;
    appState.healthCheck.deferredComplete    = report.deferredPhase;
    appState.markHealthCheckDirty();

    TEST_ASSERT_EQUAL_UINT8(2, appState.healthCheck.lastPassCount);
    TEST_ASSERT_EQUAL_UINT8(1, appState.healthCheck.lastFailCount);
    TEST_ASSERT_EQUAL_UINT8(1, appState.healthCheck.lastSkipCount);
    TEST_ASSERT_EQUAL_UINT32(42, appState.healthCheck.lastCheckDurationMs);
    TEST_ASSERT_EQUAL_UINT32(55555, appState.healthCheck.lastCheckTimestamp);
    TEST_ASSERT_TRUE(appState.healthCheck.deferredComplete);
    TEST_ASSERT_TRUE(appState.isHealthCheckDirty());
}

void test_appstate_healthcheck_reset(void) {
    // Set some values
    appState.healthCheck.lastPassCount       = 10;
    appState.healthCheck.lastFailCount       = 5;
    appState.healthCheck.deferredComplete    = true;
    appState.markHealthCheckDirty();

    // Reset
    appState.healthCheck = HealthCheckState{};
    appState.clearHealthCheckDirty();

    TEST_ASSERT_EQUAL_UINT8(0, appState.healthCheck.lastPassCount);
    TEST_ASSERT_EQUAL_UINT8(0, appState.healthCheck.lastFailCount);
    TEST_ASSERT_FALSE(appState.healthCheck.deferredComplete);
    TEST_ASSERT_FALSE(appState.isHealthCheckDirty());
}

// ===========================================================================
// Section 7: Report items array — ordered access
// ===========================================================================

void test_items_are_stored_in_order(void) {
    HealthCheckReport report;
    memset(&report, 0, sizeof(report));

    add_item(&report, "alpha",   HC_PASS, "first");
    add_item(&report, "bravo",   HC_WARN, "second");
    add_item(&report, "charlie", HC_FAIL, "third");

    TEST_ASSERT_EQUAL_STRING("alpha",   report.items[0].name);
    TEST_ASSERT_EQUAL_STRING("bravo",   report.items[1].name);
    TEST_ASSERT_EQUAL_STRING("charlie", report.items[2].name);

    TEST_ASSERT_EQUAL_STRING("first",  report.items[0].detail);
    TEST_ASSERT_EQUAL_STRING("second", report.items[1].detail);
    TEST_ASSERT_EQUAL_STRING("third",  report.items[2].detail);
}

void test_empty_name_and_detail(void) {
    HealthCheckReport report;
    memset(&report, 0, sizeof(report));

    add_item(&report, "", HC_PASS, "");

    TEST_ASSERT_EQUAL_UINT8(1, report.count);
    TEST_ASSERT_EQUAL_STRING("", report.items[0].name);
    TEST_ASSERT_EQUAL_STRING("", report.items[0].detail);
    TEST_ASSERT_EQUAL_UINT8(HC_PASS, report.items[0].result);
}

// ===========================================================================
// main
// ===========================================================================

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Section 1: Enum values
    RUN_TEST(test_enum_hc_pass_is_zero);
    RUN_TEST(test_enum_hc_warn_is_one);
    RUN_TEST(test_enum_hc_fail_is_two);
    RUN_TEST(test_enum_hc_skip_is_three);

    // Section 2: HealthCheckItem struct
    RUN_TEST(test_item_name_field_size);
    RUN_TEST(test_item_detail_field_size);
    RUN_TEST(test_item_result_field_size);
    RUN_TEST(test_item_zeroed_struct);

    // Section 3: HealthCheckReport capacity and counts
    RUN_TEST(test_max_items_constant);
    RUN_TEST(test_report_zero_initialized);
    RUN_TEST(test_add_single_pass_item);
    RUN_TEST(test_add_single_fail_item);
    RUN_TEST(test_add_single_warn_item);
    RUN_TEST(test_add_single_skip_item);
    RUN_TEST(test_add_mixed_items_counts);
    RUN_TEST(test_add_items_up_to_max);
    RUN_TEST(test_add_items_beyond_max_is_capped);
    RUN_TEST(test_add_items_beyond_max_does_not_corrupt);
    RUN_TEST(test_name_truncation);
    RUN_TEST(test_detail_truncation);
    RUN_TEST(test_report_metadata_fields);

    // Section 4: HealthCheckState defaults
    RUN_TEST(test_state_default_values);
    RUN_TEST(test_state_field_assignment);
    RUN_TEST(test_state_maxval_uint8);

    // Section 5: AppState dirty flags
    RUN_TEST(test_dirty_flag_initially_clear);
    RUN_TEST(test_mark_dirty_sets_flag);
    RUN_TEST(test_clear_dirty_resets_flag);
    RUN_TEST(test_clear_dirty_idempotent);
    RUN_TEST(test_mark_dirty_idempotent);

    // Section 6: AppState healthCheck field integration
    RUN_TEST(test_appstate_healthcheck_default);
    RUN_TEST(test_appstate_healthcheck_set_from_report);
    RUN_TEST(test_appstate_healthcheck_reset);

    // Section 7: Report items array
    RUN_TEST(test_items_are_stored_in_order);
    RUN_TEST(test_empty_name_and_detail);

    return UNITY_END();
}
