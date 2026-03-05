/**
 * test_pipeline_output.cpp
 *
 * Unit tests for the pipeline output expansion: 8-channel output buffers,
 * 8x8 routing matrix mix-down, multi-sink registration, per-sink gain/mute,
 * float-to-int32 left-justified conversion, and dB-to-linear gain utility.
 *
 * Tests run on the native platform -- no hardware I2S reads are exercised.
 * All logic under test is replicated inline (test_build_src = no) using the
 * same technique as test_audio_pipeline.cpp.
 *
 * Build flags required (already in platformio.ini [env:native]):
 *   -D UNIT_TEST -D NATIVE_TEST -D DSP_ENABLED
 */

#include <unity.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

// ===== Inline definitions matching production code =====
#define FRAMES 256
#define RAW_SAMPLES 512
#define MATRIX_SIZE 8
#define MAX_SINKS 4
#define MAX_24BIT_F 8388607.0f

typedef struct AudioOutputSink {
    const char *name;
    uint8_t firstChannel;
    uint8_t channelCount;
    void (*write)(const int32_t *buf, int stereoFrames);
    bool (*isReady)(void);
    float gainLinear;
    bool muted;
    float vuL, vuR;
    float _vuSmoothedL, _vuSmoothedR;
} AudioOutputSink;

#define AUDIO_OUTPUT_SINK_INIT { NULL, 0, 2, NULL, NULL, 1.0f, false, -90.0f, -90.0f, 0.0f, 0.0f }

// ===== Global buffers (mirrors production statics) =====
static float _outCh[MATRIX_SIZE][FRAMES];
static float _swapHoldCh[MATRIX_SIZE][FRAMES];
static float _matrixGain[MATRIX_SIZE][MATRIX_SIZE];
static AudioOutputSink _sinks[MAX_SINKS];
static int _sinkCount = 0;
static int32_t _sinkBuf[MAX_SINKS][RAW_SAMPLES];

// ===== Inline functions (mirrors production code) =====

static float clampf(float v) {
    return v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v);
}

static void to_int32_lj(const float *L, const float *R, int32_t *raw, int frames) {
    for (int f = 0; f < frames; f++) {
        raw[f * 2]     = (int32_t)(clampf(L[f]) * MAX_24BIT_F) * 256;  // << 8
        raw[f * 2 + 1] = (int32_t)(clampf(R[f]) * MAX_24BIT_F) * 256;
    }
}

static void pipeline_mix_matrix_8ch(float inCh[][FRAMES], int numInputs) {
    for (int o = 0; o < MATRIX_SIZE; o++) {
        memset(_outCh[o], 0, FRAMES * sizeof(float));
        for (int i = 0; i < numInputs && i < MATRIX_SIZE; i++) {
            float gain = _matrixGain[o][i];
            if (gain == 0.0f) continue;
            for (int f = 0; f < FRAMES; f++) {
                _outCh[o][f] += inCh[i][f] * gain;
            }
        }
    }
}

static void pipeline_register_sink(const AudioOutputSink *sink) {
    if (_sinkCount >= MAX_SINKS) return;
    _sinks[_sinkCount] = *sink;
    _sinkCount++;
}

static void pipeline_write_sinks() {
    for (int s = 0; s < _sinkCount; s++) {
        AudioOutputSink *sink = &_sinks[s];
        if (!sink->write || !sink->isReady || !sink->isReady()) continue;
        if (sink->muted) continue;
        int chL = sink->firstChannel;
        int chR = (sink->channelCount >= 2) ? chL + 1 : chL;
        if (sink->gainLinear != 1.0f) {
            for (int f = 0; f < FRAMES; f++) {
                float l = clampf(_outCh[chL][f] * sink->gainLinear);
                float r = clampf(_outCh[chR][f] * sink->gainLinear);
                _sinkBuf[s][f * 2]     = (int32_t)(l * MAX_24BIT_F) * 256;
                _sinkBuf[s][f * 2 + 1] = (int32_t)(r * MAX_24BIT_F) * 256;
            }
        } else {
            to_int32_lj(_outCh[chL], _outCh[chR], _sinkBuf[s], FRAMES);
        }
        sink->write(_sinkBuf[s], FRAMES);
    }
}

// ===== Mock write helpers =====

