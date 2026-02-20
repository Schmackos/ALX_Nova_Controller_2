#include <cmath>
#include <cstring>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

#include "dsps_wind.h"
#include "dsps_mulc.h"
#include "dsps_mul.h"
#include "dsps_add.h"
#include "dsps_dotprod.h"
#include "dsps_snr.h"
#include "dsps_sfdr.h"

/**
 * ESP-DSP Feature Tests
 *
 * Tests window functions, vector math, dot product, and SNR/SFDR analysis.
 * Uses the esp_dsp_lite ANSI C fallback library on native.
 */

// Mirror FftWindowType enum from app_state.h (avoids pulling in ESP32 deps)
#ifndef NUM_AUDIO_INPUTS
#define NUM_AUDIO_INPUTS 3
#endif

#ifndef NUM_AUDIO_ADCS
#define NUM_AUDIO_ADCS 2
#endif

static_assert(NUM_AUDIO_INPUTS >= NUM_AUDIO_ADCS, "NUM_AUDIO_INPUTS must be >= NUM_AUDIO_ADCS");

enum FftWindowType : uint8_t {
    FFT_WINDOW_HANN = 0,
    FFT_WINDOW_BLACKMAN,
    FFT_WINDOW_BLACKMAN_HARRIS,
    FFT_WINDOW_BLACKMAN_NUTTALL,
    FFT_WINDOW_NUTTALL,
    FFT_WINDOW_FLAT_TOP,
    FFT_WINDOW_COUNT
};

// Minimal AppState mock for FFT window + SNR/SFDR tests
class AppState {
public:
    FftWindowType fftWindowType = FFT_WINDOW_HANN;
    float audioSnrDb[NUM_AUDIO_INPUTS] = {};
    float audioSfdrDb[NUM_AUDIO_INPUTS] = {};
    static AppState &getInstance() { static AppState inst; return inst; }
};

static const float PI_F = 3.14159265358979f;

void setUp(void) {}
void tearDown(void) {}

// ===== Window Function Tests =====

void test_hann_window_properties(void) {
    const int N = 256;
    float window[N];
    dsps_wind_hann_f32(window, N);

    // Hann window: starts and ends at 0, peak near 1.0 in the middle
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, window[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, window[N - 1]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, window[N / 2]);

    // All values in [0, 1]
    for (int i = 0; i < N; i++) {
        TEST_ASSERT_TRUE(window[i] >= 0.0f);
        TEST_ASSERT_TRUE(window[i] <= 1.0f);
    }
}

void test_blackman_window_properties(void) {
    const int N = 256;
    float window[N];
    dsps_wind_blackman_f32(window, N);

    // Blackman: near-zero at edges, peak in middle
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, window[0]);
    TEST_ASSERT_TRUE(window[N / 2] > 0.9f);

    for (int i = 0; i < N; i++) {
        TEST_ASSERT_TRUE(window[i] >= -0.01f); // Allow small numerical error
        TEST_ASSERT_TRUE(window[i] <= 1.01f);
    }
}

void test_blackman_harris_window_properties(void) {
    const int N = 256;
    float window[N];
    dsps_wind_blackman_harris_f32(window, N);

    // Blackman-Harris: very small edges, peak near 1
    TEST_ASSERT_TRUE(window[0] < 0.01f);
    TEST_ASSERT_TRUE(window[N / 2] > 0.95f);
}

void test_flat_top_window_properties(void) {
    const int N = 256;
    float window[N];
    dsps_wind_flat_top_f32(window, N);

    // Flat-top: can go slightly negative at edges, very flat top
    // Peak should be close to 1.0
    float maxVal = 0.0f;
    for (int i = 0; i < N; i++) {
        if (window[i] > maxVal) maxVal = window[i];
    }
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, maxVal);
}

void test_all_windows_are_symmetric(void) {
    const int N = 128;
    float window[N];

    // Test symmetry for all window types
    void (*windowFuncs[])(float *, int) = {
        dsps_wind_hann_f32,
        dsps_wind_blackman_f32,
        dsps_wind_blackman_harris_f32,
        dsps_wind_blackman_nuttall_f32,
        dsps_wind_nuttall_f32,
        dsps_wind_flat_top_f32
    };

    for (int w = 0; w < 6; w++) {
        windowFuncs[w](window, N);
        for (int i = 0; i < N / 2; i++) {
            TEST_ASSERT_FLOAT_WITHIN(0.0001f, window[i], window[N - 1 - i]);
        }
    }
}

