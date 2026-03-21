// test_hal_tdm_deinterleaver.cpp
// Unit tests for HalTdmDeinterleaver — ES9843PRO 4-slot TDM deinterleaver.
//
// Tests verify:
//   1. init() allocates buffers and returns true
//   2. init() is idempotent (second call is a no-op)
//   3. deinit() releases buffers, isReady() returns false afterwards
//   4. buildSources() populates both AudioInputSource structs with non-null callbacks
//   5. Pair A read fills CH1/CH2 correctly from a synthetic TDM buffer
//   6. Pair B read returns CH3/CH4 from the buffer pair A just deinterleaved
//   7. Pair A produces the correct frame count for varying input sizes
//   8. Pair B returns 0 before pair A has produced any data
//   9. Pair B is limited to the last frame count produced by pair A
//  10. Deinterleave preserves L/R ordering (CH1→L, CH2→R, CH3→L, CH4→R)
//  11. Ping-pong: calling pair A twice and pair B once returns the SECOND fill
//  12. buildSources() names are assigned correctly
//  13. isActive() callbacks reflect _initialized state in native tests
//  14. getSampleRate() returns 48000 in native test context
//  15. Source halSlot and lane are initialised to defaults by buildSources()
//
// Thread-safety note: tests are single-threaded; the ping-pong ordering is
// exercised by calling pair A then pair B within the same synthetic "tick".

#include <unity.h>
#include <cstring>
#include <cstdlib>
#include <cstdint>

// Build under native test — no hardware access
#define NATIVE_TEST
#define DAC_ENABLED

// Signal to hal_tdm_deinterleaver.cpp that this translation unit provides its
// own definitions of the i2s_audio port2 TDM functions so the .cpp stubs are
// skipped (avoids duplicate symbol errors).
#define TDM_TEST_PROVIDES_STUBS

// ---------------------------------------------------------------------------
// Synthetic TDM DMA feed — controlled by loadTdmFrames() / clearTdmFeed()
// ---------------------------------------------------------------------------
static int32_t g_tdmFeedBuf[128 * 4] = {};   // Up to 128 TDM frames × 4 slots
static uint32_t g_tdmFeedFrames = 0;          // Frames of data loaded into buf

// These are called by the _pairARead / _pairAActive static thunks inside the
// deinterleaver.  We define them before including the .cpp so they satisfy the
// forward declarations inside the NATIVE_TEST block.
inline uint32_t i2s_audio_port2_tdm_read(int32_t* dst, uint32_t frames) {
    uint32_t avail = (g_tdmFeedFrames < frames) ? g_tdmFeedFrames : frames;
    if (avail > 0) {
        memcpy(dst, g_tdmFeedBuf, avail * 4 * sizeof(int32_t));
    }
    return avail;
}
inline bool     i2s_audio_port2_tdm_active(void) { return g_tdmFeedFrames > 0; }
inline uint32_t i2s_audio_get_sample_rate(void) { return 48000; }

// psram_alloc/heap_budget required since hal_tdm_deinterleaver uses psram_alloc()
#include "../../src/heap_budget.cpp"
#include "../../src/psram_alloc.cpp"

// Bring in the deinterleaver implementation directly (native test pattern)
#include "../../src/hal/hal_tdm_deinterleaver.cpp"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Load a synthetic 4-slot TDM buffer: each frame is [ch1][ch2][ch3][ch4].
// Values encode frame index and channel so assertions can detect misrouting.
static void loadTdmFrames(uint32_t numFrames) {
    for (uint32_t f = 0; f < numFrames && f < 128; f++) {
        g_tdmFeedBuf[f * 4 + 0] = (int32_t)(0x01000000 | f);  // CH1
        g_tdmFeedBuf[f * 4 + 1] = (int32_t)(0x02000000 | f);  // CH2
        g_tdmFeedBuf[f * 4 + 2] = (int32_t)(0x03000000 | f);  // CH3
        g_tdmFeedBuf[f * 4 + 3] = (int32_t)(0x04000000 | f);  // CH4
    }
    g_tdmFeedFrames = numFrames;
}

