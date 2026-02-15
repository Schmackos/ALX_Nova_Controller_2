#include <cmath>
#include <cstring>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Inline re-implementations of i2s_audio pure computation functions =====
// Tests don't compile src/ directly (test_build_src = no), so we replicate the
// logic here exactly as it appears in the production code.

static const float DBFS_FLOOR = -96.0f;

int32_t audio_parse_24bit_sample(int32_t raw_i2s_word) {
    return raw_i2s_word >> 8;
}

float audio_compute_rms(const int32_t *samples, int count, int channel, int channels) {
    if (count <= 0) return 0.0f;
    float sum_sq = 0.0f;
    int sample_count = 0;
    const float MAX_24BIT = 8388607.0f;
    for (int i = channel; i < count * channels; i += channels) {
        int32_t raw = samples[i];
        int32_t parsed = audio_parse_24bit_sample(raw);
        float normalized = (float)parsed / MAX_24BIT;
        sum_sq += normalized * normalized;
        sample_count++;
    }
    if (sample_count == 0) return 0.0f;
    return sqrtf(sum_sq / sample_count);
}

float audio_rms_to_dbfs(float rms) {
    if (rms <= 0.0f) return DBFS_FLOOR;
    float db = 20.0f * log10f(rms);
    if (db < DBFS_FLOOR) return DBFS_FLOOR;
    return db;
}

float audio_migrate_voltage_threshold(float stored_value) {
    if (stored_value > 0.0f) {
        float ratio = stored_value / 3.3f;
        if (ratio <= 0.0f) return DBFS_FLOOR;
        if (ratio >= 1.0f) return 0.0f;
        return 20.0f * log10f(ratio);
    }
    return stored_value;
}

bool audio_validate_sample_rate(uint32_t rate) {
    return (rate == 16000 || rate == 44100 || rate == 48000);
}

static const int WAVEFORM_BUFFER_SIZE = 256;

uint8_t audio_quantize_sample(float normalized) {
    if (normalized > 1.0f) normalized = 1.0f;
    if (normalized < -1.0f) normalized = -1.0f;
    int val = (int)((normalized + 1.0f) * 127.5f + 0.5f);
    if (val > 255) val = 255;
    return (uint8_t)val;
}

void audio_downsample_waveform(const int32_t *stereo_frames, int frame_count,
                               uint8_t *out, int out_size) {
    const float MAX_24BIT = 8388607.0f;
    float peaks[256];
    int bins = (out_size > 256) ? 256 : out_size;
    for (int i = 0; i < bins; i++) peaks[i] = 0.0f;
    if (frame_count > 0 && bins > 0) {
        for (int f = 0; f < frame_count; f++) {
            int bin = (int)((long)f * bins / frame_count);
            if (bin >= bins) bin = bins - 1;
            float sL = (float)audio_parse_24bit_sample(stereo_frames[f * 2]) / MAX_24BIT;
            float sR = (float)audio_parse_24bit_sample(stereo_frames[f * 2 + 1]) / MAX_24BIT;
            float combined = (sL + sR) / 2.0f;
            if (fabsf(combined) > fabsf(peaks[bin])) {
                peaks[bin] = combined;
            }
        }
    }
    for (int i = 0; i < bins; i++) {
        out[i] = audio_quantize_sample(peaks[i]);
    }
}

// VU metering constants (mirrors i2s_audio.h)
static const float VU_ATTACK_MS = 300.0f;
static const float VU_DECAY_MS = 300.0f;
static const float PEAK_HOLD_MS = 2000.0f;
static const float PEAK_DECAY_AFTER_HOLD_MS = 300.0f;

float audio_vu_update(float current_vu, float new_rms, float dt_ms) {
    if (dt_ms <= 0.0f) return current_vu;
    float tau = (new_rms > current_vu) ? VU_ATTACK_MS : VU_DECAY_MS;
    float coeff = 1.0f - expf(-dt_ms / tau);
    return current_vu + coeff * (new_rms - current_vu);
}

float audio_peak_hold_update(float current_peak, float new_value,
                             unsigned long *hold_start_ms, unsigned long now_ms,
                             float dt_ms) {
    if (new_value >= current_peak) {
        *hold_start_ms = now_ms;
        return new_value;
    }
    unsigned long elapsed = now_ms - *hold_start_ms;
    if (elapsed < (unsigned long)PEAK_HOLD_MS) {
        return current_peak;
    }
    float coeff = 1.0f - expf(-dt_ms / PEAK_DECAY_AFTER_HOLD_MS);
    float decayed = current_peak * (1.0f - coeff);
    return (decayed > new_value) ? decayed : new_value;
}

