/**
 * test_hal_siggen.cpp
 *
 * Tests for HalSigGen -- the Signal Generator HAL device driver.
 * Verifies descriptor fields, lifecycle (probe/init/deinit/healthCheck),
 * and AudioInputSource interface (callbacks, initial values, read behavior).
 *
 * Inline-includes hal_siggen.cpp which has built-in NATIVE_TEST stubs:
 *   siggen_is_active()       -> false
 *   siggen_is_software_mode()-> false
 *   siggen_fill_buffer()     -> no-op
 * This means read() returns zeros and isActive() returns false in native tests.
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
#include "../../src/audio_input_source.h"

// hal_siggen.cpp includes hal_device_manager.h -- provide the full chain
#include "../test_mocks/Preferences.h"
#include "../test_mocks/LittleFS.h"
#include "../../src/diag_journal.cpp"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"

// Inline the driver under test (has NATIVE_TEST stubs for siggen functions)
#include "../../src/hal/hal_siggen.cpp"

// ===== Fixtures =====

static HalSigGen* dev = nullptr;

void setUp() {
    ArduinoMock::reset();
    dev = new HalSigGen();
}

void tearDown() {
    if (dev) {
        delete dev;
        dev = nullptr;
    }
}

// =====================================================================
// Group 1: Descriptor fields (8 tests)
// =====================================================================

void test_siggen_compatible_string() {
    TEST_ASSERT_EQUAL_STRING("alx,signal-gen", dev->getDescriptor().compatible);
}

void test_siggen_device_type_is_adc() {
    TEST_ASSERT_EQUAL(HAL_DEV_ADC, dev->getType());
}

void test_siggen_capability_is_adc_path() {
    TEST_ASSERT_EQUAL(HAL_CAP_ADC_PATH, dev->getDescriptor().capabilities);
}

void test_siggen_no_dac_path_capability() {
    uint8_t caps = dev->getDescriptor().capabilities;
    TEST_ASSERT_FALSE(caps & HAL_CAP_DAC_PATH);
}

void test_siggen_bus_is_internal() {
    TEST_ASSERT_EQUAL(HAL_BUS_INTERNAL, dev->getDescriptor().bus.type);
}

void test_siggen_channel_count_is_2() {
    TEST_ASSERT_EQUAL(2, dev->getDescriptor().channelCount);
}

void test_siggen_priority_is_data() {
    TEST_ASSERT_EQUAL(HAL_PRIORITY_DATA, dev->getInitPriority());
}

void test_siggen_name() {
    TEST_ASSERT_EQUAL_STRING("Signal Generator", dev->getDescriptor().name);
}

void test_siggen_manufacturer() {
    TEST_ASSERT_EQUAL_STRING("ALX", dev->getDescriptor().manufacturer);
}

void test_siggen_sample_rates_mask() {
    uint32_t mask = dev->getDescriptor().sampleRatesMask;
    TEST_ASSERT_TRUE(mask & HAL_RATE_48K);
    TEST_ASSERT_TRUE(mask & HAL_RATE_96K);
    // SigGen does NOT advertise 44.1k
    TEST_ASSERT_FALSE(mask & HAL_RATE_44K1);
}

// =====================================================================
// Group 2: Lifecycle (7 tests)
// =====================================================================

void test_siggen_initial_state_unknown() {
    TEST_ASSERT_EQUAL(HAL_STATE_UNKNOWN, dev->_state);
    TEST_ASSERT_FALSE(dev->_ready);
}

void test_siggen_probe_always_true() {
    TEST_ASSERT_TRUE(dev->probe());
}

void test_siggen_init_sets_available() {
    HalInitResult r = dev->init();
    TEST_ASSERT_TRUE(r.success);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, dev->_state);
    TEST_ASSERT_TRUE(dev->_ready);
}

void test_siggen_init_error_code_ok() {
    HalInitResult r = dev->init();
    TEST_ASSERT_EQUAL(DIAG_OK, r.errorCode);
}

void test_siggen_deinit_sets_removed() {
    dev->init();
    dev->deinit();
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, dev->_state);
    TEST_ASSERT_FALSE(dev->_ready);
}

void test_siggen_health_check_always_true() {
    dev->init();
    TEST_ASSERT_TRUE(dev->healthCheck());
}

void test_siggen_health_check_true_even_before_init() {
    // Software device -- always healthy regardless of lifecycle state
    TEST_ASSERT_TRUE(dev->healthCheck());
}

void test_siggen_reinit_after_deinit() {
    dev->init();
    TEST_ASSERT_TRUE(dev->_ready);
    dev->deinit();
    TEST_ASSERT_FALSE(dev->_ready);

    HalInitResult r = dev->init();
    TEST_ASSERT_TRUE(r.success);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, dev->_state);
    TEST_ASSERT_TRUE(dev->_ready);
}

// =====================================================================
// Group 3: AudioInputSource interface (13 tests)
// =====================================================================

void test_siggen_get_input_source_not_null() {
    const AudioInputSource* src = dev->getInputSource();
    TEST_ASSERT_NOT_NULL(src);
}

void test_siggen_source_has_name() {
    const AudioInputSource* src = dev->getInputSource();
    TEST_ASSERT_NOT_NULL(src->name);
    TEST_ASSERT_EQUAL_STRING("Signal Gen", src->name);
}

void test_siggen_source_has_read_callback() {
    const AudioInputSource* src = dev->getInputSource();
    TEST_ASSERT_NOT_NULL(src->read);
}

void test_siggen_source_has_is_active_callback() {
    const AudioInputSource* src = dev->getInputSource();
    TEST_ASSERT_NOT_NULL(src->isActive);
}

void test_siggen_source_has_get_sample_rate_callback() {
    const AudioInputSource* src = dev->getInputSource();
    TEST_ASSERT_NOT_NULL(src->getSampleRate);
}

void test_siggen_source_gain_is_unity() {
    const AudioInputSource* src = dev->getInputSource();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, src->gainLinear);
}

void test_siggen_source_halslot_unbound() {
    const AudioInputSource* src = dev->getInputSource();
    TEST_ASSERT_EQUAL(0xFF, src->halSlot);
}

void test_siggen_source_vu_initialized() {
    const AudioInputSource* src = dev->getInputSource();
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -90.0f, src->vuL);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -90.0f, src->vuR);
}

void test_siggen_source_vu_smoothed_initialized() {
    const AudioInputSource* src = dev->getInputSource();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, src->_vuSmoothedL);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, src->_vuSmoothedR);
}

void test_siggen_is_active_returns_false_native() {
    // In native test, siggen stubs always return false
    const AudioInputSource* src = dev->getInputSource();
    TEST_ASSERT_FALSE(src->isActive());
}

void test_siggen_read_fills_zeros_when_inactive() {
    // In native test, siggen is always inactive so read should zero the buffer
    const AudioInputSource* src = dev->getInputSource();
    int32_t buf[512]; // 256 stereo frames
    memset(buf, 0xAA, sizeof(buf)); // Fill with non-zero pattern
    uint32_t got = src->read(buf, 256);
    TEST_ASSERT_EQUAL(256, got);
    // All samples should be zero (siggen inactive in native test)
    for (int i = 0; i < 512; i++) {
        TEST_ASSERT_EQUAL_INT32(0, buf[i]);
    }
}

void test_siggen_read_returns_requested_frame_count() {
    const AudioInputSource* src = dev->getInputSource();
    int32_t buf[64];
    // read() always returns the requested frame count (fills zeros if inactive)
    TEST_ASSERT_EQUAL(32, src->read(buf, 32));
    TEST_ASSERT_EQUAL(1, src->read(buf, 1));
    TEST_ASSERT_EQUAL(0, src->read(buf, 0));
}

void test_siggen_get_sample_rate_returns_48k() {
    const AudioInputSource* src = dev->getInputSource();
    TEST_ASSERT_EQUAL(48000, src->getSampleRate());
}

// =====================================================================
// Group 4: Multiple instances (2 tests)
// =====================================================================

void test_siggen_two_instances_distinct_sources() {
    HalSigGen dev2;
    const AudioInputSource* src1 = dev->getInputSource();
    const AudioInputSource* src2 = dev2.getInputSource();
    // Both should have valid, non-null sources
    TEST_ASSERT_NOT_NULL(src1);
    TEST_ASSERT_NOT_NULL(src2);
    // Both have the same name (same driver type)
    TEST_ASSERT_EQUAL_STRING(src1->name, src2->name);
    // But different memory addresses (distinct instances)
    TEST_ASSERT_NOT_EQUAL(src1, src2);
}

void test_siggen_two_instances_independent_lifecycle() {
    HalSigGen dev2;
    dev->init();
    TEST_ASSERT_TRUE(dev->_ready);
    TEST_ASSERT_FALSE(dev2._ready);

    dev2.init();
    TEST_ASSERT_TRUE(dev->_ready);
    TEST_ASSERT_TRUE(dev2._ready);

    dev->deinit();
    TEST_ASSERT_FALSE(dev->_ready);
    TEST_ASSERT_TRUE(dev2._ready);
}

// =====================================================================
// Group 5: Base class contract (2 tests)
// =====================================================================

void test_siggen_overrides_get_input_source() {
    // HalDevice base returns nullptr; HalSigGen overrides to return non-null
    TEST_ASSERT_NOT_NULL(dev->getInputSource());
}

void test_siggen_source_lane_default_zero() {
    // Lane is set to 0 by the driver -- bridge sets the real lane before registration
    const AudioInputSource* src = dev->getInputSource();
    TEST_ASSERT_EQUAL(0, src->lane);
}

// ===== Test Runner =====
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    // Group 1: Descriptor fields
    RUN_TEST(test_siggen_compatible_string);
    RUN_TEST(test_siggen_device_type_is_adc);
    RUN_TEST(test_siggen_capability_is_adc_path);
    RUN_TEST(test_siggen_no_dac_path_capability);
    RUN_TEST(test_siggen_bus_is_internal);
    RUN_TEST(test_siggen_channel_count_is_2);
    RUN_TEST(test_siggen_priority_is_data);
    RUN_TEST(test_siggen_name);
    RUN_TEST(test_siggen_manufacturer);
    RUN_TEST(test_siggen_sample_rates_mask);

    // Group 2: Lifecycle
    RUN_TEST(test_siggen_initial_state_unknown);
    RUN_TEST(test_siggen_probe_always_true);
    RUN_TEST(test_siggen_init_sets_available);
    RUN_TEST(test_siggen_init_error_code_ok);
    RUN_TEST(test_siggen_deinit_sets_removed);
    RUN_TEST(test_siggen_health_check_always_true);
    RUN_TEST(test_siggen_health_check_true_even_before_init);
    RUN_TEST(test_siggen_reinit_after_deinit);

    // Group 3: AudioInputSource
    RUN_TEST(test_siggen_get_input_source_not_null);
    RUN_TEST(test_siggen_source_has_name);
    RUN_TEST(test_siggen_source_has_read_callback);
    RUN_TEST(test_siggen_source_has_is_active_callback);
    RUN_TEST(test_siggen_source_has_get_sample_rate_callback);
    RUN_TEST(test_siggen_source_gain_is_unity);
    RUN_TEST(test_siggen_source_halslot_unbound);
    RUN_TEST(test_siggen_source_vu_initialized);
    RUN_TEST(test_siggen_source_vu_smoothed_initialized);
    RUN_TEST(test_siggen_is_active_returns_false_native);
    RUN_TEST(test_siggen_read_fills_zeros_when_inactive);
    RUN_TEST(test_siggen_read_returns_requested_frame_count);
    RUN_TEST(test_siggen_get_sample_rate_returns_48k);

    // Group 4: Multiple instances
    RUN_TEST(test_siggen_two_instances_distinct_sources);
    RUN_TEST(test_siggen_two_instances_independent_lifecycle);

    // Group 5: Base class contract
    RUN_TEST(test_siggen_overrides_get_input_source);
    RUN_TEST(test_siggen_source_lane_default_zero);

    return UNITY_END();
}
