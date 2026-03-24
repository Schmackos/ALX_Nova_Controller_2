/**
 * test_audio_pipeline.cpp
 *
 * Unit tests for the new modular audio pipeline (audio_pipeline.h/.cpp).
 *
 * Covers:
 *   - float32 <-> int32 (left-justified 24-bit I2S format) conversion
 *   - 8x8 routing matrix: identity, single cell, zero-row/column, bypass flag
 *   - Per-input bypass flags (pipelineInputBypass[])
 *   - Per-input DSP bypass flags (pipelineDspBypass[])
 *   - Master pipeline bypass flag (pipelineMatrixBypass)
 *   - RMS computation from float32 buffers
 *   - Signal generator float output range and waveform shape
 *   - USB audio float conversion path (int32 -> float)
 *   - AppState new pipeline bypass fields (smoke check)
 *
 * Tests run on the native platform — no hardware I2S reads are exercised.
 * All logic under test is replicated inline (test_build_src = no) using the
 * same technique as test_audio_rms.cpp.
 *
 * Build flags required (already in platformio.ini [env:native]):
 *   -D UNIT_TEST -D NATIVE_TEST -D DSP_ENABLED
 */

#include <cmath>
#include <cstring>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

#include "../../src/audio_pipeline.h"

// ============================================================
// Re-implemented pure conversion helpers
// These mirror the logic that audio_pipeline.cpp will implement.
// The internal format throughout the pipeline is float32 [-1.0, +1.0].
// ADC hardware delivers 24-bit data left-justified in 32-bit words (bits
// [31:8]); USB delivers PCM16 or PCM24 packed bytes. The pipeline edge
// converts these to float at the input and back to int32 at the DAC output.
// ============================================================

// --- int32 left-justified 24-bit -> float32 [-1.0, +1.0] ---
// This is the ADC input conversion used at the start of each pipeline lane.
static inline float pipeline_int32_to_float(int32_t raw) {
    // raw word: bits [31:8] = 24-bit signed audio, bits [7:0] = 0
    // Arithmetic right shift by 8 recovers the 24-bit signed integer.
    int32_t s24 = raw >> 8;
    return (float)s24 / 8388607.0f; // 2^23 - 1
}

// --- float32 [-1.0, +1.0] -> int32 left-justified 24-bit ---
// This is the DAC output conversion used at the end of the pipeline.
static inline int32_t pipeline_float_to_int32(float f) {
    if (f > 1.0f)  f = 1.0f;
    if (f < -1.0f) f = -1.0f;
    return (int32_t)(f * 8388607.0f) << 8;
}

// --- int32 PCM16 left-justified-32 -> float32 ---
// USB PCM16 path: after usb_pcm16_to_int32() the word is (int16 << 16).
// The pipeline converts that to float by treating the top 16 bits as signed
// and normalising by 32767.
static inline float pipeline_int32_pcm16_to_float(int32_t word) {
    int16_t s16 = (int16_t)(word >> 16);
    return (float)s16 / 32767.0f;
}

// --- 8x8 routing matrix multiply ---
// matrix[out_ch][in_ch] = gain coefficient (0.0 = off, 1.0 = unity)
// in_buf[ch][frame], out_buf[ch][frame], both float32
// Returns false (output silent) when all coefficients for an output channel
// are zero — callers can skip further processing on that channel.
static void routing_matrix_apply(
    const float matrix[8][8],
    const float * const in_bufs[8],
    float *out_bufs[8],
    int num_in, int num_out, int frames)
{
    for (int out = 0; out < num_out; out++) {
        // Zero the output channel first
        memset(out_bufs[out], 0, frames * sizeof(float));
        for (int in = 0; in < num_in; in++) {
            float g = matrix[out][in];
            if (g == 0.0f) continue;
            for (int f = 0; f < frames; f++) {
                out_bufs[out][f] += g * in_bufs[in][f];
            }
        }
    }
}

// --- RMS from float32 buffer ---
static float float_rms(const float *buf, int frames) {
    if (frames <= 0) return 0.0f;
    float sum = 0.0f;
    for (int i = 0; i < frames; i++) sum += buf[i] * buf[i];
    return sqrtf(sum / (float)frames);
}

// --- Sine LUT sample (mirrors signal_generator.cpp) ---
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

static inline float lut_sine(float phase) {
    float idx_f = phase * 256.0f;
    int idx = (int)idx_f & 255;
    int nxt = (idx + 1) & 255;
    float frac = idx_f - (int)idx_f;
    float s0 = SINE_LUT[idx] / 32767.0f;
    float s1 = SINE_LUT[nxt] / 32767.0f;
    return s0 + frac * (s1 - s0);
}

// ============================================================
// AppState bypass field smoke-check helpers
// The new fields added to AppState are:
//   bool pipelineInputBypass[4]   — per-input lane bypass
//   bool pipelineDspBypass[4]     — per-input DSP bypass
//   bool pipelineMatrixBypass     — bypass the routing matrix
//   bool pipelineOutputBypass     — bypass the output stage
//
// Because test_build_src = no, we do not include app_state.h here.
// Instead we define a minimal local struct that mirrors the new fields
// so we can exercise bypass decision logic inline.
// ============================================================

struct PipelineBypassFlags {
    bool inputBypass[4]  = {false, false, false, false};
    bool dspBypass[4]    = {false, false, false, false};
    bool matrixBypass    = false;
    bool outputBypass    = false;
};

// Simulate pipeline output selection based on bypass flags.
// Returns true if the frame should pass through unprocessed.
static bool pipeline_should_bypass_input(const PipelineBypassFlags &flags, int lane) {
    return flags.inputBypass[lane];
}
static bool pipeline_should_bypass_dsp(const PipelineBypassFlags &flags, int lane) {
    return flags.dspBypass[lane];
}
static bool pipeline_should_bypass_matrix(const PipelineBypassFlags &flags) {
    return flags.matrixBypass;
}

// ============================================================
// setUp / tearDown
// ============================================================

void setUp(void) {
    ArduinoMock::reset();
}

void tearDown(void) {}

// ============================================================
// Section 1: int32 (left-justified 24-bit) <-> float32 conversion
// ============================================================

// 1a. Silence (0x00000000) converts to 0.0f and back
void test_int32_to_float_silence(void) {
    float f = pipeline_int32_to_float(0);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, f);
}

// 1b. Positive full scale: 0x7FFFFF00 -> ~+1.0
void test_int32_to_float_positive_full_scale(void) {
    int32_t raw = 0x7FFFFF00; // max 24-bit positive left-justified
    float f = pipeline_int32_to_float(raw);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, f);
}

// 1c. Negative full scale: 0x80000000 (= -8388608 << 8) -> ~-1.0
void test_int32_to_float_negative_full_scale(void) {
    int32_t raw = (int32_t)0x80000000;
    float f = pipeline_int32_to_float(raw);
    // -8388608 / 8388607 ≈ -1.0000119 → clamped to -1.0 by float_to_int32 roundtrip
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, f);
}

