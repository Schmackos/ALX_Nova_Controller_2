/**
 * test_sink_slot_api.cpp
 *
 * Unit tests for the slot-indexed sink API: audio_pipeline_set_sink(),
 * audio_pipeline_remove_sink(), get_sink_count(), get_sink(), and the
 * dispatch loop that iterates populated slots.
 *
 * Tests run on the native platform (test_build_src = no).
 * The sink storage and slot API are reimplemented inline from
 * audio_pipeline.cpp (stripped of vTaskSuspendAll/xTaskResumeAll and
 * AppState::markChannelMapDirty which are #ifndef NATIVE_TEST guarded).
 *
 * Build flags required (already in platformio.ini [env:native]):
 *   -D UNIT_TEST -D NATIVE_TEST -D DSP_ENABLED
 */

#include <unity.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stddef.h>

// ===== Constants =====
#define FRAMES           256
#define RAW_SAMPLES      512
#define MAX_SINKS        4     // matches AUDIO_OUT_MAX_SINKS
#define MATRIX_SIZE      8
#define MAX_24BIT_F      8388607.0f

// ===== Inline AudioOutputSink definition (from audio_output_sink.h) =====
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
    uint8_t halSlot;
} AudioOutputSink;

#define AUDIO_OUTPUT_SINK_INIT { \
    NULL,   /* name */           \
    0,      /* firstChannel */   \
    2,      /* channelCount */   \
    NULL,   /* write */          \
    NULL,   /* isReady */        \
    1.0f,   /* gainLinear */     \
    false,  /* muted */          \
    -90.0f, /* vuL */            \
    -90.0f, /* vuR */            \
    0.0f,   /* _vuSmoothedL */   \
    0.0f,   /* _vuSmoothedR */   \
    0xFF    /* halSlot */        \
}

// ===== Sink storage (mirrors production statics) =====
static AudioOutputSink _sinks[MAX_SINKS];
static volatile int _sinkCount = 0;
static int32_t _sinkBuf[MAX_SINKS][RAW_SAMPLES];
static float _outCh[MATRIX_SIZE][FRAMES];

// ===== Dirty flag tracking (replaces AppState::markChannelMapDirty) =====
static bool _channelMapDirty = false;

static void markChannelMapDirty() {
    _channelMapDirty = true;
}

// ===== Inline slot API (reimplemented from audio_pipeline.cpp) =====

static void audio_pipeline_set_sink(int slot, const AudioOutputSink *sink) {
    if (slot < 0 || slot >= MAX_SINKS || !sink) return;
    _sinks[slot] = *sink;
    // Update _sinkCount to reflect highest occupied slot + 1
    _sinkCount = 0;
    for (int i = 0; i < MAX_SINKS; i++) {
        if (_sinks[i].write) _sinkCount = i + 1;
    }
    markChannelMapDirty();
}

static void audio_pipeline_remove_sink(int slot) {
    if (slot < 0 || slot >= MAX_SINKS) return;
    if (!_sinks[slot].write) return;  // Already empty
    memset(&_sinks[slot], 0, sizeof(AudioOutputSink));
    _sinks[slot].gainLinear = 1.0f;
    _sinks[slot].vuL = -90.0f;
    _sinks[slot].vuR = -90.0f;
    _sinks[slot].halSlot = 0xFF;
    // Update _sinkCount
    _sinkCount = 0;
    for (int i = 0; i < MAX_SINKS; i++) {
        if (_sinks[i].write) _sinkCount = i + 1;
    }
    markChannelMapDirty();
}

static void audio_pipeline_set_sink_muted(uint8_t slot, bool muted) {
    if (slot >= MAX_SINKS) return;
    _sinks[slot].muted = muted;
}

static bool audio_pipeline_is_sink_muted(uint8_t slot) {
    if (slot >= MAX_SINKS) return false;
    return _sinks[slot].muted;
}

static int audio_pipeline_get_sink_count() {
    return _sinkCount;
}

static const AudioOutputSink* audio_pipeline_get_sink(int idx) {
    if (idx < 0 || idx >= _sinkCount) return NULL;
    return &_sinks[idx];
}

// ===== Inline helpers (from audio_pipeline.cpp) =====

static float clampf(float v) {
    return v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v);
}

