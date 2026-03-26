#define UNIT_TEST
#define NATIVE_TEST
#define DSP_ENABLED
#define DSP_MAX_STAGES 24
#define DSP_PEQ_BANDS 10
#define DSP_MAX_FIR_TAPS 256
#define DSP_MAX_FIR_SLOTS 2
#define DSP_MAX_CHANNELS 4
#define DSP_MAX_DELAY_SLOTS 2
#define DSP_MAX_DELAY_SAMPLES 4800
#define DSP_DEFAULT_Q 0.707f
#define DSP_CPU_WARN_PERCENT 80.0f
#define DSP_PRESET_MAX_SLOTS 32

#include <unity.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

#include "../../lib/esp_dsp_lite/src/dsps_biquad_f32_ansi.c"
#include "../../lib/esp_dsp_lite/src/dsps_fir_f32_ansi.c"
#include "../../lib/esp_dsp_lite/src/dsps_fir_init_f32.c"
#include "../../lib/esp_dsp_lite/src/dsps_conv_f32_ansi.c"
#include "../../src/dsp_biquad_gen.c"

#include "../../src/dsp_pipeline.h"
#include "../../src/dsp_coefficients.h"
#include "../../src/dsp_rew_parser.h"

#include "../../src/heap_budget.cpp"
#include "../../src/psram_alloc.cpp"
#include "../../src/dsp_coefficients.cpp"
#include "../../src/dsp_convolution.cpp"
#include "../../src/dsp_pipeline.cpp"
#include "../../src/dsp_rew_parser.cpp"

#define FLOAT_TOL 0.001f

void setUp(void) { dsp_init(); }
void tearDown(void) {}

// ===== WAV builder helpers =====

// Write a uint16 little-endian into buf at offset
static void write_u16(uint8_t *buf, int offset, uint16_t val) {
    buf[offset]     = (uint8_t)(val & 0xFF);
    buf[offset + 1] = (uint8_t)((val >> 8) & 0xFF);
}

// Write a uint32 little-endian into buf at offset
static void write_u32(uint8_t *buf, int offset, uint32_t val) {
    buf[offset]     = (uint8_t)(val & 0xFF);
    buf[offset + 1] = (uint8_t)((val >> 8) & 0xFF);
    buf[offset + 2] = (uint8_t)((val >> 16) & 0xFF);
    buf[offset + 3] = (uint8_t)((val >> 24) & 0xFF);
}

// Build a minimal PCM16 WAV with given samples.
// Returns total byte count written into buf.
static int build_pcm16_wav(uint8_t *buf, int bufSize,
                            const int16_t *samples, int numSamples,
                            uint32_t sampleRate, uint16_t numChannels) {
    int dataBytes = numSamples * numChannels * 2;
    int totalBytes = 44 + dataBytes;
    if (totalBytes > bufSize) return 0;

    // RIFF header
    buf[0] = 'R'; buf[1] = 'I'; buf[2] = 'F'; buf[3] = 'F';
    write_u32(buf, 4, (uint32_t)(36 + dataBytes));
    buf[8] = 'W'; buf[9] = 'A'; buf[10] = 'V'; buf[11] = 'E';

    // fmt chunk
    buf[12] = 'f'; buf[13] = 'm'; buf[14] = 't'; buf[15] = ' ';
    write_u32(buf, 16, 16);                         // chunk size
    write_u16(buf, 20, 1);                          // audioFormat = PCM
    write_u16(buf, 22, numChannels);
    write_u32(buf, 24, sampleRate);
    write_u32(buf, 28, sampleRate * numChannels * 2); // byteRate
    write_u16(buf, 32, (uint16_t)(numChannels * 2)); // blockAlign
    write_u16(buf, 34, 16);                          // bitsPerSample

    // data chunk
    buf[36] = 'd'; buf[37] = 'a'; buf[38] = 't'; buf[39] = 'a';
    write_u32(buf, 40, (uint32_t)dataBytes);

    for (int i = 0; i < numSamples * (int)numChannels; i++) {
        write_u16(buf, 44 + i * 2, (uint16_t)samples[i]);
    }

    return totalBytes;
}

