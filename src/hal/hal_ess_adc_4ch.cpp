#ifdef DAC_ENABLED
// HalEssAdc4ch — Generic ESS SABRE 4-channel TDM ADC driver implementation.
// Descriptor tables for ES9843PRO, ES9842PRO, ES9841, ES9840.
// All 4 chips share the same TDM init/volume/mute/filter/gain logic driven
// by per-chip EssAdc4chDescriptor structs.

#include "hal_ess_adc_4ch.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include <Arduino.h>
#include "../debug_serial.h"
#include "../i2s_audio.h"
#else
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)
// I2S TDM port stubs — skipped if the test translation unit provides them
#ifndef TDM_TEST_PROVIDES_STUBS
inline uint32_t i2s_audio_get_sample_rate(void) { return 48000; }
#endif
#endif

// ===========================================================================
// Register constants replicated for native compilation
// ===========================================================================

#define DELAY_SENTINEL  0xFF

// ----- ES9843PRO (chip ID 0x8F) -----
#define ES9843PRO_I2C_ADDR                0x40
#define ES9843PRO_CHIP_ID                 0x8F
#define ES9843PRO_REG_SYS_CONFIG          0x00
#define ES9843PRO_REG_OUTPUT_FORMAT       0x03
#define ES9843PRO_REG_FILTER_CONFIG       0x4A
#define ES9843PRO_REG_DC_BLOCK            0x4C
#define ES9843PRO_REG_DC_BLOCK_MSB        0x4D
#define ES9843PRO_REG_CH1_VOLUME          0x51
#define ES9843PRO_REG_CH2_VOLUME          0x52
#define ES9843PRO_REG_CH3_VOLUME          0x53
#define ES9843PRO_REG_CH4_VOLUME          0x54
#define ES9843PRO_REG_GAIN_PAIR1          0x55
#define ES9843PRO_REG_GAIN_PAIR2          0x56
#define ES9843PRO_REG_ASP_CONTROL         0x6B
#define ES9843PRO_REG_ASP_BYPASS          0x6C
#define ES9843PRO_REG_CHIP_ID             0xE1
#define ES9843PRO_SOFT_RESET_CMD          0xA0
#define ES9843PRO_ENABLE_4CH              0x0F
#define ES9843PRO_OUTPUT_TDM              0x10
#define ES9843PRO_ASP_BYPASS_ALL          0x0F
#define ES9843PRO_VOL_0DB                 0x00
#define ES9843PRO_VOL_MUTE                0xFF

// ----- ES9842PRO (chip ID 0x83) -----
#define ES9842PRO_I2C_ADDR                0x40
#define ES9842PRO_CHIP_ID                 0x83
#define ES9842PRO_REG_SYS_CONFIG          0x00
#define ES9842PRO_REG_CHIP_ID             0xE1
#define ES9842PRO_REG_CH1_DC_BLOCKING     0x65
#define ES9842PRO_REG_CH1_VOLUME_LSB      0x6D
#define ES9842PRO_REG_CH1_GAIN            0x70
#define ES9842PRO_REG_CH1_FILTER          0x71
#define ES9842PRO_REG_CH2_DC_BLOCKING     0x76
#define ES9842PRO_REG_CH2_VOLUME_LSB      0x7E
#define ES9842PRO_REG_CH2_GAIN            0x81
#define ES9842PRO_REG_CH2_FILTER          0x82
#define ES9842PRO_REG_CH3_DC_BLOCKING     0x87
#define ES9842PRO_REG_CH3_VOLUME_LSB      0x8F
#define ES9842PRO_REG_CH3_GAIN            0x92
#define ES9842PRO_REG_CH3_FILTER          0x93
#define ES9842PRO_REG_CH4_DC_BLOCKING     0x98
#define ES9842PRO_REG_CH4_VOLUME_LSB      0xA0
#define ES9842PRO_REG_CH4_GAIN            0xA3
#define ES9842PRO_REG_CH4_FILTER          0xA4
#define ES9842PRO_OUTPUT_TDM              0x40
#define ES9842PRO_SOFT_RESET_CMD          0x80
#define ES9842PRO_HPF_ENABLE_BIT          0x04
#define ES9842PRO_GAIN_MASK               0x03
#define ES9842PRO_FILTER_MASK             0x1C
#define ES9842PRO_FILTER_SHIFT            2

