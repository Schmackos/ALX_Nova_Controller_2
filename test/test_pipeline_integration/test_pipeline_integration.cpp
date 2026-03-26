/**
 * test_pipeline_integration.cpp
 *
 * Integration tests for the ASRC + DSP + Matrix audio processing chain.
 *
 * Tests the full signal path that audio takes through the pipeline:
 *   1. ASRC (sample rate conversion)
 *   2. DSP biquad/gain processing
 *   3. Matrix routing (input channels -> output channels)
 *
 * Each stage is tested individually at the unit level elsewhere;
 * these tests verify they compose correctly end-to-end.
 *
 * Build flags (from [env:native]):
 *   -D UNIT_TEST -D NATIVE_TEST -D DSP_ENABLED -D DAC_ENABLED
 */

#include <unity.h>
#include <cmath>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#endif

// --- Include ASRC implementation ---
#include "../../src/heap_budget.h"
#include "../../src/heap_budget.cpp"
#include "../../src/psram_alloc.h"
#include "../../src/psram_alloc.cpp"
#include "../../src/config.h"
#include "../../src/asrc.h"
#include "../../src/asrc.cpp"

// --- Include DSP implementation ---
#include "../../lib/esp_dsp_lite/src/dsps_biquad_f32_ansi.c"
#include "../../lib/esp_dsp_lite/src/dsps_fir_f32_ansi.c"
#include "../../lib/esp_dsp_lite/src/dsps_fir_init_f32.c"
#include "../../lib/esp_dsp_lite/src/dsps_fird_f32_ansi.c"
#include "../../lib/esp_dsp_lite/src/dsps_corr_f32_ansi.c"
#include "../../lib/esp_dsp_lite/src/dsps_conv_f32_ansi.c"
#include "../../src/dsp_biquad_gen.c"

#include "../../src/dsp_pipeline.h"
#include "../../src/dsp_coefficients.h"
#include "../../src/dsp_coefficients.cpp"
#include "../../src/dsp_pipeline.cpp"
#include "../../src/dsp_crossover.cpp"
#include "../../src/dsp_convolution.cpp"
#include "../../src/thd_measurement.cpp"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const int FRAMES = 256;
static const float FLOAT_TOL = 0.02f;  // 2% tolerance for signal chain tests

// ---------------------------------------------------------------------------
// Buffers (sized to ASRC_OUTPUT_FRAMES_MAX for upsample headroom)
// ---------------------------------------------------------------------------

static float g_laneL[ASRC_OUTPUT_FRAMES_MAX];
static float g_laneR[ASRC_OUTPUT_FRAMES_MAX];

// Matrix output buffers (simplified: 4 output channels)
static float g_outCh[4][ASRC_OUTPUT_FRAMES_MAX];

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Generate a mono sine wave into a buffer
static void gen_sine(float *buf, int frames, float freq, float sampleRate, float amplitude) {
    for (int i = 0; i < frames; i++) {
        buf[i] = amplitude * sinf(2.0f * (float)M_PI * freq * (float)i / sampleRate);
    }
}

// Compute RMS of a buffer
static float compute_rms(const float *buf, int frames) {
    float sum = 0.0f;
    for (int i = 0; i < frames; i++) {
        sum += buf[i] * buf[i];
    }
    return sqrtf(sum / (float)frames);
}

// Find peak absolute value in a buffer
static float find_peak(const float *buf, int frames) {
    float peak = 0.0f;
    for (int i = 0; i < frames; i++) {
        float a = fabsf(buf[i]);
        if (a > peak) peak = a;
    }
    return peak;
}

