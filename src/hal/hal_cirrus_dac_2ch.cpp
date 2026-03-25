#ifdef DAC_ENABLED
// HalCirrusDac2ch — Generic Cirrus Logic 2-channel DAC driver implementation.
// Descriptor tables for CS43198, CS43131, CS4398, CS4399, CS43130.
// All 5 chips share the same init/configure/volume/mute/filter logic driven
// by per-chip CirrusDac2chDescriptor structs.

#include "hal_cirrus_dac_2ch.h"
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
#define REG_NO_FEATURE_16  0xFFFF  // Indicates "no feature" (16-bit address space)
#define DELAY_SENTINEL_16  0xFFFF  // reg==0xFFFF in init sequence means delay(val*5ms)

// ----- CS43198 (chip ID 0x98, 16-bit paged) -----
#define CS43198_I2C_ADDR              0x48
#define CS43198_CHIP_ID               0x98
#define CS43198_CHIP_ID_MASK          0xFF
#define CS43198_REG_DEVID_REVID       0x0001
#define CS43198_REG_POWER_CTL         0x0006
#define CS43198_REG_IFACE_CTL         0x0009
#define CS43198_REG_PCM_PATH          0x0020
#define CS43198_REG_VOL_A             0x001E
#define CS43198_REG_VOL_B             0x001F
#define CS43198_REG_CLOCK_CTL         0x000C
#define CS43198_REG_DSD_PATH          0x0030
#define CS43198_REG_DSD_INT           0x0031
#define CS43198_DSD_PATH_ENABLE       0x01
#define CS43198_DSD_PATH_DISABLE      0x00
#define CS43198_DSD_INT_DOP           0x01
#define CS43198_PDN_BIT               0x01
#define CS43198_FMT_I2S               0x00
#define CS43198_WL_32BIT              0x00
#define CS43198_WL_24BIT              0x10
#define CS43198_WL_16BIT              0x30
#define CS43198_MUTE_A_BIT            0x40
#define CS43198_MUTE_B_BIT            0x80
#define CS43198_MUTE_BOTH             (CS43198_MUTE_A_BIT | CS43198_MUTE_B_BIT)
#define CS43198_FILTER_MASK           0x07
#define CS43198_SPEED_NORMAL          0x00
#define CS43198_SPEED_DOUBLE          0x10
#define CS43198_SPEED_QUAD            0x20
#define CS43198_SPEED_OCTAL           0x30
#define CS43198_FILTER_COUNT          7
#define CS43198_IFACE_DEFAULT         (CS43198_FMT_I2S | CS43198_WL_32BIT)

// ----- CS43131 (chip ID 0x99, 16-bit paged, + HP amp) -----
#define CS43131_I2C_ADDR              0x48
#define CS43131_CHIP_ID               0x99
#define CS43131_CHIP_ID_MASK          0xFF
#define CS43131_REG_DEVID_REVID       0x0001
#define CS43131_REG_POWER_CTL         0x0006
#define CS43131_REG_IFACE_CTL         0x0009
#define CS43131_REG_PCM_PATH          0x0020
#define CS43131_REG_VOL_A             0x001E
#define CS43131_REG_VOL_B             0x001F
#define CS43131_REG_CLOCK_CTL         0x000C
#define CS43131_REG_HP_CTL            0x0032
#define CS43131_REG_DSD_PATH          0x0030
#define CS43131_REG_DSD_INT           0x0031
#define CS43131_DSD_PATH_ENABLE       0x01
#define CS43131_DSD_PATH_DISABLE      0x00
#define CS43131_DSD_INT_DOP           0x01
#define CS43131_HP_AMP_ENABLE         0x01
#define CS43131_HP_AMP_DISABLE        0x00
#define CS43131_PDN_BIT               0x01
#define CS43131_FMT_I2S               0x00
#define CS43131_WL_32BIT              0x00
#define CS43131_WL_24BIT              0x10
#define CS43131_WL_16BIT              0x30
#define CS43131_MUTE_A_BIT            0x40
#define CS43131_MUTE_B_BIT            0x80
#define CS43131_MUTE_BOTH             (CS43131_MUTE_A_BIT | CS43131_MUTE_B_BIT)
#define CS43131_FILTER_MASK           0x07
#define CS43131_SPEED_NORMAL          0x00
#define CS43131_SPEED_DOUBLE          0x10
#define CS43131_SPEED_QUAD            0x20
#define CS43131_SPEED_OCTAL           0x30
#define CS43131_FILTER_COUNT          7
#define CS43131_IFACE_DEFAULT         (CS43131_FMT_I2S | CS43131_WL_32BIT)

// ----- CS4398 (chip ID 0x72, 8-bit regs, I2C addr 0x4C) -----
#define CS4398_I2C_ADDR               0x4C
#define CS4398_CHIP_ID                0x72
#define CS4398_CHIP_ID_MASK           0xF0   // Only upper nibble holds chip family ID
#define CS4398_REG_CHIP_ID            0x01
#define CS4398_REG_MODE_CTL           0x02
#define CS4398_REG_VOL_MIX_CTL        0x03
#define CS4398_REG_MUTE_CTL           0x04
#define CS4398_REG_VOL_A              0x05
#define CS4398_REG_VOL_B              0x06
#define CS4398_REG_RAMP_FILT          0x07
#define CS4398_REG_MISC_CTL           0x08
#define CS4398_MUTE_A_BIT             0x01
#define CS4398_MUTE_B_BIT             0x02
#define CS4398_MUTE_BOTH              (CS4398_MUTE_A_BIT | CS4398_MUTE_B_BIT)
#define CS4398_FILTER_MASK            0x0C
#define CS4398_FILTER_SHIFT           2
#define CS4398_PDN_BIT                0x80
#define CS4398_FM_MASK                0x30
#define CS4398_FM_SINGLE              0x00
// CS4398 DSD: CHSL bit6 in REG_MODE_CTL enables DSD input path
#define CS4398_DSD_BIT                0x40
#define CS4398_DSD_PATH_ENABLE        CS4398_DSD_BIT
#define CS4398_DSD_PATH_DISABLE       0x00
#define CS4398_FM_DOUBLE              0x10
#define CS4398_FM_QUAD                0x20
#define CS4398_CP_EN_BIT              0x80
#define CS4398_DIF1_I2S               0x80
#define CS4398_FILTER_COUNT           3
#define CS4398_MODE_CTL_DEFAULT       (CS4398_CP_EN_BIT | CS4398_FM_SINGLE)
#define CS4398_VOL_MIX_DEFAULT        (CS4398_DIF1_I2S)
#define CS4398_WL_24BIT               0x00  // CS4398 max is 24-bit; no separate WL bits needed
#define CS4398_WL_16BIT               0x00  // No I2S word-length control reg on CS4398

