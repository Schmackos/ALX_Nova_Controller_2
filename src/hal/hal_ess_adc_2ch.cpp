#ifdef DAC_ENABLED
// HalEssAdc2ch — Generic ESS SABRE 2-channel ADC driver implementation.
// Descriptor tables for ES9822PRO, ES9826, ES9823PRO/MPRO, ES9821, ES9820.
// All 5 chips share the same init/configure/volume/mute/filter/gain logic driven
// by per-chip EssAdc2chDescriptor structs.

#include "hal_ess_adc_2ch.h"
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
// I2S port stubs for native test
inline uint32_t i2s_audio_port0_read(int32_t*, uint32_t) { return 0; }
inline uint32_t i2s_audio_port1_read(int32_t*, uint32_t) { return 0; }
inline bool     i2s_audio_port0_active(void) { return false; }
inline bool     i2s_audio_port1_active(void) { return false; }
inline uint32_t i2s_audio_get_sample_rate(void) { return 48000; }
#endif

// ===========================================================================
// Register constants replicated for native compilation
// All values verified against individual driver NATIVE_TEST blocks.
// ===========================================================================

#define DELAY_SENTINEL  0xFF   // reg==0xFF in init sequence means delay(val*5ms)

// ----- ES9822PRO (chip ID 0x81) -----
#define ES9822PRO_I2C_ADDR                0x40
#define ES9822PRO_CHIP_ID                 0x81
#define ES9822PRO_REG_SYS_CONFIG          0x00
#define ES9822PRO_REG_ADC_CLOCK_CONFIG1   0x01
#define ES9822PRO_REG_SYNC_CLK_SELECT     0xC1
#define ES9822PRO_REG_ADC_CH1A_CFG1       0x3F
#define ES9822PRO_REG_ADC_CH1A_CFG2       0x40
#define ES9822PRO_REG_ADC_CH2A_CFG1       0x41
#define ES9822PRO_REG_ADC_CH2A_CFG2       0x42
#define ES9822PRO_REG_ADC_COMMON_MODE     0x47
#define ES9822PRO_REG_CH1_DATAPATH        0x65
#define ES9822PRO_REG_CH1_VOLUME_LSB      0x6D
#define ES9822PRO_REG_CH2_DATAPATH        0x76
#define ES9822PRO_REG_CH2_VOLUME_LSB      0x7E
#define ES9822PRO_REG_CH1_GAIN            0x70
#define ES9822PRO_REG_CH2_GAIN            0x81
#define ES9822PRO_REG_CH1_FILTER          0x71
#define ES9822PRO_REG_CH2_FILTER          0x82
#define ES9822PRO_REG_CHIP_ID             0xE1
#define ES9822PRO_SOFT_RESET_BIT          0x80
#define ES9822PRO_OUTPUT_I2S              0x00
#define ES9822PRO_HPF_ENABLE_BIT          0x04
#define ES9822PRO_CLOCK_ENABLE_2CH        0x33
#define ES9822PRO_OPTIMAL_CH1A_CFG1       0xBA
#define ES9822PRO_OPTIMAL_CH1A_CFG2       0x3A
#define ES9822PRO_OPTIMAL_COMMON_MODE     0xFF

// ----- ES9826 (chip ID 0x8A) -----
#define ES9826_I2C_ADDR           0x40
#define ES9826_CHIP_ID            0x8A
#define ES9826_REG_SYS_CONFIG     0x00
#define ES9826_REG_CH1_VOL_LSB    0x2D
#define ES9826_REG_CH2_VOL_LSB    0x2F
#define ES9826_REG_PGA_GAIN       0x44
#define ES9826_REG_FILTER         0x3B
#define ES9826_REG_CHIP_ID        0xE1
#define ES9826_SOFT_RESET_BIT     0x80
#define ES9826_FILTER_SHAPE_SHIFT 2
#define ES9826_FILTER_SHAPE_MASK  0x1C
#define ES9826_PGA_MAX_DB         30
#define ES9826_PGA_STEP_DB        3
#define ES9826_PGA_MAX_NIBBLE     10

// ----- ES9823PRO / ES9823MPRO (chip IDs 0x8D / 0x8C) -----
#define ES9823PRO_I2C_ADDR                0x40
#define ES9823PRO_CHIP_ID                 0x8D
#define ES9823MPRO_CHIP_ID                0x8C
#define ES9823PRO_REG_SYS_CONFIG          0x00
#define ES9823PRO_REG_ADC_CLOCK_CONFIG1   0x01
#define ES9823PRO_REG_FILTER_SHAPE        0x4A
#define ES9823PRO_REG_CH1_VOLUME_LSB      0x51
#define ES9823PRO_REG_CH2_VOLUME_LSB      0x53
#define ES9823PRO_REG_DIGITAL_GAIN        0x55
#define ES9823PRO_REG_VOL_RAMP_RATE_UP    0x58
#define ES9823PRO_REG_VOL_RAMP_RATE_DOWN  0x59
#define ES9823PRO_REG_CHIP_ID             0xE1
#define ES9823PRO_SOFT_RESET_BIT          0x80
#define ES9823PRO_OUTPUT_I2S              0x00
#define ES9823PRO_CH1_GAIN_SHIFT          0
#define ES9823PRO_CH2_GAIN_SHIFT          4
#define ES9823PRO_CH_GAIN_MASK            0x07
#define ES9823PRO_FILTER_SHIFT            5
#define ES9823PRO_CLOCK_ENABLE_2CH        0x33

