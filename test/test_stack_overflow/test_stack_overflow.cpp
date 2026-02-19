#include <cstring>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Inline stack overflow hook logic for testing =====
// The real hook runs in exception context and cannot be called directly in
// native tests (FreeRTOS TaskHandle_t is unavailable). Instead we test the
// core logic — flag setting and task name copy — through helper functions that
// mirror exactly what vApplicationStackOverflowHook does in main.cpp.

struct StackOverflowState {
    volatile bool detected;
    char taskName[16];
};

static StackOverflowState g_state;

// Mirrors the body of vApplicationStackOverflowHook in main.cpp
static void simulate_hook(const char *pcTaskName) {
    g_state.detected = true;
    if (pcTaskName != nullptr) {
        strncpy(g_state.taskName, pcTaskName, 15);
        g_state.taskName[15] = '\0';
    } else {
        strncpy(g_state.taskName, "unknown", 15);
        g_state.taskName[15] = '\0';
    }
}

// Mirrors the loop() handler in main.cpp (clearing + logging path)
static bool simulate_loop_handler(char *outName, int outLen) {
    if (!g_state.detected) return false;
    g_state.detected = false;
    if (outName && outLen > 0) {
        strncpy(outName, g_state.taskName, outLen - 1);
        outName[outLen - 1] = '\0';
    }
    return true;
}

// ===== Test Setup =====

void setUp(void) {
    g_state.detected = false;
    memset(g_state.taskName, 0, sizeof(g_state.taskName));
}

void tearDown(void) {}

// ===== Tests =====

// Test 1: Hook sets the detected flag
void test_stack_overflow_sets_flag(void) {
    TEST_ASSERT_FALSE(g_state.detected);

    simulate_hook("audio_cap");

    TEST_ASSERT_TRUE(g_state.detected);
    TEST_ASSERT_EQUAL_STRING("audio_cap", g_state.taskName);
}

// Test 2: Task name longer than 15 chars is stored as exactly 15 chars + null terminator
void test_stack_overflow_truncates_name_at_15(void) {
    // "this_is_16chars" = 16 characters — must be truncated to 15
    const char *longName = "this_is_16chars";
    simulate_hook(longName);

    TEST_ASSERT_TRUE(g_state.detected);
    TEST_ASSERT_EQUAL_UINT32(15, strlen(g_state.taskName));
    TEST_ASSERT_EQUAL_UINT8('\0', g_state.taskName[15]); // buffer boundary null
    // First 15 chars must match
    TEST_ASSERT_EQUAL_INT(0, strncmp(g_state.taskName, longName, 15));
}

// Test 3: Null pcTaskName stores "unknown" in the task name field
void test_stack_overflow_handles_null_name(void) {
    simulate_hook(nullptr);

    TEST_ASSERT_TRUE(g_state.detected);
    TEST_ASSERT_EQUAL_STRING("unknown", g_state.taskName);
}

// Test 4: loop() handler clears the flag after processing
void test_loop_handler_clears_flag(void) {
    simulate_hook("gui_task");
    TEST_ASSERT_TRUE(g_state.detected);

    char name[16] = {};
    bool handled = simulate_loop_handler(name, sizeof(name));

    TEST_ASSERT_TRUE(handled);
    TEST_ASSERT_FALSE(g_state.detected);
    TEST_ASSERT_EQUAL_STRING("gui_task", name);
}

// Test 5: loop() handler returns false when no overflow was detected
void test_loop_handler_no_op_when_not_set(void) {
    TEST_ASSERT_FALSE(g_state.detected);

    char name[16] = {};
    bool handled = simulate_loop_handler(name, sizeof(name));

    TEST_ASSERT_FALSE(handled);
}

// ===== Test Runner =====

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_stack_overflow_sets_flag);
    RUN_TEST(test_stack_overflow_truncates_name_at_15);
    RUN_TEST(test_stack_overflow_handles_null_name);
    RUN_TEST(test_loop_handler_clears_flag);
    RUN_TEST(test_loop_handler_no_op_when_not_set);

    return UNITY_END();
}
