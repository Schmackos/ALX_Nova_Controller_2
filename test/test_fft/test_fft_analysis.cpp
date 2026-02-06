#include <cmath>
#include <cstring>
#include <unity.h>
#include <arduinoFFT.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

/**
 * FFT Analysis Tests
 *
 * Tests FFT computation, frequency detection, and spectrum band aggregation.
 * Uses the arduinoFFT library directly (pure C++, runs on native platform).
 */

static const int FFT_SIZE = 1024;
static const int SPECTRUM_BANDS = 16;
static const float PI_F = 3.14159265358979f;

// Band edge frequencies (mirrors i2s_audio.cpp)
static const float BAND_EDGES[SPECTRUM_BANDS + 1] = {
    20, 40, 80, 160, 315, 630, 1250, 2500,
    5000, 8000, 10000, 12500, 14000, 16000, 18000, 20000, 24000
};

// ===== Re-implementation of audio_aggregate_fft_bands (mirrors i2s_audio.cpp) =====

void audio_aggregate_fft_bands(const float *magnitudes, int fft_size,
                               float sample_rate, float *bands, int num_bands) {
    int half = fft_size / 2;
    float bin_width = sample_rate / (float)fft_size;

    float max_mag = 0.0001f;
    for (int i = 1; i < half; i++) {
        if (magnitudes[i] > max_mag) max_mag = magnitudes[i];
    }

    for (int b = 0; b < num_bands && b < SPECTRUM_BANDS; b++) {
        float low_freq = BAND_EDGES[b];
        float high_freq = BAND_EDGES[b + 1];

        int low_bin = (int)(low_freq / bin_width);
        int high_bin = (int)(high_freq / bin_width);
        if (low_bin < 1) low_bin = 1;
        if (high_bin >= half) high_bin = half - 1;

        if (low_bin > high_bin || low_bin >= half) {
            bands[b] = 0.0f;
            continue;
        }

        float sum = 0.0f;
        int count = 0;
        for (int i = low_bin; i <= high_bin; i++) {
            sum += magnitudes[i];
            count++;
        }

        bands[b] = (count > 0) ? (sum / count) / max_mag : 0.0f;
        if (bands[b] > 1.0f) bands[b] = 1.0f;
    }
}

// ===== Helper: generate sine wave samples =====
static void generate_sine(float *out, int count, float freq, float sample_rate, float amplitude) {
    for (int i = 0; i < count; i++) {
        out[i] = amplitude * sinf(2.0f * PI_F * freq * (float)i / sample_rate);
    }
}

// ===== Helper: run FFT pipeline =====
static ArduinoFFT<float> fft;

static void run_fft(float *vReal, float *vImag, int size, float sample_rate) {
    memset(vImag, 0, size * sizeof(float));
    fft.windowing(vReal, size, FFTWindow::Hamming, FFTDirection::Forward);
    fft.compute(vReal, vImag, size, FFTDirection::Forward);
    fft.complexToMagnitude(vReal, vImag, size);
}

// ===== Tests =====

void setUp(void) {}
void tearDown(void) {}

// Test 1: Silence produces near-zero magnitudes, no dominant frequency
void test_fft_silence(void) {
    float vReal[FFT_SIZE], vImag[FFT_SIZE];
    memset(vReal, 0, sizeof(vReal));
    run_fft(vReal, vImag, FFT_SIZE, 48000.0f);

    // All magnitudes should be near zero
    float max_mag = 0.0f;
    for (int i = 1; i < FFT_SIZE / 2; i++) {
        if (vReal[i] > max_mag) max_mag = vReal[i];
    }
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, max_mag);
}

// Test 2: 440Hz sine detected at ~440Hz
void test_fft_440hz_sine(void) {
    float vReal[FFT_SIZE], vImag[FFT_SIZE];
    generate_sine(vReal, FFT_SIZE, 440.0f, 48000.0f, 1.0f);
    run_fft(vReal, vImag, FFT_SIZE, 48000.0f);

    float dominant = fft.majorPeak(vReal, FFT_SIZE, 48000.0f);
    // Within one bin width: 48000/1024 ≈ 46.9 Hz
    TEST_ASSERT_FLOAT_WITHIN(50.0f, 440.0f, dominant);
}