// ----- CS4399 (chip ID 0x97, 16-bit paged, + NOS mode) -----
#define CS4399_I2C_ADDR               0x48
#define CS4399_CHIP_ID                0x97
#define CS4399_CHIP_ID_MASK           0xFF
#define CS4399_REG_DEVID_REVID        0x0001
#define CS4399_REG_POWER_CTL          0x0006
#define CS4399_REG_IFACE_CTL          0x0009
#define CS4399_REG_PCM_PATH           0x0020
#define CS4399_REG_VOL_A              0x001E
#define CS4399_REG_VOL_B              0x001F
#define CS4399_REG_CLOCK_CTL          0x000C
#define CS4399_REG_NOS_CTL            0x001C
#define CS4399_NOS_ENABLE             0x01
#define CS4399_NOS_DISABLE            0x00
#define CS4399_PDN_BIT                0x01
#define CS4399_FMT_I2S                0x00
#define CS4399_WL_32BIT               0x00
#define CS4399_WL_24BIT               0x10
#define CS4399_WL_16BIT               0x30
#define CS4399_MUTE_A_BIT             0x40
#define CS4399_MUTE_B_BIT             0x80
#define CS4399_MUTE_BOTH              (CS4399_MUTE_A_BIT | CS4399_MUTE_B_BIT)
#define CS4399_FILTER_MASK            0x07
#define CS4399_SPEED_NORMAL           0x00
#define CS4399_SPEED_DOUBLE           0x10
#define CS4399_SPEED_QUAD             0x20
#define CS4399_SPEED_OCTAL            0x30
#define CS4399_FILTER_COUNT           5
#define CS4399_IFACE_DEFAULT          (CS4399_FMT_I2S | CS4399_WL_32BIT)

// ----- CS43130 (chip ID 0x96, 16-bit paged, + HP amp + NOS mode, DSD64 only) -----
#define CS43130_I2C_ADDR              0x48
#define CS43130_CHIP_ID               0x96
#define CS43130_CHIP_ID_MASK          0xFF
#define CS43130_REG_DEVID_REVID       0x0001
#define CS43130_REG_POWER_CTL         0x0006
#define CS43130_REG_IFACE_CTL         0x0009
#define CS43130_REG_PCM_PATH          0x0020
#define CS43130_REG_VOL_A             0x001E
#define CS43130_REG_VOL_B             0x001F
#define CS43130_REG_CLOCK_CTL         0x000C
#define CS43130_REG_NOS_CTL           0x001C
#define CS43130_NOS_ENABLE            0x01
#define CS43130_NOS_DISABLE           0x00
#define CS43130_REG_HP_CTL            0x0032
#define CS43130_REG_DSD_PATH          0x0030
#define CS43130_REG_DSD_INT           0x0031
#define CS43130_DSD_PATH_ENABLE       0x01
#define CS43130_DSD_PATH_DISABLE      0x00
#define CS43130_DSD_INT_DOP           0x01
#define CS43130_HP_AMP_ENABLE         0x01
#define CS43130_HP_AMP_DISABLE        0x00
#define CS43130_PDN_BIT               0x01
#define CS43130_FMT_I2S               0x00
#define CS43130_WL_32BIT              0x00
#define CS43130_WL_24BIT              0x10
#define CS43130_WL_16BIT              0x30
#define CS43130_MUTE_A_BIT            0x40
#define CS43130_MUTE_B_BIT            0x80
#define CS43130_MUTE_BOTH             (CS43130_MUTE_A_BIT | CS43130_MUTE_B_BIT)
#define CS43130_FILTER_MASK           0x07
#define CS43130_SPEED_NORMAL          0x00
#define CS43130_SPEED_DOUBLE          0x10
#define CS43130_SPEED_QUAD            0x20
#define CS43130_SPEED_OCTAL           0x30
#define CS43130_FILTER_COUNT          5
#define CS43130_IFACE_DEFAULT         (CS43130_FMT_I2S | CS43130_WL_32BIT)

// ===========================================================================
// Init/deinit sequences
// ===========================================================================

// ----- CS43198 -----
// Power up, configure I2S Philips/32-bit, set speed; filter/volume set in init().
static const CirrusDac2chRegWrite kInitSeqCS43198[] = {
    { CS43198_REG_POWER_CTL, 0x00 },          // clear PDN — power up
    { DELAY_SENTINEL_16, 2          },         // delay 10ms
    { CS43198_REG_IFACE_CTL, CS43198_IFACE_DEFAULT },  // I2S slave, 32-bit
};
static const CirrusDac2chRegWrite kDeinitSeqCS43198[] = {
    { CS43198_REG_VOL_A, 0xFF },              // mute channel A
    { CS43198_REG_VOL_B, 0xFF },              // mute channel B
    { CS43198_REG_POWER_CTL, CS43198_PDN_BIT },  // power down
};

// ----- CS43131 -----
// Identical to CS43198; HP amp disabled separately in init() via descriptor.
static const CirrusDac2chRegWrite kInitSeqCS43131[] = {
    { CS43131_REG_POWER_CTL, 0x00 },
    { DELAY_SENTINEL_16, 2          },
    { CS43131_REG_IFACE_CTL, CS43131_IFACE_DEFAULT },
};
static const CirrusDac2chRegWrite kDeinitSeqCS43131[] = {
    { CS43131_REG_HP_CTL, CS43131_HP_AMP_DISABLE },  // disable HP amp first
    { CS43131_REG_VOL_A, 0xFF },
    { CS43131_REG_VOL_B, 0xFF },
    { CS43131_REG_POWER_CTL, CS43131_PDN_BIT },
};

// ----- CS4398 -----
// Enable control port, set PCM mode + speed, set I2S format, power up.
static const CirrusDac2chRegWrite kInitSeqCS4398[] = {
    { CS4398_REG_MODE_CTL,    CS4398_MODE_CTL_DEFAULT },  // CP_EN + FM_SINGLE
    { CS4398_REG_VOL_MIX_CTL, CS4398_VOL_MIX_DEFAULT },  // DIF1=1 (I2S format)
    { CS4398_REG_MISC_CTL,    0x00                    },  // power up, all features off
    { DELAY_SENTINEL_16, 2                             },  // delay 10ms
};
static const CirrusDac2chRegWrite kDeinitSeqCS4398[] = {
    { CS4398_REG_VOL_A,    0xFF },
    { CS4398_REG_VOL_B,    0xFF },
    { CS4398_REG_MUTE_CTL, CS4398_MUTE_BOTH },
    { CS4398_REG_MISC_CTL, CS4398_PDN_BIT   },
};

