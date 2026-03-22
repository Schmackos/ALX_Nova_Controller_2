// test_hal_tdm_interleaver.cpp
// Unit tests for HalTdmInterleaver — 8-slot TDM interleaver for 8-channel DAC expansion.
//
// Tests verify:
//   1.  init() allocates buffers and returns true
//   2.  init() is idempotent (second call is a no-op)
//   3.  isReady() returns false before init, true after init
//   4.  deinit() clears ready state and releases buffers
//   5.  buildSinks() populates all four AudioOutputSink structs with non-null callbacks
//   6.  buildSinks() assigns correct names to all four sinks
//   7.  buildSinks() sets firstChannel correctly (0, 2, 4, 6)
//   8.  buildSinks() sets channelCount = 2 on all sinks
//   9.  buildSinks() propagates halSlot to all four sinks
//  10.  isReady callback returns true after init
//  11.  Pairs 0-2 buffer but do NOT flush (no TDM write until pair 3)
//  12.  Pair 3 write triggers TDM assembly + flush
//  13.  TDM frame layout: pair 0 → slots 0-1, pair 1 → slots 2-3, pair 2 → slots 4-5, pair 3 → slots 6-7
//  14.  Frame count is respected — only 'n' TDM frames are written
//  15.  Interleave preserves L/R order within each pair
//  16.  Full 8-channel ordering across all frames
//  17.  Ping-pong: after flush, _writeIdx toggles; next tick writes to the other side
//  18.  Two ticks in sequence produce correct TDM output on each tick
//  19.  Multiple init/deinit cycles succeed
//  20.  First instance gets slot 0 — rebuilding after deinit reuses the slot
//  21.  Two concurrent instances get different write function pointers
//  22.  buildSinks() with all slots full leaves callbacks null (graceful failure)
//  23.  _writePair with null buf is a no-op (does not crash)
//  24.  _writePair with frames=0 does not flush
//  25.  Large frame count is capped to TDM_INTERLEAVER_FRAMES
//  26.  buildSinks() before init: isReady callback returns false
//  27.  gainLinear and volumeGain are initialised to 1.0 on all sinks
//  28.  muted is initialised to false on all sinks
//  29.  vuL / vuR are initialised to -90.0 on all sinks
//  30.  halSlot 0xFF propagates correctly (unbound sentinel)
//
// The TDM TX write is intercepted via TDM_INTERLEAVER_TEST_PROVIDES_STUBS so
// tests can verify what data was passed to the I2S DMA path without real hardware.

#include <unity.h>
#include <cstring>
#include <cstdlib>
#include <cstdint>

// NATIVE_TEST and DAC_ENABLED are already defined by the native build flags in
// platformio.ini; redefining here only produces harmless warnings, but suppress
// them by guarding:
#ifndef NATIVE_TEST
#define NATIVE_TEST
#endif
#ifndef DAC_ENABLED
#define DAC_ENABLED
#endif

// Include the header first so TDM_INTERLEAVER_FRAMES is available for the
// capture buffer size declaration below.
#include "../../src/hal/hal_tdm_interleaver.h"

// Signal to hal_tdm_interleaver.cpp that this translation unit provides its own
// definition of i2s_port_write so the default no-op stub is skipped and we can
// capture what was written.
#define TDM_INTERLEAVER_TEST_PROVIDES_STUBS

// ---------------------------------------------------------------------------
// Capture buffer for TDM TX output
// The interleaver calls i2s_port_write(_i2sPort, ...) after each flush.
// We capture the bytes written here for assertion.
// ---------------------------------------------------------------------------
static int32_t  g_txCaptureBuf[TDM_INTERLEAVER_FRAMES * 8] = {};
static size_t   g_txCaptureBytes = 0;
static uint32_t g_txCallCount    = 0;

// Our controlled TX write — captures data + counts calls
// Matches the port-generic signature: port, src, size, bw, timeout
inline void i2s_port_write(uint8_t /*port*/, const void* src, size_t size,
                            size_t* bw, uint32_t /*timeout*/) {
    g_txCaptureBytes = size;
    if (size > 0 && src) {
        memcpy(g_txCaptureBuf, src, size < sizeof(g_txCaptureBuf) ? size : sizeof(g_txCaptureBuf));
    }
    if (bw) *bw = size;
    g_txCallCount++;
}

// psram_alloc + heap_budget (required since interleaver uses psram_alloc)
#include "../../src/heap_budget.cpp"
#include "../../src/psram_alloc.cpp"