// Reset the synthetic feed (zero frames available)
static void clearTdmFeed() {
    g_tdmFeedFrames = 0;
    memset(g_tdmFeedBuf, 0, sizeof(g_tdmFeedBuf));
}

// Output buffers for pair A and pair B reads
static int32_t pairAOut[128 * 2] = {};
static int32_t pairBOut[128 * 2] = {};

// ---------------------------------------------------------------------------
// Per-test setup/teardown
// ---------------------------------------------------------------------------

static HalTdmDeinterleaver* g_deint = nullptr;

void setUp(void) {
    clearTdmFeed();
    memset(pairAOut, 0, sizeof(pairAOut));
    memset(pairBOut, 0, sizeof(pairBOut));
    g_deint = new HalTdmDeinterleaver();
}

void tearDown(void) {
    delete g_deint;
    g_deint = nullptr;
    // Clear singleton to prevent stale pointer from leaking across tests
    // (the singleton is cleared by deinit(), which the destructor calls)
}

// ---------------------------------------------------------------------------
// Test 1: init() allocates buffers and returns true
// ---------------------------------------------------------------------------
void test_init_succeeds(void) {
    bool ok = g_deint->init(2);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_FALSE(g_deint->isReady());   // Not ready until first read
}

// ---------------------------------------------------------------------------
// Test 2: init() is idempotent
// ---------------------------------------------------------------------------
void test_init_idempotent(void) {
    TEST_ASSERT_TRUE(g_deint->init(2));
    TEST_ASSERT_TRUE(g_deint->init(2));   // Second call returns true without re-allocating
    TEST_ASSERT_FALSE(g_deint->isReady());
}

// ---------------------------------------------------------------------------
// Test 3: deinit() clears ready state
// ---------------------------------------------------------------------------
void test_deinit_clears_ready(void) {
    g_deint->init(2);
    loadTdmFrames(4);
    AudioInputSource srcA = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcB = AUDIO_INPUT_SOURCE_INIT;
    g_deint->buildSources("A", "B", &srcA, &srcB);
    srcA.read(pairAOut, 4);
    TEST_ASSERT_TRUE(g_deint->isReady());

    g_deint->deinit();
    TEST_ASSERT_FALSE(g_deint->isReady());
}

// ---------------------------------------------------------------------------
// Test 4: buildSources() populates both AudioInputSource structs
// ---------------------------------------------------------------------------
void test_build_sources_populates_callbacks(void) {
    g_deint->init(2);
    AudioInputSource srcA = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcB = AUDIO_INPUT_SOURCE_INIT;
    g_deint->buildSources("ES9843PRO CH1/2", "ES9843PRO CH3/4", &srcA, &srcB);

    TEST_ASSERT_NOT_NULL(srcA.read);
    TEST_ASSERT_NOT_NULL(srcA.isActive);
    TEST_ASSERT_NOT_NULL(srcA.getSampleRate);
    TEST_ASSERT_NOT_NULL(srcB.read);
    TEST_ASSERT_NOT_NULL(srcB.isActive);
    TEST_ASSERT_NOT_NULL(srcB.getSampleRate);
}

// ---------------------------------------------------------------------------
// Test 5: Pair A correctly extracts CH1 and CH2 into L/R slots
// ---------------------------------------------------------------------------
void test_pair_a_extracts_ch1_ch2(void) {
    g_deint->init(2);
    AudioInputSource srcA = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcB = AUDIO_INPUT_SOURCE_INIT;
    g_deint->buildSources("A", "B", &srcA, &srcB);

    loadTdmFrames(4);
    uint32_t got = srcA.read(pairAOut, 4);
    TEST_ASSERT_EQUAL_UINT32(4, got);

    // Frame 0: L=CH1 value, R=CH2 value
    TEST_ASSERT_EQUAL_INT32((int32_t)(0x01000000 | 0), pairAOut[0]);  // CH1 → L
    TEST_ASSERT_EQUAL_INT32((int32_t)(0x02000000 | 0), pairAOut[1]);  // CH2 → R
    // Frame 3
    TEST_ASSERT_EQUAL_INT32((int32_t)(0x01000000 | 3), pairAOut[6]);
    TEST_ASSERT_EQUAL_INT32((int32_t)(0x02000000 | 3), pairAOut[7]);
}