static void to_int32_lj(const float *L, const float *R, int32_t *raw, int frames) {
    for (int f = 0; f < frames; f++) {
        raw[f * 2]     = (int32_t)(clampf(L[f]) * MAX_24BIT_F) * 256;
        raw[f * 2 + 1] = (int32_t)(clampf(R[f]) * MAX_24BIT_F) * 256;
    }
}

// ===== Minimal dispatch loop (mirrors pipeline_write_output sink path) =====
// Iterates all slots (not just 0.._sinkCount-1) to handle gaps between slots.

static void pipeline_dispatch_sinks() {
    for (int s = 0; s < MAX_SINKS; s++) {
        AudioOutputSink *sink = &_sinks[s];
        if (!sink->write || !sink->isReady || !sink->isReady()) continue;
        if (sink->muted) continue;

        int chL = sink->firstChannel;
        int chR = (sink->channelCount >= 2) ? chL + 1 : chL;
        if (chL >= MATRIX_SIZE) continue;
        if (chR >= MATRIX_SIZE) chR = chL;

        if (sink->gainLinear != 1.0f) {
            float g = sink->gainLinear;
            for (int f = 0; f < FRAMES; f++) {
                float l = clampf(_outCh[chL][f] * g);
                float r = clampf(_outCh[chR][f] * g);
                _sinkBuf[s][f * 2]     = (int32_t)(l * MAX_24BIT_F) * 256;
                _sinkBuf[s][f * 2 + 1] = (int32_t)(r * MAX_24BIT_F) * 256;
            }
        } else {
            to_int32_lj(_outCh[chL], _outCh[chR], _sinkBuf[s], FRAMES);
        }
        sink->write(_sinkBuf[s], FRAMES);
    }
}

// ===== Mock write functions (per-slot tracking) =====

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

static void mock_write_2(const int32_t *buf, int frames) {
    g_write_called[2] = true;
    memcpy(g_write_buf[2], buf, frames * 2 * sizeof(int32_t));
    g_write_frames[2] = frames;
}

static void mock_write_3(const int32_t *buf, int frames) {
    g_write_called[3] = true;
    memcpy(g_write_buf[3], buf, frames * 2 * sizeof(int32_t));
    g_write_frames[3] = frames;
}

typedef void (*mock_write_fn)(const int32_t *, int);
static const mock_write_fn MOCK_WRITES[MAX_SINKS] = {
    mock_write_0, mock_write_1, mock_write_2, mock_write_3
};

static bool mock_ready()     { return true;  }
static bool mock_not_ready() { return false; }

// ===== Helper: build a standard test sink =====

static AudioOutputSink make_sink(const char *name, int slot_idx, uint8_t firstCh,
                                 uint8_t chCount) {
    AudioOutputSink s = AUDIO_OUTPUT_SINK_INIT;
    s.name = name;
    s.firstChannel = firstCh;
    s.channelCount = chCount;
    s.write = MOCK_WRITES[slot_idx < MAX_SINKS ? slot_idx : 0];
    s.isReady = mock_ready;
    return s;
}

// ===== setUp / tearDown =====

void setUp() {
    // Reset sink array to default-initialised state
    for (int i = 0; i < MAX_SINKS; i++) {
        AudioOutputSink init = AUDIO_OUTPUT_SINK_INIT;
        _sinks[i] = init;
    }
    _sinkCount = 0;
    _channelMapDirty = false;
    memset(_sinkBuf, 0, sizeof(_sinkBuf));
    memset(_outCh, 0, sizeof(_outCh));
    memset(g_write_called, 0, sizeof(g_write_called));
    memset(g_write_buf, 0, sizeof(g_write_buf));
    memset(g_write_frames, 0, sizeof(g_write_frames));
}

void tearDown() {}

// ==========================================================================
// Group 1: Basic set/remove (8 tests)
// ==========================================================================

// 1. Set a sink at slot 0, verify it is stored
void test_set_sink_at_slot_0() {
    AudioOutputSink s = make_sink("DAC0", 0, 0, 2);
    audio_pipeline_set_sink(0, &s);

    TEST_ASSERT_EQUAL_INT(1, audio_pipeline_get_sink_count());
    TEST_ASSERT_EQUAL_STRING("DAC0", _sinks[0].name);
    TEST_ASSERT_NOT_NULL(_sinks[0].write);
}

