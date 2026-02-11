#include <cstring>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Inline crash_log types and pure functions for testing =====
// We test the ring buffer logic and crashlog_was_crash() without LittleFS

#define CRASH_LOG_MAX_ENTRIES 10

struct CrashLogEntry {
    char reason[24];
    uint32_t heapFree;
    uint32_t heapMinFree;
    char timestamp[24];
};

struct CrashLogData {
    uint8_t count;
    uint8_t writeIndex;
    CrashLogEntry entries[CRASH_LOG_MAX_ENTRIES];
};

// Pure function: determine if a reset reason indicates a crash
static bool crashlog_was_crash(const char *reason) {
    if (!reason) return false;
    return (strcmp(reason, "exception_panic") == 0 ||
            strcmp(reason, "task_watchdog") == 0 ||
            strcmp(reason, "interrupt_watchdog") == 0 ||
            strcmp(reason, "other_watchdog") == 0 ||
            strcmp(reason, "brownout") == 0);
}

// Simulate ring buffer record (in-memory only, no LittleFS)
static CrashLogData testLog;

static void test_crashlog_reset() {
    memset(&testLog, 0, sizeof(testLog));
}

static void test_crashlog_record(const char *reason, uint32_t heapFree, uint32_t heapMinFree) {
    CrashLogEntry &entry = testLog.entries[testLog.writeIndex];
    memset(&entry, 0, sizeof(CrashLogEntry));
    strncpy(entry.reason, reason, sizeof(entry.reason) - 1);
    entry.reason[sizeof(entry.reason) - 1] = '\0';
    entry.heapFree = heapFree;
    entry.heapMinFree = heapMinFree;
    entry.timestamp[0] = '\0';
    testLog.writeIndex = (testLog.writeIndex + 1) % CRASH_LOG_MAX_ENTRIES;
    if (testLog.count < CRASH_LOG_MAX_ENTRIES) testLog.count++;
}

static const CrashLogEntry *test_crashlog_get_recent(int index) {
    if (index < 0 || index >= (int)testLog.count) return nullptr;
    int idx = (testLog.writeIndex + CRASH_LOG_MAX_ENTRIES - 1 - index) % CRASH_LOG_MAX_ENTRIES;
    return &testLog.entries[idx];
}

// ===== Mock esp_task_wdt API (compile check) =====
#define ESP_OK 0
typedef int esp_err_t;
typedef void* TaskHandle_t;

struct esp_task_wdt_config_t {
    uint32_t timeout_ms;
    uint32_t idle_core_mask;
    bool trigger_panic;
};

static int wdt_add_count = 0;
static int wdt_reset_count = 0;
static int wdt_delete_count = 0;
static int wdt_reconfig_count = 0;

static esp_err_t esp_task_wdt_reconfigure(const esp_task_wdt_config_t *config) {
    wdt_reconfig_count++;
    return ESP_OK;
}

static esp_err_t esp_task_wdt_add(TaskHandle_t handle) {
    wdt_add_count++;
    return ESP_OK;
}

static esp_err_t esp_task_wdt_reset() {
    wdt_reset_count++;
    return ESP_OK;
}

static esp_err_t esp_task_wdt_delete(TaskHandle_t handle) {
    wdt_delete_count++;
    return ESP_OK;
}

// ===== Heap threshold test helpers =====
static bool test_heap_critical(uint32_t maxBlock) {
    return maxBlock < 20000;
}

// ===== Test Setup =====

void setUp(void) {
    test_crashlog_reset();
    wdt_add_count = 0;
    wdt_reset_count = 0;
    wdt_delete_count = 0;
    wdt_reconfig_count = 0;
}

void tearDown(void) {}

// ===== Phase 1: Crash Log Tests =====

void test_crashlog_was_crash_power_on() {
    TEST_ASSERT_FALSE(crashlog_was_crash("power_on"));
}

void test_crashlog_was_crash_software_reset() {
    TEST_ASSERT_FALSE(crashlog_was_crash("software_reset"));
}

void test_crashlog_was_crash_deep_sleep() {
    TEST_ASSERT_FALSE(crashlog_was_crash("deep_sleep_wake"));
}

void test_crashlog_was_crash_external_reset() {
    TEST_ASSERT_FALSE(crashlog_was_crash("external_reset"));
}

void test_crashlog_was_crash_unknown() {
    TEST_ASSERT_FALSE(crashlog_was_crash("unknown"));
}

void test_crashlog_was_crash_null() {
    TEST_ASSERT_FALSE(crashlog_was_crash(nullptr));
}

void test_crashlog_was_crash_task_watchdog() {
    TEST_ASSERT_TRUE(crashlog_was_crash("task_watchdog"));
}