// ----- ES9821 (chip ID 0x88) -----
#define ES9821_I2C_ADDR           0x40
#define ES9821_CHIP_ID            0x88
#define ES9821_REG_SYS_CONFIG     0x00
#define ES9821_REG_CH1_VOL_LSB    0x32
#define ES9821_REG_CH2_VOL_LSB    0x34
#define ES9821_REG_FILTER         0x40
#define ES9821_REG_CHIP_ID        0xE1
#define ES9821_SOFT_RESET_BIT     0x80
#define ES9821_FILTER_SHAPE_SHIFT 2
#define ES9821_FILTER_SHAPE_MASK  0x1C

// ----- ES9820 (chip ID 0x84) -----
#define ES9820_I2C_ADDR                   0x40
#define ES9820_CHIP_ID                    0x84
#define ES9820_REG_SYS_CONFIG             0x00
#define ES9820_REG_CH1_DATAPATH           0x65
#define ES9820_REG_CH1_VOLUME_LSB         0x6D
#define ES9820_REG_CH2_DATAPATH           0x76
#define ES9820_REG_CH2_VOLUME_LSB         0x7E
#define ES9820_REG_CH1_GAIN               0x70
#define ES9820_REG_CH2_GAIN               0x81
#define ES9820_REG_CH1_FILTER             0x71
#define ES9820_REG_CH2_FILTER             0x82
#define ES9820_REG_CHIP_ID                0xE1
#define ES9820_SOFT_RESET_BIT             0x80
#define ES9820_OUTPUT_I2S                 0x00
#define ES9820_HPF_ENABLE_BIT             0x04
#define ES9820_CLOCK_ENABLE_2CH           0x33

// Shared 0dB / mute volume constants
#define ESS_ADC_2CH_VOL_0DB    0x7FFF
#define ESS_ADC_2CH_VOL_MUTE   0x0000

// ===========================================================================
// Init/deinit sequences
// ===========================================================================

// ----- ES9822PRO -----
static const EssAdc2chRegWrite kInitSeqES9822PRO[] = {
    { ES9822PRO_REG_SYS_CONFIG,        ES9822PRO_SOFT_RESET_BIT       },  // soft reset
    { DELAY_SENTINEL, 1                                                },  // delay 5ms
    { ES9822PRO_REG_SYNC_CLK_SELECT,   0x03                           },  // MCLK, EN_ANA_CLKIN
    { ES9822PRO_REG_ADC_CH1A_CFG1,     ES9822PRO_OPTIMAL_CH1A_CFG1   },  // optimal analog ADC
    { ES9822PRO_REG_ADC_CH1A_CFG2,     ES9822PRO_OPTIMAL_CH1A_CFG2   },
    { ES9822PRO_REG_ADC_CH2A_CFG1,     ES9822PRO_OPTIMAL_CH1A_CFG1   },  // CH2A mirrors CH1A
    { ES9822PRO_REG_ADC_CH2A_CFG2,     ES9822PRO_OPTIMAL_CH1A_CFG2   },
    { ES9822PRO_REG_ADC_COMMON_MODE,   ES9822PRO_OPTIMAL_COMMON_MODE  },
    { ES9822PRO_REG_ADC_CLOCK_CONFIG1, ES9822PRO_CLOCK_ENABLE_2CH    },  // enable ADC clocks
    { ES9822PRO_REG_SYS_CONFIG,        ES9822PRO_OUTPUT_I2S           },  // I2S/Philips output
};
static const EssAdc2chRegWrite kDeinitSeqES9822PRO[] = {
    { ES9822PRO_REG_ADC_CLOCK_CONFIG1, 0x00 },  // disable ADC clocks
};

// ----- ES9826 -----
static const EssAdc2chRegWrite kInitSeqES9826[] = {
    { ES9826_REG_SYS_CONFIG, ES9826_SOFT_RESET_BIT },  // soft reset
    { DELAY_SENTINEL, 1                             },  // delay 5ms
};
// No special deinit needed for ES9826
static const EssAdc2chRegWrite kDeinitSeqES9826[] = {};

// ----- ES9823PRO -----
static const EssAdc2chRegWrite kInitSeqES9823PRO[] = {
    { ES9823PRO_REG_SYS_CONFIG,         ES9823PRO_SOFT_RESET_BIT      },  // soft reset
    { DELAY_SENTINEL, 1                                                },  // delay 5ms
    { ES9823PRO_REG_ADC_CLOCK_CONFIG1,  ES9823PRO_CLOCK_ENABLE_2CH   },  // enable ADC clocks
    { ES9823PRO_REG_SYS_CONFIG,         ES9823PRO_OUTPUT_I2S          },  // I2S/Philips output
    { ES9823PRO_REG_VOL_RAMP_RATE_UP,   0x00                          },  // disable vol ramp
    { ES9823PRO_REG_VOL_RAMP_RATE_DOWN, 0x00                          },
};
static const EssAdc2chRegWrite kDeinitSeqES9823PRO[] = {
    { ES9823PRO_REG_ADC_CLOCK_CONFIG1, 0x00 },  // disable ADC clocks
};

