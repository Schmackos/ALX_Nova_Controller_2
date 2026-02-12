#include <unity.h>
#include <math.h>
#include <string.h>

// Include DSP sources directly (test_build_src = no)
#include "../../lib/esp_dsp_lite/src/dsps_biquad_f32_ansi.c"
#include "../../lib/esp_dsp_lite/src/dsps_fir_f32_ansi.c"
#include "../../lib/esp_dsp_lite/src/dsps_fir_init_f32.c"
#include "../../src/dsp_biquad_gen.c"

#include "../../src/dsp_pipeline.h"
#include "../../src/dsp_coefficients.h"
#include "../../src/dsp_rew_parser.h"

// Include sources
#include "../../src/dsp_coefficients.cpp"
#include "../../src/dsp_pipeline.cpp"
#include "../../src/dsp_rew_parser.cpp"

#define FLOAT_TOL 0.001f

void setUp(void) {
    dsp_init();
}
void tearDown(void) {}

// ===== APO Parser Tests =====

void test_apo_single_peq(void) {
    DspChannelConfig ch;
    dsp_init_channel(ch);

    const char *text = "Filter 1: ON PK Fc 1000.00 Hz Gain 3.0 dB Q 2.00\n";
    int added = dsp_parse_apo_filters(text, ch, 48000);

    TEST_ASSERT_EQUAL_INT(1, added);
    TEST_ASSERT_EQUAL_INT(1, ch.stageCount);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_PEQ, ch.stages[0].type);
    TEST_ASSERT_TRUE(ch.stages[0].enabled);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 1000.0f, ch.stages[0].biquad.frequency);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 3.0f, ch.stages[0].biquad.gain);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 2.0f, ch.stages[0].biquad.Q);
}

void test_apo_filter_off(void) {
    DspChannelConfig ch;
    dsp_init_channel(ch);

    const char *text = "Filter 1: OFF PK Fc 500.00 Hz Gain -2.0 dB Q 1.50\n";
    int added = dsp_parse_apo_filters(text, ch, 48000);

    TEST_ASSERT_EQUAL_INT(1, added);
    TEST_ASSERT_FALSE(ch.stages[0].enabled);
}

void test_apo_multiple_types(void) {
    DspChannelConfig ch;
    dsp_init_channel(ch);

    const char *text =
        "Filter 1: ON HPQ Fc 30.00 Hz Q 0.707\n"
        "Filter 2: ON PK Fc 100.00 Hz Gain -3.0 dB Q 4.00\n"
        "Filter 3: ON LPQ Fc 18000.00 Hz Q 0.707\n"
        "Filter 4: ON LSC Fc 200.00 Hz Gain 3.0 dB Q 0.707\n"
        "Filter 5: ON HSC Fc 8000.00 Hz Gain -2.0 dB Q 0.707\n"
        "Filter 6: ON NO Fc 60.00 Hz Q 10.00\n"
        "Filter 7: ON AP Fc 1000.00 Hz Q 0.707\n";

    int added = dsp_parse_apo_filters(text, ch, 48000);
    TEST_ASSERT_EQUAL_INT(7, added);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_HPF, ch.stages[0].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_PEQ, ch.stages[1].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, ch.stages[2].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LOW_SHELF, ch.stages[3].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_HIGH_SHELF, ch.stages[4].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_NOTCH, ch.stages[5].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_ALLPASS, ch.stages[6].type);
}

void test_apo_comment_and_blank_lines(void) {
    DspChannelConfig ch;
    dsp_init_channel(ch);

    const char *text =
        "# This is a comment\n"
        "\n"
        "; Another comment\n"
        "Filter 1: ON PK Fc 500.00 Hz Gain 1.0 dB Q 1.00\n"
        "\n";

    int added = dsp_parse_apo_filters(text, ch, 48000);
    TEST_ASSERT_EQUAL_INT(1, added);
}

void test_apo_max_stages_limit(void) {
    DspChannelConfig ch;
    dsp_init_channel(ch);

    // Generate 25 filters (exceeds DSP_MAX_STAGES=20)
    char text[2048] = {0};
    int pos = 0;
    for (int i = 1; i <= 25; i++) {
        pos += snprintf(text + pos, sizeof(text) - pos,
                        "Filter %d: ON PK Fc %d.00 Hz Gain 1.0 dB Q 1.00\n", i, i * 100);
    }

    int added = dsp_parse_apo_filters(text, ch, 48000);
    TEST_ASSERT_EQUAL_INT(DSP_MAX_STAGES, added);
    TEST_ASSERT_EQUAL_INT(DSP_MAX_STAGES, ch.stageCount);
}