// ----- CS4399 -----
// NOS control disabled in init() via descriptor regNos.
static const CirrusDac2chRegWrite kInitSeqCS4399[] = {
    { CS4399_REG_POWER_CTL, 0x00 },
    { DELAY_SENTINEL_16, 2          },
    { CS4399_REG_IFACE_CTL, CS4399_IFACE_DEFAULT },
};
static const CirrusDac2chRegWrite kDeinitSeqCS4399[] = {
    { CS4399_REG_NOS_CTL,   CS4399_NOS_DISABLE   },
    { CS4399_REG_VOL_A,     0xFF                 },
    { CS4399_REG_VOL_B,     0xFF                 },
    { CS4399_REG_POWER_CTL, CS4399_PDN_BIT       },
};

// ----- CS43130 -----
// HP amp and NOS disabled in init() via descriptor regHpAmp/regNos.
static const CirrusDac2chRegWrite kInitSeqCS43130[] = {
    { CS43130_REG_POWER_CTL, 0x00 },
    { DELAY_SENTINEL_16, 2          },
    { CS43130_REG_IFACE_CTL, CS43130_IFACE_DEFAULT },
};
static const CirrusDac2chRegWrite kDeinitSeqCS43130[] = {
    { CS43130_REG_HP_CTL,   CS43130_HP_AMP_DISABLE },
    { CS43130_REG_NOS_CTL,  CS43130_NOS_DISABLE    },
    { CS43130_REG_VOL_A,    0xFF                   },
    { CS43130_REG_VOL_B,    0xFF                   },
    { CS43130_REG_POWER_CTL, CS43130_PDN_BIT       },
};

// ===========================================================================
// Supported sample rate tables
// ===========================================================================

static const uint32_t kCirrusRates5[]  = { 44100, 48000, 96000, 192000, 384000 };
static const uint32_t kCirrusRates4[]  = { 44100, 48000, 96000, 192000 };

// ===========================================================================
// Descriptor tables
// ===========================================================================

const CirrusDac2chDescriptor kDescCS43198 = {
    /* compatible         */ "cirrus,cs43198",
    /* chipName           */ "CS43198",
    /* chipId             */ CS43198_CHIP_ID,
    /* chipIdMask         */ CS43198_CHIP_ID_MASK,
    /* capabilities       */ (uint16_t)(HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS | HAL_CAP_DSD),
    /* sampleRateMask     */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K,
    /* supportedRates     */ kCirrusRates5,
    /* supportedRateCount */ 5,
    /* i2cAddr            */ CS43198_I2C_ADDR,
    /* maxBitDepth        */ 32,
    /* regType            */ REG_16BIT_PAGED,
    /* regChipId          */ CS43198_REG_DEVID_REVID,
    /* regPowerCtl        */ CS43198_REG_POWER_CTL,
    /* regIfaceCtl        */ CS43198_REG_IFACE_CTL,
    /* regPcmPath         */ CS43198_REG_PCM_PATH,
    /* regVolA            */ CS43198_REG_VOL_A,
    /* regVolB            */ CS43198_REG_VOL_B,
    /* regClockCtl        */ CS43198_REG_CLOCK_CTL,
    /* regModeCtl         */ REG_NO_FEATURE_16,
    /* regMuteCtl         */ REG_NO_FEATURE_16,
    /* ifaceDefault       */ CS43198_IFACE_DEFAULT,
    /* wl16Bit            */ CS43198_WL_16BIT,
    /* wl24Bit            */ CS43198_WL_24BIT,
    /* wl32Bit            */ CS43198_WL_32BIT,
    /* muteType           */ MUTE_PCM_PATH_RMW,
    /* muteABit           */ CS43198_MUTE_A_BIT,
    /* muteBBit           */ CS43198_MUTE_B_BIT,
    /* muteBoth           */ CS43198_MUTE_BOTH,
    /* filterCount        */ CS43198_FILTER_COUNT,
    /* filterMask         */ CS43198_FILTER_MASK,
    /* filterShift        */ 0,
    /* regFilter          */ CS43198_REG_PCM_PATH,
    /* speedType          */ SPEED_CLOCK_CTL,
    /* speedNormal        */ CS43198_SPEED_NORMAL,
    /* speedDouble        */ CS43198_SPEED_DOUBLE,
    /* speedQuad          */ CS43198_SPEED_QUAD,
    /* speedOctal         */ CS43198_SPEED_OCTAL,
    /* speedFmMask        */ 0x00,
    /* initSeq            */ kInitSeqCS43198,
    /* initSeqLen         */ (uint8_t)(sizeof(kInitSeqCS43198) / sizeof(kInitSeqCS43198[0])),
    /* deinitSeq          */ kDeinitSeqCS43198,
    /* deinitSeqLen       */ (uint8_t)(sizeof(kDeinitSeqCS43198) / sizeof(kDeinitSeqCS43198[0])),
    /* regHpAmp           */ REG_NO_FEATURE_16,
    /* hpAmpEnableVal     */ 0x00,
    /* hpAmpDisableVal    */ 0x00,
    /* regNos             */ REG_NO_FEATURE_16,
    /* nosEnableVal       */ 0x00,
    /* nosDisableVal      */ 0x00,
    /* regDsdPath         */ CS43198_REG_DSD_PATH,
    /* dsdPathEnable      */ CS43198_DSD_PATH_ENABLE,
    /* dsdPathDisable     */ CS43198_DSD_PATH_DISABLE,
    /* dsdPathMask        */ 0x01,
    /* regDsdInt          */ CS43198_REG_DSD_INT,
    /* dsdIntDefault      */ CS43198_DSD_INT_DOP,
    /* dsdFuncMode        */ 0xFF,
    /* logPrefix          */ "[HAL:CS43198]",
};

