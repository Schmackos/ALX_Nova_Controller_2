// test_hal_ess_adc_2ch.cpp
// Regression tests for the generic HalEssAdc2ch descriptor-driven driver.
//
// Verifies that the single generic driver, parameterized by per-chip descriptor
// tables, produces identical register-level behavior to the 5 original drivers:
//   ES9822PRO, ES9826, ES9823PRO/ES9823MPRO, ES9821, ES9820
//
// Tests validate descriptor field values and behavioral contracts.
//
// Section layout:
//   1.  Descriptor identity (compatible, chipName, chipId)
//   2.  Capability flags per descriptor
//   3.  Sample rate masks per descriptor
//   4.  Volume register addresses (16-bit LSB pairs)
//   5.  Gain encoding — GAIN_2BIT_6DB (ES9822PRO, ES9820)
//   6.  Gain encoding — GAIN_3BIT_6DB (ES9823PRO)
//   7.  Gain encoding — GAIN_NIBBLE_3DB (ES9826)
//   8.  Gain encoding — GAIN_NONE (ES9821)
//   9.  HPF type — HPF_BIT_IN_DATAPATH (ES9822PRO, ES9820)
//  10.  HPF type — HPF_NONE (ES9826, ES9823PRO, ES9821)
//  11.  Filter type — FILTER_SHIFT2_CH_PAIR (ES9822PRO, ES9826, ES9821, ES9820)
//  12.  Filter type — FILTER_SHIFT5_SINGLE (ES9823PRO)
//  13.  Variant detection fields (ES9823PRO vs ES9823MPRO)
//  14.  Init sequence — sentinel and sequence lengths
//  15.  Init sequence — register/value spot checks
//  16.  Deinit sequence lengths and spot checks
//  17.  I2C address (all 5 chips share 0x40)
//  18.  Log prefix format

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
#include "../../src/diag_journal.cpp"
#include "../../src/hal/hal_i2c_bus.cpp"
#include "../../src/hal/hal_ess_sabre_adc_base.cpp"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_ess_adc_2ch.cpp"
#include "../../src/hal/hal_ess_adc_2ch.h"

// =====================================================================
// All 5 descriptors for parametric iteration
// =====================================================================
static const EssAdc2chDescriptor* kAllDescs[] = {
    &kDescES9822PRO, &kDescES9826, &kDescES9823PRO, &kDescES9821, &kDescES9820
};
static const int kDescCount = 5;

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

void test_desc_es9822pro_identity(void) {
    TEST_ASSERT_EQUAL_STRING("ess,es9822pro", kDescES9822PRO.compatible);
    TEST_ASSERT_EQUAL_STRING("ES9822PRO",     kDescES9822PRO.chipName);
    TEST_ASSERT_EQUAL_HEX8(0x81,              kDescES9822PRO.chipId);
}

void test_desc_es9826_identity(void) {
    TEST_ASSERT_EQUAL_STRING("ess,es9826", kDescES9826.compatible);
    TEST_ASSERT_EQUAL_STRING("ES9826",     kDescES9826.chipName);
    TEST_ASSERT_EQUAL_HEX8(0x8A,           kDescES9826.chipId);
}

void test_desc_es9823pro_identity(void) {
    TEST_ASSERT_EQUAL_STRING("ess,es9823pro", kDescES9823PRO.compatible);
    TEST_ASSERT_EQUAL_STRING("ES9823PRO",     kDescES9823PRO.chipName);
    TEST_ASSERT_EQUAL_HEX8(0x8D,             kDescES9823PRO.chipId);
}

void test_desc_es9821_identity(void) {
    TEST_ASSERT_EQUAL_STRING("ess,es9821", kDescES9821.compatible);
    TEST_ASSERT_EQUAL_STRING("ES9821",     kDescES9821.chipName);
    TEST_ASSERT_EQUAL_HEX8(0x88,           kDescES9821.chipId);
}

void test_desc_es9820_identity(void) {
    TEST_ASSERT_EQUAL_STRING("ess,es9820", kDescES9820.compatible);
    TEST_ASSERT_EQUAL_STRING("ES9820",     kDescES9820.chipName);
    TEST_ASSERT_EQUAL_HEX8(0x84,           kDescES9820.chipId);
}