void test_apo_malformed_input(void) {
    DspChannelConfig ch;
    dsp_init_channel(ch);

    const char *text = "This is not a valid filter line\nRandom text\n";
    int added = dsp_parse_apo_filters(text, ch, 48000);
    TEST_ASSERT_EQUAL_INT(0, added);
}

void test_apo_lp_hp_variants(void) {
    DspChannelConfig ch;
    dsp_init_channel(ch);

    const char *text =
        "Filter 1: ON LP Fc 100.00 Hz Q 0.707\n"
        "Filter 2: ON HP Fc 100.00 Hz Q 0.707\n"
        "Filter 3: ON LS Fc 100.00 Hz Gain 3.0 dB Q 0.707\n"
        "Filter 4: ON HS Fc 100.00 Hz Gain 3.0 dB Q 0.707\n";

    int added = dsp_parse_apo_filters(text, ch, 48000);
    TEST_ASSERT_EQUAL_INT(4, added);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LPF, ch.stages[0].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_HPF, ch.stages[1].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_LOW_SHELF, ch.stages[2].type);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_HIGH_SHELF, ch.stages[3].type);
}

// ===== miniDSP Parser Tests =====

void test_minidsp_single_biquad(void) {
    DspChannelConfig ch;
    dsp_init_channel(ch);

    const char *text = "biquad1, b0=1.0012345, b1=-1.9876543, b2=0.9864321, a1=-1.9876543, a2=0.9876666\n";
    int added = dsp_parse_minidsp_biquads(text, ch);

    TEST_ASSERT_EQUAL_INT(1, added);
    TEST_ASSERT_EQUAL(DSP_BIQUAD_CUSTOM, ch.stages[0].type);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0012345f, ch.stages[0].biquad.coeffs[0]); // b0
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, -1.9876543f, ch.stages[0].biquad.coeffs[1]); // b1
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.9864321f, ch.stages[0].biquad.coeffs[2]); // b2
    // a1/a2 are sign-negated by parser
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.9876543f, ch.stages[0].biquad.coeffs[3]); // -(-1.987) = 1.987
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, -0.9876666f, ch.stages[0].biquad.coeffs[4]); // -(0.987) = -0.987
}

void test_minidsp_multiple_biquads(void) {
    DspChannelConfig ch;
    dsp_init_channel(ch);

    const char *text =
        "biquad1, b0=1.0, b1=0.0, b2=0.0, a1=0.0, a2=0.0\n"
        "biquad2, b0=0.5, b1=0.3, b2=0.2, a1=-0.1, a2=0.05\n";
    int added = dsp_parse_minidsp_biquads(text, ch);

    TEST_ASSERT_EQUAL_INT(2, added);
}

void test_minidsp_malformed(void) {
    DspChannelConfig ch;
    dsp_init_channel(ch);

    const char *text = "not a biquad line\n";
    int added = dsp_parse_minidsp_biquads(text, ch);
    TEST_ASSERT_EQUAL_INT(0, added);
}

// ===== FIR Text Parser Tests =====

void test_fir_text_valid(void) {
    float tapsBuf[DSP_MAX_FIR_TAPS] = {};

    const char *text = "0.5\n0.3\n0.2\n0.1\n-0.1\n";
    int taps = dsp_parse_fir_text(text, tapsBuf, DSP_MAX_FIR_TAPS);

    TEST_ASSERT_EQUAL_INT(5, taps);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.5f, tapsBuf[0]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.3f, tapsBuf[1]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.2f, tapsBuf[2]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.1f, tapsBuf[3]);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, -0.1f, tapsBuf[4]);
}

void test_fir_text_with_comments(void) {
    float tapsBuf[DSP_MAX_FIR_TAPS] = {};

    const char *text = "# FIR taps\n1.0\n; comment\n0.5\n\n0.25\n";
    int taps = dsp_parse_fir_text(text, tapsBuf, DSP_MAX_FIR_TAPS);

    TEST_ASSERT_EQUAL_INT(3, taps);
}

void test_fir_text_empty(void) {
    float tapsBuf[DSP_MAX_FIR_TAPS] = {};
    const char *text = "\n\n# just comments\n";
    int taps = dsp_parse_fir_text(text, tapsBuf, DSP_MAX_FIR_TAPS);
    TEST_ASSERT_EQUAL_INT(0, taps);
}

void test_fir_text_truncation(void) {
    // Generate more than DSP_MAX_FIR_TAPS values
    char text[8192] = {0};
    int pos = 0;
    for (int i = 0; i < DSP_MAX_FIR_TAPS + 50; i++) {
        pos += snprintf(text + pos, sizeof(text) - pos, "0.001\n");
    }

    float tapsBuf[DSP_MAX_FIR_TAPS] = {};
    int taps = dsp_parse_fir_text(text, tapsBuf, DSP_MAX_FIR_TAPS);
    TEST_ASSERT_EQUAL_INT(DSP_MAX_FIR_TAPS, taps);
}

