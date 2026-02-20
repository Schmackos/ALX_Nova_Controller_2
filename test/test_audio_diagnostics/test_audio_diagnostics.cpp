#include <cmath>
#include <cstring>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Inline re-implementations for native testing =====
// Tests don't compile src/ directly (test_build_src = no), so we replicate
// the enum, struct, and pure function here exactly as in the production code.

// Clip rate EMA constants (must match i2s_audio.cpp)
static const float CLIP_RATE_HW_FAULT = 0.3f;
static const float CLIP_RATE_CLIPPING = 0.001f;

enum AudioHealthStatus {
    AUDIO_OK = 0,
    AUDIO_NO_DATA = 1,
    AUDIO_NOISE_ONLY = 2,
    AUDIO_CLIPPING = 3,
    AUDIO_I2S_ERROR = 4,
    AUDIO_HW_FAULT = 5
};

// Per-ADC diagnostics sub-struct (mirrors i2s_audio.h)
struct AdcDiagnostics {
    AudioHealthStatus status = AUDIO_OK;
    uint32_t i2sReadErrors = 0;
    uint32_t zeroByteReads = 0;
    uint32_t allZeroBuffers = 0;
    uint32_t consecutiveZeros = 0;
    uint32_t clippedSamples = 0;
    float clipRate = 0.0f;
    float noiseFloorDbfs = -96.0f;
    float peakDbfs = -96.0f;
    float dcOffset = 0.0f;
    unsigned long lastNonZeroMs = 0;
    unsigned long lastReadMs = 0;
    uint32_t totalBuffersRead = 0;
};

#ifndef NUM_AUDIO_INPUTS
#define NUM_AUDIO_INPUTS 3
#endif

#ifndef NUM_AUDIO_ADCS
#define NUM_AUDIO_ADCS 2
#endif

static_assert(NUM_AUDIO_INPUTS >= NUM_AUDIO_ADCS, "NUM_AUDIO_INPUTS must be >= NUM_AUDIO_ADCS");

struct AudioDiagnostics {
    AdcDiagnostics adc[NUM_AUDIO_INPUTS];
    bool sigGenActive = false;
    int numAdcsDetected = 1;
    int numInputsDetected = 1;
};

// Per-ADC health status derivation (matches production i2s_audio.cpp)
AudioHealthStatus audio_derive_health_status(const AdcDiagnostics &diag) {
    if (diag.i2sReadErrors > 10) return AUDIO_I2S_ERROR;
    if (diag.consecutiveZeros > 100) return AUDIO_NO_DATA;
    if (diag.clipRate > CLIP_RATE_HW_FAULT) return AUDIO_HW_FAULT;
    if (diag.clipRate > CLIP_RATE_CLIPPING) return AUDIO_CLIPPING;
    if (diag.noiseFloorDbfs < -75.0f && diag.noiseFloorDbfs > -96.0f) return AUDIO_NOISE_ONLY;
    return AUDIO_OK;
}

// Legacy overload for AudioDiagnostics (uses adc[0])
AudioHealthStatus audio_derive_health_status(const AudioDiagnostics &diag) {
    AdcDiagnostics masked = diag.adc[0];
    if (diag.sigGenActive) masked.clipRate = 0.0f; // Mask siggen-induced clipping
    return audio_derive_health_status(masked);
}

// ===== Tests =====

void setUp(void) {}
void tearDown(void) {}

// Test 1: Returns OK for normal metrics
void test_health_status_ok_normal(void) {
    AudioDiagnostics diag;
    diag.adc[0].noiseFloorDbfs = -30.0f;
    diag.adc[0].consecutiveZeros = 0;
    diag.adc[0].i2sReadErrors = 0;
    diag.adc[0].clipRate = 0.0f;
    TEST_ASSERT_EQUAL(AUDIO_OK, audio_derive_health_status(diag));
}

// Test 2: Returns NO_DATA when consecutiveZeros > 100
void test_health_status_no_data_consecutive_zeros(void) {
    AudioDiagnostics diag;
    diag.adc[0].consecutiveZeros = 101;
    diag.adc[0].noiseFloorDbfs = -96.0f;
    TEST_ASSERT_EQUAL(AUDIO_NO_DATA, audio_derive_health_status(diag));
}

