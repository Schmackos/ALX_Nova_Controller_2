#include <cmath>
#include <cstring>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Inline re-implementations of signal_generator pure functions =====
// Tests don't compile src/ directly (test_build_src = no)

static const int16_t SINE_LUT[256] = {
        0,   804,  1608,  2410,  3212,  4011,  4808,  5602,
     6393,  7179,  7962,  8739,  9512, 10278, 11039, 11793,
    12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530,
    18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
    23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790,
    27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
    30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971,
    32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
    32767, 32757, 32728, 32678, 32609, 32521, 32412, 32285,
    32137, 31971, 31785, 31580, 31356, 31113, 30852, 30571,
    30273, 29956, 29621, 29268, 28898, 28510, 28105, 27683,
    27245, 26790, 26319, 25832, 25329, 24811, 24279, 23731,
    23170, 22594, 22005, 21403, 20787, 20159, 19519, 18868,
    18204, 17530, 16846, 16151, 15446, 14732, 14010, 13279,
    12539, 11793, 11039, 10278,  9512,  8739,  7962,  7179,
     6393,  5602,  4808,  4011,  3212,  2410,  1608,   804,
        0,  -804, -1608, -2410, -3212, -4011, -4808, -5602,
    -6393, -7179, -7962, -8739, -9512,-10278,-11039,-11793,
   -12539,-13279,-14010,-14732,-15446,-16151,-16846,-17530,
   -18204,-18868,-19519,-20159,-20787,-21403,-22005,-22594,
   -23170,-23731,-24279,-24811,-25329,-25832,-26319,-26790,
   -27245,-27683,-28105,-28510,-28898,-29268,-29621,-29956,
   -30273,-30571,-30852,-31113,-31356,-31580,-31785,-31971,
   -32137,-32285,-32412,-32521,-32609,-32678,-32728,-32757,
   -32767,-32757,-32728,-32678,-32609,-32521,-32412,-32285,
   -32137,-31971,-31785,-31580,-31356,-31113,-30852,-30571,
   -30273,-29956,-29621,-29268,-28898,-28510,-28105,-27683,
   -27245,-26790,-26319,-25832,-25329,-24811,-24279,-23731,
   -23170,-22594,-22005,-21403,-20787,-20159,-19519,-18868,
   -18204,-17530,-16846,-16151,-15446,-14732,-14010,-13279,
   -12539,-11793,-11039,-10278, -9512, -8739, -7962, -7179,
    -6393, -5602, -4808, -4011, -3212, -2410, -1608,  -804,
};
#define LUT_SIZE 256

float siggen_sine_sample(float phase) {
    float idx_f = phase * LUT_SIZE;
    int idx = (int)idx_f;
    float frac = idx_f - idx;
    idx &= (LUT_SIZE - 1);
    int next = (idx + 1) & (LUT_SIZE - 1);
    float s0 = SINE_LUT[idx] / 32767.0f;
    float s1 = SINE_LUT[next] / 32767.0f;
    return s0 + frac * (s1 - s0);
}

float siggen_square_sample(float phase) {
    return (phase < 0.5f) ? 1.0f : -1.0f;
}

float siggen_noise_sample(uint32_t *seed) {
    uint32_t s = *seed;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    *seed = s;
    return (float)(int32_t)s / 2147483648.0f;
}

float siggen_dbfs_to_linear(float dbfs) {
    if (dbfs <= -96.0f) return 0.0f;
    if (dbfs >= 0.0f) return 1.0f;
    return powf(10.0f, dbfs / 20.0f);
}

// Enums replicated from signal_generator.h
enum SignalWaveform  { WAVE_SINE = 0, WAVE_SQUARE, WAVE_NOISE, WAVE_SWEEP, WAVE_COUNT };
enum SignalOutputMode { SIGOUT_SOFTWARE = 0, SIGOUT_PWM };
enum SignalChannel   { SIGCHAN_LEFT = 0, SIGCHAN_RIGHT, SIGCHAN_BOTH };

// Simple buffer fill for testing (replicated from signal_generator.cpp core logic)
static float _test_phase = 0.0f;
static uint32_t _test_noise_seed = 42;

static void test_fill_buffer(int32_t *buf, int stereo_frames, uint32_t sample_rate,
                             int waveform, float frequency, float amp_linear, int channel) {
    float phase_inc = frequency / (float)sample_rate;
    _test_phase = 0.0f;

    for (int f = 0; f < stereo_frames; f++) {
        float sample = 0.0f;
        switch (waveform) {
            case WAVE_SINE:   sample = siggen_sine_sample(_test_phase); break;
            case WAVE_SQUARE: sample = siggen_square_sample(_test_phase); break;
            case WAVE_NOISE:  sample = siggen_noise_sample(&_test_noise_seed); break;
            default: break;
        }
        sample *= amp_linear;

        int32_t raw = (int32_t)(sample * 8388607.0f) << 8;
        int li = f * 2;
        int ri = f * 2 + 1;
        switch (channel) {
            case SIGCHAN_LEFT:  buf[li] = raw; buf[ri] = 0; break;
            case SIGCHAN_RIGHT: buf[li] = 0;   buf[ri] = raw; break;
            case SIGCHAN_BOTH:
            default: buf[li] = raw; buf[ri] = raw; break;
        }
        _test_phase += phase_inc;
        if (_test_phase >= 1.0f) _test_phase -= 1.0f;
    }
}