// ----- ES9841 (chip ID 0x91) -----
#define ES9841_I2C_ADDR                   0x40
#define ES9841_CHIP_ID                    0x91
#define ES9841_REG_SYS_CONFIG             0x00
#define ES9841_REG_FILTER_CONFIG          0x4A
#define ES9841_REG_CH1_VOLUME             0x51
#define ES9841_REG_GAIN_PAIR1             0x55
#define ES9841_REG_GAIN_PAIR2             0x56
#define ES9841_REG_CH1_DC_BLOCKING        0x65
#define ES9841_REG_CH2_DC_BLOCKING        0x76
#define ES9841_REG_CH3_DC_BLOCKING        0x87
#define ES9841_REG_CH4_DC_BLOCKING        0x98
#define ES9841_REG_CHIP_ID                0xE1
#define ES9841_OUTPUT_TDM                 0x40
#define ES9841_SOFT_RESET_CMD             0x80
#define ES9841_HPF_ENABLE_BIT             0x04
#define ES9841_FILTER_MASK                0xE0
#define ES9841_FILTER_SHIFT               5
#define ES9841_VOL_0DB                    0xFF
#define ES9841_VOL_MUTE                   0x00

// ----- ES9840 (chip ID 0x87) — identical register map to ES9842PRO -----
#define ES9840_I2C_ADDR                   0x40
#define ES9840_CHIP_ID                    0x87
// ES9840 shares all register addresses with ES9842PRO (same silicon family)

// ===========================================================================
// Init/deinit sequences
// ===========================================================================

// ----- ES9843PRO -----
static const EssAdc4chRegWrite kInitSeqES9843PRO[] = {
    { ES9843PRO_REG_SYS_CONFIG,    ES9843PRO_SOFT_RESET_CMD   },  // soft reset + EN_MCLK_IN
    { DELAY_SENTINEL, 1                                        },  // delay 5ms
    // chip ID read is done separately in init() after reset
    { ES9843PRO_REG_SYS_CONFIG,    ES9843PRO_ENABLE_4CH       },  // enable 4 ADC channels
    { ES9843PRO_REG_OUTPUT_FORMAT, ES9843PRO_OUTPUT_TDM       },  // TDM output mode
    { ES9843PRO_REG_DC_BLOCK,      0x00                       },  // DC block LSB = default
    { ES9843PRO_REG_ASP_CONTROL,   0x00                       },  // disable ASP2
    { ES9843PRO_REG_ASP_BYPASS,    ES9843PRO_ASP_BYPASS_ALL   },  // bypass all channels
};
static const EssAdc4chRegWrite kDeinitSeqES9843PRO[] = {
    { ES9843PRO_REG_SYS_CONFIG, 0x00 },  // disable all ADC channels
};

// ----- ES9842PRO -----
static const EssAdc4chRegWrite kInitSeqES9842PRO[] = {
    { ES9842PRO_REG_SYS_CONFIG, ES9842PRO_SOFT_RESET_CMD },  // soft reset
    { DELAY_SENTINEL, 1                                  },  // delay 5ms
    { ES9842PRO_REG_SYS_CONFIG, ES9842PRO_OUTPUT_TDM    },  // TDM output
};
static const EssAdc4chRegWrite kDeinitSeqES9842PRO[] = {
    { ES9842PRO_REG_SYS_CONFIG, 0x00 },  // disable TDM output
};

// ----- ES9841 -----
static const EssAdc4chRegWrite kInitSeqES9841[] = {
    { ES9841_REG_SYS_CONFIG, ES9841_SOFT_RESET_CMD },  // soft reset
    { DELAY_SENTINEL, 1                            },  // delay 5ms
    { ES9841_REG_SYS_CONFIG, ES9841_OUTPUT_TDM    },  // TDM output
};
static const EssAdc4chRegWrite kDeinitSeqES9841[] = {
    { ES9841_REG_SYS_CONFIG, 0x00 },
};

// ----- ES9840 — identical to ES9842PRO init sequence -----
static const EssAdc4chRegWrite kInitSeqES9840[] = {
    { ES9842PRO_REG_SYS_CONFIG, ES9842PRO_SOFT_RESET_CMD },
    { DELAY_SENTINEL, 1                                  },
    { ES9842PRO_REG_SYS_CONFIG, ES9842PRO_OUTPUT_TDM    },
};
static const EssAdc4chRegWrite kDeinitSeqES9840[] = {
    { ES9842PRO_REG_SYS_CONFIG, 0x00 },
};

// ===========================================================================
// Supported sample rate table (shared)
// ===========================================================================
static const uint32_t kAdc4chRates4[] = { 44100, 48000, 96000, 192000 };

// ===========================================================================
// Descriptor tables
// ===========================================================================

