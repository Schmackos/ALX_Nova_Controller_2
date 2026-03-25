/**
 * test_format_negotiation.cpp
 *
 * Unit tests for Phase 1+2 hardening: format negotiation features
 *
 *   1. AudioInputSource format fields (bitDepth, isDsd) — default init
 *   2. AudioOutputSink format fields (sampleRate, sampleRatesMask, bitDepth,
 *      maxBitDepth, supportsDsd) — default init
 *   3. HAL_CAP_ASRC capability bit (bit 14) presence and non-overlap with others
 *   4. DIAG_AUDIO_RATE_MISMATCH / DIAG_AUDIO_DSD_DETECTED codes in range 0x200F-0x2010
 *   5. EVT_FORMAT_CHANGE event bit (bit 18) distinct from all other event bits
 *   6. AudioState format fields (rateMismatch, laneSampleRates, laneDsd) — defaults
 *
 * Runs on the native platform (host machine, no hardware needed).
 */

#include <unity.h>
#include <cstring>
#include <cstdint>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// Pull in real production headers
#include "../../src/config.h"
#include "../../src/audio_input_source.h"
#include "../../src/audio_output_sink.h"
#include "../../src/hal/hal_types.h"
#include "../../src/diag_error_codes.h"
#include "../../src/app_events.h"
#include "../../src/state/audio_state.h"

// ============================================================
// 1. AudioInputSource — new format fields
// ============================================================

void test_audio_input_source_format_defaults() {
    AudioInputSource src = AUDIO_INPUT_SOURCE_INIT;
    TEST_ASSERT_EQUAL_UINT8(0, src.bitDepth);   // 0 = unknown/auto
    TEST_ASSERT_FALSE(src.isDsd);
}

void test_audio_input_source_format_fields_settable() {
    AudioInputSource src = AUDIO_INPUT_SOURCE_INIT;
    src.bitDepth = 24;
    src.isDsd    = true;
    TEST_ASSERT_EQUAL_UINT8(24, src.bitDepth);
    TEST_ASSERT_TRUE(src.isDsd);
}

void test_audio_input_source_bitdepth_range() {
    AudioInputSource src = AUDIO_INPUT_SOURCE_INIT;
    // Valid bit depths: 16, 24, 32 (and 0 = auto)
    for (uint8_t bd : {(uint8_t)0, (uint8_t)16, (uint8_t)24, (uint8_t)32}) {
        src.bitDepth = bd;
        TEST_ASSERT_EQUAL_UINT8(bd, src.bitDepth);
    }
}

// ============================================================
// 2. AudioOutputSink — new format fields
// ============================================================

void test_audio_output_sink_format_defaults() {
    AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
    TEST_ASSERT_EQUAL_UINT32(0, sink.sampleRate);
    TEST_ASSERT_EQUAL_UINT32(0, sink.sampleRatesMask);
    TEST_ASSERT_EQUAL_UINT8(0,  sink.bitDepth);
    TEST_ASSERT_EQUAL_UINT8(0,  sink.maxBitDepth);
    TEST_ASSERT_FALSE(sink.supportsDsd);
}

void test_audio_output_sink_format_fields_settable() {
    AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
    sink.sampleRate     = 192000;
    sink.sampleRatesMask = HAL_RATE_192K | HAL_RATE_96K | HAL_RATE_48K;
    sink.bitDepth       = 32;
    sink.maxBitDepth    = 32;
    sink.supportsDsd    = true;

    TEST_ASSERT_EQUAL_UINT32(192000, sink.sampleRate);
    TEST_ASSERT_TRUE(sink.sampleRatesMask & HAL_RATE_192K);
    TEST_ASSERT_TRUE(sink.sampleRatesMask & HAL_RATE_96K);
    TEST_ASSERT_TRUE(sink.sampleRatesMask & HAL_RATE_48K);
    TEST_ASSERT_EQUAL_UINT8(32, sink.bitDepth);
    TEST_ASSERT_EQUAL_UINT8(32, sink.maxBitDepth);
    TEST_ASSERT_TRUE(sink.supportsDsd);
}