// ===== Tests =====

void setUp(void) {
    _test_phase = 0.0f;
    _test_noise_seed = 42;
}

void tearDown(void) {}

// 1. Sine at known phases
void test_sine_at_phase_0(void) {
    float val = siggen_sine_sample(0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, val);
}

void test_sine_at_phase_quarter(void) {
    // phase 0.25 = index 64 in LUT = 32767 → 1.0
    float val = siggen_sine_sample(0.25f);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 1.0f, val);
}

void test_sine_at_phase_half(void) {
    float val = siggen_sine_sample(0.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, val);
}

void test_sine_at_phase_three_quarter(void) {
    float val = siggen_sine_sample(0.75f);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, -1.0f, val);
}

// 2. Square wave
void test_square_first_half_positive(void) {
    TEST_ASSERT_EQUAL_FLOAT(1.0f, siggen_square_sample(0.0f));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, siggen_square_sample(0.25f));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, siggen_square_sample(0.49f));
}

void test_square_second_half_negative(void) {
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, siggen_square_sample(0.5f));
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, siggen_square_sample(0.75f));
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, siggen_square_sample(0.99f));
}

// 3. Noise distribution
void test_noise_bounded_and_roughly_zero_mean(void) {
    uint32_t seed = 12345;
    double sum = 0.0;
    int count = 10000;
    for (int i = 0; i < count; i++) {
        float s = siggen_noise_sample(&seed);
        TEST_ASSERT_TRUE(s >= -1.0f && s <= 1.0f);
        sum += s;
    }
    float mean = (float)(sum / count);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, mean);
}

// 4. dBFS to linear conversion
void test_dbfs_0_equals_1(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, siggen_dbfs_to_linear(0.0f));
}

void test_dbfs_minus6_equals_half(void) {
    // -6.02 dBFS ≈ 0.5 linear
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.5f, siggen_dbfs_to_linear(-6.02f));
}

void test_dbfs_minus20_equals_0_1(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.1f, siggen_dbfs_to_linear(-20.0f));
}

void test_dbfs_minus96_equals_0(void) {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, siggen_dbfs_to_linear(-96.0f));
}

void test_dbfs_below_floor(void) {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, siggen_dbfs_to_linear(-100.0f));
}

// 5. Waveform enum bounds
void test_waveform_enum_count(void) {
    TEST_ASSERT_EQUAL(4, WAVE_COUNT);
}

// 6. Channel selection: left-only zeros right channel
void test_channel_left_zeros_right(void) {
    int32_t buf[256 * 2]; // 256 stereo frames
    memset(buf, 0xFF, sizeof(buf)); // Fill with non-zero
    test_fill_buffer(buf, 256, 48000, WAVE_SINE, 1000.0f, 1.0f, SIGCHAN_LEFT);
    // Right channel should all be zero
    for (int f = 0; f < 256; f++) {
        TEST_ASSERT_EQUAL_INT32(0, buf[f * 2 + 1]);
    }
    // Left channel should have non-zero samples
    bool hasNonZero = false;
    for (int f = 0; f < 256; f++) {
        if (buf[f * 2] != 0) { hasNonZero = true; break; }
    }
    TEST_ASSERT_TRUE(hasNonZero);
}

// 7. Channel selection: right-only zeros left channel
void test_channel_right_zeros_left(void) {
    int32_t buf[256 * 2];
    memset(buf, 0xFF, sizeof(buf));
    test_fill_buffer(buf, 256, 48000, WAVE_SINE, 1000.0f, 1.0f, SIGCHAN_RIGHT);
    for (int f = 0; f < 256; f++) {
        TEST_ASSERT_EQUAL_INT32(0, buf[f * 2]);
    }
    bool hasNonZero = false;
    for (int f = 0; f < 256; f++) {
        if (buf[f * 2 + 1] != 0) { hasNonZero = true; break; }
    }
    TEST_ASSERT_TRUE(hasNonZero);
}

// 8. Both channels equal
void test_channel_both_equal(void) {
    int32_t buf[256 * 2];
    test_fill_buffer(buf, 256, 48000, WAVE_SINE, 1000.0f, 1.0f, SIGCHAN_BOTH);
    for (int f = 0; f < 256; f++) {
        TEST_ASSERT_EQUAL_INT32(buf[f * 2], buf[f * 2 + 1]);
    }
}