const CirrusDac2chDescriptor kDescCS43131 = {
    /* compatible         */ "cirrus,cs43131",
    /* chipName           */ "CS43131",
    /* chipId             */ CS43131_CHIP_ID,
    /* chipIdMask         */ CS43131_CHIP_ID_MASK,
    /* capabilities       */ (uint16_t)(HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS | HAL_CAP_DSD | HAL_CAP_HP_AMP),
    /* sampleRateMask     */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K,
    /* supportedRates     */ kCirrusRates5,
    /* supportedRateCount */ 5,
    /* i2cAddr            */ CS43131_I2C_ADDR,
    /* maxBitDepth        */ 32,
    /* regType            */ REG_16BIT_PAGED,
    /* regChipId          */ CS43131_REG_DEVID_REVID,
    /* regPowerCtl        */ CS43131_REG_POWER_CTL,
    /* regIfaceCtl        */ CS43131_REG_IFACE_CTL,
    /* regPcmPath         */ CS43131_REG_PCM_PATH,
    /* regVolA            */ CS43131_REG_VOL_A,
    /* regVolB            */ CS43131_REG_VOL_B,
    /* regClockCtl        */ CS43131_REG_CLOCK_CTL,
    /* regModeCtl         */ REG_NO_FEATURE_16,
    /* regMuteCtl         */ REG_NO_FEATURE_16,
    /* ifaceDefault       */ CS43131_IFACE_DEFAULT,
    /* wl16Bit            */ CS43131_WL_16BIT,
    /* wl24Bit            */ CS43131_WL_24BIT,
    /* wl32Bit            */ CS43131_WL_32BIT,
    /* muteType           */ MUTE_PCM_PATH_RMW,
    /* muteABit           */ CS43131_MUTE_A_BIT,
    /* muteBBit           */ CS43131_MUTE_B_BIT,
    /* muteBoth           */ CS43131_MUTE_BOTH,
    /* filterCount        */ CS43131_FILTER_COUNT,
    /* filterMask         */ CS43131_FILTER_MASK,
    /* filterShift        */ 0,
    /* regFilter          */ CS43131_REG_PCM_PATH,
    /* speedType          */ SPEED_CLOCK_CTL,
    /* speedNormal        */ CS43131_SPEED_NORMAL,
    /* speedDouble        */ CS43131_SPEED_DOUBLE,
    /* speedQuad          */ CS43131_SPEED_QUAD,
    /* speedOctal         */ CS43131_SPEED_OCTAL,
    /* speedFmMask        */ 0x00,
    /* initSeq            */ kInitSeqCS43131,
    /* initSeqLen         */ (uint8_t)(sizeof(kInitSeqCS43131) / sizeof(kInitSeqCS43131[0])),
    /* deinitSeq          */ kDeinitSeqCS43131,
    /* deinitSeqLen       */ (uint8_t)(sizeof(kDeinitSeqCS43131) / sizeof(kDeinitSeqCS43131[0])),
    /* regHpAmp           */ CS43131_REG_HP_CTL,
    /* hpAmpEnableVal     */ CS43131_HP_AMP_ENABLE,
    /* hpAmpDisableVal    */ CS43131_HP_AMP_DISABLE,
    /* regNos             */ REG_NO_FEATURE_16,
    /* nosEnableVal       */ 0x00,
    /* nosDisableVal      */ 0x00,
    /* regDsdPath         */ CS43131_REG_DSD_PATH,
    /* dsdPathEnable      */ CS43131_DSD_PATH_ENABLE,
    /* dsdPathDisable     */ CS43131_DSD_PATH_DISABLE,
    /* dsdPathMask        */ 0x01,
    /* regDsdInt          */ CS43131_REG_DSD_INT,
    /* dsdIntDefault      */ CS43131_DSD_INT_DOP,
    /* dsdFuncMode        */ 0xFF,
    /* logPrefix          */ "[HAL:CS43131]",
};

const CirrusDac2chDescriptor kDescCS4398 = {
    /* compatible         */ "cirrus,cs4398",
    /* chipName           */ "CS4398",
    /* chipId             */ CS4398_CHIP_ID,
    /* chipIdMask         */ CS4398_CHIP_ID_MASK,
    /* capabilities       */ (uint16_t)(HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS | HAL_CAP_DSD),
    /* sampleRateMask     */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K,
    /* supportedRates     */ kCirrusRates4,
    /* supportedRateCount */ 4,
    /* i2cAddr            */ CS4398_I2C_ADDR,   // 0x4C — different from other Cirrus DACs
    /* maxBitDepth        */ 24,                 // CS4398 max is 24-bit
    /* regType            */ REG_8BIT,
    /* regChipId          */ CS4398_REG_CHIP_ID,
    /* regPowerCtl        */ REG_NO_FEATURE_16,  // no separate power reg; PDN via REG_MISC_CTL
    /* regIfaceCtl        */ REG_NO_FEATURE_16,  // no separate iface CTL; DIF bits in mode/vol_mix regs
    /* regPcmPath         */ REG_NO_FEATURE_16,  // no PCM path reg
    /* regVolA            */ CS4398_REG_VOL_A,
    /* regVolB            */ CS4398_REG_VOL_B,
    /* regClockCtl        */ REG_NO_FEATURE_16,  // speed via FM bits in regModeCtl
    /* regModeCtl         */ CS4398_REG_MODE_CTL,
    /* regMuteCtl         */ CS4398_REG_MUTE_CTL,
    /* ifaceDefault       */ 0x00,               // N/A for CS4398
    /* wl16Bit            */ CS4398_WL_16BIT,
    /* wl24Bit            */ CS4398_WL_24BIT,
    /* wl32Bit            */ 0x00,               // N/A: max 24-bit
    /* muteType           */ MUTE_DEDICATED_REG,
    /* muteABit           */ CS4398_MUTE_A_BIT,
    /* muteBBit           */ CS4398_MUTE_B_BIT,
    /* muteBoth           */ CS4398_MUTE_BOTH,
    /* filterCount        */ CS4398_FILTER_COUNT,
    /* filterMask         */ CS4398_FILTER_MASK,
    /* filterShift        */ CS4398_FILTER_SHIFT,
    /* regFilter          */ CS4398_REG_RAMP_FILT,
    /* speedType          */ SPEED_FM_BITS,
    /* speedNormal        */ CS4398_FM_SINGLE,
    /* speedDouble        */ CS4398_FM_DOUBLE,
    /* speedQuad          */ CS4398_FM_QUAD,
    /* speedOctal         */ CS4398_FM_QUAD,     // CS4398 max is quad (192kHz)
    /* speedFmMask        */ CS4398_FM_MASK,
    /* initSeq            */ kInitSeqCS4398,
    /* initSeqLen         */ (uint8_t)(sizeof(kInitSeqCS4398) / sizeof(kInitSeqCS4398[0])),
    /* deinitSeq          */ kDeinitSeqCS4398,
    /* deinitSeqLen       */ (uint8_t)(sizeof(kDeinitSeqCS4398) / sizeof(kDeinitSeqCS4398[0])),
    /* regHpAmp           */ REG_NO_FEATURE_16,
    /* hpAmpEnableVal     */ 0x00,
    /* hpAmpDisableVal    */ 0x00,
    /* regNos             */ REG_NO_FEATURE_16,
    /* nosEnableVal       */ 0x00,
    /* nosDisableVal      */ 0x00,
    /* regDsdPath         */ CS4398_REG_MODE_CTL,   // DSD via CHSL bit6 in MODE_CTL
    /* dsdPathEnable      */ CS4398_DSD_PATH_ENABLE,
    /* dsdPathDisable     */ CS4398_DSD_PATH_DISABLE,
    /* dsdPathMask        */ CS4398_DSD_BIT,        // Read-modify-write only the CHSL bit
    /* regDsdInt          */ REG_NO_FEATURE_16,     // CS4398 has no separate DSD interface reg
    /* dsdIntDefault      */ 0x00,
    /* dsdFuncMode        */ 0xFF,
    /* logPrefix          */ "[HAL:CS4398]",
};

