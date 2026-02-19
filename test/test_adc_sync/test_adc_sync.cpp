// test_adc_sync.cpp — Unit tests for compute_adc_sync_diag() pure function
//
// Tests exercise the cross-correlation phase detection logic without hardware.
// The pure function is re-implemented inline here (mirrors i2s_audio.cpp pattern).
// test_build_src = no means we cannot link src/ directly.

#include <cmath>
#include <cstring>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Inline definitions for native testing =====
// Mirrors the constants and struct from i2s_audio.h

#ifndef NUM_AUDIO_ADCS
#define NUM_AUDIO_ADCS 2
#endif

static const int    ADC_SYNC_CHECK_FRAMES     = 64;
static const int    ADC_SYNC_SEARCH_RANGE     = 8;
static const float  ADC_SYNC_OFFSET_THRESHOLD = 2.0f;
static const uint32_t ADC_SYNC_CHECK_INTERVAL_MS = 5000;

struct AdcSyncDiag {
    float phaseOffsetSamples = 0.0f;
    float phaseOffsetUs = 0.0f;
    float correlationPeak = 0.0f;
    bool  inSync = true;
    unsigned long lastCheckMs = 0;
    uint32_t checkCount = 0;
    uint32_t outOfSyncCount = 0;
};

// Pure function — mirrors i2s_audio.cpp implementation exactly
AdcSyncDiag compute_adc_sync_diag(const float* adc1_samples, const float* adc2_samples,
                                   int frames, float sampleRateHz) {
    AdcSyncDiag result;
    result.inSync = true;
    result.phaseOffsetSamples = 0.0f;
    result.phaseOffsetUs = 0.0f;
    result.correlationPeak = 0.0f;

    if (!adc1_samples || !adc2_samples || frames <= 0 || sampleRateHz <= 0.0f) {
        return result;
    }

    const int R = ADC_SYNC_SEARCH_RANGE;
    if (frames <= R) {
        return result;
    }

    int innerStart = R;
    int innerEnd   = frames - R - 1;
    if (innerEnd <= innerStart) {
        return result;
    }
    int innerLen = innerEnd - innerStart + 1;

    float bestCorr = -2.0f;
    int   bestLag  = 0;

    for (int lag = -R; lag <= R; lag++) {
        float sum = 0.0f;
        for (int i = innerStart; i <= innerEnd; i++) {
            sum += adc1_samples[i] * adc2_samples[i + lag];
        }
        float corr = sum / (float)innerLen;
        float absCorr = (corr < 0.0f) ? -corr : corr;
        if (absCorr > bestCorr) {
            bestCorr = absCorr;
            bestLag  = lag;
        }
    }

    float rms1 = 0.0f, rms2 = 0.0f;
    for (int i = innerStart; i <= innerEnd; i++) {
        rms1 += adc1_samples[i] * adc1_samples[i];
        rms2 += adc2_samples[i] * adc2_samples[i];
    }
    rms1 = sqrtf(rms1 / (float)innerLen);
    rms2 = sqrtf(rms2 / (float)innerLen);
    float rmsProd = rms1 * rms2;
    if (rmsProd > 1e-9f) {
        result.correlationPeak = bestCorr / rmsProd;
        if (result.correlationPeak > 1.0f) result.correlationPeak = 1.0f;
        if (result.correlationPeak < 0.0f) result.correlationPeak = 0.0f;
    } else {
        result.correlationPeak = 0.0f;
        return result;
    }

    result.phaseOffsetSamples = (float)bestLag;
    result.phaseOffsetUs = (float)bestLag / sampleRateHz * 1000000.0f;
    result.inSync = (result.phaseOffsetSamples < 0.0f
                     ? -result.phaseOffsetSamples
                     : result.phaseOffsetSamples) <= ADC_SYNC_OFFSET_THRESHOLD;
    return result;
}

// Native stub for i2s_audio_get_sync_diag — default state
AdcSyncDiag i2s_audio_get_sync_diag() {
    return AdcSyncDiag{};
}

// ===== Test helpers =====

// Generate N samples of a sine wave at the given frequency/amplitude
static void gen_sine(float* buf, int N, float freq, float sr, float amp, int startSample = 0) {
    const float PI2 = 2.0f * 3.14159265f;
    for (int i = 0; i < N; i++) {
        buf[i] = amp * sinf(PI2 * freq * (float)(i + startSample) / sr);
    }
}

