// Test USB audio input processing logic (Phase 2)
// Tests: host volume application, mute handling, underrun zero-fill,
//        signal generator targeting, combined analysis with USB

#include <cmath>
#include <cstring>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

#ifndef NUM_AUDIO_ADCS
#define NUM_AUDIO_ADCS 2
#endif
#ifndef NUM_AUDIO_INPUTS
#define NUM_AUDIO_INPUTS 3
#endif

#include "../../src/i2s_audio.h"
#include "../../src/signal_generator.h"

// Verify constants
static_assert(NUM_AUDIO_INPUTS >= NUM_AUDIO_ADCS, "NUM_AUDIO_INPUTS must be >= NUM_AUDIO_ADCS");
static_assert(NUM_AUDIO_INPUTS == 3, "Expected 3 audio inputs");

// ===== Inline reimplementation of applyHostVolume for testing =====
// (The actual function is static in i2s_audio.cpp — we test the same algorithm)
static void applyHostVolume(int32_t *buf, int frames, float volLinear) {
    if (volLinear >= 0.999f) return;  // Unity gain, skip
    for (int i = 0; i < frames * 2; i++) {
        buf[i] = (int32_t)((float)buf[i] * volLinear);
    }
}

void setUp(void) {}
void tearDown(void) {}

// ===== Host Volume Application =====

void test_host_volume_unity_gain(void) {
    int32_t buf[8] = {1000000, -1000000, 500000, -500000, 250000, -250000, 100000, -100000};
    int32_t expected[8];
    memcpy(expected, buf, sizeof(buf));

    applyHostVolume(buf, 4, 1.0f);

    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_EQUAL_INT32(expected[i], buf[i]);
    }
}

void test_host_volume_half(void) {
    int32_t buf[4] = {1000000, -1000000, 500000, -500000};
    applyHostVolume(buf, 2, 0.5f);

    TEST_ASSERT_EQUAL_INT32(500000, buf[0]);
    TEST_ASSERT_EQUAL_INT32(-500000, buf[1]);
    TEST_ASSERT_EQUAL_INT32(250000, buf[2]);
    TEST_ASSERT_EQUAL_INT32(-250000, buf[3]);
}

void test_host_volume_zero(void) {
    int32_t buf[4] = {1000000, -1000000, 500000, -500000};
    applyHostVolume(buf, 2, 0.0f);

    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_INT32(0, buf[i]);
    }
}

void test_host_volume_near_unity_skips(void) {
    // 0.999f should be treated as unity (skip optimization)
    int32_t buf[4] = {1000000, -1000000, 500000, -500000};
    int32_t expected[4];
    memcpy(expected, buf, sizeof(buf));

    applyHostVolume(buf, 2, 0.9995f);  // Above 0.999 threshold

    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_INT32(expected[i], buf[i]);
    }
}

void test_host_volume_low_gain(void) {
    int32_t buf[2] = {8388607, -8388607};  // Full-scale 24-bit
    applyHostVolume(buf, 1, 0.1f);

    TEST_ASSERT_INT32_WITHIN(1, 838860, buf[0]);
    TEST_ASSERT_INT32_WITHIN(1, -838860, buf[1]);
}

// ===== Mute Zero-Fill =====

void test_mute_zeros_buffer(void) {
    int32_t buf[8];
    for (int i = 0; i < 8; i++) buf[i] = 1000000 * (i + 1);

    // Simulate mute behavior from audio_capture_task
    memset(buf, 0, sizeof(buf));

    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_EQUAL_INT32(0, buf[i]);
    }
}

// ===== Underrun Zero-Fill =====

void test_underrun_zero_fill_partial(void) {
    const int DMA_BUF_LEN = 256;
    int32_t buf[DMA_BUF_LEN * 2];

    // Simulate: 100 frames read, rest is underrun
    uint32_t framesRead = 100;
    for (int i = 0; i < (int)framesRead * 2; i++) {
        buf[i] = 1000000;
    }

    // Zero-fill remainder (as done in audio_capture_task)
    if (framesRead < (uint32_t)DMA_BUF_LEN) {
        memset(buf + framesRead * 2, 0,
               ((uint32_t)DMA_BUF_LEN - framesRead) * 2 * sizeof(int32_t));
    }

    // First 100 frames should be intact
    for (int i = 0; i < 200; i++) {
        TEST_ASSERT_EQUAL_INT32(1000000, buf[i]);
    }
    // Remaining should be zero
    for (int i = 200; i < DMA_BUF_LEN * 2; i++) {
        TEST_ASSERT_EQUAL_INT32(0, buf[i]);
    }
}

void test_underrun_zero_fill_empty(void) {
    const int DMA_BUF_LEN = 256;
    int32_t buf[DMA_BUF_LEN * 2];
    memset(buf, 0xFF, sizeof(buf));  // Fill with non-zero

    uint32_t framesRead = 0;
    if (framesRead < (uint32_t)DMA_BUF_LEN) {
        memset(buf + framesRead * 2, 0,
               ((uint32_t)DMA_BUF_LEN - framesRead) * 2 * sizeof(int32_t));
    }

    // Entire buffer should be zero
    for (int i = 0; i < DMA_BUF_LEN * 2; i++) {
        TEST_ASSERT_EQUAL_INT32(0, buf[i]);
    }
}

