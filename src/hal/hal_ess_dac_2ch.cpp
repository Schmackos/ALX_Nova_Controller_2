#ifdef DAC_ENABLED
// HalEssDac2ch — Generic ESS SABRE 2-channel DAC driver implementation.
// Descriptor tables for ES9038Q2M, ES9039Q2M, ES9069Q, ES9033Q, ES9020.
// All 5 chips share the same init/configure/volume/mute/filter logic driven
// by per-chip EssDac2chDescriptor structs.

#include "hal_ess_dac_2ch.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include <Arduino.h>
#include "../debug_serial.h"
#else
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)
#endif

// ===========================================================================
// Register constants replicated for native compilation (mirrors per-chip regs.h)
// All values verified against individual driver NATIVE_TEST blocks.
// ===========================================================================

// ----- Shared / sentinel -----
#define REG_NO_FEATURE   0xFF   // Indicates "no feature" in descriptor
#define DELAY_SENTINEL   0xFF   // reg==0xFF in init sequence means delay(val*5ms)

// ----- ES9038Q2M (chip ID 0x90) -----
#define ES9038Q2M_I2C_ADDR          0x48
#define ES9038Q2M_CHIP_ID           0x90
#define ES9038Q2M_REG_SYS_CONFIG    0x00
#define ES9038Q2M_REG_INPUT_CFG     0x01
#define ES9038Q2M_REG_FILTER_MUTE   0x07
#define ES9038Q2M_REG_MASTER_MODE   0x0A
#define ES9038Q2M_REG_DPLL_CFG      0x0B
#define ES9038Q2M_REG_CLOCK_GEAR    0x0D
#define ES9038Q2M_REG_VOL_CH1       0x0F
#define ES9038Q2M_REG_VOL_CH2       0x10
#define ES9038Q2M_SOFT_RESET_BIT    0x01
#define ES9038Q2M_INIT_INPUT_CFG    0x03  // 32-bit Philips
#define ES9038Q2M_SLAVE_MODE        0x00
#define ES9038Q2M_INIT_DPLL_BW      0x03
#define ES9038Q2M_CLK_GEAR_1X       0x00
#define ES9038Q2M_CLK_GEAR_2X       0x01
#define ES9038Q2M_CLK_GEAR_4X       0x02
#define ES9038Q2M_MUTE_BIT          0x01
#define ES9038Q2M_FILTER_SHIFT      2
#define ES9038Q2M_FILTER_MASK       0x1C

// ----- ES9039Q2M (chip ID 0x92) -----
#define ES9039Q2M_I2C_ADDR          0x48
#define ES9039Q2M_CHIP_ID           0x92
#define ES9039Q2M_REG_SYS_CONFIG    0x00
#define ES9039Q2M_REG_INPUT_CFG     0x01
#define ES9039Q2M_REG_FILTER_MUTE   0x07
#define ES9039Q2M_REG_MASTER_MODE   0x0A
#define ES9039Q2M_REG_DPLL_CFG      0x0B
#define ES9039Q2M_REG_CLOCK_GEAR    0x0D
#define ES9039Q2M_REG_VOL_CH1       0x0F
#define ES9039Q2M_REG_VOL_CH2       0x10
#define ES9039Q2M_SOFT_RESET_BIT    0x01
#define ES9039Q2M_INIT_INPUT_CFG    0x03  // 32-bit Philips
#define ES9039Q2M_SLAVE_MODE        0x00
#define ES9039Q2M_INIT_DPLL_BW      0x05  // tighter BW for Hyperstream IV stability
#define ES9039Q2M_CLK_GEAR_1X       0x00
#define ES9039Q2M_CLK_GEAR_2X       0x01
#define ES9039Q2M_CLK_GEAR_4X       0x02
#define ES9039Q2M_MUTE_BIT          0x01
#define ES9039Q2M_FILTER_SHIFT      2
#define ES9039Q2M_FILTER_MASK       0x1C

// ----- ES9069Q (chip ID 0x94) -----
#define ES9069Q_I2C_ADDR            0x48
#define ES9069Q_CHIP_ID             0x94
#define ES9069Q_REG_SYSTEM_SETTINGS 0x00
#define ES9069Q_REG_INPUT_CONFIG    0x01
#define ES9069Q_REG_FILTER_SHAPE    0x07
#define ES9069Q_REG_DPLL_BANDWIDTH  0x0C
#define ES9069Q_REG_VOLUME_L        0x0F
#define ES9069Q_REG_VOLUME_R        0x10
#define ES9069Q_REG_MQA_CONTROL     0x17
#define ES9069Q_INPUT_I2S           0x00
#define ES9069Q_SOFT_START_BIT      0x02
#define ES9069Q_I2S_LEN_32          0x00
#define ES9069Q_MQA_ENABLE_BIT      0x01
#define ES9069Q_MQA_STATUS_MASK     0x0E
#define ES9069Q_DPLL_BW_DEFAULT     0x04