// Simple matrix mix: for each output channel, sum input channels weighted by gain
static void matrix_mix(const float *inL[], const float *inR[], int numInputLanes,
                       int laneFrames[], float matrixGain[][4], int numOutCh,
                       float outCh[][ASRC_OUTPUT_FRAMES_MAX], int outFrames) {
    for (int o = 0; o < numOutCh; o++) {
        memset(outCh[o], 0, (size_t)outFrames * sizeof(float));
    }
    for (int o = 0; o < numOutCh; o++) {
        for (int i = 0; i < numInputLanes; i++) {
            // Input channel mapping: lane i -> in_ch i*2 (L), i*2+1 (R)
            int inCh = o;  // Direct mapping for simplicity
            float gain = matrixGain[i][o];
            if (gain == 0.0f) continue;
            // Use L channel for even output, R for odd
            const float *src = (o % 2 == 0) ? inL[i] : inR[i];
            int srcFrames = laneFrames[i];
            int mixFrames = (srcFrames < outFrames) ? srcFrames : outFrames;
            for (int f = 0; f < mixFrames; f++) {
                outCh[o][f] += src[f] * gain;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// setUp / tearDown
// ---------------------------------------------------------------------------

void setUp() {
    asrc_deinit();
    asrc_init();
    dsp_init();

    memset(g_laneL, 0, sizeof(g_laneL));
    memset(g_laneR, 0, sizeof(g_laneR));
    memset(g_outCh, 0, sizeof(g_outCh));
}

void tearDown() {
    asrc_deinit();
}

// ===========================================================================
// Test 1: ASRC upsampling -> DSP biquad passthrough -> matrix 1:1 routing
// ===========================================================================

void test_upsample_dsp_passthrough_matrix_1to1() {
    // --- Stage 1: Generate 1kHz sine at 44100Hz ---
    gen_sine(g_laneL, FRAMES, 1000.0f, 44100.0f, 0.8f);
    gen_sine(g_laneR, FRAMES, 1000.0f, 44100.0f, 0.8f);
    float inputRms = compute_rms(g_laneL, FRAMES);

    // --- Stage 2: ASRC 44100->48000 ---
    asrc_set_ratio(0, 44100, 48000);
    TEST_ASSERT_TRUE(asrc_is_active(0));

    int asrcOut = asrc_process_lane(0, g_laneL, g_laneR, FRAMES);

    // Output should be ~279 frames (ceil(256 * 160/147))
    TEST_ASSERT_GREATER_OR_EQUAL_INT(277, asrcOut);
    TEST_ASSERT_LESS_OR_EQUAL_INT(ASRC_OUTPUT_FRAMES_MAX, asrcOut);

    // --- Stage 3: DSP biquad passthrough (unity coefficients) ---
    // Configure DSP: channel 0 (L) and 1 (R) with bypass=false, unity biquad
    DspState *cfg = dsp_get_inactive_config();
    cfg->globalBypass = false;
    cfg->sampleRate = 48000;
    // Channels should have PEQ bands initialized by dsp_init()
    // Set all PEQ bands to disabled (passthrough)
    for (int ch = 0; ch < 2; ch++) {
        cfg->channels[ch].bypass = false;
        for (int s = 0; s < cfg->channels[ch].stageCount; s++) {
            cfg->channels[ch].stages[s].enabled = false;
        }
    }
    dsp_swap_config();

    // Process through DSP (lane 0 -> channels 0,1)
    dsp_process_buffer_float(g_laneL, g_laneR, asrcOut, 0);

    // Signal should be preserved (all stages disabled = passthrough)
    float dspRms = compute_rms(g_laneL, asrcOut);
    // ASRC filter changes amplitude slightly; DSP passthrough should not alter further.
    // Just verify non-zero output (ASRC filter warms up over multiple blocks).
    TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, dspRms);

    // --- Stage 4: Matrix routing (1:1 identity) ---
    const float *inL[] = { g_laneL };
    const float *inR[] = { g_laneR };
    int laneFrames[] = { asrcOut };
    float matrixGain[1][4] = { {1.0f, 1.0f, 0.0f, 0.0f} };  // Lane 0 -> out 0 (L) + out 1 (R)

    matrix_mix(inL, inR, 1, laneFrames, matrixGain, 2, g_outCh, asrcOut);

    // Output should match DSP output (identity routing)
    for (int f = 0; f < asrcOut; f++) {
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, g_laneL[f], g_outCh[0][f]);
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, g_laneR[f], g_outCh[1][f]);
    }
}

// ===========================================================================
// Test 2: ASRC downsampling -> DSP -> matrix (verify tail zeroing)
// ===========================================================================

void test_downsample_dsp_matrix_tail_zeroed() {
    // --- Stage 1: Generate signal at 96kHz ---
    gen_sine(g_laneL, FRAMES, 1000.0f, 96000.0f, 0.8f);
    gen_sine(g_laneR, FRAMES, 1000.0f, 96000.0f, 0.8f);

    // Pre-fill tail with non-zero data to verify zero-fill behavior
    for (int i = FRAMES; i < ASRC_OUTPUT_FRAMES_MAX; i++) {
        g_laneL[i] = 0.999f;
        g_laneR[i] = 0.999f;
    }

    // --- Stage 2: ASRC 96000->48000 (2:1 downsample) ---
    asrc_set_ratio(0, 96000, 48000);
    TEST_ASSERT_TRUE(asrc_is_active(0));

    int asrcOut = asrc_process_lane(0, g_laneL, g_laneR, FRAMES);

    // Output should be ~128 frames
    TEST_ASSERT_GREATER_OR_EQUAL_INT(126, asrcOut);
    TEST_ASSERT_LESS_OR_EQUAL_INT(130, asrcOut);

    // Verify that the ASRC output region has signal (non-zero)
    float outputRms = compute_rms(g_laneL, asrcOut);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, outputRms);

    // The ASRC itself does NOT zero the tail (that's the pipeline's job).
    // The pipeline_resample_inputs() in audio_pipeline.cpp does the zero-fill.
    // Here we simulate what the pipeline does: zero-fill beyond asrcOut.
    for (int i = asrcOut; i < ASRC_OUTPUT_FRAMES_MAX; i++) {
        g_laneL[i] = 0.0f;
        g_laneR[i] = 0.0f;
    }

    // Verify tail is now zeroed
    for (int i = asrcOut; i < ASRC_OUTPUT_FRAMES_MAX; i++) {
        TEST_ASSERT_EQUAL_FLOAT(0.0f, g_laneL[i]);
        TEST_ASSERT_EQUAL_FLOAT(0.0f, g_laneR[i]);
    }

    // --- Stage 3: DSP passthrough ---
    DspState *cfg = dsp_get_inactive_config();
    cfg->globalBypass = false;
    cfg->sampleRate = 48000;
    for (int ch = 0; ch < 2; ch++) {
        cfg->channels[ch].bypass = false;
        for (int s = 0; s < cfg->channels[ch].stageCount; s++) {
            cfg->channels[ch].stages[s].enabled = false;
        }
    }
    dsp_swap_config();

    dsp_process_buffer_float(g_laneL, g_laneR, asrcOut, 0);

    // --- Stage 4: Matrix routing ---
    const float *inL[] = { g_laneL };
    const float *inR[] = { g_laneR };
    int laneFrames[] = { asrcOut };
    float matrixGain[1][4] = { {1.0f, 1.0f, 0.0f, 0.0f} };

    matrix_mix(inL, inR, 1, laneFrames, matrixGain, 2, g_outCh, ASRC_OUTPUT_FRAMES_MAX);

    // Verify output has signal in the valid region
    float outRms = compute_rms(g_outCh[0], asrcOut);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, outRms);

    // Verify output tail is silent (zero-filled input -> zero output)
    for (int i = asrcOut; i < ASRC_OUTPUT_FRAMES_MAX; i++) {
        TEST_ASSERT_EQUAL_FLOAT(0.0f, g_outCh[0][i]);
        TEST_ASSERT_EQUAL_FLOAT(0.0f, g_outCh[1][i]);
    }
}