const CirrusDac2chDescriptor kDescCS4399 = {
    /* compatible         */ "cirrus,cs4399",
    /* chipName           */ "CS4399",
    /* chipId             */ CS4399_CHIP_ID,
    /* chipIdMask         */ CS4399_CHIP_ID_MASK,
    /* capabilities       */ (uint16_t)(HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS),
    /* sampleRateMask     */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K,
    /* supportedRates     */ kCirrusRates5,
    /* supportedRateCount */ 5,
    /* i2cAddr            */ CS4399_I2C_ADDR,
    /* maxBitDepth        */ 32,
    /* regType            */ REG_16BIT_PAGED,
    /* regChipId          */ CS4399_REG_DEVID_REVID,
    /* regPowerCtl        */ CS4399_REG_POWER_CTL,
    /* regIfaceCtl        */ CS4399_REG_IFACE_CTL,
    /* regPcmPath         */ CS4399_REG_PCM_PATH,
    /* regVolA            */ CS4399_REG_VOL_A,
    /* regVolB            */ CS4399_REG_VOL_B,
    /* regClockCtl        */ CS4399_REG_CLOCK_CTL,
    /* regModeCtl         */ REG_NO_FEATURE_16,
    /* regMuteCtl         */ REG_NO_FEATURE_16,
    /* ifaceDefault       */ CS4399_IFACE_DEFAULT,
    /* wl16Bit            */ CS4399_WL_16BIT,
    /* wl24Bit            */ CS4399_WL_24BIT,
    /* wl32Bit            */ CS4399_WL_32BIT,
    /* muteType           */ MUTE_PCM_PATH_RMW,
    /* muteABit           */ CS4399_MUTE_A_BIT,
    /* muteBBit           */ CS4399_MUTE_B_BIT,
    /* muteBoth           */ CS4399_MUTE_BOTH,
    /* filterCount        */ CS4399_FILTER_COUNT,
    /* filterMask         */ CS4399_FILTER_MASK,
    /* filterShift        */ 0,
    /* regFilter          */ CS4399_REG_PCM_PATH,
    /* speedType          */ SPEED_CLOCK_CTL,
    /* speedNormal        */ CS4399_SPEED_NORMAL,
    /* speedDouble        */ CS4399_SPEED_DOUBLE,
    /* speedQuad          */ CS4399_SPEED_QUAD,
    /* speedOctal         */ CS4399_SPEED_OCTAL,
    /* speedFmMask        */ 0x00,
    /* initSeq            */ kInitSeqCS4399,
    /* initSeqLen         */ (uint8_t)(sizeof(kInitSeqCS4399) / sizeof(kInitSeqCS4399[0])),
    /* deinitSeq          */ kDeinitSeqCS4399,
    /* deinitSeqLen       */ (uint8_t)(sizeof(kDeinitSeqCS4399) / sizeof(kDeinitSeqCS4399[0])),
    /* regHpAmp           */ REG_NO_FEATURE_16,
    /* hpAmpEnableVal     */ 0x00,
    /* hpAmpDisableVal    */ 0x00,
    /* regNos             */ CS4399_REG_NOS_CTL,
    /* nosEnableVal       */ CS4399_NOS_ENABLE,
    /* nosDisableVal      */ CS4399_NOS_DISABLE,
    /* regDsdPath         */ REG_NO_FEATURE_16,     // CS4399 has no DSD support
    /* dsdPathEnable      */ 0x00,
    /* dsdPathDisable     */ 0x00,
    /* dsdPathMask        */ 0xFF,
    /* regDsdInt          */ REG_NO_FEATURE_16,
    /* dsdIntDefault      */ 0x00,
    /* dsdFuncMode        */ 0xFF,
    /* logPrefix          */ "[HAL:CS4399]",
};

const CirrusDac2chDescriptor kDescCS43130 = {
    /* compatible         */ "cirrus,cs43130",
    /* chipName           */ "CS43130",
    /* chipId             */ CS43130_CHIP_ID,
    /* chipIdMask         */ CS43130_CHIP_ID_MASK,
    /* capabilities       */ (uint16_t)(HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS | HAL_CAP_DSD | HAL_CAP_HP_AMP),
    /* sampleRateMask     */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K,
    /* supportedRates     */ kCirrusRates5,
    /* supportedRateCount */ 5,
    /* i2cAddr            */ CS43130_I2C_ADDR,
    /* maxBitDepth        */ 32,
    /* regType            */ REG_16BIT_PAGED,
    /* regChipId          */ CS43130_REG_DEVID_REVID,
    /* regPowerCtl        */ CS43130_REG_POWER_CTL,
    /* regIfaceCtl        */ CS43130_REG_IFACE_CTL,
    /* regPcmPath         */ CS43130_REG_PCM_PATH,
    /* regVolA            */ CS43130_REG_VOL_A,
    /* regVolB            */ CS43130_REG_VOL_B,
    /* regClockCtl        */ CS43130_REG_CLOCK_CTL,
    /* regModeCtl         */ REG_NO_FEATURE_16,
    /* regMuteCtl         */ REG_NO_FEATURE_16,
    /* ifaceDefault       */ CS43130_IFACE_DEFAULT,
    /* wl16Bit            */ CS43130_WL_16BIT,
    /* wl24Bit            */ CS43130_WL_24BIT,
    /* wl32Bit            */ CS43130_WL_32BIT,
    /* muteType           */ MUTE_PCM_PATH_RMW,
    /* muteABit           */ CS43130_MUTE_A_BIT,
    /* muteBBit           */ CS43130_MUTE_B_BIT,
    /* muteBoth           */ CS43130_MUTE_BOTH,
    /* filterCount        */ CS43130_FILTER_COUNT,
    /* filterMask         */ CS43130_FILTER_MASK,
    /* filterShift        */ 0,
    /* regFilter          */ CS43130_REG_PCM_PATH,
    /* speedType          */ SPEED_CLOCK_CTL,
    /* speedNormal        */ CS43130_SPEED_NORMAL,
    /* speedDouble        */ CS43130_SPEED_DOUBLE,
    /* speedQuad          */ CS43130_SPEED_QUAD,
    /* speedOctal         */ CS43130_SPEED_OCTAL,
    /* speedFmMask        */ 0x00,
    /* initSeq            */ kInitSeqCS43130,
    /* initSeqLen         */ (uint8_t)(sizeof(kInitSeqCS43130) / sizeof(kInitSeqCS43130[0])),
    /* deinitSeq          */ kDeinitSeqCS43130,
    /* deinitSeqLen       */ (uint8_t)(sizeof(kDeinitSeqCS43130) / sizeof(kDeinitSeqCS43130[0])),
    /* regHpAmp           */ CS43130_REG_HP_CTL,
    /* hpAmpEnableVal     */ CS43130_HP_AMP_ENABLE,
    /* hpAmpDisableVal    */ CS43130_HP_AMP_DISABLE,
    /* regNos             */ CS43130_REG_NOS_CTL,
    /* nosEnableVal       */ CS43130_NOS_ENABLE,
    /* nosDisableVal      */ CS43130_NOS_DISABLE,
    /* regDsdPath         */ CS43130_REG_DSD_PATH,  // CS43130: DSD64 only
    /* dsdPathEnable      */ CS43130_DSD_PATH_ENABLE,
    /* dsdPathDisable     */ CS43130_DSD_PATH_DISABLE,
    /* dsdPathMask        */ 0x01,
    /* regDsdInt          */ CS43130_REG_DSD_INT,
    /* dsdIntDefault      */ CS43130_DSD_INT_DOP,
    /* dsdFuncMode        */ 0xFF,
    /* logPrefix          */ "[HAL:CS43130]",
};