// ----- ES9033Q (chip ID 0x88) -----
#define ES9033Q_I2C_ADDR            0x48
#define ES9033Q_CHIP_ID             0x88
#define ES9033Q_REG_SYSTEM_SETTINGS 0x00
#define ES9033Q_REG_INPUT_CONFIG    0x01
#define ES9033Q_REG_FILTER_SHAPE    0x07
#define ES9033Q_REG_DPLL_BANDWIDTH  0x0C
#define ES9033Q_REG_VOLUME_L        0x0F
#define ES9033Q_REG_VOLUME_R        0x10
#define ES9033Q_REG_LINE_DRIVER     0x14
#define ES9033Q_INPUT_I2S           0x00
#define ES9033Q_SOFT_START_BIT      0x02
#define ES9033Q_I2S_LEN_32          0x00
#define ES9033Q_LINE_DRIVER_INIT    0x21  // LINE_DRIVER_ENABLE | LINE_DRIVER_ILIMIT
#define ES9033Q_DPLL_BW_DEFAULT     0x04

// ----- ES9020 (chip ID 0x86) -----
#define ES9020_DAC_I2C_ADDR         0x48
#define ES9020_CHIP_ID              0x86
#define ES9020_REG_SOFT_RESET       0x00
#define ES9020_REG_INPUT_CONFIG     0x01
#define ES9020_REG_FILTER           0x07
#define ES9020_REG_APLL_CTRL        0x0C
#define ES9020_REG_CLK_SOURCE       0x0D
#define ES9020_REG_VOLUME           0x0F
#define ES9020_SOFT_RESET_BIT       0x80
#define ES9020_TDM_SLOTS_2          0x00
#define ES9020_FILTER_SHAPE_MASK    0x07
#define ES9020_APLL_ENABLE_BIT      0x01
#define ES9020_APLL_LOCK_BIT        0x10
#define ES9020_CLK_BCK_RECOVERY     0x00
#define ES9020_CLK_MCLK             0x02

// ===========================================================================
// Init/deinit sequences
// ===========================================================================

// ----- ES9038Q2M -----
static const EssDac2chRegWrite kInitSeqES9038Q2M[] = {
    { ES9038Q2M_REG_SYS_CONFIG,  ES9038Q2M_SOFT_RESET_BIT  },  // soft reset
    { DELAY_SENTINEL, 1                                     },  // delay 5ms
    { ES9038Q2M_REG_INPUT_CFG,   ES9038Q2M_INIT_INPUT_CFG  },  // 32-bit Philips
    { ES9038Q2M_REG_MASTER_MODE, ES9038Q2M_SLAVE_MODE       },  // I2S slave
    { ES9038Q2M_REG_DPLL_CFG,    ES9038Q2M_INIT_DPLL_BW    },  // DPLL BW
};
static const EssDac2chRegWrite kDeinitSeqES9038Q2M[] = {
    { ES9038Q2M_REG_FILTER_MUTE, ES9038Q2M_MUTE_BIT },  // mute output
};

// ----- ES9039Q2M -----
static const EssDac2chRegWrite kInitSeqES9039Q2M[] = {
    { ES9039Q2M_REG_SYS_CONFIG,  ES9039Q2M_SOFT_RESET_BIT  },
    { DELAY_SENTINEL, 1                                     },
    { ES9039Q2M_REG_INPUT_CFG,   ES9039Q2M_INIT_INPUT_CFG  },
    { ES9039Q2M_REG_MASTER_MODE, ES9039Q2M_SLAVE_MODE       },
    { ES9039Q2M_REG_DPLL_CFG,    ES9039Q2M_INIT_DPLL_BW    },
};
static const EssDac2chRegWrite kDeinitSeqES9039Q2M[] = {
    { ES9039Q2M_REG_FILTER_MUTE, ES9039Q2M_MUTE_BIT },
};

// ----- ES9069Q -----
static const EssDac2chRegWrite kInitSeqES9069Q[] = {
    { ES9069Q_REG_SYSTEM_SETTINGS, (uint8_t)(ES9069Q_INPUT_I2S | ES9069Q_SOFT_START_BIT) },
    { ES9069Q_REG_INPUT_CONFIG,    ES9069Q_I2S_LEN_32                                    },
    { ES9069Q_REG_DPLL_BANDWIDTH,  ES9069Q_DPLL_BW_DEFAULT                               },
    { DELAY_SENTINEL, 1                                                                   },
    { ES9069Q_REG_MQA_CONTROL,     0x00                                                  },
};
static const EssDac2chRegWrite kDeinitSeqES9069Q[] = {
    { ES9069Q_REG_VOLUME_L,    0xFF },  // mute via volume
    { ES9069Q_REG_VOLUME_R,    0xFF },
    { ES9069Q_REG_MQA_CONTROL, 0x00 },
};