// Bring in the interleaver implementation directly (native test pattern)
#include "../../src/hal/hal_tdm_interleaver.cpp"

// ---------------------------------------------------------------------------
// Per-test setup/teardown
// ---------------------------------------------------------------------------

static HalTdmInterleaver* g_il = nullptr;

// Stereo pair input buffers reused across tests (each pair is 2 int32_t per frame)
static int32_t pairIn[4][TDM_INTERLEAVER_FRAMES * 2] = {};

void setUp(void) {
    // Reset TX capture state
    memset(g_txCaptureBuf, 0, sizeof(g_txCaptureBuf));
    g_txCaptureBytes = 0;
    g_txCallCount    = 0;
    // Reset pair input buffers
    memset(pairIn, 0, sizeof(pairIn));
    g_il = new HalTdmInterleaver();
}

void tearDown(void) {
    delete g_il;
    g_il = nullptr;
}

// ---------------------------------------------------------------------------
// Helper: fill a pair input buffer with a recognisable per-pair/per-channel
// pattern so channel-routing errors are easy to spot.
//
// Encoding: sample = (pairBase | frameIndex)
//   pair 0: L = 0x01000000|f, R = 0x02000000|f
//   pair 1: L = 0x03000000|f, R = 0x04000000|f
//   pair 2: L = 0x05000000|f, R = 0x06000000|f
//   pair 3: L = 0x07000000|f, R = 0x08000000|f
// ---------------------------------------------------------------------------
static void fillPair(int pairIdx, int numFrames) {
    uint32_t baseL = (uint32_t)((pairIdx * 2 + 1)) << 24;
    uint32_t baseR = (uint32_t)((pairIdx * 2 + 2)) << 24;
    for (int f = 0; f < numFrames && f < TDM_INTERLEAVER_FRAMES; f++) {
        pairIn[pairIdx][f * 2 + 0] = (int32_t)(baseL | (uint32_t)f);
        pairIn[pairIdx][f * 2 + 1] = (int32_t)(baseR | (uint32_t)f);
    }
}

// Write all four pairs via their AudioOutputSink write callbacks.
// Returns the number of times g_txCallCount was incremented (should be 1).
static uint32_t writeAllPairs(AudioOutputSink* sinks[4], int frames) {
    uint32_t before = g_txCallCount;
    for (int p = 0; p < 4; p++) {
        fillPair(p, frames);
        sinks[p]->write(pairIn[p], frames);
    }
    return g_txCallCount - before;
}

// ---------------------------------------------------------------------------
// Test 1: init() succeeds and returns true
// ---------------------------------------------------------------------------
void test_init_succeeds(void) {
    bool ok = g_il->init(0);
    TEST_ASSERT_TRUE(ok);
}

// ---------------------------------------------------------------------------
// Test 2: init() is idempotent (second call returns true without re-allocating)
// ---------------------------------------------------------------------------
void test_init_idempotent(void) {
    TEST_ASSERT_TRUE(g_il->init(0));
    TEST_ASSERT_TRUE(g_il->init(0));
}

// ---------------------------------------------------------------------------
// Test 3: isReady() returns false before init, true after
// ---------------------------------------------------------------------------
void test_is_ready_before_and_after_init(void) {
    TEST_ASSERT_FALSE(g_il->isReady());
    g_il->init(0);
    TEST_ASSERT_TRUE(g_il->isReady());
}

// ---------------------------------------------------------------------------
// Test 4: deinit() clears ready state
// ---------------------------------------------------------------------------
void test_deinit_clears_ready(void) {
    g_il->init(0);
    TEST_ASSERT_TRUE(g_il->isReady());
    g_il->deinit();
    TEST_ASSERT_FALSE(g_il->isReady());
}

// ---------------------------------------------------------------------------
// Test 5: buildSinks() populates all four AudioOutputSink write + isReady
// ---------------------------------------------------------------------------
void test_build_sinks_populates_callbacks(void) {
    g_il->init(0);
    AudioOutputSink sA = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sB = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sD = AUDIO_OUTPUT_SINK_INIT;
    g_il->buildSinks("A", "B", "C", "D", &sA, &sB, &sC, &sD, 0);

    TEST_ASSERT_NOT_NULL(sA.write);
    TEST_ASSERT_NOT_NULL(sA.isReady);
    TEST_ASSERT_NOT_NULL(sB.write);
    TEST_ASSERT_NOT_NULL(sB.isReady);
    TEST_ASSERT_NOT_NULL(sC.write);
    TEST_ASSERT_NOT_NULL(sC.isReady);
    TEST_ASSERT_NOT_NULL(sD.write);
    TEST_ASSERT_NOT_NULL(sD.isReady);
}