// Test 3: Returns I2S_ERROR when errors > 10
void test_health_status_i2s_error(void) {
    AudioDiagnostics diag;
    diag.adc[0].i2sReadErrors = 11;
    TEST_ASSERT_EQUAL(AUDIO_I2S_ERROR, audio_derive_health_status(diag));
}

// Test 4: Returns CLIPPING when clipRate is moderate (>0.001) and siggen inactive
void test_health_status_clipping(void) {
    AudioDiagnostics diag;
    diag.adc[0].clipRate = 0.01f;  // 1% clip rate — signal too hot
    diag.sigGenActive = false;
    diag.adc[0].noiseFloorDbfs = -10.0f;
    TEST_ASSERT_EQUAL(AUDIO_CLIPPING, audio_derive_health_status(diag));
}

// Test 5: Returns OK when clipRate > 0 but siggen active (expected)
void test_health_status_ok_clipping_with_siggen(void) {
    AudioDiagnostics diag;
    diag.adc[0].clipRate = 0.5f;  // High clip rate, but siggen masks it
    diag.sigGenActive = true;
    diag.adc[0].noiseFloorDbfs = -10.0f;
    TEST_ASSERT_EQUAL(AUDIO_OK, audio_derive_health_status(diag));
}

// Test 6: Returns NOISE_ONLY when floor is -85 dBFS
void test_health_status_noise_only(void) {
    AudioDiagnostics diag;
    diag.adc[0].noiseFloorDbfs = -85.0f;
    diag.adc[0].consecutiveZeros = 0;
    TEST_ASSERT_EQUAL(AUDIO_NOISE_ONLY, audio_derive_health_status(diag));
}

// Test 7: Returns NO_DATA when floor is -96 dBFS with zeros
void test_health_status_no_data_floor_with_zeros(void) {
    AudioDiagnostics diag;
    diag.adc[0].noiseFloorDbfs = -96.0f;
    diag.adc[0].consecutiveZeros = 200;
    TEST_ASSERT_EQUAL(AUDIO_NO_DATA, audio_derive_health_status(diag));
}

// Test 8: I2S_ERROR priority over NO_DATA
void test_health_status_i2s_error_priority(void) {
    AudioDiagnostics diag;
    diag.adc[0].i2sReadErrors = 15;
    diag.adc[0].consecutiveZeros = 500;
    TEST_ASSERT_EQUAL(AUDIO_I2S_ERROR, audio_derive_health_status(diag));
}

// Test 9: AdcDiagnostics sub-struct zeroed by default
void test_diagnostics_struct_defaults(void) {
    AdcDiagnostics adiag;
    TEST_ASSERT_EQUAL(AUDIO_OK, adiag.status);
    TEST_ASSERT_EQUAL_UINT32(0, adiag.i2sReadErrors);
    TEST_ASSERT_EQUAL_UINT32(0, adiag.zeroByteReads);
    TEST_ASSERT_EQUAL_UINT32(0, adiag.allZeroBuffers);
    TEST_ASSERT_EQUAL_UINT32(0, adiag.consecutiveZeros);
    TEST_ASSERT_EQUAL_UINT32(0, adiag.clippedSamples);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, adiag.clipRate);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -96.0f, adiag.noiseFloorDbfs);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -96.0f, adiag.peakDbfs);
    TEST_ASSERT_EQUAL(0UL, adiag.lastNonZeroMs);
    TEST_ASSERT_EQUAL(0UL, adiag.lastReadMs);
    TEST_ASSERT_EQUAL_UINT32(0, adiag.totalBuffersRead);
    // AudioDiagnostics wrapper defaults
    AudioDiagnostics diag;
    TEST_ASSERT_FALSE(diag.sigGenActive);
    TEST_ASSERT_EQUAL(1, diag.numAdcsDetected);
}