// ===========================================================================
// Constructor
// ===========================================================================

HalCirrusDac2ch::HalCirrusDac2ch(const CirrusDac2chDescriptor& desc)
    : HalCirrusDacBase(), _desc(desc)
{
    hal_init_descriptor(_descriptor, desc.compatible, desc.chipName, "Cirrus Logic",
        HAL_DEV_DAC, 2, desc.i2cAddr, HAL_BUS_I2C, HAL_I2C_BUS_EXP,
        desc.sampleRateMask, desc.capabilities);
    _initPriority = HAL_PRIORITY_HARDWARE;
    _i2cAddr      = desc.i2cAddr;
    _sdaPin       = CIRRUS_DAC_I2C_BUS2_SDA;
    _sclPin       = CIRRUS_DAC_I2C_BUS2_SCL;
    _i2cBusIndex  = HAL_I2C_BUS_EXP;
    if (desc.maxBitDepth == 24) _bitDepth = 24;
}

// ===========================================================================
// Private helpers
// ===========================================================================

bool HalCirrusDac2ch::_writeReg(uint16_t reg, uint8_t val) {
    if (_desc.regType == REG_8BIT) {
        return _writeReg8((uint8_t)reg, val);
    }
    return _writeRegPaged(reg, val);
}

uint8_t HalCirrusDac2ch::_readReg(uint16_t reg) {
    if (_desc.regType == REG_8BIT) {
        return _readReg8((uint8_t)reg);
    }
    return _readRegPaged(reg);
}

void HalCirrusDac2ch::_execSequence(const CirrusDac2chRegWrite* seq, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        if (seq[i].reg == DELAY_SENTINEL_16) {
#ifndef NATIVE_TEST
            delay((uint32_t)seq[i].val * 5u);
#endif
        } else {
            _writeReg(seq[i].reg, seq[i].val);
        }
    }
}

uint8_t HalCirrusDac2ch::_computeSpeedMode(uint32_t sampleRate) const {
    if (sampleRate <= 48000)  return _desc.speedNormal;
    if (sampleRate <= 96000)  return _desc.speedDouble;
    if (sampleRate <= 192000) return _desc.speedQuad;
    return _desc.speedOctal;
}

uint8_t HalCirrusDac2ch::_computeWordLengthBits(uint8_t bitDepth) const {
    if (bitDepth == 16) return _desc.wl16Bit;
    if (bitDepth == 24) return _desc.wl24Bit;
    return _desc.wl32Bit;
}

// ===========================================================================
// HalDevice lifecycle
// ===========================================================================

bool HalCirrusDac2ch::probe() {
#ifndef NATIVE_TEST
    if (!_bus().probe(_i2cAddr)) return false;
    uint8_t chipId = _readReg(_desc.regChipId);
    return ((chipId & _desc.chipIdMask) == (_desc.chipId & _desc.chipIdMask));
#else
    return true;
#endif
}