// ----- ES9033Q -----
static const EssDac2chRegWrite kInitSeqES9033Q[] = {
    { ES9033Q_REG_SYSTEM_SETTINGS, (uint8_t)(ES9033Q_INPUT_I2S | ES9033Q_SOFT_START_BIT) },
    { ES9033Q_REG_INPUT_CONFIG,    ES9033Q_I2S_LEN_32                                    },
    { ES9033Q_REG_DPLL_BANDWIDTH,  ES9033Q_DPLL_BW_DEFAULT                               },
    { DELAY_SENTINEL, 1                                                                   },
    { ES9033Q_REG_LINE_DRIVER,     ES9033Q_LINE_DRIVER_INIT                              },
};
static const EssDac2chRegWrite kDeinitSeqES9033Q[] = {
    { ES9033Q_REG_VOLUME_L,    0xFF },  // mute via volume
    { ES9033Q_REG_VOLUME_R,    0xFF },
    { ES9033Q_REG_LINE_DRIVER, 0x00 },  // power down line driver
};

// ----- ES9020 -----
static const EssDac2chRegWrite kInitSeqES9020[] = {
    { ES9020_REG_SOFT_RESET,    ES9020_SOFT_RESET_BIT },
    { DELAY_SENTINEL, 1                               },
    { ES9020_REG_INPUT_CONFIG,  ES9020_TDM_SLOTS_2    },
    { ES9020_REG_CLK_SOURCE,    ES9020_CLK_MCLK       },
};
static const EssDac2chRegWrite kDeinitSeqES9020[] = {
    { ES9020_REG_VOLUME, 0xFF },  // mute via volume
};

// ===========================================================================
// Supported sample rate tables
// ===========================================================================

static const uint32_t kRates6[]  = { 44100, 48000, 96000, 192000, 384000, 768000 };
static const uint32_t kRates4[]  = { 44100, 48000, 96000, 192000 };

// ===========================================================================
// Descriptor tables
// ===========================================================================

const EssDac2chDescriptor kDescES9038Q2M = {
    /* compatible        */ "ess,es9038q2m",
    /* chipName          */ "ES9038Q2M",
    /* chipId            */ ES9038Q2M_CHIP_ID,
    /* capabilities      */ (uint16_t)(HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS | HAL_CAP_DPLL),
    /* sampleRateMask    */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K,
    /* supportedRates    */ kRates6,
    /* supportedRateCount*/ 6,
    /* regVolL           */ ES9038Q2M_REG_VOL_CH1,
    /* regVolR           */ ES9038Q2M_REG_VOL_CH2,
    /* regFilter         */ ES9038Q2M_REG_FILTER_MUTE,
    /* volType           */ VOL_DUAL_0xFF,
    /* muteType          */ MUTE_VIA_DEDICATED_BIT,
    /* muteBitMask       */ ES9038Q2M_MUTE_BIT,
    /* filterType        */ FILTER_BITS_4_2_WITH_MUTE,
    /* filterShift       */ ES9038Q2M_FILTER_SHIFT,
    /* filterMask        */ ES9038Q2M_FILTER_MASK,
    /* reconfigType      */ RECONFIG_CLOCK_GEAR,
    /* regClockGear      */ ES9038Q2M_REG_CLOCK_GEAR,
    /* regInputConfig    */ ES9038Q2M_REG_INPUT_CFG,
    /* initSeq           */ kInitSeqES9038Q2M,
    /* initSeqLen        */ (uint8_t)(sizeof(kInitSeqES9038Q2M) / sizeof(kInitSeqES9038Q2M[0])),
    /* deinitSeq         */ kDeinitSeqES9038Q2M,
    /* deinitSeqLen      */ (uint8_t)(sizeof(kDeinitSeqES9038Q2M) / sizeof(kDeinitSeqES9038Q2M[0])),
    /* regFeature        */ REG_NO_FEATURE,
    /* featureEnableVal  */ 0x00,
    /* featureDisableVal */ 0x00,
    /* regFeatureStatus  */ REG_NO_FEATURE,
    /* featureStatusMask */ 0x00,
    /* regClockSource    */ REG_NO_FEATURE,
    /* clkSrcEnabled     */ 0x00,
    /* clkSrcDisabled    */ 0x00,
    /* logPrefix         */ "[HAL:ES9038Q2M]",
};