// ===== WAV Parser Tests =====

// Helper: build a minimal mono 16-bit WAV in memory
static int build_test_wav_16bit(uint8_t *buf, int maxLen, uint32_t sampleRate, int numSamples) {
    if (maxLen < 44 + numSamples * 2) return -1;

    // RIFF header
    memcpy(buf, "RIFF", 4);
    *(uint32_t *)(buf + 4) = 36 + numSamples * 2;
    memcpy(buf + 8, "WAVE", 4);

    // fmt chunk
    memcpy(buf + 12, "fmt ", 4);
    *(uint32_t *)(buf + 16) = 16; // chunk size
    *(uint16_t *)(buf + 20) = 1;  // PCM
    *(uint16_t *)(buf + 22) = 1;  // mono
    *(uint32_t *)(buf + 24) = sampleRate;
    *(uint32_t *)(buf + 28) = sampleRate * 2; // byte rate
    *(uint16_t *)(buf + 32) = 2;  // block align
    *(uint16_t *)(buf + 34) = 16; // bits per sample

    // data chunk
    memcpy(buf + 36, "data", 4);
    *(uint32_t *)(buf + 40) = numSamples * 2;

    // Sample data: impulse
    int16_t *samples = (int16_t *)(buf + 44);
    for (int i = 0; i < numSamples; i++) {
        samples[i] = (i == 0) ? 16384 : 0; // Impulse at sample 0
    }

    return 44 + numSamples * 2;
}

void test_wav_16bit_mono(void) {
    uint8_t wavBuf[1024];
    int len = build_test_wav_16bit(wavBuf, sizeof(wavBuf), 48000, 10);
    TEST_ASSERT_TRUE(len > 0);

    float tapsBuf[DSP_MAX_FIR_TAPS] = {};
    int taps = dsp_parse_wav_ir(wavBuf, len, tapsBuf, DSP_MAX_FIR_TAPS, 48000);

    TEST_ASSERT_EQUAL_INT(10, taps);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, tapsBuf[0]); // 16384/32768 = 0.5
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_TOL, 0.0f, tapsBuf[1]);
}

void test_wav_wrong_sample_rate(void) {
    uint8_t wavBuf[1024];
    int len = build_test_wav_16bit(wavBuf, sizeof(wavBuf), 44100, 10);

    float tapsBuf[DSP_MAX_FIR_TAPS] = {};
    int taps = dsp_parse_wav_ir(wavBuf, len, tapsBuf, DSP_MAX_FIR_TAPS, 48000); // Expect 48kHz
    TEST_ASSERT_EQUAL_INT(-1, taps); // Mismatch → reject
}

void test_wav_too_short(void) {
    uint8_t wavBuf[20] = {0};
    float tapsBuf[DSP_MAX_FIR_TAPS] = {};
    int taps = dsp_parse_wav_ir(wavBuf, 20, tapsBuf, DSP_MAX_FIR_TAPS, 48000);
    TEST_ASSERT_EQUAL_INT(-1, taps);
}

void test_wav_not_riff(void) {
    uint8_t wavBuf[64] = {0};
    memcpy(wavBuf, "NOTARIFF", 8);
    float tapsBuf[DSP_MAX_FIR_TAPS] = {};
    int taps = dsp_parse_wav_ir(wavBuf, 64, tapsBuf, DSP_MAX_FIR_TAPS, 48000);
    TEST_ASSERT_EQUAL_INT(-1, taps);
}

// ===== APO Export Tests =====

void test_apo_export_peq(void) {
    DspChannelConfig ch;
    dsp_init_channel(ch);

    DspStage &s = ch.stages[0];
    dsp_init_stage(s, DSP_BIQUAD_PEQ);
    s.biquad.frequency = 1000.0f;
    s.biquad.gain = 3.0f;
    s.biquad.Q = 2.0f;
    ch.stageCount = 1;

    char buf[256];
    int written = dsp_export_apo(ch, 48000, buf, sizeof(buf));

    TEST_ASSERT_TRUE(written > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "Filter 1"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "ON"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "PK"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "1000.00"));
}

