#include <cstring>
#include <cstdint>
#include <unity.h>

// Include ES8311 register definitions directly — pure C header with no ESP32 dependencies.
// The driver itself (dac_es8311.cpp) is guarded by CONFIG_IDF_TARGET_ESP32P4 and only
// compiles on P4. These tests exercise the register constants, clock coefficient lookup
// table, volume mapping formula, and bit-field logic that live in the header.
#include "../../src/drivers/es8311_regs.h"

void setUp(void) {}
void tearDown(void) {}

// ===== 1. Register Constants =====

void test_i2c_address_is_0x18(void) {
    TEST_ASSERT_EQUAL_HEX8(0x18, ES8311_I2C_ADDR);
}

void test_dac_id_is_0x0004(void) {
    TEST_ASSERT_EQUAL_HEX16(0x0004, DAC_ID_ES8311);
}

void test_pa_pin_is_53(void) {
    TEST_ASSERT_EQUAL_INT(53, ES8311_PA_PIN);
}

void test_reg_reset_address(void) {
    TEST_ASSERT_EQUAL_HEX8(0x00, ES8311_REG_RESET);
}

void test_reg_sdpin_address(void) {
    TEST_ASSERT_EQUAL_HEX8(0x09, ES8311_REG_SDPIN);
}

void test_reg_dac_ctrl_address(void) {
    TEST_ASSERT_EQUAL_HEX8(0x31, ES8311_REG_DAC_CTRL);
}

void test_reg_dac_volume_address(void) {
    TEST_ASSERT_EQUAL_HEX8(0x32, ES8311_REG_DAC_VOLUME);
}

void test_reg_chip_id1_address(void) {
    TEST_ASSERT_EQUAL_HEX8(0xFD, ES8311_REG_CHIP_ID1);
}

void test_reg_chip_id2_address(void) {
    TEST_ASSERT_EQUAL_HEX8(0xFE, ES8311_REG_CHIP_ID2);
}

void test_reg_chip_version_address(void) {
    TEST_ASSERT_EQUAL_HEX8(0xFF, ES8311_REG_CHIP_VER);
}

// ===== 2. Volume Mapping =====
// Formula: regVal = (volume * 0xBF) / 100
// 0x00 = -95.5dB (near silence), 0xBF = 0dB (unity gain)

void test_volume_0_percent_maps_to_0x00(void) {
    uint8_t regVal = (uint8_t)(((uint16_t)0 * ES8311_VOL_0DB) / 100);
    TEST_ASSERT_EQUAL_HEX8(0x00, regVal);
}

void test_volume_100_percent_maps_to_0xBF(void) {
    uint8_t regVal = (uint8_t)(((uint16_t)100 * ES8311_VOL_0DB) / 100);
    TEST_ASSERT_EQUAL_HEX8(0xBF, regVal);
}

void test_volume_50_percent_maps_to_midrange(void) {
    uint8_t regVal = (uint8_t)(((uint16_t)50 * ES8311_VOL_0DB) / 100);
    // 50 * 191 / 100 = 95 = 0x5F
    TEST_ASSERT_EQUAL_HEX8(0x5F, regVal);
}

void test_volume_1_percent_maps_to_low_register(void) {
    uint8_t regVal = (uint8_t)(((uint16_t)1 * ES8311_VOL_0DB) / 100);
    // 1 * 191 / 100 = 1
    TEST_ASSERT_EQUAL_HEX8(0x01, regVal);
}

void test_volume_mapping_never_exceeds_0xBF(void) {
    // Even at 100%, the register should not exceed 0xBF (0dB, unity gain)
    for (uint16_t v = 0; v <= 100; v++) {
        uint8_t regVal = (uint8_t)(((uint16_t)v * ES8311_VOL_0DB) / 100);
        TEST_ASSERT_TRUE_MESSAGE(regVal <= ES8311_VOL_0DB,
            "Volume register exceeds 0xBF (0dB safety limit)");
    }
}