void test_blackman_has_better_sidelobes_than_hann(void) {
    // Blackman should have lower first sidelobe than Hann
    // We verify this by comparing edge values: Blackman edges are lower
    const int N = 256;
    float hann[N], blackman[N];
    dsps_wind_hann_f32(hann, N);
    dsps_wind_blackman_f32(blackman, N);

    // Near edges (first 10% and last 10%), Blackman should be smaller or equal
    float hann_edge_sum = 0, blackman_edge_sum = 0;
    for (int i = 0; i < N / 10; i++) {
        hann_edge_sum += hann[i];
        blackman_edge_sum += blackman[i];
    }
    TEST_ASSERT_TRUE(blackman_edge_sum <= hann_edge_sum);
}

// ===== Vector Math Tests =====

void test_mulc_scales_array(void) {
    float input[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float output[4];
    esp_err_t err = dsps_mulc_f32(input, output, 4, 2.5f, 1, 1);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, output[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 7.5f, output[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, output[3]);
}

void test_mulc_inplace(void) {
    float buf[] = {1.0f, -2.0f, 3.0f};
    dsps_mulc_f32(buf, buf, 3, -1.0f, 1, 1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, buf[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, buf[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -3.0f, buf[2]);
}

void test_mul_element_wise(void) {
    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {4.0f, 5.0f, 6.0f};
    float out[3];
    esp_err_t err = dsps_mul_f32(a, b, out, 3, 1, 1, 1);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, out[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, out[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 18.0f, out[2]);
}

void test_add_element_wise(void) {
    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {10.0f, 20.0f, 30.0f};
    float out[3];
    esp_err_t err = dsps_add_f32(a, b, out, 3, 1, 1, 1);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 11.0f, out[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 22.0f, out[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 33.0f, out[2]);
}

void test_add_inplace_accumulate(void) {
    float acc[] = {1.0f, 2.0f, 3.0f};
    float delta[] = {0.5f, 0.5f, 0.5f};
    dsps_add_f32(acc, delta, acc, 3, 1, 1, 1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.5f, acc[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, acc[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.5f, acc[2]);
}

void test_vector_ops_reject_null(void) {
    float buf[4] = {};
    TEST_ASSERT_NOT_EQUAL(ESP_OK, dsps_mulc_f32(NULL, buf, 4, 1.0f, 1, 1));
    TEST_ASSERT_NOT_EQUAL(ESP_OK, dsps_mul_f32(NULL, buf, buf, 4, 1, 1, 1));
    TEST_ASSERT_NOT_EQUAL(ESP_OK, dsps_add_f32(NULL, buf, buf, 4, 1, 1, 1));
}

// ===== Dot Product Tests =====

void test_dotprod_basic(void) {
    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {4.0f, 5.0f, 6.0f};
    float result = 0.0f;
    esp_err_t err = dsps_dotprod_f32(a, b, &result, 3);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    // 1*4 + 2*5 + 3*6 = 32
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 32.0f, result);
}

void test_dotprod_accumulates(void) {
    float a[] = {1.0f, 1.0f};
    float b[] = {1.0f, 1.0f};
    float result = 10.0f; // Pre-existing value
    dsps_dotprod_f32(a, b, &result, 2);
    // 10 + (1*1 + 1*1) = 12
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.0f, result);
}

void test_dotprod_orthogonal_vectors(void) {
    float a[] = {1.0f, 0.0f, 0.0f};
    float b[] = {0.0f, 1.0f, 0.0f};
    float result = 0.0f;
    dsps_dotprod_f32(a, b, &result, 3);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, result);
}

// ===== SNR/SFDR Tests =====

void test_snr_pure_tone(void) {
    // Create a simple spectrum with one strong peak and low noise
    const int N = 512;
    float spectrum[N];
    memset(spectrum, 0, sizeof(spectrum));

    // Add a strong peak at bin 50
    spectrum[50] = 100.0f;
    // Add small noise floor
    for (int i = 1; i < N; i++) {
        if (i < 48 || i > 52) spectrum[i] = 0.01f;
    }

    float snr = dsps_snr_f32(spectrum, N, 0);
    // SNR should be high (signal is 10000x noise per bin)
    TEST_ASSERT_TRUE(snr > 20.0f);
}

void test_snr_no_signal(void) {
    const int N = 128;
    float spectrum[N];
    // Uniform noise floor
    for (int i = 0; i < N; i++) spectrum[i] = 1.0f;

    float snr = dsps_snr_f32(spectrum, N, 0);
    // With uniform spectrum, SNR should be very low (signal ≈ noise)
    TEST_ASSERT_TRUE(snr < 10.0f);
}

void test_sfdr_two_tones(void) {
    const int N = 256;
    float spectrum[N];
    memset(spectrum, 0, sizeof(spectrum));

    spectrum[30] = 100.0f; // Main tone
    spectrum[60] = 10.0f;  // Spur
    spectrum[90] = 0.1f;   // Noise

    float sfdr = dsps_sfdr_f32(spectrum, N, 0);
    // SFDR = 20*log10(100/10) = 20 dB
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 20.0f, sfdr);
}

void test_sfdr_single_tone(void) {
    const int N = 128;
    float spectrum[N];
    memset(spectrum, 0, sizeof(spectrum));
    spectrum[20] = 50.0f;
    // All other bins are 0 — SFDR should be very high
    float sfdr = dsps_sfdr_f32(spectrum, N, 0);
    TEST_ASSERT_TRUE(sfdr > 50.0f);
}

// ===== FFT Window Type Enum Tests =====

void test_fft_window_enum_values(void) {
    // Verify enum values match expected indices
    TEST_ASSERT_EQUAL(0, FFT_WINDOW_HANN);
    TEST_ASSERT_EQUAL(1, FFT_WINDOW_BLACKMAN);
    TEST_ASSERT_EQUAL(2, FFT_WINDOW_BLACKMAN_HARRIS);
    TEST_ASSERT_EQUAL(3, FFT_WINDOW_BLACKMAN_NUTTALL);
    TEST_ASSERT_EQUAL(4, FFT_WINDOW_NUTTALL);
    TEST_ASSERT_EQUAL(5, FFT_WINDOW_FLAT_TOP);
    TEST_ASSERT_EQUAL(6, FFT_WINDOW_COUNT);
}

void test_fft_window_appstate_default(void) {
    AppState &as = AppState::getInstance();
    // Default should be Hann
    TEST_ASSERT_EQUAL(FFT_WINDOW_HANN, as.fftWindowType);
}

void test_fft_window_appstate_persistence(void) {
    AppState &as = AppState::getInstance();
    as.fftWindowType = FFT_WINDOW_BLACKMAN_HARRIS;
    TEST_ASSERT_EQUAL(FFT_WINDOW_BLACKMAN_HARRIS, as.fftWindowType);
    as.fftWindowType = FFT_WINDOW_HANN; // Reset
}

void test_snr_sfdr_appstate_init(void) {
    AppState &as = AppState::getInstance();
    // Default values should be 0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, as.audioSnrDb[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, as.audioSfdrDb[0]);
}

// ===== Main =====

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Window functions
    RUN_TEST(test_hann_window_properties);
    RUN_TEST(test_blackman_window_properties);
    RUN_TEST(test_blackman_harris_window_properties);
    RUN_TEST(test_flat_top_window_properties);
    RUN_TEST(test_all_windows_are_symmetric);
    RUN_TEST(test_blackman_has_better_sidelobes_than_hann);

    // Vector math
    RUN_TEST(test_mulc_scales_array);
    RUN_TEST(test_mulc_inplace);
    RUN_TEST(test_mul_element_wise);
    RUN_TEST(test_add_element_wise);
    RUN_TEST(test_add_inplace_accumulate);
    RUN_TEST(test_vector_ops_reject_null);

    // Dot product
    RUN_TEST(test_dotprod_basic);
    RUN_TEST(test_dotprod_accumulates);
    RUN_TEST(test_dotprod_orthogonal_vectors);

    // SNR/SFDR
    RUN_TEST(test_snr_pure_tone);
    RUN_TEST(test_snr_no_signal);
    RUN_TEST(test_sfdr_two_tones);
    RUN_TEST(test_sfdr_single_tone);

    // AppState integration
    RUN_TEST(test_fft_window_enum_values);
    RUN_TEST(test_fft_window_appstate_default);
    RUN_TEST(test_fft_window_appstate_persistence);
    RUN_TEST(test_snr_sfdr_appstate_init);

    return UNITY_END();
}