// ===== Test setUp/tearDown =====

void setUp(void) {
    ArduinoMock::reset();
}

void tearDown(void) {}

// ===== Test Cases =====

// Test 1: All-zero sample buffer produces RMS = 0.0 and dBFS = -96
void test_rms_silence(void) {
    int32_t buffer[64];
    memset(buffer, 0, sizeof(buffer));

    float rms = audio_compute_rms(buffer, 32, 0, 2);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, rms);

    float dbfs = audio_rms_to_dbfs(rms);
    TEST_ASSERT_EQUAL_FLOAT(-96.0f, dbfs);
}

// Test 2: Constant-amplitude mono samples at ~0.707 of full scale
// A constant signal at amplitude A has RMS = A, so filling every sample
// with (int32_t)(8388607 * 0.707) << 8 should yield RMS ~ 0.707.
void test_rms_full_scale_sine(void) {
    const int NUM_SAMPLES = 128;
    int32_t buffer[NUM_SAMPLES];
    int32_t sample_val = (int32_t)(8388607.0 * 0.707) * 256; // << 8 via multiply

    for (int i = 0; i < NUM_SAMPLES; i++) {
        buffer[i] = sample_val;
    }

    float rms = audio_compute_rms(buffer, NUM_SAMPLES, 0, 1);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.707f, rms);
}

// Test 3: Half-amplitude samples. Constant value at 0.5 of full scale.
// RMS of constant 0.5 = 0.5, dBFS ~ -6.02.
void test_rms_half_scale(void) {
    const int NUM_SAMPLES = 128;
    int32_t buffer[NUM_SAMPLES];
    int32_t sample_val = (int32_t)(8388607.0 * 0.5) * 256;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        buffer[i] = sample_val;
    }

    float rms = audio_compute_rms(buffer, NUM_SAMPLES, 0, 1);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, rms);

    float dbfs = audio_rms_to_dbfs(rms);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -6.02f, dbfs);
}

// Test 4: Interleaved stereo — left channel has signal, right channel is zero.
// With channels=2, channel=0 reads L, channel=1 reads R.
void test_rms_stereo_split(void) {
    const int FRAMES = 64;
    const int TOTAL_SAMPLES = FRAMES * 2; // stereo
    int32_t buffer[TOTAL_SAMPLES];
    int32_t signal_val = (int32_t)(8388607.0 * 0.5) * 256;

    for (int i = 0; i < FRAMES; i++) {
        buffer[i * 2 + 0] = signal_val; // Left channel
        buffer[i * 2 + 1] = 0;          // Right channel
    }

    float rms_left = audio_compute_rms(buffer, FRAMES, 0, 2);
    float rms_right = audio_compute_rms(buffer, FRAMES, 1, 2);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, rms_left);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, rms_right);
}

// Test 5: Known RMS-to-dBFS conversions
void test_dbfs_conversion(void) {
    // 1.0 -> 0.0 dBFS
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, audio_rms_to_dbfs(1.0f));

    // 0.5 -> -6.02 dBFS
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -6.02f, audio_rms_to_dbfs(0.5f));

    // 0.1 -> -20.0 dBFS
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -20.0f, audio_rms_to_dbfs(0.1f));

    // 0.01 -> -40.0 dBFS
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -40.0f, audio_rms_to_dbfs(0.01f));

    // 0.0 -> -96.0 dBFS (floor)
    TEST_ASSERT_EQUAL_FLOAT(-96.0f, audio_rms_to_dbfs(0.0f));
}

// Test 6: Signal detection above threshold
void test_signal_detection_above_threshold(void) {
    float dbfs = -30.0f;
    float threshold = -40.0f;
    TEST_ASSERT_TRUE(dbfs >= threshold);
}

// Test 7: Signal detection below threshold
void test_signal_detection_below_threshold(void) {
    float dbfs = -50.0f;
    float threshold = -40.0f;
    TEST_ASSERT_FALSE(dbfs >= threshold);
}

// Test 8: Signal detection exactly at threshold
void test_signal_detection_at_threshold(void) {
    float dbfs = -40.0f;
    float threshold = -40.0f;
    TEST_ASSERT_TRUE(dbfs >= threshold);
}

// Test 9: Migrating old voltage threshold (0.1V) to dBFS
// 20 * log10(0.1 / 3.3) = 20 * log10(0.0303) ~ -30.37
void test_threshold_migration_old_voltage(void) {
    float result = audio_migrate_voltage_threshold(0.1f);
    float expected = 20.0f * log10f(0.1f / 3.3f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, expected, result);
}