// ----- ES9821 -----
static const EssAdc2chRegWrite kInitSeqES9821[] = {
    { ES9821_REG_SYS_CONFIG, ES9821_SOFT_RESET_BIT },  // soft reset
    { DELAY_SENTINEL, 1                             },  // delay 5ms
};
static const EssAdc2chRegWrite kDeinitSeqES9821[] = {};

// ----- ES9820 -----
static const EssAdc2chRegWrite kInitSeqES9820[] = {
    { ES9820_REG_SYS_CONFIG, ES9820_SOFT_RESET_BIT },  // soft reset
    { DELAY_SENTINEL, 1                             },  // delay 5ms
    { ES9820_REG_SYS_CONFIG, ES9820_OUTPUT_I2S      },  // I2S/Philips output
};
static const EssAdc2chRegWrite kDeinitSeqES9820[] = {};

// ===========================================================================
// Supported sample rate table (shared across all 2ch ADCs)
// ===========================================================================
static const uint32_t kAdcRates4[] = { 44100, 48000, 96000, 192000 };

// ===========================================================================
// Descriptor tables
// ===========================================================================

const EssAdc2chDescriptor kDescES9822PRO = {
    /* compatible          */ "ess,es9822pro",
    /* chipName            */ "ES9822PRO",
    /* chipId              */ ES9822PRO_CHIP_ID,
    /* altChipId           */ 0x00,
    /* altCompatible       */ nullptr,
    /* altChipName         */ nullptr,
    /* capabilities        */ (uint32_t)(HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL),
    /* sampleRateMask      */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K,
    /* supportedRates      */ kAdcRates4,
    /* supportedRateCount  */ 4,
    /* regChipId           */ ES9822PRO_REG_CHIP_ID,
    /* regSysConfig        */ ES9822PRO_REG_SYS_CONFIG,
    /* softResetVal        */ ES9822PRO_SOFT_RESET_BIT,
    /* regVolLsbCh1        */ ES9822PRO_REG_CH1_VOLUME_LSB,
    /* regVolLsbCh2        */ ES9822PRO_REG_CH2_VOLUME_LSB,
    /* gainType            */ GAIN_2BIT_6DB,
    /* regGainCh1          */ ES9822PRO_REG_CH1_GAIN,
    /* regGainCh2          */ ES9822PRO_REG_CH2_GAIN,
    /* gainMax             */ 18,
    /* gainStep            */ 6,
    /* gainMax_nibble      */ 0,
    /* hpfType             */ HPF_BIT_IN_DATAPATH,
    /* regHpfCh1           */ ES9822PRO_REG_CH1_DATAPATH,
    /* regHpfCh2           */ ES9822PRO_REG_CH2_DATAPATH,
    /* hpfEnableBit        */ ES9822PRO_HPF_ENABLE_BIT,
    /* filterType          */ FILTER_SHIFT2_CH_PAIR,
    /* regFilterCh1        */ ES9822PRO_REG_CH1_FILTER,
    /* regFilterCh2        */ ES9822PRO_REG_CH2_FILTER,
    /* initSeq             */ kInitSeqES9822PRO,
    /* initSeqLen          */ (uint8_t)(sizeof(kInitSeqES9822PRO) / sizeof(kInitSeqES9822PRO[0])),
    /* deinitSeq           */ kDeinitSeqES9822PRO,
    /* deinitSeqLen        */ (uint8_t)(sizeof(kDeinitSeqES9822PRO) / sizeof(kDeinitSeqES9822PRO[0])),
    /* logPrefix           */ "[HAL:ES9822PRO]",
};

const EssAdc2chDescriptor kDescES9826 = {
    /* compatible          */ "ess,es9826",
    /* chipName            */ "ES9826",
    /* chipId              */ ES9826_CHIP_ID,
    /* altChipId           */ 0x00,
    /* altCompatible       */ nullptr,
    /* altChipName         */ nullptr,
    /* capabilities        */ (uint32_t)(HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL),
    /* sampleRateMask      */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K,
    /* supportedRates      */ kAdcRates4,
    /* supportedRateCount  */ 4,
    /* regChipId           */ ES9826_REG_CHIP_ID,
    /* regSysConfig        */ ES9826_REG_SYS_CONFIG,
    /* softResetVal        */ ES9826_SOFT_RESET_BIT,
    /* regVolLsbCh1        */ ES9826_REG_CH1_VOL_LSB,
    /* regVolLsbCh2        */ ES9826_REG_CH2_VOL_LSB,
    /* gainType            */ GAIN_NIBBLE_3DB,
    /* regGainCh1          */ ES9826_REG_PGA_GAIN,
    /* regGainCh2          */ 0xFF,   // single register for both channels
    /* gainMax             */ ES9826_PGA_MAX_DB,
    /* gainStep            */ ES9826_PGA_STEP_DB,
    /* gainMax_nibble      */ ES9826_PGA_MAX_NIBBLE,
    /* hpfType             */ HPF_NONE,
    /* regHpfCh1           */ 0xFF,
    /* regHpfCh2           */ 0xFF,
    /* hpfEnableBit        */ 0x00,
    /* filterType          */ FILTER_SHIFT2_CH_PAIR,
    /* regFilterCh1        */ ES9826_REG_FILTER,
    /* regFilterCh2        */ 0xFF,   // single filter register
    /* initSeq             */ kInitSeqES9826,
    /* initSeqLen          */ (uint8_t)(sizeof(kInitSeqES9826) / sizeof(kInitSeqES9826[0])),
    /* deinitSeq           */ kDeinitSeqES9826,
    /* deinitSeqLen        */ 0,
    /* logPrefix           */ "[HAL:ES9826]",
};

