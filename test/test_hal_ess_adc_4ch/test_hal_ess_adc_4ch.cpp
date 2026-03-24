// test_hal_ess_adc_4ch.cpp
// Regression tests for the generic HalEssAdc4ch descriptor-driven driver.
//
// Verifies that the single generic driver, parameterized by per-chip descriptor
// tables, produces identical register-level behavior to the 4 original drivers:
//   ES9843PRO, ES9842PRO, ES9841, ES9840
//
// All 4 chips output 4 channels on a single TDM data line (Pattern B) and
// register 2 AudioInputSources (CH1/2 + CH3/4) via HalTdmDeinterleaver.
//
// Section layout:
//   1.  Descriptor identity (compatible, chipName, chipId)
//   2.  Capability flags per descriptor
//   3.  Sample rate masks per descriptor
//   4.  Volume type and register addresses
//       4a. VOL4_8BIT — ES9843PRO (0x00=0dB, 0xFF=mute)
//       4b. VOL4_8BIT_INV — ES9841 (0xFF=0dB, 0x00=mute)
//       4c. VOL4_16BIT — ES9842PRO, ES9840 (16-bit LSB pairs)
//   5.  Gain encoding
//       5a. GAIN4_2BIT_6DB (ES9842PRO, ES9840)
//       5b. GAIN4_3BIT_6DB + GAIN4_PACK_3_5 (ES9843PRO)
//       5c. GAIN4_3BIT_6DB + GAIN4_PACK_0_4 (ES9841)
//   6.  HPF (DC blocking)
//       6a. ES9843PRO — single MSB register (bits[7:4] = 4 channels)
//       6b. ES9842PRO, ES9841, ES9840 — per-channel registers
//   7.  Filter encoding
//       7a. FILTER4_GLOBAL_SHIFT5 (ES9843PRO, ES9841)
//       7b. FILTER4_PER_CH_SHIFT2 (ES9842PRO, ES9840)
//   8.  Source names for AudioInputSource pairs
//   9.  Init sequence — lengths and register/value spot checks
//  10.  Deinit sequence — lengths and spot checks
//  11.  ES9840 register map matches ES9842PRO (same silicon family)
//  12.  Chip ID register address (all at 0xE1)
//  13.  Log prefix format

#include <unity.h>
#include <cstring>
#include <cstdint>

// The implementation is guarded by DAC_ENABLED — define it before any includes.
#ifndef DAC_ENABLED
#define DAC_ENABLED
#endif

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Wire.h"
#endif

#include "../../src/hal/hal_types.h"
#include "../../src/drivers/ess_sabre_common.h"

// ===== Inline-include implementation dependencies =====
// hal_i2c_bus.cpp requires hal_wifi_sdio_active() — stub for native tests.
static bool hal_wifi_sdio_active() { return false; }

// Mock persistence layer needed by hal_device_manager.cpp
#include "../test_mocks/Preferences.h"
#include "../test_mocks/LittleFS.h"

// Dependency chain (order matters)
// Define TDM_TEST_PROVIDES_STUBS so hal_tdm_deinterleaver.cpp skips its inline
// i2s_audio_get_sample_rate stub — we provide them here instead.
#define TDM_TEST_PROVIDES_STUBS

// Port-generic TDM stubs (satisfy deinterleaver and adc4ch requirements)
inline uint32_t i2s_port_tdm_read(uint8_t, int32_t*, uint32_t, uint8_t) { return 0; }
inline bool     i2s_port_is_rx_active(uint8_t) { return false; }
inline uint32_t i2s_audio_get_sample_rate(void) { return 48000; }
inline bool     i2s_port_enable_rx(uint8_t, void*) { return true; }
inline void     i2s_port_disable_rx(uint8_t) {}

#include "../../src/heap_budget.cpp"
#include "../../src/psram_alloc.cpp"
#include "../../src/diag_journal.cpp"
#include "../../src/hal/hal_i2c_bus.cpp"
#include "../../src/hal/hal_tdm_deinterleaver.cpp"
#include "../../src/hal/hal_ess_sabre_adc_base.cpp"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_ess_adc_4ch.cpp"
#include "../../src/hal/hal_ess_adc_4ch.h"

// =====================================================================
// All 4 descriptors for parametric iteration
// =====================================================================
static const EssAdc4chDescriptor* kAllDescs[] = {
    &kDescES9843PRO, &kDescES9842PRO, &kDescES9841, &kDescES9840
};
static const int kDescCount = 4;