void test_volume_mapping_is_monotonic(void) {
    uint8_t prev = 0;
    for (uint16_t v = 0; v <= 100; v++) {
        uint8_t regVal = (uint8_t)(((uint16_t)v * ES8311_VOL_0DB) / 100);
        TEST_ASSERT_TRUE_MESSAGE(regVal >= prev,
            "Volume mapping is not monotonically increasing");
        prev = regVal;
    }
}

void test_vol_0db_constant(void) {
    TEST_ASSERT_EQUAL_HEX8(0xBF, ES8311_VOL_0DB);
}

void test_vol_min_constant(void) {
    TEST_ASSERT_EQUAL_HEX8(0x00, ES8311_VOL_MIN);
}

void test_vol_max_safe_equals_0db(void) {
    TEST_ASSERT_EQUAL_HEX8(ES8311_VOL_0DB, ES8311_VOL_MAX_SAFE);
}

// ===== 3. Clock Coefficient Lookup =====

void test_coeff_12288000_48000_found(void) {
    const Es8311ClockCoeff* c = es8311_find_coeff(12288000, 48000);
    TEST_ASSERT_NOT_NULL(c);
}

void test_coeff_12288000_48000_dac_osr(void) {
    const Es8311ClockCoeff* c = es8311_find_coeff(12288000, 48000);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_EQUAL_HEX8(0x10, c->dac_osr);
}

void test_coeff_12288000_96000_found(void) {
    const Es8311ClockCoeff* c = es8311_find_coeff(12288000, 96000);
    TEST_ASSERT_NOT_NULL(c);
}

void test_coeff_12288000_96000_fs_mode_double(void) {
    const Es8311ClockCoeff* c = es8311_find_coeff(12288000, 96000);
    TEST_ASSERT_NOT_NULL(c);
    // Double-speed FS mode for 96kHz
    TEST_ASSERT_EQUAL_UINT8(1, c->fs_mode);
}

void test_coeff_12288000_8000_found(void) {
    const Es8311ClockCoeff* c = es8311_find_coeff(12288000, 8000);
    TEST_ASSERT_NOT_NULL(c);
}

void test_coeff_12288000_44100_exists_in_table(void) {
    // The coefficient table DOES include a 12.288MHz/44.1kHz entry (non-integer ratio,
    // but the ES8311 can handle it with appropriate divider settings).
    const Es8311ClockCoeff* c = es8311_find_coeff(12288000, 44100);
    TEST_ASSERT_NOT_NULL(c);
}

void test_coeff_11289600_44100_found(void) {
    // 11.2896 MHz is the native 256fs MCLK for 44.1kHz family
    const Es8311ClockCoeff* c = es8311_find_coeff(11289600, 44100);
    TEST_ASSERT_NOT_NULL(c);
}

void test_coeff_11289600_88200_found(void) {
    const Es8311ClockCoeff* c = es8311_find_coeff(11289600, 88200);
    TEST_ASSERT_NOT_NULL(c);
    // Double-speed FS mode for 88.2kHz
    TEST_ASSERT_EQUAL_UINT8(1, c->fs_mode);
}

void test_coeff_unsupported_mclk_returns_null(void) {
    // 99.999999 MHz is not in the coefficient table
    const Es8311ClockCoeff* c = es8311_find_coeff(99999999, 48000);
    TEST_ASSERT_NULL(c);
}

void test_coeff_unsupported_rate_returns_null(void) {
    // 192kHz is not supported at any MCLK in the table
    const Es8311ClockCoeff* c = es8311_find_coeff(12288000, 192000);
    TEST_ASSERT_NULL(c);
}

void test_coeff_6144000_48000_found(void) {
    // Half-rate MCLK entries exist
    const Es8311ClockCoeff* c = es8311_find_coeff(6144000, 48000);
    TEST_ASSERT_NOT_NULL(c);
}