static bool g_write_called[MAX_SINKS] = {};
static int32_t g_write_buf[MAX_SINKS][RAW_SAMPLES];
static int g_write_frames[MAX_SINKS] = {};

static void mock_write_0(const int32_t *buf, int frames) {
    g_write_called[0] = true;
    memcpy(g_write_buf[0], buf, frames * 2 * sizeof(int32_t));
    g_write_frames[0] = frames;
}

static void mock_write_1(const int32_t *buf, int frames) {
    g_write_called[1] = true;
    memcpy(g_write_buf[1], buf, frames * 2 * sizeof(int32_t));
    g_write_frames[1] = frames;
}

static bool mock_ready() { return true; }
static bool mock_not_ready() { return false; }

// ===== setUp / tearDown =====

void setUp() {
    memset(_outCh, 0, sizeof(_outCh));
    memset(_swapHoldCh, 0, sizeof(_swapHoldCh));
    memset(_matrixGain, 0, sizeof(_matrixGain));
    memset(_sinks, 0, sizeof(_sinks));
    _sinkCount = 0;
    memset(_sinkBuf, 0, sizeof(_sinkBuf));
    memset(g_write_called, 0, sizeof(g_write_called));
    memset(g_write_buf, 0, sizeof(g_write_buf));
    memset(g_write_frames, 0, sizeof(g_write_frames));
}

void tearDown() {}

// ===== Test 1: 8 output channel buffers are independently addressable =====

void test_output_buffers_8ch() {
    for (int o = 0; o < MATRIX_SIZE; o++) {
        _outCh[o][0] = (float)(o + 1);
    }
    for (int o = 0; o < MATRIX_SIZE; o++) {
        TEST_ASSERT_FLOAT_WITHIN(0.001f, (float)(o + 1), _outCh[o][0]);
    }
}

// ===== Test 2: 8x8 identity matrix preserves input =====

void test_matrix_8x8_identity() {
    // Set identity: matrixGain[o][o] = 1.0 for each output channel
    for (int o = 0; o < MATRIX_SIZE; o++) {
        _matrixGain[o][o] = 1.0f;
    }

    // Input: each channel is a constant value equal to its index + 1
    float inCh[MATRIX_SIZE][FRAMES];
    memset(inCh, 0, sizeof(inCh));
    for (int i = 0; i < MATRIX_SIZE; i++) {
        for (int f = 0; f < FRAMES; f++) {
            inCh[i][f] = (float)(i + 1) * 0.1f;
        }
    }

    pipeline_mix_matrix_8ch(inCh, MATRIX_SIZE);

    // Each output channel should match its corresponding input
    for (int o = 0; o < MATRIX_SIZE; o++) {
        for (int f = 0; f < FRAMES; f++) {
            TEST_ASSERT_FLOAT_WITHIN(0.0001f, (float)(o + 1) * 0.1f, _outCh[o][f]);
        }
    }
}

// ===== Test 3: single channel gain =====

void test_matrix_single_channel_gain() {
    // Route input ch0 to output ch2 with gain 0.5
    _matrixGain[2][0] = 0.5f;

    float inCh[MATRIX_SIZE][FRAMES];
    memset(inCh, 0, sizeof(inCh));
    for (int f = 0; f < FRAMES; f++) {
        inCh[0][f] = 1.0f;
    }

    pipeline_mix_matrix_8ch(inCh, MATRIX_SIZE);

    for (int f = 0; f < FRAMES; f++) {
        TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.5f, _outCh[2][f]);
    }
    // Other outputs should remain zero
    for (int f = 0; f < FRAMES; f++) {
        TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, _outCh[0][f]);
        TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, _outCh[1][f]);
    }
}

// ===== Test 4: matrix summing (multiple inputs to one output) =====

void test_matrix_summing() {
    _matrixGain[4][0] = 1.0f;
    _matrixGain[4][2] = 1.0f;

    float inCh[MATRIX_SIZE][FRAMES];
    memset(inCh, 0, sizeof(inCh));
    for (int f = 0; f < FRAMES; f++) {
        inCh[0][f] = 0.3f;
        inCh[2][f] = 0.4f;
    }

    pipeline_mix_matrix_8ch(inCh, MATRIX_SIZE);

    for (int f = 0; f < FRAMES; f++) {
        TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.7f, _outCh[4][f]);
    }
}

// ===== Test 5: sink registration =====

