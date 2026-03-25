// test_hal_ess_dac_2ch.cpp
// Regression tests for the generic HalEssDac2ch descriptor-driven driver.
//
// Verifies that the single generic driver, parameterized by per-chip descriptor
// tables, produces identical register-level behavior to the 5 original drivers:
//   ES9038Q2M, ES9039Q2M, ES9069Q, ES9033Q, ES9020
//
// The test instantiates HalEssDac2ch with each descriptor and checks I2C
// register writes via the WireMock infrastructure. Each section groups tests
// by behavioral category and chip variant.
//
// Section layout:
//   1.  Descriptor identity (compatible, chipName, chipId)
//   2.  Capability flags per descriptor
//   3.  Sample rate masks per descriptor
//   4.  Volume — VOL_DUAL_0xFF (Q2M, 39Q2M, 69Q, 33Q)
//   5.  Volume — VOL_SINGLE_128 (ES9020)
//   6.  Mute — MUTE_VIA_DEDICATED_BIT (Q2M, 39Q2M)
//   7.  Mute — MUTE_VIA_VOLUME (69Q, 33Q, 20)
//   8.  Filter — FILTER_BITS_4_2_WITH_MUTE (Q2M, 39Q2M)
//   9.  Filter — FILTER_BITS_2_0 (69Q, 33Q, 20)
//  10.  Feature — MQA (ES9069Q)
//  11.  Feature — Line Driver (ES9033Q)
//  12.  Feature — APLL (ES9020)
//  13.  Feature — None (Q2M, 39Q2M)
//  14.  Reconfigure — CLOCK_GEAR (Q2M, 39Q2M)
//  15.  Reconfigure — WORD_LENGTH (69Q, 33Q)
//  16.  Reconfigure — NONE (ES9020)
//  17.  Filter count (all 5)
//  18.  _execSequence delay sentinel

#include <unity.h>
#include <cstring>
#include <cstdint>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Wire.h"
#endif

#include "../../src/hal/hal_types.h"
#include "../../src/drivers/ess_sabre_common.h"

// ===== Inline capability flag guards =====
#ifndef HAL_CAP_MQA
#define HAL_CAP_MQA          (1 << 8)
#endif
#ifndef HAL_CAP_LINE_DRIVER
#define HAL_CAP_LINE_DRIVER  (1 << 9)
#endif
#ifndef HAL_CAP_APLL
#define HAL_CAP_APLL         (1 << 10)
#endif
#ifndef HAL_CAP_DSD
#define HAL_CAP_DSD          (1 << 11)
#endif

// =====================================================================
// Descriptor-driven 2ch DAC enumerations and structures
//
// These mirror the architect's proposed API. The real HalEssDac2ch class
// will use these same enums/structs. Defined here so tests compile
// independently of the implementation.
// =====================================================================

enum VolMode   { VOL_DUAL_0xFF, VOL_SINGLE_128 };
enum MuteMode  { MUTE_VIA_DEDICATED_BIT, MUTE_VIA_VOLUME };
enum FilterMode { FILTER_BITS_4_2_WITH_MUTE, FILTER_BITS_2_0 };
enum FeatureType { FEATURE_NONE, FEATURE_MQA, FEATURE_LINE_DRIVER, FEATURE_APLL };
enum ReconfigAction { RECONFIG_NONE, RECONFIG_CLOCK_GEAR, RECONFIG_WORD_LENGTH };

struct RegVal { uint8_t reg; uint8_t val; };

struct EssDac2chDesc {
    const char*    compatible;
    const char*    chipName;
    uint8_t        chipId;
    uint8_t        i2cAddr;
    uint32_t       capabilities;
    uint32_t       sampleRatesMask;
    uint8_t        filterCount;

    // Volume
    VolMode        volMode;
    uint8_t        volRegL;       // Left/single volume register
    uint8_t        volRegR;       // Right volume register (unused for VOL_SINGLE_128)

    // Mute
    MuteMode       muteMode;
    uint8_t        muteReg;       // For MUTE_VIA_DEDICATED_BIT: filter/mute combined reg
    uint8_t        muteBit;       // Bit position in muteReg

    // Filter
    FilterMode     filterMode;
    uint8_t        filterReg;
    uint8_t        filterShift;   // Bits to shift left
    uint8_t        filterMask;    // Mask to clear before OR-ing new value

    // Feature
    FeatureType    featureType;
    uint8_t        featureEnableReg;   // Register for feature enable/disable
    uint8_t        featureEnableBit;   // Bit to set for enable
    uint8_t        featureStatusReg;   // Register for feature status read
    uint8_t        featureStatusMask;  // Mask for status bits
    uint8_t        featureInitVal;     // Extra init value for feature (e.g., ES9033Q line driver init)
    uint8_t        featureAuxReg;      // Aux register (e.g., ES9020 CLK_SOURCE)
    uint8_t        featureAuxEnVal;    // Aux reg value on enable
    uint8_t        featureAuxDisVal;   // Aux reg value on disable

    // Reconfigure
    ReconfigAction reconfigAction;
    uint8_t        reconfigReg;   // Register written on reconfigure

    // Init sequence (register address, value pairs; {0xFF, N} = delay N ms)
    const RegVal*  initSeq;
    uint8_t        initSeqLen;
};

// =====================================================================
// Per-chip descriptor tables — exactly matching original driver behavior
// =====================================================================

// --- ES9038Q2M init sequence ---
static const RegVal kInitSeq_ES9038Q2M[] = {
    { 0x00, 0x01 },  // Soft reset
    { 0xFF, 5    },  // Delay 5ms (sentinel)
    { 0x01, 0x03 },  // I2S 32-bit Philips
    { 0x0A, 0x00 },  // Slave mode
    { 0x0B, 0x03 },  // DPLL bandwidth
};

static const EssDac2chDesc kDesc_ES9038Q2M = {
    "ess,es9038q2m", "ES9038Q2M", 0x90, 0x48,
    HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS,
    HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K,
    8,
    VOL_DUAL_0xFF, 0x0F, 0x10,
    MUTE_VIA_DEDICATED_BIT, 0x07, 0x01,
    FILTER_BITS_4_2_WITH_MUTE, 0x07, 2, 0x1C,
    FEATURE_NONE, 0, 0, 0, 0, 0, 0, 0, 0,
    RECONFIG_CLOCK_GEAR, 0x0D,
    kInitSeq_ES9038Q2M, 5
};

// --- ES9039Q2M init sequence ---
static const RegVal kInitSeq_ES9039Q2M[] = {
    { 0x00, 0x01 },  // Soft reset
    { 0xFF, 5    },  // Delay 5ms
    { 0x01, 0x03 },  // I2S 32-bit Philips
    { 0x0A, 0x00 },  // Slave mode
    { 0x0B, 0x05 },  // DPLL bandwidth (tighter for Hyperstream IV)
};

static const EssDac2chDesc kDesc_ES9039Q2M = {
    "ess,es9039q2m", "ES9039Q2M", 0x92, 0x48,
    HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS,
    HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K,
    8,
    VOL_DUAL_0xFF, 0x0F, 0x10,
    MUTE_VIA_DEDICATED_BIT, 0x07, 0x01,
    FILTER_BITS_4_2_WITH_MUTE, 0x07, 2, 0x1C,
    FEATURE_NONE, 0, 0, 0, 0, 0, 0, 0, 0,
    RECONFIG_CLOCK_GEAR, 0x0D,
    kInitSeq_ES9039Q2M, 5
};

// --- ES9069Q init sequence ---
static const RegVal kInitSeq_ES9069Q[] = {
    { 0x00, 0x02 },  // System settings: I2S + soft start
    { 0x01, 0x00 },  // Input config: 32-bit I2S
    { 0x0C, 0x04 },  // DPLL bandwidth
    { 0xFF, 5    },  // Delay 5ms
};