// 2. Set a sink at slot 1, verify count reflects highest occupied + 1
void test_set_sink_at_slot_1() {
    AudioOutputSink s = make_sink("ES8311", 1, 2, 2);
    audio_pipeline_set_sink(1, &s);

    // Slot 0 is empty, slot 1 has a sink -> _sinkCount = 2
    TEST_ASSERT_EQUAL_INT(2, audio_pipeline_get_sink_count());
    TEST_ASSERT_EQUAL_STRING("ES8311", _sinks[1].name);
    TEST_ASSERT_NULL(_sinks[0].write);
}

// 3. Remove a sink that was set
void test_remove_existing_sink() {
    AudioOutputSink s = make_sink("DAC0", 0, 0, 2);
    audio_pipeline_set_sink(0, &s);
    TEST_ASSERT_EQUAL_INT(1, audio_pipeline_get_sink_count());

    audio_pipeline_remove_sink(0);
    TEST_ASSERT_EQUAL_INT(0, audio_pipeline_get_sink_count());
    TEST_ASSERT_NULL(_sinks[0].write);
}

// 4. Remove from an empty slot is a no-op (no crash, count unchanged)
void test_remove_nonexistent_sink() {
    // Slot 0 has never been set
    audio_pipeline_remove_sink(0);
    TEST_ASSERT_EQUAL_INT(0, audio_pipeline_get_sink_count());
}

// 5. Null sink pointer is rejected
void test_set_null_sink_rejected() {
    audio_pipeline_set_sink(0, NULL);
    TEST_ASSERT_EQUAL_INT(0, audio_pipeline_get_sink_count());
    TEST_ASSERT_NULL(_sinks[0].write);
}

// 6. Negative slot index is rejected
void test_set_negative_slot_rejected() {
    AudioOutputSink s = make_sink("DAC0", 0, 0, 2);
    audio_pipeline_set_sink(-1, &s);
    TEST_ASSERT_EQUAL_INT(0, audio_pipeline_get_sink_count());
}

// 7. Out-of-bounds slot index is rejected (set)
void test_set_oob_slot_rejected() {
    AudioOutputSink s = make_sink("DAC0", 0, 0, 2);
    audio_pipeline_set_sink(MAX_SINKS, &s);
    TEST_ASSERT_EQUAL_INT(0, audio_pipeline_get_sink_count());
}

// 8. Out-of-bounds slot index is rejected (remove)
void test_remove_oob_slot_rejected() {
    // Should not crash or modify state
    audio_pipeline_remove_sink(MAX_SINKS);
    audio_pipeline_remove_sink(-1);
    TEST_ASSERT_EQUAL_INT(0, audio_pipeline_get_sink_count());
}

// ==========================================================================
// Group 2: Overwrite and idempotency (5 tests)
// ==========================================================================

// 9. Overwrite a slot with a different sink
void test_overwrite_same_slot() {
    AudioOutputSink s1 = make_sink("DAC0", 0, 0, 2);
    audio_pipeline_set_sink(0, &s1);
    TEST_ASSERT_EQUAL_STRING("DAC0", _sinks[0].name);

    AudioOutputSink s2 = make_sink("DAC1", 0, 4, 2);
    audio_pipeline_set_sink(0, &s2);
    TEST_ASSERT_EQUAL_STRING("DAC1", _sinks[0].name);
    TEST_ASSERT_EQUAL_UINT8(4, _sinks[0].firstChannel);
    TEST_ASSERT_EQUAL_INT(1, audio_pipeline_get_sink_count());
}

// 10. Double remove is a no-op (second remove sees empty slot)
void test_double_remove() {
    AudioOutputSink s = make_sink("DAC0", 0, 0, 2);
    audio_pipeline_set_sink(0, &s);

    audio_pipeline_remove_sink(0);
    TEST_ASSERT_EQUAL_INT(0, audio_pipeline_get_sink_count());

    // Second remove should be a safe no-op
    _channelMapDirty = false;
    audio_pipeline_remove_sink(0);
    TEST_ASSERT_EQUAL_INT(0, audio_pipeline_get_sink_count());
    // Dirty flag should NOT be set on empty-slot remove
    TEST_ASSERT_FALSE(_channelMapDirty);
}