// Test 10: AudioHealthStatus enum values 0-5
void test_health_status_enum_values(void) {
    TEST_ASSERT_EQUAL(0, AUDIO_OK);
    TEST_ASSERT_EQUAL(1, AUDIO_NO_DATA);
    TEST_ASSERT_EQUAL(2, AUDIO_NOISE_ONLY);
    TEST_ASSERT_EQUAL(3, AUDIO_CLIPPING);
    TEST_ASSERT_EQUAL(4, AUDIO_I2S_ERROR);
    TEST_ASSERT_EQUAL(5, AUDIO_HW_FAULT);
}

// ===== Dual ADC Tests =====

// Test 11: AdcDiagnostics overload works directly
void test_adc_diagnostics_direct_overload(void) {
    AdcDiagnostics adiag;
    adiag.noiseFloorDbfs = -30.0f;
    TEST_ASSERT_EQUAL(AUDIO_OK, audio_derive_health_status(adiag));

    adiag.i2sReadErrors = 20;
    TEST_ASSERT_EQUAL(AUDIO_I2S_ERROR, audio_derive_health_status(adiag));
}

// Test 12: Each ADC in AudioDiagnostics can have independent status
void test_dual_adc_independent_status(void) {
    AudioDiagnostics diag;
    // ADC 0: healthy signal
    diag.adc[0].noiseFloorDbfs = -30.0f;
    diag.adc[0].consecutiveZeros = 0;
    // ADC 1: no data (disconnected)
    diag.adc[1].consecutiveZeros = 200;
    diag.adc[1].noiseFloorDbfs = -96.0f;

    TEST_ASSERT_EQUAL(AUDIO_OK, audio_derive_health_status(diag.adc[0]));
    TEST_ASSERT_EQUAL(AUDIO_NO_DATA, audio_derive_health_status(diag.adc[1]));
    // Legacy overload uses adc[0]
    TEST_ASSERT_EQUAL(AUDIO_OK, audio_derive_health_status(diag));
}

// Test 13: sigGenActive masking in legacy AudioDiagnostics overload (clipRate)
void test_adc_diagnostics_siggen_masking(void) {
    AudioDiagnostics diag;
    diag.adc[0].clipRate = 0.05f;  // Moderate clip rate
    diag.adc[0].noiseFloorDbfs = -10.0f;
    // Without siggen, should report clipping
    diag.sigGenActive = false;
    TEST_ASSERT_EQUAL(AUDIO_CLIPPING, audio_derive_health_status(diag));
    // With siggen active, clipRate is masked -> OK
    diag.sigGenActive = true;
    TEST_ASSERT_EQUAL(AUDIO_OK, audio_derive_health_status(diag));
    // Direct AdcDiagnostics overload always reports clipping (no masking)
    TEST_ASSERT_EQUAL(AUDIO_CLIPPING, audio_derive_health_status(diag.adc[0]));
}

// Test 14: NUM_AUDIO_ADCS array size
void test_num_audio_adcs_array_size(void) {
    TEST_ASSERT_EQUAL(2, NUM_AUDIO_ADCS);
    TEST_ASSERT_EQUAL(3, NUM_AUDIO_INPUTS);
    AudioDiagnostics diag;
    // Both ADCs should be independently addressable
    diag.adc[0].i2sReadErrors = 5;
    diag.adc[1].i2sReadErrors = 15;
    TEST_ASSERT_EQUAL_UINT32(5, diag.adc[0].i2sReadErrors);
    TEST_ASSERT_EQUAL_UINT32(15, diag.adc[1].i2sReadErrors);
}

// ===== New EMA Clip Rate + HW_FAULT Tests =====

// Test 15: HW_FAULT at high clipRate (>0.3)
void test_health_status_hw_fault_high_clip_rate(void) {
    AdcDiagnostics diag;
    diag.clipRate = 0.5f;  // 50% of samples clipping = hardware fault
    diag.noiseFloorDbfs = -10.0f;
    TEST_ASSERT_EQUAL(AUDIO_HW_FAULT, audio_derive_health_status(diag));
}