// ---------------------------------------------------------------------------
// Test 6: Pair B returns CH3/CH4 from the buffer pair A just filled
// ---------------------------------------------------------------------------
void test_pair_b_extracts_ch3_ch4(void) {
    g_deint->init(2);
    AudioInputSource srcA = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcB = AUDIO_INPUT_SOURCE_INIT;
    g_deint->buildSources("A", "B", &srcA, &srcB);

    loadTdmFrames(4);
    srcA.read(pairAOut, 4);        // Triggers deinterleave + ping-pong swap
    uint32_t got = srcB.read(pairBOut, 4);
    TEST_ASSERT_EQUAL_UINT32(4, got);

    // Frame 0: L=CH3 value, R=CH4 value
    TEST_ASSERT_EQUAL_INT32((int32_t)(0x03000000 | 0), pairBOut[0]);  // CH3 → L
    TEST_ASSERT_EQUAL_INT32((int32_t)(0x04000000 | 0), pairBOut[1]);  // CH4 → R
    // Frame 3
    TEST_ASSERT_EQUAL_INT32((int32_t)(0x03000000 | 3), pairBOut[6]);
    TEST_ASSERT_EQUAL_INT32((int32_t)(0x04000000 | 3), pairBOut[7]);
}

// ---------------------------------------------------------------------------
// Test 7: Pair A returns the correct frame count from a partial buffer
// ---------------------------------------------------------------------------
void test_pair_a_correct_frame_count(void) {
    g_deint->init(2);
    AudioInputSource srcA = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcB = AUDIO_INPUT_SOURCE_INIT;
    g_deint->buildSources("A", "B", &srcA, &srcB);

    loadTdmFrames(7);
    uint32_t got = srcA.read(pairAOut, 64);  // Request more than available
    TEST_ASSERT_EQUAL_UINT32(7, got);
}

// ---------------------------------------------------------------------------
// Test 8: Pair B returns 0 before pair A has produced any data
// ---------------------------------------------------------------------------
void test_pair_b_returns_zero_before_pair_a(void) {
    g_deint->init(2);
    AudioInputSource srcA = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcB = AUDIO_INPUT_SOURCE_INIT;
    g_deint->buildSources("A", "B", &srcA, &srcB);

    // No TDM feed, no pair A read
    uint32_t got = srcB.read(pairBOut, 4);
    TEST_ASSERT_EQUAL_UINT32(0, got);
}

// ---------------------------------------------------------------------------
// Test 9: Pair B is limited to the frame count pair A stored
// ---------------------------------------------------------------------------
void test_pair_b_frame_count_matches_pair_a(void) {
    g_deint->init(2);
    AudioInputSource srcA = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcB = AUDIO_INPUT_SOURCE_INIT;
    g_deint->buildSources("A", "B", &srcA, &srcB);

    loadTdmFrames(5);
    srcA.read(pairAOut, 64);

    // Pair B requests 64 but should return only 5 (what pair A produced)
    uint32_t got = srcB.read(pairBOut, 64);
    TEST_ASSERT_EQUAL_UINT32(5, got);
}