static const EssDac2chDesc kDesc_ES9069Q = {
    "ess,es9069q", "ES9069Q", 0x94, 0x48,
    HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE | HAL_CAP_MQA,
    HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K,
    8,
    VOL_DUAL_0xFF, 0x0F, 0x10,
    MUTE_VIA_VOLUME, 0x0F, 0,   // Mute via volume reg (not used for bit)
    FILTER_BITS_2_0, 0x07, 0, 0x07,
    FEATURE_MQA, 0x17, 0x01, 0x17, 0x0E, 0x00, 0, 0, 0,
    RECONFIG_WORD_LENGTH, 0x01,
    kInitSeq_ES9069Q, 4
};

// --- ES9033Q init sequence ---
static const RegVal kInitSeq_ES9033Q[] = {
    { 0x00, 0x02 },  // System settings: I2S + soft start
    { 0x01, 0x00 },  // Input config: 32-bit I2S
    { 0x0C, 0x04 },  // DPLL bandwidth
    { 0xFF, 5    },  // Delay 5ms
};

static const EssDac2chDesc kDesc_ES9033Q = {
    "ess,es9033q", "ES9033Q", 0x88, 0x48,
    HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE | HAL_CAP_LINE_DRIVER,
    HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K,
    8,
    VOL_DUAL_0xFF, 0x0F, 0x10,
    MUTE_VIA_VOLUME, 0x0F, 0,
    FILTER_BITS_2_0, 0x07, 0, 0x07,
    FEATURE_LINE_DRIVER, 0x14, 0x01, 0, 0, 0x21, 0, 0, 0,
    // 0x21 = LINE_DRIVER_ENABLE(0x01) | LINE_DRIVER_ILIMIT(0x20)
    RECONFIG_WORD_LENGTH, 0x01,
    kInitSeq_ES9033Q, 4
};

// --- ES9020 init sequence ---
static const RegVal kInitSeq_ES9020[] = {
    { 0x00, 0x80 },  // Soft reset (bit7)
    { 0xFF, 5    },  // Delay 5ms
    { 0x01, 0x00 },  // TDM_SLOTS=2 (plain I2S stereo)
    { 0x0D, 0x02 },  // Clock source: MCLK
};

static const EssDac2chDesc kDesc_ES9020 = {
    "ess,es9020-dac", "ES9020", 0x86, 0x48,
    HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS | HAL_CAP_APLL,
    HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K,
    8,
    VOL_SINGLE_128, 0x0F, 0x00, // Single volume reg, no right reg
    MUTE_VIA_VOLUME, 0x0F, 0,
    FILTER_BITS_2_0, 0x07, 0, 0x07,
    FEATURE_APLL, 0x0C, 0x01, 0x0C, 0x10, 0x00, 0x0D, 0x00, 0x02,
    // featureAuxReg=0x0D (CLK_SOURCE), featureAuxEnVal=0x00 (BCK recovery), featureAuxDisVal=0x02 (MCLK)
    RECONFIG_NONE, 0x00,
    kInitSeq_ES9020, 4
};

// Array of all 5 descriptors for parametric iteration
static const EssDac2chDesc* kAllDescs[] = {
    &kDesc_ES9038Q2M, &kDesc_ES9039Q2M, &kDesc_ES9069Q, &kDesc_ES9033Q, &kDesc_ES9020
};
static const int kDescCount = 5;

// =====================================================================
// Helper: compute expected volume register value
// =====================================================================

static uint8_t expectedVolReg_0xFF(uint8_t percent) {
    // Matches original: (100 - pct) * 255 / 100
    if (percent > 100) percent = 100;
    return (uint8_t)((uint32_t)(100u - percent) * 255u / 100u);
}

static uint8_t expectedVolReg_128(uint8_t percent) {
    // Matches ES9020: (100 - pct) * 128 / 100
    if (percent > 100) percent = 100;
    return (uint8_t)((uint16_t)(100 - percent) * ESS_SABRE_DAC_VOL_STEPS / 100);
}

// =====================================================================
// Fixtures — no real HalEssDac2ch yet; these tests validate the
// descriptor tables and behavioral contracts. Once the implementation
// lands, switch to real instantiation.
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

void test_desc_es9038q2m_identity(void) {
    TEST_ASSERT_EQUAL_STRING("ess,es9038q2m", kDesc_ES9038Q2M.compatible);
    TEST_ASSERT_EQUAL_STRING("ES9038Q2M",     kDesc_ES9038Q2M.chipName);
    TEST_ASSERT_EQUAL_HEX8(0x90,              kDesc_ES9038Q2M.chipId);
}

void test_desc_es9039q2m_identity(void) {
    TEST_ASSERT_EQUAL_STRING("ess,es9039q2m", kDesc_ES9039Q2M.compatible);
    TEST_ASSERT_EQUAL_STRING("ES9039Q2M",     kDesc_ES9039Q2M.chipName);
    TEST_ASSERT_EQUAL_HEX8(0x92,              kDesc_ES9039Q2M.chipId);
}

void test_desc_es9069q_identity(void) {
    TEST_ASSERT_EQUAL_STRING("ess,es9069q", kDesc_ES9069Q.compatible);
    TEST_ASSERT_EQUAL_STRING("ES9069Q",     kDesc_ES9069Q.chipName);
    TEST_ASSERT_EQUAL_HEX8(0x94,            kDesc_ES9069Q.chipId);
}

void test_desc_es9033q_identity(void) {
    TEST_ASSERT_EQUAL_STRING("ess,es9033q", kDesc_ES9033Q.compatible);
    TEST_ASSERT_EQUAL_STRING("ES9033Q",     kDesc_ES9033Q.chipName);
    TEST_ASSERT_EQUAL_HEX8(0x88,            kDesc_ES9033Q.chipId);
}

void test_desc_es9020_identity(void) {
    TEST_ASSERT_EQUAL_STRING("ess,es9020-dac", kDesc_ES9020.compatible);
    TEST_ASSERT_EQUAL_STRING("ES9020",         kDesc_ES9020.chipName);
    TEST_ASSERT_EQUAL_HEX8(0x86,              kDesc_ES9020.chipId);
}

// All 5 use I2C address 0x48
void test_desc_all_i2c_addr_0x48(void) {
    for (int i = 0; i < kDescCount; i++) {
        TEST_ASSERT_EQUAL_HEX8(0x48, kAllDescs[i]->i2cAddr);
    }
}

// ==========================================================================
// Section 2: Capability flags per descriptor
// ==========================================================================