const EssAdc2chDescriptor kDescES9823PRO = {
    /* compatible          */ "ess,es9823pro",
    /* chipName            */ "ES9823PRO",
    /* chipId              */ ES9823PRO_CHIP_ID,
    /* altChipId           */ ES9823MPRO_CHIP_ID,
    /* altCompatible       */ "ess,es9823mpro",
    /* altChipName         */ "ES9823MPRO",
    /* capabilities        */ (uint32_t)(HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL),
    /* sampleRateMask      */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K,
    /* supportedRates      */ kAdcRates4,
    /* supportedRateCount  */ 4,
    /* regChipId           */ ES9823PRO_REG_CHIP_ID,
    /* regSysConfig        */ ES9823PRO_REG_SYS_CONFIG,
    /* softResetVal        */ ES9823PRO_SOFT_RESET_BIT,
    /* regVolLsbCh1        */ ES9823PRO_REG_CH1_VOLUME_LSB,
    /* regVolLsbCh2        */ ES9823PRO_REG_CH2_VOLUME_LSB,
    /* gainType            */ GAIN_3BIT_6DB,
    /* regGainCh1          */ ES9823PRO_REG_DIGITAL_GAIN,
    /* regGainCh2          */ 0xFF,  // packed with CH1 in same register
    /* gainMax             */ 42,
    /* gainStep            */ 6,
    /* gainMax_nibble      */ 0,
    /* hpfType             */ HPF_NONE,  // no dedicated HPF register
    /* regHpfCh1           */ 0xFF,
    /* regHpfCh2           */ 0xFF,
    /* hpfEnableBit        */ 0x00,
    /* filterType          */ FILTER_SHIFT5_SINGLE,
    /* regFilterCh1        */ ES9823PRO_REG_FILTER_SHAPE,
    /* regFilterCh2        */ 0xFF,  // single register
    /* initSeq             */ kInitSeqES9823PRO,
    /* initSeqLen          */ (uint8_t)(sizeof(kInitSeqES9823PRO) / sizeof(kInitSeqES9823PRO[0])),
    /* deinitSeq           */ kDeinitSeqES9823PRO,
    /* deinitSeqLen        */ (uint8_t)(sizeof(kDeinitSeqES9823PRO) / sizeof(kDeinitSeqES9823PRO[0])),
    /* logPrefix           */ "[HAL:ES9823PRO]",
};

const EssAdc2chDescriptor kDescES9821 = {
    /* compatible          */ "ess,es9821",
    /* chipName            */ "ES9821",
    /* chipId              */ ES9821_CHIP_ID,
    /* altChipId           */ 0x00,
    /* altCompatible       */ nullptr,
    /* altChipName         */ nullptr,
    /* capabilities        */ (uint32_t)(HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME),
    /* sampleRateMask      */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K,
    /* supportedRates      */ kAdcRates4,
    /* supportedRateCount  */ 4,
    /* regChipId           */ ES9821_REG_CHIP_ID,
    /* regSysConfig        */ ES9821_REG_SYS_CONFIG,
    /* softResetVal        */ ES9821_SOFT_RESET_BIT,
    /* regVolLsbCh1        */ ES9821_REG_CH1_VOL_LSB,
    /* regVolLsbCh2        */ ES9821_REG_CH2_VOL_LSB,
    /* gainType            */ GAIN_NONE,
    /* regGainCh1          */ 0xFF,
    /* regGainCh2          */ 0xFF,
    /* gainMax             */ 0,
    /* gainStep            */ 0,
    /* gainMax_nibble      */ 0,
    /* hpfType             */ HPF_NONE,  // no dedicated HPF register
    /* regHpfCh1           */ 0xFF,
    /* regHpfCh2           */ 0xFF,
    /* hpfEnableBit        */ 0x00,
    /* filterType          */ FILTER_SHIFT2_CH_PAIR,
    /* regFilterCh1        */ ES9821_REG_FILTER,
    /* regFilterCh2        */ 0xFF,  // single filter register
    /* initSeq             */ kInitSeqES9821,
    /* initSeqLen          */ (uint8_t)(sizeof(kInitSeqES9821) / sizeof(kInitSeqES9821[0])),
    /* deinitSeq           */ kDeinitSeqES9821,
    /* deinitSeqLen        */ 0,
    /* logPrefix           */ "[HAL:ES9821]",
};

