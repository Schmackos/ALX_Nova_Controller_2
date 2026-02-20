#include <unity.h>
#include <cstring>
#include <cstdio>

/**
 * Heap Budget Pattern Enforcement Tests
 *
 * These tests verify heap-saving patterns at the source level by reading
 * actual source files and asserting that prohibited patterns (e.g. Arduino
 * String for topic/payload construction) are absent, and that required
 * patterns (e.g. PSRAM allocations, static-buffer helpers) are present.
 *
 * Runs on native platform only — no ESP32 hardware required.
 * File paths are relative to the project root, where PlatformIO runs tests.
 */

// ---------------------------------------------------------------------------
// Helper: count occurrences of a pattern in a file, skipping comment lines
// Returns -1 if the file could not be opened.
// ---------------------------------------------------------------------------
static int count_pattern_in_file(const char* filepath, const char* pattern) {
    FILE* f = fopen(filepath, "r");
    if (!f) return -1;  // file not found
    int count = 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        // Skip single-line comment lines (leading // after optional whitespace)
        const char* trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        if (trimmed[0] == '/' && trimmed[1] == '/') continue;

        const char* pos = line;
        while ((pos = strstr(pos, pattern)) != NULL) {
            count++;
            pos += strlen(pattern);
        }
    }
    fclose(f);
    return count;
}

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// Test: MQTT handler should not use Arduino String for topic/value construction
// ---------------------------------------------------------------------------
// After Phase 2, all topic construction uses mqttTopic() helper with static
// buffers. The only remaining "String " usage should be in local temporary
// variables — never as "String topic" or "String payload".
void test_mqtt_no_string_topic_construction(void) {
    int count = count_pattern_in_file("src/mqtt_handler.cpp", "String topic");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, count,
        "src/mqtt_handler.cpp not found — run tests from project root");
    TEST_ASSERT_EQUAL_MESSAGE(0, count,
        "mqtt_handler.cpp should not use 'String topic' — use mqttTopic() helper");

    count = count_pattern_in_file("src/mqtt_handler.cpp", "String payload");
    TEST_ASSERT_EQUAL_MESSAGE(0, count,
        "mqtt_handler.cpp should not use 'String payload' — use _jsonBuf");
}

// ---------------------------------------------------------------------------
// Test: WebSocket broadcast functions should not use String json
// ---------------------------------------------------------------------------
// After Phase 5, all broadcasts use wsBroadcastJson() or _wsBuf.
// Up to 3 occurrences are allowed (fallback paths inside the helper functions
// wsBroadcastJson, wsSendJson, and audio levels fallback).
void test_ws_no_string_json_broadcasts(void) {
    int count = count_pattern_in_file("src/websocket_handler.cpp", "String json");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, count,
        "src/websocket_handler.cpp not found — run tests from project root");
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(3, count,
        "websocket_handler.cpp should not use 'String json' outside helper fallbacks");
}

// ---------------------------------------------------------------------------
// Test: AppState char[] fields should not have .c_str() calls anywhere
// ---------------------------------------------------------------------------
// All fields listed below are declared as char[] arrays in app_state.h.
// Calling .c_str() on a char array is a compile error — this test documents
// that no such pattern accidentally appears (e.g. from a refactor regression).
void test_appstate_no_cstr_on_char_arrays(void) {
    const char* files[] = {
        "src/mqtt_handler.cpp",
        "src/websocket_handler.cpp",
        "src/settings_manager.cpp",
        "src/wifi_manager.cpp",
        "src/ota_updater.cpp",
        "src/main.cpp",
        "src/auth_handler.cpp"
    };
    const int NUM_FILES = 7;

    // These are the char[] fields in AppState — calling .c_str() on them
    // would be a compile error (char arrays don't have member functions).
    const char* patterns[] = {
        "appState.wifiSSID.c_str()",
        "appState.wifiPassword.c_str()",
        "appState.mqttBroker.c_str()",
        "appState.mqttPassword.c_str()",
        "appState.mqttUsername.c_str()",
        "appState.mqttBaseTopic.c_str()",
        "appState.deviceSerialNumber.c_str()",
        "appState.apSSID.c_str()",
        "appState.apPassword.c_str()",
        "appState.otaStatus.c_str()",
        "appState.otaStatusMessage.c_str()",
        "appState.customDeviceName.c_str()",
        "appState.webPassword.c_str()",
        "appState.wifiNewIP.c_str()",
        "appState.wifiConnectError.c_str()",
        "appState.errorMessage.c_str()"
    };
    const int NUM_PATTERNS = 16;

    for (int f = 0; f < NUM_FILES; f++) {
        for (int p = 0; p < NUM_PATTERNS; p++) {
            int count = count_pattern_in_file(files[f], patterns[p]);
            if (count > 0) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                    "%s contains '%s' — char[] fields don't have .c_str()",
                    files[f], patterns[p]);
                TEST_FAIL_MESSAGE(msg);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Test: Heap budget constants exist in config.h
// ---------------------------------------------------------------------------
void test_heap_budget_constants_exist(void) {
    int count;

    count = count_pattern_in_file("src/config.h", "HEAP_CRITICAL_THRESHOLD_BYTES");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, count,
        "src/config.h not found — run tests from project root");
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, count,
        "config.h missing HEAP_CRITICAL_THRESHOLD_BYTES");

    count = count_pattern_in_file("src/config.h", "HEAP_WARNING_THRESHOLD_BYTES");
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, count,
        "config.h missing HEAP_WARNING_THRESHOLD_BYTES");

    count = count_pattern_in_file("src/config.h", "HEAP_WIFI_RESERVE_BYTES");
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, count,
        "config.h missing HEAP_WIFI_RESERVE_BYTES");
}

