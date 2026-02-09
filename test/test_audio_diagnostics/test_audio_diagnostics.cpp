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

enum AudioHealthStatus {
    AUDIO_OK = 0,
    AUDIO_NO_DATA = 1,
    AUDIO_NOISE_ONLY = 2,
    AUDIO_CLIPPING = 3,
    AUDIO_I2S_ERROR = 4
};

struct AudioDiagnostics {
    AudioHealthStatus status = AUDIO_OK;
    uint32_t i2sReadErrors = 0;
    uint32_t zeroByteReads = 0;
    uint32_t allZeroBuffers = 0;
    uint32_t consecutiveZeros = 0;
    uint32_t clippedSamples = 0;
    float noiseFloorDbfs = -96.0f;
    float peakDbfs = -96.0f;
    unsigned long lastNonZeroMs = 0;
    unsigned long lastReadMs = 0;
    uint32_t totalBuffersRead = 0;
    bool sigGenActive = false;
};

AudioHealthStatus audio_derive_health_status(const AudioDiagnostics &diag) {
    if (diag.i2sReadErrors > 10) return AUDIO_I2S_ERROR;
    if (diag.consecutiveZeros > 100) return AUDIO_NO_DATA;
    if (diag.clippedSamples > 0 && !diag.sigGenActive) return AUDIO_CLIPPING;
    if (diag.noiseFloorDbfs < -75.0f && diag.noiseFloorDbfs > -96.0f) return AUDIO_NOISE_ONLY;
    return AUDIO_OK;
}

// ===== Tests =====

void setUp(void) {}
void tearDown(void) {}

// Test 1: Returns OK for normal metrics
void test_health_status_ok_normal(void) {
    AudioDiagnostics diag;
    diag.noiseFloorDbfs = -30.0f;
    diag.consecutiveZeros = 0;
    diag.i2sReadErrors = 0;
    diag.clippedSamples = 0;
    TEST_ASSERT_EQUAL(AUDIO_OK, audio_derive_health_status(diag));
}

// Test 2: Returns NO_DATA when consecutiveZeros > 100
void test_health_status_no_data_consecutive_zeros(void) {
    AudioDiagnostics diag;
    diag.consecutiveZeros = 101;
    diag.noiseFloorDbfs = -96.0f;
    TEST_ASSERT_EQUAL(AUDIO_NO_DATA, audio_derive_health_status(diag));
}

// Test 3: Returns I2S_ERROR when errors > 10
void test_health_status_i2s_error(void) {
    AudioDiagnostics diag;
    diag.i2sReadErrors = 11;
    TEST_ASSERT_EQUAL(AUDIO_I2S_ERROR, audio_derive_health_status(diag));
}

// Test 4: Returns CLIPPING when clips present and siggen inactive
void test_health_status_clipping(void) {
    AudioDiagnostics diag;
    diag.clippedSamples = 5;
    diag.sigGenActive = false;
    diag.noiseFloorDbfs = -10.0f;
    TEST_ASSERT_EQUAL(AUDIO_CLIPPING, audio_derive_health_status(diag));
}

// Test 5: Returns OK when clips present but siggen active (expected)
void test_health_status_ok_clipping_with_siggen(void) {
    AudioDiagnostics diag;
    diag.clippedSamples = 100;
    diag.sigGenActive = true;
    diag.noiseFloorDbfs = -10.0f;
    TEST_ASSERT_EQUAL(AUDIO_OK, audio_derive_health_status(diag));
}

// Test 6: Returns NOISE_ONLY when floor is -85 dBFS
void test_health_status_noise_only(void) {
    AudioDiagnostics diag;
    diag.noiseFloorDbfs = -85.0f;
    diag.consecutiveZeros = 0;
    TEST_ASSERT_EQUAL(AUDIO_NOISE_ONLY, audio_derive_health_status(diag));
}

// Test 7: Returns NO_DATA when floor is -96 dBFS with zeros
void test_health_status_no_data_floor_with_zeros(void) {
    AudioDiagnostics diag;
    diag.noiseFloorDbfs = -96.0f;
    diag.consecutiveZeros = 200;
    TEST_ASSERT_EQUAL(AUDIO_NO_DATA, audio_derive_health_status(diag));
}

// Test 8: I2S_ERROR priority over NO_DATA
void test_health_status_i2s_error_priority(void) {
    AudioDiagnostics diag;
    diag.i2sReadErrors = 15;
    diag.consecutiveZeros = 500;
    TEST_ASSERT_EQUAL(AUDIO_I2S_ERROR, audio_derive_health_status(diag));
}

// Test 9: AudioDiagnostics struct zeroed by default
void test_diagnostics_struct_defaults(void) {
    AudioDiagnostics diag;
    TEST_ASSERT_EQUAL(AUDIO_OK, diag.status);
    TEST_ASSERT_EQUAL_UINT32(0, diag.i2sReadErrors);
    TEST_ASSERT_EQUAL_UINT32(0, diag.zeroByteReads);
    TEST_ASSERT_EQUAL_UINT32(0, diag.allZeroBuffers);
    TEST_ASSERT_EQUAL_UINT32(0, diag.consecutiveZeros);
    TEST_ASSERT_EQUAL_UINT32(0, diag.clippedSamples);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -96.0f, diag.noiseFloorDbfs);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -96.0f, diag.peakDbfs);
    TEST_ASSERT_EQUAL(0UL, diag.lastNonZeroMs);
    TEST_ASSERT_EQUAL(0UL, diag.lastReadMs);
    TEST_ASSERT_EQUAL_UINT32(0, diag.totalBuffersRead);
    TEST_ASSERT_FALSE(diag.sigGenActive);
}

// Test 10: AudioHealthStatus enum values 0-4
void test_health_status_enum_values(void) {
    TEST_ASSERT_EQUAL(0, AUDIO_OK);
    TEST_ASSERT_EQUAL(1, AUDIO_NO_DATA);
    TEST_ASSERT_EQUAL(2, AUDIO_NOISE_ONLY);
    TEST_ASSERT_EQUAL(3, AUDIO_CLIPPING);
    TEST_ASSERT_EQUAL(4, AUDIO_I2S_ERROR);
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
    return UNITY_END();
}