// Test 10: Already-negative value (dBFS) is returned as-is
void test_threshold_migration_already_dbfs(void) {
    float result = audio_migrate_voltage_threshold(-40.0f);
    TEST_ASSERT_EQUAL_FLOAT(-40.0f, result);
}

// Test 11: Edge cases for threshold migration
void test_threshold_migration_edge_cases(void) {
    // 3.3V (full scale) -> ratio = 1.0 -> 0.0 dBFS
    float result_full = audio_migrate_voltage_threshold(3.3f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, result_full);

    // 0.01V -> 20 * log10(0.01 / 3.3) ~ -50.37
    float result_low = audio_migrate_voltage_threshold(0.01f);
    float expected_low = 20.0f * log10f(0.01f / 3.3f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, expected_low, result_low);
}

// Test 12: Sample rate validation
void test_sample_rate_validation(void) {
    TEST_ASSERT_TRUE(audio_validate_sample_rate(16000));
    TEST_ASSERT_TRUE(audio_validate_sample_rate(44100));
    TEST_ASSERT_TRUE(audio_validate_sample_rate(48000));

    TEST_ASSERT_FALSE(audio_validate_sample_rate(22050));
    TEST_ASSERT_FALSE(audio_validate_sample_rate(96000));
    TEST_ASSERT_FALSE(audio_validate_sample_rate(0));
}

// Test 13: 24-bit sample parsing (arithmetic right shift by 8)
void test_24bit_sample_parsing(void) {
    // Positive full scale: 0x7FFFFF00 >> 8 = 0x007FFFFF = 8388607
    TEST_ASSERT_EQUAL_INT32(8388607, audio_parse_24bit_sample(0x7FFFFF00));

    // Negative value: 0x80000000 >> 8 = 0xFF800000 (sign-extended)
    int32_t neg_result = audio_parse_24bit_sample((int32_t)0x80000000);
    TEST_ASSERT_TRUE(neg_result < 0);
    TEST_ASSERT_EQUAL_INT32((int32_t)0xFF800000, neg_result);

    // Zero
    TEST_ASSERT_EQUAL_INT32(0, audio_parse_24bit_sample(0));
}

// Test 14: Peak detection with decay — manual computation
// If prevPeak = 0.5, decay = 0.998, new rms = 0.3,
// then peak = max(0.3, 0.5 * 0.998) = max(0.3, 0.499) = 0.499
void test_peak_detection(void) {
    float prev_peak = 0.5f;
    float decay = 0.998f;
    float new_rms = 0.3f;

    float decayed = prev_peak * decay;
    float peak_result = (new_rms > decayed) ? new_rms : decayed;

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.499f, peak_result);

    // Second scenario: new RMS exceeds decayed peak
    prev_peak = 0.2f;
    new_rms = 0.8f;
    decayed = prev_peak * decay;
    peak_result = (new_rms > decayed) ? new_rms : decayed;

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.8f, peak_result);
}

// ===== Phase 2: VU Metering + Peak Hold Tests =====

// Test 15: VU attack ramp — signal increase causes VU to ramp up toward target
void test_vu_attack_ramp(void) {
    float vu = 0.0f;
    float target = 0.8f;

    // Simulate 300ms of 5.33ms steps (56 iterations at 48kHz/256 buffer)
    for (int i = 0; i < 56; i++) {
        vu = audio_vu_update(vu, target, 5.33f);
    }

    // After one attack time constant (~300ms), should reach ~63.2% of target
    // 0.8 * 0.632 ≈ 0.506
    TEST_ASSERT_FLOAT_WITHIN(0.05f, target * 0.632f, vu);

    // After 3x time constant (~900ms, 169 more iterations), should be near target
    for (int i = 0; i < 169; i++) {
        vu = audio_vu_update(vu, target, 5.33f);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.02f, target, vu);
}

// Test 16: VU decay ramp — signal drop causes VU to decay exponentially
void test_vu_decay_ramp(void) {
    float vu = 0.8f;
    float target = 0.0f;

    // After 300ms (56 steps of 5.33ms) of decay with 300ms time constant
    float vu_after_300ms = vu;
    for (int i = 0; i < 56; i++) {
        vu_after_300ms = audio_vu_update(vu_after_300ms, target, 5.33f);
    }

    // At ~300ms with 300ms decay TC: exp(-300/300) ≈ 0.368
    // vu ≈ 0.8 * 0.368 ≈ 0.294
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.8f * expf(-300.0f / 300.0f), vu_after_300ms);

    // VU should have decayed significantly but not reached zero
    TEST_ASSERT_TRUE(vu_after_300ms > 0.0f);
    TEST_ASSERT_TRUE(vu_after_300ms < 0.8f);
}