const EssAdc4chDescriptor kDescES9843PRO = {
    /* compatible          */ "ess,es9843pro",
    /* chipName            */ "ES9843PRO",
    /* chipId              */ ES9843PRO_CHIP_ID,
    /* capabilities        */ (uint32_t)(HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL),
    /* sampleRateMask      */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K,
    /* supportedRates      */ kAdc4chRates4,
    /* supportedRateCount  */ 4,
    /* regChipId           */ ES9843PRO_REG_CHIP_ID,
    /* regSysConfig        */ ES9843PRO_REG_SYS_CONFIG,
    /* softResetVal        */ ES9843PRO_SOFT_RESET_CMD,
    /* tdmEnableVal        */ ES9843PRO_OUTPUT_TDM,
    /* volType             */ VOL4_8BIT,
    /* regVolCh1           */ ES9843PRO_REG_CH1_VOLUME,
    /* gainType            */ GAIN4_3BIT_6DB,
    /* gainPack            */ GAIN4_PACK_3_5,
    /* regGainPair1        */ ES9843PRO_REG_GAIN_PAIR1,
    /* regGainPair2        */ ES9843PRO_REG_GAIN_PAIR2,
    /* gainMax             */ 42,
    /* gainMask            */ 0x07,
    /* regHpfCh1           */ ES9843PRO_REG_DC_BLOCK_MSB,  // special: MSB reg, bits[7:4]
    /* regHpfCh2           */ 0xFF,  // same register for all channels (MSB)
    /* regHpfCh3           */ 0xFF,
    /* regHpfCh4           */ 0xFF,
    /* hpfEnableBit        */ 0xF0,  // bits[7:4] = all 4 channels
    /* filterType          */ FILTER4_GLOBAL_SHIFT5,
    /* regFilter           */ ES9843PRO_REG_FILTER_CONFIG,
    /* regFilterCh2        */ 0xFF,
    /* regFilterCh3        */ 0xFF,
    /* regFilterCh4        */ 0xFF,
    /* filterMask          */ 0xE0,  // bits[7:5]
    /* sourceNameA         */ "ES9843PRO CH1/2",
    /* sourceNameB         */ "ES9843PRO CH3/4",
    /* initSeq             */ kInitSeqES9843PRO,
    /* initSeqLen          */ (uint8_t)(sizeof(kInitSeqES9843PRO) / sizeof(kInitSeqES9843PRO[0])),
    /* deinitSeq           */ kDeinitSeqES9843PRO,
    /* deinitSeqLen        */ (uint8_t)(sizeof(kDeinitSeqES9843PRO) / sizeof(kDeinitSeqES9843PRO[0])),
    /* logPrefix           */ "[HAL:ES9843PRO]",
};

const EssAdc4chDescriptor kDescES9842PRO = {
    /* compatible          */ "ess,es9842pro",
    /* chipName            */ "ES9842PRO",
    /* chipId              */ ES9842PRO_CHIP_ID,
    /* capabilities        */ (uint32_t)(HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL),
    /* sampleRateMask      */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K,
    /* supportedRates      */ kAdc4chRates4,
    /* supportedRateCount  */ 4,
    /* regChipId           */ ES9842PRO_REG_CHIP_ID,
    /* regSysConfig        */ ES9842PRO_REG_SYS_CONFIG,
    /* softResetVal        */ ES9842PRO_SOFT_RESET_CMD,
    /* tdmEnableVal        */ ES9842PRO_OUTPUT_TDM,
    /* volType             */ VOL4_16BIT,
    /* regVolCh1           */ ES9842PRO_REG_CH1_VOLUME_LSB,
    /* gainType            */ GAIN4_2BIT_6DB,
    /* gainPack            */ GAIN4_PACK_3_5,  // n/a for 2-bit
    /* regGainPair1        */ ES9842PRO_REG_CH1_GAIN,
    /* regGainPair2        */ ES9842PRO_REG_CH2_GAIN,
    /* gainMax             */ 18,
    /* gainMask            */ ES9842PRO_GAIN_MASK,
    /* regHpfCh1           */ ES9842PRO_REG_CH1_DC_BLOCKING,
    /* regHpfCh2           */ ES9842PRO_REG_CH2_DC_BLOCKING,
    /* regHpfCh3           */ ES9842PRO_REG_CH3_DC_BLOCKING,
    /* regHpfCh4           */ ES9842PRO_REG_CH4_DC_BLOCKING,
    /* hpfEnableBit        */ ES9842PRO_HPF_ENABLE_BIT,
    /* filterType          */ FILTER4_PER_CH_SHIFT2,
    /* regFilter           */ ES9842PRO_REG_CH1_FILTER,
    /* regFilterCh2        */ ES9842PRO_REG_CH2_FILTER,
    /* regFilterCh3        */ ES9842PRO_REG_CH3_FILTER,
    /* regFilterCh4        */ ES9842PRO_REG_CH4_FILTER,
    /* filterMask          */ ES9842PRO_FILTER_MASK,
    /* sourceNameA         */ "ES9842PRO CH1/2",
    /* sourceNameB         */ "ES9842PRO CH3/4",
    /* initSeq             */ kInitSeqES9842PRO,
    /* initSeqLen          */ (uint8_t)(sizeof(kInitSeqES9842PRO) / sizeof(kInitSeqES9842PRO[0])),
    /* deinitSeq           */ kDeinitSeqES9842PRO,
    /* deinitSeqLen        */ (uint8_t)(sizeof(kDeinitSeqES9842PRO) / sizeof(kDeinitSeqES9842PRO[0])),
    /* logPrefix           */ "[HAL:ES9842PRO]",
};