// 11. Reuse a slot after removal
void test_reuse_after_remove() {
    AudioOutputSink s1 = make_sink("DAC0", 0, 0, 2);
    audio_pipeline_set_sink(0, &s1);
    audio_pipeline_remove_sink(0);

    AudioOutputSink s2 = make_sink("DAC_NEW", 0, 6, 2);
    audio_pipeline_set_sink(0, &s2);

    TEST_ASSERT_EQUAL_INT(1, audio_pipeline_get_sink_count());
    TEST_ASSERT_EQUAL_STRING("DAC_NEW", _sinks[0].name);
    TEST_ASSERT_EQUAL_UINT8(6, _sinks[0].firstChannel);
}

// 12. Fill all slots
void test_all_slots_full() {
    for (int i = 0; i < MAX_SINKS; i++) {
        AudioOutputSink s = make_sink("SINK", i, (uint8_t)(i * 2), 2);
        audio_pipeline_set_sink(i, &s);
    }
    TEST_ASSERT_EQUAL_INT(MAX_SINKS, audio_pipeline_get_sink_count());
    for (int i = 0; i < MAX_SINKS; i++) {
        TEST_ASSERT_NOT_NULL(_sinks[i].write);
    }
}

// 13. Remove middle slot, count reflects highest occupied
void test_remove_middle_updates_count() {
    AudioOutputSink s0 = make_sink("S0", 0, 0, 2);
    AudioOutputSink s1 = make_sink("S1", 1, 2, 2);
    AudioOutputSink s2 = make_sink("S2", 2, 4, 2);
    audio_pipeline_set_sink(0, &s0);
    audio_pipeline_set_sink(1, &s1);
    audio_pipeline_set_sink(2, &s2);
    TEST_ASSERT_EQUAL_INT(3, audio_pipeline_get_sink_count());

    // Remove the middle sink (slot 1)
    audio_pipeline_remove_sink(1);
    // Slot 2 is still occupied, so _sinkCount = 3
    TEST_ASSERT_EQUAL_INT(3, audio_pipeline_get_sink_count());
    TEST_ASSERT_NULL(_sinks[1].write);
    TEST_ASSERT_NOT_NULL(_sinks[2].write);
}

// ==========================================================================
// Group 3: halSlot field (4 tests)
// ==========================================================================

// 14. Default halSlot is 0xFF
void test_halSlot_default_0xFF() {
    AudioOutputSink s = AUDIO_OUTPUT_SINK_INIT;
    TEST_ASSERT_EQUAL_UINT8(0xFF, s.halSlot);
}

// 15. halSlot can be set and read back
void test_halSlot_set_and_read() {
    AudioOutputSink s = make_sink("DAC0", 0, 0, 2);
    s.halSlot = 3;
    audio_pipeline_set_sink(0, &s);
    TEST_ASSERT_EQUAL_UINT8(3, _sinks[0].halSlot);
}

// 16. halSlot survives overwrite (new value replaces old)
void test_halSlot_survives_overwrite() {
    AudioOutputSink s1 = make_sink("DAC0", 0, 0, 2);
    s1.halSlot = 5;
    audio_pipeline_set_sink(0, &s1);

    AudioOutputSink s2 = make_sink("DAC1", 0, 0, 2);
    s2.halSlot = 7;
    audio_pipeline_set_sink(0, &s2);

    TEST_ASSERT_EQUAL_UINT8(7, _sinks[0].halSlot);
}

// 17. halSlot is reset to 0xFF on remove
void test_halSlot_cleared_on_remove() {
    AudioOutputSink s = make_sink("DAC0", 0, 0, 2);
    s.halSlot = 2;
    audio_pipeline_set_sink(0, &s);
    TEST_ASSERT_EQUAL_UINT8(2, _sinks[0].halSlot);

    audio_pipeline_remove_sink(0);
    TEST_ASSERT_EQUAL_UINT8(0xFF, _sinks[0].halSlot);
}

// ==========================================================================
// Group 4: Slot-based dispatch (8 tests)
// ==========================================================================