void test_audio_output_sink_dsd_flag_for_cirrus() {
    // Cirrus Logic DSD-capable devices (CS43198, CS43131, CS43130) should set supportsDsd
    AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
    sink.supportsDsd = true;
    TEST_ASSERT_TRUE(sink.supportsDsd);
    // Non-DSD devices keep default false
    AudioOutputSink sink2 = AUDIO_OUTPUT_SINK_INIT;
    TEST_ASSERT_FALSE(sink2.supportsDsd);
}

// ============================================================
// 3. HAL_CAP_ASRC capability bit
// ============================================================

void test_hal_cap_asrc_is_bit14() {
    TEST_ASSERT_EQUAL_UINT32((1 << 14), HAL_CAP_ASRC);
}

void test_hal_cap_asrc_no_overlap_with_other_caps() {
    uint16_t all_other_caps =
        HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE |
        HAL_CAP_ADC_PATH  | HAL_CAP_DAC_PATH | HAL_CAP_PGA_CONTROL |
        HAL_CAP_HPF_CONTROL | HAL_CAP_CODEC | HAL_CAP_MQA |
        HAL_CAP_LINE_DRIVER | HAL_CAP_APLL | HAL_CAP_DSD |
        HAL_CAP_HP_AMP | HAL_CAP_POWER_MGMT;
    TEST_ASSERT_EQUAL_UINT32(0, HAL_CAP_ASRC & all_other_caps);
}

void test_hal_cap_asrc_fits_in_uint16() {
    // All capability flags must fit in uint16_t (the capabilities field type)
    TEST_ASSERT_LESS_THAN_UINT32(0x10000, (uint32_t)HAL_CAP_ASRC);
}

// ============================================================
// 4. Diagnostic error codes
// ============================================================

void test_diag_rate_mismatch_code_value() {
    // DIAG_AUDIO_RATE_MISMATCH = 0x200F
    TEST_ASSERT_EQUAL_HEX16(0x200F, (uint16_t)DIAG_AUDIO_RATE_MISMATCH);
}

void test_diag_dsd_detected_code_value() {
    // DIAG_AUDIO_DSD_DETECTED = 0x2010
    TEST_ASSERT_EQUAL_HEX16(0x2010, (uint16_t)DIAG_AUDIO_DSD_DETECTED);
}

void test_diag_rate_mismatch_after_dma_alloc_fail() {
    // New codes must come after DIAG_AUDIO_DMA_ALLOC_FAIL = 0x200E
    TEST_ASSERT_GREATER_THAN_UINT16((uint16_t)DIAG_AUDIO_DMA_ALLOC_FAIL,
                                    (uint16_t)DIAG_AUDIO_RATE_MISMATCH);
}

void test_diag_codes_sequential() {
    TEST_ASSERT_EQUAL_HEX16(
        (uint16_t)DIAG_AUDIO_RATE_MISMATCH + 1,
        (uint16_t)DIAG_AUDIO_DSD_DETECTED);
}

// ============================================================
// 5. EVT_FORMAT_CHANGE event bit
// ============================================================

void test_evt_format_change_is_bit18() {
    TEST_ASSERT_EQUAL_UINT32((1UL << 18), EVT_FORMAT_CHANGE);
}

void test_evt_format_change_distinct_from_all_others() {
    // Collect all defined event bits
    uint32_t all_others =
        EVT_OTA | EVT_DISPLAY | EVT_BUZZER | EVT_SIGGEN |
        EVT_DSP_CONFIG | EVT_EEPROM | EVT_USB_AUDIO | EVT_USB_VU |
        EVT_SETTINGS | EVT_ADC_ENABLED | EVT_DIAG | EVT_ETHERNET |
        EVT_HAL_DEVICE | EVT_CHANNEL_MAP | EVT_HEAP_PRESSURE | EVT_HEALTH;
    TEST_ASSERT_EQUAL_UINT32(0, EVT_FORMAT_CHANGE & all_others);
}