// ---------------------------------------------------------------------------
// Test 6: buildSinks() assigns correct names
// ---------------------------------------------------------------------------
void test_build_sinks_names(void) {
    g_il->init(0);
    AudioOutputSink sA = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sB = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sD = AUDIO_OUTPUT_SINK_INIT;
    g_il->buildSinks("ES9038PRO CH1/2", "ES9038PRO CH3/4",
                     "ES9038PRO CH5/6", "ES9038PRO CH7/8",
                     &sA, &sB, &sC, &sD, 0);

    TEST_ASSERT_EQUAL_STRING("ES9038PRO CH1/2", sA.name);
    TEST_ASSERT_EQUAL_STRING("ES9038PRO CH3/4", sB.name);
    TEST_ASSERT_EQUAL_STRING("ES9038PRO CH5/6", sC.name);
    TEST_ASSERT_EQUAL_STRING("ES9038PRO CH7/8", sD.name);
}

// ---------------------------------------------------------------------------
// Test 7: buildSinks() sets firstChannel to 0, 2, 4, 6
// ---------------------------------------------------------------------------
void test_build_sinks_first_channel(void) {
    g_il->init(0);
    AudioOutputSink sA = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sB = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sD = AUDIO_OUTPUT_SINK_INIT;
    g_il->buildSinks("A", "B", "C", "D", &sA, &sB, &sC, &sD, 0);

    TEST_ASSERT_EQUAL_UINT8(0, sA.firstChannel);
    TEST_ASSERT_EQUAL_UINT8(2, sB.firstChannel);
    TEST_ASSERT_EQUAL_UINT8(4, sC.firstChannel);
    TEST_ASSERT_EQUAL_UINT8(6, sD.firstChannel);
}

// ---------------------------------------------------------------------------
// Test 8: buildSinks() sets channelCount = 2 on all sinks
// ---------------------------------------------------------------------------
void test_build_sinks_channel_count(void) {
    g_il->init(0);
    AudioOutputSink sA = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sB = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sD = AUDIO_OUTPUT_SINK_INIT;
    g_il->buildSinks("A", "B", "C", "D", &sA, &sB, &sC, &sD, 0);

    TEST_ASSERT_EQUAL_UINT8(2, sA.channelCount);
    TEST_ASSERT_EQUAL_UINT8(2, sB.channelCount);
    TEST_ASSERT_EQUAL_UINT8(2, sC.channelCount);
    TEST_ASSERT_EQUAL_UINT8(2, sD.channelCount);
}

// ---------------------------------------------------------------------------
// Test 9: buildSinks() propagates halSlot to all four sinks
// ---------------------------------------------------------------------------
void test_build_sinks_hal_slot_propagated(void) {
    g_il->init(0);
    AudioOutputSink sA = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sB = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sD = AUDIO_OUTPUT_SINK_INIT;
    g_il->buildSinks("A", "B", "C", "D", &sA, &sB, &sC, &sD, 7);

    TEST_ASSERT_EQUAL_UINT8(7, sA.halSlot);
    TEST_ASSERT_EQUAL_UINT8(7, sB.halSlot);
    TEST_ASSERT_EQUAL_UINT8(7, sC.halSlot);
    TEST_ASSERT_EQUAL_UINT8(7, sD.halSlot);
}

// ---------------------------------------------------------------------------
// Test 10: isReady callback returns true after init
// ---------------------------------------------------------------------------
void test_is_ready_callback_returns_true_after_init(void) {
    g_il->init(0);
    AudioOutputSink sA = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sB = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sD = AUDIO_OUTPUT_SINK_INIT;
    g_il->buildSinks("A", "B", "C", "D", &sA, &sB, &sC, &sD, 0);

    TEST_ASSERT_TRUE(sA.isReady());
    TEST_ASSERT_TRUE(sD.isReady());
}

// ---------------------------------------------------------------------------
// Test 11: Pairs 0-2 buffer but do NOT trigger a TDM TX write
// ---------------------------------------------------------------------------
void test_pairs_0_2_do_not_flush(void) {
    g_il->init(0);
    AudioOutputSink sA = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sB = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sD = AUDIO_OUTPUT_SINK_INIT;
    g_il->buildSinks("A", "B", "C", "D", &sA, &sB, &sC, &sD, 0);

    fillPair(0, 4); sA.write(pairIn[0], 4);
    TEST_ASSERT_EQUAL_UINT32(0, g_txCallCount);

    fillPair(1, 4); sB.write(pairIn[1], 4);
    TEST_ASSERT_EQUAL_UINT32(0, g_txCallCount);

    fillPair(2, 4); sC.write(pairIn[2], 4);
    TEST_ASSERT_EQUAL_UINT32(0, g_txCallCount);
}