const EssDac2chDescriptor kDescES9039Q2M = {
    /* compatible        */ "ess,es9039q2m",
    /* chipName          */ "ES9039Q2M",
    /* chipId            */ ES9039Q2M_CHIP_ID,
    /* capabilities      */ (uint16_t)(HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS | HAL_CAP_DPLL),
    /* sampleRateMask    */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K,
    /* supportedRates    */ kRates6,
    /* supportedRateCount*/ 6,
    /* regVolL           */ ES9039Q2M_REG_VOL_CH1,
    /* regVolR           */ ES9039Q2M_REG_VOL_CH2,
    /* regFilter         */ ES9039Q2M_REG_FILTER_MUTE,
    /* volType           */ VOL_DUAL_0xFF,
    /* muteType          */ MUTE_VIA_DEDICATED_BIT,
    /* muteBitMask       */ ES9039Q2M_MUTE_BIT,
    /* filterType        */ FILTER_BITS_4_2_WITH_MUTE,
    /* filterShift       */ ES9039Q2M_FILTER_SHIFT,
    /* filterMask        */ ES9039Q2M_FILTER_MASK,
    /* reconfigType      */ RECONFIG_CLOCK_GEAR,
    /* regClockGear      */ ES9039Q2M_REG_CLOCK_GEAR,
    /* regInputConfig    */ ES9039Q2M_REG_INPUT_CFG,
    /* initSeq           */ kInitSeqES9039Q2M,
    /* initSeqLen        */ (uint8_t)(sizeof(kInitSeqES9039Q2M) / sizeof(kInitSeqES9039Q2M[0])),
    /* deinitSeq         */ kDeinitSeqES9039Q2M,
    /* deinitSeqLen      */ (uint8_t)(sizeof(kDeinitSeqES9039Q2M) / sizeof(kDeinitSeqES9039Q2M[0])),
    /* regFeature        */ REG_NO_FEATURE,
    /* featureEnableVal  */ 0x00,
    /* featureDisableVal */ 0x00,
    /* regFeatureStatus  */ REG_NO_FEATURE,
    /* featureStatusMask */ 0x00,
    /* regClockSource    */ REG_NO_FEATURE,
    /* clkSrcEnabled     */ 0x00,
    /* clkSrcDisabled    */ 0x00,
    /* logPrefix         */ "[HAL:ES9039Q2M]",
};

const EssDac2chDescriptor kDescES9069Q = {
    /* compatible        */ "ess,es9069q",
    /* chipName          */ "ES9069Q",
    /* chipId            */ ES9069Q_CHIP_ID,
    /* capabilities      */ (uint16_t)(HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE | HAL_CAP_MQA | HAL_CAP_DPLL),
    /* sampleRateMask    */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K,
    /* supportedRates    */ kRates6,
    /* supportedRateCount*/ 6,
    /* regVolL           */ ES9069Q_REG_VOLUME_L,
    /* regVolR           */ ES9069Q_REG_VOLUME_R,
    /* regFilter         */ ES9069Q_REG_FILTER_SHAPE,
    /* volType           */ VOL_DUAL_0xFF,
    /* muteType          */ MUTE_VIA_VOLUME,
    /* muteBitMask       */ 0x00,
    /* filterType        */ FILTER_BITS_2_0,
    /* filterShift       */ 0,
    /* filterMask        */ 0x07,
    /* reconfigType      */ RECONFIG_WORD_LENGTH,
    /* regClockGear      */ REG_NO_FEATURE,
    /* regInputConfig    */ ES9069Q_REG_INPUT_CONFIG,
    /* initSeq           */ kInitSeqES9069Q,
    /* initSeqLen        */ (uint8_t)(sizeof(kInitSeqES9069Q) / sizeof(kInitSeqES9069Q[0])),
    /* deinitSeq         */ kDeinitSeqES9069Q,
    /* deinitSeqLen      */ (uint8_t)(sizeof(kDeinitSeqES9069Q) / sizeof(kDeinitSeqES9069Q[0])),
    /* regFeature        */ ES9069Q_REG_MQA_CONTROL,
    /* featureEnableVal  */ ES9069Q_MQA_ENABLE_BIT,
    /* featureDisableVal */ 0x00,
    /* regFeatureStatus  */ ES9069Q_REG_MQA_CONTROL,
    /* featureStatusMask */ ES9069Q_MQA_STATUS_MASK,
    /* regClockSource    */ REG_NO_FEATURE,
    /* clkSrcEnabled     */ 0x00,
    /* clkSrcDisabled    */ 0x00,
    /* logPrefix         */ "[HAL:ES9069Q]",
};