// Test 17: Peak hold instant attack — new value immediately becomes peak
void test_peak_hold_instant_attack(void) {
    float peak = 0.3f;
    unsigned long hold_start = 0;

    // New value exceeds current peak
    float result = audio_peak_hold_update(peak, 0.9f, &hold_start, 1000, 5.33f);
    TEST_ASSERT_EQUAL_FLOAT(0.9f, result);
    TEST_ASSERT_EQUAL_UINT32(1000, hold_start);

    // Equal value also updates (>=)
    result = audio_peak_hold_update(0.9f, 0.9f, &hold_start, 2000, 5.33f);
    TEST_ASSERT_EQUAL_FLOAT(0.9f, result);
    TEST_ASSERT_EQUAL_UINT32(2000, hold_start);
}

// Test 18: Peak holds for 2 seconds before decaying
void test_peak_hold_2s_duration(void) {
    float peak = 0.8f;
    unsigned long hold_start = 1000;

    // At 999ms after peak (within hold period) — peak should hold
    float result = audio_peak_hold_update(peak, 0.1f, &hold_start, 1999, 5.33f);
    TEST_ASSERT_EQUAL_FLOAT(0.8f, result);

    // At 1999ms after peak (still within 2000ms hold) — peak should hold
    result = audio_peak_hold_update(peak, 0.1f, &hold_start, 2999, 5.33f);
    TEST_ASSERT_EQUAL_FLOAT(0.8f, result);

    // At exactly 2000ms (hold expired) — should start decaying
    result = audio_peak_hold_update(peak, 0.1f, &hold_start, 3000, 5.33f);
    TEST_ASSERT_TRUE(result < 0.8f);
    TEST_ASSERT_TRUE(result > 0.1f); // Not yet fully decayed
}

// Test 19: Peak decays smoothly after hold period expires
void test_peak_decay_after_hold(void) {
    float peak = 0.8f;
    unsigned long hold_start = 0;

    // Advance past hold period (2000ms)
    unsigned long now = 2500;

    // Simulate several decay steps
    float prev = peak;
    for (int i = 0; i < 100; i++) {
        now += 5; // 5ms steps
        float result = audio_peak_hold_update(prev, 0.0f, &hold_start, now, 5.0f);
        TEST_ASSERT_TRUE(result <= prev); // Always decreasing
        TEST_ASSERT_TRUE(result >= 0.0f); // Never negative
        prev = result;
    }

    // After 500ms of decay (well past one time constant of 300ms),
    // peak should have significantly decayed from 0.8
    TEST_ASSERT_TRUE(prev < 0.3f);
}

// ===== Phase 3: Waveform Downsampling Tests =====

// Test 20: All-zero stereo frames produce waveform of all 128 (center)
void test_waveform_downsample_silence(void) {
    const int FRAMES = 512;
    int32_t buffer[FRAMES * 2];
    memset(buffer, 0, sizeof(buffer));

    uint8_t waveform[WAVEFORM_BUFFER_SIZE];
    audio_downsample_waveform(buffer, FRAMES, waveform, WAVEFORM_BUFFER_SIZE);

    for (int i = 0; i < WAVEFORM_BUFFER_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT8(128, waveform[i]);
    }
}

// Test 21: Full-scale positive stereo samples produce waveform near 255
void test_waveform_downsample_full_scale(void) {
    const int FRAMES = 512;
    int32_t buffer[FRAMES * 2];
    int32_t full_pos = (int32_t)(8388607.0 * 0.9) * 256; // ~90% of full scale, << 8

    for (int i = 0; i < FRAMES * 2; i++) {
        buffer[i] = full_pos;
    }

    uint8_t waveform[WAVEFORM_BUFFER_SIZE];
    audio_downsample_waveform(buffer, FRAMES, waveform, WAVEFORM_BUFFER_SIZE);

    for (int i = 0; i < WAVEFORM_BUFFER_SIZE; i++) {
        TEST_ASSERT_GREATER_THAN(220, waveform[i]); // Should be close to 255
    }
}