// Build a 32-bit float WAV
static int build_float32_wav(uint8_t *buf, int bufSize,
                              const float *samples, int numSamples,
                              uint32_t sampleRate) {
    int dataBytes = numSamples * 4;
    int totalBytes = 44 + dataBytes;
    if (totalBytes > bufSize) return 0;

    buf[0] = 'R'; buf[1] = 'I'; buf[2] = 'F'; buf[3] = 'F';
    write_u32(buf, 4, (uint32_t)(36 + dataBytes));
    buf[8] = 'W'; buf[9] = 'A'; buf[10] = 'V'; buf[11] = 'E';

    buf[12] = 'f'; buf[13] = 'm'; buf[14] = 't'; buf[15] = ' ';
    write_u32(buf, 16, 16);
    write_u16(buf, 20, 3);           // audioFormat = IEEE float
    write_u16(buf, 22, 1);           // mono
    write_u32(buf, 24, sampleRate);
    write_u32(buf, 28, sampleRate * 4);
    write_u16(buf, 32, 4);
    write_u16(buf, 34, 32);

    buf[36] = 'd'; buf[37] = 'a'; buf[38] = 't'; buf[39] = 'a';
    write_u32(buf, 40, (uint32_t)dataBytes);

    for (int i = 0; i < numSamples; i++) {
        memcpy(buf + 44 + i * 4, &samples[i], 4);
    }
    return totalBytes;
}

// ===== Tests =====

void test_wav_ir_null_data_returns_minus1(void) {
    float taps[16];
    int result = dsp_parse_wav_ir(nullptr, 44, taps, 16, 48000);
    TEST_ASSERT_EQUAL_INT(-1, result);
}

void test_wav_ir_too_short_returns_minus1(void) {
    uint8_t buf[20] = {};
    float taps[16];
    int result = dsp_parse_wav_ir(buf, 20, taps, 16, 48000);
    TEST_ASSERT_EQUAL_INT(-1, result);
}

void test_wav_ir_missing_riff_returns_minus1(void) {
    uint8_t buf[64] = {};
    // Write valid size but wrong header
    buf[0] = 'X'; buf[1] = 'X'; buf[2] = 'X'; buf[3] = 'X';
    float taps[16];
    int result = dsp_parse_wav_ir(buf, 64, taps, 16, 48000);
    TEST_ASSERT_EQUAL_INT(-1, result);
}

void test_wav_ir_missing_wave_tag_returns_minus1(void) {
    uint8_t buf[64] = {};
    buf[0] = 'R'; buf[1] = 'I'; buf[2] = 'F'; buf[3] = 'F';
    write_u32(buf, 4, 56);
    buf[8]  = 'X'; buf[9]  = 'X'; buf[10] = 'X'; buf[11] = 'X'; // Not WAVE
    float taps[16];
    int result = dsp_parse_wav_ir(buf, 64, taps, 16, 48000);
    TEST_ASSERT_EQUAL_INT(-1, result);
}

void test_wav_ir_stereo_returns_minus1(void) {
    static uint8_t buf[1024];
    int16_t samples[8] = {32767, -32768, 16384, -16384, 0, 0, 0, 0};
    // 2 channels
    int len = build_pcm16_wav(buf, sizeof(buf), samples, 4, 48000, 2);
    TEST_ASSERT_TRUE(len > 0);

    float taps[8];
    int result = dsp_parse_wav_ir(buf, len, taps, 8, 48000);
    TEST_ASSERT_EQUAL_INT(-1, result);
}

void test_wav_ir_sample_rate_mismatch_returns_minus1(void) {
    static uint8_t buf[512];
    int16_t samples[4] = {32767, 0, -32767, 0};
    int len = build_pcm16_wav(buf, sizeof(buf), samples, 4, 44100, 1);
    TEST_ASSERT_TRUE(len > 0);

    float taps[4];
    // File is 44100 but we expect 48000
    int result = dsp_parse_wav_ir(buf, len, taps, 4, 48000);
    TEST_ASSERT_EQUAL_INT(-1, result);
}

void test_wav_ir_pcm16_valid_returns_tap_count(void) {
    static uint8_t buf[512];
    int16_t samples[4] = {16384, 0, -16384, 0};
    int len = build_pcm16_wav(buf, sizeof(buf), samples, 4, 48000, 1);
    TEST_ASSERT_TRUE(len > 0);

    float taps[8];
    int result = dsp_parse_wav_ir(buf, len, taps, 8, 48000);
    TEST_ASSERT_EQUAL_INT(4, result);
}