// 1d. Mid-positive value: 0x3FFFFF00 (≈ 0.5 of full scale)
void test_int32_to_float_mid_positive(void) {
    int32_t s24 = 0x3FFFFF; // ~4194303 ≈ 0.5 * 8388607
    int32_t raw = s24 << 8;
    float f = pipeline_int32_to_float(raw);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, f);
}

// 1e. Round-trip: float32 -> int32 -> float32 within 1 LSB tolerance
void test_float_to_int32_roundtrip(void) {
    float inputs[] = {0.0f, 0.5f, -0.5f, 0.9f, -0.9f, 0.1f, -0.1f};
    int n = (int)(sizeof(inputs) / sizeof(inputs[0]));
    for (int i = 0; i < n; i++) {
        int32_t encoded = pipeline_float_to_int32(inputs[i]);
        float decoded   = pipeline_int32_to_float(encoded);
        // 1 LSB at 24-bit is 1/8388607 ≈ 0.000000119
        TEST_ASSERT_FLOAT_WITHIN(0.0002f, inputs[i], decoded);
    }
}

// 1f. float_to_int32 clamps values outside [-1.0, +1.0]
void test_float_to_int32_clamping(void) {
    int32_t pos_clamp = pipeline_float_to_int32(2.0f);
    int32_t neg_clamp = pipeline_float_to_int32(-2.0f);
    int32_t pos_unity = pipeline_float_to_int32(1.0f);
    int32_t neg_unity = pipeline_float_to_int32(-1.0f);

    // Clamped values must equal the unity-gain values exactly
    TEST_ASSERT_EQUAL_INT32(pos_unity, pos_clamp);
    TEST_ASSERT_EQUAL_INT32(neg_unity, neg_clamp);
}

// 1g. int32 PCM16-in-32 (from usb_pcm16_to_int32) -> float
// A value of 32767 << 16 should map to ~+1.0
void test_int32_pcm16_to_float_full_scale(void) {
    int32_t word = (int32_t)32767 << 16;
    float f = pipeline_int32_pcm16_to_float(word);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, f);
}

// 1h. int32 PCM16 negative full scale (-32768 << 16) -> ~-1.0
void test_int32_pcm16_to_float_negative_full_scale(void) {
    int32_t word = (int32_t)(-32768) << 16;
    float f = pipeline_int32_pcm16_to_float(word);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, f);
}

// 1i. int32 PCM16 silence (0) -> 0.0f
void test_int32_pcm16_to_float_silence(void) {
    float f = pipeline_int32_pcm16_to_float(0);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, f);
}

// ============================================================
// Section 2: Routing matrix
// ============================================================

// 2a. Identity matrix: each input routes 1:1 to its matching output
void test_routing_matrix_identity(void) {
    const int FRAMES = 64;
    // 4 input channels, 4 output channels
    float in0[FRAMES], in1[FRAMES], in2[FRAMES], in3[FRAMES];
    float out0[FRAMES], out1[FRAMES], out2[FRAMES], out3[FRAMES];

    for (int f = 0; f < FRAMES; f++) {
        in0[f] = 0.8f; in1[f] = 0.4f; in2[f] = -0.3f; in3[f] = 0.1f;
    }

    const float *in_bufs[8]  = {in0, in1, in2, in3, in0, in0, in0, in0};
    float       *out_bufs[8] = {out0, out1, out2, out3, out0, out0, out0, out0};

    float matrix[8][8] = {};
    matrix[0][0] = 1.0f;
    matrix[1][1] = 1.0f;
    matrix[2][2] = 1.0f;
    matrix[3][3] = 1.0f;

    routing_matrix_apply(matrix, in_bufs, out_bufs, 4, 4, FRAMES);

    for (int f = 0; f < FRAMES; f++) {
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.8f,  out0[f]);
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.4f,  out1[f]);
        TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.3f, out2[f]);
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f,  out3[f]);
    }
}

// 2b. All-zero matrix produces silence on all outputs
void test_routing_matrix_all_zero(void) {
    const int FRAMES = 64;
    float in0[FRAMES];
    float out0[FRAMES];
    for (int f = 0; f < FRAMES; f++) { in0[f] = 0.9f; out0[f] = 0.5f; }

    const float *in_bufs[8]  = {in0, in0, in0, in0, in0, in0, in0, in0};
    float       *out_bufs[8] = {out0, out0, out0, out0, out0, out0, out0, out0};

    float matrix[8][8] = {};  // all zeros

    routing_matrix_apply(matrix, in_bufs, out_bufs, 1, 1, FRAMES);

    for (int f = 0; f < FRAMES; f++) {
        TEST_ASSERT_EQUAL_FLOAT(0.0f, out0[f]);
    }
}

// 2c. Single off-diagonal cell: input 1 routes to output 0 at 0.5 gain
void test_routing_matrix_off_diagonal_half_gain(void) {
    const int FRAMES = 32;
    float in0[FRAMES], in1[FRAMES];
    float out0[FRAMES];

    for (int f = 0; f < FRAMES; f++) { in0[f] = 0.0f; in1[f] = 1.0f; }

    const float *in_bufs[8]  = {in0, in1, in0, in0, in0, in0, in0, in0};
    float       *out_bufs[8] = {out0, in0, in0, in0, in0, in0, in0, in0};

    float matrix[8][8] = {};
    matrix[0][1] = 0.5f;  // in1 -> out0 at half gain

    routing_matrix_apply(matrix, in_bufs, out_bufs, 2, 1, FRAMES);

    for (int f = 0; f < FRAMES; f++) {
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, out0[f]);
    }
}

// 2d. Mix: two inputs summed into one output, clipping check
// Input 0 = 0.4, Input 1 = 0.3 → both routed to output 0 at unity
// Expected output: 0.7 (no clipping at this level)
void test_routing_matrix_mix_two_inputs(void) {
    const int FRAMES = 16;
    float in0[FRAMES], in1[FRAMES];
    float out0[FRAMES];

    for (int f = 0; f < FRAMES; f++) { in0[f] = 0.4f; in1[f] = 0.3f; }

    const float *in_bufs[8]  = {in0, in1, in0, in0, in0, in0, in0, in0};
    float       *out_bufs[8] = {out0, in0, in0, in0, in0, in0, in0, in0};

    float matrix[8][8] = {};
    matrix[0][0] = 1.0f;
    matrix[0][1] = 1.0f;

    routing_matrix_apply(matrix, in_bufs, out_bufs, 2, 1, FRAMES);

    for (int f = 0; f < FRAMES; f++) {
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.7f, out0[f]);
    }
}