HalInitResult HalCirrusDac2ch::init() {
    // 1. Apply per-device config overrides from HAL Device Manager
    _applyConfigOverrides();

    // Clamp bit depth to device maximum (CS4398: 24-bit max)
    if (_bitDepth > _desc.maxBitDepth) {
        LOG_W("%s Bit depth %u exceeds maximum %u — clamping",
              _desc.logPrefix, _bitDepth, _desc.maxBitDepth);
        _bitDepth = _desc.maxBitDepth;
    }

    LOG_I("%s Initializing (I2C addr=0x%02X bus=%u SDA=%d SCL=%d sr=%luHz bits=%u)",
          _desc.logPrefix, _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth);

#ifndef NATIVE_TEST
    // 2. Select TwoWire instance and initialize I2C bus at 400 kHz
    _selectWire();
    LOG_I("%s I2C initialized (bus %u SDA=%d SCL=%d 400kHz)",
          _desc.logPrefix, _i2cBusIndex, _sdaPin, _sclPin);
#endif

    // 3. Verify chip ID (log warning on mismatch — continue)
    uint8_t chipId = _readReg(_desc.regChipId);
    if ((chipId & _desc.chipIdMask) != (_desc.chipId & _desc.chipIdMask)) {
        LOG_W("%s Unexpected chip ID: 0x%02X (expected 0x%02X) — continuing",
              _desc.logPrefix, chipId, _desc.chipId);
    } else {
        LOG_I("%s Chip ID OK (0x%02X)", _desc.logPrefix, chipId);
    }

    // 4. Run chip-specific init sequence (power up, I2S format, etc.)
    _execSequence(_desc.initSeq, _desc.initSeqLen);

    // 5. Set speed mode
    if (_desc.speedType == SPEED_CLOCK_CTL && _desc.regClockCtl != REG_NO_FEATURE_16) {
        _writeReg(_desc.regClockCtl, _computeSpeedMode(_sampleRate));
    } else if (_desc.speedType == SPEED_FM_BITS && _desc.regModeCtl != REG_NO_FEATURE_16) {
        // CS4398: read-modify-write FM bits in mode control register
        uint8_t modeCtl = _readReg(_desc.regModeCtl);
        modeCtl = (uint8_t)((modeCtl & (uint8_t)~_desc.speedFmMask) | _computeSpeedMode(_sampleRate));
        _writeReg(_desc.regModeCtl, modeCtl);
    }

    // 6. Configure interface word length (paged chips only; CS4398 handles via vol_mix + mode_ctl)
    if (_desc.regType == REG_16BIT_PAGED && _desc.regIfaceCtl != REG_NO_FEATURE_16) {
        uint8_t wlBits = _computeWordLengthBits(_bitDepth);
        _writeReg(_desc.regIfaceCtl, (uint8_t)(_desc.ifaceDefault | wlBits));
    }

    // 7. Set filter preset
    {
        uint8_t preset = (_filterPreset >= _desc.filterCount) ? 0 : _filterPreset;
        if (_desc.regType == REG_16BIT_PAGED && _desc.regFilter == _desc.regPcmPath) {
            // Paged chips: filter + mute bits share the PCM path register
            uint8_t pcmPath = (uint8_t)((preset & _desc.filterMask) << _desc.filterShift);
            if (_muted) pcmPath = (uint8_t)(pcmPath | _desc.muteBoth);
            _writeReg(_desc.regFilter, pcmPath);
        } else {
            // CS4398: filter in separate REG_RAMP_FILT via read-modify-write
            uint8_t rampFilt = _readReg(_desc.regFilter);
            rampFilt = (uint8_t)((rampFilt & (uint8_t)~_desc.filterMask) |
                                 ((uint8_t)(preset & 0x03u) << _desc.filterShift));
            _writeReg(_desc.regFilter, rampFilt);
        }
    }

    // 8. Set initial volume (0x00=0dB, 0xFF=full attenuation)
    {
        uint8_t volReg = _muted ? 0xFF :
                        (uint8_t)(((uint32_t)(100u - _volume) * 0xFF) / 100u);
        _writeReg(_desc.regVolA, volReg);
        _writeReg(_desc.regVolB, volReg);
    }

    // 9. Apply initial mute via dedicated register if available (CS4398)
    if (_desc.muteType == MUTE_DEDICATED_REG && _desc.regMuteCtl != REG_NO_FEATURE_16) {
        _writeReg(_desc.regMuteCtl, _muted ? _desc.muteBoth : 0x00);
    }

    // 10. Disable optional features at init (HP amp, NOS mode)
    if (_desc.regHpAmp != REG_NO_FEATURE_16) {
        _hpAmpEnabled = false;
        _writeReg(_desc.regHpAmp, _desc.hpAmpDisableVal);
    }
    if (_desc.regNos != REG_NO_FEATURE_16) {
        _nosEnabled = false;
        _writeReg(_desc.regNos, _desc.nosDisableVal);
    }

    // 11. Enable expansion I2S TX output
    if (!_enableI2sTx()) {
        LOG_E("%s Failed to enable expansion I2S TX", _desc.logPrefix);
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "I2S TX enable failed");
    }

    // 12. Mark device ready
    _initialized = true;
    _state = HAL_STATE_AVAILABLE;
    setReady(true);

    LOG_I("%s Ready (vol=%u%% muted=%d filter=%u)", _desc.logPrefix, _volume, (int)_muted, _filterPreset);
    return hal_init_ok();
}

void HalCirrusDac2ch::deinit() {
    if (!_initialized) return;

    setReady(false);

    // Run chip-specific deinit sequence (mute, disable features, power down)
    _execSequence(_desc.deinitSeq, _desc.deinitSeqLen);

    _disableI2sTx();

    _hpAmpEnabled = false;
    _nosEnabled   = false;
    _initialized  = false;
    _i2sTxEnabled = false;
    _state = HAL_STATE_REMOVED;

    LOG_I("%s Deinitialized", _desc.logPrefix);
}

void HalCirrusDac2ch::dumpConfig() {
    LOG_I("%s %s by Cirrus Logic (compat=%s) i2c=0x%02X bus=%u sda=%d scl=%d "
          "sr=%luHz bits=%u vol=%u%% muted=%d filter=%u",
          _desc.logPrefix, _descriptor.name, _descriptor.compatible,
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth,
          _volume, (int)_muted, _filterPreset);
}

bool HalCirrusDac2ch::healthCheck() {
#ifndef NATIVE_TEST
    if (!_initialized) return false;
    uint8_t id = _readReg(_desc.regChipId);
    return ((id & _desc.chipIdMask) == (_desc.chipId & _desc.chipIdMask));
#else
    return _initialized;
#endif
}

// ===========================================================================
// HalAudioDevice
// ===========================================================================

bool HalCirrusDac2ch::configure(uint32_t sampleRate, uint8_t bitDepth) {
    if (!_validateSampleRate(sampleRate, _desc.supportedRates, _desc.supportedRateCount)) {
        LOG_W("%s Unsupported sample rate: %luHz", _desc.logPrefix, (unsigned long)sampleRate);
        return false;
    }
    // Clamp bit depth to device maximum
    if (bitDepth > _desc.maxBitDepth) bitDepth = _desc.maxBitDepth;
    if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32) {
        LOG_W("%s Unsupported bit depth: %u", _desc.logPrefix, bitDepth);
        return false;
    }

    _sampleRate = sampleRate;
    _bitDepth   = bitDepth;

    if (_initialized) {
        // Update speed mode
        if (_desc.speedType == SPEED_CLOCK_CTL && _desc.regClockCtl != REG_NO_FEATURE_16) {
            _writeReg(_desc.regClockCtl, _computeSpeedMode(sampleRate));
        } else if (_desc.speedType == SPEED_FM_BITS && _desc.regModeCtl != REG_NO_FEATURE_16) {
            uint8_t modeCtl = _readReg(_desc.regModeCtl);
            modeCtl = (uint8_t)((modeCtl & (uint8_t)~_desc.speedFmMask) | _computeSpeedMode(sampleRate));
            _writeReg(_desc.regModeCtl, modeCtl);
        }

        // Update interface word length (paged chips only)
        if (_desc.regType == REG_16BIT_PAGED && _desc.regIfaceCtl != REG_NO_FEATURE_16) {
            uint8_t wlBits = _computeWordLengthBits(bitDepth);
            _writeReg(_desc.regIfaceCtl, (uint8_t)(_desc.ifaceDefault | wlBits));
        }
    }

    LOG_I("%s Configured: %luHz %ubit", _desc.logPrefix, (unsigned long)sampleRate, bitDepth);
    return true;
}

bool HalCirrusDac2ch::setVolume(uint8_t percent) {
    if (!_initialized) return false;
    if (percent > 100) percent = 100;
    _volume = percent;

    // All Cirrus DACs: 0x00=0dB, 0xFF=full attenuation, 0.5dB/step.
    // Map 100% → 0x00 (no attenuation), 0% → 0xFF (full attenuation).
    uint8_t atten = (uint8_t)(((uint32_t)(100u - percent) * 0xFF) / 100u);
    bool ok  = _writeReg(_desc.regVolA, atten);
    ok       = ok && _writeReg(_desc.regVolB, atten);
    LOG_D("%s Volume: %d%% -> atten=0x%02X", _desc.logPrefix, percent, atten);
    return ok;
}