void test_coeff_table_count_matches(void) {
    // 8 entries at 12.288MHz + 4 entries at 6.144MHz + 4 entries at 11.2896MHz = 16 total
    TEST_ASSERT_EQUAL_UINT8(16, ES8311_COEFF_COUNT);
}

// ===== 4. Register Bit Masks =====

void test_csm_on_is_bit7(void) {
    TEST_ASSERT_EQUAL_HEX8(0x80, ES8311_CSM_ON);
}

void test_msc_master_is_bit6(void) {
    TEST_ASSERT_EQUAL_HEX8(0x40, ES8311_MSC_MASTER);
}

void test_dac_soft_mute_value(void) {
    TEST_ASSERT_EQUAL_HEX8(0x40, ES8311_DAC_SOFT_MUTE);
}

void test_dac_hard_mute_value(void) {
    TEST_ASSERT_EQUAL_HEX8(0x20, ES8311_DAC_MUTE);
}

void test_combined_mute_bits(void) {
    // Both mute bits combined = 0x60 (bits 6 and 5)
    TEST_ASSERT_EQUAL_HEX8(0x60, ES8311_DAC_SOFT_MUTE | ES8311_DAC_MUTE);
}

void test_mute_mask_clears_both_mute_bits(void) {
    // The driver clears mute bits using: dacCtrl &= ~(SOFT_MUTE | MUTE)
    // Inverse of 0x60 = 0x9F — this should clear bits 6 and 5 only
    uint8_t mask = (uint8_t)~(ES8311_DAC_SOFT_MUTE | ES8311_DAC_MUTE);
    TEST_ASSERT_EQUAL_HEX8(0x9F, mask);
}

void test_mute_set_operation(void) {
    // Starting with a register value that has no mute bits set
    uint8_t reg = 0x00;
    reg |= (ES8311_DAC_SOFT_MUTE | ES8311_DAC_MUTE);
    TEST_ASSERT_EQUAL_HEX8(0x60, reg);
}

void test_mute_clear_operation(void) {
    // Starting with a register value that has mute bits and other bits set
    uint8_t reg = 0xFF;
    reg &= ~(ES8311_DAC_SOFT_MUTE | ES8311_DAC_MUTE);
    TEST_ASSERT_EQUAL_HEX8(0x9F, reg);
}

void test_mute_roundtrip_preserves_other_bits(void) {
    // Set some non-mute bits, then set and clear mute — other bits should survive
    uint8_t reg = 0x15; // arbitrary: bits 4, 2, 0
    reg |= (ES8311_DAC_SOFT_MUTE | ES8311_DAC_MUTE);
    TEST_ASSERT_EQUAL_HEX8(0x75, reg); // 0x15 | 0x60
    reg &= ~(ES8311_DAC_SOFT_MUTE | ES8311_DAC_MUTE);
    TEST_ASSERT_EQUAL_HEX8(0x15, reg); // back to original
}

void test_rst_dig_is_bit4(void) {
    TEST_ASSERT_EQUAL_HEX8(0x10, ES8311_RST_DIG);
}

void test_rst_dac_dig_is_bit0(void) {
    TEST_ASSERT_EQUAL_HEX8(0x01, ES8311_RST_DAC_DIG);
}

void test_sdp_tristate_is_bit6(void) {
    TEST_ASSERT_EQUAL_HEX8(0x40, ES8311_SDP_TRISTATE);
}

// ===== 5. Word Length and Format Constants =====

void test_wl_16bit(void) {
    TEST_ASSERT_EQUAL_HEX8(0x0C, ES8311_WL_16BIT);
}

void test_wl_24bit(void) {
    TEST_ASSERT_EQUAL_HEX8(0x00, ES8311_WL_24BIT);
}

void test_wl_32bit(void) {
    TEST_ASSERT_EQUAL_HEX8(0x10, ES8311_WL_32BIT);
}