// 18. Dispatch calls only populated slots
void test_dispatch_populated_slots_only() {
    AudioOutputSink s = make_sink("DAC0", 0, 0, 2);
    audio_pipeline_set_sink(0, &s);
    for (int f = 0; f < FRAMES; f++) {
        _outCh[0][f] = 0.5f;
        _outCh[1][f] = 0.5f;
    }

    pipeline_dispatch_sinks();

    TEST_ASSERT_TRUE(g_write_called[0]);
    TEST_ASSERT_FALSE(g_write_called[1]);
    TEST_ASSERT_FALSE(g_write_called[2]);
    TEST_ASSERT_FALSE(g_write_called[3]);
}

// 19. Dispatch skips sink with null write
void test_dispatch_skips_null_write() {
    AudioOutputSink s = make_sink("DAC0", 0, 0, 2);
    s.write = NULL;
    audio_pipeline_set_sink(0, &s);

    pipeline_dispatch_sinks();
    TEST_ASSERT_FALSE(g_write_called[0]);
}

// 20. Dispatch skips sink that is not ready
void test_dispatch_skips_not_ready() {
    AudioOutputSink s = make_sink("DAC0", 0, 0, 2);
    s.isReady = mock_not_ready;
    audio_pipeline_set_sink(0, &s);
    for (int f = 0; f < FRAMES; f++) {
        _outCh[0][f] = 0.5f;
        _outCh[1][f] = 0.5f;
    }

    pipeline_dispatch_sinks();
    TEST_ASSERT_FALSE(g_write_called[0]);
}

// 21. Dispatch skips muted sink
void test_dispatch_skips_muted() {
    AudioOutputSink s = make_sink("DAC0", 0, 0, 2);
    s.muted = true;
    audio_pipeline_set_sink(0, &s);
    for (int f = 0; f < FRAMES; f++) {
        _outCh[0][f] = 0.5f;
        _outCh[1][f] = 0.5f;
    }

    pipeline_dispatch_sinks();
    TEST_ASSERT_FALSE(g_write_called[0]);
}

// 22. Dispatch applies per-sink gain
void test_dispatch_gain_applied() {
    AudioOutputSink s = make_sink("DAC0", 0, 0, 2);
    s.gainLinear = 0.5f;
    audio_pipeline_set_sink(0, &s);

    for (int f = 0; f < FRAMES; f++) {
        _outCh[0][f] = 1.0f;
        _outCh[1][f] = 1.0f;
    }

    pipeline_dispatch_sinks();
    TEST_ASSERT_TRUE(g_write_called[0]);

    // 1.0 * 0.5 gain = 0.5 -> int32 left-justified
    int32_t expected = (int32_t)(0.5f * MAX_24BIT_F) * 256;
    TEST_ASSERT_INT32_WITHIN(512, expected, g_write_buf[0][0]);
}

// 23. Dispatch uses correct channel mapping per sink
void test_dispatch_channel_mapping() {
    AudioOutputSink s0 = make_sink("DAC0", 0, 0, 2);
    AudioOutputSink s1 = make_sink("DAC1", 1, 4, 2);
    audio_pipeline_set_sink(0, &s0);
    audio_pipeline_set_sink(1, &s1);

    for (int f = 0; f < FRAMES; f++) {
        _outCh[0][f] = 0.25f;   // s0 L
        _outCh[1][f] = 0.25f;   // s0 R
        _outCh[4][f] = 0.75f;   // s1 L
        _outCh[5][f] = 0.75f;   // s1 R
    }

    pipeline_dispatch_sinks();

    TEST_ASSERT_TRUE(g_write_called[0]);
    TEST_ASSERT_TRUE(g_write_called[1]);

    int32_t exp_0 = (int32_t)(0.25f * MAX_24BIT_F) * 256;
    int32_t exp_1 = (int32_t)(0.75f * MAX_24BIT_F) * 256;
    TEST_ASSERT_INT32_WITHIN(512, exp_0, g_write_buf[0][0]);
    TEST_ASSERT_INT32_WITHIN(512, exp_1, g_write_buf[1][0]);
}