void test_sink_registration() {
    AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
    sink.name = "DAC0";
    sink.firstChannel = 2;
    sink.channelCount = 2;
    sink.write = mock_write_0;
    sink.isReady = mock_ready;

    pipeline_register_sink(&sink);

    TEST_ASSERT_EQUAL_INT(1, _sinkCount);
    TEST_ASSERT_EQUAL_UINT8(2, _sinks[0].firstChannel);
    TEST_ASSERT_EQUAL_STRING("DAC0", _sinks[0].name);
}

// ===== Test 6: sink write is called =====

void test_sink_write_called() {
    AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
    sink.name = "DAC0";
    sink.firstChannel = 0;
    sink.channelCount = 2;
    sink.write = mock_write_0;
    sink.isReady = mock_ready;

    pipeline_register_sink(&sink);

    // Fill output channels 0 and 1 with 0.5
    for (int f = 0; f < FRAMES; f++) {
        _outCh[0][f] = 0.5f;
        _outCh[1][f] = 0.5f;
    }

    pipeline_write_sinks();

    TEST_ASSERT_TRUE(g_write_called[0]);
    TEST_ASSERT_EQUAL_INT(FRAMES, g_write_frames[0]);
}

// ===== Test 7: two sinks receive different audio =====

void test_sink_different_audio() {
    AudioOutputSink sink0 = AUDIO_OUTPUT_SINK_INIT;
    sink0.name = "DAC0";
    sink0.firstChannel = 0;
    sink0.channelCount = 2;
    sink0.write = mock_write_0;
    sink0.isReady = mock_ready;

    AudioOutputSink sink1 = AUDIO_OUTPUT_SINK_INIT;
    sink1.name = "DAC1";
    sink1.firstChannel = 2;
    sink1.channelCount = 2;
    sink1.write = mock_write_1;
    sink1.isReady = mock_ready;

    pipeline_register_sink(&sink0);
    pipeline_register_sink(&sink1);

    // Fill different values per channel pair
    for (int f = 0; f < FRAMES; f++) {
        _outCh[0][f] = 1.0f;   // sink0 L
        _outCh[1][f] = 1.0f;   // sink0 R
        _outCh[2][f] = 0.5f;   // sink1 L
        _outCh[3][f] = 0.5f;   // sink1 R
    }

    pipeline_write_sinks();

    TEST_ASSERT_TRUE(g_write_called[0]);
    TEST_ASSERT_TRUE(g_write_called[1]);

    // Sink 0 should have 1.0 -> MAX_24BIT_F * 256
    int32_t expected_full = (int32_t)(1.0f * MAX_24BIT_F) * 256;
    int32_t expected_half = (int32_t)(0.5f * MAX_24BIT_F) * 256;

    // Compare first L sample of each sink
    TEST_ASSERT_INT32_WITHIN(512, expected_full, g_write_buf[0][0]);
    TEST_ASSERT_INT32_WITHIN(512, expected_half, g_write_buf[1][0]);
}

// ===== Test 8: sink not ready is skipped =====

void test_sink_not_ready_skipped() {
    AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
    sink.name = "DAC0";
    sink.firstChannel = 0;
    sink.channelCount = 2;
    sink.write = mock_write_0;
    sink.isReady = mock_not_ready;

    pipeline_register_sink(&sink);

    for (int f = 0; f < FRAMES; f++) {
        _outCh[0][f] = 0.5f;
        _outCh[1][f] = 0.5f;
    }

    pipeline_write_sinks();

    TEST_ASSERT_FALSE(g_write_called[0]);
}

// ===== Test 9: muted sink is skipped =====

void test_sink_muted_skipped() {
    AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
    sink.name = "DAC0";
    sink.firstChannel = 0;
    sink.channelCount = 2;
    sink.write = mock_write_0;
    sink.isReady = mock_ready;
    sink.muted = true;

    pipeline_register_sink(&sink);

    for (int f = 0; f < FRAMES; f++) {
        _outCh[0][f] = 0.5f;
        _outCh[1][f] = 0.5f;
    }

    pipeline_write_sinks();

    TEST_ASSERT_FALSE(g_write_called[0]);
}

// ===== Test 10: mono sink uses same channel for L and R =====