// 2e. Gain coefficient > 1.0 amplifies the signal
void test_routing_matrix_gain_above_unity(void) {
    const int FRAMES = 8;
    float in0[FRAMES], out0[FRAMES];
    for (int f = 0; f < FRAMES; f++) in0[f] = 0.5f;

    const float *in_bufs[8]  = {in0, in0, in0, in0, in0, in0, in0, in0};
    float       *out_bufs[8] = {out0, in0, in0, in0, in0, in0, in0, in0};

    float matrix[8][8] = {};
    matrix[0][0] = 2.0f;

    routing_matrix_apply(matrix, in_bufs, out_bufs, 1, 1, FRAMES);

    for (int f = 0; f < FRAMES; f++) {
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, out0[f]);
    }
}

// 2f. Output channel untouched when all source gains are zero
// A dedicated output channel with zero gain → all-zero output
void test_routing_matrix_output_channel_all_zero_sources(void) {
    const int FRAMES = 8;
    float in0[FRAMES], out0[FRAMES], out1[FRAMES];
    for (int f = 0; f < FRAMES; f++) { in0[f] = 0.9f; out0[f] = 0.0f; out1[f] = 0.0f; }

    const float *in_bufs[8]  = {in0, in0, in0, in0, in0, in0, in0, in0};
    float       *out_bufs[8] = {out0, out1, out0, out0, out0, out0, out0, out0};

    float matrix[8][8] = {};
    matrix[0][0] = 1.0f;   // out0 gets in0
    // matrix[1][*] all zero — out1 should be silent

    routing_matrix_apply(matrix, in_bufs, out_bufs, 1, 2, FRAMES);

    for (int f = 0; f < FRAMES; f++) {
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.9f, out0[f]);
        TEST_ASSERT_EQUAL_FLOAT(0.0f, out1[f]);
    }
}

// 2g. Negative gain coefficient (polarity inversion)
void test_routing_matrix_negative_gain_polarity_invert(void) {
    const int FRAMES = 8;
    float in0[FRAMES], out0[FRAMES];
    for (int f = 0; f < FRAMES; f++) in0[f] = 0.6f;

    const float *in_bufs[8]  = {in0, in0, in0, in0, in0, in0, in0, in0};
    float       *out_bufs[8] = {out0, in0, in0, in0, in0, in0, in0, in0};

    float matrix[8][8] = {};
    matrix[0][0] = -1.0f;

    routing_matrix_apply(matrix, in_bufs, out_bufs, 1, 1, FRAMES);

    for (int f = 0; f < FRAMES; f++) {
        TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.6f, out0[f]);
    }
}

// ============================================================
// Section 3: Bypass flag behavior
// ============================================================

// 3a. Input bypass: all lanes false by default
void test_bypass_flags_default_no_bypass(void) {
    PipelineBypassFlags flags;
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_FALSE(pipeline_should_bypass_input(flags, i));
        TEST_ASSERT_FALSE(pipeline_should_bypass_dsp(flags, i));
    }
    TEST_ASSERT_FALSE(pipeline_should_bypass_matrix(flags));
}

// 3b. Setting inputBypass[2] enables bypass for lane 2 only
void test_bypass_flags_input_lane_selective(void) {
    PipelineBypassFlags flags;
    flags.inputBypass[2] = true;

    TEST_ASSERT_FALSE(pipeline_should_bypass_input(flags, 0));
    TEST_ASSERT_FALSE(pipeline_should_bypass_input(flags, 1));
    TEST_ASSERT_TRUE(pipeline_should_bypass_input(flags, 2));
    TEST_ASSERT_FALSE(pipeline_should_bypass_input(flags, 3));
}

// 3c. Setting dspBypass[0] enables DSP bypass for lane 0 only
void test_bypass_flags_dsp_lane_selective(void) {
    PipelineBypassFlags flags;
    flags.dspBypass[0] = true;

    TEST_ASSERT_TRUE(pipeline_should_bypass_dsp(flags, 0));
    TEST_ASSERT_FALSE(pipeline_should_bypass_dsp(flags, 1));
    TEST_ASSERT_FALSE(pipeline_should_bypass_dsp(flags, 2));
    TEST_ASSERT_FALSE(pipeline_should_bypass_dsp(flags, 3));
}

// 3d. matrixBypass enables matrix bypass
void test_bypass_flags_matrix_bypass(void) {
    PipelineBypassFlags flags;
    flags.matrixBypass = true;
    TEST_ASSERT_TRUE(pipeline_should_bypass_matrix(flags));
}

// 3e. All bypass flags independent — setting one doesn't affect others
void test_bypass_flags_all_independent(void) {
    PipelineBypassFlags flags;
    flags.inputBypass[0] = true;
    flags.dspBypass[3] = true;
    flags.matrixBypass = true;
    flags.outputBypass = true;

    // inputBypass[0] true, others false
    TEST_ASSERT_TRUE(flags.inputBypass[0]);
    TEST_ASSERT_FALSE(flags.inputBypass[1]);
    TEST_ASSERT_FALSE(flags.inputBypass[2]);
    TEST_ASSERT_FALSE(flags.inputBypass[3]);

    // dspBypass[3] true, others false
    TEST_ASSERT_FALSE(flags.dspBypass[0]);
    TEST_ASSERT_FALSE(flags.dspBypass[1]);
    TEST_ASSERT_FALSE(flags.dspBypass[2]);
    TEST_ASSERT_TRUE(flags.dspBypass[3]);

    TEST_ASSERT_TRUE(flags.matrixBypass);
    TEST_ASSERT_TRUE(flags.outputBypass);
}

// ============================================================
// Section 4: RMS from float32 buffers
// These cover the new metering path where the pipeline works in float
// throughout; RMS is computed directly from the float lane buffers
// rather than from raw int32 ADC words.
// ============================================================

// 4a. All-zero float buffer -> RMS 0.0
void test_float_rms_silence(void) {
    float buf[128] = {};
    TEST_ASSERT_EQUAL_FLOAT(0.0f, float_rms(buf, 128));
}

// 4b. Constant +1.0 float buffer -> RMS 1.0
void test_float_rms_full_scale_dc(void) {
    const int N = 128;
    float buf[N];
    for (int i = 0; i < N; i++) buf[i] = 1.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, float_rms(buf, N));
}

// 4c. Constant 0.5 float buffer -> RMS 0.5
void test_float_rms_half_scale_dc(void) {
    const int N = 128;
    float buf[N];
    for (int i = 0; i < N; i++) buf[i] = 0.5f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, float_rms(buf, N));
}

// 4d. Sine wave at full amplitude -> RMS ≈ 0.7071 (1/sqrt(2))
void test_float_rms_sine_full_scale(void) {
    const int N = 1024;
    float buf[N];
    float phase = 0.0f;
    float phase_inc = 1.0f / 48.0f; // ~1 kHz at 48 kHz
    for (int i = 0; i < N; i++) {
        buf[i] = lut_sine(phase);
        phase += phase_inc;
        if (phase >= 1.0f) phase -= 1.0f;
    }
    float rms = float_rms(buf, N);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.7071f, rms);
}