// 24. Dispatch after remove — removed sink no longer called
void test_dispatch_after_remove() {
    AudioOutputSink s0 = make_sink("DAC0", 0, 0, 2);
    AudioOutputSink s1 = make_sink("DAC1", 1, 2, 2);
    audio_pipeline_set_sink(0, &s0);
    audio_pipeline_set_sink(1, &s1);

    audio_pipeline_remove_sink(0);

    for (int f = 0; f < FRAMES; f++) {
        _outCh[2][f] = 0.6f;
        _outCh[3][f] = 0.6f;
    }

    pipeline_dispatch_sinks();

    TEST_ASSERT_FALSE(g_write_called[0]);
    TEST_ASSERT_TRUE(g_write_called[1]);
}

// 25. Mono sink uses same channel for L and R
void test_dispatch_mono_sink() {
    AudioOutputSink s = make_sink("MONO", 0, 4, 1);
    audio_pipeline_set_sink(0, &s);

    for (int f = 0; f < FRAMES; f++) {
        _outCh[4][f] = 0.8f;
    }

    pipeline_dispatch_sinks();
    TEST_ASSERT_TRUE(g_write_called[0]);

    int32_t expected = (int32_t)(0.8f * MAX_24BIT_F) * 256;
    // Mono: both L and R of interleaved output come from the same channel
    TEST_ASSERT_INT32_WITHIN(512, expected, g_write_buf[0][0]);  // L
    TEST_ASSERT_INT32_WITHIN(512, expected, g_write_buf[0][1]);  // R
}

// ==========================================================================
// Group 5: get_sink_count / get_sink (4 tests)
// ==========================================================================

// 26. get_sink_count returns 0 when empty
void test_get_sink_count_empty() {
    TEST_ASSERT_EQUAL_INT(0, audio_pipeline_get_sink_count());
}

// 27. get_sink_count with gaps reflects highest occupied slot + 1
void test_get_sink_count_with_gaps() {
    AudioOutputSink s = make_sink("DAC3", 3, 6, 2);
    audio_pipeline_set_sink(3, &s);

    // Only slot 3 occupied -> _sinkCount = 4
    TEST_ASSERT_EQUAL_INT(4, audio_pipeline_get_sink_count());
}

// 28. get_sink returns the correct sink by index
void test_get_sink_by_index() {
    AudioOutputSink s0 = make_sink("DAC0", 0, 0, 2);
    AudioOutputSink s1 = make_sink("ES8311", 1, 2, 2);
    audio_pipeline_set_sink(0, &s0);
    audio_pipeline_set_sink(1, &s1);

    const AudioOutputSink *p0 = audio_pipeline_get_sink(0);
    const AudioOutputSink *p1 = audio_pipeline_get_sink(1);

    TEST_ASSERT_NOT_NULL(p0);
    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_EQUAL_STRING("DAC0", p0->name);
    TEST_ASSERT_EQUAL_STRING("ES8311", p1->name);
}

// 29. get_sink returns NULL for out-of-bounds index
void test_get_sink_oob_returns_null() {
    AudioOutputSink s = make_sink("DAC0", 0, 0, 2);
    audio_pipeline_set_sink(0, &s);

    TEST_ASSERT_NULL(audio_pipeline_get_sink(-1));
    TEST_ASSERT_NULL(audio_pipeline_get_sink(MAX_SINKS));
    // Index beyond _sinkCount but within array should also return NULL
    TEST_ASSERT_NULL(audio_pipeline_get_sink(1));
}

// ==========================================================================
// Group 6: markChannelMapDirty (3 tests)
// ==========================================================================

// 30. set_sink marks channel map dirty
void test_dirty_on_set() {
    _channelMapDirty = false;
    AudioOutputSink s = make_sink("DAC0", 0, 0, 2);
    audio_pipeline_set_sink(0, &s);
    TEST_ASSERT_TRUE(_channelMapDirty);
}

// 31. remove_sink marks channel map dirty (when slot was occupied)
void test_dirty_on_remove() {
    AudioOutputSink s = make_sink("DAC0", 0, 0, 2);
    audio_pipeline_set_sink(0, &s);

    _channelMapDirty = false;
    audio_pipeline_remove_sink(0);
    TEST_ASSERT_TRUE(_channelMapDirty);
}

// 32. remove_sink does NOT mark dirty when slot is already empty
void test_not_dirty_on_empty_slot_remove() {
    _channelMapDirty = false;
    audio_pipeline_remove_sink(0);
    TEST_ASSERT_FALSE(_channelMapDirty);
}