// ---------------------------------------------------------------------------
// Test 10: Deinterleave preserves per-frame ordering for all channels
// ---------------------------------------------------------------------------
void test_deinterleave_all_channels_ordering(void) {
    g_deint->init(2);
    AudioInputSource srcA = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcB = AUDIO_INPUT_SOURCE_INIT;
    g_deint->buildSources("A", "B", &srcA, &srcB);

    loadTdmFrames(8);
    srcA.read(pairAOut, 8);
    srcB.read(pairBOut, 8);

    for (uint32_t f = 0; f < 8; f++) {
        TEST_ASSERT_EQUAL_INT32((int32_t)(0x01000000 | f), pairAOut[f * 2 + 0]);  // CH1 L
        TEST_ASSERT_EQUAL_INT32((int32_t)(0x02000000 | f), pairAOut[f * 2 + 1]);  // CH2 R
        TEST_ASSERT_EQUAL_INT32((int32_t)(0x03000000 | f), pairBOut[f * 2 + 0]);  // CH3 L
        TEST_ASSERT_EQUAL_INT32((int32_t)(0x04000000 | f), pairBOut[f * 2 + 1]);  // CH4 R
    }
}

// ---------------------------------------------------------------------------
// Test 11: Ping-pong — pair B after two pair A calls reads the second fill
// ---------------------------------------------------------------------------
void test_pingpong_reads_latest_pair_a_fill(void) {
    g_deint->init(2);
    AudioInputSource srcA = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcB = AUDIO_INPUT_SOURCE_INIT;
    g_deint->buildSources("A", "B", &srcA, &srcB);

    // First fill: frames with CH1 encoded as 0x01000000 | f
    loadTdmFrames(4);
    srcA.read(pairAOut, 4);

    // Second fill: frames with a different signature — shift all values up
    for (uint32_t f = 0; f < 4; f++) {
        g_tdmFeedBuf[f * 4 + 0] = (int32_t)(0x11000000 | f);  // CH1 new
        g_tdmFeedBuf[f * 4 + 1] = (int32_t)(0x12000000 | f);  // CH2 new
        g_tdmFeedBuf[f * 4 + 2] = (int32_t)(0x13000000 | f);  // CH3 new
        g_tdmFeedBuf[f * 4 + 3] = (int32_t)(0x14000000 | f);  // CH4 new
    }
    g_tdmFeedFrames = 4;
    srcA.read(pairAOut, 4);   // Second pair A fill — moves to next ping-pong side

    // Pair B should read CH3/CH4 from the SECOND fill
    uint32_t got = srcB.read(pairBOut, 4);
    TEST_ASSERT_EQUAL_UINT32(4, got);
    TEST_ASSERT_EQUAL_INT32((int32_t)(0x13000000 | 0), pairBOut[0]);  // CH3 new frame 0
    TEST_ASSERT_EQUAL_INT32((int32_t)(0x14000000 | 0), pairBOut[1]);  // CH4 new frame 0
}

// ---------------------------------------------------------------------------
// Test 12: buildSources() assigns correct names to both sources
// ---------------------------------------------------------------------------
void test_build_sources_names(void) {
    g_deint->init(2);
    AudioInputSource srcA = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcB = AUDIO_INPUT_SOURCE_INIT;
    g_deint->buildSources("ES9843PRO CH1/2", "ES9843PRO CH3/4", &srcA, &srcB);

    TEST_ASSERT_EQUAL_STRING("ES9843PRO CH1/2", srcA.name);
    TEST_ASSERT_EQUAL_STRING("ES9843PRO CH3/4", srcB.name);
}

// ---------------------------------------------------------------------------
// Test 13: isActive callbacks reflect TDM feed state in native test
// ---------------------------------------------------------------------------
void test_is_active_reflects_tdm_feed(void) {
    g_deint->init(2);
    AudioInputSource srcA = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcB = AUDIO_INPUT_SOURCE_INIT;
    g_deint->buildSources("A", "B", &srcA, &srcB);

    clearTdmFeed();
    TEST_ASSERT_FALSE(srcA.isActive());

    loadTdmFrames(4);
    TEST_ASSERT_TRUE(srcA.isActive());
}

// ---------------------------------------------------------------------------
// Test 14: getSampleRate() returns 48000 in native context
// ---------------------------------------------------------------------------
void test_get_sample_rate(void) {
    g_deint->init(2);
    AudioInputSource srcA = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcB = AUDIO_INPUT_SOURCE_INIT;
    g_deint->buildSources("A", "B", &srcA, &srcB);

    TEST_ASSERT_EQUAL_UINT32(48000, srcA.getSampleRate());
    TEST_ASSERT_EQUAL_UINT32(48000, srcB.getSampleRate());
}