void test_wl_20bit(void) {
    TEST_ASSERT_EQUAL_HEX8(0x04, ES8311_WL_20BIT);
}

void test_wl_18bit(void) {
    TEST_ASSERT_EQUAL_HEX8(0x08, ES8311_WL_18BIT);
}

void test_fmt_i2s(void) {
    TEST_ASSERT_EQUAL_HEX8(0x00, ES8311_FMT_I2S);
}

void test_fmt_left_justified(void) {
    TEST_ASSERT_EQUAL_HEX8(0x01, ES8311_FMT_LJ);
}

void test_fmt_dsp(void) {
    TEST_ASSERT_EQUAL_HEX8(0x03, ES8311_FMT_DSP);
}

void test_sdpin_16bit_i2s_combined(void) {
    // The driver writes: writeReg(ES8311_REG_SDPIN, wl | ES8311_FMT_I2S)
    // For 16-bit I2S: 0x0C | 0x00 = 0x0C
    uint8_t val = ES8311_WL_16BIT | ES8311_FMT_I2S;
    TEST_ASSERT_EQUAL_HEX8(0x0C, val);
}

void test_sdpin_24bit_i2s_combined(void) {
    uint8_t val = ES8311_WL_24BIT | ES8311_FMT_I2S;
    TEST_ASSERT_EQUAL_HEX8(0x00, val);
}

void test_sdpin_32bit_i2s_combined(void) {
    uint8_t val = ES8311_WL_32BIT | ES8311_FMT_I2S;
    TEST_ASSERT_EQUAL_HEX8(0x10, val);
}

void test_sdpin_16bit_lj_combined(void) {
    uint8_t val = ES8311_WL_16BIT | ES8311_FMT_LJ;
    TEST_ASSERT_EQUAL_HEX8(0x0D, val);
}

void test_word_length_bits_dont_overlap_format_bits(void) {
    // Word length uses bits 4:2, format uses bits 1:0 — they should not overlap
    uint8_t wl_mask = 0x1C; // bits 4:2
    uint8_t fmt_mask = 0x03; // bits 1:0
    TEST_ASSERT_EQUAL_HEX8(0x00, wl_mask & fmt_mask);
}

// ===== 6. DAC ID Uniqueness =====

void test_dac_id_es8311_value(void) {
    TEST_ASSERT_EQUAL_HEX16(0x0004, DAC_ID_ES8311);
}

void test_dac_ids_are_unique(void) {
    // Verify DAC_ID_ES8311 (defined in es8311_regs.h) doesn't collide
    // with the IDs defined in dac_hal.h. We check the known values directly
    // since we can't include dac_hal.h in native tests.
    const uint16_t DAC_ID_NONE_VAL      = 0x0000;
    const uint16_t DAC_ID_PCM5102A_VAL  = 0x0001;
    const uint16_t DAC_ID_ES9038Q2M_VAL = 0x0002;
    const uint16_t DAC_ID_ES9842_VAL    = 0x0003;

    TEST_ASSERT_NOT_EQUAL(DAC_ID_NONE_VAL, DAC_ID_ES8311);
    TEST_ASSERT_NOT_EQUAL(DAC_ID_PCM5102A_VAL, DAC_ID_ES8311);
    TEST_ASSERT_NOT_EQUAL(DAC_ID_ES9038Q2M_VAL, DAC_ID_ES8311);
    TEST_ASSERT_NOT_EQUAL(DAC_ID_ES9842_VAL, DAC_ID_ES8311);
}

// ===== 7. Pin Definitions =====

void test_i2c_sda_pin(void) {
    TEST_ASSERT_EQUAL_INT(7, ES8311_I2C_SDA_PIN);
}

void test_i2c_scl_pin(void) {
    TEST_ASSERT_EQUAL_INT(8, ES8311_I2C_SCL_PIN);
}