// ==========================================================================
// Group 7: Sink mute API (3 tests)
// ==========================================================================

// 33. set_sink_muted updates the sink struct, is_sink_muted reflects it
void test_set_sink_muted_updates_sink_struct() {
    AudioOutputSink s = make_sink("DAC0", 0, 0, 2);
    audio_pipeline_set_sink(0, &s);

    // Mute
    audio_pipeline_set_sink_muted(0, true);
    TEST_ASSERT_TRUE(audio_pipeline_is_sink_muted(0));
    TEST_ASSERT_TRUE(_sinks[0].muted);

    // Unmute
    audio_pipeline_set_sink_muted(0, false);
    TEST_ASSERT_FALSE(audio_pipeline_is_sink_muted(0));
    TEST_ASSERT_FALSE(_sinks[0].muted);
}

// 34. set_sink_muted ignores invalid slot indices without crashing
void test_set_sink_muted_ignores_invalid_slot() {
    // Slot >= MAX_SINKS
    audio_pipeline_set_sink_muted(MAX_SINKS, true);
    audio_pipeline_set_sink_muted(255, true);
    // is_sink_muted returns false for invalid slots
    TEST_ASSERT_FALSE(audio_pipeline_is_sink_muted(MAX_SINKS));
    TEST_ASSERT_FALSE(audio_pipeline_is_sink_muted(255));
}

// 35. Newly registered sink is unmuted by default
void test_unmuted_by_default() {
    AudioOutputSink s = make_sink("DAC0", 0, 0, 2);
    audio_pipeline_set_sink(0, &s);
    TEST_ASSERT_FALSE(audio_pipeline_is_sink_muted(0));
}

// ===== Main =====

int main() {
    UNITY_BEGIN();

    // Group 1: Basic set/remove
    RUN_TEST(test_set_sink_at_slot_0);
    RUN_TEST(test_set_sink_at_slot_1);
    RUN_TEST(test_remove_existing_sink);
    RUN_TEST(test_remove_nonexistent_sink);
    RUN_TEST(test_set_null_sink_rejected);
    RUN_TEST(test_set_negative_slot_rejected);
    RUN_TEST(test_set_oob_slot_rejected);
    RUN_TEST(test_remove_oob_slot_rejected);

    // Group 2: Overwrite and idempotency
    RUN_TEST(test_overwrite_same_slot);
    RUN_TEST(test_double_remove);
    RUN_TEST(test_reuse_after_remove);
    RUN_TEST(test_all_slots_full);
    RUN_TEST(test_remove_middle_updates_count);

    // Group 3: halSlot field
    RUN_TEST(test_halSlot_default_0xFF);
    RUN_TEST(test_halSlot_set_and_read);
    RUN_TEST(test_halSlot_survives_overwrite);
    RUN_TEST(test_halSlot_cleared_on_remove);

    // Group 4: Slot-based dispatch
    RUN_TEST(test_dispatch_populated_slots_only);
    RUN_TEST(test_dispatch_skips_null_write);
    RUN_TEST(test_dispatch_skips_not_ready);
    RUN_TEST(test_dispatch_skips_muted);
    RUN_TEST(test_dispatch_gain_applied);
    RUN_TEST(test_dispatch_channel_mapping);
    RUN_TEST(test_dispatch_after_remove);
    RUN_TEST(test_dispatch_mono_sink);

    // Group 5: get_sink_count / get_sink
    RUN_TEST(test_get_sink_count_empty);
    RUN_TEST(test_get_sink_count_with_gaps);
    RUN_TEST(test_get_sink_by_index);
    RUN_TEST(test_get_sink_oob_returns_null);

    // Group 6: markChannelMapDirty
    RUN_TEST(test_dirty_on_set);
    RUN_TEST(test_dirty_on_remove);
    RUN_TEST(test_not_dirty_on_empty_slot_remove);

    // Group 7: Sink mute API
    RUN_TEST(test_set_sink_muted_updates_sink_struct);
    RUN_TEST(test_set_sink_muted_ignores_invalid_slot);
    RUN_TEST(test_unmuted_by_default);

    return UNITY_END();
}