const EssAdc2chDescriptor kDescES9820 = {
    /* compatible          */ "ess,es9820",
    /* chipName            */ "ES9820",
    /* chipId              */ ES9820_CHIP_ID,
    /* altChipId           */ 0x00,
    /* altCompatible       */ nullptr,
    /* altChipName         */ nullptr,
    /* capabilities        */ (uint32_t)(HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL),
    /* sampleRateMask      */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K,
    /* supportedRates      */ kAdcRates4,
    /* supportedRateCount  */ 4,
    /* regChipId           */ ES9820_REG_CHIP_ID,
    /* regSysConfig        */ ES9820_REG_SYS_CONFIG,
    /* softResetVal        */ ES9820_SOFT_RESET_BIT,
    /* regVolLsbCh1        */ ES9820_REG_CH1_VOLUME_LSB,
    /* regVolLsbCh2        */ ES9820_REG_CH2_VOLUME_LSB,
    /* gainType            */ GAIN_2BIT_6DB,
    /* regGainCh1          */ ES9820_REG_CH1_GAIN,
    /* regGainCh2          */ ES9820_REG_CH2_GAIN,
    /* gainMax             */ 18,
    /* gainStep            */ 6,
    /* gainMax_nibble      */ 0,
    /* hpfType             */ HPF_BIT_IN_DATAPATH,
    /* regHpfCh1           */ ES9820_REG_CH1_DATAPATH,
    /* regHpfCh2           */ ES9820_REG_CH2_DATAPATH,
    /* hpfEnableBit        */ ES9820_HPF_ENABLE_BIT,
    /* filterType          */ FILTER_SHIFT2_CH_PAIR,
    /* regFilterCh1        */ ES9820_REG_CH1_FILTER,
    /* regFilterCh2        */ ES9820_REG_CH2_FILTER,
    /* initSeq             */ kInitSeqES9820,
    /* initSeqLen          */ (uint8_t)(sizeof(kInitSeqES9820) / sizeof(kInitSeqES9820[0])),
    /* deinitSeq           */ kDeinitSeqES9820,
    /* deinitSeqLen        */ 0,
    /* logPrefix           */ "[HAL:ES9820]",
};

// ===========================================================================
// HalEssAdc2ch — Generic implementation
// ===========================================================================

HalEssAdc2ch::HalEssAdc2ch(const EssAdc2chDescriptor& desc)
    : HalEssSabreAdcBase(), _desc(desc)
{
    hal_init_descriptor(_descriptor, desc.compatible, desc.chipName, "ESS Technology",
        HAL_DEV_ADC, 2, ES9822PRO_I2C_ADDR /* default; overridden by _applyConfigOverrides */,
        HAL_BUS_I2C, HAL_I2C_BUS_EXP,
        desc.sampleRateMask, desc.capabilities);
    _initPriority = HAL_PRIORITY_HARDWARE;
}