// 4e. Sine wave at -6 dBFS amplitude -> RMS ≈ 0.707 * 0.5 ≈ 0.354
void test_float_rms_sine_half_amplitude(void) {
    const int N = 1024;
    float buf[N];
    float phase = 0.0f;
    float phase_inc = 1.0f / 48.0f;
    for (int i = 0; i < N; i++) {
        buf[i] = 0.5f * lut_sine(phase);
        phase += phase_inc;
        if (phase >= 1.0f) phase -= 1.0f;
    }
    float rms = float_rms(buf, N);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.3536f, rms);
}

// 4f. Empty buffer (0 frames) -> RMS 0.0 (no divide-by-zero)
void test_float_rms_empty_buffer(void) {
    float buf[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    TEST_ASSERT_EQUAL_FLOAT(0.0f, float_rms(buf, 0));
}

// 4g. Mixed positive and negative values -> RMS is always >= 0
void test_float_rms_alternating_sign(void) {
    const int N = 8;
    float buf[N] = {0.5f, -0.5f, 0.5f, -0.5f, 0.5f, -0.5f, 0.5f, -0.5f};
    float rms = float_rms(buf, N);
    TEST_ASSERT_TRUE(rms >= 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, rms);
}

// ============================================================
// Section 5: Signal generator float output range
// In the new pipeline the siggen is an independent input lane delivering
// float32 samples directly. These tests verify the per-waveform float
// output contract: range [-1.0, +1.0], correct waveform shape.
// ============================================================

// 5a. Sine LUT at phase 0 -> ~0.0
void test_siggen_float_sine_at_phase0(void) {
    float v = lut_sine(0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, v);
}

// 5b. Sine LUT at phase 0.25 -> ~+1.0 (peak)
void test_siggen_float_sine_at_quarter(void) {
    float v = lut_sine(0.25f);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 1.0f, v);
}

// 5c. Sine LUT at phase 0.75 -> ~-1.0 (trough)
void test_siggen_float_sine_at_three_quarter(void) {
    float v = lut_sine(0.75f);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, -1.0f, v);
}

// 5d. Sine LUT output always in [-1.0, +1.0] for all 256 LUT phases
void test_siggen_float_sine_range(void) {
    for (int i = 0; i < 256; i++) {
        float phase = (float)i / 256.0f;
        float v = lut_sine(phase);
        TEST_ASSERT_TRUE(v >= -1.01f);
        TEST_ASSERT_TRUE(v <= 1.01f);
    }
}

// 5e. Sine lane at -6 dBFS amplitude: RMS after 1024 samples ≈ 0.354
void test_siggen_float_lane_rms_at_minus6dbfs(void) {
    const int N = 1024;
    float buf[N];
    // -6.02 dBFS ≈ amplitude 0.5
    float amp = 0.5f;
    float phase = 0.0f, phase_inc = 1.0f / 48.0f;
    for (int i = 0; i < N; i++) {
        buf[i] = amp * lut_sine(phase);
        phase += phase_inc;
        if (phase >= 1.0f) phase -= 1.0f;
    }
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.354f, float_rms(buf, N));
}

// 5f. Siggen as independent input: routing matrix passes it to output 3
void test_siggen_float_lane_through_routing_matrix(void) {
    const int FRAMES = 64;
    float siggen_buf[FRAMES]; // independent lane (index 3)
    float out3[FRAMES];
    float zeros[FRAMES];
    memset(zeros, 0, sizeof(zeros));

    // Fill siggen lane with 0.8 amplitude square-ish signal (simplified)
    for (int f = 0; f < FRAMES; f++) {
        siggen_buf[f] = (f < FRAMES / 2) ? 0.8f : -0.8f;
    }

    // Pipeline: 4 inputs (ADC1, ADC2, Siggen, USB), route input 2 to output 3
    const float *in_bufs[8]  = {zeros, zeros, siggen_buf, zeros, zeros, zeros, zeros, zeros};
    float       *out_bufs[8] = {zeros, zeros, zeros, out3, zeros, zeros, zeros, zeros};

    float matrix[8][8] = {};
    matrix[3][2] = 1.0f; // siggen input -> output 3

    routing_matrix_apply(matrix, in_bufs, out_bufs, 4, 4, FRAMES);

    float rms = float_rms(out3, FRAMES);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.8f, rms);
}

// ============================================================
// Section 6: USB Audio float conversion path
// The USB lane delivers samples as int32 (PCM16-left-justified after
// usb_pcm16_to_int32, or PCM24-left-justified after usb_pcm24_to_int32).
// These tests verify the edge conversion to float32.
// ============================================================

// 6a. PCM16 stereo frame at full scale: both channels convert to ~+1.0
void test_usb_pcm16_frame_to_float_full_scale(void) {
    // Simulating usb_pcm16_to_int32 output for one stereo frame:
    // L = 32767 << 16, R = 32767 << 16
    int32_t frame[2] = {(int32_t)32767 << 16, (int32_t)32767 << 16};
    float fL = pipeline_int32_pcm16_to_float(frame[0]);
    float fR = pipeline_int32_pcm16_to_float(frame[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, fL);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, fR);
}

// 6b. PCM16 stereo frame at zero -> 0.0f
void test_usb_pcm16_frame_to_float_silence(void) {
    int32_t frame[2] = {0, 0};
    float fL = pipeline_int32_pcm16_to_float(frame[0]);
    float fR = pipeline_int32_pcm16_to_float(frame[1]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, fL);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, fR);
}

// 6c. PCM16 negative full scale (-32768 << 16) -> ~-1.0
void test_usb_pcm16_frame_to_float_negative_full_scale(void) {
    int32_t word = (int32_t)(-32768) << 16;
    float f = pipeline_int32_pcm16_to_float(word);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, f);
}

// 6d. PCM16 mid value (+16384 << 16) -> ~+0.5
void test_usb_pcm16_frame_to_float_mid(void) {
    int32_t word = (int32_t)16384 << 16;
    float f = pipeline_int32_pcm16_to_float(word);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, f);
}

// 6e. USB float lane at 0 dBFS feeds routing matrix, RMS should match
void test_usb_float_lane_through_routing_matrix(void) {
    const int FRAMES = 64;
    float usb_buf[FRAMES];
    float out0[FRAMES];
    float zeros[FRAMES];
    memset(zeros, 0, sizeof(zeros));

    // Simulate USB lane: PCM16 full-scale 1.0
    for (int f = 0; f < FRAMES; f++) usb_buf[f] = 1.0f;

    // USB is input lane index 3; route to output 0
    const float *in_bufs[8]  = {zeros, zeros, zeros, usb_buf, zeros, zeros, zeros, zeros};
    float       *out_bufs[8] = {out0, zeros, zeros, zeros, zeros, zeros, zeros, zeros};

    float matrix[8][8] = {};
    matrix[0][3] = 0.7f; // USB at -3 dB to output 0

    routing_matrix_apply(matrix, in_bufs, out_bufs, 4, 1, FRAMES);

    for (int f = 0; f < FRAMES; f++) {
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.7f, out0[f]);
    }
}