// Test 16: CLIPPING at moderate clipRate (>0.001, <=0.3)
void test_health_status_clipping_moderate_clip_rate(void) {
    AdcDiagnostics diag;
    diag.clipRate = 0.1f;  // 10% clipping — signal too hot but not HW fault
    diag.noiseFloorDbfs = -10.0f;
    TEST_ASSERT_EQUAL(AUDIO_CLIPPING, audio_derive_health_status(diag));
}

// Test 17: Recovery — clipRate=0 with high lifetime clippedSamples -> OK
void test_health_status_recovery_after_clipping_stops(void) {
    AdcDiagnostics diag;
    diag.clippedSamples = 100000;  // High lifetime count
    diag.clipRate = 0.0f;          // But EMA has decayed to zero
    diag.noiseFloorDbfs = -30.0f;  // Normal signal
    TEST_ASSERT_EQUAL(AUDIO_OK, audio_derive_health_status(diag));
}

// Test 18: HW_FAULT boundary — 0.3 exact vs 0.301
void test_health_status_hw_fault_boundary(void) {
    AdcDiagnostics diag;
    diag.noiseFloorDbfs = -10.0f;
    // Exactly 0.3 — not above threshold, should be CLIPPING
    diag.clipRate = 0.3f;
    TEST_ASSERT_EQUAL(AUDIO_CLIPPING, audio_derive_health_status(diag));
    // Just above 0.3 — should be HW_FAULT
    diag.clipRate = 0.301f;
    TEST_ASSERT_EQUAL(AUDIO_HW_FAULT, audio_derive_health_status(diag));
}

// Test 19: clipRate below CLIPPING threshold -> OK (not noise)
void test_health_status_ok_below_clip_threshold(void) {
    AdcDiagnostics diag;
    diag.clipRate = 0.0005f;  // Below 0.001 threshold
    diag.noiseFloorDbfs = -30.0f;
    TEST_ASSERT_EQUAL(AUDIO_OK, audio_derive_health_status(diag));
}

// Test 20: I2S_ERROR takes priority over HW_FAULT
void test_health_status_i2s_error_over_hw_fault(void) {
    AdcDiagnostics diag;
    diag.i2sReadErrors = 20;
    diag.clipRate = 0.9f;  // Extreme clip rate
    TEST_ASSERT_EQUAL(AUDIO_I2S_ERROR, audio_derive_health_status(diag));
}

// Test 21: NO_DATA takes priority over HW_FAULT
void test_health_status_no_data_over_hw_fault(void) {
    AdcDiagnostics diag;
    diag.consecutiveZeros = 200;
    diag.clipRate = 0.9f;  // Extreme clip rate
    TEST_ASSERT_EQUAL(AUDIO_NO_DATA, audio_derive_health_status(diag));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_health_status_ok_normal);
    RUN_TEST(test_health_status_no_data_consecutive_zeros);
    RUN_TEST(test_health_status_i2s_error);
    RUN_TEST(test_health_status_clipping);
    RUN_TEST(test_health_status_ok_clipping_with_siggen);
    RUN_TEST(test_health_status_noise_only);
    RUN_TEST(test_health_status_no_data_floor_with_zeros);
    RUN_TEST(test_health_status_i2s_error_priority);
    RUN_TEST(test_diagnostics_struct_defaults);
    RUN_TEST(test_health_status_enum_values);
    RUN_TEST(test_adc_diagnostics_direct_overload);
    RUN_TEST(test_dual_adc_independent_status);
    RUN_TEST(test_adc_diagnostics_siggen_masking);
    RUN_TEST(test_num_audio_adcs_array_size);
    RUN_TEST(test_health_status_hw_fault_high_clip_rate);
    RUN_TEST(test_health_status_clipping_moderate_clip_rate);
    RUN_TEST(test_health_status_recovery_after_clipping_stops);
    RUN_TEST(test_health_status_hw_fault_boundary);
    RUN_TEST(test_health_status_ok_below_clip_threshold);
    RUN_TEST(test_health_status_i2s_error_over_hw_fault);
    RUN_TEST(test_health_status_no_data_over_hw_fault);
    return UNITY_END();
}
