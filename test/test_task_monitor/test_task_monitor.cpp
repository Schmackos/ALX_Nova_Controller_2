#include <cstring>
#include <string>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// Include the task_monitor implementation directly (test_build_src = no)
// Config constants and LOG macros are handled by NATIVE_TEST guards in the .cpp
#include "../../src/task_monitor.h"
#include "../../src/task_monitor.cpp"

// ===== Test helpers =====

static void reset_all() {
    ArduinoMock::reset();
    task_monitor_init();
}

// ===== Test Setup =====

void setUp(void) {
    reset_all();
}

void tearDown(void) {}

// ===== Tests =====

void test_lookup_known_task_loopTask(void) {
    uint32_t alloc = task_monitor_lookup_stack_alloc("loopTask");
    TEST_ASSERT_EQUAL_UINT32(8192, alloc);
}

void test_lookup_known_task_audio_cap(void) {
    uint32_t alloc = task_monitor_lookup_stack_alloc("audio_cap");
    TEST_ASSERT_EQUAL_UINT32(TASK_STACK_SIZE_AUDIO, alloc);
}

void test_lookup_known_task_OTA_DL(void) {
    uint32_t alloc = task_monitor_lookup_stack_alloc("OTA_DL");
    TEST_ASSERT_EQUAL_UINT32(TASK_STACK_SIZE_OTA, alloc);
}

void test_lookup_unknown_task_returns_zero(void) {
    uint32_t alloc = task_monitor_lookup_stack_alloc("unknown_task");
    TEST_ASSERT_EQUAL_UINT32(0, alloc);
}

void test_state_name_running(void) {
    TEST_ASSERT_EQUAL_STRING("R", task_monitor_state_name(0));
}

void test_state_name_ready(void) {
    TEST_ASSERT_EQUAL_STRING("r", task_monitor_state_name(1));
}

void test_state_name_blocked(void) {
    TEST_ASSERT_EQUAL_STRING("B", task_monitor_state_name(2));
}

void test_state_name_suspended(void) {
    TEST_ASSERT_EQUAL_STRING("S", task_monitor_state_name(3));
}

void test_state_name_deleted(void) {
    TEST_ASSERT_EQUAL_STRING("D", task_monitor_state_name(4));
}

void test_state_name_invalid(void) {
    TEST_ASSERT_EQUAL_STRING("?", task_monitor_state_name(99));
}

void test_sort_by_priority_descending(void) {
    TaskInfo tasks[4];
    memset(tasks, 0, sizeof(tasks));

    strcpy(tasks[0].name, "low");
    tasks[0].priority = 0;
    strcpy(tasks[1].name, "high");
    tasks[1].priority = 3;
    strcpy(tasks[2].name, "mid");
    tasks[2].priority = 1;
    strcpy(tasks[3].name, "med");
    tasks[3].priority = 2;

    task_monitor_sort_by_priority(tasks, 4);

    TEST_ASSERT_EQUAL_UINT8(3, tasks[0].priority);
    TEST_ASSERT_EQUAL_STRING("high", tasks[0].name);
    TEST_ASSERT_EQUAL_UINT8(2, tasks[1].priority);
    TEST_ASSERT_EQUAL_STRING("med", tasks[1].name);
    TEST_ASSERT_EQUAL_UINT8(1, tasks[2].priority);
    TEST_ASSERT_EQUAL_STRING("mid", tasks[2].name);
    TEST_ASSERT_EQUAL_UINT8(0, tasks[3].priority);
    TEST_ASSERT_EQUAL_STRING("low", tasks[3].name);
}

void test_sort_single_element(void) {
    TaskInfo tasks[1];
    memset(tasks, 0, sizeof(tasks));
    strcpy(tasks[0].name, "only");
    tasks[0].priority = 5;

    task_monitor_sort_by_priority(tasks, 1);

    TEST_ASSERT_EQUAL_STRING("only", tasks[0].name);
    TEST_ASSERT_EQUAL_UINT8(5, tasks[0].priority);
}