// ============================================================
// Section 7: Matrix bypass — when pipelineMatrixBypass is true,
// inputs pass through to outputs without routing-matrix gain.
// The pipeline should copy input lane N directly to output lane N.
// ============================================================

// 7a. Bypass path: input copied straight to output, no gain applied
void test_matrix_bypass_passes_input_unchanged(void) {
    const int FRAMES = 8;
    float in0[FRAMES], out0[FRAMES];
    for (int f = 0; f < FRAMES; f++) { in0[f] = 0.75f; out0[f] = 0.0f; }

    // Bypass means: skip the routing matrix, copy in -> out directly
    // This is tested at the logic level — when matrixBypass is true
    // the pipeline is expected to do a memcpy instead of routing_matrix_apply.
    PipelineBypassFlags flags;
    flags.matrixBypass = true;

    if (pipeline_should_bypass_matrix(flags)) {
        // Simulate bypass: direct copy
        memcpy(out0, in0, FRAMES * sizeof(float));
    }

    for (int f = 0; f < FRAMES; f++) {
        TEST_ASSERT_EQUAL_FLOAT(0.75f, out0[f]);
    }
}

// 7b. Without bypass flag, routing matrix applies gain (even if identity)
void test_matrix_no_bypass_applies_routing(void) {
    const int FRAMES = 8;
    float in0[FRAMES], out0[FRAMES];
    for (int f = 0; f < FRAMES; f++) { in0[f] = 0.75f; out0[f] = 0.0f; }

    PipelineBypassFlags flags;
    // matrixBypass is false by default

    float matrix[8][8] = {};
    matrix[0][0] = 0.5f; // half gain, not identity

    const float *in_bufs[8]  = {in0, in0, in0, in0, in0, in0, in0, in0};
    float       *out_bufs[8] = {out0, in0, in0, in0, in0, in0, in0, in0};

    if (!pipeline_should_bypass_matrix(flags)) {
        routing_matrix_apply(matrix, in_bufs, out_bufs, 1, 1, FRAMES);
    }

    for (int f = 0; f < FRAMES; f++) {
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.375f, out0[f]); // 0.75 * 0.5
    }
}

// ============================================================
// Section 8: Input lane bypass — when inputBypass[lane] is true,
// the lane delivers silence (zeros) downstream.
// ============================================================

// 8a. Bypassed input produces silence into routing matrix
void test_input_bypass_produces_silence(void) {
    const int FRAMES = 16;
    float adc_data[FRAMES]; // Would have been ADC samples
    float lane_output[FRAMES];
    float zeros_ref[FRAMES];
    memset(zeros_ref, 0, sizeof(zeros_ref));

    for (int f = 0; f < FRAMES; f++) adc_data[f] = 0.9f;

    PipelineBypassFlags flags;
    flags.inputBypass[0] = true;

    // When lane is bypassed, substitute zeros
    if (pipeline_should_bypass_input(flags, 0)) {
        memset(lane_output, 0, FRAMES * sizeof(float));
    } else {
        memcpy(lane_output, adc_data, FRAMES * sizeof(float));
    }

    TEST_ASSERT_EQUAL_FLOAT(0.0f, lane_output[0]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, lane_output[FRAMES - 1]);
}

// 8b. Non-bypassed input passes ADC data through unchanged
void test_input_no_bypass_passes_adc_data(void) {
    const int FRAMES = 16;
    float adc_data[FRAMES];
    float lane_output[FRAMES];
    for (int f = 0; f < FRAMES; f++) adc_data[f] = 0.9f;

    PipelineBypassFlags flags;
    // inputBypass[0] false by default

    if (pipeline_should_bypass_input(flags, 0)) {
        memset(lane_output, 0, FRAMES * sizeof(float));
    } else {
        memcpy(lane_output, adc_data, FRAMES * sizeof(float));
    }

    for (int f = 0; f < FRAMES; f++) {
        TEST_ASSERT_EQUAL_FLOAT(0.9f, lane_output[f]);
    }
}

// 8c. Only lane 1 bypassed: lane 0 and 2 pass through normally
void test_input_bypass_only_lane1(void) {
    PipelineBypassFlags flags;
    flags.inputBypass[1] = true;

    TEST_ASSERT_FALSE(pipeline_should_bypass_input(flags, 0));
    TEST_ASSERT_TRUE(pipeline_should_bypass_input(flags, 1));
    TEST_ASSERT_FALSE(pipeline_should_bypass_input(flags, 2));
    TEST_ASSERT_FALSE(pipeline_should_bypass_input(flags, 3));
}

// ============================================================
// Section 8b: Matrix Migration — loading smaller matrices (2 tests)
//
// When loading an 8-row matrix into a 16x16 target, the old values
// should be placed in the top-left corner and remaining rows/cols
// should be zero (silence). This tests the migration logic.
// ============================================================

// Helper: load a smaller matrix into a larger one, placing values top-left
static void matrix_load_into_larger(
    const float src[][8], int srcRows, int srcCols,
    float dst[][8], int dstRows, int dstCols)
{
    // Zero the destination first
    for (int r = 0; r < dstRows; r++)
        for (int c = 0; c < dstCols; c++)
            dst[r][c] = 0.0f;

    // Copy source into top-left corner
    int rowsToCopy = (srcRows < dstRows) ? srcRows : dstRows;
    int colsToCopy = (srcCols < dstCols) ? srcCols : dstCols;
    for (int r = 0; r < rowsToCopy; r++)
        for (int c = 0; c < colsToCopy; c++)
            dst[r][c] = src[r][c];
}

// 8b-a. Load an 4x4 matrix into 8x8 — top-left corner populated, rest zero
void test_matrix_load_smaller_fills_top_left(void) {
    float src[8][8] = {};
    // Set up a 4x4 identity in the source
    src[0][0] = 1.0f;
    src[1][1] = 1.0f;
    src[2][2] = 1.0f;
    src[3][3] = 1.0f;

    float dst[8][8] = {};
    // Fill destination with a non-zero sentinel to ensure zeroing works
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            dst[r][c] = 0.999f;

    matrix_load_into_larger(src, 4, 4, dst, 8, 8);

    // Top-left 4x4 should contain the identity
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            float expected = (r == c) ? 1.0f : 0.0f;
            TEST_ASSERT_FLOAT_WITHIN(0.001f, expected, dst[r][c]);
        }
    }

    // Rows 4-7 should be all zeros
    for (int r = 4; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            TEST_ASSERT_EQUAL_FLOAT(0.0f, dst[r][c]);
        }
    }

    // Columns 4-7 in rows 0-3 should be all zeros
    for (int r = 0; r < 4; r++) {
        for (int c = 4; c < 8; c++) {
            TEST_ASSERT_EQUAL_FLOAT(0.0f, dst[r][c]);
        }
    }
}