bool HalCirrusDac2ch::setMute(bool mute) {
    if (!_initialized) return false;
    _muted = mute;

    bool ok;
    if (_desc.muteType == MUTE_DEDICATED_REG && _desc.regMuteCtl != REG_NO_FEATURE_16) {
        // CS4398: write to dedicated mute register
        ok = _writeReg(_desc.regMuteCtl, mute ? _desc.muteBoth : 0x00);
    } else {
        // Paged chips: read-modify-write mute bits in PCM path register
        uint8_t pcmPath = _readReg(_desc.regPcmPath);
        if (mute) {
            pcmPath = (uint8_t)(pcmPath | _desc.muteBoth);
        } else {
            pcmPath = (uint8_t)(pcmPath & (uint8_t)~_desc.muteBoth);
        }
        ok = _writeReg(_desc.regPcmPath, pcmPath);
    }

    LOG_I("%s %s", _desc.logPrefix, mute ? "Muted" : "Unmuted");
    return ok;
}

// ===========================================================================
// Filter preset
// ===========================================================================

bool HalCirrusDac2ch::setFilterPreset(uint8_t preset) {
    if (preset >= _desc.filterCount) {
        LOG_W("%s Invalid filter preset %u (max %u)", _desc.logPrefix, preset, _desc.filterCount - 1);
        return false;
    }
    _filterPreset = preset;

    if (_initialized) {
        if (_desc.regType == REG_16BIT_PAGED && _desc.regFilter == _desc.regPcmPath) {
            // Paged chips: preserve mute bits while updating filter field
            uint8_t pcmPath = _readReg(_desc.regPcmPath);
            pcmPath = (uint8_t)((pcmPath & (uint8_t)~_desc.filterMask) |
                                (uint8_t)((preset & _desc.filterMask) << _desc.filterShift));
            _writeReg(_desc.regFilter, pcmPath);
        } else {
            // CS4398: read-modify-write filter bits in REG_RAMP_FILT
            uint8_t rampFilt = _readReg(_desc.regFilter);
            rampFilt = (uint8_t)((rampFilt & (uint8_t)~_desc.filterMask) |
                                 ((uint8_t)(preset & 0x03u) << _desc.filterShift));
            _writeReg(_desc.regFilter, rampFilt);
        }
    }

    LOG_I("%s Filter preset: %u", _desc.logPrefix, preset);
    return true;
}

// ===========================================================================
// Optional feature APIs (HP amp / NOS mode)
// ===========================================================================

bool HalCirrusDac2ch::setHeadphoneAmpEnabled(bool enable) {
    if (_desc.regHpAmp == REG_NO_FEATURE_16) return false;

    _hpAmpEnabled = enable;

    if (!_initialized) return true;  // Deferred — applied at next init

    bool ok = _writeReg(_desc.regHpAmp, enable ? _desc.hpAmpEnableVal : _desc.hpAmpDisableVal);
    LOG_I("%s Headphone amp: %s", _desc.logPrefix, enable ? "enabled" : "disabled");
    return ok;
}

bool HalCirrusDac2ch::isHeadphoneAmpEnabled() const {
    return _hpAmpEnabled;
}

bool HalCirrusDac2ch::setNosMode(bool enable) {
    if (_desc.regNos == REG_NO_FEATURE_16) return false;

    _nosEnabled = enable;

    if (!_initialized) return true;  // Deferred — applied at next init

    bool ok = _writeReg(_desc.regNos, enable ? _desc.nosEnableVal : _desc.nosDisableVal);
    LOG_I("%s NOS filter: %s", _desc.logPrefix, enable ? "enabled" : "disabled");
    return ok;
}

bool HalCirrusDac2ch::isNosMode() const {
    return _nosEnabled;
}

// ===========================================================================
// DSD mode switching
// ===========================================================================
// Sequence: mute → 5ms settle → write DSD registers → 10ms settle → unmute.
// For CS4398: DSD is a single bit in MODE_CTL (read-modify-write).
// For paged chips (CS43198, CS43131, CS43130): write DSD_PATH and DSD_INT.
// regDsdPath == 0xFFFF means no DSD support (CS4399) — returns false.

bool HalCirrusDac2ch::setDsdMode(bool enable) {
    if (_desc.regDsdPath == REG_NO_FEATURE_16) return false;
    if (!_initialized) return false;
    if (_dsdEnabled == enable) return true;  // No change

    // Step 1: Mute to avoid click during mode switch
    setMute(true);

#ifndef NATIVE_TEST
    delay(5);
#endif

    bool ok;
    if (_desc.regType == REG_8BIT) {
        // CS4398: single CHSL bit in MODE_CTL — read-modify-write
        uint8_t modeCtl = _readReg(_desc.regDsdPath);
        if (enable) {
            modeCtl = (uint8_t)((modeCtl & (uint8_t)~_desc.dsdPathMask) | _desc.dsdPathEnable);
        } else {
            modeCtl = (uint8_t)(modeCtl & (uint8_t)~_desc.dsdPathMask);
        }
        ok = _writeReg(_desc.regDsdPath, modeCtl);
    } else {
        // Paged chips (CS43198, CS43131, CS43130): dedicated DSD_PATH and DSD_INT registers
        if (enable) {
            // Write DSD interface config before enabling DSD path
            if (_desc.regDsdInt != REG_NO_FEATURE_16) {
                _writeReg(_desc.regDsdInt, _desc.dsdIntDefault);
            }
            ok = _writeReg(_desc.regDsdPath, _desc.dsdPathEnable);
        } else {
            ok = _writeReg(_desc.regDsdPath, _desc.dsdPathDisable);
            if (_desc.regDsdInt != REG_NO_FEATURE_16) {
                _writeReg(_desc.regDsdInt, 0x00);
            }
        }
    }

    if (!ok) {
        setMute(false);
        return false;
    }

    _dsdEnabled = enable;

#ifndef NATIVE_TEST
    delay(10);
#endif

    // Step 3: Unmute (restore previous mute state only if we weren't already muted)
    if (!_muted) {
        setMute(false);
    }

    LOG_I("%s DSD mode: %s", _desc.logPrefix, enable ? "enabled (DoP)" : "disabled (PCM)");
    return true;
}

bool HalCirrusDac2ch::isDsdMode() const {
    return _dsdEnabled;
}

#endif // DAC_ENABLED