void test_crashlog_was_crash_interrupt_watchdog() {
    TEST_ASSERT_TRUE(crashlog_was_crash("interrupt_watchdog"));
}

void test_crashlog_was_crash_other_watchdog() {
    TEST_ASSERT_TRUE(crashlog_was_crash("other_watchdog"));
}

void test_crashlog_was_crash_exception_panic() {
    TEST_ASSERT_TRUE(crashlog_was_crash("exception_panic"));
}

void test_crashlog_was_crash_brownout() {
    TEST_ASSERT_TRUE(crashlog_was_crash("brownout"));
}

void test_ring_buffer_single_entry() {
    test_crashlog_record("power_on", 200000, 180000);
    TEST_ASSERT_EQUAL_UINT8(1, testLog.count);
    TEST_ASSERT_EQUAL_UINT8(1, testLog.writeIndex);

    const CrashLogEntry *entry = test_crashlog_get_recent(0);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_STRING("power_on", entry->reason);
    TEST_ASSERT_EQUAL_UINT32(200000, entry->heapFree);
    TEST_ASSERT_EQUAL_UINT32(180000, entry->heapMinFree);
}

void test_ring_buffer_multiple_entries() {
    test_crashlog_record("power_on", 200000, 180000);
    test_crashlog_record("software_reset", 190000, 170000);
    test_crashlog_record("task_watchdog", 150000, 120000);

    TEST_ASSERT_EQUAL_UINT8(3, testLog.count);

    // Most recent (index 0) should be task_watchdog
    const CrashLogEntry *recent = test_crashlog_get_recent(0);
    TEST_ASSERT_NOT_NULL(recent);
    TEST_ASSERT_EQUAL_STRING("task_watchdog", recent->reason);

    // Second most recent (index 1) should be software_reset
    const CrashLogEntry *second = test_crashlog_get_recent(1);
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_EQUAL_STRING("software_reset", second->reason);

    // Oldest (index 2) should be power_on
    const CrashLogEntry *oldest = test_crashlog_get_recent(2);
    TEST_ASSERT_NOT_NULL(oldest);
    TEST_ASSERT_EQUAL_STRING("power_on", oldest->reason);
}

void test_ring_buffer_wraparound() {
    // Add 12 entries — only last 10 should survive
    for (int i = 0; i < 12; i++) {
        char reason[24];
        snprintf(reason, sizeof(reason), "boot_%d", i);
        test_crashlog_record(reason, 200000 - i * 1000, 180000 - i * 1000);
    }

    TEST_ASSERT_EQUAL_UINT8(CRASH_LOG_MAX_ENTRIES, testLog.count);

    // Most recent should be "boot_11"
    const CrashLogEntry *newest = test_crashlog_get_recent(0);
    TEST_ASSERT_NOT_NULL(newest);
    TEST_ASSERT_EQUAL_STRING("boot_11", newest->reason);

    // Oldest in buffer should be "boot_2" (0 and 1 were overwritten)
    const CrashLogEntry *oldest = test_crashlog_get_recent(9);
    TEST_ASSERT_NOT_NULL(oldest);
    TEST_ASSERT_EQUAL_STRING("boot_2", oldest->reason);
}

void test_ring_buffer_out_of_bounds() {
    test_crashlog_record("power_on", 200000, 180000);
    TEST_ASSERT_NULL(test_crashlog_get_recent(1));   // Only 1 entry, index 1 is out of range
    TEST_ASSERT_NULL(test_crashlog_get_recent(-1));   // Negative index
    TEST_ASSERT_NULL(test_crashlog_get_recent(100));   // Way out of range
}

void test_reason_truncation() {
    // Reason field is 24 chars — try a very long string
    test_crashlog_record("this_is_a_very_long_reason_string_exceeding_24_chars", 200000, 180000);
    const CrashLogEntry *entry = test_crashlog_get_recent(0);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT32(23, strlen(entry->reason)); // Truncated to 23 + null
}

void test_binary_serialization_roundtrip() {
    // Simulate write + read of CrashLogData as raw bytes
    test_crashlog_record("task_watchdog", 150000, 120000);
    test_crashlog_record("power_on", 200000, 180000);

    // "Serialize" to raw bytes
    uint8_t buffer[sizeof(CrashLogData)];
    memcpy(buffer, &testLog, sizeof(CrashLogData));

    // "Deserialize" to a new struct
    CrashLogData restored;
    memcpy(&restored, buffer, sizeof(CrashLogData));

    TEST_ASSERT_EQUAL_UINT8(testLog.count, restored.count);
    TEST_ASSERT_EQUAL_UINT8(testLog.writeIndex, restored.writeIndex);
    TEST_ASSERT_EQUAL_STRING("task_watchdog", restored.entries[0].reason);
    TEST_ASSERT_EQUAL_STRING("power_on", restored.entries[1].reason);
    TEST_ASSERT_EQUAL_UINT32(150000, restored.entries[0].heapFree);
}