// Fill buffer with pseudo-random noise in [-1, 1] using a simple LCG
static void gen_noise(float* buf, int N, uint32_t seed = 12345) {
    uint32_t state = seed;
    for (int i = 0; i < N; i++) {
        state = state * 1664525u + 1013904223u;
        buf[i] = ((float)(state >> 16) / 32767.5f) - 1.0f;
    }
}

// Build a shifted-copy test signal:
//   base[] is a reference waveform of length N.
//   s1[i] = base[i]
//   s2[i] = base[i + lag_s2_ahead]   (s2 is lag_s2_ahead samples ahead of s1)
//
// With the correlation definition corr(lag) = Σ s1[i]*s2[i+lag], the peak
// occurs at lag = -lag_s2_ahead (to compensate s2 being ahead).
// Therefore phaseOffsetSamples = -lag_s2_ahead.
static void make_lagged_pair(float* s1, float* s2, int N, int lag_s2_ahead) {
    // Use a sine + some harmonics so each sample is distinct across the lag range
    const float PI2 = 2.0f * 3.14159265f;
    for (int i = 0; i < N; i++) {
        // Index into a virtual longer signal
        float x = 0.7f * sinf(PI2 * 1000.0f * (float)i / 48000.0f)
                + 0.3f * sinf(PI2 * 3000.0f * (float)i / 48000.0f);
        s1[i] = x;
    }
    for (int i = 0; i < N; i++) {
        int src = i + lag_s2_ahead;
        // Clamp out-of-range accesses to boundary value
        if (src < 0) src = 0;
        if (src >= N) src = N - 1;
        float x = 0.7f * sinf(PI2 * 1000.0f * (float)src / 48000.0f)
                + 0.3f * sinf(PI2 * 3000.0f * (float)src / 48000.0f);
        s2[i] = x;
    }
}

// ===== Tests =====

void setUp(void) {}
void tearDown(void) {}

// Test 1: Two identical sine waves -> offset = 0, high correlation, inSync = true
void test_identical_signals_zero_offset(void) {
    const int N = ADC_SYNC_CHECK_FRAMES + ADC_SYNC_SEARCH_RANGE;
    float s1[N], s2[N];
    gen_sine(s1, N, 1000.0f, 48000.0f, 0.5f);
    gen_sine(s2, N, 1000.0f, 48000.0f, 0.5f); // identical

    AdcSyncDiag d = compute_adc_sync_diag(s1, s2, N, 48000.0f);

    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, d.phaseOffsetSamples);
    TEST_ASSERT_TRUE(d.inSync);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.0f, d.correlationPeak); // should be ~1.0
}

// Test 2: s2 lags s1 by 4 samples (adc2 delayed).
// make_lagged_pair with lag_s2_ahead = -4 → s2 lags s1 by 4.
// corr peaks at lag = -(-4) = +4 → phaseOffsetSamples = +4, inSync = false.
void test_delayed_signal_detects_offset(void) {
    const int N = ADC_SYNC_CHECK_FRAMES + ADC_SYNC_SEARCH_RANGE;
    float s1[N], s2[N];
    // lag_s2_ahead = -4: s2[i] = base[i-4] → s2 is 4 samples behind s1
    make_lagged_pair(s1, s2, N, -4);

    AdcSyncDiag d = compute_adc_sync_diag(s1, s2, N, 48000.0f);

    TEST_ASSERT_FLOAT_WITHIN(1.5f, 4.0f, d.phaseOffsetSamples);
    TEST_ASSERT_FALSE(d.inSync); // 4 > ADC_SYNC_OFFSET_THRESHOLD (2.0)
    TEST_ASSERT_GREATER_THAN_FLOAT(0.5f, d.correlationPeak);
}

// Test 3: s2 leads s1 by 3 samples (adc2 ahead).
// make_lagged_pair with lag_s2_ahead = 3 → s2 is 3 samples ahead of s1.
// corr peaks at lag = -3 → phaseOffsetSamples = -3, inSync = false.
void test_negative_delay_detected(void) {
    const int N = ADC_SYNC_CHECK_FRAMES + ADC_SYNC_SEARCH_RANGE;
    float s1[N], s2[N];
    // lag_s2_ahead = +3: s2[i] = base[i+3] → s2 leads s1 by 3
    make_lagged_pair(s1, s2, N, 3);

    AdcSyncDiag d = compute_adc_sync_diag(s1, s2, N, 48000.0f);

    TEST_ASSERT_FLOAT_WITHIN(1.5f, -3.0f, d.phaseOffsetSamples);
    TEST_ASSERT_FALSE(d.inSync); // |-3| > 2.0
}

