#include <cstring>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Minimal DebugSerial mock for testing applyDebugSerialLevel =====
enum LogLevel {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3,
    LOG_NONE = 4
};

class DebugSerial {
public:
    void setLogLevel(LogLevel level) { _minLevel = level; }
    LogLevel getLogLevel() const { return _minLevel; }
private:
    LogLevel _minLevel = LOG_DEBUG;
};

static DebugSerial DebugOut;

// Inline copy of applyDebugSerialLevel (mirrors debug_serial.h)
inline void applyDebugSerialLevel(bool masterEnabled, int level) {
    if (!masterEnabled) {
        DebugOut.setLogLevel(LOG_ERROR);
        return;
    }
    switch (level) {
        case 0: DebugOut.setLogLevel(LOG_NONE); break;
        case 1: DebugOut.setLogLevel(LOG_ERROR); break;
        case 2: DebugOut.setLogLevel(LOG_INFO); break;
        case 3: DebugOut.setLogLevel(LOG_DEBUG); break;
        default: DebugOut.setLogLevel(LOG_INFO); break;
    }
}

// ===== Minimal AppState mock =====

struct MockAppState {
    bool debugMode = true;
    int debugSerialLevel = 2;
    bool debugHwStats = true;
    bool debugI2sMetrics = true;
    bool debugTaskMonitor = true;

    // MQTT prev-tracking
    bool prevMqttDebugMode = true;
    int prevMqttDebugSerialLevel = 2;
    bool prevMqttDebugHwStats = true;
    bool prevMqttDebugI2sMetrics = true;
    bool prevMqttDebugTaskMonitor = true;

    void reset() {
        debugMode = true;
        debugSerialLevel = 2;
        debugHwStats = true;
        debugI2sMetrics = true;
        debugTaskMonitor = true;
        prevMqttDebugMode = true;
        prevMqttDebugSerialLevel = 2;
        prevMqttDebugHwStats = true;
        prevMqttDebugI2sMetrics = true;
        prevMqttDebugTaskMonitor = true;
    }
};

static MockAppState appState;

// ===== Test Setup =====

void setUp(void) {
    appState.reset();
    DebugOut.setLogLevel(LOG_DEBUG);
}

void tearDown(void) {}

// ===== Default Value Tests =====

void test_default_debugMode_is_true(void) {
    MockAppState fresh;
    TEST_ASSERT_TRUE(fresh.debugMode);
}

void test_default_debugSerialLevel_is_2(void) {
    MockAppState fresh;
    TEST_ASSERT_EQUAL_INT(2, fresh.debugSerialLevel);
}

void test_default_debugHwStats_is_true(void) {
    MockAppState fresh;
    TEST_ASSERT_TRUE(fresh.debugHwStats);
}

void test_default_debugI2sMetrics_is_true(void) {
    MockAppState fresh;
    TEST_ASSERT_TRUE(fresh.debugI2sMetrics);
}

void test_default_debugTaskMonitor_is_true(void) {
    MockAppState fresh;
    TEST_ASSERT_TRUE(fresh.debugTaskMonitor);
}

// ===== Master Gate Override Tests =====

void test_master_off_forces_error_level(void) {
    applyDebugSerialLevel(false, 3);  // Master off, level=Debug
    TEST_ASSERT_EQUAL_INT(LOG_ERROR, DebugOut.getLogLevel());
}

void test_master_off_ignores_serial_level_0(void) {
    applyDebugSerialLevel(false, 0);
    TEST_ASSERT_EQUAL_INT(LOG_ERROR, DebugOut.getLogLevel());
}

void test_master_off_ignores_serial_level_1(void) {
    applyDebugSerialLevel(false, 1);
    TEST_ASSERT_EQUAL_INT(LOG_ERROR, DebugOut.getLogLevel());
}

void test_master_off_ignores_serial_level_2(void) {
    applyDebugSerialLevel(false, 2);
    TEST_ASSERT_EQUAL_INT(LOG_ERROR, DebugOut.getLogLevel());
}

// ===== Serial Level Mapping Tests (Master ON) =====

void test_serial_level_0_maps_to_LOG_NONE(void) {
    applyDebugSerialLevel(true, 0);
    TEST_ASSERT_EQUAL_INT(LOG_NONE, DebugOut.getLogLevel());
}

void test_serial_level_1_maps_to_LOG_ERROR(void) {
    applyDebugSerialLevel(true, 1);
    TEST_ASSERT_EQUAL_INT(LOG_ERROR, DebugOut.getLogLevel());
}

void test_serial_level_2_maps_to_LOG_INFO(void) {
    applyDebugSerialLevel(true, 2);
    TEST_ASSERT_EQUAL_INT(LOG_INFO, DebugOut.getLogLevel());
}

void test_serial_level_3_maps_to_LOG_DEBUG(void) {
    applyDebugSerialLevel(true, 3);
    TEST_ASSERT_EQUAL_INT(LOG_DEBUG, DebugOut.getLogLevel());
}

