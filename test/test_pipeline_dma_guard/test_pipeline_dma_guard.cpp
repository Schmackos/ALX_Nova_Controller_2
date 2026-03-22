/**
 * test_pipeline_dma_guard.cpp
 *
 * Unit tests for the DMA heap pressure fix:
 *   - AudioState.dmaAllocFailed and dmaAllocFailMask default values
 *   - dmaAllocFailMask bit encoding (lanes 0-7 for rawBuf, 8-15 for sinkBuf)
 *   - audio_pipeline_set_source() validation logic (bool return)
 *   - audio_pipeline_set_sink() validation logic (bool return)
 *   - DIAG_AUDIO_DMA_ALLOC_FAIL error code value
 *
 * Tests run on the native platform (host machine, no hardware).
 * Validation logic is replicated inline from audio_pipeline.cpp
 * (test_build_src = no — same approach as test_matrix_bounds).
 *
 * Build flags required (already in platformio.ini [env:native]):
 *   -D UNIT_TEST -D NATIVE_TEST -D DSP_ENABLED -D DAC_ENABLED -D USB_AUDIO_ENABLED
 */

#include <unity.h>
#include <cstring>
#include <stdint.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// Pull in the real config.h for AUDIO_PIPELINE_MAX_INPUTS, AUDIO_OUT_MAX_SINKS, etc.
#include "../../src/config.h"

// Pull in the real AppState to verify AudioState fields
#include "../../src/app_state.h"

// Pull in diagnostic error codes
#include "../../src/diag_error_codes.h"

// Pull in AudioInputSource and AudioOutputSink structs
#include "../../src/audio_input_source.h"
#include "../../src/audio_output_sink.h"

// Pull in pipeline header for dimension constants
#include "../../src/audio_pipeline.h"

// ============================================================
// Replicated validation logic from audio_pipeline.cpp
//
// These mirror the exact bounds checks at the top of
// audio_pipeline_set_source() and audio_pipeline_set_sink().
// The NATIVE_TEST code path uses static arrays (no heap_caps),
// but the argument validation is identical to the ESP32 path.
// ============================================================

static bool pipeline_set_source_validate(int lane, const AudioInputSource *src) {
    if (lane < 0 || lane >= AUDIO_PIPELINE_MAX_INPUTS || !src) return false;
    if (lane * 2 + 1 >= AUDIO_PIPELINE_MATRIX_SIZE) return false;
    return true;
}

static bool pipeline_set_sink_validate(int slot, const AudioOutputSink *sink) {
    if (slot < 0 || slot >= AUDIO_OUT_MAX_SINKS || !sink) return false;
    if (sink->firstChannel + sink->channelCount > AUDIO_PIPELINE_MATRIX_SIZE) return false;
    return true;
}

// ============================================================
// setUp / tearDown
// ============================================================

void setUp(void) {
    ArduinoMock::reset();
    // Reset DMA tracking fields in AppState
    AppState &as = AppState::getInstance();
    as.audio.dmaAllocFailed = false;
    as.audio.dmaAllocFailMask = 0;
}

void tearDown(void) {}

// ============================================================
// Group 1: AudioState DMA Fields Defaults (2 tests)
// ============================================================

// 1a. Default values after setUp reset
void test_audio_state_dma_fields_default(void) {
    AppState &as = AppState::getInstance();
    TEST_ASSERT_FALSE(as.audio.dmaAllocFailed);
    TEST_ASSERT_EQUAL_UINT16(0, as.audio.dmaAllocFailMask);
}

// 1b. Verify field types are correct sizes for the encoding scheme
void test_audio_state_dma_field_sizes(void) {
    // dmaAllocFailed is bool (1 byte)
    TEST_ASSERT_EQUAL(1, sizeof(AppState::getInstance().audio.dmaAllocFailed));
    // dmaAllocFailMask is uint16_t (2 bytes) — fits 8 source lanes + 8 sink slots
    TEST_ASSERT_EQUAL(2, sizeof(AppState::getInstance().audio.dmaAllocFailMask));
}