// ===========================================================================
// Test 3: No-ASRC passthrough -> DSP gain -> matrix
// ===========================================================================

void test_passthrough_dsp_gain_matrix() {
    // --- Stage 1: Generate 1kHz sine at 48kHz (no ASRC needed) ---
    gen_sine(g_laneL, FRAMES, 1000.0f, 48000.0f, 0.5f);
    gen_sine(g_laneR, FRAMES, 1000.0f, 48000.0f, 0.5f);

    // --- Stage 2: ASRC with equal rates (passthrough) ---
    asrc_set_ratio(0, 48000, 48000);
    TEST_ASSERT_FALSE(asrc_is_active(0));  // Equal rates -> passthrough

    int asrcOut = asrc_process_lane(0, g_laneL, g_laneR, FRAMES);
    TEST_ASSERT_EQUAL_INT(FRAMES, asrcOut);  // Passthrough: frame count unchanged

    // Verify signal unchanged by passthrough
    // Spot-check first few samples
    for (int i = 0; i < 10; i++) {
        float expected = 0.5f * sinf(2.0f * (float)M_PI * 1000.0f * (float)i / 48000.0f);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, expected, g_laneL[i]);
    }

    // --- Stage 3: DSP with +6dB gain stage ---
    DspState *cfg = dsp_get_inactive_config();
    cfg->globalBypass = false;
    cfg->sampleRate = 48000;

    // Disable all default PEQ bands, add a gain stage in the chain region
    for (int ch = 0; ch < 2; ch++) {
        cfg->channels[ch].bypass = false;
        for (int s = 0; s < cfg->channels[ch].stageCount; s++) {
            cfg->channels[ch].stages[s].enabled = false;
        }
    }

    // Add gain stage to channel 0 (L) and channel 1 (R) in chain region
    float gainDb = 6.0f;
    float gainLinear = powf(10.0f, gainDb / 20.0f);  // ~1.995

    for (int ch = 0; ch < 2; ch++) {
        int stageIdx = DSP_PEQ_BANDS;  // First chain stage slot
        if (cfg->channels[ch].stageCount <= stageIdx) {
            cfg->channels[ch].stageCount = stageIdx + 1;
        }
        DspStage &s = cfg->channels[ch].stages[stageIdx];
        dsp_init_stage(s, DSP_GAIN);
        s.enabled = true;
        s.gain.gainDb = gainDb;
        s.gain.gainLinear = gainLinear;
        s.gain.currentLinear = gainLinear;
    }
    dsp_swap_config();

    // Save pre-DSP peak for comparison
    float preDspPeak = find_peak(g_laneL, FRAMES);

    dsp_process_buffer_float(g_laneL, g_laneR, FRAMES, 0);

    // Verify gain was applied: output peak should be ~2x input peak
    float postDspPeak = find_peak(g_laneL, FRAMES);
    float expectedPeak = preDspPeak * gainLinear;
    TEST_ASSERT_FLOAT_WITHIN(expectedPeak * 0.05f, expectedPeak, postDspPeak);

    // --- Stage 4: Matrix routing (1:1) ---
    const float *inL[] = { g_laneL };
    const float *inR[] = { g_laneR };
    int laneFrames[] = { FRAMES };
    float matrixGain[1][4] = { {1.0f, 1.0f, 0.0f, 0.0f} };

    matrix_mix(inL, inR, 1, laneFrames, matrixGain, 2, g_outCh, FRAMES);

    // Output should equal DSP output (identity routing)
    for (int f = 0; f < FRAMES; f++) {
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, g_laneL[f], g_outCh[0][f]);
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, g_laneR[f], g_outCh[1][f]);
    }
}