const EssAdc4chDescriptor kDescES9841 = {
    /* compatible          */ "ess,es9841",
    /* chipName            */ "ES9841",
    /* chipId              */ ES9841_CHIP_ID,
    /* capabilities        */ (uint32_t)(HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL),
    /* sampleRateMask      */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K,
    /* supportedRates      */ kAdc4chRates4,
    /* supportedRateCount  */ 4,
    /* regChipId           */ ES9841_REG_CHIP_ID,
    /* regSysConfig        */ ES9841_REG_SYS_CONFIG,
    /* softResetVal        */ ES9841_SOFT_RESET_CMD,
    /* tdmEnableVal        */ ES9841_OUTPUT_TDM,
    /* volType             */ VOL4_8BIT_INV,
    /* regVolCh1           */ ES9841_REG_CH1_VOLUME,
    /* gainType            */ GAIN4_3BIT_6DB,
    /* gainPack            */ GAIN4_PACK_0_4,
    /* regGainPair1        */ ES9841_REG_GAIN_PAIR1,
    /* regGainPair2        */ ES9841_REG_GAIN_PAIR2,
    /* gainMax             */ 42,
    /* gainMask            */ 0x07,
    /* regHpfCh1           */ ES9841_REG_CH1_DC_BLOCKING,
    /* regHpfCh2           */ ES9841_REG_CH2_DC_BLOCKING,
    /* regHpfCh3           */ ES9841_REG_CH3_DC_BLOCKING,
    /* regHpfCh4           */ ES9841_REG_CH4_DC_BLOCKING,
    /* hpfEnableBit        */ ES9841_HPF_ENABLE_BIT,
    /* filterType          */ FILTER4_GLOBAL_SHIFT5,
    /* regFilter           */ ES9841_REG_FILTER_CONFIG,
    /* regFilterCh2        */ 0xFF,
    /* regFilterCh3        */ 0xFF,
    /* regFilterCh4        */ 0xFF,
    /* filterMask          */ ES9841_FILTER_MASK,
    /* sourceNameA         */ "ES9841 CH1/2",
    /* sourceNameB         */ "ES9841 CH3/4",
    /* initSeq             */ kInitSeqES9841,
    /* initSeqLen          */ (uint8_t)(sizeof(kInitSeqES9841) / sizeof(kInitSeqES9841[0])),
    /* deinitSeq           */ kDeinitSeqES9841,
    /* deinitSeqLen        */ (uint8_t)(sizeof(kDeinitSeqES9841) / sizeof(kDeinitSeqES9841[0])),
    /* logPrefix           */ "[HAL:ES9841]",
};

const EssAdc4chDescriptor kDescES9840 = {
    /* compatible          */ "ess,es9840",
    /* chipName            */ "ES9840",
    /* chipId              */ ES9840_CHIP_ID,
    /* capabilities        */ (uint32_t)(HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL),
    /* sampleRateMask      */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K,
    /* supportedRates      */ kAdc4chRates4,
    /* supportedRateCount  */ 4,
    /* regChipId           */ ES9842PRO_REG_CHIP_ID,  // same register map as ES9842PRO
    /* regSysConfig        */ ES9842PRO_REG_SYS_CONFIG,
    /* softResetVal        */ ES9842PRO_SOFT_RESET_CMD,
    /* tdmEnableVal        */ ES9842PRO_OUTPUT_TDM,
    /* volType             */ VOL4_16BIT,
    /* regVolCh1           */ ES9842PRO_REG_CH1_VOLUME_LSB,
    /* gainType            */ GAIN4_2BIT_6DB,
    /* gainPack            */ GAIN4_PACK_3_5,
    /* regGainPair1        */ ES9842PRO_REG_CH1_GAIN,
    /* regGainPair2        */ ES9842PRO_REG_CH2_GAIN,
    /* gainMax             */ 18,
    /* gainMask            */ ES9842PRO_GAIN_MASK,
    /* regHpfCh1           */ ES9842PRO_REG_CH1_DC_BLOCKING,
    /* regHpfCh2           */ ES9842PRO_REG_CH2_DC_BLOCKING,
    /* regHpfCh3           */ ES9842PRO_REG_CH3_DC_BLOCKING,
    /* regHpfCh4           */ ES9842PRO_REG_CH4_DC_BLOCKING,
    /* hpfEnableBit        */ ES9842PRO_HPF_ENABLE_BIT,
    /* filterType          */ FILTER4_PER_CH_SHIFT2,
    /* regFilter           */ ES9842PRO_REG_CH1_FILTER,
    /* regFilterCh2        */ ES9842PRO_REG_CH2_FILTER,
    /* regFilterCh3        */ ES9842PRO_REG_CH3_FILTER,
    /* regFilterCh4        */ ES9842PRO_REG_CH4_FILTER,
    /* filterMask          */ ES9842PRO_FILTER_MASK,
    /* sourceNameA         */ "ES9840 CH1/2",
    /* sourceNameB         */ "ES9840 CH3/4",
    /* initSeq             */ kInitSeqES9840,
    /* initSeqLen          */ (uint8_t)(sizeof(kInitSeqES9840) / sizeof(kInitSeqES9840[0])),
    /* deinitSeq           */ kDeinitSeqES9840,
    /* deinitSeqLen        */ (uint8_t)(sizeof(kDeinitSeqES9840) / sizeof(kDeinitSeqES9840[0])),
    /* logPrefix           */ "[HAL:ES9840]",
};

