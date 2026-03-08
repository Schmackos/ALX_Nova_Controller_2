/**
 * test_hal_usb_audio.cpp
 *
 * Tests for HalUsbAudio -- the USB Audio HAL device driver.
 * Verifies descriptor fields, lifecycle (probe/init/deinit/healthCheck),
 * and AudioInputSource interface (callbacks, initial values, read behavior).
 *
 * Inline-includes hal_usb_audio.cpp which has built-in NATIVE_TEST stubs:
 *   usb_audio_is_streaming()       -> false
 *   usb_audio_is_connected()       -> false
 *   usb_audio_read()               -> returns 0 frames
 *   usb_audio_get_negotiated_rate()-> 48000
 *   usb_audio_get_volume_linear()  -> 1.0f
 *   usb_audio_get_mute()           -> false
 * This means read() returns 0 frames and isActive() returns false in native tests.
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

// hal_usb_audio.cpp includes hal_device_manager.h -- provide the full chain
#include "../test_mocks/Preferences.h"
#include "../test_mocks/LittleFS.h"
#include "../../src/diag_journal.cpp"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"

// Inline the driver under test (has NATIVE_TEST stubs for usb_audio functions)
#include "../../src/hal/hal_usb_audio.cpp"

// ===== Fixtures =====

static HalUsbAudio* dev = nullptr;

void setUp() {
    ArduinoMock::reset();
    dev = new HalUsbAudio();
}

void tearDown() {
    if (dev) {
        delete dev;
        dev = nullptr;
    }
}

// =====================================================================
// Group 1: Descriptor fields (11 tests)
// =====================================================================

void test_usb_compatible_string() {
    TEST_ASSERT_EQUAL_STRING("alx,usb-audio", dev->getDescriptor().compatible);
}

void test_usb_device_type_is_adc() {
    TEST_ASSERT_EQUAL(HAL_DEV_ADC, dev->getType());
}

void test_usb_capability_is_adc_path() {
    TEST_ASSERT_EQUAL(HAL_CAP_ADC_PATH, dev->getDescriptor().capabilities);
}

void test_usb_no_dac_path_capability() {
    uint8_t caps = dev->getDescriptor().capabilities;
    TEST_ASSERT_FALSE(caps & HAL_CAP_DAC_PATH);
}

void test_usb_bus_is_internal() {
    TEST_ASSERT_EQUAL(HAL_BUS_INTERNAL, dev->getDescriptor().bus.type);
}

void test_usb_channel_count_is_2() {
    TEST_ASSERT_EQUAL(2, dev->getDescriptor().channelCount);
}

void test_usb_priority_is_data() {
    TEST_ASSERT_EQUAL(HAL_PRIORITY_DATA, dev->getInitPriority());
}

void test_usb_name() {
    TEST_ASSERT_EQUAL_STRING("USB Audio", dev->getDescriptor().name);
}

void test_usb_manufacturer() {
    TEST_ASSERT_EQUAL_STRING("ALX", dev->getDescriptor().manufacturer);
}

void test_usb_supports_44k1() {
    TEST_ASSERT_TRUE(dev->getDescriptor().sampleRatesMask & HAL_RATE_44K1);
}

void test_usb_supports_48k() {
    TEST_ASSERT_TRUE(dev->getDescriptor().sampleRatesMask & HAL_RATE_48K);
}

void test_usb_supports_96k() {
    TEST_ASSERT_TRUE(dev->getDescriptor().sampleRatesMask & HAL_RATE_96K);
}

void test_usb_sample_rates_complete() {
    // USB Audio supports exactly 44.1k, 48k, and 96k
    uint32_t mask = dev->getDescriptor().sampleRatesMask;
    uint32_t expected = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K;
    TEST_ASSERT_EQUAL(expected, mask);
}

// =====================================================================
// Group 2: Lifecycle (8 tests)
// =====================================================================

void test_usb_initial_state_unknown() {
    TEST_ASSERT_EQUAL(HAL_STATE_UNKNOWN, dev->_state);
    TEST_ASSERT_FALSE(dev->_ready);
}

void test_usb_probe_always_true() {
    TEST_ASSERT_TRUE(dev->probe());
}

void test_usb_init_sets_available() {
    HalInitResult r = dev->init();
    TEST_ASSERT_TRUE(r.success);
    TEST_ASSERT_EQUAL(HAL_STATE_AVAILABLE, dev->_state);
    TEST_ASSERT_TRUE(dev->_ready);
}

void test_usb_init_error_code_ok() {
    HalInitResult r = dev->init();
    TEST_ASSERT_EQUAL(DIAG_OK, r.errorCode);
}

void test_usb_deinit_sets_removed() {
    dev->init();
    dev->deinit();
    TEST_ASSERT_EQUAL(HAL_STATE_REMOVED, dev->_state);
    TEST_ASSERT_FALSE(dev->_ready);
}

void test_usb_health_check_always_true() {
    dev->init();
    TEST_ASSERT_TRUE(dev->healthCheck());
}

void test_usb_health_check_true_before_init() {
    // USB device is always healthy -- disconnection is not a hardware fault
    TEST_ASSERT_TRUE(dev->healthCheck());
}

void test_usb_reinit_after_deinit() {
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

void test_usb_get_input_source_not_null() {
    const AudioInputSource* src = dev->getInputSource();
    TEST_ASSERT_NOT_NULL(src);
}

void test_usb_source_has_name() {
    const AudioInputSource* src = dev->getInputSource();
    TEST_ASSERT_NOT_NULL(src->name);
    TEST_ASSERT_EQUAL_STRING("USB Audio", src->name);
}

void test_usb_source_has_read_callback() {
    const AudioInputSource* src = dev->getInputSource();
    TEST_ASSERT_NOT_NULL(src->read);
}

void test_usb_source_has_is_active_callback() {
    const AudioInputSource* src = dev->getInputSource();
    TEST_ASSERT_NOT_NULL(src->isActive);
}

void test_usb_source_has_get_sample_rate_callback() {
    const AudioInputSource* src = dev->getInputSource();
    TEST_ASSERT_NOT_NULL(src->getSampleRate);
}

void test_usb_source_gain_is_unity() {
    const AudioInputSource* src = dev->getInputSource();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, src->gainLinear);
}

void test_usb_source_halslot_unbound() {
    const AudioInputSource* src = dev->getInputSource();
    TEST_ASSERT_EQUAL(0xFF, src->halSlot);
}

void test_usb_source_vu_initialized() {
    const AudioInputSource* src = dev->getInputSource();
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -90.0f, src->vuL);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -90.0f, src->vuR);
}

void test_usb_source_vu_smoothed_initialized() {
    const AudioInputSource* src = dev->getInputSource();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, src->_vuSmoothedL);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, src->_vuSmoothedR);
}

void test_usb_is_active_returns_false_native() {
    // In native test, usb_audio_is_streaming() stub returns false
    const AudioInputSource* src = dev->getInputSource();
    TEST_ASSERT_FALSE(src->isActive());
}

void test_usb_read_returns_zero_frames_native() {
    // In native test, usb_audio_read() stub returns 0 frames read
    const AudioInputSource* src = dev->getInputSource();
    int32_t buf[512];
    memset(buf, 0xAA, sizeof(buf));
    uint32_t got = src->read(buf, 256);
    TEST_ASSERT_EQUAL(0, got);
}

void test_usb_read_small_request() {
    const AudioInputSource* src = dev->getInputSource();
    int32_t buf[4];
    uint32_t got = src->read(buf, 2);
    // Stub returns 0 regardless of request size
    TEST_ASSERT_EQUAL(0, got);
}

void test_usb_get_sample_rate_returns_48k_native() {
    // Stub usb_audio_get_negotiated_rate() returns 48000
    const AudioInputSource* src = dev->getInputSource();
    TEST_ASSERT_EQUAL(48000, src->getSampleRate());
}

// =====================================================================
// Group 4: Multiple instances (2 tests)
// =====================================================================

void test_usb_two_instances_distinct_sources() {
    HalUsbAudio dev2;
    const AudioInputSource* src1 = dev->getInputSource();
    const AudioInputSource* src2 = dev2.getInputSource();
    TEST_ASSERT_NOT_NULL(src1);
    TEST_ASSERT_NOT_NULL(src2);
    TEST_ASSERT_EQUAL_STRING(src1->name, src2->name);
    // Different memory addresses (distinct instances)
    TEST_ASSERT_NOT_EQUAL(src1, src2);
}

void test_usb_two_instances_independent_lifecycle() {
    HalUsbAudio dev2;
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

void test_usb_overrides_get_input_source() {
    // HalDevice base returns nullptr; HalUsbAudio overrides to return non-null
    TEST_ASSERT_NOT_NULL(dev->getInputSource());
}

void test_usb_source_lane_default_zero() {
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
    RUN_TEST(test_usb_compatible_string);
    RUN_TEST(test_usb_device_type_is_adc);
    RUN_TEST(test_usb_capability_is_adc_path);
    RUN_TEST(test_usb_no_dac_path_capability);
    RUN_TEST(test_usb_bus_is_internal);
    RUN_TEST(test_usb_channel_count_is_2);
    RUN_TEST(test_usb_priority_is_data);
    RUN_TEST(test_usb_name);
    RUN_TEST(test_usb_manufacturer);
    RUN_TEST(test_usb_supports_44k1);
    RUN_TEST(test_usb_supports_48k);
    RUN_TEST(test_usb_supports_96k);
    RUN_TEST(test_usb_sample_rates_complete);

    // Group 2: Lifecycle
    RUN_TEST(test_usb_initial_state_unknown);
    RUN_TEST(test_usb_probe_always_true);
    RUN_TEST(test_usb_init_sets_available);
    RUN_TEST(test_usb_init_error_code_ok);
    RUN_TEST(test_usb_deinit_sets_removed);
    RUN_TEST(test_usb_health_check_always_true);
    RUN_TEST(test_usb_health_check_true_before_init);
    RUN_TEST(test_usb_reinit_after_deinit);

    // Group 3: AudioInputSource
    RUN_TEST(test_usb_get_input_source_not_null);
    RUN_TEST(test_usb_source_has_name);
    RUN_TEST(test_usb_source_has_read_callback);
    RUN_TEST(test_usb_source_has_is_active_callback);
    RUN_TEST(test_usb_source_has_get_sample_rate_callback);
    RUN_TEST(test_usb_source_gain_is_unity);
    RUN_TEST(test_usb_source_halslot_unbound);
    RUN_TEST(test_usb_source_vu_initialized);
    RUN_TEST(test_usb_source_vu_smoothed_initialized);
    RUN_TEST(test_usb_is_active_returns_false_native);
    RUN_TEST(test_usb_read_returns_zero_frames_native);
    RUN_TEST(test_usb_read_small_request);
    RUN_TEST(test_usb_get_sample_rate_returns_48k_native);

    // Group 4: Multiple instances
    RUN_TEST(test_usb_two_instances_distinct_sources);
    RUN_TEST(test_usb_two_instances_independent_lifecycle);

    // Group 5: Base class contract
    RUN_TEST(test_usb_overrides_get_input_source);
    RUN_TEST(test_usb_source_lane_default_zero);

    return UNITY_END();
}