void test_apo_roundtrip(void) {
    // Export → Import → compare
    DspChannelConfig ch1;
    dsp_init_channel(ch1);

    const char *originalText =
        "Filter 1: ON PK Fc 1000.00 Hz Gain 3.0 dB Q 2.0000\n"
        "Filter 2: OFF PK Fc 500.00 Hz Gain -2.0 dB Q 1.5000\n";

    dsp_parse_apo_filters(originalText, ch1, 48000);
    TEST_ASSERT_EQUAL_INT(2, ch1.stageCount);

    // Export
    char exported[512];
    dsp_export_apo(ch1, 48000, exported, sizeof(exported));

    // Re-import
    DspChannelConfig ch2;
    dsp_init_channel(ch2);
    int added = dsp_parse_apo_filters(exported, ch2, 48000);
    TEST_ASSERT_EQUAL_INT(2, added);

    // Compare
    TEST_ASSERT_EQUAL(ch1.stages[0].type, ch2.stages[0].type);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, ch1.stages[0].biquad.frequency, ch2.stages[0].biquad.frequency);
    TEST_ASSERT_EQUAL(ch1.stages[1].enabled, ch2.stages[1].enabled);
}

// ===== miniDSP Export Tests =====

void test_minidsp_export_sign_convention(void) {
    DspChannelConfig ch;
    dsp_init_channel(ch);

    DspStage &s = ch.stages[0];
    dsp_init_stage(s, DSP_BIQUAD_CUSTOM);
    s.biquad.coeffs[0] = 1.0f;  // b0
    s.biquad.coeffs[1] = -0.5f; // b1
    s.biquad.coeffs[2] = 0.3f;  // b2
    s.biquad.coeffs[3] = -0.8f; // a1 (standard)
    s.biquad.coeffs[4] = 0.4f;  // a2 (standard)
    ch.stageCount = 1;

    char buf[256];
    int written = dsp_export_minidsp(ch, buf, sizeof(buf));

    TEST_ASSERT_TRUE(written > 0);
    // miniDSP negates a1/a2, so exported a1 should be 0.8, a2 should be -0.4
    TEST_ASSERT_NOT_NULL(strstr(buf, "biquad1"));
}

void test_minidsp_roundtrip(void) {
    // Create a custom biquad
    DspChannelConfig ch1;
    dsp_init_channel(ch1);
    DspStage &s = ch1.stages[0];
    dsp_init_stage(s, DSP_BIQUAD_CUSTOM);
    s.biquad.coeffs[0] = 1.001f;
    s.biquad.coeffs[1] = -1.987f;
    s.biquad.coeffs[2] = 0.986f;
    s.biquad.coeffs[3] = -1.987f;
    s.biquad.coeffs[4] = 0.987f;
    ch1.stageCount = 1;

    // Export
    char exported[256];
    dsp_export_minidsp(ch1, exported, sizeof(exported));

    // Re-import
    DspChannelConfig ch2;
    dsp_init_channel(ch2);
    int added = dsp_parse_minidsp_biquads(exported, ch2);
    TEST_ASSERT_EQUAL_INT(1, added);

    // Compare coefficients (b0, b1, b2 should match; a1, a2 double-negated = original)
    TEST_ASSERT_FLOAT_WITHIN(0.001f, ch1.stages[0].biquad.coeffs[0], ch2.stages[0].biquad.coeffs[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, ch1.stages[0].biquad.coeffs[1], ch2.stages[0].biquad.coeffs[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, ch1.stages[0].biquad.coeffs[2], ch2.stages[0].biquad.coeffs[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, ch1.stages[0].biquad.coeffs[3], ch2.stages[0].biquad.coeffs[3]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, ch1.stages[0].biquad.coeffs[4], ch2.stages[0].biquad.coeffs[4]);
}

// ===== Runner =====

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // APO parser
    RUN_TEST(test_apo_single_peq);
    RUN_TEST(test_apo_filter_off);
    RUN_TEST(test_apo_multiple_types);
    RUN_TEST(test_apo_comment_and_blank_lines);
    RUN_TEST(test_apo_max_stages_limit);
    RUN_TEST(test_apo_malformed_input);
    RUN_TEST(test_apo_lp_hp_variants);

    // miniDSP parser
    RUN_TEST(test_minidsp_single_biquad);
    RUN_TEST(test_minidsp_multiple_biquads);
    RUN_TEST(test_minidsp_malformed);

    // FIR text parser
    RUN_TEST(test_fir_text_valid);
    RUN_TEST(test_fir_text_with_comments);
    RUN_TEST(test_fir_text_empty);
    RUN_TEST(test_fir_text_truncation);

    // WAV parser
    RUN_TEST(test_wav_16bit_mono);
    RUN_TEST(test_wav_wrong_sample_rate);
    RUN_TEST(test_wav_too_short);
    RUN_TEST(test_wav_not_riff);

    // APO export
    RUN_TEST(test_apo_export_peq);
    RUN_TEST(test_apo_roundtrip);

    // miniDSP export
    RUN_TEST(test_minidsp_export_sign_convention);
    RUN_TEST(test_minidsp_roundtrip);

    return UNITY_END();
}