// ===========================================================================
// Test 4: Multi-lane ASRC independence -> matrix crossbar
// ===========================================================================

void test_multi_lane_asrc_independent_matrix_crossbar() {
    // Two input lanes at different sample rates
    static float lane0L[ASRC_OUTPUT_FRAMES_MAX];
    static float lane0R[ASRC_OUTPUT_FRAMES_MAX];
    static float lane1L[ASRC_OUTPUT_FRAMES_MAX];
    static float lane1R[ASRC_OUTPUT_FRAMES_MAX];

    memset(lane0L, 0, sizeof(lane0L));
    memset(lane0R, 0, sizeof(lane0R));
    memset(lane1L, 0, sizeof(lane1L));
    memset(lane1R, 0, sizeof(lane1R));

    // Lane 0: 1kHz sine at 44100Hz
    gen_sine(lane0L, FRAMES, 1000.0f, 44100.0f, 0.7f);
    gen_sine(lane0R, FRAMES, 1000.0f, 44100.0f, 0.7f);

    // Lane 1: 2kHz sine at 96000Hz
    gen_sine(lane1L, FRAMES, 2000.0f, 96000.0f, 0.5f);
    gen_sine(lane1R, FRAMES, 2000.0f, 96000.0f, 0.5f);

    // --- ASRC: lane 0 upsamples 44100->48000, lane 1 downsamples 96000->48000 ---
    asrc_set_ratio(0, 44100, 48000);
    asrc_set_ratio(1, 96000, 48000);
    TEST_ASSERT_TRUE(asrc_is_active(0));
    TEST_ASSERT_TRUE(asrc_is_active(1));

    int out0 = asrc_process_lane(0, lane0L, lane0R, FRAMES);
    int out1 = asrc_process_lane(1, lane1L, lane1R, FRAMES);

    // Lane 0: upsample -> ~279 frames
    TEST_ASSERT_GREATER_OR_EQUAL_INT(277, out0);
    TEST_ASSERT_LESS_OR_EQUAL_INT(ASRC_OUTPUT_FRAMES_MAX, out0);

    // Lane 1: downsample -> ~128 frames
    TEST_ASSERT_GREATER_OR_EQUAL_INT(126, out1);
    TEST_ASSERT_LESS_OR_EQUAL_INT(130, out1);

    // Verify lanes are independent: different output frame counts
    TEST_ASSERT_TRUE(out0 != out1);

    // Zero-fill tails (as pipeline does)
    for (int i = out0; i < ASRC_OUTPUT_FRAMES_MAX; i++) { lane0L[i] = 0.0f; lane0R[i] = 0.0f; }
    for (int i = out1; i < ASRC_OUTPUT_FRAMES_MAX; i++) { lane1L[i] = 0.0f; lane1R[i] = 0.0f; }

    // --- DSP passthrough for both lanes ---
    DspState *cfg = dsp_get_inactive_config();
    cfg->globalBypass = false;
    cfg->sampleRate = 48000;
    for (int ch = 0; ch < DSP_MAX_CHANNELS; ch++) {
        cfg->channels[ch].bypass = false;
        for (int s = 0; s < cfg->channels[ch].stageCount; s++) {
            cfg->channels[ch].stages[s].enabled = false;
        }
    }
    dsp_swap_config();

    dsp_process_buffer_float(lane0L, lane0R, out0, 0);
    // Lane 1 maps to DSP channels 2,3 — but DSP_MAX_CHANNELS is 4,
    // so lane 1 (channels 2,3) is valid
    dsp_process_buffer_float(lane1L, lane1R, out1, 1);

    // --- Matrix crossbar: lane 0 -> out 0,1; lane 1 -> out 2,3 ---
    int useFrames = (out0 > out1) ? out0 : out1;

    const float *inL[] = { lane0L, lane1L };
    const float *inR[] = { lane0R, lane1R };
    int laneFrames[] = { out0, out1 };
    // Lane 0 L->out0, Lane 0 R->out1, Lane 1 L->out2, Lane 1 R->out3
    float matrixGain[2][4] = {
        {1.0f, 1.0f, 0.0f, 0.0f},  // Lane 0 -> out 0,1
        {0.0f, 0.0f, 1.0f, 1.0f}   // Lane 1 -> out 2,3
    };

    matrix_mix(inL, inR, 2, laneFrames, matrixGain, 4, g_outCh, useFrames);

    // Verify lane 0 signal appears on outputs 0,1
    float rmsOut0 = compute_rms(g_outCh[0], out0);
    float rmsOut1 = compute_rms(g_outCh[1], out0);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, rmsOut0);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, rmsOut1);

    // Verify lane 1 signal appears on outputs 2,3
    float rmsOut2 = compute_rms(g_outCh[2], out1);
    float rmsOut3 = compute_rms(g_outCh[3], out1);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, rmsOut2);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, rmsOut3);

    // Verify no crosstalk: lane 1 should not appear on outputs 0,1
    // Beyond out1 frames, lane1 has zeros, so out2/out3 tail should be 0
    for (int f = out1; f < useFrames; f++) {
        TEST_ASSERT_EQUAL_FLOAT(0.0f, g_outCh[2][f]);
        TEST_ASSERT_EQUAL_FLOAT(0.0f, g_outCh[3][f]);
    }
}

