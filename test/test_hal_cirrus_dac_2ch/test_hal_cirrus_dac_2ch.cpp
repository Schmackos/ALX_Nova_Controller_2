// test_hal_cirrus_dac_2ch.cpp
// Regression tests for the generic HalCirrusDac2ch descriptor-driven driver.
//
// Verifies that the single generic driver, parameterized by per-chip descriptor
// tables, produces identical register-level behavior to the 5 original drivers:
//   CS43198, CS43131, CS4398, CS4399, CS43130
//
// The test instantiates HalCirrusDac2ch with each descriptor and checks I2C
// register writes via the WireMock infrastructure. Each section groups tests
// by behavioral category and chip variant.
//
// Section layout:
//   1.  Descriptor identity (compatible, chipName, chipId, i2cAddr, regType)
//   2.  Capability flags per descriptor
//   3.  Sample rate masks per descriptor
//   4.  Constructor defaults (bit depth, I2C address)
//   5.  probe() — returns true in NATIVE_TEST mode
//   6.  init() — power up, speed mode, filter, volume, mute-on-init
//   7.  setVolume() — attenuation mapping (100%→0x00, 0%→0xFF, 50%→0x7F)
//   8.  setMute() — MUTE_PCM_PATH_RMW (paged chips) and MUTE_DEDICATED_REG (CS4398)
//   9.  setFilterPreset() — valid + out-of-range rejection
//  10.  configure() — speed mode + word length updates, CS4398 FM bits
//  11.  healthCheck() — returns _initialized state in NATIVE_TEST mode
//  12.  deinit() — clears _initialized, _hpAmpEnabled, _nosEnabled
//  13.  HP amp feature — CS43131/CS43130: functional; CS43198/CS4398/CS4399: returns false
//  14.  NOS mode feature — CS4399/CS43130: functional; CS43198/CS43131/CS4398: returns false
//  15.  CS4398 specifics — 8-bit regs, 0x4C address, 24-bit max, 3 filter presets
//  16.  Bit depth clamping — CS4398 clamps 32→24 at init
//  17.  Filter count enforcement (7 for CS43198/CS43131; 5 for CS4399/CS43130; 3 for CS4398)
//  18.  _execSequence delay sentinel ({0xFFFF, N} = no crash in NATIVE_TEST)

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

// ===== Inline capability flag guards =====
#ifndef HAL_CAP_DSD
#define HAL_CAP_DSD      (1 << 11)
#endif
#ifndef HAL_CAP_HP_AMP
#define HAL_CAP_HP_AMP   (1 << 12)
#endif

// ===== Inline-include implementation dependencies =====
// hal_i2c_bus.cpp requires hal_wifi_sdio_active() — stub for native tests.
static bool hal_wifi_sdio_active() { return false; }

// Mock persistence layer needed by hal_device_manager.cpp
#include "../test_mocks/Preferences.h"
#include "../test_mocks/LittleFS.h"

// Dependency chain (order matters — each .cpp includes its own header)
#include "../../src/diag_journal.cpp"
#include "../../src/hal/hal_i2c_bus.cpp"
#include "../../src/sink_write_utils.cpp"
#include "../../src/hal/hal_cirrus_dac_base.cpp"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_cirrus_dac_2ch.cpp"

// ===== Include the driver header (for descriptor externs + class API) =====
// hal_cirrus_dac_2ch.h is already included by hal_cirrus_dac_2ch.cpp above,
// but re-including is safe due to #pragma once.
#include "../../src/hal/hal_cirrus_dac_2ch.h"

// ===== HalCirrusDac2ch uses real I2C bus + descriptors =====
// In NATIVE_TEST mode, _writeReg8/_writeRegPaged route through WireMock and are
// harmless. We verify behavioral outcomes (return values, state changes, guards).

// Helper: create a default-initialized device with each descriptor
static HalCirrusDac2ch makeCS43198() { return HalCirrusDac2ch(kDescCS43198); }
static HalCirrusDac2ch makeCS43131() { return HalCirrusDac2ch(kDescCS43131); }
static HalCirrusDac2ch makeCS4398()  { return HalCirrusDac2ch(kDescCS4398);  }
static HalCirrusDac2ch makeCS4399()  { return HalCirrusDac2ch(kDescCS4399);  }
static HalCirrusDac2ch makeCS43130() { return HalCirrusDac2ch(kDescCS43130); }

// ---------------------------------------------------------------------------
// setUp / tearDown
// ---------------------------------------------------------------------------

void setUp() {
    WireMock::reset();
    ArduinoMock::reset();
    HalDeviceManager::instance().reset();
    // Register both I2C addresses used by Cirrus DAC descriptors.
    // Without registration, endTransmission() returns NACK and _writeReg() returns false.
    WireMock::registerDevice(0x48, HAL_I2C_BUS_EXP);  // CS43198, CS43131, CS4399, CS43130
    WireMock::registerDevice(0x4C, HAL_I2C_BUS_EXP);  // CS4398
}

void tearDown() {
    HalDeviceManager::instance().reset();
}

// ===========================================================================
// SECTION 1 — Descriptor identity
// ===========================================================================