// Test 3: 1kHz sine detected correctly
void test_fft_1khz_sine(void) {
    float vReal[FFT_SIZE], vImag[FFT_SIZE];
    generate_sine(vReal, FFT_SIZE, 1000.0f, 48000.0f, 1.0f);
    run_fft(vReal, vImag, FFT_SIZE, 48000.0f);

    float dominant = fft.majorPeak(vReal, FFT_SIZE, 48000.0f);
    TEST_ASSERT_FLOAT_WITHIN(50.0f, 1000.0f, dominant);
}

// Test 4: Multiple known test tones detected accurately
void test_fft_dominant_frequency_accuracy(void) {
    float test_freqs[] = {100.0f, 500.0f, 2000.0f, 10000.0f};
    float vReal[FFT_SIZE], vImag[FFT_SIZE];

    for (int t = 0; t < 4; t++) {
        generate_sine(vReal, FFT_SIZE, test_freqs[t], 48000.0f, 1.0f);
        run_fft(vReal, vImag, FFT_SIZE, 48000.0f);

        float dominant = fft.majorPeak(vReal, FFT_SIZE, 48000.0f);
        TEST_ASSERT_FLOAT_WITHIN(50.0f, test_freqs[t], dominant);
    }
}

// Test 5: Band aggregation at 48kHz — 440Hz tone lands in correct band
void test_spectrum_band_aggregation_48k(void) {
    float vReal[FFT_SIZE], vImag[FFT_SIZE];
    generate_sine(vReal, FFT_SIZE, 440.0f, 48000.0f, 1.0f);
    run_fft(vReal, vImag, FFT_SIZE, 48000.0f);

    float bands[SPECTRUM_BANDS];
    audio_aggregate_fft_bands(vReal, FFT_SIZE, 48000.0f, bands, SPECTRUM_BANDS);

    // 440Hz falls in band 4 (315-630 Hz) — should be the dominant band
    int max_band = 0;
    for (int b = 1; b < SPECTRUM_BANDS; b++) {
        if (bands[b] > bands[max_band]) max_band = b;
    }
    TEST_ASSERT_EQUAL(4, max_band);
    TEST_ASSERT_TRUE(bands[4] > 0.0f);
}

// Test 6: Band aggregation at 44.1kHz — same band mapping
void test_spectrum_band_aggregation_44k(void) {
    float vReal[FFT_SIZE], vImag[FFT_SIZE];
    generate_sine(vReal, FFT_SIZE, 440.0f, 44100.0f, 1.0f);
    run_fft(vReal, vImag, FFT_SIZE, 44100.0f);

    float bands[SPECTRUM_BANDS];
    audio_aggregate_fft_bands(vReal, FFT_SIZE, 44100.0f, bands, SPECTRUM_BANDS);

    // 440Hz still in band 4 (315-630 Hz) — should be dominant
    int max_band = 0;
    for (int b = 1; b < SPECTRUM_BANDS; b++) {
        if (bands[b] > bands[max_band]) max_band = b;
    }
    TEST_ASSERT_EQUAL(4, max_band);
}

// Test 7: At 16kHz, bands above 8kHz should be zero (beyond Nyquist)
void test_spectrum_band_aggregation_16k(void) {
    float vReal[FFT_SIZE], vImag[FFT_SIZE];
    generate_sine(vReal, FFT_SIZE, 1000.0f, 16000.0f, 1.0f);
    run_fft(vReal, vImag, FFT_SIZE, 16000.0f);

    float bands[SPECTRUM_BANDS];
    audio_aggregate_fft_bands(vReal, FFT_SIZE, 16000.0f, bands, SPECTRUM_BANDS);

    // Bands above Nyquist (8kHz) should be zero
    for (int b = 8; b < SPECTRUM_BANDS; b++) {
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, bands[b]);
    }
}