// Test 4: White noise from two different seeds -> low correlation peak, no crash
void test_noise_low_correlation(void) {
    const int N = ADC_SYNC_CHECK_FRAMES + ADC_SYNC_SEARCH_RANGE;
    float s1[N], s2[N];
    gen_noise(s1, N, 111);
    gen_noise(s2, N, 999); // different seed -> uncorrelated

    AdcSyncDiag d = compute_adc_sync_diag(s1, s2, N, 48000.0f);

    // With uncorrelated noise, peak correlation should be low (< 0.5)
    TEST_ASSERT_LESS_THAN_FLOAT(0.5f, d.correlationPeak);
    // No NaN or Inf
    TEST_ASSERT_FALSE(isnan(d.phaseOffsetSamples));
    TEST_ASSERT_FALSE(isnan(d.correlationPeak));
    TEST_ASSERT_FALSE(isinf(d.phaseOffsetSamples));
}

// Test 5: s2 lags s1 by 1 sample -> inSync = true (within threshold of 2.0)
void test_sync_ok_within_threshold(void) {
    const int N = ADC_SYNC_CHECK_FRAMES + ADC_SYNC_SEARCH_RANGE;
    float s1[N], s2[N];
    // lag_s2_ahead = -1: s2 lags s1 by 1 → offset = +1 ≤ 2.0 → inSync = true
    make_lagged_pair(s1, s2, N, -1);

    AdcSyncDiag d = compute_adc_sync_diag(s1, s2, N, 48000.0f);

    TEST_ASSERT_FLOAT_WITHIN(1.5f, 1.0f, d.phaseOffsetSamples);
    TEST_ASSERT_TRUE(d.inSync); // 1 <= 2.0 threshold
}

// Test 6: s2 lags s1 by 5 samples -> inSync = false (beyond threshold of 2.0)
void test_sync_fail_beyond_threshold(void) {
    const int N = ADC_SYNC_CHECK_FRAMES + ADC_SYNC_SEARCH_RANGE;
    float s1[N], s2[N];
    // lag_s2_ahead = -5: s2 lags by 5 → offset = +5 > 2.0 → inSync = false
    make_lagged_pair(s1, s2, N, -5);

    AdcSyncDiag d = compute_adc_sync_diag(s1, s2, N, 48000.0f);

    TEST_ASSERT_FLOAT_WITHIN(1.5f, 5.0f, d.phaseOffsetSamples);
    TEST_ASSERT_FALSE(d.inSync); // 5 > 2.0 threshold
}

// Test 7: All-zero input -> returns gracefully with default inSync=true, no crash/NaN
void test_single_sample_silence_skipped(void) {
    const int N = ADC_SYNC_CHECK_FRAMES + ADC_SYNC_SEARCH_RANGE;
    float s1[N], s2[N];
    memset(s1, 0, sizeof(s1));
    memset(s2, 0, sizeof(s2));

    AdcSyncDiag d = compute_adc_sync_diag(s1, s2, N, 48000.0f);

    // Both silence -> rmsProd = 0 -> returns early with defaults
    TEST_ASSERT_TRUE(d.inSync);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, d.phaseOffsetSamples);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, d.correlationPeak);
    TEST_ASSERT_FALSE(isnan(d.phaseOffsetSamples));
    TEST_ASSERT_FALSE(isnan(d.correlationPeak));
}

// Test 8: Default state of i2s_audio_get_sync_diag() stub is inSync=true
void test_default_state_is_in_sync(void) {
    AdcSyncDiag d = i2s_audio_get_sync_diag();
    TEST_ASSERT_TRUE(d.inSync);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, d.phaseOffsetSamples);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, d.correlationPeak);
    TEST_ASSERT_EQUAL_UINT32(0, d.checkCount);
    TEST_ASSERT_EQUAL_UINT32(0, d.outOfSyncCount);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_identical_signals_zero_offset);
    RUN_TEST(test_delayed_signal_detects_offset);
    RUN_TEST(test_negative_delay_detected);
    RUN_TEST(test_noise_low_correlation);
    RUN_TEST(test_sync_ok_within_threshold);
    RUN_TEST(test_sync_fail_beyond_threshold);
    RUN_TEST(test_single_sample_silence_skipped);
    RUN_TEST(test_default_state_is_in_sync);
    return UNITY_END();
}