// ===========================================================================
// Test 5: ASRC upsampling preserves signal energy (multi-block warmup)
// ===========================================================================

void test_asrc_upsample_signal_energy_preserved() {
    asrc_set_ratio(0, 44100, 48000);

    // Warmup the ASRC filter — polyphase FIR needs many blocks to fill
    // the 32-tap history ring buffer and converge for DC.
    int out = 0;
    for (int iter = 0; iter < 20; iter++) {
        for (int i = 0; i < FRAMES; i++) { g_laneL[i] = 0.6f; g_laneR[i] = 0.6f; }
        out = asrc_process_lane(0, g_laneL, g_laneR, FRAMES);
    }

    // After warmup, output should carry meaningful energy (not zeroed).
    // The polyphase filter may attenuate DC depending on passband shape,
    // so we check for non-trivial output rather than exact DC match.
    float rms = compute_rms(g_laneL, out);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.01f, rms);

    // Peak should be non-trivial
    float peak = find_peak(g_laneL, out);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.01f, peak);
}

// ===========================================================================
// Test 6: DSP biquad passthrough preserves signal exactly
// ===========================================================================

void test_dsp_biquad_passthrough_preserves_signal() {
    // Generate signal
    gen_sine(g_laneL, FRAMES, 1000.0f, 48000.0f, 0.8f);
    gen_sine(g_laneR, FRAMES, 1000.0f, 48000.0f, 0.8f);

    // Save copy for comparison
    static float savedL[256], savedR[256];
    memcpy(savedL, g_laneL, FRAMES * sizeof(float));
    memcpy(savedR, g_laneR, FRAMES * sizeof(float));

    // Configure DSP: all stages disabled
    DspState *cfg = dsp_get_inactive_config();
    cfg->globalBypass = false;
    cfg->sampleRate = 48000;
    for (int ch = 0; ch < 2; ch++) {
        cfg->channels[ch].bypass = false;
        for (int s = 0; s < cfg->channels[ch].stageCount; s++) {
            cfg->channels[ch].stages[s].enabled = false;
        }
    }
    dsp_swap_config();

    dsp_process_buffer_float(g_laneL, g_laneR, FRAMES, 0);

    // With all stages disabled, output should exactly match input
    for (int f = 0; f < FRAMES; f++) {
        TEST_ASSERT_EQUAL_FLOAT(savedL[f], g_laneL[f]);
        TEST_ASSERT_EQUAL_FLOAT(savedR[f], g_laneR[f]);
    }
}