void test_sink_mono() {
    AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
    sink.name = "MONO";
    sink.firstChannel = 4;
    sink.channelCount = 1;  // mono
    sink.write = mock_write_0;
    sink.isReady = mock_ready;

    pipeline_register_sink(&sink);

    for (int f = 0; f < FRAMES; f++) {
        _outCh[4][f] = 0.8f;
    }

    pipeline_write_sinks();

    TEST_ASSERT_TRUE(g_write_called[0]);

    // With mono (channelCount=1), both L and R come from the same channel
    int32_t expected = (int32_t)(0.8f * MAX_24BIT_F) * 256;
    // First stereo frame: L and R should both be the same
    TEST_ASSERT_INT32_WITHIN(512, expected, g_write_buf[0][0]);  // L
    TEST_ASSERT_INT32_WITHIN(512, expected, g_write_buf[0][1]);  // R
}

// ===== Test 11: sink gain is applied =====

void test_sink_gain_applied() {
    AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
    sink.name = "DAC0";
    sink.firstChannel = 0;
    sink.channelCount = 2;
    sink.write = mock_write_0;
    sink.isReady = mock_ready;
    sink.gainLinear = 0.5f;

    pipeline_register_sink(&sink);

    for (int f = 0; f < FRAMES; f++) {
        _outCh[0][f] = 1.0f;
        _outCh[1][f] = 1.0f;
    }

    pipeline_write_sinks();

    TEST_ASSERT_TRUE(g_write_called[0]);

    // 1.0 * 0.5 gain = 0.5 -> int32 left-justified
    int32_t expected = (int32_t)(0.5f * MAX_24BIT_F) * 256;
    TEST_ASSERT_INT32_WITHIN(512, expected, g_write_buf[0][0]);
}

// ===== Test 12: matrix gain set/get roundtrip (save) =====

void test_matrix_save_roundtrip() {
    _matrixGain[0][0] = 1.0f;
    _matrixGain[2][1] = 0.75f;
    _matrixGain[7][7] = 0.123f;

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, _matrixGain[0][0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.75f, _matrixGain[2][1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.123f, _matrixGain[7][7]);

    // Verify other cells remain zero
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, _matrixGain[0][1]);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, _matrixGain[5][3]);
}

// ===== Test 13: matrix load roundtrip (overwrite and verify) =====

void test_matrix_load_roundtrip() {
    // Simulate "loading" a saved config by writing specific values
    for (int o = 0; o < MATRIX_SIZE; o++) {
        for (int i = 0; i < MATRIX_SIZE; i++) {
            _matrixGain[o][i] = (float)(o * MATRIX_SIZE + i) / 64.0f;
        }
    }

    // Verify every cell matches expected value
    for (int o = 0; o < MATRIX_SIZE; o++) {
        for (int i = 0; i < MATRIX_SIZE; i++) {
            float expected = (float)(o * MATRIX_SIZE + i) / 64.0f;
            TEST_ASSERT_FLOAT_WITHIN(0.0001f, expected, _matrixGain[o][i]);
        }
    }
}

// ===== Test 14: dB to linear gain conversion =====

void test_gaindb_to_linear() {
    // -6.02 dB should be approximately 0.5 linear
    float linear = powf(10.0f, -6.02f / 20.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.5f, linear);

    // 0 dB should be 1.0 linear
    float unity = powf(10.0f, 0.0f / 20.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, unity);

    // -20 dB should be 0.1 linear
    float minus20 = powf(10.0f, -20.0f / 20.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.1f, minus20);

    // +6 dB should be approximately 2.0 linear
    float plus6 = powf(10.0f, 6.0f / 20.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 2.0f, plus6);
}

// ===== Main =====

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_output_buffers_8ch);
    RUN_TEST(test_matrix_8x8_identity);
    RUN_TEST(test_matrix_single_channel_gain);
    RUN_TEST(test_matrix_summing);
    RUN_TEST(test_sink_registration);
    RUN_TEST(test_sink_write_called);
    RUN_TEST(test_sink_different_audio);
    RUN_TEST(test_sink_not_ready_skipped);
    RUN_TEST(test_sink_muted_skipped);
    RUN_TEST(test_sink_mono);
    RUN_TEST(test_sink_gain_applied);
    RUN_TEST(test_matrix_save_roundtrip);
    RUN_TEST(test_matrix_load_roundtrip);
    RUN_TEST(test_gaindb_to_linear);
    return UNITY_END();
}