// =====================================================================
// Test fixtures
// =====================================================================

void setUp(void) {
    WireMock::reset();
    ArduinoMock::reset();
}

void tearDown(void) {
}

// ==========================================================================
// Section 1: Descriptor identity — compatible, chipName, chipId
// ==========================================================================

void test_desc_es9843pro_identity(void) {
    TEST_ASSERT_EQUAL_STRING("ess,es9843pro", kDescES9843PRO.compatible);
    TEST_ASSERT_EQUAL_STRING("ES9843PRO",     kDescES9843PRO.chipName);
    TEST_ASSERT_EQUAL_HEX8(0x8F,              kDescES9843PRO.chipId);
}

void test_desc_es9842pro_identity(void) {
    TEST_ASSERT_EQUAL_STRING("ess,es9842pro", kDescES9842PRO.compatible);
    TEST_ASSERT_EQUAL_STRING("ES9842PRO",     kDescES9842PRO.chipName);
    TEST_ASSERT_EQUAL_HEX8(0x83,              kDescES9842PRO.chipId);
}

void test_desc_es9841_identity(void) {
    TEST_ASSERT_EQUAL_STRING("ess,es9841", kDescES9841.compatible);
    TEST_ASSERT_EQUAL_STRING("ES9841",     kDescES9841.chipName);
    TEST_ASSERT_EQUAL_HEX8(0x91,           kDescES9841.chipId);
}

void test_desc_es9840_identity(void) {
    TEST_ASSERT_EQUAL_STRING("ess,es9840", kDescES9840.compatible);
    TEST_ASSERT_EQUAL_STRING("ES9840",     kDescES9840.chipName);
    TEST_ASSERT_EQUAL_HEX8(0x87,           kDescES9840.chipId);
}

// ==========================================================================
// Section 2: Capability flags per descriptor
// ==========================================================================