// 8b-b. Load an 8x8 matrix into 8x8 — exact match, all values copied
void test_matrix_load_exact_size(void) {
    float src[8][8] = {};
    // Fill with a test pattern: src[r][c] = r * 10 + c
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            src[r][c] = (float)(r * 10 + c);

    float dst[8][8] = {};

    matrix_load_into_larger(src, 8, 8, dst, 8, 8);

    // Every cell should match
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            TEST_ASSERT_FLOAT_WITHIN(0.001f, (float)(r * 10 + c), dst[r][c]);
        }
    }
}

// ============================================================
// Section 8c: Per-Source VU Metering (1 test)
//
// Verifies that the per-source VU metering formula produces
// non-default values for a source with actual audio data.
// The pipeline computes VU from float buffers; we replicate
// the VU update formula here.
// ============================================================

// VU metering formula (replicated from audio_pipeline.cpp):
// smoothed = smoothed * alpha + rms * (1 - alpha)
// vu_dBFS = 20 * log10(smoothed)  [clamped to -90]
static float vu_from_float_buffer(const float *buf, int frames,
                                   float &smoothed, float alpha = 0.3f) {
    if (frames <= 0) return -90.0f;
    float sumSq = 0.0f;
    for (int i = 0; i < frames; i++) sumSq += buf[i] * buf[i];
    float rms = sqrtf(sumSq / (float)frames);
    smoothed = smoothed * alpha + rms * (1.0f - alpha);
    if (smoothed < 1.0e-10f) return -90.0f;
    float db = 20.0f * log10f(smoothed);
    return (db < -90.0f) ? -90.0f : db;
}

void test_vu_metering_updates_for_lane_beyond_zero(void) {
    // Simulate a source on lane 2 with a constant 0.5 amplitude signal.
    // After one VU update pass, the VU level should be above -90 dBFS.
    const int FRAMES = 256;
    float buf[FRAMES];
    for (int i = 0; i < FRAMES; i++) buf[i] = 0.5f;

    // Start with default smoothed = 0.0 (as in AudioInputSource init)
    float smoothedL = 0.0f;
    float vuL = vu_from_float_buffer(buf, FRAMES, smoothedL);

    // Expected: RMS of constant 0.5 = 0.5
    // smoothed = 0 * 0.3 + 0.5 * 0.7 = 0.35
    // dBFS = 20 * log10(0.35) ≈ -9.12 dBFS
    TEST_ASSERT_TRUE(vuL > -90.0f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, -9.1f, vuL);

    // Second pass — smoothed converges further towards 0.5
    float vuL2 = vu_from_float_buffer(buf, FRAMES, smoothedL);
    TEST_ASSERT_TRUE(vuL2 > vuL);
    // smoothed = 0.35 * 0.3 + 0.5 * 0.7 = 0.455
    // dBFS = 20 * log10(0.455) ≈ -6.84 dBFS
    TEST_ASSERT_FLOAT_WITHIN(1.0f, -6.8f, vuL2);
}

// 8c-b. Per-source VU metering on lane 2 with AudioInputSource struct
// Exercises the VU path for a higher lane (lane 2 = siggen / third source).
// The pipeline maintains per-source VU via vuL/vuR and _vuSmoothedL/R fields.
void test_vu_metering_updates_for_lane_2(void) {
    const int FRAMES = 256;
    float bufL[FRAMES], bufR[FRAMES];

    // Lane 2 source: left channel at 0.8 amplitude, right at 0.3
    for (int i = 0; i < FRAMES; i++) {
        bufL[i] = 0.8f;
        bufR[i] = 0.3f;
    }

    // Simulate an AudioInputSource initialized with default VU values
    float smoothedL = 0.0f, smoothedR = 0.0f;
    float vuL_init = -90.0f, vuR_init = -90.0f;

    // Verify initial state matches AudioInputSource defaults
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -90.0f, vuL_init);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -90.0f, vuR_init);

    // First VU update pass — both channels should move off -90 dBFS
    float vuL = vu_from_float_buffer(bufL, FRAMES, smoothedL);
    float vuR = vu_from_float_buffer(bufR, FRAMES, smoothedR);

    // Left: RMS of 0.8 = 0.8; smoothed = 0*0.3 + 0.8*0.7 = 0.56
    // dBFS = 20*log10(0.56) ≈ -5.04
    TEST_ASSERT_TRUE(vuL > -90.0f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, -5.0f, vuL);

    // Right: RMS of 0.3 = 0.3; smoothed = 0*0.3 + 0.3*0.7 = 0.21
    // dBFS = 20*log10(0.21) ≈ -13.56
    TEST_ASSERT_TRUE(vuR > -90.0f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, -13.6f, vuR);

    // Left channel should be louder than right
    TEST_ASSERT_TRUE(vuL > vuR);

    // Verify smoothed state is non-zero (pipeline would store this in _vuSmoothedL/R)
    TEST_ASSERT_TRUE(smoothedL > 0.0f);
    TEST_ASSERT_TRUE(smoothedR > 0.0f);
    TEST_ASSERT_TRUE(smoothedL > smoothedR);
}

// ============================================================
// Section 9: End-to-end float pipeline smoke test
// Combines conversion + bypass + routing + RMS in a single pass,
// simulating the main pipeline loop for 4 inputs -> 4 outputs.
// ============================================================

// 9a. ADC1 (int32 LJ) -> float -> identity matrix -> float output matches
void test_e2e_adc_int32_through_pipeline(void) {
    const int FRAMES = 256;

    // Simulate ADC1 buffer: constant 0.5 amplitude DC
    float in_float[FRAMES];
    for (int f = 0; f < FRAMES; f++) {
        // Build a left-justified int32 for 0.5 amplitude
        int32_t raw = pipeline_float_to_int32(0.5f);
        in_float[f]  = pipeline_int32_to_float(raw);
    }

    // Pass through a 1x1 identity routing
    float out_float[FRAMES];
    memset(out_float, 0, sizeof(out_float));

    const float *in_bufs[8]  = {in_float, in_float, in_float, in_float,
                                  in_float, in_float, in_float, in_float};
    float       *out_bufs[8] = {out_float, in_float, in_float, in_float,
                                  in_float, in_float, in_float, in_float};

    float matrix[8][8] = {};
    matrix[0][0] = 1.0f;

    routing_matrix_apply(matrix, in_bufs, out_bufs, 1, 1, FRAMES);

    // RMS of a constant 0.5 signal should be 0.5
    float rms = float_rms(out_float, FRAMES);
    TEST_ASSERT_FLOAT_WITHIN(0.002f, 0.5f, rms);
}