const EssDac2chDescriptor kDescES9033Q = {
    /* compatible        */ "ess,es9033q",
    /* chipName          */ "ES9033Q",
    /* chipId            */ ES9033Q_CHIP_ID,
    /* capabilities      */ (uint16_t)(HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE | HAL_CAP_LINE_DRIVER | HAL_CAP_DPLL),
    /* sampleRateMask    */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K,
    /* supportedRates    */ kRates6,
    /* supportedRateCount*/ 6,
    /* regVolL           */ ES9033Q_REG_VOLUME_L,
    /* regVolR           */ ES9033Q_REG_VOLUME_R,
    /* regFilter         */ ES9033Q_REG_FILTER_SHAPE,
    /* volType           */ VOL_DUAL_0xFF,
    /* muteType          */ MUTE_VIA_VOLUME,
    /* muteBitMask       */ 0x00,
    /* filterType        */ FILTER_BITS_2_0,
    /* filterShift       */ 0,
    /* filterMask        */ 0x07,
    /* reconfigType      */ RECONFIG_WORD_LENGTH,
    /* regClockGear      */ REG_NO_FEATURE,
    /* regInputConfig    */ ES9033Q_REG_INPUT_CONFIG,
    /* initSeq           */ kInitSeqES9033Q,
    /* initSeqLen        */ (uint8_t)(sizeof(kInitSeqES9033Q) / sizeof(kInitSeqES9033Q[0])),
    /* deinitSeq         */ kDeinitSeqES9033Q,
    /* deinitSeqLen      */ (uint8_t)(sizeof(kDeinitSeqES9033Q) / sizeof(kDeinitSeqES9033Q[0])),
    /* regFeature        */ ES9033Q_REG_LINE_DRIVER,
    /* featureEnableVal  */ ES9033Q_LINE_DRIVER_INIT,
    /* featureDisableVal */ 0x00,
    /* regFeatureStatus  */ REG_NO_FEATURE,
    /* featureStatusMask */ 0x00,
    /* regClockSource    */ REG_NO_FEATURE,
    /* clkSrcEnabled     */ 0x00,
    /* clkSrcDisabled    */ 0x00,
    /* logPrefix         */ "[HAL:ES9033Q]",
};

const EssDac2chDescriptor kDescES9020Dac = {
    /* compatible        */ "ess,es9020-dac",
    /* chipName          */ "ES9020",
    /* chipId            */ ES9020_CHIP_ID,
    /* capabilities      */ (uint16_t)(HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS | HAL_CAP_APLL | HAL_CAP_DPLL),
    /* sampleRateMask    */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K,
    /* supportedRates    */ kRates4,
    /* supportedRateCount*/ 4,
    /* regVolL           */ ES9020_REG_VOLUME,
    /* regVolR           */ REG_NO_FEATURE,   // single-register chip
    /* regFilter         */ ES9020_REG_FILTER,
    /* volType           */ VOL_SINGLE_128,
    /* muteType          */ MUTE_VIA_VOLUME,
    /* muteBitMask       */ 0x00,
    /* filterType        */ FILTER_BITS_2_0,
    /* filterShift       */ 0,
    /* filterMask        */ ES9020_FILTER_SHAPE_MASK,
    /* reconfigType      */ RECONFIG_NONE,
    /* regClockGear      */ REG_NO_FEATURE,
    /* regInputConfig    */ ES9020_REG_INPUT_CONFIG,
    /* initSeq           */ kInitSeqES9020,
    /* initSeqLen        */ (uint8_t)(sizeof(kInitSeqES9020) / sizeof(kInitSeqES9020[0])),
    /* deinitSeq         */ kDeinitSeqES9020,
    /* deinitSeqLen      */ (uint8_t)(sizeof(kDeinitSeqES9020) / sizeof(kDeinitSeqES9020[0])),
    /* regFeature        */ ES9020_REG_APLL_CTRL,
    /* featureEnableVal  */ ES9020_APLL_ENABLE_BIT,
    /* featureDisableVal */ 0x00,
    /* regFeatureStatus  */ ES9020_REG_APLL_CTRL,
    /* featureStatusMask */ ES9020_APLL_LOCK_BIT,
    /* regClockSource    */ ES9020_REG_CLK_SOURCE,
    /* clkSrcEnabled     */ ES9020_CLK_BCK_RECOVERY,
    /* clkSrcDisabled    */ ES9020_CLK_MCLK,
    /* logPrefix         */ "[HAL:ES9020]",
};

// ===========================================================================
// Constructor
// ===========================================================================