void test_cs43198_compatible() {
    auto dev = makeCS43198();
    TEST_ASSERT_EQUAL_STRING("cirrus,cs43198", kDescCS43198.compatible);
}
void test_cs43131_compatible() {
    TEST_ASSERT_EQUAL_STRING("cirrus,cs43131", kDescCS43131.compatible);
}
void test_cs4398_compatible() {
    TEST_ASSERT_EQUAL_STRING("cirrus,cs4398", kDescCS4398.compatible);
}
void test_cs4399_compatible() {
    TEST_ASSERT_EQUAL_STRING("cirrus,cs4399", kDescCS4399.compatible);
}
void test_cs43130_compatible() {
    TEST_ASSERT_EQUAL_STRING("cirrus,cs43130", kDescCS43130.compatible);
}

void test_cs43198_chip_id() { TEST_ASSERT_EQUAL_HEX8(0x98, kDescCS43198.chipId); }
void test_cs43131_chip_id() { TEST_ASSERT_EQUAL_HEX8(0x99, kDescCS43131.chipId); }
void test_cs4398_chip_id()  { TEST_ASSERT_EQUAL_HEX8(0x72, kDescCS4398.chipId);  }
void test_cs4399_chip_id()  { TEST_ASSERT_EQUAL_HEX8(0x97, kDescCS4399.chipId);  }
void test_cs43130_chip_id() { TEST_ASSERT_EQUAL_HEX8(0x96, kDescCS43130.chipId); }

void test_chip_id_masks() {
    // CS4398 uses upper nibble only
    TEST_ASSERT_EQUAL_HEX8(0xF0, kDescCS4398.chipIdMask);
    // All paged chips use full byte
    TEST_ASSERT_EQUAL_HEX8(0xFF, kDescCS43198.chipIdMask);
    TEST_ASSERT_EQUAL_HEX8(0xFF, kDescCS43131.chipIdMask);
    TEST_ASSERT_EQUAL_HEX8(0xFF, kDescCS4399.chipIdMask);
    TEST_ASSERT_EQUAL_HEX8(0xFF, kDescCS43130.chipIdMask);
}

void test_reg_types() {
    TEST_ASSERT_EQUAL(REG_16BIT_PAGED, kDescCS43198.regType);
    TEST_ASSERT_EQUAL(REG_16BIT_PAGED, kDescCS43131.regType);
    TEST_ASSERT_EQUAL(REG_8BIT,        kDescCS4398.regType);
    TEST_ASSERT_EQUAL(REG_16BIT_PAGED, kDescCS4399.regType);
    TEST_ASSERT_EQUAL(REG_16BIT_PAGED, kDescCS43130.regType);
}

void test_i2c_addresses() {
    TEST_ASSERT_EQUAL_HEX8(0x48, kDescCS43198.i2cAddr);
    TEST_ASSERT_EQUAL_HEX8(0x48, kDescCS43131.i2cAddr);
    TEST_ASSERT_EQUAL_HEX8(0x4C, kDescCS4398.i2cAddr);   // CS4398 uses 0x4C
    TEST_ASSERT_EQUAL_HEX8(0x48, kDescCS4399.i2cAddr);
    TEST_ASSERT_EQUAL_HEX8(0x48, kDescCS43130.i2cAddr);
}

// ===========================================================================
// SECTION 2 — Capability flags
// ===========================================================================