// ---------------------------------------------------------------------------
// Test 12: Pair 3 write triggers exactly one TDM TX call
// ---------------------------------------------------------------------------
void test_pair_3_triggers_flush(void) {
    g_il->init(0);
    AudioOutputSink sA = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sB = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sD = AUDIO_OUTPUT_SINK_INIT;
    g_il->buildSinks("A", "B", "C", "D", &sA, &sB, &sC, &sD, 0);

    fillPair(0, 4); sA.write(pairIn[0], 4);
    fillPair(1, 4); sB.write(pairIn[1], 4);
    fillPair(2, 4); sC.write(pairIn[2], 4);
    TEST_ASSERT_EQUAL_UINT32(0, g_txCallCount);

    fillPair(3, 4); sD.write(pairIn[3], 4);
    TEST_ASSERT_EQUAL_UINT32(1, g_txCallCount);
}

// ---------------------------------------------------------------------------
// Test 13: TDM frame layout — pair 0 → slots 0-1, pair 1 → slots 2-3, etc.
// (spot check on frame 0 only)
// ---------------------------------------------------------------------------
void test_tdm_frame_layout_slot_assignment(void) {
    g_il->init(0);
    AudioOutputSink sA = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sB = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sD = AUDIO_OUTPUT_SINK_INIT;
    g_il->buildSinks("A", "B", "C", "D", &sA, &sB, &sC, &sD, 0);

    for (int p = 0; p < 4; p++) fillPair(p, 1);
    sA.write(pairIn[0], 1);
    sB.write(pairIn[1], 1);
    sC.write(pairIn[2], 1);
    sD.write(pairIn[3], 1);

    // Frame 0, slot 0 = pair0 L = 0x01000000 | 0
    TEST_ASSERT_EQUAL_INT32((int32_t)(0x01000000), g_txCaptureBuf[0]);
    // Frame 0, slot 1 = pair0 R = 0x02000000 | 0
    TEST_ASSERT_EQUAL_INT32((int32_t)(0x02000000), g_txCaptureBuf[1]);
    // Frame 0, slot 2 = pair1 L = 0x03000000 | 0
    TEST_ASSERT_EQUAL_INT32((int32_t)(0x03000000), g_txCaptureBuf[2]);
    // Frame 0, slot 3 = pair1 R = 0x04000000 | 0
    TEST_ASSERT_EQUAL_INT32((int32_t)(0x04000000), g_txCaptureBuf[3]);
    // Frame 0, slot 4 = pair2 L = 0x05000000 | 0
    TEST_ASSERT_EQUAL_INT32((int32_t)(0x05000000), g_txCaptureBuf[4]);
    // Frame 0, slot 5 = pair2 R = 0x06000000 | 0
    TEST_ASSERT_EQUAL_INT32((int32_t)(0x06000000), g_txCaptureBuf[5]);
    // Frame 0, slot 6 = pair3 L = 0x07000000 | 0
    TEST_ASSERT_EQUAL_INT32((int32_t)(0x07000000), g_txCaptureBuf[6]);
    // Frame 0, slot 7 = pair3 R = 0x08000000 | 0
    TEST_ASSERT_EQUAL_INT32((int32_t)(0x08000000), g_txCaptureBuf[7]);
}

// ---------------------------------------------------------------------------
// Test 14: TDM TX byte count matches frames × 8 slots × sizeof(int32_t)
// ---------------------------------------------------------------------------
void test_flush_writes_correct_byte_count(void) {
    g_il->init(0);
    AudioOutputSink sA = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sB = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sD = AUDIO_OUTPUT_SINK_INIT;
    g_il->buildSinks("A", "B", "C", "D", &sA, &sB, &sC, &sD, 0);

    const int testFrames = 16;
    for (int p = 0; p < 4; p++) fillPair(p, testFrames);
    sA.write(pairIn[0], testFrames);
    sB.write(pairIn[1], testFrames);
    sC.write(pairIn[2], testFrames);
    sD.write(pairIn[3], testFrames);

    size_t expectedBytes = (size_t)testFrames * 8 * sizeof(int32_t);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)expectedBytes, (uint32_t)g_txCaptureBytes);
}