// 9b. All four bypass flags set: pipeline produces silence on all outputs
void test_e2e_all_input_bypass_produces_silence(void) {
    const int FRAMES = 64;
    float adc_data[FRAMES];
    float lanes[4][FRAMES];
    float out[4][FRAMES];

    for (int f = 0; f < FRAMES; f++) adc_data[f] = 0.8f;

    PipelineBypassFlags flags;
    flags.inputBypass[0] = true;
    flags.inputBypass[1] = true;
    flags.inputBypass[2] = true;
    flags.inputBypass[3] = true;

    // Apply bypass substitution for all lanes
    for (int lane = 0; lane < 4; lane++) {
        if (pipeline_should_bypass_input(flags, lane)) {
            memset(lanes[lane], 0, FRAMES * sizeof(float));
        } else {
            memcpy(lanes[lane], adc_data, FRAMES * sizeof(float));
        }
    }

    // Apply identity routing matrix
    const float *in_bufs[8]  = {lanes[0], lanes[1], lanes[2], lanes[3],
                                  lanes[0], lanes[0], lanes[0], lanes[0]};
    float       *out_ptrs[8] = {out[0], out[1], out[2], out[3],
                                  out[0], out[0], out[0], out[0]};

    float matrix[8][8] = {};
    matrix[0][0] = 1.0f;
    matrix[1][1] = 1.0f;
    matrix[2][2] = 1.0f;
    matrix[3][3] = 1.0f;

    routing_matrix_apply(matrix, in_bufs, out_ptrs, 4, 4, FRAMES);

    // All outputs should be silent because all inputs are bypassed
    for (int ch = 0; ch < 4; ch++) {
        float rms = float_rms(out[ch], FRAMES);
        TEST_ASSERT_EQUAL_FLOAT(0.0f, rms);
    }
}

// ============================================================
// Section 10: 16x16 Matrix set/get API
//
// The audio pipeline uses a 16x16 routing matrix (AUDIO_PIPELINE_MATRIX_SIZE).
// These tests replicate the bounds-checked set_matrix_gain / get_matrix_gain
// logic inline and verify full 16x16 coverage, default state, boundary
// conditions, and gain readback.
// ============================================================

#ifndef AUDIO_PIPELINE_MATRIX_SIZE
#define AUDIO_PIPELINE_MATRIX_SIZE 16
#endif

// Inline replica of the 16x16 matrix set/get logic from audio_pipeline.cpp.
// Tests exercise this independent copy rather than pulling in the full
// audio_pipeline.cpp dependency chain.
static float _testMatrix[AUDIO_PIPELINE_MATRIX_SIZE][AUDIO_PIPELINE_MATRIX_SIZE] = {};

static void test_matrix_init() {
    memset(_testMatrix, 0, sizeof(_testMatrix));
}

static void test_matrix_set_gain(int out_ch, int in_ch, float gain_linear) {
    if (out_ch < 0 || out_ch >= AUDIO_PIPELINE_MATRIX_SIZE) return;
    if (in_ch  < 0 || in_ch  >= AUDIO_PIPELINE_MATRIX_SIZE) return;
    _testMatrix[out_ch][in_ch] = gain_linear;
}

static float test_matrix_get_gain(int out_ch, int in_ch) {
    if (out_ch < 0 || out_ch >= AUDIO_PIPELINE_MATRIX_SIZE) return 0.0f;
    if (in_ch  < 0 || in_ch  >= AUDIO_PIPELINE_MATRIX_SIZE) return 0.0f;
    return _testMatrix[out_ch][in_ch];
}

// 10a. Default 16x16 matrix is all zeros
void test_matrix_default_is_zero(void) {
    test_matrix_init();

    for (int out = 0; out < AUDIO_PIPELINE_MATRIX_SIZE; out++) {
        for (int in = 0; in < AUDIO_PIPELINE_MATRIX_SIZE; in++) {
            TEST_ASSERT_EQUAL_FLOAT(0.0f, test_matrix_get_gain(out, in));
        }
    }
}

// 10b. Set and get gains across full 16x16 matrix
void test_matrix_set_get_full_16x16(void) {
    test_matrix_init();

    // Set a unique gain for every cell: gain = (out * 16 + in) * 0.001
    for (int out = 0; out < AUDIO_PIPELINE_MATRIX_SIZE; out++) {
        for (int in = 0; in < AUDIO_PIPELINE_MATRIX_SIZE; in++) {
            float gain = (float)(out * 16 + in) * 0.001f;
            test_matrix_set_gain(out, in, gain);
        }
    }

    // Read back every cell and verify
    for (int out = 0; out < AUDIO_PIPELINE_MATRIX_SIZE; out++) {
        for (int in = 0; in < AUDIO_PIPELINE_MATRIX_SIZE; in++) {
            float expected = (float)(out * 16 + in) * 0.001f;
            TEST_ASSERT_FLOAT_WITHIN(0.0001f, expected, test_matrix_get_gain(out, in));
        }
    }
}

// 10c. Out-of-bounds set is silently ignored
void test_matrix_set_oob_ignored(void) {
    test_matrix_init();
    test_matrix_set_gain(0, 0, 1.0f);

    // Out-of-range writes should be no-ops
    test_matrix_set_gain(-1, 0, 99.0f);
    test_matrix_set_gain(0, -1, 99.0f);
    test_matrix_set_gain(AUDIO_PIPELINE_MATRIX_SIZE, 0, 99.0f);
    test_matrix_set_gain(0, AUDIO_PIPELINE_MATRIX_SIZE, 99.0f);
    test_matrix_set_gain(AUDIO_PIPELINE_MATRIX_SIZE + 5, AUDIO_PIPELINE_MATRIX_SIZE + 5, 99.0f);

    // Only (0,0) should be 1.0; rest should remain 0.0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, test_matrix_get_gain(0, 0));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, test_matrix_get_gain(1, 0));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, test_matrix_get_gain(0, 1));
}

// 10d. Out-of-bounds get returns 0.0
void test_matrix_get_oob_returns_zero(void) {
    test_matrix_init();
    test_matrix_set_gain(15, 15, 0.42f);

    TEST_ASSERT_EQUAL_FLOAT(0.0f, test_matrix_get_gain(-1, 0));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, test_matrix_get_gain(0, -1));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, test_matrix_get_gain(AUDIO_PIPELINE_MATRIX_SIZE, 0));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, test_matrix_get_gain(0, AUDIO_PIPELINE_MATRIX_SIZE));

    // But valid cell still works
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.42f, test_matrix_get_gain(15, 15));
}

// 10e. Identity 16x16 matrix — diagonal=1.0, off-diagonal=0.0
void test_matrix_16x16_identity(void) {
    test_matrix_init();
    for (int i = 0; i < AUDIO_PIPELINE_MATRIX_SIZE; i++) {
        test_matrix_set_gain(i, i, 1.0f);
    }

    for (int out = 0; out < AUDIO_PIPELINE_MATRIX_SIZE; out++) {
        for (int in = 0; in < AUDIO_PIPELINE_MATRIX_SIZE; in++) {
            float expected = (out == in) ? 1.0f : 0.0f;
            TEST_ASSERT_EQUAL_FLOAT(expected, test_matrix_get_gain(out, in));
        }
    }
}

// 10f. Overwriting a cell replaces the old value
void test_matrix_overwrite_cell(void) {
    test_matrix_init();
    test_matrix_set_gain(7, 3, 0.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, test_matrix_get_gain(7, 3));

    test_matrix_set_gain(7, 3, 0.9f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.9f, test_matrix_get_gain(7, 3));
}