// ============================================================
// Group 2: DMA Fail Mask Encoding (2 tests)
//
// The dmaAllocFailMask uses bits 0-7 for rawBuf lanes (input)
// and bits 8-15 for sinkBuf slots (output). This encoding is
// set by audio_pipeline.cpp: `(1u << lane)` for sources and
// `(1u << (slot + 8))` for sinks.
// ============================================================

// 2a. Bits 0-7 correspond to rawBuf lanes (source input DMA buffers)
void test_dma_fail_mask_lane_encoding(void) {
    AppState &as = AppState::getInstance();
    for (int lane = 0; lane < 8; lane++) {
        as.audio.dmaAllocFailMask = 0;
        as.audio.dmaAllocFailMask |= (1u << lane);

        // Verify correct bit is set
        TEST_ASSERT_EQUAL_UINT16((1u << lane), as.audio.dmaAllocFailMask);
        // Verify the bit is in the lower byte (lanes 0-7)
        TEST_ASSERT_TRUE((as.audio.dmaAllocFailMask & 0x00FF) != 0);
        TEST_ASSERT_TRUE((as.audio.dmaAllocFailMask & 0xFF00) == 0);
    }
    // Reset
    as.audio.dmaAllocFailMask = 0;
}

// 2b. Bits 8-15 correspond to sinkBuf slots (output DMA buffers)
void test_dma_fail_mask_sink_encoding(void) {
    AppState &as = AppState::getInstance();
    for (int slot = 0; slot < 8; slot++) {
        as.audio.dmaAllocFailMask = 0;
        as.audio.dmaAllocFailMask |= (1u << (slot + 8));

        // Verify correct bit is set
        TEST_ASSERT_EQUAL_UINT16((1u << (slot + 8)), as.audio.dmaAllocFailMask);
        // Verify the bit is in the upper byte (sinks 0-7)
        TEST_ASSERT_TRUE((as.audio.dmaAllocFailMask & 0x00FF) == 0);
        TEST_ASSERT_TRUE((as.audio.dmaAllocFailMask & 0xFF00) != 0);
    }
    // Reset
    as.audio.dmaAllocFailMask = 0;
}

// ============================================================
// Group 3: set_source() Validation Logic (2 tests)
//
// Replicated from audio_pipeline_set_source() argument checks.
// On NATIVE_TEST the real function also has these same guards
// before the static-array copy path.
// ============================================================

// 3a. Valid source at lane 0 passes validation
void test_set_source_returns_true_valid(void) {
    AudioInputSource src = AUDIO_INPUT_SOURCE_INIT;
    src.name = "TestADC";
    src.lane = 0;

    bool result = pipeline_set_source_validate(0, &src);
    TEST_ASSERT_TRUE(result);

    // Also valid at the last lane that fits in the matrix
    // lane * 2 + 1 < AUDIO_PIPELINE_MATRIX_SIZE (16)
    // Max lane: 7 (7*2+1 = 15 < 16)
    result = pipeline_set_source_validate(AUDIO_PIPELINE_MAX_INPUTS - 1, &src);
    TEST_ASSERT_TRUE(result);
}

// 3b. Invalid lane values or NULL source fail validation
void test_set_source_returns_false_invalid_lane(void) {
    AudioInputSource src = AUDIO_INPUT_SOURCE_INIT;
    src.name = "TestADC";

    // Negative lane
    TEST_ASSERT_FALSE(pipeline_set_source_validate(-1, &src));

    // Lane way out of range
    TEST_ASSERT_FALSE(pipeline_set_source_validate(99, &src));

    // Lane at AUDIO_PIPELINE_MAX_INPUTS (one past last valid)
    TEST_ASSERT_FALSE(pipeline_set_source_validate(AUDIO_PIPELINE_MAX_INPUTS, &src));

    // NULL source pointer
    TEST_ASSERT_FALSE(pipeline_set_source_validate(0, NULL));
}

// ============================================================
// Group 4: set_sink() Validation Logic (2 tests)
//
// Replicated from audio_pipeline_set_sink() argument checks.
// Includes the firstChannel + channelCount overflow guard.
// ============================================================