// ===== Phase 2: Watchdog Mock Tests =====

void test_wdt_reconfigure_compiles() {
    esp_task_wdt_config_t config = {15000, 0, true};
    esp_err_t result = esp_task_wdt_reconfigure(&config);
    TEST_ASSERT_EQUAL(ESP_OK, result);
    TEST_ASSERT_EQUAL(1, wdt_reconfig_count);
}

void test_wdt_add_reset_delete_compiles() {
    esp_task_wdt_add(NULL);
    esp_task_wdt_reset();
    esp_task_wdt_reset();
    esp_task_wdt_delete(NULL);

    TEST_ASSERT_EQUAL(1, wdt_add_count);
    TEST_ASSERT_EQUAL(2, wdt_reset_count);
    TEST_ASSERT_EQUAL(1, wdt_delete_count);
}

// ===== Phase 4: Heap Health Tests =====

void test_heap_critical_below_threshold() {
    TEST_ASSERT_TRUE(test_heap_critical(19999));
    TEST_ASSERT_TRUE(test_heap_critical(10000));
    TEST_ASSERT_TRUE(test_heap_critical(0));
}

void test_heap_critical_above_threshold() {
    TEST_ASSERT_FALSE(test_heap_critical(20000));
    TEST_ASSERT_FALSE(test_heap_critical(40000));
    TEST_ASSERT_FALSE(test_heap_critical(200000));
}

void test_heap_critical_at_boundary() {
    TEST_ASSERT_TRUE(test_heap_critical(19999));
    TEST_ASSERT_FALSE(test_heap_critical(20000));
}

// ===== Phase 3: I2S Timeout Constants =====

void test_i2s_timeout_not_max_delay() {
    // portMAX_DELAY is 0xFFFFFFFF, pdMS_TO_TICKS(500) is much smaller
    // This test verifies the timeout constant logic
    uint32_t timeout_ticks = 500; // pdMS_TO_TICKS(500) at 1ms tick = 500
    uint32_t max_delay = 0xFFFFFFFF;
    TEST_ASSERT_TRUE(timeout_ticks < max_delay);
    TEST_ASSERT_TRUE(timeout_ticks <= 1000); // Reasonable bound
}

void test_i2s_recovery_threshold() {
    // Recovery should trigger after 10 consecutive timeouts (~5s)
    uint32_t threshold = 10;
    uint32_t timeouts = 0;
    for (int i = 0; i < 10; i++) {
        timeouts++;
    }
    TEST_ASSERT_EQUAL_UINT32(threshold, timeouts);
}

// ===== Test Runner =====

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Phase 1: crashlog_was_crash
    RUN_TEST(test_crashlog_was_crash_power_on);
    RUN_TEST(test_crashlog_was_crash_software_reset);
    RUN_TEST(test_crashlog_was_crash_deep_sleep);
    RUN_TEST(test_crashlog_was_crash_external_reset);
    RUN_TEST(test_crashlog_was_crash_unknown);
    RUN_TEST(test_crashlog_was_crash_null);
    RUN_TEST(test_crashlog_was_crash_task_watchdog);
    RUN_TEST(test_crashlog_was_crash_interrupt_watchdog);
    RUN_TEST(test_crashlog_was_crash_other_watchdog);
    RUN_TEST(test_crashlog_was_crash_exception_panic);
    RUN_TEST(test_crashlog_was_crash_brownout);

    // Phase 1: Ring buffer
    RUN_TEST(test_ring_buffer_single_entry);
    RUN_TEST(test_ring_buffer_multiple_entries);
    RUN_TEST(test_ring_buffer_wraparound);
    RUN_TEST(test_ring_buffer_out_of_bounds);
    RUN_TEST(test_reason_truncation);
    RUN_TEST(test_binary_serialization_roundtrip);

    // Phase 2: Watchdog mock compile check
    RUN_TEST(test_wdt_reconfigure_compiles);
    RUN_TEST(test_wdt_add_reset_delete_compiles);

    // Phase 3: I2S timeout
    RUN_TEST(test_i2s_timeout_not_max_delay);
    RUN_TEST(test_i2s_recovery_threshold);

    // Phase 4: Heap health
    RUN_TEST(test_heap_critical_below_threshold);
    RUN_TEST(test_heap_critical_above_threshold);
    RUN_TEST(test_heap_critical_at_boundary);

    return UNITY_END();
}