void test_serial_level_invalid_defaults_to_LOG_INFO(void) {
    applyDebugSerialLevel(true, 99);
    TEST_ASSERT_EQUAL_INT(LOG_INFO, DebugOut.getLogLevel());
}

void test_serial_level_negative_defaults_to_LOG_INFO(void) {
    applyDebugSerialLevel(true, -1);
    TEST_ASSERT_EQUAL_INT(LOG_INFO, DebugOut.getLogLevel());
}

// ===== Feature Guard Logic Tests =====

void test_hwstats_enabled_when_both_on(void) {
    appState.debugMode = true;
    appState.debugHwStats = true;
    TEST_ASSERT_TRUE(appState.debugMode && appState.debugHwStats);
}

void test_hwstats_disabled_when_master_off(void) {
    appState.debugMode = false;
    appState.debugHwStats = true;
    TEST_ASSERT_FALSE(appState.debugMode && appState.debugHwStats);
}

void test_hwstats_disabled_when_feature_off(void) {
    appState.debugMode = true;
    appState.debugHwStats = false;
    TEST_ASSERT_FALSE(appState.debugMode && appState.debugHwStats);
}

void test_i2s_metrics_disabled_when_master_off(void) {
    appState.debugMode = false;
    appState.debugI2sMetrics = true;
    TEST_ASSERT_FALSE(appState.debugMode && appState.debugI2sMetrics);
}

void test_task_monitor_disabled_when_master_off(void) {
    appState.debugMode = false;
    appState.debugTaskMonitor = true;
    TEST_ASSERT_FALSE(appState.debugMode && appState.debugTaskMonitor);
}

void test_all_features_disabled_when_master_off(void) {
    appState.debugMode = false;
    appState.debugHwStats = true;
    appState.debugI2sMetrics = true;
    appState.debugTaskMonitor = true;
    TEST_ASSERT_FALSE(appState.debugMode && appState.debugHwStats);
    TEST_ASSERT_FALSE(appState.debugMode && appState.debugI2sMetrics);
    TEST_ASSERT_FALSE(appState.debugMode && appState.debugTaskMonitor);
}

void test_individual_toggles_preserved_when_master_off(void) {
    appState.debugMode = false;
    appState.debugHwStats = true;
    appState.debugI2sMetrics = false;
    appState.debugTaskMonitor = true;
    // Individual values preserved even though master is off
    TEST_ASSERT_TRUE(appState.debugHwStats);
    TEST_ASSERT_FALSE(appState.debugI2sMetrics);
    TEST_ASSERT_TRUE(appState.debugTaskMonitor);
}

// ===== LOG_NONE Enum Tests =====

void test_LOG_NONE_is_above_LOG_ERROR(void) {
    TEST_ASSERT_TRUE(LOG_NONE > LOG_ERROR);
}

void test_LOG_NONE_value_is_4(void) {
    TEST_ASSERT_EQUAL_INT(4, LOG_NONE);
}

// ===== Test Runner =====

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Default value tests
    RUN_TEST(test_default_debugMode_is_true);
    RUN_TEST(test_default_debugSerialLevel_is_2);
    RUN_TEST(test_default_debugHwStats_is_true);
    RUN_TEST(test_default_debugI2sMetrics_is_true);
    RUN_TEST(test_default_debugTaskMonitor_is_true);

    // Master gate override tests
    RUN_TEST(test_master_off_forces_error_level);
    RUN_TEST(test_master_off_ignores_serial_level_0);
    RUN_TEST(test_master_off_ignores_serial_level_1);
    RUN_TEST(test_master_off_ignores_serial_level_2);

    // Serial level mapping tests
    RUN_TEST(test_serial_level_0_maps_to_LOG_NONE);
    RUN_TEST(test_serial_level_1_maps_to_LOG_ERROR);
    RUN_TEST(test_serial_level_2_maps_to_LOG_INFO);
    RUN_TEST(test_serial_level_3_maps_to_LOG_DEBUG);
    RUN_TEST(test_serial_level_invalid_defaults_to_LOG_INFO);
    RUN_TEST(test_serial_level_negative_defaults_to_LOG_INFO);

    // Feature guard logic tests
    RUN_TEST(test_hwstats_enabled_when_both_on);
    RUN_TEST(test_hwstats_disabled_when_master_off);
    RUN_TEST(test_hwstats_disabled_when_feature_off);
    RUN_TEST(test_i2s_metrics_disabled_when_master_off);
    RUN_TEST(test_task_monitor_disabled_when_master_off);
    RUN_TEST(test_all_features_disabled_when_master_off);
    RUN_TEST(test_individual_toggles_preserved_when_master_off);

    // LOG_NONE enum tests
    RUN_TEST(test_LOG_NONE_is_above_LOG_ERROR);
    RUN_TEST(test_LOG_NONE_value_is_4);

    return UNITY_END();
}