// ---------------------------------------------------------------------------
// Test 15: Interleave preserves L/R order within each pair (multi-frame)
// ---------------------------------------------------------------------------
void test_interleave_preserves_lr_order(void) {
    g_il->init(0);
    AudioOutputSink sA = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sB = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sD = AUDIO_OUTPUT_SINK_INIT;
    g_il->buildSinks("A", "B", "C", "D", &sA, &sB, &sC, &sD, 0);

    const int testFrames = 4;
    for (int p = 0; p < 4; p++) fillPair(p, testFrames);
    sA.write(pairIn[0], testFrames);
    sB.write(pairIn[1], testFrames);
    sC.write(pairIn[2], testFrames);
    sD.write(pairIn[3], testFrames);

    // Verify pair 0 L/R across all 4 frames
    for (int f = 0; f < testFrames; f++) {
        const int32_t* slot = g_txCaptureBuf + f * 8;
        TEST_ASSERT_EQUAL_INT32((int32_t)(0x01000000 | f), slot[0]);  // pair0 L
        TEST_ASSERT_EQUAL_INT32((int32_t)(0x02000000 | f), slot[1]);  // pair0 R
    }
}

// ---------------------------------------------------------------------------
// Test 16: Full 8-channel ordering across multiple frames
// ---------------------------------------------------------------------------
void test_full_8ch_ordering_all_frames(void) {
    g_il->init(0);
    AudioOutputSink sA = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sB = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sD = AUDIO_OUTPUT_SINK_INIT;
    g_il->buildSinks("A", "B", "C", "D", &sA, &sB, &sC, &sD, 0);

    const int testFrames = 8;
    for (int p = 0; p < 4; p++) fillPair(p, testFrames);
    sA.write(pairIn[0], testFrames);
    sB.write(pairIn[1], testFrames);
    sC.write(pairIn[2], testFrames);
    sD.write(pairIn[3], testFrames);

    for (int f = 0; f < testFrames; f++) {
        const int32_t* slot = g_txCaptureBuf + f * 8;
        TEST_ASSERT_EQUAL_INT32((int32_t)(0x01000000 | f), slot[0]);  // pair0 L (CH1)
        TEST_ASSERT_EQUAL_INT32((int32_t)(0x02000000 | f), slot[1]);  // pair0 R (CH2)
        TEST_ASSERT_EQUAL_INT32((int32_t)(0x03000000 | f), slot[2]);  // pair1 L (CH3)
        TEST_ASSERT_EQUAL_INT32((int32_t)(0x04000000 | f), slot[3]);  // pair1 R (CH4)
        TEST_ASSERT_EQUAL_INT32((int32_t)(0x05000000 | f), slot[4]);  // pair2 L (CH5)
        TEST_ASSERT_EQUAL_INT32((int32_t)(0x06000000 | f), slot[5]);  // pair2 R (CH6)
        TEST_ASSERT_EQUAL_INT32((int32_t)(0x07000000 | f), slot[6]);  // pair3 L (CH7)
        TEST_ASSERT_EQUAL_INT32((int32_t)(0x08000000 | f), slot[7]);  // pair3 R (CH8)
    }
}

// ---------------------------------------------------------------------------
// Test 17: Ping-pong — _writeIdx toggles after flush
// Two ticks: second tick data must overwrite first tick in the other ping-pong side.
// ---------------------------------------------------------------------------
void test_pingpong_toggles_after_flush(void) {
    g_il->init(0);
    AudioOutputSink sA = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sB = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sD = AUDIO_OUTPUT_SINK_INIT;
    g_il->buildSinks("A", "B", "C", "D", &sA, &sB, &sC, &sD, 0);

    // First tick: fill with signature 0x0N000000
    for (int p = 0; p < 4; p++) fillPair(p, 1);
    sA.write(pairIn[0], 1);
    sB.write(pairIn[1], 1);
    sC.write(pairIn[2], 1);
    sD.write(pairIn[3], 1);
    TEST_ASSERT_EQUAL_UINT32(1, g_txCallCount);

    // Second tick: fill pair 0 with a different signature to verify buffer swap
    // Use manually crafted distinct values
    pairIn[0][0] = (int32_t)0xAA000000;  // L distinct
    pairIn[0][1] = (int32_t)0xBB000000;  // R distinct
    for (int p = 1; p < 4; p++) fillPair(p, 1);

    sA.write(pairIn[0], 1);
    sB.write(pairIn[1], 1);
    sC.write(pairIn[2], 1);
    sD.write(pairIn[3], 1);
    TEST_ASSERT_EQUAL_UINT32(2, g_txCallCount);

    // TDM slot 0 of the second flush must be the second tick's pair0 L value
    TEST_ASSERT_EQUAL_INT32((int32_t)0xAA000000, g_txCaptureBuf[0]);
    TEST_ASSERT_EQUAL_INT32((int32_t)0xBB000000, g_txCaptureBuf[1]);
}