// 4a. Valid sink at slot 0 passes validation
void test_set_sink_returns_true_valid(void) {
    AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
    sink.name = "TestDAC";
    sink.firstChannel = 0;
    sink.channelCount = 2;

    bool result = pipeline_set_sink_validate(0, &sink);
    TEST_ASSERT_TRUE(result);

    // Also valid at the last slot
    result = pipeline_set_sink_validate(AUDIO_OUT_MAX_SINKS - 1, &sink);
    TEST_ASSERT_TRUE(result);

    // Valid at the edge of the matrix (ch(MATRIX_SIZE-2)+2 = MATRIX_SIZE, OK)
    sink.firstChannel = AUDIO_PIPELINE_MATRIX_SIZE - 2;
    sink.channelCount = 2;
    result = pipeline_set_sink_validate(0, &sink);
    TEST_ASSERT_TRUE(result);
}

// 4b. Invalid slot, NULL sink, or channel overflow fail validation
void test_set_sink_returns_false_invalid(void) {
    AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
    sink.name = "TestDAC";
    sink.firstChannel = 0;
    sink.channelCount = 2;

    // Negative slot
    TEST_ASSERT_FALSE(pipeline_set_sink_validate(-1, &sink));

    // Slot out of range
    TEST_ASSERT_FALSE(pipeline_set_sink_validate(99, &sink));

    // Slot at AUDIO_OUT_MAX_SINKS (one past last valid)
    TEST_ASSERT_FALSE(pipeline_set_sink_validate(AUDIO_OUT_MAX_SINKS, &sink));

    // NULL sink pointer
    TEST_ASSERT_FALSE(pipeline_set_sink_validate(0, NULL));

    // firstChannel + channelCount > MATRIX_SIZE (overflow guard)
    AudioOutputSink bad_sink = AUDIO_OUTPUT_SINK_INIT;
    bad_sink.name = "Overflow";
    bad_sink.firstChannel = (uint8_t)(AUDIO_PIPELINE_MATRIX_SIZE - 1);  // last ch + 2 > MATRIX_SIZE
    bad_sink.channelCount = 2;
    TEST_ASSERT_FALSE(pipeline_set_sink_validate(0, &bad_sink));

    // Edge: firstChannel = MATRIX_SIZE (out of bounds)
    AudioOutputSink edge_sink = AUDIO_OUTPUT_SINK_INIT;
    edge_sink.name = "Edge";
    edge_sink.firstChannel = AUDIO_PIPELINE_MATRIX_SIZE;
    edge_sink.channelCount = 1;
    TEST_ASSERT_FALSE(pipeline_set_sink_validate(0, &edge_sink));
}

// ============================================================
// Group 5: Diagnostic Error Code (1 test)
// ============================================================

// 5a. DIAG_AUDIO_DMA_ALLOC_FAIL has the correct hex value and subsystem
void test_diag_code_value(void) {
    TEST_ASSERT_EQUAL_HEX16(0x200E, DIAG_AUDIO_DMA_ALLOC_FAIL);

    // Verify it is in the Audio subsystem (0x20xx)
    DiagSubsystem sub = diag_subsystem_from_code(DIAG_AUDIO_DMA_ALLOC_FAIL);
    TEST_ASSERT_EQUAL(DIAG_SUB_AUDIO, sub);

    // Verify it comes before the sentinel
    TEST_ASSERT_TRUE(DIAG_AUDIO_DMA_ALLOC_FAIL < DIAG_CODE_COUNT);
}

// ============================================================
// Main
// ============================================================

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    // Group 1: AudioState DMA Fields Defaults
    RUN_TEST(test_audio_state_dma_fields_default);
    RUN_TEST(test_audio_state_dma_field_sizes);

    // Group 2: DMA Fail Mask Encoding
    RUN_TEST(test_dma_fail_mask_lane_encoding);
    RUN_TEST(test_dma_fail_mask_sink_encoding);

    // Group 3: set_source() Validation Logic
    RUN_TEST(test_set_source_returns_true_valid);
    RUN_TEST(test_set_source_returns_false_invalid_lane);

    // Group 4: set_sink() Validation Logic
    RUN_TEST(test_set_sink_returns_true_valid);
    RUN_TEST(test_set_sink_returns_false_invalid);

    // Group 5: Diagnostic Error Code
    RUN_TEST(test_diag_code_value);

    return UNITY_END();
}