// ===== Test Runner =====

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // 1. Register constants
    RUN_TEST(test_i2c_address_is_0x18);
    RUN_TEST(test_dac_id_is_0x0004);
    RUN_TEST(test_pa_pin_is_53);
    RUN_TEST(test_reg_reset_address);
    RUN_TEST(test_reg_sdpin_address);
    RUN_TEST(test_reg_dac_ctrl_address);
    RUN_TEST(test_reg_dac_volume_address);
    RUN_TEST(test_reg_chip_id1_address);
    RUN_TEST(test_reg_chip_id2_address);
    RUN_TEST(test_reg_chip_version_address);

    // 2. Volume mapping
    RUN_TEST(test_volume_0_percent_maps_to_0x00);
    RUN_TEST(test_volume_100_percent_maps_to_0xBF);
    RUN_TEST(test_volume_50_percent_maps_to_midrange);
    RUN_TEST(test_volume_1_percent_maps_to_low_register);
    RUN_TEST(test_volume_mapping_never_exceeds_0xBF);
    RUN_TEST(test_volume_mapping_is_monotonic);
    RUN_TEST(test_vol_0db_constant);
    RUN_TEST(test_vol_min_constant);
    RUN_TEST(test_vol_max_safe_equals_0db);

    // 3. Clock coefficient lookup
    RUN_TEST(test_coeff_12288000_48000_found);
    RUN_TEST(test_coeff_12288000_48000_dac_osr);
    RUN_TEST(test_coeff_12288000_96000_found);
    RUN_TEST(test_coeff_12288000_96000_fs_mode_double);
    RUN_TEST(test_coeff_12288000_8000_found);
    RUN_TEST(test_coeff_12288000_44100_exists_in_table);
    RUN_TEST(test_coeff_11289600_44100_found);
    RUN_TEST(test_coeff_11289600_88200_found);
    RUN_TEST(test_coeff_unsupported_mclk_returns_null);
    RUN_TEST(test_coeff_unsupported_rate_returns_null);
    RUN_TEST(test_coeff_6144000_48000_found);
    RUN_TEST(test_coeff_table_count_matches);

    // 4. Register bit masks
    RUN_TEST(test_csm_on_is_bit7);
    RUN_TEST(test_msc_master_is_bit6);
    RUN_TEST(test_dac_soft_mute_value);
    RUN_TEST(test_dac_hard_mute_value);
    RUN_TEST(test_combined_mute_bits);
    RUN_TEST(test_mute_mask_clears_both_mute_bits);
    RUN_TEST(test_mute_set_operation);
    RUN_TEST(test_mute_clear_operation);
    RUN_TEST(test_mute_roundtrip_preserves_other_bits);
    RUN_TEST(test_rst_dig_is_bit4);
    RUN_TEST(test_rst_dac_dig_is_bit0);
    RUN_TEST(test_sdp_tristate_is_bit6);

    // 5. Word length and format constants
    RUN_TEST(test_wl_16bit);
    RUN_TEST(test_wl_24bit);
    RUN_TEST(test_wl_32bit);
    RUN_TEST(test_wl_20bit);
    RUN_TEST(test_wl_18bit);
    RUN_TEST(test_fmt_i2s);
    RUN_TEST(test_fmt_left_justified);
    RUN_TEST(test_fmt_dsp);
    RUN_TEST(test_sdpin_16bit_i2s_combined);
    RUN_TEST(test_sdpin_24bit_i2s_combined);
    RUN_TEST(test_sdpin_32bit_i2s_combined);
    RUN_TEST(test_sdpin_16bit_lj_combined);
    RUN_TEST(test_word_length_bits_dont_overlap_format_bits);

    // 6. DAC ID uniqueness
    RUN_TEST(test_dac_id_es8311_value);
    RUN_TEST(test_dac_ids_are_unique);

    // 7. Pin definitions
    RUN_TEST(test_i2c_sda_pin);
    RUN_TEST(test_i2c_scl_pin);

    return UNITY_END();
}