// ---------------------------------------------------------------------------
// Test 18: Two complete ticks each produce a TDM flush (txCallCount increments)
// ---------------------------------------------------------------------------
void test_two_ticks_produce_two_flushes(void) {
    g_il->init(0);
    AudioOutputSink sA = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sB = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sD = AUDIO_OUTPUT_SINK_INIT;
    g_il->buildSinks("A", "B", "C", "D", &sA, &sB, &sC, &sD, 0);

    AudioOutputSink* sinks[4] = { &sA, &sB, &sC, &sD };
    writeAllPairs(sinks, 4);
    writeAllPairs(sinks, 4);
    TEST_ASSERT_EQUAL_UINT32(2, g_txCallCount);
}

// ---------------------------------------------------------------------------
// Test 19: Multiple init/deinit cycles succeed
// ---------------------------------------------------------------------------
void test_multiple_init_deinit_cycles(void) {
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_TRUE(g_il->init(0));
        TEST_ASSERT_TRUE(g_il->isReady());
        g_il->deinit();
        TEST_ASSERT_FALSE(g_il->isReady());
    }
    // Final init after loop — stays ready
    TEST_ASSERT_TRUE(g_il->init(0));
    TEST_ASSERT_TRUE(g_il->isReady());
}

// ---------------------------------------------------------------------------
// Test 20: Rebuilding after deinit reacquires slot 0 successfully
// ---------------------------------------------------------------------------
void test_rebuild_after_deinit_reacquires_slot(void) {
    g_il->init(0);
    AudioOutputSink sA = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sB = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sD = AUDIO_OUTPUT_SINK_INIT;
    g_il->buildSinks("A", "B", "C", "D", &sA, &sB, &sC, &sD, 0);
    TEST_ASSERT_NOT_NULL(sA.write);

    g_il->deinit();
    g_il->init(0);

    AudioOutputSink sA2 = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sB2 = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC2 = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sD2 = AUDIO_OUTPUT_SINK_INIT;
    g_il->buildSinks("A2", "B2", "C2", "D2", &sA2, &sB2, &sC2, &sD2, 0);
    TEST_ASSERT_NOT_NULL(sA2.write);
    TEST_ASSERT_NOT_NULL(sD2.write);
}

// ---------------------------------------------------------------------------
// Test 21: Two concurrent instances get different write function pointers
// ---------------------------------------------------------------------------
void test_two_instances_get_different_thunks(void) {
    HalTdmInterleaver il2;

    AudioOutputSink sA0 = AUDIO_OUTPUT_SINK_INIT, sB0 = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC0 = AUDIO_OUTPUT_SINK_INIT, sD0 = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sA1 = AUDIO_OUTPUT_SINK_INIT, sB1 = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC1 = AUDIO_OUTPUT_SINK_INIT, sD1 = AUDIO_OUTPUT_SINK_INIT;

    g_il->init(0);
    g_il->buildSinks("A0", "B0", "C0", "D0", &sA0, &sB0, &sC0, &sD0, 0);

    il2.init(0);
    il2.buildSinks("A1", "B1", "C1", "D1", &sA1, &sB1, &sC1, &sD1, 1);

    TEST_ASSERT_NOT_NULL(sA0.write);
    TEST_ASSERT_NOT_NULL(sA1.write);
    // Slot 0 and slot 1 thunks must be different function pointers
    TEST_ASSERT_NOT_EQUAL((void*)sA0.write, (void*)sA1.write);
    TEST_ASSERT_NOT_EQUAL((void*)sD0.write, (void*)sD1.write);
    TEST_ASSERT_NOT_EQUAL((void*)sA0.isReady, (void*)sA1.isReady);

    il2.deinit();
}