// ---------------------------------------------------------------------------
// _execSequence — execute a {reg,val} sequence; {0xFF,N} delays N*5ms
// ---------------------------------------------------------------------------
void HalEssAdc2ch::_execSequence(const EssAdc2chRegWrite* seq, uint8_t len) {
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
// _clampGainDb — clamp to descriptor max
// ---------------------------------------------------------------------------
uint8_t HalEssAdc2ch::_clampGainDb(uint8_t gainDb) const {
    if (_desc.gainType == GAIN_NONE) return 0;
    if (gainDb > _desc.gainMax) return _desc.gainMax;
    return gainDb;
}

// ---------------------------------------------------------------------------
// _computeGainReg — compute register value from clamped gainDb
// Returns the value to write (interpretation depends on gainType)
// ---------------------------------------------------------------------------
uint8_t HalEssAdc2ch::_computeGainReg(uint8_t gainDb) const {
    switch (_desc.gainType) {
        case GAIN_2BIT_6DB: {
            // 2-bit: 0=0dB, 1=+6dB, 2=+12dB, 3=+18dB
            uint8_t step = gainDb / 6;
            if (step > 3) step = 3;
            return step & 0x03;
        }
        case GAIN_3BIT_6DB: {
            // 3-bit packed both channels: CH1=bits[2:0], CH2=bits[6:4]
            uint8_t step = (gainDb + 3) / 6;  // round to nearest
            if (step > 7) step = 7;
            return (uint8_t)((step & ES9823PRO_CH_GAIN_MASK) |
                             ((step & ES9823PRO_CH_GAIN_MASK) << ES9823PRO_CH2_GAIN_SHIFT));
        }
        case GAIN_NIBBLE_3DB: {
            // Nibble-packed: high nibble = CH1, low nibble = CH2 (same value)
            uint8_t stepedGain = (gainDb / _desc.gainStep) * _desc.gainStep;
            uint8_t nibble = stepedGain / _desc.gainStep;
            if (nibble > _desc.gainMax_nibble) nibble = _desc.gainMax_nibble;
            return (uint8_t)((nibble << 4) | nibble);
        }
        case GAIN_NONE:
        default:
            return 0x00;
    }
}

// ---------------------------------------------------------------------------
// probe
// ---------------------------------------------------------------------------
bool HalEssAdc2ch::probe() {
#ifndef NATIVE_TEST
    if (!_bus().probe(_i2cAddr)) return false;
    uint8_t chipId = _readReg(_desc.regChipId);
    return (chipId == _desc.chipId) || (_desc.altChipId != 0 && chipId == _desc.altChipId);
#else
    return true;
#endif
}

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------
HalInitResult HalEssAdc2ch::init() {
    // 1. Read per-device config overrides
    _applyConfigOverrides();
    _gainDb = _clampGainDb(_gainDb);

    LOG_I("%s Initializing (I2C addr=0x%02X bus=%u SDA=%d SCL=%d sr=%luHz bits=%u)",
          _desc.logPrefix, _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth);

#ifndef NATIVE_TEST
    // 2+3. Select and initialize TwoWire instance
    _selectWire();
    LOG_I("%s I2C initialized (bus %u SDA=%d SCL=%d 400kHz)",
          _desc.logPrefix, _i2cBusIndex, _sdaPin, _sclPin);
#endif

    // 4. Check chip ID and detect alt variant
    uint8_t chipId = _readReg(_desc.regChipId);
    if (_desc.altChipId != 0 && chipId == _desc.altChipId) {
        _isAltVariant = true;
        // Update compatible + name to reflect actual variant detected
        hal_safe_strcpy(_descriptor.compatible, sizeof(_descriptor.compatible), _desc.altCompatible);
        hal_safe_strcpy(_descriptor.name,       sizeof(_descriptor.name),       _desc.altChipName);
        LOG_I("%s %s detected (chip ID 0x%02X)", _desc.logPrefix, _desc.altChipName, chipId);
    } else if (chipId == _desc.chipId) {
        _isAltVariant = false;
        LOG_I("%s Chip ID OK (0x%02X)", _desc.logPrefix, chipId);
    } else {
        LOG_W("%s Unexpected chip ID: 0x%02X (expected 0x%02X) — continuing",
              _desc.logPrefix, chipId, _desc.chipId);
    }

    // 5. Execute init sequence (includes soft reset, clock config, output format)
    _execSequence(_desc.initSeq, _desc.initSeqLen);

    // 6. Set unity gain volume on both channels (0x7FFF = 0 dB)
    _writeReg16(_desc.regVolLsbCh1, ESS_ADC_2CH_VOL_0DB);
    _writeReg16(_desc.regVolLsbCh2, ESS_ADC_2CH_VOL_0DB);

    // 7. PGA gain
    if (_desc.gainType != GAIN_NONE) {
        uint8_t gainReg = _computeGainReg(_gainDb);
        if (_desc.gainType == GAIN_NIBBLE_3DB || _desc.gainType == GAIN_3BIT_6DB) {
            // Single register covers both channels
            _writeReg(_desc.regGainCh1, gainReg);
        } else {
            // Two separate registers
            _writeReg(_desc.regGainCh1, gainReg);
            _writeReg(_desc.regGainCh2, gainReg);
        }
        // Snap _gainDb to actual programmed step
        if (_desc.gainType == GAIN_2BIT_6DB) {
            uint8_t step = gainReg & 0x03;
            _gainDb = (uint8_t)(step * 6);
        } else if (_desc.gainType == GAIN_3BIT_6DB) {
            uint8_t step = (gainReg & ES9823PRO_CH_GAIN_MASK);
            _gainDb = (uint8_t)(step * 6);
        } else if (_desc.gainType == GAIN_NIBBLE_3DB) {
            uint8_t nibble = gainReg & 0x0F;
            _gainDb = (uint8_t)(nibble * _desc.gainStep);
        }
    }

    // 8. HPF (DC blocking)
    if (_desc.hpfType == HPF_BIT_IN_DATAPATH) {
        uint8_t datapath = _hpfEnabled ? _desc.hpfEnableBit : 0x00;
        _writeReg(_desc.regHpfCh1, datapath);
        _writeReg(_desc.regHpfCh2, datapath);
    }
    // HPF_NONE: flag stored for API compat, no register write

    // 9. Digital filter preset
    {
        uint8_t preset = (_filterPreset > 7) ? 7 : _filterPreset;
        if (_desc.filterType == FILTER_SHIFT5_SINGLE) {
            // Single register, bits[7:5] — ES9823PRO style
            uint8_t filterReg = (uint8_t)((preset & 0x07) << 5);
            _writeReg(_desc.regFilterCh1, filterReg);
        } else {
            // FILTER_SHIFT2_CH_PAIR — bits[4:2], per-channel or single reg
            uint8_t filterBits = (uint8_t)((preset & 0x07) << 2);
            if (_desc.regFilterCh2 == 0xFF) {
                // Single register (ES9826, ES9821)
                uint8_t cur = _readReg(_desc.regFilterCh1);
                uint8_t val = (uint8_t)((cur & ~(uint8_t)0x1C) | filterBits);
                _writeReg(_desc.regFilterCh1, val);
            } else {
                // Per-channel registers (ES9822PRO, ES9820) — preserve lower bits
                uint8_t ch1cur = _readReg(_desc.regFilterCh1);
                uint8_t ch2cur = _readReg(_desc.regFilterCh2);
                _writeReg(_desc.regFilterCh1, (uint8_t)((ch1cur & 0x03) | filterBits));
                _writeReg(_desc.regFilterCh2, (uint8_t)((ch2cur & 0x03) | filterBits));
            }
        }
    }

    // 10. Populate AudioInputSource (port-indexed I2S callbacks)
    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);
    uint8_t port = (cfg && cfg->valid && cfg->i2sPort != 255) ? cfg->i2sPort : 2;

    memset(&_inputSrc, 0, sizeof(_inputSrc));
    _inputSrc.name          = _descriptor.name;
    _inputSrc.isHardwareAdc = true;
    _inputSrc.gainLinear    = 1.0f;
    _inputSrc.vuL           = -90.0f;
    _inputSrc.vuR           = -90.0f;

#ifndef NATIVE_TEST
    if (port == 0) {
        _inputSrc.read     = i2s_audio_port0_read;
        _inputSrc.isActive = i2s_audio_port0_active;
    } else if (port == 1) {
        _inputSrc.read     = i2s_audio_port1_read;
        _inputSrc.isActive = i2s_audio_port1_active;
    } else {
        _inputSrc.read     = i2s_audio_port2_read;
        _inputSrc.isActive = i2s_audio_port2_active;
    }
    _inputSrc.getSampleRate = i2s_audio_get_sample_rate;
#endif
    _inputSrcReady = true;

    // 11. Mark device ready
    _initialized = true;
    _state = HAL_STATE_AVAILABLE;
    setReady(true);

    LOG_I("%s Ready (i2s port=%u gain=%ddB hpf=%d filter=%u%s)",
          _desc.logPrefix, port, _gainDb, (int)_hpfEnabled, _filterPreset,
          _isAltVariant ? " [alt variant]" : "");
    return hal_init_ok();
}