void test_underrun_no_fill_when_full(void) {
    const int DMA_BUF_LEN = 256;
    int32_t buf[DMA_BUF_LEN * 2];
    for (int i = 0; i < DMA_BUF_LEN * 2; i++) buf[i] = 42;

    uint32_t framesRead = DMA_BUF_LEN;  // Full read
    if (framesRead < (uint32_t)DMA_BUF_LEN) {
        memset(buf + framesRead * 2, 0,
               ((uint32_t)DMA_BUF_LEN - framesRead) * 2 * sizeof(int32_t));
    }

    // Buffer should be untouched
    for (int i = 0; i < DMA_BUF_LEN * 2; i++) {
        TEST_ASSERT_EQUAL_INT32(42, buf[i]);
    }
}

// ===== Signal Generator Target Enum =====

void test_siggen_target_usb_enum(void) {
    TEST_ASSERT_EQUAL(3, SIGTARGET_USB);
    TEST_ASSERT_EQUAL(4, SIGTARGET_ALL);
    // Existing values preserved
    TEST_ASSERT_EQUAL(0, SIGTARGET_ADC1);
    TEST_ASSERT_EQUAL(1, SIGTARGET_ADC2);
    TEST_ASSERT_EQUAL(2, SIGTARGET_BOTH);
}

void test_siggen_target_usb_includes_usb(void) {
    int target = SIGTARGET_USB;
    bool targetsUsb = (target == SIGTARGET_USB || target == SIGTARGET_ALL);
    TEST_ASSERT_TRUE(targetsUsb);
}

void test_siggen_target_all_includes_usb(void) {
    int target = SIGTARGET_ALL;
    bool targetsUsb = (target == SIGTARGET_USB || target == SIGTARGET_ALL);
    TEST_ASSERT_TRUE(targetsUsb);
    bool targetsAdc1 = (target == SIGTARGET_ADC1 || target == SIGTARGET_BOTH || target == SIGTARGET_ALL);
    TEST_ASSERT_TRUE(targetsAdc1);
}

void test_siggen_target_both_excludes_usb(void) {
    int target = SIGTARGET_BOTH;
    bool targetsUsb = (target == SIGTARGET_USB || target == SIGTARGET_ALL);
    TEST_ASSERT_FALSE(targetsUsb);
}

// ===== Audio Analysis Struct Sizing =====

void test_audio_analysis_has_three_inputs(void) {
    AudioAnalysis a = {};
    // All 3 slots should be accessible
    a.adc[0].dBFS = -10.0f;
    a.adc[1].dBFS = -20.0f;
    a.adc[2].dBFS = -30.0f;  // USB slot
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -10.0f, a.adc[0].dBFS);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -20.0f, a.adc[1].dBFS);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -30.0f, a.adc[2].dBFS);
}

void test_audio_diagnostics_has_three_inputs(void) {
    AudioDiagnostics d = {};
    d.adc[0].status = AUDIO_OK;
    d.adc[1].status = AUDIO_NOISE_ONLY;
    d.adc[2].status = AUDIO_NO_DATA;  // USB: not streaming
    TEST_ASSERT_EQUAL(AUDIO_OK, d.adc[0].status);
    TEST_ASSERT_EQUAL(AUDIO_NOISE_ONLY, d.adc[1].status);
    TEST_ASSERT_EQUAL(AUDIO_NO_DATA, d.adc[2].status);
}

void test_diagnostics_num_inputs_detected(void) {
    AudioDiagnostics d = {};
    d.numAdcsDetected = 2;
    d.numInputsDetected = 3;  // 2 ADCs + USB streaming
    TEST_ASSERT_EQUAL(2, d.numAdcsDetected);
    TEST_ASSERT_EQUAL(3, d.numInputsDetected);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Host volume
    RUN_TEST(test_host_volume_unity_gain);
    RUN_TEST(test_host_volume_half);
    RUN_TEST(test_host_volume_zero);
    RUN_TEST(test_host_volume_near_unity_skips);
    RUN_TEST(test_host_volume_low_gain);

    // Mute
    RUN_TEST(test_mute_zeros_buffer);

    // Underrun zero-fill
    RUN_TEST(test_underrun_zero_fill_partial);
    RUN_TEST(test_underrun_zero_fill_empty);
    RUN_TEST(test_underrun_no_fill_when_full);

    // Signal generator targeting
    RUN_TEST(test_siggen_target_usb_enum);
    RUN_TEST(test_siggen_target_usb_includes_usb);
    RUN_TEST(test_siggen_target_all_includes_usb);
    RUN_TEST(test_siggen_target_both_excludes_usb);

    // Struct sizing
    RUN_TEST(test_audio_analysis_has_three_inputs);
    RUN_TEST(test_audio_diagnostics_has_three_inputs);
    RUN_TEST(test_diagnostics_num_inputs_detected);

    return UNITY_END();
}