// ===========================================================================
// HalEssAdc4ch — Generic implementation
// ===========================================================================

HalEssAdc4ch::HalEssAdc4ch(const EssAdc4chDescriptor& desc)
    : HalEssSabreAdcBase(), _desc(desc)
{
    hal_init_descriptor(_descriptor, desc.compatible, desc.chipName, "ESS Technology",
        HAL_DEV_ADC, 4, ES9843PRO_I2C_ADDR /* default; overridden by _applyConfigOverrides */,
        HAL_BUS_I2C, HAL_I2C_BUS_EXP,
        desc.sampleRateMask, desc.capabilities);
    _initPriority = HAL_PRIORITY_HARDWARE;
}

// ---------------------------------------------------------------------------
// _execSequence
// ---------------------------------------------------------------------------
void HalEssAdc4ch::_execSequence(const EssAdc4chRegWrite* seq, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        if (seq[i].reg == DELAY_SENTINEL) {
#ifndef NATIVE_TEST
            delay((uint32_t)seq[i].val * 5);
#endif
        } else {
            _writeReg(seq[i].reg, seq[i].val);
        }
    }
}

// ---------------------------------------------------------------------------
// _writeGainRegs
// ---------------------------------------------------------------------------
void HalEssAdc4ch::_writeGainRegs(uint8_t gainStep) {
    if (_desc.gainType == GAIN4_2BIT_6DB) {
        // Separate per-channel 2-bit registers
        // pair1 = CH1, pair2 = CH2 (ES9842PRO/ES9840 layout: CH1@0x70, CH2@0x81, CH3@0x92, CH4@0xA3)
        uint8_t val = gainStep & _desc.gainMask;
        _writeReg(_desc.regGainPair1, val);            // CH1
        _writeReg(_desc.regGainPair2, val);            // CH2
        // CH3 and CH4 are at fixed offsets relative to CH2 on ES9842PRO/ES9840
        _writeReg((uint8_t)(_desc.regGainPair2 + 0x11), val);  // CH3 = 0x92
        _writeReg((uint8_t)(_desc.regGainPair2 + 0x22), val);  // CH4 = 0xA3
    } else {
        // GAIN4_3BIT_6DB — packed pairs in 2 registers
        uint8_t gainVal = gainStep & 0x07;
        uint8_t pair1, pair2;
        if (_desc.gainPack == GAIN4_PACK_3_5) {
            // ES9843PRO: bits[2:0]=CH1, bits[5:3]=CH2
            pair1 = (uint8_t)((gainVal) | (gainVal << 3));
            pair2 = (uint8_t)((gainVal) | (gainVal << 3));
        } else {
            // ES9841: bits[2:0]=CH1, bits[6:4]=CH2
            pair1 = (uint8_t)((gainVal) | (gainVal << 4));
            pair2 = (uint8_t)((gainVal) | (gainVal << 4));
        }
        _writeReg(_desc.regGainPair1, pair1);
        _writeReg(_desc.regGainPair2, pair2);
    }
}

// ---------------------------------------------------------------------------
// _writeHpfRegs
// ---------------------------------------------------------------------------
void HalEssAdc4ch::_writeHpfRegs(bool en) {
    if (_desc.regHpfCh2 == 0xFF) {
        // ES9843PRO: single MSB register controls all 4 channels
        uint8_t val = en ? _desc.hpfEnableBit : 0x00;
        _writeReg(_desc.regHpfCh1, val);
    } else {
        // Per-channel DC blocking registers (ES9842PRO, ES9840, ES9841)
        uint8_t val = en ? _desc.hpfEnableBit : 0x00;
        _writeReg(_desc.regHpfCh1, val);
        _writeReg(_desc.regHpfCh2, val);
        _writeReg(_desc.regHpfCh3, val);
        _writeReg(_desc.regHpfCh4, val);
    }
}