// ---------------------------------------------------------------------------
// Test 15: buildSources() leaves lane/halSlot at defaults (bridge sets them)
// ---------------------------------------------------------------------------
void test_build_sources_lane_halslot_defaults(void) {
    g_deint->init(2);
    AudioInputSource srcA = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcB = AUDIO_INPUT_SOURCE_INIT;
    g_deint->buildSources("A", "B", &srcA, &srcB);

    // Bridge sets lane after registration — deinterleaver must not pre-set it
    TEST_ASSERT_EQUAL_UINT8(0, srcA.lane);
    TEST_ASSERT_EQUAL_UINT8(0xFF, srcA.halSlot);
    TEST_ASSERT_EQUAL_UINT8(0, srcB.lane);
    TEST_ASSERT_EQUAL_UINT8(0xFF, srcB.halSlot);
}

// ---------------------------------------------------------------------------
// Test 16: pair A returns 0 when TDM feed is empty
// ---------------------------------------------------------------------------
void test_pair_a_returns_zero_on_empty_feed(void) {
    g_deint->init(2);
    AudioInputSource srcA = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcB = AUDIO_INPUT_SOURCE_INIT;
    g_deint->buildSources("A", "B", &srcA, &srcB);

    clearTdmFeed();
    uint32_t got = srcA.read(pairAOut, 4);
    TEST_ASSERT_EQUAL_UINT32(0, got);
    TEST_ASSERT_FALSE(g_deint->isReady());
}

// ---------------------------------------------------------------------------
// Test 17: pair B isActive returns false before pair A produces first frame
// ---------------------------------------------------------------------------
void test_pair_b_inactive_before_pair_a(void) {
    g_deint->init(2);
    AudioInputSource srcA = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcB = AUDIO_INPUT_SOURCE_INIT;
    g_deint->buildSources("A", "B", &srcA, &srcB);

    TEST_ASSERT_FALSE(srcB.isActive());

    loadTdmFrames(1);
    srcA.read(pairAOut, 1);
    TEST_ASSERT_TRUE(srcB.isActive());
}

// ---------------------------------------------------------------------------
// Test 18: init() without prior deinit() on second instance sees fresh state
// ---------------------------------------------------------------------------
void test_second_instance_after_first_deinit(void) {
    g_deint->init(2);
    loadTdmFrames(2);
    AudioInputSource srcA = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcB = AUDIO_INPUT_SOURCE_INIT;
    g_deint->buildSources("A", "B", &srcA, &srcB);
    srcA.read(pairAOut, 2);
    TEST_ASSERT_TRUE(g_deint->isReady());

    // Destroy and rebuild
    delete g_deint;
    g_deint = new HalTdmDeinterleaver();
    TEST_ASSERT_FALSE(g_deint->isReady());
    TEST_ASSERT_TRUE(g_deint->init(2));
}

// ---------------------------------------------------------------------------
// Test 19: first instance gets slot 0 — callbacks are valid after buildSources
// ---------------------------------------------------------------------------
void test_tdm_deinterleaver_first_instance_gets_slot_0(void) {
    AudioInputSource srcA = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcB = AUDIO_INPUT_SOURCE_INIT;
    g_deint->init(2);
    g_deint->buildSources("CH1/2", "CH3/4", &srcA, &srcB);
    TEST_ASSERT_NOT_NULL(srcA.read);
    TEST_ASSERT_NOT_NULL(srcB.read);
    // deinit via tearDown — verify slot is freed by re-init
    g_deint->deinit();
    // Re-initialise and rebuild: slot should be free again
    g_deint->init(2);
    AudioInputSource srcA2 = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcB2 = AUDIO_INPUT_SOURCE_INIT;
    g_deint->buildSources("CH1/2", "CH3/4", &srcA2, &srcB2);
    TEST_ASSERT_NOT_NULL(srcA2.read);
    TEST_ASSERT_NOT_NULL(srcB2.read);
}