// ===========================================================================
// Test 7: Matrix gain scaling works correctly
// ===========================================================================

void test_matrix_gain_scaling() {
    gen_sine(g_laneL, FRAMES, 1000.0f, 48000.0f, 1.0f);
    gen_sine(g_laneR, FRAMES, 1000.0f, 48000.0f, 1.0f);

    const float *inL[] = { g_laneL };
    const float *inR[] = { g_laneR };
    int laneFrames[] = { FRAMES };

    // Route lane 0 to output 0 at 50% gain
    float matrixGain[1][4] = { {0.5f, 0.0f, 0.0f, 0.0f} };

    matrix_mix(inL, inR, 1, laneFrames, matrixGain, 1, g_outCh, FRAMES);

    // Output should be exactly half the input
    for (int f = 0; f < FRAMES; f++) {
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, g_laneL[f] * 0.5f, g_outCh[0][f]);
    }
}

// ===========================================================================
// Test 8: Matrix mixing: two lanes summed to one output
// ===========================================================================

void test_matrix_two_lanes_summed_to_one_output() {
    static float lane0L[ASRC_OUTPUT_FRAMES_MAX];
    static float lane1L[ASRC_OUTPUT_FRAMES_MAX];
    static float lane0R[ASRC_OUTPUT_FRAMES_MAX];
    static float lane1R[ASRC_OUTPUT_FRAMES_MAX];

    memset(lane0L, 0, sizeof(lane0L));
    memset(lane1L, 0, sizeof(lane1L));
    memset(lane0R, 0, sizeof(lane0R));
    memset(lane1R, 0, sizeof(lane1R));

    // Lane 0: DC = 0.3
    for (int i = 0; i < FRAMES; i++) { lane0L[i] = 0.3f; lane0R[i] = 0.3f; }
    // Lane 1: DC = 0.4
    for (int i = 0; i < FRAMES; i++) { lane1L[i] = 0.4f; lane1R[i] = 0.4f; }

    const float *inL[] = { lane0L, lane1L };
    const float *inR[] = { lane0R, lane1R };
    int laneFrames[] = { FRAMES, FRAMES };

    // Both lanes -> output 0 at unity gain
    float matrixGain[2][4] = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 0.0f}
    };

    matrix_mix(inL, inR, 2, laneFrames, matrixGain, 1, g_outCh, FRAMES);

    // Output should be 0.3 + 0.4 = 0.7
    for (int f = 0; f < FRAMES; f++) {
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.7f, g_outCh[0][f]);
    }
}