// ---------------------------------------------------------------------------
// _writeFilterRegs
// ---------------------------------------------------------------------------
void HalEssAdc4ch::_writeFilterRegs(uint8_t preset) {
    if (_desc.filterType == FILTER4_GLOBAL_SHIFT5) {
        // Single global register, bits[7:5] — ES9843PRO / ES9841
        uint8_t cur = _readReg(_desc.regFilter);
        uint8_t val = (uint8_t)((cur & ~_desc.filterMask) |
                                ((uint8_t)((preset & 0x07) << ES9841_FILTER_SHIFT) & _desc.filterMask));
        _writeReg(_desc.regFilter, val);
    } else {
        // Per-channel filter registers, bits[4:2] — ES9842PRO / ES9840
        uint8_t filterVal = (uint8_t)((preset << ES9842PRO_FILTER_SHIFT) & _desc.filterMask);
        uint8_t cur1 = _readReg(_desc.regFilter);
        _writeReg(_desc.regFilter,    (uint8_t)((cur1 & ~_desc.filterMask) | filterVal));
        uint8_t cur2 = _readReg(_desc.regFilterCh2);
        _writeReg(_desc.regFilterCh2, (uint8_t)((cur2 & ~_desc.filterMask) | filterVal));
        uint8_t cur3 = _readReg(_desc.regFilterCh3);
        _writeReg(_desc.regFilterCh3, (uint8_t)((cur3 & ~_desc.filterMask) | filterVal));
        uint8_t cur4 = _readReg(_desc.regFilterCh4);
        _writeReg(_desc.regFilterCh4, (uint8_t)((cur4 & ~_desc.filterMask) | filterVal));
    }
}

// ---------------------------------------------------------------------------
// _writeVol8Regs
// ---------------------------------------------------------------------------
void HalEssAdc4ch::_writeVol8Regs(uint8_t vol) {
    _writeReg(_desc.regVolCh1,       vol);
    _writeReg((uint8_t)(_desc.regVolCh1 + 1), vol);
    _writeReg((uint8_t)(_desc.regVolCh1 + 2), vol);
    _writeReg((uint8_t)(_desc.regVolCh1 + 3), vol);
}

// ---------------------------------------------------------------------------
// _writeVol16Regs
// ---------------------------------------------------------------------------
void HalEssAdc4ch::_writeVol16Regs(uint16_t vol) {
    // ES9842PRO / ES9840 have non-consecutive 16-bit volume regs
    // CH1: regVolCh1, CH2: 0x7E, CH3: 0x8F, CH4: 0xA0
    // These follow the same non-consecutive layout: +0x11, +0x11 from CH1 pair base
    _writeReg16(_desc.regVolCh1, vol);                             // CH1 = 0x6D
    _writeReg16((uint8_t)(_desc.regVolCh1 + 0x11), vol);          // CH2 = 0x7E
    _writeReg16((uint8_t)(_desc.regVolCh1 + 0x22), vol);          // CH3 = 0x8F
    _writeReg16((uint8_t)(_desc.regVolCh1 + 0x33), vol);          // CH4 = 0xA0
}

// ---------------------------------------------------------------------------
// probe
// ---------------------------------------------------------------------------
bool HalEssAdc4ch::probe() {
#ifndef NATIVE_TEST
    if (!_bus().probe(_i2cAddr)) return false;
    uint8_t chipId = _readReg(_desc.regChipId);
    return (chipId == _desc.chipId);
#else
    return true;
#endif
}

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------
HalInitResult HalEssAdc4ch::init() {
    // 1. Read per-device config overrides
    _applyConfigOverrides();
    if (_gainDb > _desc.gainMax) _gainDb = _desc.gainMax;

    LOG_I("%s Initializing (I2C addr=0x%02X bus=%u SDA=%d SCL=%d sr=%luHz bits=%u)",
          _desc.logPrefix, _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth);

#ifndef NATIVE_TEST
    // 2+3. Select and initialize TwoWire instance
    _selectWire();
    LOG_I("%s I2C initialized (bus %u SDA=%d SCL=%d 400kHz)",
          _desc.logPrefix, _i2cBusIndex, _sdaPin, _sclPin);
#endif

    // 4. Execute init sequence (soft reset, TDM mode, etc.)
    _execSequence(_desc.initSeq, _desc.initSeqLen);

    // 5. Verify chip ID (after reset)
    uint8_t chipId = _readReg(_desc.regChipId);
    if (chipId != _desc.chipId) {
        LOG_W("%s Unexpected chip ID: 0x%02X (expected 0x%02X) — continuing",
              _desc.logPrefix, chipId, _desc.chipId);
    } else {
        LOG_I("%s Chip ID OK (0x%02X)", _desc.logPrefix, chipId);
    }

    // 6. Set all channels to default volume (0 dB)
    if (_desc.volType == VOL4_16BIT) {
        _writeVol16Regs(0x7FFF);
    } else if (_desc.volType == VOL4_8BIT) {
        // 0x00 = 0 dB on ES9843PRO
        _writeVol8Regs(ES9843PRO_VOL_0DB);
    } else {
        // VOL4_8BIT_INV: 0xFF = 0 dB on ES9841
        _savedVol8 = ES9841_VOL_0DB;
        _writeVol8Regs(ES9841_VOL_0DB);
    }

    // 7. Set PGA gain
    {
        uint8_t gainStep = _gainDb / 6;
        if (_desc.gainType == GAIN4_2BIT_6DB && gainStep > 3) gainStep = 3;
        if (_desc.gainType == GAIN4_3BIT_6DB && gainStep > 7) gainStep = 7;
        _writeGainRegs(gainStep);
        _gainDb = (uint8_t)(gainStep * 6);
    }

    // 8. HPF (DC blocking)
    _writeHpfRegs(_hpfEnabled);

    // 9. Filter preset
    {
        uint8_t preset = (_filterPreset > 7) ? 7 : _filterPreset;
        _writeFilterRegs(preset);
    }

    // 10. Get I2S port config and init TDM deinterleaver
    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);
    uint8_t port = (cfg && cfg->valid && cfg->i2sPort != 255) ? cfg->i2sPort : 2;
    int8_t dinPinRaw = (cfg && cfg->valid && cfg->pinData > 0) ? cfg->pinData : -1;
    if (dinPinRaw < 0) {
        LOG_W("%s No DATA_IN pin configured — set pinData in HAL config", _desc.logPrefix);
        dinPinRaw = 0;
    }