// All 5 use I2C address 0x40
void test_desc_all_i2c_addr_0x40(void) {
    // The chip ID register is at 0xE1 for all ESS ADCs
    for (int i = 0; i < kDescCount; i++) {
        TEST_ASSERT_EQUAL_HEX8(0xE1, kAllDescs[i]->regChipId);
    }
}

// ==========================================================================
// Section 2: Capability flags per descriptor
// ==========================================================================

void test_caps_es9822pro(void) {
    uint32_t c = kDescES9822PRO.capabilities;
    TEST_ASSERT_TRUE(c & HAL_CAP_ADC_PATH);
    TEST_ASSERT_TRUE(c & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(c & HAL_CAP_PGA_CONTROL);
    TEST_ASSERT_TRUE(c & HAL_CAP_HPF_CONTROL);
    TEST_ASSERT_FALSE(c & HAL_CAP_DAC_PATH);
}

void test_caps_es9826(void) {
    uint32_t c = kDescES9826.capabilities;
    TEST_ASSERT_TRUE(c & HAL_CAP_ADC_PATH);
    TEST_ASSERT_TRUE(c & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(c & HAL_CAP_PGA_CONTROL);
    // ES9826 has no HPF
    TEST_ASSERT_FALSE(c & HAL_CAP_HPF_CONTROL);
    TEST_ASSERT_FALSE(c & HAL_CAP_DAC_PATH);
}

void test_caps_es9823pro(void) {
    uint32_t c = kDescES9823PRO.capabilities;
    TEST_ASSERT_TRUE(c & HAL_CAP_ADC_PATH);
    TEST_ASSERT_TRUE(c & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(c & HAL_CAP_PGA_CONTROL);
    TEST_ASSERT_FALSE(c & HAL_CAP_HPF_CONTROL);
    TEST_ASSERT_FALSE(c & HAL_CAP_DAC_PATH);
}

void test_caps_es9821(void) {
    uint32_t c = kDescES9821.capabilities;
    TEST_ASSERT_TRUE(c & HAL_CAP_ADC_PATH);
    TEST_ASSERT_TRUE(c & HAL_CAP_HW_VOLUME);
    // ES9821 has no PGA
    TEST_ASSERT_FALSE(c & HAL_CAP_PGA_CONTROL);
    TEST_ASSERT_FALSE(c & HAL_CAP_HPF_CONTROL);
    TEST_ASSERT_FALSE(c & HAL_CAP_DAC_PATH);
}

void test_caps_es9820(void) {
    uint32_t c = kDescES9820.capabilities;
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

void test_rates_es9822pro_no_384k(void) {
    TEST_ASSERT_FALSE(kDescES9822PRO.sampleRateMask & HAL_RATE_384K);
}

// ==========================================================================
// Section 4: Volume register addresses (16-bit LSB pairs)
// ==========================================================================

void test_vol_es9822pro_regs(void) {
    TEST_ASSERT_EQUAL_HEX8(0x6D, kDescES9822PRO.regVolLsbCh1);
    TEST_ASSERT_EQUAL_HEX8(0x7E, kDescES9822PRO.regVolLsbCh2);
}

void test_vol_es9826_regs(void) {
    TEST_ASSERT_EQUAL_HEX8(0x2D, kDescES9826.regVolLsbCh1);
    TEST_ASSERT_EQUAL_HEX8(0x2F, kDescES9826.regVolLsbCh2);
}

void test_vol_es9823pro_regs(void) {
    TEST_ASSERT_EQUAL_HEX8(0x51, kDescES9823PRO.regVolLsbCh1);
    TEST_ASSERT_EQUAL_HEX8(0x53, kDescES9823PRO.regVolLsbCh2);
}

void test_vol_es9821_regs(void) {
    TEST_ASSERT_EQUAL_HEX8(0x32, kDescES9821.regVolLsbCh1);
    TEST_ASSERT_EQUAL_HEX8(0x34, kDescES9821.regVolLsbCh2);
}

void test_vol_es9820_regs(void) {
    // ES9820 uses same layout as ES9822PRO
    TEST_ASSERT_EQUAL_HEX8(0x6D, kDescES9820.regVolLsbCh1);
    TEST_ASSERT_EQUAL_HEX8(0x7E, kDescES9820.regVolLsbCh2);
}

// ==========================================================================
// Section 5: Gain encoding — GAIN_2BIT_6DB (ES9822PRO, ES9820)
// ==========================================================================

void test_gain_es9822pro_type_and_params(void) {
    TEST_ASSERT_EQUAL(GAIN_2BIT_6DB, kDescES9822PRO.gainType);
    TEST_ASSERT_EQUAL(18,            kDescES9822PRO.gainMax);
    TEST_ASSERT_EQUAL(6,             kDescES9822PRO.gainStep);
    TEST_ASSERT_EQUAL_HEX8(0x70,     kDescES9822PRO.regGainCh1);
    TEST_ASSERT_EQUAL_HEX8(0x81,     kDescES9822PRO.regGainCh2);
}

void test_gain_es9820_type_and_params(void) {
    TEST_ASSERT_EQUAL(GAIN_2BIT_6DB, kDescES9820.gainType);
    TEST_ASSERT_EQUAL(18,            kDescES9820.gainMax);
    TEST_ASSERT_EQUAL(6,             kDescES9820.gainStep);
    TEST_ASSERT_EQUAL_HEX8(0x70,     kDescES9820.regGainCh1);
    TEST_ASSERT_EQUAL_HEX8(0x81,     kDescES9820.regGainCh2);
}

// ==========================================================================
// Section 6: Gain encoding — GAIN_3BIT_6DB (ES9823PRO)
// ==========================================================================

void test_gain_es9823pro_type_and_params(void) {
    TEST_ASSERT_EQUAL(GAIN_3BIT_6DB, kDescES9823PRO.gainType);
    TEST_ASSERT_EQUAL(42,            kDescES9823PRO.gainMax);
    TEST_ASSERT_EQUAL(6,             kDescES9823PRO.gainStep);
    // Single register holds both channels packed
    TEST_ASSERT_EQUAL_HEX8(0x55, kDescES9823PRO.regGainCh1);
}

// ==========================================================================
// Section 7: Gain encoding — GAIN_NIBBLE_3DB (ES9826)
// ==========================================================================

void test_gain_es9826_type_and_params(void) {
    TEST_ASSERT_EQUAL(GAIN_NIBBLE_3DB, kDescES9826.gainType);
    TEST_ASSERT_EQUAL(30,              kDescES9826.gainMax);
    TEST_ASSERT_EQUAL(3,               kDescES9826.gainStep);
    TEST_ASSERT_EQUAL(10,              kDescES9826.gainMax_nibble);
    // Single register for both channels (nibble packed)
    TEST_ASSERT_EQUAL_HEX8(0x44, kDescES9826.regGainCh1);
    // regGainCh2 = 0xFF signals single-register encoding
    TEST_ASSERT_EQUAL_HEX8(0xFF, kDescES9826.regGainCh2);
}

// ==========================================================================
// Section 8: Gain encoding — GAIN_NONE (ES9821)
// ==========================================================================

void test_gain_es9821_none(void) {
    TEST_ASSERT_EQUAL(GAIN_NONE, kDescES9821.gainType);
    TEST_ASSERT_EQUAL(0,         kDescES9821.gainMax);
    // Gain registers should be 0xFF (no register)
    TEST_ASSERT_EQUAL_HEX8(0xFF, kDescES9821.regGainCh1);
    TEST_ASSERT_EQUAL_HEX8(0xFF, kDescES9821.regGainCh2);
}

// ==========================================================================
// Section 9: HPF type — HPF_BIT_IN_DATAPATH (ES9822PRO, ES9820)
// ==========================================================================

void test_hpf_es9822pro_datapath_bit(void) {
    TEST_ASSERT_EQUAL(HPF_BIT_IN_DATAPATH, kDescES9822PRO.hpfType);
    TEST_ASSERT_EQUAL_HEX8(0x65,           kDescES9822PRO.regHpfCh1);
    TEST_ASSERT_EQUAL_HEX8(0x76,           kDescES9822PRO.regHpfCh2);
    TEST_ASSERT_EQUAL_HEX8(0x04,           kDescES9822PRO.hpfEnableBit);
}

void test_hpf_es9820_datapath_bit(void) {
    TEST_ASSERT_EQUAL(HPF_BIT_IN_DATAPATH, kDescES9820.hpfType);
    TEST_ASSERT_EQUAL_HEX8(0x65,           kDescES9820.regHpfCh1);
    TEST_ASSERT_EQUAL_HEX8(0x76,           kDescES9820.regHpfCh2);
    TEST_ASSERT_EQUAL_HEX8(0x04,           kDescES9820.hpfEnableBit);
}

// ==========================================================================
// Section 10: HPF type — HPF_NONE (ES9826, ES9823PRO, ES9821)
// ==========================================================================

void test_hpf_es9826_none(void) {
    TEST_ASSERT_EQUAL(HPF_NONE, kDescES9826.hpfType);
    TEST_ASSERT_EQUAL_HEX8(0xFF, kDescES9826.regHpfCh1);
    TEST_ASSERT_EQUAL_HEX8(0xFF, kDescES9826.regHpfCh2);
}

void test_hpf_es9823pro_none(void) {
    TEST_ASSERT_EQUAL(HPF_NONE, kDescES9823PRO.hpfType);
    TEST_ASSERT_EQUAL_HEX8(0xFF, kDescES9823PRO.regHpfCh1);
    TEST_ASSERT_EQUAL_HEX8(0xFF, kDescES9823PRO.regHpfCh2);
}

void test_hpf_es9821_none(void) {
    TEST_ASSERT_EQUAL(HPF_NONE, kDescES9821.hpfType);
    TEST_ASSERT_EQUAL_HEX8(0xFF, kDescES9821.regHpfCh1);
    TEST_ASSERT_EQUAL_HEX8(0xFF, kDescES9821.regHpfCh2);
}

// ==========================================================================
// Section 11: Filter type — FILTER_SHIFT2_CH_PAIR (ES9822PRO, ES9826, ES9821, ES9820)
// ==========================================================================

void test_filter_es9822pro_per_ch_shift2(void) {
    TEST_ASSERT_EQUAL(FILTER_SHIFT2_CH_PAIR, kDescES9822PRO.filterType);
    TEST_ASSERT_EQUAL_HEX8(0x71, kDescES9822PRO.regFilterCh1);
    TEST_ASSERT_EQUAL_HEX8(0x82, kDescES9822PRO.regFilterCh2);
}

void test_filter_es9826_single_reg(void) {
    TEST_ASSERT_EQUAL(FILTER_SHIFT2_CH_PAIR, kDescES9826.filterType);
    TEST_ASSERT_EQUAL_HEX8(0x3B, kDescES9826.regFilterCh1);
    // ES9826 uses a single filter register — regFilterCh2 = 0xFF
    TEST_ASSERT_EQUAL_HEX8(0xFF, kDescES9826.regFilterCh2);
}

void test_filter_es9821_single_reg(void) {
    TEST_ASSERT_EQUAL(FILTER_SHIFT2_CH_PAIR, kDescES9821.filterType);
    TEST_ASSERT_EQUAL_HEX8(0x40, kDescES9821.regFilterCh1);
}

void test_filter_es9820_per_ch_shift2(void) {
    TEST_ASSERT_EQUAL(FILTER_SHIFT2_CH_PAIR, kDescES9820.filterType);
    TEST_ASSERT_EQUAL_HEX8(0x71, kDescES9820.regFilterCh1);
    TEST_ASSERT_EQUAL_HEX8(0x82, kDescES9820.regFilterCh2);
}

// ==========================================================================
// Section 12: Filter type — FILTER_SHIFT5_SINGLE (ES9823PRO)
// ==========================================================================

void test_filter_es9823pro_global_shift5(void) {
    TEST_ASSERT_EQUAL(FILTER_SHIFT5_SINGLE, kDescES9823PRO.filterType);
    TEST_ASSERT_EQUAL_HEX8(0x4A, kDescES9823PRO.regFilterCh1);
    // Single register — regFilterCh2 = 0xFF
    TEST_ASSERT_EQUAL_HEX8(0xFF, kDescES9823PRO.regFilterCh2);
}

// ==========================================================================
// Section 13: Variant detection fields (ES9823PRO vs ES9823MPRO)
// ==========================================================================

void test_variant_es9823pro_has_alt_fields(void) {
    // ES9823PRO descriptor covers both PRO and MPRO via altChipId
    TEST_ASSERT_EQUAL_HEX8(0x8C, kDescES9823PRO.altChipId);
    TEST_ASSERT_NOT_NULL(kDescES9823PRO.altCompatible);
    TEST_ASSERT_NOT_NULL(kDescES9823PRO.altChipName);
    TEST_ASSERT_EQUAL_STRING("ess,es9823mpro", kDescES9823PRO.altCompatible);
    TEST_ASSERT_EQUAL_STRING("ES9823MPRO",     kDescES9823PRO.altChipName);
}

void test_variant_others_have_no_alt(void) {
    // Only ES9823PRO has dual-variant support
    TEST_ASSERT_EQUAL(0x00, kDescES9822PRO.altChipId);
    TEST_ASSERT_NULL(kDescES9822PRO.altCompatible);
    TEST_ASSERT_EQUAL(0x00, kDescES9826.altChipId);
    TEST_ASSERT_NULL(kDescES9826.altCompatible);
    TEST_ASSERT_EQUAL(0x00, kDescES9821.altChipId);
    TEST_ASSERT_NULL(kDescES9821.altCompatible);
    TEST_ASSERT_EQUAL(0x00, kDescES9820.altChipId);
    TEST_ASSERT_NULL(kDescES9820.altCompatible);
}

// ==========================================================================
// Section 14: Init sequence — sentinel and sequence lengths
// ==========================================================================

void test_init_seq_es9822pro_length(void) {
    // ES9822PRO has 10 init entries (reset, delay, sync_clk, ch1a_cfg1, ch1a_cfg2, ch2a_cfg1, ch2a_cfg2, common_mode, clk_config, sys_config)
    TEST_ASSERT_EQUAL(10, kDescES9822PRO.initSeqLen);
    TEST_ASSERT_NOT_NULL(kDescES9822PRO.initSeq);
}

void test_init_seq_es9826_length(void) {
    // ES9826 has 2 init entries (reset, delay)
    TEST_ASSERT_EQUAL(2, kDescES9826.initSeqLen);
    TEST_ASSERT_NOT_NULL(kDescES9826.initSeq);
}

void test_init_seq_es9823pro_length(void) {
    // ES9823PRO has 6 init entries
    TEST_ASSERT_EQUAL(6, kDescES9823PRO.initSeqLen);
    TEST_ASSERT_NOT_NULL(kDescES9823PRO.initSeq);
}

void test_init_seq_es9821_length(void) {
    // ES9821 has 2 init entries (reset, delay)
    TEST_ASSERT_EQUAL(2, kDescES9821.initSeqLen);
    TEST_ASSERT_NOT_NULL(kDescES9821.initSeq);
}

void test_init_seq_es9820_length(void) {
    // ES9820 has 3 init entries (reset, delay, output mode)
    TEST_ASSERT_EQUAL(3, kDescES9820.initSeqLen);
    TEST_ASSERT_NOT_NULL(kDescES9820.initSeq);
}

// ==========================================================================
// Section 15: Init sequence — register/value spot checks
// ==========================================================================

void test_init_seq_es9822pro_first_entry_is_soft_reset(void) {
    const EssAdc2chRegWrite* seq = kDescES9822PRO.initSeq;
    TEST_ASSERT_EQUAL_HEX8(0x00, seq[0].reg);   // REG_SYS_CONFIG
    TEST_ASSERT_EQUAL_HEX8(0x80, seq[0].val);   // SOFT_RESET_BIT
}

void test_init_seq_es9822pro_second_entry_is_delay_sentinel(void) {
    const EssAdc2chRegWrite* seq = kDescES9822PRO.initSeq;
    TEST_ASSERT_EQUAL_HEX8(0xFF, seq[1].reg);   // DELAY_SENTINEL
    TEST_ASSERT_EQUAL(1,          seq[1].val);   // 1 * 5ms = 5ms
}

void test_init_seq_es9826_first_entry_is_soft_reset(void) {
    const EssAdc2chRegWrite* seq = kDescES9826.initSeq;
    TEST_ASSERT_EQUAL_HEX8(0x00, seq[0].reg);   // REG_SYS_CONFIG
    TEST_ASSERT_EQUAL_HEX8(0x80, seq[0].val);   // SOFT_RESET_BIT
}

void test_init_seq_es9823pro_enables_adc_clocks(void) {
    // Entry index 2 should enable ADC clocks
    const EssAdc2chRegWrite* seq = kDescES9823PRO.initSeq;
    TEST_ASSERT_EQUAL_HEX8(0x01, seq[2].reg);   // REG_ADC_CLOCK_CONFIG1
    TEST_ASSERT_EQUAL_HEX8(0x33, seq[2].val);   // CLOCK_ENABLE_2CH
}

void test_init_seq_es9820_sets_i2s_output(void) {
    // Last entry should set I2S output mode
    const EssAdc2chRegWrite* seq = kDescES9820.initSeq;
    uint8_t last = kDescES9820.initSeqLen - 1;
    TEST_ASSERT_EQUAL_HEX8(0x00, seq[last].reg);  // REG_SYS_CONFIG
    TEST_ASSERT_EQUAL_HEX8(0x00, seq[last].val);  // OUTPUT_I2S
}

// ==========================================================================
// Section 16: Deinit sequence lengths and spot checks
// ==========================================================================

void test_deinit_seq_es9822pro_disables_clocks(void) {
    TEST_ASSERT_EQUAL(1, kDescES9822PRO.deinitSeqLen);
    TEST_ASSERT_NOT_NULL(kDescES9822PRO.deinitSeq);
    // Must disable ADC clocks
    TEST_ASSERT_EQUAL_HEX8(0x01, kDescES9822PRO.deinitSeq[0].reg);  // REG_ADC_CLOCK_CONFIG1
    TEST_ASSERT_EQUAL_HEX8(0x00, kDescES9822PRO.deinitSeq[0].val);
}

void test_deinit_seq_es9826_empty(void) {
    TEST_ASSERT_EQUAL(0, kDescES9826.deinitSeqLen);
}

void test_deinit_seq_es9823pro_disables_clocks(void) {
    TEST_ASSERT_EQUAL(1, kDescES9823PRO.deinitSeqLen);
    TEST_ASSERT_NOT_NULL(kDescES9823PRO.deinitSeq);
    TEST_ASSERT_EQUAL_HEX8(0x01, kDescES9823PRO.deinitSeq[0].reg);  // REG_ADC_CLOCK_CONFIG1
    TEST_ASSERT_EQUAL_HEX8(0x00, kDescES9823PRO.deinitSeq[0].val);
}

void test_deinit_seq_es9821_empty(void) {
    TEST_ASSERT_EQUAL(0, kDescES9821.deinitSeqLen);
}

void test_deinit_seq_es9820_empty(void) {
    TEST_ASSERT_EQUAL(0, kDescES9820.deinitSeqLen);
}

// ==========================================================================
// Section 17: Chip ID / I2C address
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
// Section 18: Log prefix format
// ==========================================================================

void test_log_prefix_es9822pro(void) {
    TEST_ASSERT_EQUAL_STRING("[HAL:ES9822PRO]", kDescES9822PRO.logPrefix);
}

void test_log_prefix_es9826(void) {
    TEST_ASSERT_EQUAL_STRING("[HAL:ES9826]", kDescES9826.logPrefix);
}

void test_log_prefix_es9823pro(void) {
    TEST_ASSERT_EQUAL_STRING("[HAL:ES9823PRO]", kDescES9823PRO.logPrefix);
}

void test_log_prefix_es9821(void) {
    TEST_ASSERT_EQUAL_STRING("[HAL:ES9821]", kDescES9821.logPrefix);
}

void test_log_prefix_es9820(void) {
    TEST_ASSERT_EQUAL_STRING("[HAL:ES9820]", kDescES9820.logPrefix);
}

// ==========================================================================
// Section: Soft reset values
// ==========================================================================

void test_soft_reset_es9822pro(void) {
    TEST_ASSERT_EQUAL_HEX8(0x80, kDescES9822PRO.softResetVal);
}

void test_soft_reset_es9826(void) {
    TEST_ASSERT_EQUAL_HEX8(0x80, kDescES9826.softResetVal);
}

void test_soft_reset_es9823pro(void) {
    TEST_ASSERT_EQUAL_HEX8(0x80, kDescES9823PRO.softResetVal);
}

void test_soft_reset_es9821(void) {
    TEST_ASSERT_EQUAL_HEX8(0x80, kDescES9821.softResetVal);
}

void test_soft_reset_es9820(void) {
    TEST_ASSERT_EQUAL_HEX8(0x80, kDescES9820.softResetVal);
}

// ==========================================================================
// main
// ==========================================================================

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Section 1: Identity
    RUN_TEST(test_desc_es9822pro_identity);
    RUN_TEST(test_desc_es9826_identity);
    RUN_TEST(test_desc_es9823pro_identity);
    RUN_TEST(test_desc_es9821_identity);
    RUN_TEST(test_desc_es9820_identity);
    RUN_TEST(test_desc_all_i2c_addr_0x40);

    // Section 2: Capabilities
    RUN_TEST(test_caps_es9822pro);
    RUN_TEST(test_caps_es9826);
    RUN_TEST(test_caps_es9823pro);
    RUN_TEST(test_caps_es9821);
    RUN_TEST(test_caps_es9820);

    // Section 3: Sample rates
    RUN_TEST(test_rates_all_support_44k1_to_192k);
    RUN_TEST(test_rates_all_have_4_supported_rates);
    RUN_TEST(test_rates_es9822pro_no_384k);

    // Section 4: Volume registers
    RUN_TEST(test_vol_es9822pro_regs);
    RUN_TEST(test_vol_es9826_regs);
    RUN_TEST(test_vol_es9823pro_regs);
    RUN_TEST(test_vol_es9821_regs);
    RUN_TEST(test_vol_es9820_regs);

    // Section 5: Gain GAIN_2BIT_6DB
    RUN_TEST(test_gain_es9822pro_type_and_params);
    RUN_TEST(test_gain_es9820_type_and_params);

    // Section 6: Gain GAIN_3BIT_6DB
    RUN_TEST(test_gain_es9823pro_type_and_params);

    // Section 7: Gain GAIN_NIBBLE_3DB
    RUN_TEST(test_gain_es9826_type_and_params);

    // Section 8: Gain GAIN_NONE
    RUN_TEST(test_gain_es9821_none);

    // Section 9: HPF_BIT_IN_DATAPATH
    RUN_TEST(test_hpf_es9822pro_datapath_bit);
    RUN_TEST(test_hpf_es9820_datapath_bit);

    // Section 10: HPF_NONE
    RUN_TEST(test_hpf_es9826_none);
    RUN_TEST(test_hpf_es9823pro_none);
    RUN_TEST(test_hpf_es9821_none);

    // Section 11: FILTER_SHIFT2_CH_PAIR
    RUN_TEST(test_filter_es9822pro_per_ch_shift2);
    RUN_TEST(test_filter_es9826_single_reg);
    RUN_TEST(test_filter_es9821_single_reg);
    RUN_TEST(test_filter_es9820_per_ch_shift2);

    // Section 12: FILTER_SHIFT5_SINGLE
    RUN_TEST(test_filter_es9823pro_global_shift5);

    // Section 13: Variant detection
    RUN_TEST(test_variant_es9823pro_has_alt_fields);
    RUN_TEST(test_variant_others_have_no_alt);

    // Section 14: Init sequence lengths
    RUN_TEST(test_init_seq_es9822pro_length);
    RUN_TEST(test_init_seq_es9826_length);
    RUN_TEST(test_init_seq_es9823pro_length);
    RUN_TEST(test_init_seq_es9821_length);
    RUN_TEST(test_init_seq_es9820_length);

    // Section 15: Init sequence spot checks
    RUN_TEST(test_init_seq_es9822pro_first_entry_is_soft_reset);
    RUN_TEST(test_init_seq_es9822pro_second_entry_is_delay_sentinel);
    RUN_TEST(test_init_seq_es9826_first_entry_is_soft_reset);
    RUN_TEST(test_init_seq_es9823pro_enables_adc_clocks);
    RUN_TEST(test_init_seq_es9820_sets_i2s_output);

    // Section 16: Deinit sequences
    RUN_TEST(test_deinit_seq_es9822pro_disables_clocks);
    RUN_TEST(test_deinit_seq_es9826_empty);
    RUN_TEST(test_deinit_seq_es9823pro_disables_clocks);
    RUN_TEST(test_deinit_seq_es9821_empty);
    RUN_TEST(test_deinit_seq_es9820_empty);

    // Section 17: Register addresses
    RUN_TEST(test_chip_id_reg_is_0xe1_for_all);
    RUN_TEST(test_sys_config_reg_is_0x00_for_all);

    // Section 18: Log prefixes
    RUN_TEST(test_log_prefix_es9822pro);
    RUN_TEST(test_log_prefix_es9826);
    RUN_TEST(test_log_prefix_es9823pro);
    RUN_TEST(test_log_prefix_es9821);
    RUN_TEST(test_log_prefix_es9820);

    // Soft reset values
    RUN_TEST(test_soft_reset_es9822pro);
    RUN_TEST(test_soft_reset_es9826);
    RUN_TEST(test_soft_reset_es9823pro);
    RUN_TEST(test_soft_reset_es9821);
    RUN_TEST(test_soft_reset_es9820);

    return UNITY_END();
}