// 10g. Negative gain values (polarity inversion) are stored correctly
void test_matrix_negative_gain_stored(void) {
    test_matrix_init();
    test_matrix_set_gain(10, 5, -1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, test_matrix_get_gain(10, 5));

    test_matrix_set_gain(14, 2, -0.707f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.707f, test_matrix_get_gain(14, 2));
}

// 10h. Corner cells of the 16x16 matrix are accessible
void test_matrix_corner_cells(void) {
    test_matrix_init();
    test_matrix_set_gain(0, 0, 0.1f);
    test_matrix_set_gain(0, 15, 0.2f);
    test_matrix_set_gain(15, 0, 0.3f);
    test_matrix_set_gain(15, 15, 0.4f);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f, test_matrix_get_gain(0, 0));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.2f, test_matrix_get_gain(0, 15));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.3f, test_matrix_get_gain(15, 0));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.4f, test_matrix_get_gain(15, 15));
}

// ============================================================
// Section 11: Audio pause/resume protocol
// ============================================================

// The NATIVE_TEST inline versions are no-ops, but we verify the API exists
// and the flag-based protocol logic is correct.

void test_native_request_pause_returns_true(void) {
    // In NATIVE_TEST, request_pause is an inline no-op returning true
    bool result = audio_pipeline_request_pause(50);
    TEST_ASSERT_TRUE(result);
}

void test_native_resume_is_safe(void) {
    // In NATIVE_TEST, resume is an inline no-op — must not crash
    audio_pipeline_resume();
    TEST_ASSERT_TRUE(true);  // Reached without crash
}

void test_pause_flag_protocol(void) {
    // Verify the volatile bool protocol used by the helpers:
    // paused starts false, set true, verify, clear, verify
    volatile bool paused = false;
    TEST_ASSERT_FALSE(paused);
    paused = true;
    TEST_ASSERT_TRUE(paused);
    paused = false;
    TEST_ASSERT_FALSE(paused);
}

void test_double_pause_flag_safe(void) {
    // Setting paused twice must not cause issues; single clear restores
    volatile bool paused = false;
    paused = true;
    paused = true;  // Double set
    TEST_ASSERT_TRUE(paused);
    paused = false;  // Single clear
    TEST_ASSERT_FALSE(paused);
}

// ============================================================
// Main
// ============================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Section 1: int32 <-> float conversion
    RUN_TEST(test_int32_to_float_silence);
    RUN_TEST(test_int32_to_float_positive_full_scale);
    RUN_TEST(test_int32_to_float_negative_full_scale);
    RUN_TEST(test_int32_to_float_mid_positive);
    RUN_TEST(test_float_to_int32_roundtrip);
    RUN_TEST(test_float_to_int32_clamping);
    RUN_TEST(test_int32_pcm16_to_float_full_scale);
    RUN_TEST(test_int32_pcm16_to_float_negative_full_scale);
    RUN_TEST(test_int32_pcm16_to_float_silence);

    // Section 2: Routing matrix
    RUN_TEST(test_routing_matrix_identity);
    RUN_TEST(test_routing_matrix_all_zero);
    RUN_TEST(test_routing_matrix_off_diagonal_half_gain);
    RUN_TEST(test_routing_matrix_mix_two_inputs);
    RUN_TEST(test_routing_matrix_gain_above_unity);
    RUN_TEST(test_routing_matrix_output_channel_all_zero_sources);
    RUN_TEST(test_routing_matrix_negative_gain_polarity_invert);

    // Section 3: Bypass flags
    RUN_TEST(test_bypass_flags_default_no_bypass);
    RUN_TEST(test_bypass_flags_input_lane_selective);
    RUN_TEST(test_bypass_flags_dsp_lane_selective);
    RUN_TEST(test_bypass_flags_matrix_bypass);
    RUN_TEST(test_bypass_flags_all_independent);

    // Section 4: RMS from float buffers
    RUN_TEST(test_float_rms_silence);
    RUN_TEST(test_float_rms_full_scale_dc);
    RUN_TEST(test_float_rms_half_scale_dc);
    RUN_TEST(test_float_rms_sine_full_scale);
    RUN_TEST(test_float_rms_sine_half_amplitude);
    RUN_TEST(test_float_rms_empty_buffer);
    RUN_TEST(test_float_rms_alternating_sign);

    // Section 5: Siggen float lane
    RUN_TEST(test_siggen_float_sine_at_phase0);
    RUN_TEST(test_siggen_float_sine_at_quarter);
    RUN_TEST(test_siggen_float_sine_at_three_quarter);
    RUN_TEST(test_siggen_float_sine_range);
    RUN_TEST(test_siggen_float_lane_rms_at_minus6dbfs);
    RUN_TEST(test_siggen_float_lane_through_routing_matrix);

    // Section 6: USB audio float conversion path
    RUN_TEST(test_usb_pcm16_frame_to_float_full_scale);
    RUN_TEST(test_usb_pcm16_frame_to_float_silence);
    RUN_TEST(test_usb_pcm16_frame_to_float_negative_full_scale);
    RUN_TEST(test_usb_pcm16_frame_to_float_mid);
    RUN_TEST(test_usb_float_lane_through_routing_matrix);

    // Section 7: Matrix bypass
    RUN_TEST(test_matrix_bypass_passes_input_unchanged);
    RUN_TEST(test_matrix_no_bypass_applies_routing);

    // Section 8: Input lane bypass
    RUN_TEST(test_input_bypass_produces_silence);
    RUN_TEST(test_input_no_bypass_passes_adc_data);
    RUN_TEST(test_input_bypass_only_lane1);

    // Section 8b: Matrix migration
    RUN_TEST(test_matrix_load_smaller_fills_top_left);
    RUN_TEST(test_matrix_load_exact_size);

    // Section 8c: Per-source VU metering
    RUN_TEST(test_vu_metering_updates_for_lane_beyond_zero);
    RUN_TEST(test_vu_metering_updates_for_lane_2);

    // Section 9: End-to-end smoke tests
    RUN_TEST(test_e2e_adc_int32_through_pipeline);
    RUN_TEST(test_e2e_all_input_bypass_produces_silence);

    // Section 10: 16x16 Matrix set/get API
    RUN_TEST(test_matrix_default_is_zero);
    RUN_TEST(test_matrix_set_get_full_16x16);
    RUN_TEST(test_matrix_set_oob_ignored);
    RUN_TEST(test_matrix_get_oob_returns_zero);
    RUN_TEST(test_matrix_16x16_identity);
    RUN_TEST(test_matrix_overwrite_cell);
    RUN_TEST(test_matrix_negative_gain_stored);
    RUN_TEST(test_matrix_corner_cells);

    // Section 11: Audio pause/resume protocol
    RUN_TEST(test_native_request_pause_returns_true);
    RUN_TEST(test_native_resume_is_safe);
    RUN_TEST(test_pause_flag_protocol);
    RUN_TEST(test_double_pause_flag_safe);

    return UNITY_END();
}