// ---------------------------------------------------------------------------
// Test 22: buildSinks() with all slots full leaves callbacks null (graceful failure)
// ---------------------------------------------------------------------------
void test_build_sinks_slot_full_leaves_callbacks_null(void) {
    HalTdmInterleaver il2, il3;

    AudioOutputSink sA0 = AUDIO_OUTPUT_SINK_INIT, sB0 = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC0 = AUDIO_OUTPUT_SINK_INIT, sD0 = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sA1 = AUDIO_OUTPUT_SINK_INIT, sB1 = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC1 = AUDIO_OUTPUT_SINK_INIT, sD1 = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sA2 = AUDIO_OUTPUT_SINK_INIT, sB2 = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC2 = AUDIO_OUTPUT_SINK_INIT, sD2 = AUDIO_OUTPUT_SINK_INIT;

    // Fill both slots
    g_il->init(0);
    g_il->buildSinks("A0", "B0", "C0", "D0", &sA0, &sB0, &sC0, &sD0, 0);
    il2.init(0);
    il2.buildSinks("A1", "B1", "C1", "D1", &sA1, &sB1, &sC1, &sD1, 1);

    // Third instance: no slot available — callbacks must remain null
    il3.init(0);
    il3.buildSinks("A2", "B2", "C2", "D2", &sA2, &sB2, &sC2, &sD2, 2);
    TEST_ASSERT_NULL(sA2.write);
    TEST_ASSERT_NULL(sD2.write);

    il2.deinit();
    // il3 and g_il cleaned up by tearDown / destructors
}

// ---------------------------------------------------------------------------
// Test 23: _writePair via sink callback with null buf is a no-op
// ---------------------------------------------------------------------------
void test_write_null_buf_no_crash(void) {
    g_il->init(0);
    AudioOutputSink sA = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sB = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sD = AUDIO_OUTPUT_SINK_INIT;
    g_il->buildSinks("A", "B", "C", "D", &sA, &sB, &sC, &sD, 0);

    // Passing null should not crash; no TX flush should occur
    sA.write(nullptr, 4);
    sB.write(nullptr, 4);
    sC.write(nullptr, 4);
    sD.write(nullptr, 4);
    TEST_ASSERT_EQUAL_UINT32(0, g_txCallCount);
}

// ---------------------------------------------------------------------------
// Test 24: _writePair with frames=0 does not trigger a flush
// ---------------------------------------------------------------------------
void test_write_zero_frames_no_flush(void) {
    g_il->init(0);
    AudioOutputSink sA = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sB = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sD = AUDIO_OUTPUT_SINK_INIT;
    g_il->buildSinks("A", "B", "C", "D", &sA, &sB, &sC, &sD, 0);

    for (int p = 0; p < 4; p++) fillPair(p, 0);
    sA.write(pairIn[0], 0);
    sB.write(pairIn[1], 0);
    sC.write(pairIn[2], 0);
    sD.write(pairIn[3], 0);
    TEST_ASSERT_EQUAL_UINT32(0, g_txCallCount);
}

// ---------------------------------------------------------------------------
// Test 25: Frame count larger than TDM_INTERLEAVER_FRAMES is capped
// ---------------------------------------------------------------------------
void test_frame_count_capped_at_max(void) {
    g_il->init(0);
    AudioOutputSink sA = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sB = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sD = AUDIO_OUTPUT_SINK_INIT;
    g_il->buildSinks("A", "B", "C", "D", &sA, &sB, &sC, &sD, 0);

    // Request more frames than the buffer holds — should not overflow
    const int overRequest = TDM_INTERLEAVER_FRAMES + 64;
    for (int p = 0; p < 4; p++) fillPair(p, TDM_INTERLEAVER_FRAMES);
    sA.write(pairIn[0], overRequest);
    sB.write(pairIn[1], overRequest);
    sC.write(pairIn[2], overRequest);
    sD.write(pairIn[3], overRequest);

    // Should have flushed exactly one TDM TX call with capped byte count
    TEST_ASSERT_EQUAL_UINT32(1, g_txCallCount);
    size_t expectedBytes = (size_t)TDM_INTERLEAVER_FRAMES * 8 * sizeof(int32_t);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)expectedBytes, (uint32_t)g_txCaptureBytes);
}

// ---------------------------------------------------------------------------
// Test 26: buildSinks() before init() — isReady callback returns false
// ---------------------------------------------------------------------------
void test_is_ready_callback_false_before_init(void) {
    // Do NOT call init() — build sinks on an uninitialised instance
    AudioOutputSink sA = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sB = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sD = AUDIO_OUTPUT_SINK_INIT;
    g_il->buildSinks("A", "B", "C", "D", &sA, &sB, &sC, &sD, 0);

    if (sA.isReady != nullptr) {
        TEST_ASSERT_FALSE(sA.isReady());
    }
    // If callbacks were not registered (no init), they remain null — acceptable
}