void test_evt_format_change_within_evt_any() {
    // Must be covered by EVT_ANY (all 24 usable bits)
    TEST_ASSERT_EQUAL_UINT32(EVT_FORMAT_CHANGE, EVT_FORMAT_CHANGE & EVT_ANY);
}

// ============================================================
// 6. AudioState format fields
// ============================================================

void test_audio_state_format_fields_default() {
    AudioState state;
    TEST_ASSERT_FALSE(state.rateMismatch);
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
        TEST_ASSERT_EQUAL_UINT32(0, state.laneSampleRates[i]);
        TEST_ASSERT_FALSE(state.laneDsd[i]);
    }
}

void test_audio_state_format_fields_settable() {
    AudioState state;
    state.rateMismatch = true;
    state.laneSampleRates[0] = 48000;
    state.laneSampleRates[1] = 44100;
    state.laneDsd[2] = true;

    TEST_ASSERT_TRUE(state.rateMismatch);
    TEST_ASSERT_EQUAL_UINT32(48000, state.laneSampleRates[0]);
    TEST_ASSERT_EQUAL_UINT32(44100, state.laneSampleRates[1]);
    TEST_ASSERT_TRUE(state.laneDsd[2]);
    TEST_ASSERT_FALSE(state.laneDsd[0]);
}

void test_audio_state_lane_count_matches_max_inputs() {
    // laneSampleRates and laneDsd arrays must be AUDIO_PIPELINE_MAX_INPUTS wide
    AudioState state;
    // Write to all lanes to verify no out-of-bounds
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
        state.laneSampleRates[i] = (uint32_t)(48000 * (i + 1));
        state.laneDsd[i] = (i % 2 == 0);
    }
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(48000 * AUDIO_PIPELINE_MAX_INPUTS),
                              state.laneSampleRates[AUDIO_PIPELINE_MAX_INPUTS - 1]);
}

// ============================================================
// 7. DoP (DSD-over-PCM) marker constants
// ============================================================

void test_dop_marker_a_value() {
    // DoP v1.1: marker A byte = 0x05 (appears in MSB of odd frames)
    // This is a documentation/contract test — the value is fixed by the DoP standard
    TEST_ASSERT_EQUAL_HEX8(0x05u, 0x05u);  // Sanity check
    // Verify that a left-justified 32-bit sample with 0x05 MSB would be detected
    uint32_t sample_a = 0x05000000u;
    TEST_ASSERT_EQUAL_HEX8(0x05u, (uint8_t)(sample_a >> 24));
}

void test_dop_marker_b_value() {
    // DoP v1.1: marker B byte = 0xFA (appears in MSB of even frames)
    uint32_t sample_b = 0xFA000000u;
    TEST_ASSERT_EQUAL_HEX8(0xFAu, (uint8_t)(sample_b >> 24));
}

void test_dop_markers_complement() {
    // 0x05 and 0xFA are bitwise complements of each other (mod 256)
    TEST_ASSERT_EQUAL_HEX8(0xFFu, (uint8_t)(0x05u ^ 0xFAu));
}

void test_audio_input_source_isdsd_default_false() {
    // By default no lane is DSD — prevents false DoP triggers at init
    AudioInputSource src = AUDIO_INPUT_SOURCE_INIT;
    TEST_ASSERT_FALSE(src.isDsd);
}

void test_audio_state_lanedsd_default_false() {
    // AudioState laneDsd mirrors source isDsd, initializes to all-false
    AudioState state;
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
        TEST_ASSERT_FALSE(state.laneDsd[i]);
    }
}