#ifndef NATIVE_TEST
    bool tdmOk = i2s_port_enable_rx((uint8_t)port, I2S_MODE_TDM, 4,
                                     (gpio_num_t)dinPinRaw,
                                     GPIO_NUM_NC,
                                     (gpio_num_t)I2S_BCK_PIN,
                                     (gpio_num_t)I2S_LRC_PIN);
    if (!tdmOk) {
        LOG_E("%s I2S TDM init failed (port=%u) — audio will be silent", _desc.logPrefix, port);
    }
#endif

    // 11. Allocate deinterleaver ping-pong buffers (PSRAM preferred)
    if (!_tdm.init((uint8_t)port)) {
        LOG_E("%s TDM deinterleaver init failed — out of memory", _desc.logPrefix);
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "TDM deinterleaver alloc failed");
    }

    // 12. Build the two AudioInputSource structs from the deinterleaver
    _tdm.buildSources(_desc.sourceNameA, _desc.sourceNameB, &_srcA, &_srcB);

    // 13. Mark device ready
    _initialized = true;
    _state = HAL_STATE_AVAILABLE;
    setReady(true);

    LOG_I("%s Ready — TDM mode, port=%u DIN=GPIO%d gain=%ddB hpf=%d filter=%u",
          _desc.logPrefix, port, dinPinRaw, _gainDb, (int)_hpfEnabled, _filterPreset);
    LOG_I("%s Registered sources: '%s' (pair A) + '%s' (pair B)",
          _desc.logPrefix, _desc.sourceNameA, _desc.sourceNameB);
    return hal_init_ok();
}

// ---------------------------------------------------------------------------
// deinit
// ---------------------------------------------------------------------------
void HalEssAdc4ch::deinit() {
    if (!_initialized) return;

    setReady(false);

#ifndef NATIVE_TEST
    _execSequence(_desc.deinitSeq, _desc.deinitSeqLen);

    // Release I2S expansion TDM RX via port-generic API
    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(getSlot());
    uint8_t port = (cfg && cfg->valid && cfg->i2sPort != 255) ? cfg->i2sPort : 2;
    i2s_port_disable_rx(port);
#endif

    // Release deinterleaver ping-pong buffers
    _tdm.deinit();

    _initialized = false;
    _state       = HAL_STATE_REMOVED;

    LOG_I("%s Deinitialized (TDM + I2S released)", _desc.logPrefix);
}

// ---------------------------------------------------------------------------
// dumpConfig
// ---------------------------------------------------------------------------
void HalEssAdc4ch::dumpConfig() {
    LOG_I("%s %s by ESS Technology (compat=%s) i2c=0x%02X bus=%u sda=%d scl=%d "
          "sr=%luHz bits=%u gain=%ddB hpf=%d filter=%u",
          _desc.logPrefix,
          _descriptor.name, _descriptor.compatible,
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth, _gainDb,
          (int)_hpfEnabled, _filterPreset);
}

// ---------------------------------------------------------------------------
// healthCheck
// ---------------------------------------------------------------------------
bool HalEssAdc4ch::healthCheck() {
#ifndef NATIVE_TEST
    if (!_initialized) return false;
    uint8_t id = _readReg(_desc.regChipId);
    return (id == _desc.chipId);
#else
    return _initialized;
#endif
}

// ---------------------------------------------------------------------------
// configure
// ---------------------------------------------------------------------------
bool HalEssAdc4ch::configure(uint32_t sampleRate, uint8_t bitDepth) {
    if (!_validateSampleRate(sampleRate, _desc.supportedRates, _desc.supportedRateCount)) {
        LOG_W("%s Unsupported sample rate: %luHz", _desc.logPrefix, (unsigned long)sampleRate);
        return false;
    }
    _sampleRate = sampleRate;
    _bitDepth   = bitDepth;
    LOG_I("%s Configured: %luHz %ubit", _desc.logPrefix, (unsigned long)sampleRate, bitDepth);
    return true;
}