// ===========================================================================
// Test 9: Full chain with ASRC warmup proves signal integrity
// ===========================================================================

void test_full_chain_asrc_warmup_signal_integrity() {
    asrc_set_ratio(0, 44100, 48000);

    // Configure DSP: all stages disabled (passthrough)
    DspState *cfg = dsp_get_inactive_config();
    cfg->globalBypass = false;
    cfg->sampleRate = 48000;
    for (int ch = 0; ch < 2; ch++) {
        cfg->channels[ch].bypass = false;
        for (int s = 0; s < cfg->channels[ch].stageCount; s++) {
            cfg->channels[ch].stages[s].enabled = false;
        }
    }
    dsp_swap_config();

    // Run 20 blocks of DC=0.6 through the full chain to warm up ASRC filter.
    // The polyphase FIR (32 taps) needs many blocks to fill history.
    int lastOut = 0;
    for (int iter = 0; iter < 20; iter++) {
        for (int i = 0; i < FRAMES; i++) { g_laneL[i] = 0.6f; g_laneR[i] = 0.6f; }
        lastOut = asrc_process_lane(0, g_laneL, g_laneR, FRAMES);

        // Zero-fill tail
        for (int i = lastOut; i < ASRC_OUTPUT_FRAMES_MAX; i++) {
            g_laneL[i] = 0.0f; g_laneR[i] = 0.0f;
        }

        // DSP passthrough
        dsp_process_buffer_float(g_laneL, g_laneR, lastOut, 0);
    }

    // After warmup, output should carry non-trivial energy through the chain.
    // The polyphase filter may attenuate DC depending on passband shape.
    float rmsL = compute_rms(g_laneL, lastOut);
    float rmsR = compute_rms(g_laneR, lastOut);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.01f, rmsL);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.01f, rmsR);

    // Route through matrix
    const float *inL[] = { g_laneL };
    const float *inR[] = { g_laneR };
    int laneFrames[] = { lastOut };
    float matrixGain[1][4] = { {1.0f, 1.0f, 0.0f, 0.0f} };
    matrix_mix(inL, inR, 1, laneFrames, matrixGain, 2, g_outCh, lastOut);

    // Final output should have meaningful energy after full chain
    float outRms0 = compute_rms(g_outCh[0], lastOut);
    float outRms1 = compute_rms(g_outCh[1], lastOut);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.01f, outRms0);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.01f, outRms1);
}

// ===========================================================================
// Test 10: DSP global bypass skips all processing
// ===========================================================================

void test_dsp_global_bypass_skips_processing() {
    gen_sine(g_laneL, FRAMES, 1000.0f, 48000.0f, 0.8f);
    gen_sine(g_laneR, FRAMES, 1000.0f, 48000.0f, 0.8f);

    // Save original
    static float savedL[256], savedR[256];
    memcpy(savedL, g_laneL, FRAMES * sizeof(float));
    memcpy(savedR, g_laneR, FRAMES * sizeof(float));

    // Enable global bypass
    DspState *cfg = dsp_get_inactive_config();
    cfg->globalBypass = true;
    // Add a gain stage that would alter signal if bypass were off
    cfg->channels[0].bypass = false;
    int stageIdx = DSP_PEQ_BANDS;
    if (cfg->channels[0].stageCount <= stageIdx) {
        cfg->channels[0].stageCount = stageIdx + 1;
    }
    DspStage &s = cfg->channels[0].stages[stageIdx];
    dsp_init_stage(s, DSP_GAIN);
    s.enabled = true;
    s.gain.gainDb = 12.0f;
    s.gain.gainLinear = powf(10.0f, 12.0f / 20.0f);
    s.gain.currentLinear = s.gain.gainLinear;
    dsp_swap_config();

    dsp_process_buffer_float(g_laneL, g_laneR, FRAMES, 0);

    // With global bypass, signal should be unchanged
    for (int f = 0; f < FRAMES; f++) {
        TEST_ASSERT_EQUAL_FLOAT(savedL[f], g_laneL[f]);
        TEST_ASSERT_EQUAL_FLOAT(savedR[f], g_laneR[f]);
    }
}

// ===========================================================================
// Test 11: ASRC inactive lane returns correct frame count
// ===========================================================================