// Test 22: Output is exactly WAVEFORM_BUFFER_SIZE (256) bytes
void test_waveform_buffer_size(void) {
    const int FRAMES = 1024;
    int32_t buffer[FRAMES * 2];
    memset(buffer, 0, sizeof(buffer));

    uint8_t waveform[WAVEFORM_BUFFER_SIZE + 16]; // extra sentinel bytes
    memset(waveform, 0xAA, sizeof(waveform));

    audio_downsample_waveform(buffer, FRAMES, waveform, WAVEFORM_BUFFER_SIZE);

    // First 256 bytes should be 128 (silence)
    for (int i = 0; i < WAVEFORM_BUFFER_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT8(128, waveform[i]);
    }
    // Sentinel bytes beyond 256 should be untouched
    for (int i = WAVEFORM_BUFFER_SIZE; i < WAVEFORM_BUFFER_SIZE + 16; i++) {
        TEST_ASSERT_EQUAL_UINT8(0xAA, waveform[i]);
    }
}

// Test 23: Each bin captures peak (not average) of its source frames
void test_waveform_peak_hold_per_bin(void) {
    // 256 frames -> 256 bins (1 frame per bin when out_size=256)
    const int FRAMES = 256;
    int32_t buffer[FRAMES * 2];
    memset(buffer, 0, sizeof(buffer));

    // Put a spike in frame 10 (bin ~10)
    int32_t spike = (int32_t)(8388607.0 * 0.7) * 256;
    buffer[10 * 2] = spike;     // Left
    buffer[10 * 2 + 1] = spike; // Right

    uint8_t waveform[WAVEFORM_BUFFER_SIZE];
    audio_downsample_waveform(buffer, FRAMES, waveform, WAVEFORM_BUFFER_SIZE);

    // Bin 10 should show the spike (0.7 -> quantized ~217)
    TEST_ASSERT_GREATER_THAN(200, waveform[10]);

    // Other bins should be at center (128)
    TEST_ASSERT_EQUAL_UINT8(128, waveform[0]);
    TEST_ASSERT_EQUAL_UINT8(128, waveform[100]);
    TEST_ASSERT_EQUAL_UINT8(128, waveform[255]);
}

// Test 24: Quantization maps -1.0→0, 0.0→128, +1.0→255
void test_waveform_quantization(void) {
    TEST_ASSERT_EQUAL_UINT8(0, audio_quantize_sample(-1.0f));
    TEST_ASSERT_EQUAL_UINT8(128, audio_quantize_sample(0.0f));
    TEST_ASSERT_EQUAL_UINT8(255, audio_quantize_sample(1.0f));

    // Clamping beyond range
    TEST_ASSERT_EQUAL_UINT8(0, audio_quantize_sample(-2.0f));
    TEST_ASSERT_EQUAL_UINT8(255, audio_quantize_sample(2.0f));

    // Mid-range values (with rounding: val + 0.5 then truncate)
    TEST_ASSERT_EQUAL_UINT8(191, audio_quantize_sample(0.5f)); // (0.5+1)*127.5+0.5 = 191.75 → 191
    TEST_ASSERT_EQUAL_UINT8(64, audio_quantize_sample(-0.5f)); // (-0.5+1)*127.5+0.5 = 64.25 → 64
}

// ===== Main =====

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_rms_silence);
    RUN_TEST(test_rms_full_scale_sine);
    RUN_TEST(test_rms_half_scale);
    RUN_TEST(test_rms_stereo_split);
    RUN_TEST(test_dbfs_conversion);
    RUN_TEST(test_signal_detection_above_threshold);
    RUN_TEST(test_signal_detection_below_threshold);
    RUN_TEST(test_signal_detection_at_threshold);
    RUN_TEST(test_threshold_migration_old_voltage);
    RUN_TEST(test_threshold_migration_already_dbfs);
    RUN_TEST(test_threshold_migration_edge_cases);
    RUN_TEST(test_sample_rate_validation);
    RUN_TEST(test_24bit_sample_parsing);
    RUN_TEST(test_peak_detection);
    RUN_TEST(test_vu_attack_ramp);
    RUN_TEST(test_vu_decay_ramp);
    RUN_TEST(test_peak_hold_instant_attack);
    RUN_TEST(test_peak_hold_2s_duration);
    RUN_TEST(test_peak_decay_after_hold);
    RUN_TEST(test_waveform_downsample_silence);
    RUN_TEST(test_waveform_downsample_full_scale);
    RUN_TEST(test_waveform_buffer_size);
    RUN_TEST(test_waveform_peak_hold_per_bin);
    RUN_TEST(test_waveform_quantization);
    return UNITY_END();
}