// ---------------------------------------------------------------------------
// setVolume
// ---------------------------------------------------------------------------
bool HalEssAdc4ch::setVolume(uint8_t percent) {
    if (!_initialized) return false;
    if (percent > 100) percent = 100;

    if (_desc.volType == VOL4_16BIT) {
        uint16_t vol16 = (percent == 0) ? 0x0000 :
                         (percent == 100) ? 0x7FFF :
                         (uint16_t)(((uint32_t)percent * 0x7FFF) / 100);
        _writeVol16Regs(vol16);
        LOG_D("%s Volume: %d%% -> 0x%04X", _desc.logPrefix, percent, vol16);
    } else if (_desc.volType == VOL4_8BIT) {
        // ES9843PRO: 0x00=0dB, 0xFF=mute; linear scale
        uint8_t vol8 = (percent == 0) ? 0xFF :
                       (percent == 100) ? 0x00 :
                       (uint8_t)(0xFE - (uint8_t)(((uint32_t)(percent - 1) * 0xFE) / 99));
        _writeVol8Regs(vol8);
        LOG_D("%s Volume: %d%% -> 0x%02X", _desc.logPrefix, percent, vol8);
    } else {
        // VOL4_8BIT_INV: ES9841: 0xFF=0dB, 0x00=mute; linear scale
        uint8_t vol8 = (percent == 0) ? 0x00 :
                       (percent == 100) ? 0xFF :
                       (uint8_t)(((uint32_t)percent * 0xFF) / 100);
        _savedVol8 = vol8;
        _writeVol8Regs(vol8);
        LOG_D("%s Volume: %d%% -> 0x%02X", _desc.logPrefix, percent, vol8);
    }
    return true;
}

// ---------------------------------------------------------------------------
// setMute
// ---------------------------------------------------------------------------
bool HalEssAdc4ch::setMute(bool mute) {
    if (!_initialized) return false;

    if (_desc.volType == VOL4_16BIT) {
        uint16_t vol = mute ? 0x0000 : 0x7FFF;
        _writeVol16Regs(vol);
    } else if (_desc.volType == VOL4_8BIT) {
        uint8_t vol = mute ? ES9843PRO_VOL_MUTE : ES9843PRO_VOL_0DB;
        _writeVol8Regs(vol);
    } else {
        // VOL4_8BIT_INV (ES9841): mute=0x00, unmute=restore _savedVol8
        uint8_t vol = mute ? ES9841_VOL_MUTE : _savedVol8;
        _writeVol8Regs(vol);
    }
    LOG_I("%s %s", _desc.logPrefix, mute ? "Muted" : "Unmuted");
    return true;
}

// ---------------------------------------------------------------------------
// adcSetGain
// ---------------------------------------------------------------------------
bool HalEssAdc4ch::adcSetGain(uint8_t gainDb) {
    if (!_initialized) return false;
    if (gainDb > _desc.gainMax) gainDb = _desc.gainMax;
    uint8_t gainStep = gainDb / 6;
    if (_desc.gainType == GAIN4_2BIT_6DB && gainStep > 3) gainStep = 3;
    if (_desc.gainType == GAIN4_3BIT_6DB && gainStep > 7) gainStep = 7;
    _writeGainRegs(gainStep);
    _gainDb = (uint8_t)(gainStep * 6);
    LOG_I("%s ADC gain: %ddB (step=%u)", _desc.logPrefix, _gainDb, gainStep);
    return true;
}

// ---------------------------------------------------------------------------
// adcSetHpfEnabled
// ---------------------------------------------------------------------------
bool HalEssAdc4ch::adcSetHpfEnabled(bool en) {
    if (!_initialized) return false;
    _writeHpfRegs(en);
    _hpfEnabled = en;
    LOG_I("%s HPF: %s", _desc.logPrefix, en ? "enabled" : "disabled");
    return true;
}

// ---------------------------------------------------------------------------
// adcSetSampleRate
// ---------------------------------------------------------------------------
bool HalEssAdc4ch::adcSetSampleRate(uint32_t hz) {
    return configure(hz, _bitDepth);
}

// ---------------------------------------------------------------------------
// setFilterPreset
// ---------------------------------------------------------------------------
bool HalEssAdc4ch::setFilterPreset(uint8_t preset) {
    if (!_initialized) return false;
    if (preset > 7) return false;
    _writeFilterRegs(preset);
    _filterPreset = preset;
    LOG_I("%s Filter preset: %u", _desc.logPrefix, preset);
    return true;
}

// ---------------------------------------------------------------------------
// getInputSourceAt
// ---------------------------------------------------------------------------
const AudioInputSource* HalEssAdc4ch::getInputSourceAt(int idx) const {
    if (!_initialized) return nullptr;
    if (idx == 0) return &_srcA;
    if (idx == 1) return &_srcB;
    return nullptr;
}

#endif // DAC_ENABLED