HalEssDac2ch::HalEssDac2ch(const EssDac2chDescriptor& desc)
    : HalEssSabreDacBase(), _desc(desc)
{
    hal_init_descriptor(_descriptor, desc.compatible, desc.chipName, "ESS Technology",
        HAL_DEV_DAC, 2, ESS_SABRE_DAC_I2C_ADDR_BASE, HAL_BUS_I2C, HAL_I2C_BUS_EXP,
        desc.sampleRateMask, desc.capabilities);
    _initPriority = HAL_PRIORITY_HARDWARE;
    _i2cAddr      = ESS_SABRE_DAC_I2C_ADDR_BASE;
    _sdaPin       = ESS_SABRE_I2C_BUS2_SDA;
    _sclPin       = ESS_SABRE_I2C_BUS2_SCL;
    _i2cBusIndex  = HAL_I2C_BUS_EXP;
}

// ===========================================================================
// Private helpers
// ===========================================================================

void HalEssDac2ch::_execSequence(const EssDac2chRegWrite* seq, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        if (seq[i].reg == DELAY_SENTINEL) {
#ifndef NATIVE_TEST
            delay((uint32_t)seq[i].val * 5u);
#endif
        } else {
            _writeReg(seq[i].reg, seq[i].val);
        }
    }
}

uint8_t HalEssDac2ch::_computeClockGear(uint32_t sampleRate) const {
    // 0x00=1x (<=192kHz), 0x01=2x (<=384kHz), 0x02=4x (>384kHz)
    if (sampleRate > 384000) return 0x02;
    if (sampleRate > 192000) return 0x01;
    return 0x00;
}

uint8_t HalEssDac2ch::_computeWordLength(uint8_t bitDepth) const {
    // ES9069Q / ES9033Q REG_INPUT_CONFIG encoding:
    //   32-bit = 0x00, 24-bit = 0x40, 16-bit = 0xC0
    if (bitDepth == 16) return 0xC0;
    if (bitDepth == 24) return 0x40;
    return 0x00;  // 32-bit default
}

// ===========================================================================
// HalDevice lifecycle
// ===========================================================================

bool HalEssDac2ch::probe() {
#ifndef NATIVE_TEST
    if (!_bus().probe(_i2cAddr)) return false;
    uint8_t chipId = _readReg(ESS_SABRE_REG_CHIP_ID);
    return (chipId == _desc.chipId);
#else
    return true;
#endif
}

HalInitResult HalEssDac2ch::init() {
    // 1. Apply per-device config overrides from HAL Device Manager
    _applyConfigOverrides();

    LOG_I("%s Initializing (I2C addr=0x%02X bus=%u SDA=%d SCL=%d sr=%luHz bits=%u)",
          _desc.logPrefix, _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth);

    // 2. Select TwoWire instance and initialize I2C bus at 400 kHz
    _selectWire();

    // 3. Verify chip ID (log warning on mismatch — continue)
    uint8_t chipId = _readReg(ESS_SABRE_REG_CHIP_ID);
    if (chipId != _desc.chipId) {
        LOG_W("%s Unexpected chip ID: 0x%02X (expected 0x%02X) — continuing",
              _desc.logPrefix, chipId, _desc.chipId);
    } else {
        LOG_I("%s Chip ID OK (0x%02X)", _desc.logPrefix, chipId);
    }

    // 4. Run chip-specific init sequence (resets, format config, DPLL, etc.)
    _execSequence(_desc.initSeq, _desc.initSeqLen);

    // 5. Clock gear / word length based on current sample rate
    if (_desc.reconfigType == RECONFIG_CLOCK_GEAR && _desc.regClockGear != REG_NO_FEATURE) {
        _writeReg(_desc.regClockGear, _computeClockGear(_sampleRate));
    }

    // 6. Apply stored volume
    uint8_t volReg;
    switch (_desc.volType) {
        case VOL_SINGLE_128:
            volReg = _muted ? 0xFF : (uint8_t)(((uint16_t)(100U - _volume) * ESS_SABRE_DAC_VOL_STEPS) / 100U);
            _writeReg(_desc.regVolL, volReg);
            break;
        case VOL_DUAL_0xFF:
        default:
            if (_desc.muteType == MUTE_VIA_VOLUME && _muted) {
                _writeReg(_desc.regVolL, 0xFF);
                if (_desc.regVolR != REG_NO_FEATURE) _writeReg(_desc.regVolR, 0xFF);
            } else {
                volReg = (uint8_t)((100U - _volume) * 255U / 100U);
                _writeReg(_desc.regVolL, volReg);
                if (_desc.regVolR != REG_NO_FEATURE) _writeReg(_desc.regVolR, volReg);
            }
            break;
    }

    // 7. Apply stored filter preset
    uint8_t preset = (_filterPreset >= ESS_SABRE_FILTER_COUNT) ? 0 : _filterPreset;
    if (_desc.filterType == FILTER_BITS_4_2_WITH_MUTE) {
        uint8_t reg = (uint8_t)((preset & 0x07u) << _desc.filterShift);
        if (_muted) reg |= _desc.muteBitMask;
        _writeReg(_desc.regFilter, reg);
    } else {
        _writeReg(_desc.regFilter, (uint8_t)(preset & _desc.filterMask));
    }

    // 8. Enable expansion I2S TX output
    if (!_enableI2sTx()) {
        LOG_E("%s Expansion I2S TX enable failed", _desc.logPrefix);
        _state = HAL_STATE_ERROR;
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "I2S TX init failed");
    }

    // 9. Mark device ready
    _initialized = true;
    _state = HAL_STATE_AVAILABLE;
    setReady(true);

    LOG_I("%s Ready (vol=%u%% muted=%d filter=%u)", _desc.logPrefix, _volume, (int)_muted, _filterPreset);
    return hal_init_ok();
}