void test_caps_es9843pro(void) {
    uint16_t c = kDescES9843PRO.capabilities;
    TEST_ASSERT_TRUE(c & HAL_CAP_ADC_PATH);
    TEST_ASSERT_TRUE(c & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(c & HAL_CAP_PGA_CONTROL);
    TEST_ASSERT_TRUE(c & HAL_CAP_HPF_CONTROL);
    TEST_ASSERT_FALSE(c & HAL_CAP_DAC_PATH);
}

void test_caps_es9842pro(void) {
    uint16_t c = kDescES9842PRO.capabilities;
    TEST_ASSERT_TRUE(c & HAL_CAP_ADC_PATH);
    TEST_ASSERT_TRUE(c & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(c & HAL_CAP_PGA_CONTROL);
    TEST_ASSERT_TRUE(c & HAL_CAP_HPF_CONTROL);
    TEST_ASSERT_FALSE(c & HAL_CAP_DAC_PATH);
}

void test_caps_es9841(void) {
    uint16_t c = kDescES9841.capabilities;
    TEST_ASSERT_TRUE(c & HAL_CAP_ADC_PATH);
    TEST_ASSERT_TRUE(c & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(c & HAL_CAP_PGA_CONTROL);
    TEST_ASSERT_TRUE(c & HAL_CAP_HPF_CONTROL);
    TEST_ASSERT_FALSE(c & HAL_CAP_DAC_PATH);
}

void test_caps_es9840(void) {
    uint16_t c = kDescES9840.capabilities;
    TEST_ASSERT_TRUE(c & HAL_CAP_ADC_PATH);
    TEST_ASSERT_TRUE(c & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(c & HAL_CAP_PGA_CONTROL);
    TEST_ASSERT_TRUE(c & HAL_CAP_HPF_CONTROL);
    TEST_ASSERT_FALSE(c & HAL_CAP_DAC_PATH);
}

// ==========================================================================
// Section 3: Sample rate masks per descriptor
// ==========================================================================

void test_rates_all_support_44k1_to_192k(void) {
    for (int i = 0; i < kDescCount; i++) {
        uint32_t m = kAllDescs[i]->sampleRateMask;
        TEST_ASSERT_TRUE(m & HAL_RATE_44K1);
        TEST_ASSERT_TRUE(m & HAL_RATE_48K);
        TEST_ASSERT_TRUE(m & HAL_RATE_96K);
        TEST_ASSERT_TRUE(m & HAL_RATE_192K);
    }
}

void test_rates_all_have_4_supported_rates(void) {
    for (int i = 0; i < kDescCount; i++) {
        TEST_ASSERT_EQUAL(4, kAllDescs[i]->supportedRateCount);
        TEST_ASSERT_NOT_NULL(kAllDescs[i]->supportedRates);
    }
}

void test_rates_none_support_384k_or_higher(void) {
    for (int i = 0; i < kDescCount; i++) {
        TEST_ASSERT_FALSE(kAllDescs[i]->sampleRateMask & HAL_RATE_384K);
    }
}

// ==========================================================================
// Section 4a: VOL4_8BIT — ES9843PRO (0x00=0dB, 0xFF=mute)
// ==========================================================================

void test_vol_es9843pro_type_and_regs(void) {
    TEST_ASSERT_EQUAL(VOL4_8BIT, kDescES9843PRO.volType);
    TEST_ASSERT_EQUAL_HEX8(0x51, kDescES9843PRO.regVolCh1);
    // CH2=0x52, CH3=0x53, CH4=0x54 — consecutive from regVolCh1
    TEST_ASSERT_EQUAL_HEX8(0x52, kDescES9843PRO.regVolCh1 + 1);
    TEST_ASSERT_EQUAL_HEX8(0x53, kDescES9843PRO.regVolCh1 + 2);
    TEST_ASSERT_EQUAL_HEX8(0x54, kDescES9843PRO.regVolCh1 + 3);
}

// ==========================================================================
// Section 4b: VOL4_8BIT_INV — ES9841 (0xFF=0dB, 0x00=mute)
// ==========================================================================

void test_vol_es9841_type_inverted(void) {
    TEST_ASSERT_EQUAL(VOL4_8BIT_INV, kDescES9841.volType);
    TEST_ASSERT_EQUAL_HEX8(0x51, kDescES9841.regVolCh1);
}

// ==========================================================================
// Section 4c: VOL4_16BIT — ES9842PRO, ES9840
// ==========================================================================

void test_vol_es9842pro_type_and_ch1_reg(void) {
    TEST_ASSERT_EQUAL(VOL4_16BIT, kDescES9842PRO.volType);
    TEST_ASSERT_EQUAL_HEX8(0x6D, kDescES9842PRO.regVolCh1);
}

void test_vol_es9840_type_and_ch1_reg(void) {
    TEST_ASSERT_EQUAL(VOL4_16BIT, kDescES9840.volType);
    TEST_ASSERT_EQUAL_HEX8(0x6D, kDescES9840.regVolCh1);
}

// ==========================================================================
// Section 5a: Gain GAIN4_2BIT_6DB (ES9842PRO, ES9840)
// ==========================================================================

void test_gain_es9842pro_type_and_params(void) {
    TEST_ASSERT_EQUAL(GAIN4_2BIT_6DB, kDescES9842PRO.gainType);
    TEST_ASSERT_EQUAL(18,             kDescES9842PRO.gainMax);
    TEST_ASSERT_EQUAL_HEX8(0x03,     kDescES9842PRO.gainMask);
    TEST_ASSERT_EQUAL_HEX8(0x70,     kDescES9842PRO.regGainPair1);
    TEST_ASSERT_EQUAL_HEX8(0x81,     kDescES9842PRO.regGainPair2);
}

void test_gain_es9840_type_and_params(void) {
    TEST_ASSERT_EQUAL(GAIN4_2BIT_6DB, kDescES9840.gainType);
    TEST_ASSERT_EQUAL(18,             kDescES9840.gainMax);
    TEST_ASSERT_EQUAL_HEX8(0x03,     kDescES9840.gainMask);
    // ES9840 shares gain register addresses with ES9842PRO
    TEST_ASSERT_EQUAL_HEX8(0x70, kDescES9840.regGainPair1);
    TEST_ASSERT_EQUAL_HEX8(0x81, kDescES9840.regGainPair2);
}

// ==========================================================================
// Section 5b: Gain GAIN4_3BIT_6DB + GAIN4_PACK_3_5 (ES9843PRO)
// ==========================================================================

void test_gain_es9843pro_type_pack_3_5(void) {
    TEST_ASSERT_EQUAL(GAIN4_3BIT_6DB, kDescES9843PRO.gainType);
    TEST_ASSERT_EQUAL(GAIN4_PACK_3_5, kDescES9843PRO.gainPack);
    TEST_ASSERT_EQUAL(42,             kDescES9843PRO.gainMax);
    TEST_ASSERT_EQUAL_HEX8(0x07,     kDescES9843PRO.gainMask);
    TEST_ASSERT_EQUAL_HEX8(0x55,     kDescES9843PRO.regGainPair1);
    TEST_ASSERT_EQUAL_HEX8(0x56,     kDescES9843PRO.regGainPair2);
}

// ==========================================================================
// Section 5c: Gain GAIN4_3BIT_6DB + GAIN4_PACK_0_4 (ES9841)
// ==========================================================================

void test_gain_es9841_type_pack_0_4(void) {
    TEST_ASSERT_EQUAL(GAIN4_3BIT_6DB, kDescES9841.gainType);
    TEST_ASSERT_EQUAL(GAIN4_PACK_0_4, kDescES9841.gainPack);
    TEST_ASSERT_EQUAL(42,             kDescES9841.gainMax);
    TEST_ASSERT_EQUAL_HEX8(0x07,     kDescES9841.gainMask);
    TEST_ASSERT_EQUAL_HEX8(0x55,     kDescES9841.regGainPair1);
    TEST_ASSERT_EQUAL_HEX8(0x56,     kDescES9841.regGainPair2);
}

// ==========================================================================
// Section 6a: HPF — ES9843PRO single MSB register (bits[7:4])
// ==========================================================================

void test_hpf_es9843pro_msb_reg_single(void) {
    // ES9843PRO uses single MSB register 0x4D bits[7:4] for all 4 channels
    TEST_ASSERT_EQUAL_HEX8(0x4D, kDescES9843PRO.regHpfCh1);
    // regHpfCh2/3/4 = 0xFF (sentinel: same register)
    TEST_ASSERT_EQUAL_HEX8(0xFF, kDescES9843PRO.regHpfCh2);
    TEST_ASSERT_EQUAL_HEX8(0xFF, kDescES9843PRO.regHpfCh3);
    TEST_ASSERT_EQUAL_HEX8(0xFF, kDescES9843PRO.regHpfCh4);
    // hpfEnableBit = 0xF0 covers all 4 channels in bits[7:4]
    TEST_ASSERT_EQUAL_HEX8(0xF0, kDescES9843PRO.hpfEnableBit);
}

// ==========================================================================
// Section 6b: HPF — per-channel registers (ES9842PRO, ES9841, ES9840)
// ==========================================================================

void test_hpf_es9842pro_per_channel_regs(void) {
    TEST_ASSERT_EQUAL_HEX8(0x65, kDescES9842PRO.regHpfCh1);
    TEST_ASSERT_EQUAL_HEX8(0x76, kDescES9842PRO.regHpfCh2);
    TEST_ASSERT_EQUAL_HEX8(0x87, kDescES9842PRO.regHpfCh3);
    TEST_ASSERT_EQUAL_HEX8(0x98, kDescES9842PRO.regHpfCh4);
    TEST_ASSERT_EQUAL_HEX8(0x04, kDescES9842PRO.hpfEnableBit);
}

void test_hpf_es9841_per_channel_regs(void) {
    TEST_ASSERT_EQUAL_HEX8(0x65, kDescES9841.regHpfCh1);
    TEST_ASSERT_EQUAL_HEX8(0x76, kDescES9841.regHpfCh2);
    TEST_ASSERT_EQUAL_HEX8(0x87, kDescES9841.regHpfCh3);
    TEST_ASSERT_EQUAL_HEX8(0x98, kDescES9841.regHpfCh4);
    TEST_ASSERT_EQUAL_HEX8(0x04, kDescES9841.hpfEnableBit);
}

void test_hpf_es9840_per_channel_regs_match_es9842pro(void) {
    // ES9840 uses the same HPF register map as ES9842PRO
    TEST_ASSERT_EQUAL_HEX8(kDescES9842PRO.regHpfCh1, kDescES9840.regHpfCh1);
    TEST_ASSERT_EQUAL_HEX8(kDescES9842PRO.regHpfCh2, kDescES9840.regHpfCh2);
    TEST_ASSERT_EQUAL_HEX8(kDescES9842PRO.regHpfCh3, kDescES9840.regHpfCh3);
    TEST_ASSERT_EQUAL_HEX8(kDescES9842PRO.regHpfCh4, kDescES9840.regHpfCh4);
    TEST_ASSERT_EQUAL_HEX8(kDescES9842PRO.hpfEnableBit, kDescES9840.hpfEnableBit);
}

// ==========================================================================
// Section 7a: Filter FILTER4_GLOBAL_SHIFT5 (ES9843PRO, ES9841)
// ==========================================================================

void test_filter_es9843pro_global_shift5(void) {
    TEST_ASSERT_EQUAL(FILTER4_GLOBAL_SHIFT5, kDescES9843PRO.filterType);
    TEST_ASSERT_EQUAL_HEX8(0x4A, kDescES9843PRO.regFilter);
    // Single global register — regFilterCh2/3/4 = 0xFF
    TEST_ASSERT_EQUAL_HEX8(0xFF, kDescES9843PRO.regFilterCh2);
    TEST_ASSERT_EQUAL_HEX8(0xFF, kDescES9843PRO.regFilterCh3);
    TEST_ASSERT_EQUAL_HEX8(0xFF, kDescES9843PRO.regFilterCh4);
    TEST_ASSERT_EQUAL_HEX8(0xE0, kDescES9843PRO.filterMask);  // bits[7:5]
}

void test_filter_es9841_global_shift5(void) {
    TEST_ASSERT_EQUAL(FILTER4_GLOBAL_SHIFT5, kDescES9841.filterType);
    TEST_ASSERT_EQUAL_HEX8(0x4A, kDescES9841.regFilter);
    TEST_ASSERT_EQUAL_HEX8(0xFF, kDescES9841.regFilterCh2);
    TEST_ASSERT_EQUAL_HEX8(0xFF, kDescES9841.regFilterCh3);
    TEST_ASSERT_EQUAL_HEX8(0xFF, kDescES9841.regFilterCh4);
    TEST_ASSERT_EQUAL_HEX8(0xE0, kDescES9841.filterMask);
}

// ==========================================================================
// Section 7b: Filter FILTER4_PER_CH_SHIFT2 (ES9842PRO, ES9840)
// ==========================================================================

void test_filter_es9842pro_per_ch_shift2(void) {
    TEST_ASSERT_EQUAL(FILTER4_PER_CH_SHIFT2, kDescES9842PRO.filterType);
    TEST_ASSERT_EQUAL_HEX8(0x71, kDescES9842PRO.regFilter);
    TEST_ASSERT_EQUAL_HEX8(0x82, kDescES9842PRO.regFilterCh2);
    TEST_ASSERT_EQUAL_HEX8(0x93, kDescES9842PRO.regFilterCh3);
    TEST_ASSERT_EQUAL_HEX8(0xA4, kDescES9842PRO.regFilterCh4);
    TEST_ASSERT_EQUAL_HEX8(0x1C, kDescES9842PRO.filterMask);  // bits[4:2]
}

void test_filter_es9840_per_ch_shift2_matches_es9842pro(void) {
    TEST_ASSERT_EQUAL(FILTER4_PER_CH_SHIFT2, kDescES9840.filterType);
    TEST_ASSERT_EQUAL_HEX8(kDescES9842PRO.regFilter,    kDescES9840.regFilter);
    TEST_ASSERT_EQUAL_HEX8(kDescES9842PRO.regFilterCh2, kDescES9840.regFilterCh2);
    TEST_ASSERT_EQUAL_HEX8(kDescES9842PRO.regFilterCh3, kDescES9840.regFilterCh3);
    TEST_ASSERT_EQUAL_HEX8(kDescES9842PRO.regFilterCh4, kDescES9840.regFilterCh4);
    TEST_ASSERT_EQUAL_HEX8(kDescES9842PRO.filterMask,   kDescES9840.filterMask);
}

// ==========================================================================
// Section 8: Source names for AudioInputSource pairs
// ==========================================================================

void test_source_names_es9843pro(void) {
    TEST_ASSERT_EQUAL_STRING("ES9843PRO CH1/2", kDescES9843PRO.sourceNameA);
    TEST_ASSERT_EQUAL_STRING("ES9843PRO CH3/4", kDescES9843PRO.sourceNameB);
}

void test_source_names_es9842pro(void) {
    TEST_ASSERT_EQUAL_STRING("ES9842PRO CH1/2", kDescES9842PRO.sourceNameA);
    TEST_ASSERT_EQUAL_STRING("ES9842PRO CH3/4", kDescES9842PRO.sourceNameB);
}

void test_source_names_es9841(void) {
    TEST_ASSERT_EQUAL_STRING("ES9841 CH1/2", kDescES9841.sourceNameA);
    TEST_ASSERT_EQUAL_STRING("ES9841 CH3/4", kDescES9841.sourceNameB);
}

void test_source_names_es9840(void) {
    TEST_ASSERT_EQUAL_STRING("ES9840 CH1/2", kDescES9840.sourceNameA);
    TEST_ASSERT_EQUAL_STRING("ES9840 CH3/4", kDescES9840.sourceNameB);
}

void test_all_source_names_non_null(void) {
    for (int i = 0; i < kDescCount; i++) {
        TEST_ASSERT_NOT_NULL(kAllDescs[i]->sourceNameA);
        TEST_ASSERT_NOT_NULL(kAllDescs[i]->sourceNameB);
    }
}

// ==========================================================================
// Section 9: Init sequence — lengths and register/value spot checks
// ==========================================================================

void test_init_seq_es9843pro_length(void) {
    // soft_reset, delay, enable_4ch, output_format, dc_block, asp_control, asp_bypass = 7
    TEST_ASSERT_EQUAL(7, kDescES9843PRO.initSeqLen);
    TEST_ASSERT_NOT_NULL(kDescES9843PRO.initSeq);
}

void test_init_seq_es9842pro_length(void) {
    // soft_reset, delay, output_tdm = 3
    TEST_ASSERT_EQUAL(3, kDescES9842PRO.initSeqLen);
    TEST_ASSERT_NOT_NULL(kDescES9842PRO.initSeq);
}

void test_init_seq_es9841_length(void) {
    // soft_reset, delay, output_tdm = 3
    TEST_ASSERT_EQUAL(3, kDescES9841.initSeqLen);
    TEST_ASSERT_NOT_NULL(kDescES9841.initSeq);
}

void test_init_seq_es9840_length(void) {
    // Same structure as ES9842PRO: soft_reset, delay, output_tdm = 3
    TEST_ASSERT_EQUAL(3, kDescES9840.initSeqLen);
    TEST_ASSERT_NOT_NULL(kDescES9840.initSeq);
}

void test_init_seq_es9843pro_first_entry_soft_reset(void) {
    const EssAdc4chRegWrite* seq = kDescES9843PRO.initSeq;
    TEST_ASSERT_EQUAL_HEX8(0x00, seq[0].reg);   // REG_SYS_CONFIG
    TEST_ASSERT_EQUAL_HEX8(0xA0, seq[0].val);   // SOFT_RESET_CMD
}

void test_init_seq_es9843pro_has_delay_sentinel(void) {
    const EssAdc4chRegWrite* seq = kDescES9843PRO.initSeq;
    TEST_ASSERT_EQUAL_HEX8(0xFF, seq[1].reg);   // DELAY_SENTINEL
    TEST_ASSERT_EQUAL(1,          seq[1].val);   // 1 * 5ms = 5ms
}

void test_init_seq_es9842pro_first_entry_soft_reset(void) {
    const EssAdc4chRegWrite* seq = kDescES9842PRO.initSeq;
    TEST_ASSERT_EQUAL_HEX8(0x00, seq[0].reg);   // REG_SYS_CONFIG
    TEST_ASSERT_EQUAL_HEX8(0x80, seq[0].val);   // SOFT_RESET_CMD
}

void test_init_seq_es9842pro_third_entry_enables_tdm(void) {
    const EssAdc4chRegWrite* seq = kDescES9842PRO.initSeq;
    TEST_ASSERT_EQUAL_HEX8(0x00, seq[2].reg);   // REG_SYS_CONFIG
    TEST_ASSERT_EQUAL_HEX8(0x40, seq[2].val);   // OUTPUT_TDM
}

void test_init_seq_es9841_first_entry_soft_reset(void) {
    const EssAdc4chRegWrite* seq = kDescES9841.initSeq;
    TEST_ASSERT_EQUAL_HEX8(0x00, seq[0].reg);   // REG_SYS_CONFIG
    TEST_ASSERT_EQUAL_HEX8(0x80, seq[0].val);   // SOFT_RESET_CMD
}

void test_init_seq_all_have_delay_sentinel(void) {
    // All 4 chips have a delay sentinel as their second sequence entry
    for (int i = 0; i < kDescCount; i++) {
        const EssAdc4chRegWrite* seq = kAllDescs[i]->initSeq;
        TEST_ASSERT_EQUAL_HEX8(0xFF, seq[1].reg);  // DELAY_SENTINEL
    }
}

// ==========================================================================
// Section 10: Deinit sequence — lengths and spot checks
// ==========================================================================

void test_deinit_seq_es9843pro_disables_adc(void) {
    TEST_ASSERT_EQUAL(1, kDescES9843PRO.deinitSeqLen);
    TEST_ASSERT_NOT_NULL(kDescES9843PRO.deinitSeq);
    TEST_ASSERT_EQUAL_HEX8(0x00, kDescES9843PRO.deinitSeq[0].reg);  // REG_SYS_CONFIG
    TEST_ASSERT_EQUAL_HEX8(0x00, kDescES9843PRO.deinitSeq[0].val);  // disable all
}

void test_deinit_seq_es9842pro_disables_tdm(void) {
    TEST_ASSERT_EQUAL(1, kDescES9842PRO.deinitSeqLen);
    TEST_ASSERT_NOT_NULL(kDescES9842PRO.deinitSeq);
    TEST_ASSERT_EQUAL_HEX8(0x00, kDescES9842PRO.deinitSeq[0].reg);
    TEST_ASSERT_EQUAL_HEX8(0x00, kDescES9842PRO.deinitSeq[0].val);
}

void test_deinit_seq_es9841_disables_tdm(void) {
    TEST_ASSERT_EQUAL(1, kDescES9841.deinitSeqLen);
    TEST_ASSERT_NOT_NULL(kDescES9841.deinitSeq);
    TEST_ASSERT_EQUAL_HEX8(0x00, kDescES9841.deinitSeq[0].reg);
    TEST_ASSERT_EQUAL_HEX8(0x00, kDescES9841.deinitSeq[0].val);
}

void test_deinit_seq_es9840_disables_tdm(void) {
    TEST_ASSERT_EQUAL(1, kDescES9840.deinitSeqLen);
    TEST_ASSERT_NOT_NULL(kDescES9840.deinitSeq);
    TEST_ASSERT_EQUAL_HEX8(0x00, kDescES9840.deinitSeq[0].reg);
    TEST_ASSERT_EQUAL_HEX8(0x00, kDescES9840.deinitSeq[0].val);
}

// ==========================================================================
// Section 11: ES9840 register map matches ES9842PRO (same silicon family)
// ==========================================================================

void test_es9840_vol_regs_match_es9842pro(void) {
    TEST_ASSERT_EQUAL_HEX8(kDescES9842PRO.regVolCh1, kDescES9840.regVolCh1);
}

void test_es9840_gain_regs_match_es9842pro(void) {
    TEST_ASSERT_EQUAL_HEX8(kDescES9842PRO.regGainPair1, kDescES9840.regGainPair1);
    TEST_ASSERT_EQUAL_HEX8(kDescES9842PRO.regGainPair2, kDescES9840.regGainPair2);
    TEST_ASSERT_EQUAL(kDescES9842PRO.gainMax,  kDescES9840.gainMax);
    TEST_ASSERT_EQUAL(kDescES9842PRO.gainType, kDescES9840.gainType);
}

void test_es9840_soft_reset_matches_es9842pro(void) {
    TEST_ASSERT_EQUAL_HEX8(kDescES9842PRO.softResetVal,  kDescES9840.softResetVal);
    TEST_ASSERT_EQUAL_HEX8(kDescES9842PRO.tdmEnableVal,  kDescES9840.tdmEnableVal);
    TEST_ASSERT_EQUAL_HEX8(kDescES9842PRO.regSysConfig,  kDescES9840.regSysConfig);
}

// Distinct chip IDs even though register maps match
void test_es9840_and_es9842pro_have_different_chip_ids(void) {
    TEST_ASSERT_NOT_EQUAL(kDescES9842PRO.chipId, kDescES9840.chipId);
}

// ==========================================================================
// Section 12: Chip ID register address (all at 0xE1)
// ==========================================================================

void test_chip_id_reg_is_0xe1_for_all(void) {
    for (int i = 0; i < kDescCount; i++) {
        TEST_ASSERT_EQUAL_HEX8(0xE1, kAllDescs[i]->regChipId);
    }
}

void test_sys_config_reg_is_0x00_for_all(void) {
    for (int i = 0; i < kDescCount; i++) {
        TEST_ASSERT_EQUAL_HEX8(0x00, kAllDescs[i]->regSysConfig);
    }
}

// ==========================================================================
// Section 13: Log prefix format
// ==========================================================================

void test_log_prefix_es9843pro(void) {
    TEST_ASSERT_EQUAL_STRING("[HAL:ES9843PRO]", kDescES9843PRO.logPrefix);
}

void test_log_prefix_es9842pro(void) {
    TEST_ASSERT_EQUAL_STRING("[HAL:ES9842PRO]", kDescES9842PRO.logPrefix);
}

void test_log_prefix_es9841(void) {
    TEST_ASSERT_EQUAL_STRING("[HAL:ES9841]", kDescES9841.logPrefix);
}

void test_log_prefix_es9840(void) {
    TEST_ASSERT_EQUAL_STRING("[HAL:ES9840]", kDescES9840.logPrefix);
}

// ==========================================================================
// main
// ==========================================================================

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Section 1: Identity
    RUN_TEST(test_desc_es9843pro_identity);
    RUN_TEST(test_desc_es9842pro_identity);
    RUN_TEST(test_desc_es9841_identity);
    RUN_TEST(test_desc_es9840_identity);

    // Section 2: Capabilities
    RUN_TEST(test_caps_es9843pro);
    RUN_TEST(test_caps_es9842pro);
    RUN_TEST(test_caps_es9841);
    RUN_TEST(test_caps_es9840);

    // Section 3: Sample rates
    RUN_TEST(test_rates_all_support_44k1_to_192k);
    RUN_TEST(test_rates_all_have_4_supported_rates);
    RUN_TEST(test_rates_none_support_384k_or_higher);

    // Section 4: Volume types and registers
    RUN_TEST(test_vol_es9843pro_type_and_regs);
    RUN_TEST(test_vol_es9841_type_inverted);
    RUN_TEST(test_vol_es9842pro_type_and_ch1_reg);
    RUN_TEST(test_vol_es9840_type_and_ch1_reg);

    // Section 5: Gain encoding
    RUN_TEST(test_gain_es9842pro_type_and_params);
    RUN_TEST(test_gain_es9840_type_and_params);
    RUN_TEST(test_gain_es9843pro_type_pack_3_5);
    RUN_TEST(test_gain_es9841_type_pack_0_4);

    // Section 6: HPF
    RUN_TEST(test_hpf_es9843pro_msb_reg_single);
    RUN_TEST(test_hpf_es9842pro_per_channel_regs);
    RUN_TEST(test_hpf_es9841_per_channel_regs);
    RUN_TEST(test_hpf_es9840_per_channel_regs_match_es9842pro);

    // Section 7: Filter
    RUN_TEST(test_filter_es9843pro_global_shift5);
    RUN_TEST(test_filter_es9841_global_shift5);
    RUN_TEST(test_filter_es9842pro_per_ch_shift2);
    RUN_TEST(test_filter_es9840_per_ch_shift2_matches_es9842pro);

    // Section 8: Source names
    RUN_TEST(test_source_names_es9843pro);
    RUN_TEST(test_source_names_es9842pro);
    RUN_TEST(test_source_names_es9841);
    RUN_TEST(test_source_names_es9840);
    RUN_TEST(test_all_source_names_non_null);

    // Section 9: Init sequences
    RUN_TEST(test_init_seq_es9843pro_length);
    RUN_TEST(test_init_seq_es9842pro_length);
    RUN_TEST(test_init_seq_es9841_length);
    RUN_TEST(test_init_seq_es9840_length);
    RUN_TEST(test_init_seq_es9843pro_first_entry_soft_reset);
    RUN_TEST(test_init_seq_es9843pro_has_delay_sentinel);
    RUN_TEST(test_init_seq_es9842pro_first_entry_soft_reset);
    RUN_TEST(test_init_seq_es9842pro_third_entry_enables_tdm);
    RUN_TEST(test_init_seq_es9841_first_entry_soft_reset);
    RUN_TEST(test_init_seq_all_have_delay_sentinel);

    // Section 10: Deinit sequences
    RUN_TEST(test_deinit_seq_es9843pro_disables_adc);
    RUN_TEST(test_deinit_seq_es9842pro_disables_tdm);
    RUN_TEST(test_deinit_seq_es9841_disables_tdm);
    RUN_TEST(test_deinit_seq_es9840_disables_tdm);

    // Section 11: ES9840 shares ES9842PRO register map
    RUN_TEST(test_es9840_vol_regs_match_es9842pro);
    RUN_TEST(test_es9840_gain_regs_match_es9842pro);
    RUN_TEST(test_es9840_soft_reset_matches_es9842pro);
    RUN_TEST(test_es9840_and_es9842pro_have_different_chip_ids);

    // Section 12: Register addresses
    RUN_TEST(test_chip_id_reg_is_0xe1_for_all);
    RUN_TEST(test_sys_config_reg_is_0x00_for_all);

    // Section 13: Log prefixes
    RUN_TEST(test_log_prefix_es9843pro);
    RUN_TEST(test_log_prefix_es9842pro);
    RUN_TEST(test_log_prefix_es9841);
    RUN_TEST(test_log_prefix_es9840);

    return UNITY_END();
}