// ---------------------------------------------------------------------------
// deinit
// ---------------------------------------------------------------------------
void HalEssAdc2ch::deinit() {
    if (!_initialized) return;

    setReady(false);

#ifndef NATIVE_TEST
    _execSequence(_desc.deinitSeq, _desc.deinitSeqLen);
#endif

    _initialized   = false;
    _inputSrcReady = false;
    _state = HAL_STATE_REMOVED;

    LOG_I("%s Deinitialized", _desc.logPrefix);
}

// ---------------------------------------------------------------------------
// dumpConfig
// ---------------------------------------------------------------------------
void HalEssAdc2ch::dumpConfig() {
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
bool HalEssAdc2ch::healthCheck() {
#ifndef NATIVE_TEST
    if (!_initialized) return false;
    uint8_t id = _readReg(_desc.regChipId);
    return (id == _desc.chipId) || (_desc.altChipId != 0 && id == _desc.altChipId);
#else
    return _initialized;
#endif
}

// ---------------------------------------------------------------------------
// configure
// ---------------------------------------------------------------------------
bool HalEssAdc2ch::configure(uint32_t sampleRate, uint8_t bitDepth) {
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
bool HalEssAdc2ch::setVolume(uint8_t percent) {
    if (!_initialized) return false;
    if (percent > 100) percent = 100;
    uint16_t vol16 = (uint16_t)(((uint32_t)percent * ESS_ADC_2CH_VOL_0DB) / 100);
    bool ok  = _writeReg16(_desc.regVolLsbCh1, vol16);
    ok       = ok && _writeReg16(_desc.regVolLsbCh2, vol16);
    LOG_D("%s Volume: %d%% -> 0x%04X", _desc.logPrefix, percent, vol16);
    return ok;
}

// ---------------------------------------------------------------------------
// setMute
// ---------------------------------------------------------------------------
bool HalEssAdc2ch::setMute(bool mute) {
    if (!_initialized) return false;
    uint16_t vol16 = mute ? ESS_ADC_2CH_VOL_MUTE : ESS_ADC_2CH_VOL_0DB;
    bool ok  = _writeReg16(_desc.regVolLsbCh1, vol16);
    ok       = ok && _writeReg16(_desc.regVolLsbCh2, vol16);
    LOG_I("%s %s", _desc.logPrefix, mute ? "Muted" : "Unmuted");
    return ok;
}

// ---------------------------------------------------------------------------
// adcSetGain
// ---------------------------------------------------------------------------
bool HalEssAdc2ch::adcSetGain(uint8_t gainDb) {
    if (!_initialized) return false;
    if (_desc.gainType == GAIN_NONE) {
        if (gainDb != 0) {
            LOG_W("%s adcSetGain: no hardware PGA (only 0dB supported)", _desc.logPrefix);
            return false;
        }
        _gainDb = 0;
        return true;
    }
    gainDb = _clampGainDb(gainDb);
    uint8_t gainReg = _computeGainReg(gainDb);
    bool ok = true;
    if (_desc.gainType == GAIN_NIBBLE_3DB || _desc.gainType == GAIN_3BIT_6DB) {
        ok = _writeReg(_desc.regGainCh1, gainReg);
    } else {
        ok  = _writeReg(_desc.regGainCh1, gainReg);
        ok  = ok && _writeReg(_desc.regGainCh2, gainReg);
    }
    // Snap to actual step
    if (_desc.gainType == GAIN_2BIT_6DB) {
        _gainDb = (uint8_t)((gainReg & 0x03) * 6);
    } else if (_desc.gainType == GAIN_3BIT_6DB) {
        uint8_t step = gainReg & ES9823PRO_CH_GAIN_MASK;
        _gainDb = (uint8_t)(step * 6);
    } else if (_desc.gainType == GAIN_NIBBLE_3DB) {
        uint8_t nibble = gainReg & 0x0F;
        _gainDb = (uint8_t)(nibble * _desc.gainStep);
    }
    LOG_I("%s ADC gain: %ddB (reg=0x%02X)", _desc.logPrefix, _gainDb, gainReg);
    return ok;
}

// ---------------------------------------------------------------------------
// adcSetHpfEnabled
// ---------------------------------------------------------------------------
bool HalEssAdc2ch::adcSetHpfEnabled(bool en) {
    if (!_initialized) return false;
    if (_desc.hpfType == HPF_NONE) {
        _hpfEnabled = en;
        LOG_I("%s HPF: %s (flag only — no dedicated HPF register)", _desc.logPrefix,
              en ? "enabled" : "disabled");
        return true;
    }
    // HPF_BIT_IN_DATAPATH
    uint8_t ch1 = _readReg(_desc.regHpfCh1);
    uint8_t ch2 = _readReg(_desc.regHpfCh2);
    if (en) {
        ch1 |= _desc.hpfEnableBit;
        ch2 |= _desc.hpfEnableBit;
    } else {
        ch1 &= (uint8_t)~_desc.hpfEnableBit;
        ch2 &= (uint8_t)~_desc.hpfEnableBit;
    }
    bool ok  = _writeReg(_desc.regHpfCh1, ch1);
    ok       = ok && _writeReg(_desc.regHpfCh2, ch2);
    _hpfEnabled = en;
    LOG_I("%s HPF: %s", _desc.logPrefix, en ? "enabled" : "disabled");
    return ok;
}

// ---------------------------------------------------------------------------
// adcSetSampleRate
// ---------------------------------------------------------------------------
bool HalEssAdc2ch::adcSetSampleRate(uint32_t hz) {
    return configure(hz, _bitDepth);
}

// ---------------------------------------------------------------------------
// setFilterPreset
// ---------------------------------------------------------------------------
bool HalEssAdc2ch::setFilterPreset(uint8_t preset) {
    if (preset > 7) return false;
    if (!_initialized) {
        _filterPreset = preset;
        return true;
    }
    if (_desc.filterType == FILTER_SHIFT5_SINGLE) {
        // bits[7:5] in single register (ES9823PRO)
        uint8_t filterReg = (uint8_t)((preset & 0x07) << 5);
        bool ok = _writeReg(_desc.regFilterCh1, filterReg);
        if (ok) _filterPreset = preset;
        LOG_I("%s Filter preset: %u", _desc.logPrefix, preset);
        return ok;
    }
    // FILTER_SHIFT2_CH_PAIR — bits[4:2]
    uint8_t filterBits = (uint8_t)((preset & 0x07) << 2);
    bool ok;
    if (_desc.regFilterCh2 == 0xFF) {
        // Single filter register (ES9826, ES9821)
        uint8_t cur = _readReg(_desc.regFilterCh1);
        uint8_t val = (uint8_t)((cur & ~(uint8_t)0x1C) | filterBits);
        ok = _writeReg(_desc.regFilterCh1, val);
    } else {
        // Per-channel (ES9822PRO, ES9820)
        uint8_t ch1cur = _readReg(_desc.regFilterCh1);
        uint8_t ch2cur = _readReg(_desc.regFilterCh2);
        uint8_t ch1new = (uint8_t)((ch1cur & ~(uint8_t)0x1C) | filterBits);
        uint8_t ch2new = (uint8_t)((ch2cur & ~(uint8_t)0x1C) | filterBits);
        ok  = _writeReg(_desc.regFilterCh1, ch1new);
        ok  = ok && _writeReg(_desc.regFilterCh2, ch2new);
    }
    if (ok) _filterPreset = preset;
    LOG_I("%s Filter preset: %u", _desc.logPrefix, preset);
    return ok;
}

// ---------------------------------------------------------------------------
// getInputSource
// ---------------------------------------------------------------------------
const AudioInputSource* HalEssAdc2ch::getInputSource() const {
    return _inputSrcReady ? &_inputSrc : nullptr;
}

#endif // DAC_ENABLED