// ---------------------------------------------------------------------------
// Test 27: gainLinear and volumeGain are 1.0 after buildSinks
// ---------------------------------------------------------------------------
void test_build_sinks_gain_defaults(void) {
    g_il->init(0);
    AudioOutputSink sA = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sB = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sD = AUDIO_OUTPUT_SINK_INIT;
    g_il->buildSinks("A", "B", "C", "D", &sA, &sB, &sC, &sD, 0);

    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, sA.gainLinear);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, sA.volumeGain);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, sD.gainLinear);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, sD.volumeGain);
}

// ---------------------------------------------------------------------------
// Test 28: muted is false after buildSinks
// ---------------------------------------------------------------------------
void test_build_sinks_not_muted(void) {
    g_il->init(0);
    AudioOutputSink sA = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sB = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sD = AUDIO_OUTPUT_SINK_INIT;
    g_il->buildSinks("A", "B", "C", "D", &sA, &sB, &sC, &sD, 0);

    TEST_ASSERT_FALSE(sA.muted);
    TEST_ASSERT_FALSE(sB.muted);
    TEST_ASSERT_FALSE(sC.muted);
    TEST_ASSERT_FALSE(sD.muted);
}

// ---------------------------------------------------------------------------
// Test 29: vuL and vuR are -90.0 after buildSinks
// ---------------------------------------------------------------------------
void test_build_sinks_vu_defaults(void) {
    g_il->init(0);
    AudioOutputSink sA = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sB = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sD = AUDIO_OUTPUT_SINK_INIT;
    g_il->buildSinks("A", "B", "C", "D", &sA, &sB, &sC, &sD, 0);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, -90.0f, sA.vuL);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -90.0f, sA.vuR);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -90.0f, sD.vuL);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -90.0f, sD.vuR);
}

// ---------------------------------------------------------------------------
// Test 30: halSlot 0xFF (unbound sentinel) propagates correctly
// ---------------------------------------------------------------------------
void test_build_sinks_hal_slot_0xff(void) {
    g_il->init(0);
    AudioOutputSink sA = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sB = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sC = AUDIO_OUTPUT_SINK_INIT;
    AudioOutputSink sD = AUDIO_OUTPUT_SINK_INIT;
    g_il->buildSinks("A", "B", "C", "D", &sA, &sB, &sC, &sD, 0xFF);

    TEST_ASSERT_EQUAL_UINT8(0xFF, sA.halSlot);
    TEST_ASSERT_EQUAL_UINT8(0xFF, sB.halSlot);
    TEST_ASSERT_EQUAL_UINT8(0xFF, sC.halSlot);
    TEST_ASSERT_EQUAL_UINT8(0xFF, sD.halSlot);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_init_succeeds);
    RUN_TEST(test_init_idempotent);
    RUN_TEST(test_is_ready_before_and_after_init);
    RUN_TEST(test_deinit_clears_ready);
    RUN_TEST(test_build_sinks_populates_callbacks);
    RUN_TEST(test_build_sinks_names);
    RUN_TEST(test_build_sinks_first_channel);
    RUN_TEST(test_build_sinks_channel_count);
    RUN_TEST(test_build_sinks_hal_slot_propagated);
    RUN_TEST(test_is_ready_callback_returns_true_after_init);
    RUN_TEST(test_pairs_0_2_do_not_flush);
    RUN_TEST(test_pair_3_triggers_flush);
    RUN_TEST(test_tdm_frame_layout_slot_assignment);
    RUN_TEST(test_flush_writes_correct_byte_count);
    RUN_TEST(test_interleave_preserves_lr_order);
    RUN_TEST(test_full_8ch_ordering_all_frames);
    RUN_TEST(test_pingpong_toggles_after_flush);
    RUN_TEST(test_two_ticks_produce_two_flushes);
    RUN_TEST(test_multiple_init_deinit_cycles);
    RUN_TEST(test_rebuild_after_deinit_reacquires_slot);
    RUN_TEST(test_two_instances_get_different_thunks);
    RUN_TEST(test_build_sinks_slot_full_leaves_callbacks_null);
    RUN_TEST(test_write_null_buf_no_crash);
    RUN_TEST(test_write_zero_frames_no_flush);
    RUN_TEST(test_frame_count_capped_at_max);
    RUN_TEST(test_is_ready_callback_false_before_init);
    RUN_TEST(test_build_sinks_gain_defaults);
    RUN_TEST(test_build_sinks_not_muted);
    RUN_TEST(test_build_sinks_vu_defaults);
    RUN_TEST(test_build_sinks_hal_slot_0xff);

    return UNITY_END();
}