void test_audio_state_lanedsd_settable_per_lane() {
    AudioState state;
    state.laneDsd[0] = true;
    state.laneDsd[3] = true;
    TEST_ASSERT_TRUE(state.laneDsd[0]);
    TEST_ASSERT_FALSE(state.laneDsd[1]);
    TEST_ASSERT_FALSE(state.laneDsd[2]);
    TEST_ASSERT_TRUE(state.laneDsd[3]);
}

void test_diag_dsd_detected_after_rate_mismatch() {
    // DIAG_AUDIO_DSD_DETECTED must be sequential after DIAG_AUDIO_RATE_MISMATCH
    TEST_ASSERT_EQUAL_HEX16(
        (uint16_t)DIAG_AUDIO_RATE_MISMATCH + 1,
        (uint16_t)DIAG_AUDIO_DSD_DETECTED);
}

// ============================================================
// 8. HAL_RATE_* bitmask helpers
// ============================================================

void test_hal_rate_masks_distinct_bits() {
    uint32_t combined = HAL_RATE_8K | HAL_RATE_16K | HAL_RATE_44K1 |
                        HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K |
                        HAL_RATE_384K | HAL_RATE_768K;
    // 8 distinct bits → 8 ones in combined
    uint32_t popcount = 0;
    for (uint32_t v = combined; v; v &= v - 1) popcount++;
    TEST_ASSERT_EQUAL_UINT32(8, popcount);
}

void test_hal_rate_384k_768k_bits() {
    // From CLAUDE.md: HAL_RATE_384K = bit 6, HAL_RATE_768K = bit 7
    TEST_ASSERT_EQUAL_UINT32((1 << 6), HAL_RATE_384K);
    TEST_ASSERT_EQUAL_UINT32((1 << 7), HAL_RATE_768K);
}

// ============================================================
// Main
// ============================================================

void setUp() {}
void tearDown() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // AudioInputSource
    RUN_TEST(test_audio_input_source_format_defaults);
    RUN_TEST(test_audio_input_source_format_fields_settable);
    RUN_TEST(test_audio_input_source_bitdepth_range);

    // AudioOutputSink
    RUN_TEST(test_audio_output_sink_format_defaults);
    RUN_TEST(test_audio_output_sink_format_fields_settable);
    RUN_TEST(test_audio_output_sink_dsd_flag_for_cirrus);

    // HAL_CAP_ASRC
    RUN_TEST(test_hal_cap_asrc_is_bit14);
    RUN_TEST(test_hal_cap_asrc_no_overlap_with_other_caps);
    RUN_TEST(test_hal_cap_asrc_fits_in_uint16);

    // Diagnostic codes
    RUN_TEST(test_diag_rate_mismatch_code_value);
    RUN_TEST(test_diag_dsd_detected_code_value);
    RUN_TEST(test_diag_rate_mismatch_after_dma_alloc_fail);
    RUN_TEST(test_diag_codes_sequential);

    // Event bit
    RUN_TEST(test_evt_format_change_is_bit18);
    RUN_TEST(test_evt_format_change_distinct_from_all_others);
    RUN_TEST(test_evt_format_change_within_evt_any);

    // AudioState
    RUN_TEST(test_audio_state_format_fields_default);
    RUN_TEST(test_audio_state_format_fields_settable);
    RUN_TEST(test_audio_state_lane_count_matches_max_inputs);

    // DoP detection constants and state
    RUN_TEST(test_dop_marker_a_value);
    RUN_TEST(test_dop_marker_b_value);
    RUN_TEST(test_dop_markers_complement);
    RUN_TEST(test_audio_input_source_isdsd_default_false);
    RUN_TEST(test_audio_state_lanedsd_default_false);
    RUN_TEST(test_audio_state_lanedsd_settable_per_lane);
    RUN_TEST(test_diag_dsd_detected_after_rate_mismatch);

    // HAL rate masks
    RUN_TEST(test_hal_rate_masks_distinct_bits);
    RUN_TEST(test_hal_rate_384k_768k_bits);

    return UNITY_END();
}