// Test 8: Single 250Hz tone concentrates in band 3 (160-315Hz)
void test_spectrum_single_tone_in_correct_band(void) {
    float vReal[FFT_SIZE], vImag[FFT_SIZE];
    generate_sine(vReal, FFT_SIZE, 250.0f, 48000.0f, 1.0f);
    run_fft(vReal, vImag, FFT_SIZE, 48000.0f);

    float bands[SPECTRUM_BANDS];
    audio_aggregate_fft_bands(vReal, FFT_SIZE, 48000.0f, bands, SPECTRUM_BANDS);

    // 250Hz falls in band 3 (160-315 Hz) — should be dominant
    int max_band = 0;
    for (int b = 1; b < SPECTRUM_BANDS; b++) {
        if (bands[b] > bands[max_band]) max_band = b;
    }
    TEST_ASSERT_EQUAL(3, max_band);
}

// Test 9: All band values are in 0.0-1.0 range
void test_spectrum_normalization(void) {
    float vReal[FFT_SIZE], vImag[FFT_SIZE];
    // Mix of two tones
    generate_sine(vReal, FFT_SIZE, 440.0f, 48000.0f, 0.8f);
    float temp[FFT_SIZE];
    generate_sine(temp, FFT_SIZE, 2000.0f, 48000.0f, 0.5f);
    for (int i = 0; i < FFT_SIZE; i++) vReal[i] += temp[i];

    run_fft(vReal, vImag, FFT_SIZE, 48000.0f);

    float bands[SPECTRUM_BANDS];
    audio_aggregate_fft_bands(vReal, FFT_SIZE, 48000.0f, bands, SPECTRUM_BANDS);

    for (int b = 0; b < SPECTRUM_BANDS; b++) {
        TEST_ASSERT_TRUE(bands[b] >= 0.0f);
        TEST_ASSERT_TRUE(bands[b] <= 1.0f);
    }
}

// Test 10: Hamming window reduces spectral leakage
void test_fft_window_applied(void) {
    float vRealWindowed[FFT_SIZE], vImagWindowed[FFT_SIZE];
    float vRealRect[FFT_SIZE], vImagRect[FFT_SIZE];

    // Generate 440Hz tone
    generate_sine(vRealWindowed, FFT_SIZE, 440.0f, 48000.0f, 1.0f);
    memcpy(vRealRect, vRealWindowed, sizeof(vRealRect));

    // Windowed FFT
    memset(vImagWindowed, 0, sizeof(vImagWindowed));
    fft.windowing(vRealWindowed, FFT_SIZE, FFTWindow::Hamming, FFTDirection::Forward);
    fft.compute(vRealWindowed, vImagWindowed, FFT_SIZE, FFTDirection::Forward);
    fft.complexToMagnitude(vRealWindowed, vImagWindowed, FFT_SIZE);

    // Rectangular (no window) FFT
    memset(vImagRect, 0, sizeof(vImagRect));
    fft.compute(vRealRect, vImagRect, FFT_SIZE, FFTDirection::Forward);
    fft.complexToMagnitude(vRealRect, vImagRect, FFT_SIZE);

    // Measure "leakage": sum of magnitudes far from the 440Hz bin
    int target_bin = (int)(440.0f / (48000.0f / FFT_SIZE));
    float leakage_windowed = 0.0f, leakage_rect = 0.0f;
    for (int i = 1; i < FFT_SIZE / 2; i++) {
        if (abs(i - target_bin) > 5) { // bins more than 5 away from peak
            leakage_windowed += vRealWindowed[i];
            leakage_rect += vRealRect[i];
        }
    }

    // Windowed should have significantly less leakage
    TEST_ASSERT_TRUE(leakage_windowed < leakage_rect);
}

// ===== Main =====

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_fft_silence);
    RUN_TEST(test_fft_440hz_sine);
    RUN_TEST(test_fft_1khz_sine);
    RUN_TEST(test_fft_dominant_frequency_accuracy);
    RUN_TEST(test_spectrum_band_aggregation_48k);
    RUN_TEST(test_spectrum_band_aggregation_44k);
    RUN_TEST(test_spectrum_band_aggregation_16k);
    RUN_TEST(test_spectrum_single_tone_in_correct_band);
    RUN_TEST(test_spectrum_normalization);
    RUN_TEST(test_fft_window_applied);
    return UNITY_END();
}