void test_wav_ir_pcm16_normalisation_correct(void) {
    // 16-bit PCM: max value 32767 should map to ~1.0 (= 32767/32768)
    static uint8_t buf[512];
    int16_t samples[2] = {32767, -32768};
    int len = build_pcm16_wav(buf, sizeof(buf), samples, 2, 48000, 1);

    float taps[2];
    dsp_parse_wav_ir(buf, len, taps, 2, 48000);

    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 32767.0f / 32768.0f, taps[0]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, -32768.0f / 32768.0f, taps[1]);
}

void test_wav_ir_float32_valid_returns_tap_count(void) {
    static uint8_t buf[512];
    float samples[4] = {1.0f, 0.5f, -0.5f, -1.0f};
    int len = build_float32_wav(buf, sizeof(buf), samples, 4, 48000);
    TEST_ASSERT_TRUE(len > 0);

    float taps[8];
    int result = dsp_parse_wav_ir(buf, len, taps, 8, 48000);
    TEST_ASSERT_EQUAL_INT(4, result);
}

void test_wav_ir_float32_values_preserved(void) {
    static uint8_t buf[512];
    float samples[3] = {0.75f, -0.25f, 0.0f};
    int len = build_float32_wav(buf, sizeof(buf), samples, 3, 48000);

    float taps[3];
    dsp_parse_wav_ir(buf, len, taps, 3, 48000);

    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.75f, taps[0]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, -0.25f, taps[1]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.0f, taps[2]);
}

void test_wav_ir_clamped_to_max_taps(void) {
    // File has 8 samples but maxTaps=4 → returns 4
    static uint8_t buf[512];
    float samples[8] = {1.0f, 0.5f, 0.25f, 0.125f, 0.0625f, 0.03f, 0.015f, 0.008f};
    int len = build_float32_wav(buf, sizeof(buf), samples, 8, 48000);

    float taps[4];
    int result = dsp_parse_wav_ir(buf, len, taps, 4, 48000);
    TEST_ASSERT_EQUAL_INT(4, result);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 1.0f, taps[0]);
}

void test_wav_ir_zero_max_taps_returns_minus1(void) {
    static uint8_t buf[512];
    float samples[4] = {1.0f, 0.5f, -0.5f, -1.0f};
    int len = build_float32_wav(buf, sizeof(buf), samples, 4, 48000);

    float taps[4];
    int result = dsp_parse_wav_ir(buf, len, taps, 0, 48000);
    TEST_ASSERT_EQUAL_INT(-1, result);
}

void test_wav_ir_null_taps_buf_returns_minus1(void) {
    static uint8_t buf[512];
    float samples[4] = {1.0f, 0.5f, -0.5f, -1.0f};
    int len = build_float32_wav(buf, sizeof(buf), samples, 4, 48000);

    int result = dsp_parse_wav_ir(buf, len, nullptr, 8, 48000);
    TEST_ASSERT_EQUAL_INT(-1, result);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_wav_ir_null_data_returns_minus1);
    RUN_TEST(test_wav_ir_too_short_returns_minus1);
    RUN_TEST(test_wav_ir_missing_riff_returns_minus1);
    RUN_TEST(test_wav_ir_missing_wave_tag_returns_minus1);
    RUN_TEST(test_wav_ir_stereo_returns_minus1);
    RUN_TEST(test_wav_ir_sample_rate_mismatch_returns_minus1);
    RUN_TEST(test_wav_ir_pcm16_valid_returns_tap_count);
    RUN_TEST(test_wav_ir_pcm16_normalisation_correct);
    RUN_TEST(test_wav_ir_float32_valid_returns_tap_count);
    RUN_TEST(test_wav_ir_float32_values_preserved);
    RUN_TEST(test_wav_ir_clamped_to_max_taps);
    RUN_TEST(test_wav_ir_zero_max_taps_returns_minus1);
    RUN_TEST(test_wav_ir_null_taps_buf_returns_minus1);

    return UNITY_END();
}