void HalEssDac2ch::deinit() {
    if (!_initialized) return;

    setReady(false);

    // Run chip-specific deinit sequence (mute output, disable features, etc.)
    _execSequence(_desc.deinitSeq, _desc.deinitSeqLen);

    _disableI2sTx();

    _featureEnabled = false;
    _initialized    = false;
    _i2sTxEnabled   = false;
    _state = HAL_STATE_REMOVED;

    LOG_I("%s Deinitialized", _desc.logPrefix);
}

void HalEssDac2ch::dumpConfig() {
    LOG_I("%s %s by ESS Technology (compat=%s) i2c=0x%02X bus=%u sda=%d scl=%d "
          "sr=%luHz bits=%u vol=%u%% muted=%d filter=%u",
          _desc.logPrefix, _descriptor.name, _descriptor.compatible,
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth,
          _volume, (int)_muted, _filterPreset);
}

bool HalEssDac2ch::healthCheck() {
#ifndef NATIVE_TEST
    if (!_initialized) return false;
    uint8_t id = _readReg(ESS_SABRE_REG_CHIP_ID);
    return (id == _desc.chipId);
#else
    return _initialized;
#endif
}

ClockStatus HalEssDac2ch::getClockStatus() {
    if (!_initialized || _state != HAL_STATE_AVAILABLE) {
        return {false, false, "not available"};
    }
#ifndef NATIVE_TEST
    uint8_t status = _readReg(ESS_SABRE_REG_DPLL_LOCK);
    bool locked = (status & ESS_SABRE_DPLL_LOCKED_BIT) != 0;
    return {true, locked, locked ? "DPLL locked" : "DPLL unlocked"};
#else
    return {true, true, "DPLL locked"};
#endif
}

// ===========================================================================
// HalAudioDevice
// ===========================================================================

bool HalEssDac2ch::configure(uint32_t sampleRate, uint8_t bitDepth) {
    if (!_validateSampleRate(sampleRate, _desc.supportedRates, _desc.supportedRateCount)) {
        LOG_W("%s Unsupported sample rate: %luHz", _desc.logPrefix, (unsigned long)sampleRate);
        return false;
    }
    if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32) {
        LOG_W("%s Unsupported bit depth: %u", _desc.logPrefix, bitDepth);
        return false;
    }

    _sampleRate = sampleRate;
    _bitDepth   = bitDepth;

    if (_initialized) {
        switch (_desc.reconfigType) {
            case RECONFIG_CLOCK_GEAR:
                if (_desc.regClockGear != REG_NO_FEATURE) {
                    _writeReg(_desc.regClockGear, _computeClockGear(sampleRate));
                }
                break;
            case RECONFIG_WORD_LENGTH:
                _writeReg(_desc.regInputConfig, _computeWordLength(bitDepth));
                break;
            case RECONFIG_NONE:
            default:
                break;
        }
    }

    LOG_I("%s Configured: %luHz %ubit", _desc.logPrefix, (unsigned long)sampleRate, bitDepth);
    return true;
}

bool HalEssDac2ch::setVolume(uint8_t percent) {
    if (!_initialized) return false;
    if (percent > 100) percent = 100;

    bool ok;
    switch (_desc.volType) {
        case VOL_SINGLE_128: {
            if (_muted) { _volume = percent; return true; }  // defer — mute holds 0xFF
            uint8_t attenReg = (uint8_t)(((uint16_t)(100U - percent) * ESS_SABRE_DAC_VOL_STEPS) / 100U);
            ok = _writeReg(_desc.regVolL, attenReg);
            LOG_D("%s Volume: %d%% -> attenReg=0x%02X", _desc.logPrefix, percent, attenReg);
            break;
        }
        case VOL_DUAL_0xFF:
        default: {
            uint8_t volReg = (uint8_t)((100U - percent) * 255U / 100U);
            ok  = _writeReg(_desc.regVolL, volReg);
            if (_desc.regVolR != REG_NO_FEATURE) ok = ok && _writeReg(_desc.regVolR, volReg);
            LOG_D("%s Volume: %d%% -> reg=0x%02X", _desc.logPrefix, percent, volReg);
            break;
        }
    }

    _volume = percent;
    return ok;
}

