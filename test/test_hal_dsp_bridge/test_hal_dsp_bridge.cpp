/**
 * test_hal_dsp_bridge.cpp
 *
 * Tests for HalDspBridge -- the DSP Pipeline HAL device driver.
 * Verifies descriptor fields, lifecycle (probe/init/deinit/healthCheck),
 * and HalAudioDspInterface stubs (dspIsActive, dspSetBypassed,
 * dspGetInputLevel, dspGetOutputLevel).
 *
 * Under NATIVE_TEST all DSP/audio functions are compiled out, so the
 * level getters return 0.0f and dspIsActive() returns false.
 */

#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Inline HAL implementation for native testing =====
#include "../../src/hal/hal_types.h"
#include "../../src/hal/hal_device.h"
#include "../../src/hal/hal_audio_interfaces.h"

// hal_dsp_bridge.cpp includes hal_device_manager.h -- provide the full chain
#include "../test_mocks/Preferences.h"
#include "../test_mocks/LittleFS.h"
#include "../../src/diag_journal.cpp"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"

// Inline the driver under test (has NATIVE_TEST stubs)
#include "../../src/hal/hal_dsp_bridge.cpp"

// ===== Fixtures =====

static HalDspBridge* dev = nullptr;

void setUp() {
    ArduinoMock::reset();
    dev = new HalDspBridge();
}

void tearDown() {
    if (dev) {
        delete dev;
        dev = nullptr;
    }
}

// =====================================================================
// Group 1: Descriptor fields
// =====================================================================

void test_dsp_bridge_compatible_string() {
    TEST_ASSERT_EQUAL_STRING("alx,dsp-pipeline", dev->getDescriptor().compatible);
}

void test_dsp_bridge_name() {
    TEST_ASSERT_EQUAL_STRING("DSP Pipeline", dev->getDescriptor().name);
}

void test_dsp_bridge_manufacturer() {
    TEST_ASSERT_EQUAL_STRING("ALX Audio", dev->getDescriptor().manufacturer);
}

void test_dsp_bridge_type() {
    TEST_ASSERT_EQUAL(HAL_DEV_DSP, dev->getDescriptor().type);
}

void test_dsp_bridge_bus_internal() {
    TEST_ASSERT_EQUAL(HAL_BUS_INTERNAL, dev->getDescriptor().bus.type);
}

void test_dsp_bridge_channel_count() {
    TEST_ASSERT_EQUAL(4, dev->getDescriptor().channelCount);
}

// =====================================================================
// Group 2: Lifecycle
// =====================================================================

void test_dsp_bridge_probe_always_true() {
    TEST_ASSERT_TRUE(dev->probe());
}

void test_dsp_bridge_init_sets_available() {
    HalInitResult res = dev->init();
    TEST_ASSERT_TRUE(res.success);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, dev->_state);
    TEST_ASSERT_TRUE(dev->_ready);
}

void test_dsp_bridge_deinit_sets_removed() {
    dev->init();
    dev->deinit();
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, dev->_state);
    TEST_ASSERT_FALSE(dev->_ready);
}

void test_dsp_bridge_health_check_true() {
    TEST_ASSERT_TRUE(dev->healthCheck());
}

// =====================================================================
// Group 3: HalAudioDspInterface — native stubs
// =====================================================================

void test_dsp_is_active_returns_false_native() {
    TEST_ASSERT_FALSE(dev->dspIsActive());
}

void test_dsp_set_bypassed_returns_true_native() {
    TEST_ASSERT_TRUE(dev->dspSetBypassed(true));
    TEST_ASSERT_TRUE(dev->dspSetBypassed(false));
}

void test_dsp_get_input_level_returns_zero_all_lanes() {
    for (uint8_t lane = 0; lane < 8; lane++) {
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, dev->dspGetInputLevel(lane));
    }
}

void test_dsp_get_output_level_returns_zero_all_lanes() {
    for (uint8_t lane = 0; lane < 8; lane++) {
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, dev->dspGetOutputLevel(lane));
    }
}

void test_dsp_get_input_level_oob_returns_zero() {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, dev->dspGetInputLevel(255));
}

void test_dsp_get_output_level_oob_returns_zero() {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, dev->dspGetOutputLevel(255));
}

// =====================================================================
// main
// =====================================================================

int main() {
    UNITY_BEGIN();

    // Group 1: Descriptor
    RUN_TEST(test_dsp_bridge_compatible_string);
    RUN_TEST(test_dsp_bridge_name);
    RUN_TEST(test_dsp_bridge_manufacturer);
    RUN_TEST(test_dsp_bridge_type);
    RUN_TEST(test_dsp_bridge_bus_internal);
    RUN_TEST(test_dsp_bridge_channel_count);

    // Group 2: Lifecycle
    RUN_TEST(test_dsp_bridge_probe_always_true);
    RUN_TEST(test_dsp_bridge_init_sets_available);
    RUN_TEST(test_dsp_bridge_deinit_sets_removed);
    RUN_TEST(test_dsp_bridge_health_check_true);

    // Group 3: DSP interface stubs
    RUN_TEST(test_dsp_is_active_returns_false_native);
    RUN_TEST(test_dsp_set_bypassed_returns_true_native);
    RUN_TEST(test_dsp_get_input_level_returns_zero_all_lanes);
    RUN_TEST(test_dsp_get_output_level_returns_zero_all_lanes);
    RUN_TEST(test_dsp_get_input_level_oob_returns_zero);
    RUN_TEST(test_dsp_get_output_level_oob_returns_zero);

    return UNITY_END();
}