// ---------------------------------------------------------------------------
// Test 20: two concurrent instances get different thunk pointers
// ---------------------------------------------------------------------------
void test_tdm_deinterleaver_second_instance_gets_different_thunks(void) {
    HalTdmDeinterleaver tdm1;
    AudioInputSource srcA0 = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcB0 = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcA1 = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcB1 = AUDIO_INPUT_SOURCE_INIT;

    g_deint->init(2);
    g_deint->buildSources("A-CH1/2", "A-CH3/4", &srcA0, &srcB0);

    tdm1.init(2);
    tdm1.buildSources("B-CH1/2", "B-CH3/4", &srcA1, &srcB1);

    // Both instances must have non-null callbacks
    TEST_ASSERT_NOT_NULL(srcA0.read);
    TEST_ASSERT_NOT_NULL(srcA1.read);
    // The two instances must receive DIFFERENT read function pointers (slot 0 vs slot 1)
    TEST_ASSERT_NOT_EQUAL((void*)srcA0.read, (void*)srcA1.read);
    TEST_ASSERT_NOT_EQUAL((void*)srcB0.read, (void*)srcB1.read);

    tdm1.deinit();
}

// ---------------------------------------------------------------------------
// Test 21: buildSources() with all slots full returns gracefully (null callbacks)
// ---------------------------------------------------------------------------
void test_tdm_deinterleaver_slot_full_buildSources_returns_gracefully(void) {
    HalTdmDeinterleaver tdm1, tdm2;
    AudioInputSource srcA0 = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcB0 = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcA1 = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcB1 = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcA2 = AUDIO_INPUT_SOURCE_INIT;
    AudioInputSource srcB2 = AUDIO_INPUT_SOURCE_INIT;

    // Fill both slots
    g_deint->init(2);
    g_deint->buildSources("D0-A", "D0-B", &srcA0, &srcB0);
    tdm1.init(2);
    tdm1.buildSources("D1-A", "D1-B", &srcA1, &srcB1);

    // Third instance: all slots full — buildSources must not crash and must leave
    // callbacks null (indicating failure to the caller)
    tdm2.init(2);
    tdm2.buildSources("D2-A", "D2-B", &srcA2, &srcB2);
    TEST_ASSERT_NULL(srcA2.read);
    TEST_ASSERT_NULL(srcB2.read);

    tdm1.deinit();
    // tdm2 and g_deint cleaned up by tearDown / their destructors
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();

    RUN_TEST(test_init_succeeds);
    RUN_TEST(test_init_idempotent);
    RUN_TEST(test_deinit_clears_ready);
    RUN_TEST(test_build_sources_populates_callbacks);
    RUN_TEST(test_pair_a_extracts_ch1_ch2);
    RUN_TEST(test_pair_b_extracts_ch3_ch4);
    RUN_TEST(test_pair_a_correct_frame_count);
    RUN_TEST(test_pair_b_returns_zero_before_pair_a);
    RUN_TEST(test_pair_b_frame_count_matches_pair_a);
    RUN_TEST(test_deinterleave_all_channels_ordering);
    RUN_TEST(test_pingpong_reads_latest_pair_a_fill);
    RUN_TEST(test_build_sources_names);
    RUN_TEST(test_is_active_reflects_tdm_feed);
    RUN_TEST(test_get_sample_rate);
    RUN_TEST(test_build_sources_lane_halslot_defaults);
    RUN_TEST(test_pair_a_returns_zero_on_empty_feed);
    RUN_TEST(test_pair_b_inactive_before_pair_a);
    RUN_TEST(test_second_instance_after_first_deinit);
    RUN_TEST(test_tdm_deinterleaver_first_instance_gets_slot_0);
    RUN_TEST(test_tdm_deinterleaver_second_instance_gets_different_thunks);
    RUN_TEST(test_tdm_deinterleaver_slot_full_buildSources_returns_gracefully);

    return UNITY_END();
}