bool HalEssDac2ch::setMute(bool mute) {
    if (!_initialized) return false;

    bool ok;
    switch (_desc.muteType) {
        case MUTE_VIA_DEDICATED_BIT: {
            uint8_t reg = _readReg(_desc.regFilter);
            if (mute) reg |=  _desc.muteBitMask;
            else      reg &= (uint8_t)~_desc.muteBitMask;
            ok = _writeReg(_desc.regFilter, reg);
            break;
        }
        case MUTE_VIA_VOLUME:
        default: {
            if (mute) {
                ok  = _writeReg(_desc.regVolL, 0xFF);
                if (_desc.regVolR != REG_NO_FEATURE) ok = ok && _writeReg(_desc.regVolR, 0xFF);
            } else {
                // Restore current volume
                uint8_t val;
                if (_desc.volType == VOL_SINGLE_128) {
                    val = (uint8_t)(((uint16_t)(100U - _volume) * ESS_SABRE_DAC_VOL_STEPS) / 100U);
                    ok = _writeReg(_desc.regVolL, val);
                } else {
                    val = (uint8_t)((100U - _volume) * 255U / 100U);
                    ok  = _writeReg(_desc.regVolL, val);
                    if (_desc.regVolR != REG_NO_FEATURE) ok = ok && _writeReg(_desc.regVolR, val);
                }
            }
            break;
        }
    }

    _muted = mute;
    LOG_I("%s %s", _desc.logPrefix, mute ? "Muted" : "Unmuted");
    return ok;
}

// ===========================================================================
// Filter preset
// ===========================================================================

bool HalEssDac2ch::setFilterPreset(uint8_t preset) {
    if (preset >= ESS_SABRE_FILTER_COUNT) {
        LOG_W("%s Invalid filter preset: %u (max %u)", _desc.logPrefix, preset, ESS_SABRE_FILTER_COUNT - 1);
        return false;
    }

    bool ok = true;
    if (_initialized) {
        if (_desc.filterType == FILTER_BITS_4_2_WITH_MUTE) {
            uint8_t reg = _readReg(_desc.regFilter);
            reg &= (uint8_t)~_desc.filterMask;
            reg |= (uint8_t)((preset & 0x07u) << _desc.filterShift);
            ok = _writeReg(_desc.regFilter, reg);
        } else {
            ok = _writeReg(_desc.regFilter, (uint8_t)(preset & _desc.filterMask));
        }
    }

    _filterPreset = preset;
    LOG_I("%s Filter preset: %u", _desc.logPrefix, preset);
    return ok;
}

// ===========================================================================
// Generic chip-specific feature (MQA / line driver / APLL)
// ===========================================================================

bool HalEssDac2ch::setFeatureEnabled(bool enable) {
    if (_desc.regFeature == REG_NO_FEATURE) return false;

    _featureEnabled = enable;

    if (!_initialized) return true;  // Deferred — applied at next init

    // For APLL (ES9020): write clock source first, then APLL control
    if (_desc.regClockSource != REG_NO_FEATURE) {
        uint8_t clkSrc = enable ? _desc.clkSrcEnabled : _desc.clkSrcDisabled;
        if (!_writeReg(_desc.regClockSource, clkSrc)) return false;
    }

    // For MQA (ES9069Q): read-modify-write to preserve other bits
    if (_desc.regFeatureStatus == _desc.regFeature) {
        uint8_t cur  = _readReg(_desc.regFeature);
        uint8_t next = enable ? (uint8_t)(cur | _desc.featureEnableVal)
                              : (uint8_t)(cur & (uint8_t)~_desc.featureEnableVal);
        bool ok = _writeReg(_desc.regFeature, next);
        LOG_I("%s Feature %s", _desc.logPrefix, enable ? "enabled" : "disabled");
        return ok;
    }

    // For line driver (ES9033Q): simple write
    bool ok = _writeReg(_desc.regFeature, enable ? _desc.featureEnableVal : _desc.featureDisableVal);
    LOG_I("%s Feature %s", _desc.logPrefix, enable ? "enabled" : "disabled");
    return ok;
}

bool HalEssDac2ch::isFeatureActive() const {
    if (_desc.regFeature == REG_NO_FEATURE) return false;
    if (_desc.regFeatureStatus == REG_NO_FEATURE) return _featureEnabled;
    if (!_initialized) return false;

    uint8_t val = const_cast<HalEssDac2ch*>(this)->_readReg(_desc.regFeatureStatus);
    return (val & _desc.featureStatusMask) != 0;
}

#endif // DAC_ENABLED