void test_sort_already_sorted(void) {
    TaskInfo tasks[3];
    memset(tasks, 0, sizeof(tasks));
    strcpy(tasks[0].name, "a");
    tasks[0].priority = 10;
    strcpy(tasks[1].name, "b");
    tasks[1].priority = 5;
    strcpy(tasks[2].name, "c");
    tasks[2].priority = 1;

    task_monitor_sort_by_priority(tasks, 3);

    TEST_ASSERT_EQUAL_UINT8(10, tasks[0].priority);
    TEST_ASSERT_EQUAL_UINT8(5, tasks[1].priority);
    TEST_ASSERT_EQUAL_UINT8(1, tasks[2].priority);
}

void test_loop_timing_basic(void) {
    ArduinoMock::mockMicros = 1000;
    task_monitor_loop_start();

    ArduinoMock::mockMicros = 1450;
    task_monitor_loop_end();

    const TaskMonitorData& data = task_monitor_get_data();
    TEST_ASSERT_EQUAL_UINT32(450, data.loopTimeUs);
}

void test_loop_timing_max_tracking(void) {
    // First iteration: 100us
    ArduinoMock::mockMicros = 0;
    task_monitor_loop_start();
    ArduinoMock::mockMicros = 100;
    task_monitor_loop_end();

    // Second iteration: 500us
    ArduinoMock::mockMicros = 1000;
    task_monitor_loop_start();
    ArduinoMock::mockMicros = 1500;
    task_monitor_loop_end();

    // Third iteration: 200us
    ArduinoMock::mockMicros = 2000;
    task_monitor_loop_start();
    ArduinoMock::mockMicros = 2200;
    task_monitor_loop_end();

    const TaskMonitorData& data = task_monitor_get_data();
    TEST_ASSERT_EQUAL_UINT32(500, data.loopTimeMaxUs);
    TEST_ASSERT_EQUAL_UINT32(200, data.loopTimeUs); // Last iteration
}

void test_loop_timing_no_end_without_start(void) {
    // Call end without start — should not crash or update
    task_monitor_loop_end();
    const TaskMonitorData& data = task_monitor_get_data();
    TEST_ASSERT_EQUAL_UINT32(0, data.loopTimeUs);
}

void test_init_resets_data(void) {
    // Accumulate some data
    ArduinoMock::mockMicros = 0;
    task_monitor_loop_start();
    ArduinoMock::mockMicros = 999;
    task_monitor_loop_end();

    // Re-init
    task_monitor_init();

    const TaskMonitorData& data = task_monitor_get_data();
    TEST_ASSERT_EQUAL_UINT32(0, data.loopTimeUs);
    TEST_ASSERT_EQUAL_UINT32(0, data.loopTimeMaxUs);
    TEST_ASSERT_EQUAL_UINT32(0, data.loopTimeAvgUs);
    TEST_ASSERT_EQUAL_UINT8(0, data.taskCount);
}

// ===== Test Runner =====

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Stack lookup tests
    RUN_TEST(test_lookup_known_task_loopTask);
    RUN_TEST(test_lookup_known_task_audio_cap);
    RUN_TEST(test_lookup_known_task_OTA_DL);
    RUN_TEST(test_lookup_unknown_task_returns_zero);

    // State name tests
    RUN_TEST(test_state_name_running);
    RUN_TEST(test_state_name_ready);
    RUN_TEST(test_state_name_blocked);
    RUN_TEST(test_state_name_suspended);
    RUN_TEST(test_state_name_deleted);
    RUN_TEST(test_state_name_invalid);

    // Sort tests
    RUN_TEST(test_sort_by_priority_descending);
    RUN_TEST(test_sort_single_element);
    RUN_TEST(test_sort_already_sorted);

    // Loop timing tests
    RUN_TEST(test_loop_timing_basic);
    RUN_TEST(test_loop_timing_max_tracking);
    RUN_TEST(test_loop_timing_no_end_without_start);
    RUN_TEST(test_init_resets_data);

    return UNITY_END();
}