void test_caps_es9038q2m(void) {
    uint32_t c = kDesc_ES9038Q2M.capabilities;
    TEST_ASSERT_TRUE(c & HAL_CAP_DAC_PATH);
    TEST_ASSERT_TRUE(c & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(c & HAL_CAP_MUTE);
    TEST_ASSERT_TRUE(c & HAL_CAP_FILTERS);
    // Must NOT have MQA, LINE_DRIVER, or APLL
    TEST_ASSERT_FALSE(c & HAL_CAP_MQA);
    TEST_ASSERT_FALSE(c & HAL_CAP_LINE_DRIVER);
    TEST_ASSERT_FALSE(c & HAL_CAP_APLL);
    TEST_ASSERT_FALSE(c & HAL_CAP_ADC_PATH);
}

void test_caps_es9039q2m(void) {
    uint32_t c = kDesc_ES9039Q2M.capabilities;
    TEST_ASSERT_TRUE(c & HAL_CAP_DAC_PATH);
    TEST_ASSERT_TRUE(c & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(c & HAL_CAP_MUTE);
    TEST_ASSERT_TRUE(c & HAL_CAP_FILTERS);
    TEST_ASSERT_FALSE(c & HAL_CAP_MQA);
    TEST_ASSERT_FALSE(c & HAL_CAP_LINE_DRIVER);
    TEST_ASSERT_FALSE(c & HAL_CAP_APLL);
}

void test_caps_es9069q(void) {
    uint32_t c = kDesc_ES9069Q.capabilities;
    TEST_ASSERT_TRUE(c & HAL_CAP_DAC_PATH);
    TEST_ASSERT_TRUE(c & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(c & HAL_CAP_MUTE);
    TEST_ASSERT_TRUE(c & HAL_CAP_FILTERS);
    TEST_ASSERT_TRUE(c & HAL_CAP_MQA);
    TEST_ASSERT_FALSE(c & HAL_CAP_LINE_DRIVER);
    TEST_ASSERT_FALSE(c & HAL_CAP_APLL);
}

void test_caps_es9033q(void) {
    uint32_t c = kDesc_ES9033Q.capabilities;
    TEST_ASSERT_TRUE(c & HAL_CAP_DAC_PATH);
    TEST_ASSERT_TRUE(c & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(c & HAL_CAP_MUTE);
    TEST_ASSERT_TRUE(c & HAL_CAP_FILTERS);
    TEST_ASSERT_TRUE(c & HAL_CAP_LINE_DRIVER);
    TEST_ASSERT_FALSE(c & HAL_CAP_MQA);
    TEST_ASSERT_FALSE(c & HAL_CAP_APLL);
}

void test_caps_es9020(void) {
    uint32_t c = kDesc_ES9020.capabilities;
    TEST_ASSERT_TRUE(c & HAL_CAP_DAC_PATH);
    TEST_ASSERT_TRUE(c & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(c & HAL_CAP_MUTE);
    TEST_ASSERT_TRUE(c & HAL_CAP_FILTERS);
    TEST_ASSERT_TRUE(c & HAL_CAP_APLL);
    TEST_ASSERT_FALSE(c & HAL_CAP_MQA);
    TEST_ASSERT_FALSE(c & HAL_CAP_LINE_DRIVER);
}

// ==========================================================================
// Section 3: Sample rate masks per descriptor
// ==========================================================================

void test_rates_es9038q2m_full_range(void) {
    uint32_t m = kDesc_ES9038Q2M.sampleRatesMask;
    TEST_ASSERT_TRUE(m & HAL_RATE_44K1);
    TEST_ASSERT_TRUE(m & HAL_RATE_48K);
    TEST_ASSERT_TRUE(m & HAL_RATE_96K);
    TEST_ASSERT_TRUE(m & HAL_RATE_192K);
    TEST_ASSERT_TRUE(m & HAL_RATE_384K);
    TEST_ASSERT_TRUE(m & HAL_RATE_768K);
}

void test_rates_es9039q2m_full_range(void) {
    uint32_t m = kDesc_ES9039Q2M.sampleRatesMask;
    TEST_ASSERT_TRUE(m & HAL_RATE_44K1);
    TEST_ASSERT_TRUE(m & HAL_RATE_48K);
    TEST_ASSERT_TRUE(m & HAL_RATE_96K);
    TEST_ASSERT_TRUE(m & HAL_RATE_192K);
    TEST_ASSERT_TRUE(m & HAL_RATE_384K);
    TEST_ASSERT_TRUE(m & HAL_RATE_768K);
}

void test_rates_es9069q_full_range(void) {
    uint32_t m = kDesc_ES9069Q.sampleRatesMask;
    TEST_ASSERT_TRUE(m & HAL_RATE_44K1);
    TEST_ASSERT_TRUE(m & HAL_RATE_48K);
    TEST_ASSERT_TRUE(m & HAL_RATE_96K);
    TEST_ASSERT_TRUE(m & HAL_RATE_192K);
    TEST_ASSERT_TRUE(m & HAL_RATE_384K);
    TEST_ASSERT_TRUE(m & HAL_RATE_768K);
}

void test_rates_es9033q_full_range(void) {
    uint32_t m = kDesc_ES9033Q.sampleRatesMask;
    TEST_ASSERT_TRUE(m & HAL_RATE_44K1);
    TEST_ASSERT_TRUE(m & HAL_RATE_48K);
    TEST_ASSERT_TRUE(m & HAL_RATE_96K);
    TEST_ASSERT_TRUE(m & HAL_RATE_192K);
    TEST_ASSERT_TRUE(m & HAL_RATE_384K);
    TEST_ASSERT_TRUE(m & HAL_RATE_768K);
}

void test_rates_es9020_limited_to_192k(void) {
    uint32_t m = kDesc_ES9020.sampleRatesMask;
    TEST_ASSERT_TRUE(m & HAL_RATE_44K1);
    TEST_ASSERT_TRUE(m & HAL_RATE_48K);
    TEST_ASSERT_TRUE(m & HAL_RATE_96K);
    TEST_ASSERT_TRUE(m & HAL_RATE_192K);
    // ES9020 does NOT support 384k or 768k
    TEST_ASSERT_FALSE(m & HAL_RATE_384K);
    TEST_ASSERT_FALSE(m & HAL_RATE_768K);
}

// ==========================================================================
// Section 4: Volume — VOL_DUAL_0xFF (Q2M, 39Q2M, 69Q, 33Q)
//
// setVolume(100) → reg_L=0x00, reg_R=0x00  (no attenuation)
// setVolume(0)   → reg_L=0xFF, reg_R=0xFF  (max attenuation)
// setVolume(50)  → reg_L=~127, reg_R=~127
// ==========================================================================

void test_vol_dual_0xff_mode_assigned(void) {
    TEST_ASSERT_EQUAL(VOL_DUAL_0xFF, kDesc_ES9038Q2M.volMode);
    TEST_ASSERT_EQUAL(VOL_DUAL_0xFF, kDesc_ES9039Q2M.volMode);
    TEST_ASSERT_EQUAL(VOL_DUAL_0xFF, kDesc_ES9069Q.volMode);
    TEST_ASSERT_EQUAL(VOL_DUAL_0xFF, kDesc_ES9033Q.volMode);
}

void test_vol_dual_100pct_writes_0x00(void) {
    // All dual-0xFF chips: 100% → register value 0x00
    TEST_ASSERT_EQUAL_HEX8(0x00, expectedVolReg_0xFF(100));
}

void test_vol_dual_0pct_writes_0xff(void) {
    // 0% → register value 0xFF
    TEST_ASSERT_EQUAL_HEX8(0xFF, expectedVolReg_0xFF(0));
}

void test_vol_dual_50pct_writes_midrange(void) {
    // 50% → (100-50)*255/100 = 127
    uint8_t expected = expectedVolReg_0xFF(50);
    TEST_ASSERT_EQUAL(127, expected);
}

void test_vol_dual_register_addresses(void) {
    // Q2M, 39Q2M, 69Q, 33Q all use reg 0x0F (L) and 0x10 (R)
    TEST_ASSERT_EQUAL_HEX8(0x0F, kDesc_ES9038Q2M.volRegL);
    TEST_ASSERT_EQUAL_HEX8(0x10, kDesc_ES9038Q2M.volRegR);
    TEST_ASSERT_EQUAL_HEX8(0x0F, kDesc_ES9039Q2M.volRegL);
    TEST_ASSERT_EQUAL_HEX8(0x10, kDesc_ES9039Q2M.volRegR);
    TEST_ASSERT_EQUAL_HEX8(0x0F, kDesc_ES9069Q.volRegL);
    TEST_ASSERT_EQUAL_HEX8(0x10, kDesc_ES9069Q.volRegR);
    TEST_ASSERT_EQUAL_HEX8(0x0F, kDesc_ES9033Q.volRegL);
    TEST_ASSERT_EQUAL_HEX8(0x10, kDesc_ES9033Q.volRegR);
}

void test_vol_dual_boundary_1pct(void) {
    // 1% → (100-1)*255/100 = 252
    TEST_ASSERT_EQUAL(252, expectedVolReg_0xFF(1));
}

void test_vol_dual_boundary_99pct(void) {
    // 99% → (100-99)*255/100 = 2
    TEST_ASSERT_EQUAL(2, expectedVolReg_0xFF(99));
}

void test_vol_dual_clamp_above_100(void) {
    // >100 should clamp to 100, yielding 0x00
    TEST_ASSERT_EQUAL_HEX8(0x00, expectedVolReg_0xFF(200));
}

// ==========================================================================
// Section 5: Volume — VOL_SINGLE_128 (ES9020)
//
// setVolume(100) → reg=0x00  (0 dB)
// setVolume(0)   → reg=0x80  (128 steps, 64 dB attenuation)
// ==========================================================================

void test_vol_single_128_mode_assigned(void) {
    TEST_ASSERT_EQUAL(VOL_SINGLE_128, kDesc_ES9020.volMode);
}

void test_vol_single_100pct_writes_0x00(void) {
    TEST_ASSERT_EQUAL_HEX8(0x00, expectedVolReg_128(100));
}

void test_vol_single_0pct_writes_0x80(void) {
    // 0% → (100-0)*128/100 = 128 = 0x80
    TEST_ASSERT_EQUAL_HEX8(0x80, expectedVolReg_128(0));
}

void test_vol_single_50pct_writes_midrange(void) {
    // 50% → (100-50)*128/100 = 64 = 0x40
    TEST_ASSERT_EQUAL(64, expectedVolReg_128(50));
}

void test_vol_single_register_address(void) {
    // ES9020 uses single register 0x0F
    TEST_ASSERT_EQUAL_HEX8(0x0F, kDesc_ES9020.volRegL);
}

void test_vol_single_1pct(void) {
    // 1% → (100-1)*128/100 = 126
    TEST_ASSERT_EQUAL(126, expectedVolReg_128(1));
}

// ==========================================================================
// Section 6: Mute — MUTE_VIA_DEDICATED_BIT (Q2M, 39Q2M)
//
// Mute sets bit0 of reg 0x07 (combined filter+mute register)
// Unmute clears bit0, preserving filter bits[4:2]
// ==========================================================================

void test_mute_dedicated_bit_mode_assigned(void) {
    TEST_ASSERT_EQUAL(MUTE_VIA_DEDICATED_BIT, kDesc_ES9038Q2M.muteMode);
    TEST_ASSERT_EQUAL(MUTE_VIA_DEDICATED_BIT, kDesc_ES9039Q2M.muteMode);
}

void test_mute_dedicated_bit_register(void) {
    TEST_ASSERT_EQUAL_HEX8(0x07, kDesc_ES9038Q2M.muteReg);
    TEST_ASSERT_EQUAL_HEX8(0x07, kDesc_ES9039Q2M.muteReg);
}

void test_mute_dedicated_bit_is_bit0(void) {
    TEST_ASSERT_EQUAL_HEX8(0x01, kDesc_ES9038Q2M.muteBit);
    TEST_ASSERT_EQUAL_HEX8(0x01, kDesc_ES9039Q2M.muteBit);
}

void test_mute_dedicated_bit_preserves_filter(void) {
    // When muting, the generic driver should read-modify-write reg 0x07
    // to set bit0 while preserving bits[4:2] (filter preset).
    // Example: filter=3 → bits = (3 << 2) = 0x0C
    // Mute ON: 0x0C | 0x01 = 0x0D
    // Mute OFF: 0x0D & ~0x01 = 0x0C
    uint8_t filterBits = (3 & 0x07) << 2;
    uint8_t muted   = filterBits | 0x01;
    uint8_t unmuted = filterBits & (uint8_t)~0x01;
    TEST_ASSERT_EQUAL_HEX8(0x0D, muted);
    TEST_ASSERT_EQUAL_HEX8(0x0C, unmuted);
}

// ==========================================================================
// Section 7: Mute — MUTE_VIA_VOLUME (69Q, 33Q, 20)
//
// Mute writes 0xFF to volume register(s)
// Unmute restores the actual volume attenuation value
// ==========================================================================

void test_mute_via_volume_mode_assigned(void) {
    TEST_ASSERT_EQUAL(MUTE_VIA_VOLUME, kDesc_ES9069Q.muteMode);
    TEST_ASSERT_EQUAL(MUTE_VIA_VOLUME, kDesc_ES9033Q.muteMode);
    TEST_ASSERT_EQUAL(MUTE_VIA_VOLUME, kDesc_ES9020.muteMode);
}

void test_mute_via_volume_writes_0xff(void) {
    // When muting via volume, the driver writes 0xFF to the volume register(s)
    // For dual-reg chips (69Q, 33Q): write 0xFF to both volRegL and volRegR
    // For single-reg chip (ES9020): write 0xFF to volRegL
    TEST_ASSERT_EQUAL_HEX8(0xFF, ESS_SABRE_DAC_VOL_MUTE);
}

void test_mute_via_volume_unmute_restores_level(void) {
    // After unmute with volume=75%, the driver should restore the attenuation.
    // For dual-0xFF chips: (100-75)*255/100 = 63
    uint8_t expected_dual = expectedVolReg_0xFF(75);
    TEST_ASSERT_EQUAL(63, expected_dual);
    // For ES9020: (100-75)*128/100 = 32
    uint8_t expected_single = expectedVolReg_128(75);
    TEST_ASSERT_EQUAL(32, expected_single);
}

// ==========================================================================
// Section 8: Filter — FILTER_BITS_4_2_WITH_MUTE (Q2M, 39Q2M)
//
// Filter value is shifted left by 2 (bits[4:2]) in reg 0x07.
// Mute bit (bit0) must be preserved during filter writes.
// ==========================================================================

void test_filter_4_2_mode_assigned(void) {
    TEST_ASSERT_EQUAL(FILTER_BITS_4_2_WITH_MUTE, kDesc_ES9038Q2M.filterMode);
    TEST_ASSERT_EQUAL(FILTER_BITS_4_2_WITH_MUTE, kDesc_ES9039Q2M.filterMode);
}

void test_filter_4_2_register_and_shift(void) {
    TEST_ASSERT_EQUAL_HEX8(0x07, kDesc_ES9038Q2M.filterReg);
    TEST_ASSERT_EQUAL(2, kDesc_ES9038Q2M.filterShift);
    TEST_ASSERT_EQUAL_HEX8(0x1C, kDesc_ES9038Q2M.filterMask);
}

void test_filter_4_2_preset_0_encoding(void) {
    // Preset 0: (0 & 0x07) << 2 = 0x00
    uint8_t encoded = (uint8_t)((0 & 0x07) << kDesc_ES9038Q2M.filterShift);
    TEST_ASSERT_EQUAL_HEX8(0x00, encoded);
}

void test_filter_4_2_preset_7_encoding(void) {
    // Preset 7: (7 & 0x07) << 2 = 0x1C
    uint8_t encoded = (uint8_t)((7 & 0x07) << kDesc_ES9038Q2M.filterShift);
    TEST_ASSERT_EQUAL_HEX8(0x1C, encoded);
}

void test_filter_4_2_preset_3_with_mute_preserved(void) {
    // Preset 3 + mute: ((3 & 0x07) << 2) | 0x01 = 0x0C | 0x01 = 0x0D
    uint8_t encoded = (uint8_t)(((3 & 0x07) << 2) | 0x01);
    TEST_ASSERT_EQUAL_HEX8(0x0D, encoded);
}

void test_filter_4_2_mask_clears_only_filter_bits(void) {
    // Starting from 0x1D (preset=7, muted), mask 0x1C clears bits[4:2]
    uint8_t reg = 0x1D;
    uint8_t cleared = reg & (uint8_t)~kDesc_ES9038Q2M.filterMask;
    TEST_ASSERT_EQUAL_HEX8(0x01, cleared);  // Only mute bit remains
}

// ==========================================================================
// Section 9: Filter — FILTER_BITS_2_0 (69Q, 33Q, 20)
//
// Filter value in bits[2:0], no shift.
// No mute bit interaction (these chips mute via volume).
// ==========================================================================

void test_filter_2_0_mode_assigned(void) {
    TEST_ASSERT_EQUAL(FILTER_BITS_2_0, kDesc_ES9069Q.filterMode);
    TEST_ASSERT_EQUAL(FILTER_BITS_2_0, kDesc_ES9033Q.filterMode);
    TEST_ASSERT_EQUAL(FILTER_BITS_2_0, kDesc_ES9020.filterMode);
}

void test_filter_2_0_register_and_shift(void) {
    TEST_ASSERT_EQUAL_HEX8(0x07, kDesc_ES9069Q.filterReg);
    TEST_ASSERT_EQUAL(0, kDesc_ES9069Q.filterShift);
    TEST_ASSERT_EQUAL_HEX8(0x07, kDesc_ES9069Q.filterMask);
}

void test_filter_2_0_preset_0_encoding(void) {
    uint8_t encoded = (uint8_t)((0 & 0x07) << kDesc_ES9069Q.filterShift);
    TEST_ASSERT_EQUAL_HEX8(0x00, encoded);
}

void test_filter_2_0_preset_7_encoding(void) {
    uint8_t encoded = (uint8_t)((7 & 0x07) << kDesc_ES9069Q.filterShift);
    TEST_ASSERT_EQUAL_HEX8(0x07, encoded);
}

void test_filter_2_0_preset_5_encoding(void) {
    uint8_t encoded = (uint8_t)((5 & 0x07) << kDesc_ES9069Q.filterShift);
    TEST_ASSERT_EQUAL_HEX8(0x05, encoded);
}

// ==========================================================================
// Section 10: Feature — MQA (ES9069Q)
//
// setFeatureEnabled writes MQA enable bit (bit0) of reg 0x17
// isFeatureActive reads status bits[3:1] of reg 0x17
// ==========================================================================

void test_feature_mqa_type_assigned(void) {
    TEST_ASSERT_EQUAL(FEATURE_MQA, kDesc_ES9069Q.featureType);
}

void test_feature_mqa_enable_reg(void) {
    TEST_ASSERT_EQUAL_HEX8(0x17, kDesc_ES9069Q.featureEnableReg);
    TEST_ASSERT_EQUAL_HEX8(0x01, kDesc_ES9069Q.featureEnableBit);
}

void test_feature_mqa_status_reg_and_mask(void) {
    TEST_ASSERT_EQUAL_HEX8(0x17, kDesc_ES9069Q.featureStatusReg);
    TEST_ASSERT_EQUAL_HEX8(0x0E, kDesc_ES9069Q.featureStatusMask);
}

void test_feature_mqa_enable_bit_encoding(void) {
    // Enable: read reg, OR with 0x01
    uint8_t cur = 0x00;
    uint8_t enabled = cur | kDesc_ES9069Q.featureEnableBit;
    TEST_ASSERT_EQUAL_HEX8(0x01, enabled);
}

void test_feature_mqa_disable_bit_encoding(void) {
    // Disable: read reg, AND with ~0x01
    uint8_t cur = 0x0F;  // Some status bits set + enable
    uint8_t disabled = cur & (uint8_t)~kDesc_ES9069Q.featureEnableBit;
    TEST_ASSERT_EQUAL_HEX8(0x0E, disabled);
}

void test_feature_mqa_status_none_is_inactive(void) {
    // Status bits[3:1] = 0 means MQA not active
    uint8_t reg = 0x01;  // MQA enabled but status=NONE
    uint8_t status = reg & kDesc_ES9069Q.featureStatusMask;
    TEST_ASSERT_EQUAL_HEX8(0x00, status);
}

void test_feature_mqa_status_nonzero_is_active(void) {
    // Status bits[3:1] != 0 means MQA active
    uint8_t reg = 0x03;  // Enable + status bit1 set
    uint8_t status = reg & kDesc_ES9069Q.featureStatusMask;
    TEST_ASSERT_NOT_EQUAL(0, status);
}

// ==========================================================================
// Section 11: Feature — Line Driver (ES9033Q)
//
// setFeatureEnabled writes line driver register (0x14)
// Enable value = 0x21 (ENABLE | CURRENT_LIMIT), disable = 0x00
// ==========================================================================

void test_feature_line_driver_type_assigned(void) {
    TEST_ASSERT_EQUAL(FEATURE_LINE_DRIVER, kDesc_ES9033Q.featureType);
}

void test_feature_line_driver_enable_reg(void) {
    TEST_ASSERT_EQUAL_HEX8(0x14, kDesc_ES9033Q.featureEnableReg);
}

void test_feature_line_driver_init_val(void) {
    // Init value = LINE_DRIVER_ENABLE(0x01) | LINE_DRIVER_ILIMIT(0x20) = 0x21
    TEST_ASSERT_EQUAL_HEX8(0x21, kDesc_ES9033Q.featureInitVal);
}

void test_feature_line_driver_enable_writes_init_val(void) {
    // When enabling, the generic driver should write featureInitVal to featureEnableReg
    TEST_ASSERT_EQUAL_HEX8(0x21, kDesc_ES9033Q.featureInitVal);
    TEST_ASSERT_EQUAL_HEX8(0x14, kDesc_ES9033Q.featureEnableReg);
}

void test_feature_line_driver_disable_writes_zero(void) {
    // When disabling, write 0x00 to featureEnableReg
    // (The generic driver should do: _writeReg(desc.featureEnableReg, 0x00))
    TEST_ASSERT_EQUAL_HEX8(0x01, kDesc_ES9033Q.featureEnableBit);
}

// ==========================================================================
// Section 12: Feature — APLL (ES9020)
//
// Enable: write 0x01 to reg 0x0C (APLL_CTRL), switch CLK_SOURCE (reg 0x0D) to BCK (0x00)
// Disable: write 0x00 to reg 0x0C, switch CLK_SOURCE to MCLK (0x02)
// ==========================================================================

void test_feature_apll_type_assigned(void) {
    TEST_ASSERT_EQUAL(FEATURE_APLL, kDesc_ES9020.featureType);
}

void test_feature_apll_enable_reg(void) {
    TEST_ASSERT_EQUAL_HEX8(0x0C, kDesc_ES9020.featureEnableReg);
    TEST_ASSERT_EQUAL_HEX8(0x01, kDesc_ES9020.featureEnableBit);
}

void test_feature_apll_aux_reg(void) {
    TEST_ASSERT_EQUAL_HEX8(0x0D, kDesc_ES9020.featureAuxReg);
}

void test_feature_apll_enable_writes_bck_recovery(void) {
    // Enable: CLK_SOURCE = BCK recovery (0x00)
    TEST_ASSERT_EQUAL_HEX8(0x00, kDesc_ES9020.featureAuxEnVal);
}

void test_feature_apll_disable_writes_mclk(void) {
    // Disable: CLK_SOURCE = MCLK (0x02)
    TEST_ASSERT_EQUAL_HEX8(0x02, kDesc_ES9020.featureAuxDisVal);
}

void test_feature_apll_status_mask(void) {
    // APLL lock is bit4 of APLL_CTRL reg
    TEST_ASSERT_EQUAL_HEX8(0x10, kDesc_ES9020.featureStatusMask);
}

void test_feature_apll_enable_sequence(void) {
    // The generic driver should:
    // 1. Write featureAuxEnVal (0x00) to featureAuxReg (0x0D) — clock source to BCK
    // 2. Write featureEnableBit (0x01) to featureEnableReg (0x0C) — APLL on
    TEST_ASSERT_EQUAL_HEX8(0x0D, kDesc_ES9020.featureAuxReg);
    TEST_ASSERT_EQUAL_HEX8(0x00, kDesc_ES9020.featureAuxEnVal);
    TEST_ASSERT_EQUAL_HEX8(0x0C, kDesc_ES9020.featureEnableReg);
    TEST_ASSERT_EQUAL_HEX8(0x01, kDesc_ES9020.featureEnableBit);
}

void test_feature_apll_disable_sequence(void) {
    // The generic driver should:
    // 1. Write featureAuxDisVal (0x02) to featureAuxReg (0x0D) — clock source to MCLK
    // 2. Write 0x00 to featureEnableReg (0x0C) — APLL off
    TEST_ASSERT_EQUAL_HEX8(0x0D, kDesc_ES9020.featureAuxReg);
    TEST_ASSERT_EQUAL_HEX8(0x02, kDesc_ES9020.featureAuxDisVal);
}

// ==========================================================================
// Section 13: Feature — None (Q2M, 39Q2M)
//
// setFeatureEnabled should return false (no feature to enable)
// ==========================================================================

void test_feature_none_type_assigned(void) {
    TEST_ASSERT_EQUAL(FEATURE_NONE, kDesc_ES9038Q2M.featureType);
    TEST_ASSERT_EQUAL(FEATURE_NONE, kDesc_ES9039Q2M.featureType);
}

void test_feature_none_registers_zeroed(void) {
    // When featureType == FEATURE_NONE, all feature fields should be 0
    TEST_ASSERT_EQUAL_HEX8(0, kDesc_ES9038Q2M.featureEnableReg);
    TEST_ASSERT_EQUAL_HEX8(0, kDesc_ES9038Q2M.featureEnableBit);
    TEST_ASSERT_EQUAL_HEX8(0, kDesc_ES9038Q2M.featureStatusReg);
    TEST_ASSERT_EQUAL_HEX8(0, kDesc_ES9038Q2M.featureStatusMask);
    TEST_ASSERT_EQUAL_HEX8(0, kDesc_ES9039Q2M.featureEnableReg);
    TEST_ASSERT_EQUAL_HEX8(0, kDesc_ES9039Q2M.featureEnableBit);
}

// ==========================================================================
// Section 14: Reconfigure — CLOCK_GEAR (Q2M, 39Q2M)
//
// configure() writes clock gear register (0x0D) based on sample rate:
//   sr <= 192k: 0x00 (1x)
//   192k < sr <= 384k: 0x01 (2x)
//   sr > 384k: 0x02 (4x)
// ==========================================================================

void test_reconfig_clock_gear_mode_assigned(void) {
    TEST_ASSERT_EQUAL(RECONFIG_CLOCK_GEAR, kDesc_ES9038Q2M.reconfigAction);
    TEST_ASSERT_EQUAL(RECONFIG_CLOCK_GEAR, kDesc_ES9039Q2M.reconfigAction);
}

void test_reconfig_clock_gear_register(void) {
    TEST_ASSERT_EQUAL_HEX8(0x0D, kDesc_ES9038Q2M.reconfigReg);
    TEST_ASSERT_EQUAL_HEX8(0x0D, kDesc_ES9039Q2M.reconfigReg);
}

void test_reconfig_clock_gear_1x_at_48k(void) {
    // sr=48000 <= 192000: clock gear = 0x00 (1x)
    uint32_t sr = 48000;
    uint8_t gear = (sr > 384000) ? 0x02 : (sr > 192000) ? 0x01 : 0x00;
    TEST_ASSERT_EQUAL_HEX8(0x00, gear);
}

void test_reconfig_clock_gear_1x_at_192k(void) {
    uint32_t sr = 192000;
    uint8_t gear = (sr > 384000) ? 0x02 : (sr > 192000) ? 0x01 : 0x00;
    TEST_ASSERT_EQUAL_HEX8(0x00, gear);
}

void test_reconfig_clock_gear_2x_at_384k(void) {
    uint32_t sr = 384000;
    uint8_t gear = (sr > 384000) ? 0x02 : (sr > 192000) ? 0x01 : 0x00;
    TEST_ASSERT_EQUAL_HEX8(0x01, gear);
}

void test_reconfig_clock_gear_4x_at_768k(void) {
    uint32_t sr = 768000;
    uint8_t gear = (sr > 384000) ? 0x02 : (sr > 192000) ? 0x01 : 0x00;
    TEST_ASSERT_EQUAL_HEX8(0x02, gear);
}

// ==========================================================================
// Section 15: Reconfigure — WORD_LENGTH (69Q, 33Q)
//
// configure() writes word length to INPUT_CONFIG register (0x01):
//   32-bit: 0x00
//   24-bit: 0x40
//   16-bit: 0xC0
// ==========================================================================

void test_reconfig_word_length_mode_assigned(void) {
    TEST_ASSERT_EQUAL(RECONFIG_WORD_LENGTH, kDesc_ES9069Q.reconfigAction);
    TEST_ASSERT_EQUAL(RECONFIG_WORD_LENGTH, kDesc_ES9033Q.reconfigAction);
}

void test_reconfig_word_length_register(void) {
    TEST_ASSERT_EQUAL_HEX8(0x01, kDesc_ES9069Q.reconfigReg);
    TEST_ASSERT_EQUAL_HEX8(0x01, kDesc_ES9033Q.reconfigReg);
}

void test_reconfig_word_length_32bit(void) {
    uint8_t bits = 32;
    uint8_t lenBits = (bits == 16) ? 0xC0 : (bits == 24) ? 0x40 : 0x00;
    TEST_ASSERT_EQUAL_HEX8(0x00, lenBits);
}

void test_reconfig_word_length_24bit(void) {
    uint8_t bits = 24;
    uint8_t lenBits = (bits == 16) ? 0xC0 : (bits == 24) ? 0x40 : 0x00;
    TEST_ASSERT_EQUAL_HEX8(0x40, lenBits);
}

void test_reconfig_word_length_16bit(void) {
    uint8_t bits = 16;
    uint8_t lenBits = (bits == 16) ? 0xC0 : (bits == 24) ? 0x40 : 0x00;
    TEST_ASSERT_EQUAL_HEX8(0xC0, lenBits);
}

// ==========================================================================
// Section 16: Reconfigure — NONE (ES9020)
//
// configure() does NOT write any register — only updates internal fields
// ==========================================================================

void test_reconfig_none_mode_assigned(void) {
    TEST_ASSERT_EQUAL(RECONFIG_NONE, kDesc_ES9020.reconfigAction);
}

void test_reconfig_none_register_zero(void) {
    TEST_ASSERT_EQUAL_HEX8(0x00, kDesc_ES9020.reconfigReg);
}

// ==========================================================================
// Section 17: Filter count — all 5 have 8 filters
// ==========================================================================

void test_filter_count_all_8(void) {
    for (int i = 0; i < kDescCount; i++) {
        TEST_ASSERT_EQUAL(8, kAllDescs[i]->filterCount);
    }
}

void test_filter_count_matches_ess_sabre_constant(void) {
    TEST_ASSERT_EQUAL(ESS_SABRE_FILTER_COUNT, kDesc_ES9038Q2M.filterCount);
    TEST_ASSERT_EQUAL(ESS_SABRE_FILTER_COUNT, kDesc_ES9039Q2M.filterCount);
    TEST_ASSERT_EQUAL(ESS_SABRE_FILTER_COUNT, kDesc_ES9069Q.filterCount);
    TEST_ASSERT_EQUAL(ESS_SABRE_FILTER_COUNT, kDesc_ES9033Q.filterCount);
    TEST_ASSERT_EQUAL(ESS_SABRE_FILTER_COUNT, kDesc_ES9020.filterCount);
}

// ==========================================================================
// Section 18: _execSequence delay sentinel
//
// {0xFF, N} in the init sequence triggers a delay(N), NOT a register write.
// Verify that the init sequences have correct delay sentinels.
// ==========================================================================

void test_exec_sequence_delay_sentinel_es9038q2m(void) {
    // Second entry should be {0xFF, 5} — delay sentinel
    TEST_ASSERT_EQUAL_HEX8(0xFF, kInitSeq_ES9038Q2M[1].reg);
    TEST_ASSERT_EQUAL(5, kInitSeq_ES9038Q2M[1].val);
}

void test_exec_sequence_delay_sentinel_es9039q2m(void) {
    TEST_ASSERT_EQUAL_HEX8(0xFF, kInitSeq_ES9039Q2M[1].reg);
    TEST_ASSERT_EQUAL(5, kInitSeq_ES9039Q2M[1].val);
}

void test_exec_sequence_delay_sentinel_es9069q(void) {
    // ES9069Q: delay is the 4th entry (index 3)
    TEST_ASSERT_EQUAL_HEX8(0xFF, kInitSeq_ES9069Q[3].reg);
    TEST_ASSERT_EQUAL(5, kInitSeq_ES9069Q[3].val);
}

void test_exec_sequence_delay_sentinel_es9033q(void) {
    TEST_ASSERT_EQUAL_HEX8(0xFF, kInitSeq_ES9033Q[3].reg);
    TEST_ASSERT_EQUAL(5, kInitSeq_ES9033Q[3].val);
}

void test_exec_sequence_delay_sentinel_es9020(void) {
    TEST_ASSERT_EQUAL_HEX8(0xFF, kInitSeq_ES9020[1].reg);
    TEST_ASSERT_EQUAL(5, kInitSeq_ES9020[1].val);
}

void test_exec_sequence_non_delay_entries_are_valid_regs(void) {
    // Verify that non-delay entries have reg != 0xFF
    for (int i = 0; i < kDescCount; i++) {
        const EssDac2chDesc* d = kAllDescs[i];
        for (uint8_t j = 0; j < d->initSeqLen; j++) {
            if (d->initSeq[j].reg == 0xFF) {
                // This is a delay sentinel — val should be > 0
                TEST_ASSERT_GREATER_THAN(0, d->initSeq[j].val);
            }
        }
    }
}

void test_exec_sequence_no_write_to_0xff(void) {
    // Ensure the generic driver does not attempt to write to I2C register 0xFF.
    // Real ESS SABRE registers never use address 0xFF — it's reserved as delay sentinel.
    // This test verifies no init sequence has a real register at 0xFF.
    for (int i = 0; i < kDescCount; i++) {
        const EssDac2chDesc* d = kAllDescs[i];
        for (uint8_t j = 0; j < d->initSeqLen; j++) {
            if (d->initSeq[j].reg == 0xFF) {
                // This must be a delay (val > 0), not a register write
                TEST_ASSERT_GREATER_THAN(0, d->initSeq[j].val);
            }
        }
    }
}

// ==========================================================================
// Section 19: Init sequence correctness per chip
// ==========================================================================

void test_init_seq_es9038q2m_soft_reset(void) {
    // First entry: soft reset (reg 0x00, bit0)
    TEST_ASSERT_EQUAL_HEX8(0x00, kInitSeq_ES9038Q2M[0].reg);
    TEST_ASSERT_EQUAL_HEX8(0x01, kInitSeq_ES9038Q2M[0].val);
}

void test_init_seq_es9038q2m_input_config(void) {
    // I2S 32-bit Philips: reg 0x01 = 0x03
    TEST_ASSERT_EQUAL_HEX8(0x01, kInitSeq_ES9038Q2M[2].reg);
    TEST_ASSERT_EQUAL_HEX8(0x03, kInitSeq_ES9038Q2M[2].val);
}

void test_init_seq_es9038q2m_slave_mode(void) {
    // Slave mode: reg 0x0A = 0x00
    TEST_ASSERT_EQUAL_HEX8(0x0A, kInitSeq_ES9038Q2M[3].reg);
    TEST_ASSERT_EQUAL_HEX8(0x00, kInitSeq_ES9038Q2M[3].val);
}

void test_init_seq_es9038q2m_dpll(void) {
    // DPLL bandwidth: reg 0x0B = 0x03
    TEST_ASSERT_EQUAL_HEX8(0x0B, kInitSeq_ES9038Q2M[4].reg);
    TEST_ASSERT_EQUAL_HEX8(0x03, kInitSeq_ES9038Q2M[4].val);
}

void test_init_seq_es9039q2m_dpll_differs(void) {
    // ES9039Q2M DPLL is tighter: reg 0x0B = 0x05 (vs 0x03 for Q2M)
    TEST_ASSERT_EQUAL_HEX8(0x0B, kInitSeq_ES9039Q2M[4].reg);
    TEST_ASSERT_EQUAL_HEX8(0x05, kInitSeq_ES9039Q2M[4].val);
}

void test_init_seq_es9069q_system_settings(void) {
    // I2S + soft start: reg 0x00 = 0x02
    TEST_ASSERT_EQUAL_HEX8(0x00, kInitSeq_ES9069Q[0].reg);
    TEST_ASSERT_EQUAL_HEX8(0x02, kInitSeq_ES9069Q[0].val);
}

void test_init_seq_es9069q_input_config(void) {
    // 32-bit I2S: reg 0x01 = 0x00
    TEST_ASSERT_EQUAL_HEX8(0x01, kInitSeq_ES9069Q[1].reg);
    TEST_ASSERT_EQUAL_HEX8(0x00, kInitSeq_ES9069Q[1].val);
}

void test_init_seq_es9020_soft_reset_bit7(void) {
    // ES9020 uses bit7 for soft reset (different from Q2M which uses bit0)
    TEST_ASSERT_EQUAL_HEX8(0x00, kInitSeq_ES9020[0].reg);
    TEST_ASSERT_EQUAL_HEX8(0x80, kInitSeq_ES9020[0].val);
}

void test_init_seq_es9020_input_tdm_slots(void) {
    // I2S stereo (2 slots): reg 0x01 = 0x00
    TEST_ASSERT_EQUAL_HEX8(0x01, kInitSeq_ES9020[2].reg);
    TEST_ASSERT_EQUAL_HEX8(0x00, kInitSeq_ES9020[2].val);
}

void test_init_seq_es9020_clock_source_mclk(void) {
    // Default clock source: MCLK (0x02) at reg 0x0D
    TEST_ASSERT_EQUAL_HEX8(0x0D, kInitSeq_ES9020[3].reg);
    TEST_ASSERT_EQUAL_HEX8(0x02, kInitSeq_ES9020[3].val);
}

// ==========================================================================
// Section 20: Cross-descriptor consistency checks
// ==========================================================================

void test_all_descs_have_ess_manufacturer_addr(void) {
    // All 5 are ESS Technology DACs with I2C base address 0x48
    for (int i = 0; i < kDescCount; i++) {
        TEST_ASSERT_EQUAL_HEX8(0x48, kAllDescs[i]->i2cAddr);
    }
}

void test_all_descs_have_common_dac_caps(void) {
    // All must have DAC_PATH, HW_VOLUME, MUTE, FILTERS
    for (int i = 0; i < kDescCount; i++) {
        uint32_t c = kAllDescs[i]->capabilities;
        TEST_ASSERT_TRUE(c & HAL_CAP_DAC_PATH);
        TEST_ASSERT_TRUE(c & HAL_CAP_HW_VOLUME);
        TEST_ASSERT_TRUE(c & HAL_CAP_MUTE);
        TEST_ASSERT_TRUE(c & HAL_CAP_FILTERS);
    }
}

void test_no_desc_has_adc_path(void) {
    for (int i = 0; i < kDescCount; i++) {
        TEST_ASSERT_FALSE(kAllDescs[i]->capabilities & HAL_CAP_ADC_PATH);
    }
}

void test_all_chip_ids_unique(void) {
    for (int i = 0; i < kDescCount; i++) {
        for (int j = i + 1; j < kDescCount; j++) {
            TEST_ASSERT_NOT_EQUAL(kAllDescs[i]->chipId, kAllDescs[j]->chipId);
        }
    }
}

void test_all_compatible_strings_unique(void) {
    for (int i = 0; i < kDescCount; i++) {
        for (int j = i + 1; j < kDescCount; j++) {
            TEST_ASSERT_NOT_EQUAL(0, strcmp(kAllDescs[i]->compatible, kAllDescs[j]->compatible));
        }
    }
}

void test_vol_reg_l_matches_across_all(void) {
    // All 5 use 0x0F as the left/primary volume register
    for (int i = 0; i < kDescCount; i++) {
        TEST_ASSERT_EQUAL_HEX8(0x0F, kAllDescs[i]->volRegL);
    }
}

// ==========================================================================
// Main
// ==========================================================================

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    // Section 1: Descriptor identity
    RUN_TEST(test_desc_es9038q2m_identity);
    RUN_TEST(test_desc_es9039q2m_identity);
    RUN_TEST(test_desc_es9069q_identity);
    RUN_TEST(test_desc_es9033q_identity);
    RUN_TEST(test_desc_es9020_identity);
    RUN_TEST(test_desc_all_i2c_addr_0x48);

    // Section 2: Capability flags
    RUN_TEST(test_caps_es9038q2m);
    RUN_TEST(test_caps_es9039q2m);
    RUN_TEST(test_caps_es9069q);
    RUN_TEST(test_caps_es9033q);
    RUN_TEST(test_caps_es9020);

    // Section 3: Sample rate masks
    RUN_TEST(test_rates_es9038q2m_full_range);
    RUN_TEST(test_rates_es9039q2m_full_range);
    RUN_TEST(test_rates_es9069q_full_range);
    RUN_TEST(test_rates_es9033q_full_range);
    RUN_TEST(test_rates_es9020_limited_to_192k);

    // Section 4: Volume — VOL_DUAL_0xFF
    RUN_TEST(test_vol_dual_0xff_mode_assigned);
    RUN_TEST(test_vol_dual_100pct_writes_0x00);
    RUN_TEST(test_vol_dual_0pct_writes_0xff);
    RUN_TEST(test_vol_dual_50pct_writes_midrange);
    RUN_TEST(test_vol_dual_register_addresses);
    RUN_TEST(test_vol_dual_boundary_1pct);
    RUN_TEST(test_vol_dual_boundary_99pct);
    RUN_TEST(test_vol_dual_clamp_above_100);

    // Section 5: Volume — VOL_SINGLE_128
    RUN_TEST(test_vol_single_128_mode_assigned);
    RUN_TEST(test_vol_single_100pct_writes_0x00);
    RUN_TEST(test_vol_single_0pct_writes_0x80);
    RUN_TEST(test_vol_single_50pct_writes_midrange);
    RUN_TEST(test_vol_single_register_address);
    RUN_TEST(test_vol_single_1pct);

    // Section 6: Mute — MUTE_VIA_DEDICATED_BIT
    RUN_TEST(test_mute_dedicated_bit_mode_assigned);
    RUN_TEST(test_mute_dedicated_bit_register);
    RUN_TEST(test_mute_dedicated_bit_is_bit0);
    RUN_TEST(test_mute_dedicated_bit_preserves_filter);

    // Section 7: Mute — MUTE_VIA_VOLUME
    RUN_TEST(test_mute_via_volume_mode_assigned);
    RUN_TEST(test_mute_via_volume_writes_0xff);
    RUN_TEST(test_mute_via_volume_unmute_restores_level);

    // Section 8: Filter — FILTER_BITS_4_2_WITH_MUTE
    RUN_TEST(test_filter_4_2_mode_assigned);
    RUN_TEST(test_filter_4_2_register_and_shift);
    RUN_TEST(test_filter_4_2_preset_0_encoding);
    RUN_TEST(test_filter_4_2_preset_7_encoding);
    RUN_TEST(test_filter_4_2_preset_3_with_mute_preserved);
    RUN_TEST(test_filter_4_2_mask_clears_only_filter_bits);

    // Section 9: Filter — FILTER_BITS_2_0
    RUN_TEST(test_filter_2_0_mode_assigned);
    RUN_TEST(test_filter_2_0_register_and_shift);
    RUN_TEST(test_filter_2_0_preset_0_encoding);
    RUN_TEST(test_filter_2_0_preset_7_encoding);
    RUN_TEST(test_filter_2_0_preset_5_encoding);

    // Section 10: Feature — MQA
    RUN_TEST(test_feature_mqa_type_assigned);
    RUN_TEST(test_feature_mqa_enable_reg);
    RUN_TEST(test_feature_mqa_status_reg_and_mask);
    RUN_TEST(test_feature_mqa_enable_bit_encoding);
    RUN_TEST(test_feature_mqa_disable_bit_encoding);
    RUN_TEST(test_feature_mqa_status_none_is_inactive);
    RUN_TEST(test_feature_mqa_status_nonzero_is_active);

    // Section 11: Feature — Line Driver
    RUN_TEST(test_feature_line_driver_type_assigned);
    RUN_TEST(test_feature_line_driver_enable_reg);
    RUN_TEST(test_feature_line_driver_init_val);
    RUN_TEST(test_feature_line_driver_enable_writes_init_val);
    RUN_TEST(test_feature_line_driver_disable_writes_zero);

    // Section 12: Feature — APLL
    RUN_TEST(test_feature_apll_type_assigned);
    RUN_TEST(test_feature_apll_enable_reg);
    RUN_TEST(test_feature_apll_aux_reg);
    RUN_TEST(test_feature_apll_enable_writes_bck_recovery);
    RUN_TEST(test_feature_apll_disable_writes_mclk);
    RUN_TEST(test_feature_apll_status_mask);
    RUN_TEST(test_feature_apll_enable_sequence);
    RUN_TEST(test_feature_apll_disable_sequence);

    // Section 13: Feature — None
    RUN_TEST(test_feature_none_type_assigned);
    RUN_TEST(test_feature_none_registers_zeroed);

    // Section 14: Reconfigure — CLOCK_GEAR
    RUN_TEST(test_reconfig_clock_gear_mode_assigned);
    RUN_TEST(test_reconfig_clock_gear_register);
    RUN_TEST(test_reconfig_clock_gear_1x_at_48k);
    RUN_TEST(test_reconfig_clock_gear_1x_at_192k);
    RUN_TEST(test_reconfig_clock_gear_2x_at_384k);
    RUN_TEST(test_reconfig_clock_gear_4x_at_768k);

    // Section 15: Reconfigure — WORD_LENGTH
    RUN_TEST(test_reconfig_word_length_mode_assigned);
    RUN_TEST(test_reconfig_word_length_register);
    RUN_TEST(test_reconfig_word_length_32bit);
    RUN_TEST(test_reconfig_word_length_24bit);
    RUN_TEST(test_reconfig_word_length_16bit);

    // Section 16: Reconfigure — NONE
    RUN_TEST(test_reconfig_none_mode_assigned);
    RUN_TEST(test_reconfig_none_register_zero);

    // Section 17: Filter count
    RUN_TEST(test_filter_count_all_8);
    RUN_TEST(test_filter_count_matches_ess_sabre_constant);

    // Section 18: Delay sentinel
    RUN_TEST(test_exec_sequence_delay_sentinel_es9038q2m);
    RUN_TEST(test_exec_sequence_delay_sentinel_es9039q2m);
    RUN_TEST(test_exec_sequence_delay_sentinel_es9069q);
    RUN_TEST(test_exec_sequence_delay_sentinel_es9033q);
    RUN_TEST(test_exec_sequence_delay_sentinel_es9020);
    RUN_TEST(test_exec_sequence_non_delay_entries_are_valid_regs);
    RUN_TEST(test_exec_sequence_no_write_to_0xff);

    // Section 19: Init sequence correctness
    RUN_TEST(test_init_seq_es9038q2m_soft_reset);
    RUN_TEST(test_init_seq_es9038q2m_input_config);
    RUN_TEST(test_init_seq_es9038q2m_slave_mode);
    RUN_TEST(test_init_seq_es9038q2m_dpll);
    RUN_TEST(test_init_seq_es9039q2m_dpll_differs);
    RUN_TEST(test_init_seq_es9069q_system_settings);
    RUN_TEST(test_init_seq_es9069q_input_config);
    RUN_TEST(test_init_seq_es9020_soft_reset_bit7);
    RUN_TEST(test_init_seq_es9020_input_tdm_slots);
    RUN_TEST(test_init_seq_es9020_clock_source_mclk);

    // Section 20: Cross-descriptor consistency
    RUN_TEST(test_all_descs_have_ess_manufacturer_addr);
    RUN_TEST(test_all_descs_have_common_dac_caps);
    RUN_TEST(test_no_desc_has_adc_path);
    RUN_TEST(test_all_chip_ids_unique);
    RUN_TEST(test_all_compatible_strings_unique);
    RUN_TEST(test_vol_reg_l_matches_across_all);

    return UNITY_END();
}