// ---------------------------------------------------------------------------
// Test: DMA buffer count is defined in config.h
// ---------------------------------------------------------------------------
void test_dma_buffer_count_reasonable(void) {
    int count = count_pattern_in_file("src/config.h", "I2S_DMA_BUF_COUNT");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, count,
        "src/config.h not found — run tests from project root");
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, count,
        "config.h should define I2S_DMA_BUF_COUNT");
}

// ---------------------------------------------------------------------------
// Test: GUI task stack is allocated from PSRAM
// ---------------------------------------------------------------------------
void test_gui_stack_psram(void) {
    int count = count_pattern_in_file("src/gui/gui_manager.cpp", "MALLOC_CAP_SPIRAM");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, count,
        "src/gui/gui_manager.cpp not found — run tests from project root");
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, count,
        "gui_manager.cpp should allocate stack from PSRAM (MALLOC_CAP_SPIRAM)");
}

// ---------------------------------------------------------------------------
// Test: Debug log ring buffer uses PSRAM
// ---------------------------------------------------------------------------
void test_debug_log_psram(void) {
    int count = count_pattern_in_file("src/debug_serial.cpp", "MALLOC_CAP_SPIRAM");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, count,
        "src/debug_serial.cpp not found — run tests from project root");
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, count,
        "debug_serial.cpp should allocate log buffer from PSRAM (MALLOC_CAP_SPIRAM)");
}

// ---------------------------------------------------------------------------
// Test: WebSocket broadcast buffer uses PSRAM
// ---------------------------------------------------------------------------
void test_ws_buffer_psram(void) {
    int count = count_pattern_in_file("src/websocket_handler.cpp", "MALLOC_CAP_SPIRAM");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, count,
        "src/websocket_handler.cpp not found — run tests from project root");
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, count,
        "websocket_handler.cpp should allocate _wsBuf from PSRAM (MALLOC_CAP_SPIRAM)");
}

// ---------------------------------------------------------------------------
// Test: mqttTopic() helper is used extensively in mqtt_handler.cpp
// ---------------------------------------------------------------------------
void test_mqtt_topic_helper_exists(void) {
    int count = count_pattern_in_file("src/mqtt_handler.cpp", "mqttTopic(");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, count,
        "src/mqtt_handler.cpp not found — run tests from project root");
    TEST_ASSERT_GREATER_THAN_MESSAGE(10, count,
        "mqtt_handler.cpp should use mqttTopic() helper extensively (>10 calls)");
}

// ---------------------------------------------------------------------------
// Test: webPassword field in app_state.h is large enough for a SHA256 hash
// ---------------------------------------------------------------------------
// SHA256 produces a 64-char hex string + null terminator = 65 bytes minimum.
// Regression guard: the field must NOT be declared as [33] (plain-password
// size), which silently truncated the stored hash and broke web login.
void test_webPassword_field_size_sufficient_for_sha256(void) {
    // Must NOT contain the old undersized declaration
    int bad = count_pattern_in_file("src/app_state.h", "webPassword[33]");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, bad,
        "src/app_state.h not found — run tests from project root");
    TEST_ASSERT_EQUAL_MESSAGE(0, bad,
        "app_state.h: webPassword[33] is too small for SHA256 (64 chars + null). "
        "Must be at least webPassword[65].");

    // Must contain a declaration that fits the full 64-char hash
    int good = count_pattern_in_file("src/app_state.h", "webPassword[65]");
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, good,
        "app_state.h: webPassword must be declared as char[65] to hold a full "
        "SHA256 hex hash (64 chars + null terminator).");
}

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------
int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_mqtt_no_string_topic_construction);
    RUN_TEST(test_ws_no_string_json_broadcasts);
    RUN_TEST(test_appstate_no_cstr_on_char_arrays);
    RUN_TEST(test_heap_budget_constants_exist);
    RUN_TEST(test_dma_buffer_count_reasonable);
    RUN_TEST(test_gui_stack_psram);
    RUN_TEST(test_debug_log_psram);
    RUN_TEST(test_ws_buffer_psram);
    RUN_TEST(test_mqtt_topic_helper_exists);
    RUN_TEST(test_webPassword_field_size_sufficient_for_sha256);
    return UNITY_END();
}