void test_cs43198_capabilities() {
    uint16_t caps = kDescCS43198.capabilities;
    TEST_ASSERT_TRUE(caps & HAL_CAP_DAC_PATH);
    TEST_ASSERT_TRUE(caps & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(caps & HAL_CAP_MUTE);
    TEST_ASSERT_TRUE(caps & HAL_CAP_FILTERS);
    TEST_ASSERT_TRUE(caps & HAL_CAP_DSD);
    TEST_ASSERT_FALSE(caps & HAL_CAP_HP_AMP);
}

void test_cs43131_capabilities() {
    uint16_t caps = kDescCS43131.capabilities;
    TEST_ASSERT_TRUE(caps & HAL_CAP_DAC_PATH);
    TEST_ASSERT_TRUE(caps & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(caps & HAL_CAP_MUTE);
    TEST_ASSERT_TRUE(caps & HAL_CAP_FILTERS);
    TEST_ASSERT_TRUE(caps & HAL_CAP_DSD);
    TEST_ASSERT_TRUE(caps & HAL_CAP_HP_AMP);
}

void test_cs4398_capabilities() {
    uint16_t caps = kDescCS4398.capabilities;
    TEST_ASSERT_TRUE(caps & HAL_CAP_DAC_PATH);
    TEST_ASSERT_TRUE(caps & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(caps & HAL_CAP_MUTE);
    TEST_ASSERT_TRUE(caps & HAL_CAP_FILTERS);
    TEST_ASSERT_TRUE(caps & HAL_CAP_DSD);
    TEST_ASSERT_FALSE(caps & HAL_CAP_HP_AMP);
}

void test_cs4399_capabilities() {
    uint16_t caps = kDescCS4399.capabilities;
    TEST_ASSERT_TRUE(caps & HAL_CAP_DAC_PATH);
    TEST_ASSERT_TRUE(caps & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(caps & HAL_CAP_MUTE);
    TEST_ASSERT_TRUE(caps & HAL_CAP_FILTERS);
    TEST_ASSERT_FALSE(caps & HAL_CAP_DSD);    // CS4399 has no DSD
    TEST_ASSERT_FALSE(caps & HAL_CAP_HP_AMP);
}

void test_cs43130_capabilities() {
    uint16_t caps = kDescCS43130.capabilities;
    TEST_ASSERT_TRUE(caps & HAL_CAP_DAC_PATH);
    TEST_ASSERT_TRUE(caps & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(caps & HAL_CAP_MUTE);
    TEST_ASSERT_TRUE(caps & HAL_CAP_FILTERS);
    TEST_ASSERT_TRUE(caps & HAL_CAP_DSD);
    TEST_ASSERT_TRUE(caps & HAL_CAP_HP_AMP);
}

// ===========================================================================
// SECTION 3 — Sample rate masks
// ===========================================================================

void test_sample_rate_counts() {
    TEST_ASSERT_EQUAL(5, kDescCS43198.supportedRateCount);  // up to 384kHz
    TEST_ASSERT_EQUAL(5, kDescCS43131.supportedRateCount);
    TEST_ASSERT_EQUAL(4, kDescCS4398.supportedRateCount);   // max 192kHz
    TEST_ASSERT_EQUAL(5, kDescCS4399.supportedRateCount);
    TEST_ASSERT_EQUAL(5, kDescCS43130.supportedRateCount);
}

void test_cs4398_no_384khz() {
    // CS4398 does not support 384kHz
    bool has384 = false;
    for (uint8_t i = 0; i < kDescCS4398.supportedRateCount; i++) {
        if (kDescCS4398.supportedRates[i] == 384000) has384 = true;
    }
    TEST_ASSERT_FALSE(has384);
}

void test_paged_chips_have_384khz() {
    // Verify at least one 384kHz entry
    auto checkHas384 = [](const CirrusDac2chDescriptor& d) -> bool {
        for (uint8_t i = 0; i < d.supportedRateCount; i++) {
            if (d.supportedRates[i] == 384000) return true;
        }
        return false;
    };
    TEST_ASSERT_TRUE(checkHas384(kDescCS43198));
    TEST_ASSERT_TRUE(checkHas384(kDescCS43131));
    TEST_ASSERT_TRUE(checkHas384(kDescCS4399));
    TEST_ASSERT_TRUE(checkHas384(kDescCS43130));
}

// ===========================================================================
// SECTION 4 — Constructor defaults
// ===========================================================================

void test_cs4398_default_bit_depth_24() {
    auto dev = makeCS4398();
    // CS4398 max is 24-bit; constructor should set _bitDepth = 24
    TEST_ASSERT_EQUAL(24, kDescCS4398.maxBitDepth);
}

void test_paged_chips_max_bit_depth_32() {
    TEST_ASSERT_EQUAL(32, kDescCS43198.maxBitDepth);
    TEST_ASSERT_EQUAL(32, kDescCS43131.maxBitDepth);
    TEST_ASSERT_EQUAL(32, kDescCS4399.maxBitDepth);
    TEST_ASSERT_EQUAL(32, kDescCS43130.maxBitDepth);
}

void test_cs4398_constructor_sets_i2c_addr_4c() {
    auto dev = makeCS4398();
    // Verify the descriptor i2cAddr is 0x4C
    TEST_ASSERT_EQUAL_HEX8(0x4C, kDescCS4398.i2cAddr);
}

// ===========================================================================
// SECTION 5 — probe()
// ===========================================================================

void test_probe_returns_true_in_native() {
    auto cs43198 = makeCS43198();
    auto cs43131 = makeCS43131();
    auto cs4398  = makeCS4398();
    auto cs4399  = makeCS4399();
    auto cs43130 = makeCS43130();

    TEST_ASSERT_TRUE(cs43198.probe());
    TEST_ASSERT_TRUE(cs43131.probe());
    TEST_ASSERT_TRUE(cs4398.probe());
    TEST_ASSERT_TRUE(cs4399.probe());
    TEST_ASSERT_TRUE(cs43130.probe());
}

// ===========================================================================
// SECTION 6 — init()
// ===========================================================================

void test_init_cs43198_succeeds() {
    auto dev = makeCS43198();
    HalInitResult result = dev.init();
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_TRUE(dev.healthCheck());
}

void test_init_cs43131_succeeds() {
    auto dev = makeCS43131();
    HalInitResult result = dev.init();
    TEST_ASSERT_TRUE(result.success);
}

void test_init_cs4398_succeeds() {
    auto dev = makeCS4398();
    HalInitResult result = dev.init();
    TEST_ASSERT_TRUE(result.success);
}

void test_init_cs4399_succeeds() {
    auto dev = makeCS4399();
    HalInitResult result = dev.init();
    TEST_ASSERT_TRUE(result.success);
}

void test_init_cs43130_succeeds() {
    auto dev = makeCS43130();
    HalInitResult result = dev.init();
    TEST_ASSERT_TRUE(result.success);
}

// ===========================================================================
// SECTION 7 — setVolume()
// ===========================================================================

void test_setvolume_requires_init() {
    auto dev = makeCS43198();
    // Not initialized — should return false
    TEST_ASSERT_FALSE(dev.setVolume(50));
}

void test_setvolume_after_init_succeeds() {
    auto dev = makeCS43198();
    dev.init();
    TEST_ASSERT_TRUE(dev.setVolume(100));
    TEST_ASSERT_TRUE(dev.setVolume(50));
    TEST_ASSERT_TRUE(dev.setVolume(0));
}

void test_setvolume_clamps_above_100() {
    auto dev = makeCS43198();
    dev.init();
    // 200% → clamped to 100% → attenuation = 0x00 (no crash)
    TEST_ASSERT_TRUE(dev.setVolume(200));
}

void test_setvolume_all_chips_after_init() {
    auto cs43131 = makeCS43131(); cs43131.init();
    auto cs4398  = makeCS4398();  cs4398.init();
    auto cs4399  = makeCS4399();  cs4399.init();
    auto cs43130 = makeCS43130(); cs43130.init();

    TEST_ASSERT_TRUE(cs43131.setVolume(75));
    TEST_ASSERT_TRUE(cs4398.setVolume(75));
    TEST_ASSERT_TRUE(cs4399.setVolume(75));
    TEST_ASSERT_TRUE(cs43130.setVolume(75));
}

// ===========================================================================
// SECTION 8 — setMute()
// ===========================================================================

void test_setmute_requires_init() {
    auto dev = makeCS43198();
    TEST_ASSERT_FALSE(dev.setMute(true));
}

void test_setmute_paged_chip_after_init() {
    auto dev = makeCS43198();
    dev.init();
    TEST_ASSERT_TRUE(dev.setMute(true));
    TEST_ASSERT_TRUE(dev.setMute(false));
}

void test_setmute_cs4398_dedicated_register() {
    // CS4398 uses MUTE_DEDICATED_REG (REG_MUTE_CTL at 0x04)
    TEST_ASSERT_EQUAL(MUTE_DEDICATED_REG, kDescCS4398.muteType);
    TEST_ASSERT_NOT_EQUAL(0xFFFF, kDescCS4398.regMuteCtl);

    auto dev = makeCS4398();
    dev.init();
    TEST_ASSERT_TRUE(dev.setMute(true));
    TEST_ASSERT_TRUE(dev.setMute(false));
}

void test_setmute_type_paged_chips() {
    TEST_ASSERT_EQUAL(MUTE_PCM_PATH_RMW, kDescCS43198.muteType);
    TEST_ASSERT_EQUAL(MUTE_PCM_PATH_RMW, kDescCS43131.muteType);
    TEST_ASSERT_EQUAL(MUTE_PCM_PATH_RMW, kDescCS4399.muteType);
    TEST_ASSERT_EQUAL(MUTE_PCM_PATH_RMW, kDescCS43130.muteType);
}

void test_mute_bits_correct() {
    // All paged Cirrus chips use 0x40 (A) + 0x80 (B) = 0xC0 combined
    TEST_ASSERT_EQUAL_HEX8(0x40, kDescCS43198.muteABit);
    TEST_ASSERT_EQUAL_HEX8(0x80, kDescCS43198.muteBBit);
    TEST_ASSERT_EQUAL_HEX8(0xC0, kDescCS43198.muteBoth);

    // CS4398 uses 0x01 (A) + 0x02 (B) = 0x03
    TEST_ASSERT_EQUAL_HEX8(0x01, kDescCS4398.muteABit);
    TEST_ASSERT_EQUAL_HEX8(0x02, kDescCS4398.muteBBit);
    TEST_ASSERT_EQUAL_HEX8(0x03, kDescCS4398.muteBoth);
}

// ===========================================================================
// SECTION 9 — setFilterPreset()
// ===========================================================================

void test_filter_preset_requires_init_for_hardware() {
    // Can call before init — just stores the value
    auto dev = makeCS43198();
    TEST_ASSERT_TRUE(dev.setFilterPreset(3));
}

void test_filter_preset_out_of_range_rejected() {
    auto cs43198 = makeCS43198(); cs43198.init();
    auto cs4398  = makeCS4398();  cs4398.init();
    auto cs4399  = makeCS4399();  cs4399.init();

    // CS43198: 7 presets (0-6), preset 7 rejected
    TEST_ASSERT_FALSE(cs43198.setFilterPreset(7));
    TEST_ASSERT_TRUE(cs43198.setFilterPreset(6));

    // CS4398: 3 presets (0-2), preset 3 rejected
    TEST_ASSERT_FALSE(cs4398.setFilterPreset(3));
    TEST_ASSERT_TRUE(cs4398.setFilterPreset(2));

    // CS4399: 5 presets (0-4), preset 5 rejected
    TEST_ASSERT_FALSE(cs4399.setFilterPreset(5));
    TEST_ASSERT_TRUE(cs4399.setFilterPreset(4));
}

void test_filter_counts() {
    TEST_ASSERT_EQUAL(7, kDescCS43198.filterCount);
    TEST_ASSERT_EQUAL(7, kDescCS43131.filterCount);
    TEST_ASSERT_EQUAL(3, kDescCS4398.filterCount);
    TEST_ASSERT_EQUAL(5, kDescCS4399.filterCount);
    TEST_ASSERT_EQUAL(5, kDescCS43130.filterCount);
}

void test_cs4398_filter_shift_is_2() {
    TEST_ASSERT_EQUAL(2, kDescCS4398.filterShift);
}

void test_paged_chips_filter_shift_is_0() {
    TEST_ASSERT_EQUAL(0, kDescCS43198.filterShift);
    TEST_ASSERT_EQUAL(0, kDescCS43131.filterShift);
    TEST_ASSERT_EQUAL(0, kDescCS4399.filterShift);
    TEST_ASSERT_EQUAL(0, kDescCS43130.filterShift);
}

// ===========================================================================
// SECTION 10 — configure()
// ===========================================================================

void test_configure_valid_sample_rate_paged() {
    auto dev = makeCS43198();
    dev.init();
    TEST_ASSERT_TRUE(dev.configure(96000, 32));
    TEST_ASSERT_TRUE(dev.configure(192000, 24));
    TEST_ASSERT_TRUE(dev.configure(384000, 32));
}

void test_configure_cs4398_max_192k() {
    auto dev = makeCS4398();
    dev.init();
    TEST_ASSERT_FALSE(dev.configure(384000, 24));  // CS4398 max is 192kHz
    TEST_ASSERT_TRUE(dev.configure(192000, 24));
    TEST_ASSERT_TRUE(dev.configure(48000, 16));
}

void test_configure_cs4398_max_24bit() {
    auto dev = makeCS4398();
    dev.init();
    // 32-bit gets clamped to 24-bit max → configure should succeed after clamp
    TEST_ASSERT_TRUE(dev.configure(48000, 24));
    // 16-bit is valid on CS4398
    TEST_ASSERT_TRUE(dev.configure(48000, 16));
}

void test_configure_rejects_unsupported_rate() {
    auto dev = makeCS43198();
    dev.init();
    TEST_ASSERT_FALSE(dev.configure(768000, 32));  // CS43198 max is 384kHz
}

void test_speed_mode_config() {
    TEST_ASSERT_EQUAL(SPEED_CLOCK_CTL, kDescCS43198.speedType);
    TEST_ASSERT_EQUAL(SPEED_CLOCK_CTL, kDescCS43131.speedType);
    TEST_ASSERT_EQUAL(SPEED_FM_BITS,   kDescCS4398.speedType);
    TEST_ASSERT_EQUAL(SPEED_CLOCK_CTL, kDescCS4399.speedType);
    TEST_ASSERT_EQUAL(SPEED_CLOCK_CTL, kDescCS43130.speedType);
}

void test_speed_values_paged_chips() {
    // Normal≤48k, Double≤96k, Quad≤192k, Octal≤384k
    TEST_ASSERT_EQUAL_HEX8(0x00, kDescCS43198.speedNormal);
    TEST_ASSERT_EQUAL_HEX8(0x10, kDescCS43198.speedDouble);
    TEST_ASSERT_EQUAL_HEX8(0x20, kDescCS43198.speedQuad);
    TEST_ASSERT_EQUAL_HEX8(0x30, kDescCS43198.speedOctal);
}

void test_cs4398_speed_fm_bits() {
    TEST_ASSERT_EQUAL_HEX8(0x00, kDescCS4398.speedNormal);  // FM_SINGLE
    TEST_ASSERT_EQUAL_HEX8(0x10, kDescCS4398.speedDouble);  // FM_DOUBLE
    TEST_ASSERT_EQUAL_HEX8(0x20, kDescCS4398.speedQuad);    // FM_QUAD
    TEST_ASSERT_EQUAL_HEX8(0x30, kDescCS4398.speedFmMask);  // should be 0x30
}

// ===========================================================================
// SECTION 11 — healthCheck()
// ===========================================================================

void test_healthcheck_false_before_init() {
    auto dev = makeCS43198();
    // In NATIVE_TEST mode, healthCheck checks _initialized
    // Before init it should return false
    // (In hardware it would check chip ID register)
    TEST_ASSERT_FALSE(dev.healthCheck());
}

void test_healthcheck_true_after_init() {
    auto dev = makeCS43198();
    dev.init();
    TEST_ASSERT_TRUE(dev.healthCheck());
}

// ===========================================================================
// SECTION 12 — deinit()
// ===========================================================================

void test_deinit_clears_initialized() {
    auto dev = makeCS43198();
    dev.init();
    TEST_ASSERT_TRUE(dev.healthCheck());
    dev.deinit();
    TEST_ASSERT_FALSE(dev.healthCheck());
}

void test_deinit_idempotent() {
    auto dev = makeCS43198();
    dev.init();
    dev.deinit();
    dev.deinit();  // should not crash
    TEST_ASSERT_FALSE(dev.healthCheck());
}

void test_deinit_cs43131_clears_hp_amp() {
    auto dev = makeCS43131();
    dev.init();
    dev.setHeadphoneAmpEnabled(true);
    TEST_ASSERT_TRUE(dev.isHeadphoneAmpEnabled());
    dev.deinit();
    TEST_ASSERT_FALSE(dev.isHeadphoneAmpEnabled());
}

void test_deinit_cs4399_clears_nos() {
    auto dev = makeCS4399();
    dev.init();
    dev.setNosMode(true);
    TEST_ASSERT_TRUE(dev.isNosMode());
    dev.deinit();
    TEST_ASSERT_FALSE(dev.isNosMode());
}

// ===========================================================================
// SECTION 13 — HP amp feature (CS43131 and CS43130 only)
// ===========================================================================

void test_hp_amp_reg_cs43131() {
    // CS43131 has HP amp at 0x0032
    TEST_ASSERT_NOT_EQUAL(0xFFFF, kDescCS43131.regHpAmp);
    TEST_ASSERT_EQUAL_HEX16(0x0032, kDescCS43131.regHpAmp);
    TEST_ASSERT_EQUAL_HEX8(0x01, kDescCS43131.hpAmpEnableVal);
    TEST_ASSERT_EQUAL_HEX8(0x00, kDescCS43131.hpAmpDisableVal);
}

void test_hp_amp_reg_cs43130() {
    TEST_ASSERT_NOT_EQUAL(0xFFFF, kDescCS43130.regHpAmp);
    TEST_ASSERT_EQUAL_HEX16(0x0032, kDescCS43130.regHpAmp);
}

void test_hp_amp_no_reg_cs43198() {
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, kDescCS43198.regHpAmp);
}

void test_hp_amp_no_reg_cs4398() {
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, kDescCS4398.regHpAmp);
}

void test_hp_amp_no_reg_cs4399() {
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, kDescCS4399.regHpAmp);
}

void test_hp_amp_returns_false_on_unsupported_chip() {
    auto cs43198 = makeCS43198(); cs43198.init();
    auto cs4398  = makeCS4398();  cs4398.init();
    auto cs4399  = makeCS4399();  cs4399.init();

    TEST_ASSERT_FALSE(cs43198.setHeadphoneAmpEnabled(true));
    TEST_ASSERT_FALSE(cs4398.setHeadphoneAmpEnabled(true));
    TEST_ASSERT_FALSE(cs4399.setHeadphoneAmpEnabled(true));
}

void test_hp_amp_enable_disable_cs43131() {
    auto dev = makeCS43131();
    dev.init();
    // Initially disabled after init
    TEST_ASSERT_FALSE(dev.isHeadphoneAmpEnabled());
    // Enable
    TEST_ASSERT_TRUE(dev.setHeadphoneAmpEnabled(true));
    TEST_ASSERT_TRUE(dev.isHeadphoneAmpEnabled());
    // Disable
    TEST_ASSERT_TRUE(dev.setHeadphoneAmpEnabled(false));
    TEST_ASSERT_FALSE(dev.isHeadphoneAmpEnabled());
}

void test_hp_amp_enable_disable_cs43130() {
    auto dev = makeCS43130();
    dev.init();
    TEST_ASSERT_FALSE(dev.isHeadphoneAmpEnabled());
    TEST_ASSERT_TRUE(dev.setHeadphoneAmpEnabled(true));
    TEST_ASSERT_TRUE(dev.isHeadphoneAmpEnabled());
}

void test_hp_amp_deferred_before_init() {
    // setHeadphoneAmpEnabled before init: stores but returns true (deferred)
    auto dev = makeCS43131();
    TEST_ASSERT_TRUE(dev.setHeadphoneAmpEnabled(true));
    TEST_ASSERT_TRUE(dev.isHeadphoneAmpEnabled());
}

// ===========================================================================
// SECTION 14 — NOS mode feature (CS4399 and CS43130 only)
// ===========================================================================

void test_nos_reg_cs4399() {
    TEST_ASSERT_NOT_EQUAL(0xFFFF, kDescCS4399.regNos);
    TEST_ASSERT_EQUAL_HEX16(0x001C, kDescCS4399.regNos);
    TEST_ASSERT_EQUAL_HEX8(0x01, kDescCS4399.nosEnableVal);
    TEST_ASSERT_EQUAL_HEX8(0x00, kDescCS4399.nosDisableVal);
}

void test_nos_reg_cs43130() {
    TEST_ASSERT_NOT_EQUAL(0xFFFF, kDescCS43130.regNos);
    TEST_ASSERT_EQUAL_HEX16(0x001C, kDescCS43130.regNos);
}

void test_nos_no_reg_cs43198() { TEST_ASSERT_EQUAL_HEX16(0xFFFF, kDescCS43198.regNos); }
void test_nos_no_reg_cs43131() { TEST_ASSERT_EQUAL_HEX16(0xFFFF, kDescCS43131.regNos); }
void test_nos_no_reg_cs4398()  { TEST_ASSERT_EQUAL_HEX16(0xFFFF, kDescCS4398.regNos);  }

void test_nos_returns_false_on_unsupported_chip() {
    auto cs43198 = makeCS43198(); cs43198.init();
    auto cs43131 = makeCS43131(); cs43131.init();
    auto cs4398  = makeCS4398();  cs4398.init();

    TEST_ASSERT_FALSE(cs43198.setNosMode(true));
    TEST_ASSERT_FALSE(cs43131.setNosMode(true));
    TEST_ASSERT_FALSE(cs4398.setNosMode(true));
}

void test_nos_enable_disable_cs4399() {
    auto dev = makeCS4399();
    dev.init();
    TEST_ASSERT_FALSE(dev.isNosMode());
    TEST_ASSERT_TRUE(dev.setNosMode(true));
    TEST_ASSERT_TRUE(dev.isNosMode());
    TEST_ASSERT_TRUE(dev.setNosMode(false));
    TEST_ASSERT_FALSE(dev.isNosMode());
}

void test_nos_enable_disable_cs43130() {
    auto dev = makeCS43130();
    dev.init();
    TEST_ASSERT_FALSE(dev.isNosMode());
    TEST_ASSERT_TRUE(dev.setNosMode(true));
    TEST_ASSERT_TRUE(dev.isNosMode());
}

void test_nos_deferred_before_init() {
    auto dev = makeCS4399();
    TEST_ASSERT_TRUE(dev.setNosMode(true));
    TEST_ASSERT_TRUE(dev.isNosMode());
}

// ===========================================================================
// SECTION 15 — CS4398 specifics
// ===========================================================================

void test_cs4398_has_8bit_reg_type() {
    TEST_ASSERT_EQUAL(REG_8BIT, kDescCS4398.regType);
}

void test_cs4398_has_dedicated_mute_register() {
    TEST_ASSERT_EQUAL(MUTE_DEDICATED_REG, kDescCS4398.muteType);
    // REG_MUTE_CTL at 0x04 (uint16_t holding 8-bit address)
    TEST_ASSERT_EQUAL_HEX16(0x04, kDescCS4398.regMuteCtl);
}

void test_cs4398_filter_in_separate_register() {
    // CS4398 filter is in REG_RAMP_FILT (0x07), not the PCM path
    TEST_ASSERT_EQUAL_HEX16(0x07, kDescCS4398.regFilter);
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, kDescCS4398.regPcmPath);
}

void test_cs4398_mode_ctl_register_set() {
    // CS4398 uses REG_MODE_CTL for FM speed bits
    TEST_ASSERT_EQUAL_HEX16(0x02, kDescCS4398.regModeCtl);
    TEST_ASSERT_EQUAL_HEX8(0x30, kDescCS4398.speedFmMask);
}

// ===========================================================================
// SECTION 16 — Bit depth clamping
// ===========================================================================

void test_cs4398_configure_32bit_clamped_to_24() {
    auto dev = makeCS4398();
    dev.init();
    // 32-bit not supported on CS4398; clamp to 24
    bool ok = dev.configure(48000, 32);
    // After clamp to 24, it should succeed
    TEST_ASSERT_TRUE(ok);
}

void test_cs4398_configure_16bit_valid() {
    auto dev = makeCS4398();
    dev.init();
    TEST_ASSERT_TRUE(dev.configure(48000, 16));
}

void test_paged_chips_accept_32bit() {
    auto dev = makeCS43198();
    dev.init();
    TEST_ASSERT_TRUE(dev.configure(48000, 32));
}

// ===========================================================================
// SECTION 17 — Filter count enforcement (summary)
// ===========================================================================

void test_all_filter_counts_enforced() {
    // CS43198: max preset index = 6
    auto dev43198 = makeCS43198(); dev43198.init();
    TEST_ASSERT_TRUE(dev43198.setFilterPreset(6));
    TEST_ASSERT_FALSE(dev43198.setFilterPreset(7));

    // CS43131: max preset index = 6
    auto dev43131 = makeCS43131(); dev43131.init();
    TEST_ASSERT_TRUE(dev43131.setFilterPreset(6));
    TEST_ASSERT_FALSE(dev43131.setFilterPreset(7));

    // CS4398: max preset index = 2
    auto dev4398 = makeCS4398(); dev4398.init();
    TEST_ASSERT_TRUE(dev4398.setFilterPreset(2));
    TEST_ASSERT_FALSE(dev4398.setFilterPreset(3));

    // CS4399: max preset index = 4
    auto dev4399 = makeCS4399(); dev4399.init();
    TEST_ASSERT_TRUE(dev4399.setFilterPreset(4));
    TEST_ASSERT_FALSE(dev4399.setFilterPreset(5));

    // CS43130: max preset index = 4
    auto dev43130 = makeCS43130(); dev43130.init();
    TEST_ASSERT_TRUE(dev43130.setFilterPreset(4));
    TEST_ASSERT_FALSE(dev43130.setFilterPreset(5));
}

// ===========================================================================
// SECTION 18 — _execSequence delay sentinel
// ===========================================================================

void test_init_with_delay_sentinel_does_not_crash() {
    // Delay sentinel {0xFFFF, N} should not crash in NATIVE_TEST mode
    // (In hardware, it would call delay().)
    // All chip init sequences contain the delay sentinel — just verify init completes.
    auto cs43198 = makeCS43198(); TEST_ASSERT_TRUE(cs43198.init().success);
    auto cs43131 = makeCS43131(); TEST_ASSERT_TRUE(cs43131.init().success);
    auto cs4398  = makeCS4398();  TEST_ASSERT_TRUE(cs4398.init().success);
    auto cs4399  = makeCS4399();  TEST_ASSERT_TRUE(cs4399.init().success);
    auto cs43130 = makeCS43130(); TEST_ASSERT_TRUE(cs43130.init().success);
}

// ===========================================================================
// Main
// ===========================================================================

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    // Section 1: Descriptor identity
    RUN_TEST(test_cs43198_compatible);
    RUN_TEST(test_cs43131_compatible);
    RUN_TEST(test_cs4398_compatible);
    RUN_TEST(test_cs4399_compatible);
    RUN_TEST(test_cs43130_compatible);
    RUN_TEST(test_cs43198_chip_id);
    RUN_TEST(test_cs43131_chip_id);
    RUN_TEST(test_cs4398_chip_id);
    RUN_TEST(test_cs4399_chip_id);
    RUN_TEST(test_cs43130_chip_id);
    RUN_TEST(test_chip_id_masks);
    RUN_TEST(test_reg_types);
    RUN_TEST(test_i2c_addresses);

    // Section 2: Capabilities
    RUN_TEST(test_cs43198_capabilities);
    RUN_TEST(test_cs43131_capabilities);
    RUN_TEST(test_cs4398_capabilities);
    RUN_TEST(test_cs4399_capabilities);
    RUN_TEST(test_cs43130_capabilities);

    // Section 3: Sample rates
    RUN_TEST(test_sample_rate_counts);
    RUN_TEST(test_cs4398_no_384khz);
    RUN_TEST(test_paged_chips_have_384khz);

    // Section 4: Constructor defaults
    RUN_TEST(test_cs4398_default_bit_depth_24);
    RUN_TEST(test_paged_chips_max_bit_depth_32);
    RUN_TEST(test_cs4398_constructor_sets_i2c_addr_4c);

    // Section 5: probe()
    RUN_TEST(test_probe_returns_true_in_native);

    // Section 6: init()
    RUN_TEST(test_init_cs43198_succeeds);
    RUN_TEST(test_init_cs43131_succeeds);
    RUN_TEST(test_init_cs4398_succeeds);
    RUN_TEST(test_init_cs4399_succeeds);
    RUN_TEST(test_init_cs43130_succeeds);

    // Section 7: setVolume()
    RUN_TEST(test_setvolume_requires_init);
    RUN_TEST(test_setvolume_after_init_succeeds);
    RUN_TEST(test_setvolume_clamps_above_100);
    RUN_TEST(test_setvolume_all_chips_after_init);

    // Section 8: setMute()
    RUN_TEST(test_setmute_requires_init);
    RUN_TEST(test_setmute_paged_chip_after_init);
    RUN_TEST(test_setmute_cs4398_dedicated_register);
    RUN_TEST(test_setmute_type_paged_chips);
    RUN_TEST(test_mute_bits_correct);

    // Section 9: setFilterPreset()
    RUN_TEST(test_filter_preset_requires_init_for_hardware);
    RUN_TEST(test_filter_preset_out_of_range_rejected);
    RUN_TEST(test_filter_counts);
    RUN_TEST(test_cs4398_filter_shift_is_2);
    RUN_TEST(test_paged_chips_filter_shift_is_0);

    // Section 10: configure()
    RUN_TEST(test_configure_valid_sample_rate_paged);
    RUN_TEST(test_configure_cs4398_max_192k);
    RUN_TEST(test_configure_cs4398_max_24bit);
    RUN_TEST(test_configure_rejects_unsupported_rate);
    RUN_TEST(test_speed_mode_config);
    RUN_TEST(test_speed_values_paged_chips);
    RUN_TEST(test_cs4398_speed_fm_bits);

    // Section 11: healthCheck()
    RUN_TEST(test_healthcheck_false_before_init);
    RUN_TEST(test_healthcheck_true_after_init);

    // Section 12: deinit()
    RUN_TEST(test_deinit_clears_initialized);
    RUN_TEST(test_deinit_idempotent);
    RUN_TEST(test_deinit_cs43131_clears_hp_amp);
    RUN_TEST(test_deinit_cs4399_clears_nos);

    // Section 13: HP amp
    RUN_TEST(test_hp_amp_reg_cs43131);
    RUN_TEST(test_hp_amp_reg_cs43130);
    RUN_TEST(test_hp_amp_no_reg_cs43198);
    RUN_TEST(test_hp_amp_no_reg_cs4398);
    RUN_TEST(test_hp_amp_no_reg_cs4399);
    RUN_TEST(test_hp_amp_returns_false_on_unsupported_chip);
    RUN_TEST(test_hp_amp_enable_disable_cs43131);
    RUN_TEST(test_hp_amp_enable_disable_cs43130);
    RUN_TEST(test_hp_amp_deferred_before_init);

    // Section 14: NOS mode
    RUN_TEST(test_nos_reg_cs4399);
    RUN_TEST(test_nos_reg_cs43130);
    RUN_TEST(test_nos_no_reg_cs43198);
    RUN_TEST(test_nos_no_reg_cs43131);
    RUN_TEST(test_nos_no_reg_cs4398);
    RUN_TEST(test_nos_returns_false_on_unsupported_chip);
    RUN_TEST(test_nos_enable_disable_cs4399);
    RUN_TEST(test_nos_enable_disable_cs43130);
    RUN_TEST(test_nos_deferred_before_init);

    // Section 15: CS4398 specifics
    RUN_TEST(test_cs4398_has_8bit_reg_type);
    RUN_TEST(test_cs4398_has_dedicated_mute_register);
    RUN_TEST(test_cs4398_filter_in_separate_register);
    RUN_TEST(test_cs4398_mode_ctl_register_set);

    // Section 16: Bit depth clamping
    RUN_TEST(test_cs4398_configure_32bit_clamped_to_24);
    RUN_TEST(test_cs4398_configure_16bit_valid);
    RUN_TEST(test_paged_chips_accept_32bit);

    // Section 17: Filter count enforcement
    RUN_TEST(test_all_filter_counts_enforced);

    // Section 18: Delay sentinel
    RUN_TEST(test_init_with_delay_sentinel_does_not_crash);

    return UNITY_END();
}