// 9. Frequency accuracy: 1 kHz at 48 kHz sample rate → 48 samples per period
void test_frequency_accuracy_1khz(void) {
    int32_t buf[480 * 2]; // 10ms at 48kHz = 480 frames
    test_fill_buffer(buf, 480, 48000, WAVE_SQUARE, 1000.0f, 1.0f, SIGCHAN_BOTH);

    // Count zero crossings (sign changes) in left channel
    int crossings = 0;
    for (int f = 1; f < 480; f++) {
        int32_t prev = buf[(f - 1) * 2];
        int32_t curr = buf[f * 2];
        if ((prev > 0 && curr < 0) || (prev < 0 && curr > 0)) {
            crossings++;
        }
    }
    // 1 kHz in 10ms = 10 full cycles = 20 zero crossings
    TEST_ASSERT_INT_WITHIN(2, 20, crossings);
}

// 10. Amplitude scaling
void test_amplitude_scaling(void) {
    // Generate sine at 0 dBFS (amp=1.0) and -6 dBFS (amp≈0.5)
    int32_t buf_full[48 * 2];
    int32_t buf_half[48 * 2];

    test_fill_buffer(buf_full, 48, 48000, WAVE_SINE, 1000.0f, 1.0f, SIGCHAN_BOTH);
    _test_phase = 0.0f; // Reset phase
    test_fill_buffer(buf_half, 48, 48000, WAVE_SINE, 1000.0f, 0.5f, SIGCHAN_BOTH);

    // Find peak values
    int32_t peak_full = 0, peak_half = 0;
    for (int f = 0; f < 48; f++) {
        int32_t v = abs(buf_full[f * 2]);
        if (v > peak_full) peak_full = v;
        v = abs(buf_half[f * 2]);
        if (v > peak_half) peak_half = v;
    }
    // Peak at half amplitude should be roughly 50% of full
    float ratio = (float)peak_half / (float)peak_full;
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.5f, ratio);
}

// 11. Phase wraps correctly
void test_phase_wraps(void) {
    // Generate many samples to ensure phase doesn't exceed 1.0
    int32_t buf[4800 * 2]; // 100ms at 48kHz
    test_fill_buffer(buf, 4800, 48000, WAVE_SINE, 440.0f, 1.0f, SIGCHAN_BOTH);
    // If phase didn't wrap correctly, we'd see garbage — test that all values are
    // valid (within 24-bit left-justified range)
    for (int f = 0; f < 4800; f++) {
        int32_t parsed = buf[f * 2] >> 8; // Undo left-justify
        TEST_ASSERT_TRUE(parsed >= -8388607 && parsed <= 8388607);
    }
}

// 12. Noise seed produces different values
void test_noise_seed_deterministic(void) {
    uint32_t seed1 = 42, seed2 = 42;
    float a = siggen_noise_sample(&seed1);
    float b = siggen_noise_sample(&seed2);
    TEST_ASSERT_EQUAL_FLOAT(a, b); // Same seed → same output
}

// 13. Stereo interleaved format matches I2S layout (L, R, L, R)
void test_stereo_interleaved_format(void) {
    int32_t buf[4 * 2]; // 4 frames
    test_fill_buffer(buf, 4, 48000, WAVE_SQUARE, 1000.0f, 1.0f, SIGCHAN_LEFT);
    // Left channel indices: 0, 2, 4, 6
    // Right channel indices: 1, 3, 5, 7
    for (int f = 0; f < 4; f++) {
        TEST_ASSERT_EQUAL_INT32(0, buf[f * 2 + 1]); // Right should be zero
    }
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_sine_at_phase_0);
    RUN_TEST(test_sine_at_phase_quarter);
    RUN_TEST(test_sine_at_phase_half);
    RUN_TEST(test_sine_at_phase_three_quarter);
    RUN_TEST(test_square_first_half_positive);
    RUN_TEST(test_square_second_half_negative);
    RUN_TEST(test_noise_bounded_and_roughly_zero_mean);
    RUN_TEST(test_dbfs_0_equals_1);
    RUN_TEST(test_dbfs_minus6_equals_half);
    RUN_TEST(test_dbfs_minus20_equals_0_1);
    RUN_TEST(test_dbfs_minus96_equals_0);
    RUN_TEST(test_dbfs_below_floor);
    RUN_TEST(test_waveform_enum_count);
    RUN_TEST(test_channel_left_zeros_right);
    RUN_TEST(test_channel_right_zeros_left);
    RUN_TEST(test_channel_both_equal);
    RUN_TEST(test_frequency_accuracy_1khz);
    RUN_TEST(test_amplitude_scaling);
    RUN_TEST(test_phase_wraps);
    RUN_TEST(test_noise_seed_deterministic);
    RUN_TEST(test_stereo_interleaved_format);

    return UNITY_END();
}