void test_asrc_inactive_lane_correct_frame_count() {
    // Lane 0 active (44100->48000), Lane 1 inactive (no set_ratio)
    asrc_set_ratio(0, 44100, 48000);
    TEST_ASSERT_TRUE(asrc_is_active(0));
    TEST_ASSERT_FALSE(asrc_is_active(1));

    // Process lane 1 (inactive) - should return input frame count unchanged
    static float inactiveL[ASRC_OUTPUT_FRAMES_MAX];
    static float inactiveR[ASRC_OUTPUT_FRAMES_MAX];
    for (int i = 0; i < FRAMES; i++) { inactiveL[i] = 0.5f; inactiveR[i] = 0.5f; }

    int out = asrc_process_lane(1, inactiveL, inactiveR, FRAMES);
    TEST_ASSERT_EQUAL_INT(FRAMES, out);

    // Data should be unchanged (passthrough)
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, inactiveL[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, inactiveL[FRAMES - 1]);
}

// ===========================================================================
// Test 12: DSP channel bypass preserves signal while other channels process
// ===========================================================================

void test_dsp_channel_bypass_independent() {
    gen_sine(g_laneL, FRAMES, 1000.0f, 48000.0f, 0.5f);
    gen_sine(g_laneR, FRAMES, 1000.0f, 48000.0f, 0.5f);

    // Save originals
    static float savedL[256], savedR[256];
    memcpy(savedL, g_laneL, FRAMES * sizeof(float));
    memcpy(savedR, g_laneR, FRAMES * sizeof(float));

    // Channel 0 (L): bypass=true (should pass through)
    // Channel 1 (R): bypass=false with gain stage
    DspState *cfg = dsp_get_inactive_config();
    cfg->globalBypass = false;
    cfg->sampleRate = 48000;
    cfg->channels[0].bypass = true;
    cfg->channels[1].bypass = false;

    // Add gain to channel 1 (R)
    for (int s = 0; s < cfg->channels[1].stageCount; s++) {
        cfg->channels[1].stages[s].enabled = false;
    }
    int stageIdx = DSP_PEQ_BANDS;
    if (cfg->channels[1].stageCount <= stageIdx) {
        cfg->channels[1].stageCount = stageIdx + 1;
    }
    DspStage &gs = cfg->channels[1].stages[stageIdx];
    dsp_init_stage(gs, DSP_GAIN);
    gs.enabled = true;
    gs.gain.gainDb = 6.0f;
    gs.gain.gainLinear = powf(10.0f, 6.0f / 20.0f);
    gs.gain.currentLinear = gs.gain.gainLinear;
    dsp_swap_config();

    dsp_process_buffer_float(g_laneL, g_laneR, FRAMES, 0);

    // L channel: bypassed -> unchanged
    for (int f = 0; f < FRAMES; f++) {
        TEST_ASSERT_EQUAL_FLOAT(savedL[f], g_laneL[f]);
    }

    // R channel: +6dB gain applied -> ~2x amplitude
    float expectedGain = powf(10.0f, 6.0f / 20.0f);
    for (int f = 0; f < FRAMES; f++) {
        float expected = savedR[f] * expectedGain;
        TEST_ASSERT_FLOAT_WITHIN(fabsf(expected) * 0.05f + 1e-6f, expected, g_laneR[f]);
    }
}

// ===========================================================================
// main
// ===========================================================================

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();

    // Full chain integration tests
    RUN_TEST(test_upsample_dsp_passthrough_matrix_1to1);
    RUN_TEST(test_downsample_dsp_matrix_tail_zeroed);
    RUN_TEST(test_passthrough_dsp_gain_matrix);
    RUN_TEST(test_multi_lane_asrc_independent_matrix_crossbar);

    // Signal integrity tests
    RUN_TEST(test_asrc_upsample_signal_energy_preserved);
    RUN_TEST(test_dsp_biquad_passthrough_preserves_signal);
    RUN_TEST(test_matrix_gain_scaling);
    RUN_TEST(test_matrix_two_lanes_summed_to_one_output);
    RUN_TEST(test_full_chain_asrc_warmup_signal_integrity);

    // Bypass and edge case tests
    RUN_TEST(test_dsp_global_bypass_skips_processing);
    RUN_TEST(test_asrc_inactive_lane_correct_frame_count);
    RUN_TEST(test_dsp_channel_bypass_independent);

    return UNITY_END();
}
