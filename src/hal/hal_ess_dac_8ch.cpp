#ifdef DAC_ENABLED
// HalEssDac8ch — Generic ESS SABRE 8-channel DAC driver implementation.
// Descriptor tables for ES9038PRO, ES9028PRO, ES9039PRO/MPRO, ES9027PRO,
// ES9081, ES9082, ES9017.
// All 7 chips share the same init/configure/volume/mute/filter logic driven
// by per-chip EssDac8chDescriptor structs.

#include "hal_ess_dac_8ch.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include <Arduino.h>
#include "../debug_serial.h"
#include "../i2s_audio.h"
#include "../config.h"
#else
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)
#endif

#include <string.h>
#include <stdio.h>

// ===========================================================================
// Register constants replicated for native compilation
// All values verified against individual driver NATIVE_TEST blocks.
// ===========================================================================

// ----- Shared / sentinel -----
#define DELAY_SENTINEL_8CH   0xFF   // reg==0xFF in init sequence means delay(val*5ms)

// ----- Common to all 7 chips -----
#define ESS8CH_I2C_ADDR          0x48
#define ESS8CH_REG_SYS_CONFIG    0x00
#define ESS8CH_REG_INPUT_CFG     0x01
#define ESS8CH_REG_FILTER_MUTE   0x07
#define ESS8CH_REG_MASTER_MODE   0x0A
#define ESS8CH_REG_DPLL_CFG      0x0C
#define ESS8CH_REG_SOFT_START    0x0E
#define ESS8CH_REG_VOL_CH1       0x0F
#define ESS8CH_REG_CHIP_ID       0xE1
#define ESS8CH_SOFT_RESET_BIT    0x01
#define ESS8CH_CHANNEL_MODE_8CH  0x04
#define ESS8CH_INPUT_TDM         0x0C
#define ESS8CH_INPUT_32BIT       0x20
#define ESS8CH_SLAVE_MODE        0x00
#define ESS8CH_FILTER_MASK       0x07
#define ESS8CH_MUTE_BIT          0x20
#define ESS8CH_VOL_0DB           0x00
#define ESS8CH_VOL_MUTE          0xFF

// Chip IDs
#define ES9038PRO_CHIP_ID        0x03
#define ES9028PRO_CHIP_ID        0x02
#define ES9039PRO_CHIP_ID        0x39
#define ES9039MPRO_CHIP_ID       0x3A
#define ES9027PRO_CHIP_ID        0x27
#define ES9081_CHIP_ID           0x81
#define ES9082_CHIP_ID           0x82
#define ES9017_CHIP_ID           0x17

// DPLL bandwidths (per-chip differences)
#define ES9038PRO_DPLL_BW        0x05  // HyperStream II
#define ES9028PRO_DPLL_BW        0x05  // HyperStream II
#define ES9039PRO_DPLL_BW        0x04  // HyperStream IV (tighter)
#define ES9027PRO_DPLL_BW        0x04  // HyperStream IV (tighter)
#define ES9081_DPLL_BW           0x05  // HyperStream IV cost-tier
#define ES9082_DPLL_BW           0x05  // HyperStream IV cost-tier
#define ES9017_DPLL_BW           0x04  // HyperStream IV (tighter, per ES9027PRO)

// ===========================================================================
// Init/deinit sequences
// All chips share the same sequence structure; only the DPLL value differs.
// The sequences are parameterized by chip-specific values embedded in the
// descriptor, so a single _execSequence() call handles all chips.
// ===========================================================================

// ----- ES9038PRO -----
static const EssDac8chRegWrite kInitSeqES9038PRO[] = {
    { ESS8CH_REG_SYS_CONFIG,   ESS8CH_SOFT_RESET_BIT                              },  // soft reset
    { DELAY_SENTINEL_8CH, 1                                                        },  // delay 5ms
    { ESS8CH_REG_SYS_CONFIG,   ESS8CH_CHANNEL_MODE_8CH                            },  // 8-channel mode
    { ESS8CH_REG_INPUT_CFG,    (uint8_t)(ESS8CH_INPUT_TDM | ESS8CH_INPUT_32BIT)   },  // TDM 32-bit
    { ESS8CH_REG_MASTER_MODE,  ESS8CH_SLAVE_MODE                                  },  // I2S slave
    { ESS8CH_REG_DPLL_CFG,     ES9038PRO_DPLL_BW                                  },  // DPLL BW
    { ESS8CH_REG_SOFT_START,   0x00                                                },  // soft start default
};
static const EssDac8chRegWrite kDeinitSeqES9038PRO[] = {
    { ESS8CH_REG_FILTER_MUTE,  ESS8CH_MUTE_BIT },  // mute all channels
};

// ----- ES9028PRO -----
static const EssDac8chRegWrite kInitSeqES9028PRO[] = {
    { ESS8CH_REG_SYS_CONFIG,   ESS8CH_SOFT_RESET_BIT                              },
    { DELAY_SENTINEL_8CH, 1                                                        },
    { ESS8CH_REG_SYS_CONFIG,   ESS8CH_CHANNEL_MODE_8CH                            },
    { ESS8CH_REG_INPUT_CFG,    (uint8_t)(ESS8CH_INPUT_TDM | ESS8CH_INPUT_32BIT)   },
    { ESS8CH_REG_MASTER_MODE,  ESS8CH_SLAVE_MODE                                  },
    { ESS8CH_REG_DPLL_CFG,     ES9028PRO_DPLL_BW                                  },
    { ESS8CH_REG_SOFT_START,   0x00                                                },
};
static const EssDac8chRegWrite kDeinitSeqES9028PRO[] = {
    { ESS8CH_REG_FILTER_MUTE,  ESS8CH_MUTE_BIT },
};

// ----- ES9039PRO / ES9039MPRO -----
static const EssDac8chRegWrite kInitSeqES9039PRO[] = {
    { ESS8CH_REG_SYS_CONFIG,   ESS8CH_SOFT_RESET_BIT                              },
    { DELAY_SENTINEL_8CH, 1                                                        },
    { ESS8CH_REG_SYS_CONFIG,   ESS8CH_CHANNEL_MODE_8CH                            },
    { ESS8CH_REG_INPUT_CFG,    (uint8_t)(ESS8CH_INPUT_TDM | ESS8CH_INPUT_32BIT)   },
    { ESS8CH_REG_MASTER_MODE,  ESS8CH_SLAVE_MODE                                  },
    { ESS8CH_REG_DPLL_CFG,     ES9039PRO_DPLL_BW                                  },
    { ESS8CH_REG_SOFT_START,   0x00                                                },
};
static const EssDac8chRegWrite kDeinitSeqES9039PRO[] = {
    { ESS8CH_REG_FILTER_MUTE,  ESS8CH_MUTE_BIT },
};

// ----- ES9027PRO -----
static const EssDac8chRegWrite kInitSeqES9027PRO[] = {
    { ESS8CH_REG_SYS_CONFIG,   ESS8CH_SOFT_RESET_BIT                              },
    { DELAY_SENTINEL_8CH, 1                                                        },
    { ESS8CH_REG_SYS_CONFIG,   ESS8CH_CHANNEL_MODE_8CH                            },
    { ESS8CH_REG_INPUT_CFG,    (uint8_t)(ESS8CH_INPUT_TDM | ESS8CH_INPUT_32BIT)   },
    { ESS8CH_REG_MASTER_MODE,  ESS8CH_SLAVE_MODE                                  },
    { ESS8CH_REG_DPLL_CFG,     ES9027PRO_DPLL_BW                                  },
    { ESS8CH_REG_SOFT_START,   0x00                                                },
};
static const EssDac8chRegWrite kDeinitSeqES9027PRO[] = {
    { ESS8CH_REG_FILTER_MUTE,  ESS8CH_MUTE_BIT },
};

// ----- ES9081 -----
static const EssDac8chRegWrite kInitSeqES9081[] = {
    { ESS8CH_REG_SYS_CONFIG,   ESS8CH_SOFT_RESET_BIT                              },
    { DELAY_SENTINEL_8CH, 1                                                        },
    { ESS8CH_REG_SYS_CONFIG,   ESS8CH_CHANNEL_MODE_8CH                            },
    { ESS8CH_REG_INPUT_CFG,    (uint8_t)(ESS8CH_INPUT_TDM | ESS8CH_INPUT_32BIT)   },
    { ESS8CH_REG_MASTER_MODE,  ESS8CH_SLAVE_MODE                                  },
    { ESS8CH_REG_DPLL_CFG,     ES9081_DPLL_BW                                     },
    { ESS8CH_REG_SOFT_START,   0x00                                                },
};
static const EssDac8chRegWrite kDeinitSeqES9081[] = {
    { ESS8CH_REG_FILTER_MUTE,  ESS8CH_MUTE_BIT },
};

// ----- ES9082 -----
static const EssDac8chRegWrite kInitSeqES9082[] = {
    { ESS8CH_REG_SYS_CONFIG,   ESS8CH_SOFT_RESET_BIT                              },
    { DELAY_SENTINEL_8CH, 1                                                        },
    { ESS8CH_REG_SYS_CONFIG,   ESS8CH_CHANNEL_MODE_8CH                            },
    { ESS8CH_REG_INPUT_CFG,    (uint8_t)(ESS8CH_INPUT_TDM | ESS8CH_INPUT_32BIT)   },
    { ESS8CH_REG_MASTER_MODE,  ESS8CH_SLAVE_MODE                                  },
    { ESS8CH_REG_DPLL_CFG,     ES9082_DPLL_BW                                     },
    { ESS8CH_REG_SOFT_START,   0x00                                                },
};
static const EssDac8chRegWrite kDeinitSeqES9082[] = {
    { ESS8CH_REG_FILTER_MUTE,  ESS8CH_MUTE_BIT },
};

// ----- ES9017 -----
static const EssDac8chRegWrite kInitSeqES9017[] = {
    { ESS8CH_REG_SYS_CONFIG,   ESS8CH_SOFT_RESET_BIT                              },
    { DELAY_SENTINEL_8CH, 1                                                        },
    { ESS8CH_REG_SYS_CONFIG,   ESS8CH_CHANNEL_MODE_8CH                            },
    { ESS8CH_REG_INPUT_CFG,    (uint8_t)(ESS8CH_INPUT_TDM | ESS8CH_INPUT_32BIT)   },
    { ESS8CH_REG_MASTER_MODE,  ESS8CH_SLAVE_MODE                                  },
    { ESS8CH_REG_DPLL_CFG,     ES9017_DPLL_BW                                     },
    { ESS8CH_REG_SOFT_START,   0x00                                                },
};
static const EssDac8chRegWrite kDeinitSeqES9017[] = {
    { ESS8CH_REG_FILTER_MUTE,  ESS8CH_MUTE_BIT },
};

// ===========================================================================
// Supported sample rate tables
// ===========================================================================

static const uint32_t kRates8ch[] = { 44100, 48000, 96000, 192000, 384000, 768000 };

// ===========================================================================
// Descriptor tables
// ===========================================================================

const EssDac8chDescriptor kDescES9038PRO = {
    /* compatible        */ "ess,es9038pro",
    /* chipName          */ "ES9038PRO",
    /* chipId            */ ES9038PRO_CHIP_ID,
    /* altChipId         */ 0xFF,
    /* altChipName       */ nullptr,
    /* altCompatible     */ nullptr,
    /* capabilities      */ (uint32_t)(HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS | HAL_CAP_DPLL),
    /* sampleRateMask    */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K,
    /* supportedRates    */ kRates8ch,
    /* supportedRateCount*/ 6,
    /* regSysConfig      */ ESS8CH_REG_SYS_CONFIG,
    /* regInputCfg       */ ESS8CH_REG_INPUT_CFG,
    /* regFilterMute     */ ESS8CH_REG_FILTER_MUTE,
    /* regMasterMode     */ ESS8CH_REG_MASTER_MODE,
    /* regDpllCfg        */ ESS8CH_REG_DPLL_CFG,
    /* regSoftStart      */ ESS8CH_REG_SOFT_START,
    /* regVolCh1         */ ESS8CH_REG_VOL_CH1,
    /* regChipId         */ ESS8CH_REG_CHIP_ID,
    /* softResetBit      */ ESS8CH_SOFT_RESET_BIT,
    /* channelMode8ch    */ ESS8CH_CHANNEL_MODE_8CH,
    /* inputTdm32bit     */ (uint8_t)(ESS8CH_INPUT_TDM | ESS8CH_INPUT_32BIT),
    /* slaveMode         */ ESS8CH_SLAVE_MODE,
    /* dpllBandwidth     */ ES9038PRO_DPLL_BW,
    /* filterMask        */ ESS8CH_FILTER_MASK,
    /* muteBit           */ ESS8CH_MUTE_BIT,
    /* vol0dB            */ ESS8CH_VOL_0DB,
    /* volMute           */ ESS8CH_VOL_MUTE,
    /* initSeq           */ kInitSeqES9038PRO,
    /* initSeqLen        */ (uint8_t)(sizeof(kInitSeqES9038PRO) / sizeof(kInitSeqES9038PRO[0])),
    /* deinitSeq         */ kDeinitSeqES9038PRO,
    /* deinitSeqLen      */ (uint8_t)(sizeof(kDeinitSeqES9038PRO) / sizeof(kDeinitSeqES9038PRO[0])),
    /* sinkNamePrefix    */ "ES9038PRO",
    /* logPrefix         */ "[HAL:ES9038PRO]",
};

const EssDac8chDescriptor kDescES9028PRO = {
    /* compatible        */ "ess,es9028pro",
    /* chipName          */ "ES9028PRO",
    /* chipId            */ ES9028PRO_CHIP_ID,
    /* altChipId         */ 0xFF,
    /* altChipName       */ nullptr,
    /* altCompatible     */ nullptr,
    /* capabilities      */ (uint32_t)(HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS | HAL_CAP_DPLL),
    /* sampleRateMask    */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K,
    /* supportedRates    */ kRates8ch,
    /* supportedRateCount*/ 6,
    /* regSysConfig      */ ESS8CH_REG_SYS_CONFIG,
    /* regInputCfg       */ ESS8CH_REG_INPUT_CFG,
    /* regFilterMute     */ ESS8CH_REG_FILTER_MUTE,
    /* regMasterMode     */ ESS8CH_REG_MASTER_MODE,
    /* regDpllCfg        */ ESS8CH_REG_DPLL_CFG,
    /* regSoftStart      */ ESS8CH_REG_SOFT_START,
    /* regVolCh1         */ ESS8CH_REG_VOL_CH1,
    /* regChipId         */ ESS8CH_REG_CHIP_ID,
    /* softResetBit      */ ESS8CH_SOFT_RESET_BIT,
    /* channelMode8ch    */ ESS8CH_CHANNEL_MODE_8CH,
    /* inputTdm32bit     */ (uint8_t)(ESS8CH_INPUT_TDM | ESS8CH_INPUT_32BIT),
    /* slaveMode         */ ESS8CH_SLAVE_MODE,
    /* dpllBandwidth     */ ES9028PRO_DPLL_BW,
    /* filterMask        */ ESS8CH_FILTER_MASK,
    /* muteBit           */ ESS8CH_MUTE_BIT,
    /* vol0dB            */ ESS8CH_VOL_0DB,
    /* volMute           */ ESS8CH_VOL_MUTE,
    /* initSeq           */ kInitSeqES9028PRO,
    /* initSeqLen        */ (uint8_t)(sizeof(kInitSeqES9028PRO) / sizeof(kInitSeqES9028PRO[0])),
    /* deinitSeq         */ kDeinitSeqES9028PRO,
    /* deinitSeqLen      */ (uint8_t)(sizeof(kDeinitSeqES9028PRO) / sizeof(kDeinitSeqES9028PRO[0])),
    /* sinkNamePrefix    */ "ES9028PRO",
    /* logPrefix         */ "[HAL:ES9028PRO]",
};

const EssDac8chDescriptor kDescES9039PRO = {
    /* compatible        */ "ess,es9039pro",
    /* chipName          */ "ES9039PRO",
    /* chipId            */ ES9039PRO_CHIP_ID,
    /* altChipId         */ ES9039MPRO_CHIP_ID,
    /* altChipName       */ "ES9039MPRO",
    /* altCompatible     */ "ess,es9039mpro",
    /* capabilities      */ (uint32_t)(HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS | HAL_CAP_DPLL),
    /* sampleRateMask    */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K,
    /* supportedRates    */ kRates8ch,
    /* supportedRateCount*/ 6,
    /* regSysConfig      */ ESS8CH_REG_SYS_CONFIG,
    /* regInputCfg       */ ESS8CH_REG_INPUT_CFG,
    /* regFilterMute     */ ESS8CH_REG_FILTER_MUTE,
    /* regMasterMode     */ ESS8CH_REG_MASTER_MODE,
    /* regDpllCfg        */ ESS8CH_REG_DPLL_CFG,
    /* regSoftStart      */ ESS8CH_REG_SOFT_START,
    /* regVolCh1         */ ESS8CH_REG_VOL_CH1,
    /* regChipId         */ ESS8CH_REG_CHIP_ID,
    /* softResetBit      */ ESS8CH_SOFT_RESET_BIT,
    /* channelMode8ch    */ ESS8CH_CHANNEL_MODE_8CH,
    /* inputTdm32bit     */ (uint8_t)(ESS8CH_INPUT_TDM | ESS8CH_INPUT_32BIT),
    /* slaveMode         */ ESS8CH_SLAVE_MODE,
    /* dpllBandwidth     */ ES9039PRO_DPLL_BW,
    /* filterMask        */ ESS8CH_FILTER_MASK,
    /* muteBit           */ ESS8CH_MUTE_BIT,
    /* vol0dB            */ ESS8CH_VOL_0DB,
    /* volMute           */ ESS8CH_VOL_MUTE,
    /* initSeq           */ kInitSeqES9039PRO,
    /* initSeqLen        */ (uint8_t)(sizeof(kInitSeqES9039PRO) / sizeof(kInitSeqES9039PRO[0])),
    /* deinitSeq         */ kDeinitSeqES9039PRO,
    /* deinitSeqLen      */ (uint8_t)(sizeof(kDeinitSeqES9039PRO) / sizeof(kDeinitSeqES9039PRO[0])),
    /* sinkNamePrefix    */ "ES9039PRO",
    /* logPrefix         */ "[HAL:ES9039PRO]",
};

const EssDac8chDescriptor kDescES9027PRO = {
    /* compatible        */ "ess,es9027pro",
    /* chipName          */ "ES9027PRO",
    /* chipId            */ ES9027PRO_CHIP_ID,
    /* altChipId         */ 0xFF,
    /* altChipName       */ nullptr,
    /* altCompatible     */ nullptr,
    /* capabilities      */ (uint32_t)(HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS | HAL_CAP_DPLL),
    /* sampleRateMask    */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K,
    /* supportedRates    */ kRates8ch,
    /* supportedRateCount*/ 6,
    /* regSysConfig      */ ESS8CH_REG_SYS_CONFIG,
    /* regInputCfg       */ ESS8CH_REG_INPUT_CFG,
    /* regFilterMute     */ ESS8CH_REG_FILTER_MUTE,
    /* regMasterMode     */ ESS8CH_REG_MASTER_MODE,
    /* regDpllCfg        */ ESS8CH_REG_DPLL_CFG,
    /* regSoftStart      */ ESS8CH_REG_SOFT_START,
    /* regVolCh1         */ ESS8CH_REG_VOL_CH1,
    /* regChipId         */ ESS8CH_REG_CHIP_ID,
    /* softResetBit      */ ESS8CH_SOFT_RESET_BIT,
    /* channelMode8ch    */ ESS8CH_CHANNEL_MODE_8CH,
    /* inputTdm32bit     */ (uint8_t)(ESS8CH_INPUT_TDM | ESS8CH_INPUT_32BIT),
    /* slaveMode         */ ESS8CH_SLAVE_MODE,
    /* dpllBandwidth     */ ES9027PRO_DPLL_BW,
    /* filterMask        */ ESS8CH_FILTER_MASK,
    /* muteBit           */ ESS8CH_MUTE_BIT,
    /* vol0dB            */ ESS8CH_VOL_0DB,
    /* volMute           */ ESS8CH_VOL_MUTE,
    /* initSeq           */ kInitSeqES9027PRO,
    /* initSeqLen        */ (uint8_t)(sizeof(kInitSeqES9027PRO) / sizeof(kInitSeqES9027PRO[0])),
    /* deinitSeq         */ kDeinitSeqES9027PRO,
    /* deinitSeqLen      */ (uint8_t)(sizeof(kDeinitSeqES9027PRO) / sizeof(kDeinitSeqES9027PRO[0])),
    /* sinkNamePrefix    */ "ES9027PRO",
    /* logPrefix         */ "[HAL:ES9027PRO]",
};

const EssDac8chDescriptor kDescES9081 = {
    /* compatible        */ "ess,es9081",
    /* chipName          */ "ES9081",
    /* chipId            */ ES9081_CHIP_ID,
    /* altChipId         */ 0xFF,
    /* altChipName       */ nullptr,
    /* altCompatible     */ nullptr,
    /* capabilities      */ (uint32_t)(HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS | HAL_CAP_DPLL),
    /* sampleRateMask    */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K,
    /* supportedRates    */ kRates8ch,
    /* supportedRateCount*/ 6,
    /* regSysConfig      */ ESS8CH_REG_SYS_CONFIG,
    /* regInputCfg       */ ESS8CH_REG_INPUT_CFG,
    /* regFilterMute     */ ESS8CH_REG_FILTER_MUTE,
    /* regMasterMode     */ ESS8CH_REG_MASTER_MODE,
    /* regDpllCfg        */ ESS8CH_REG_DPLL_CFG,
    /* regSoftStart      */ ESS8CH_REG_SOFT_START,
    /* regVolCh1         */ ESS8CH_REG_VOL_CH1,
    /* regChipId         */ ESS8CH_REG_CHIP_ID,
    /* softResetBit      */ ESS8CH_SOFT_RESET_BIT,
    /* channelMode8ch    */ ESS8CH_CHANNEL_MODE_8CH,
    /* inputTdm32bit     */ (uint8_t)(ESS8CH_INPUT_TDM | ESS8CH_INPUT_32BIT),
    /* slaveMode         */ ESS8CH_SLAVE_MODE,
    /* dpllBandwidth     */ ES9081_DPLL_BW,
    /* filterMask        */ ESS8CH_FILTER_MASK,
    /* muteBit           */ ESS8CH_MUTE_BIT,
    /* vol0dB            */ ESS8CH_VOL_0DB,
    /* volMute           */ ESS8CH_VOL_MUTE,
    /* initSeq           */ kInitSeqES9081,
    /* initSeqLen        */ (uint8_t)(sizeof(kInitSeqES9081) / sizeof(kInitSeqES9081[0])),
    /* deinitSeq         */ kDeinitSeqES9081,
    /* deinitSeqLen      */ (uint8_t)(sizeof(kDeinitSeqES9081) / sizeof(kDeinitSeqES9081[0])),
    /* sinkNamePrefix    */ "ES9081",
    /* logPrefix         */ "[HAL:ES9081]",
};

const EssDac8chDescriptor kDescES9082 = {
    /* compatible        */ "ess,es9082",
    /* chipName          */ "ES9082",
    /* chipId            */ ES9082_CHIP_ID,
    /* altChipId         */ 0xFF,
    /* altChipName       */ nullptr,
    /* altCompatible     */ nullptr,
    /* capabilities      */ (uint32_t)(HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS | HAL_CAP_DPLL),
    /* sampleRateMask    */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K,
    /* supportedRates    */ kRates8ch,
    /* supportedRateCount*/ 6,
    /* regSysConfig      */ ESS8CH_REG_SYS_CONFIG,
    /* regInputCfg       */ ESS8CH_REG_INPUT_CFG,
    /* regFilterMute     */ ESS8CH_REG_FILTER_MUTE,
    /* regMasterMode     */ ESS8CH_REG_MASTER_MODE,
    /* regDpllCfg        */ ESS8CH_REG_DPLL_CFG,
    /* regSoftStart      */ ESS8CH_REG_SOFT_START,
    /* regVolCh1         */ ESS8CH_REG_VOL_CH1,
    /* regChipId         */ ESS8CH_REG_CHIP_ID,
    /* softResetBit      */ ESS8CH_SOFT_RESET_BIT,
    /* channelMode8ch    */ ESS8CH_CHANNEL_MODE_8CH,
    /* inputTdm32bit     */ (uint8_t)(ESS8CH_INPUT_TDM | ESS8CH_INPUT_32BIT),
    /* slaveMode         */ ESS8CH_SLAVE_MODE,
    /* dpllBandwidth     */ ES9082_DPLL_BW,
    /* filterMask        */ ESS8CH_FILTER_MASK,
    /* muteBit           */ ESS8CH_MUTE_BIT,
    /* vol0dB            */ ESS8CH_VOL_0DB,
    /* volMute           */ ESS8CH_VOL_MUTE,
    /* initSeq           */ kInitSeqES9082,
    /* initSeqLen        */ (uint8_t)(sizeof(kInitSeqES9082) / sizeof(kInitSeqES9082[0])),
    /* deinitSeq         */ kDeinitSeqES9082,
    /* deinitSeqLen      */ (uint8_t)(sizeof(kDeinitSeqES9082) / sizeof(kDeinitSeqES9082[0])),
    /* sinkNamePrefix    */ "ES9082",
    /* logPrefix         */ "[HAL:ES9082]",
};

const EssDac8chDescriptor kDescES9017 = {
    /* compatible        */ "ess,es9017",
    /* chipName          */ "ES9017",
    /* chipId            */ ES9017_CHIP_ID,
    /* altChipId         */ 0xFF,
    /* altChipName       */ nullptr,
    /* altCompatible     */ nullptr,
    /* capabilities      */ (uint32_t)(HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS | HAL_CAP_DPLL),
    /* sampleRateMask    */ HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K,
    /* supportedRates    */ kRates8ch,
    /* supportedRateCount*/ 6,
    /* regSysConfig      */ ESS8CH_REG_SYS_CONFIG,
    /* regInputCfg       */ ESS8CH_REG_INPUT_CFG,
    /* regFilterMute     */ ESS8CH_REG_FILTER_MUTE,
    /* regMasterMode     */ ESS8CH_REG_MASTER_MODE,
    /* regDpllCfg        */ ESS8CH_REG_DPLL_CFG,
    /* regSoftStart      */ ESS8CH_REG_SOFT_START,
    /* regVolCh1         */ ESS8CH_REG_VOL_CH1,
    /* regChipId         */ ESS8CH_REG_CHIP_ID,
    /* softResetBit      */ ESS8CH_SOFT_RESET_BIT,
    /* channelMode8ch    */ ESS8CH_CHANNEL_MODE_8CH,
    /* inputTdm32bit     */ (uint8_t)(ESS8CH_INPUT_TDM | ESS8CH_INPUT_32BIT),
    /* slaveMode         */ ESS8CH_SLAVE_MODE,
    /* dpllBandwidth     */ ES9017_DPLL_BW,
    /* filterMask        */ ESS8CH_FILTER_MASK,
    /* muteBit           */ ESS8CH_MUTE_BIT,
    /* vol0dB            */ ESS8CH_VOL_0DB,
    /* volMute           */ ESS8CH_VOL_MUTE,
    /* initSeq           */ kInitSeqES9017,
    /* initSeqLen        */ (uint8_t)(sizeof(kInitSeqES9017) / sizeof(kInitSeqES9017[0])),
    /* deinitSeq         */ kDeinitSeqES9017,
    /* deinitSeqLen      */ (uint8_t)(sizeof(kDeinitSeqES9017) / sizeof(kDeinitSeqES9017[0])),
    /* sinkNamePrefix    */ "ES9017",
    /* logPrefix         */ "[HAL:ES9017]",
};

// ===========================================================================
// HalEssDac8ch implementation
// ===========================================================================

HalEssDac8ch::HalEssDac8ch(const EssDac8chDescriptor& desc)
    : HalEssSabreDacBase(), _desc(desc)
{
    hal_init_descriptor(_descriptor, desc.compatible, desc.chipName, "ESS Technology",
        HAL_DEV_DAC, 8, ESS8CH_I2C_ADDR, HAL_BUS_I2C, HAL_I2C_BUS_EXP,
        desc.sampleRateMask, desc.capabilities);
    _initPriority = HAL_PRIORITY_HARDWARE;
    _i2cAddr     = ESS8CH_I2C_ADDR;
    _sdaPin      = ESS_SABRE_I2C_BUS2_SDA;
    _sclPin      = ESS_SABRE_I2C_BUS2_SCL;
    _i2cBusIndex = HAL_I2C_BUS_EXP;
}

// ---------------------------------------------------------------------------

void HalEssDac8ch::_execSequence(const EssDac8chRegWrite* seq, uint8_t len) {
    for (uint8_t i = 0; i < len; ++i) {
        if (seq[i].reg == DELAY_SENTINEL_8CH) {
#ifndef NATIVE_TEST
            delay((unsigned long)seq[i].val * 5UL);
#endif
        } else {
            _writeReg(seq[i].reg, seq[i].val);
        }
    }
}

// ---------------------------------------------------------------------------

bool HalEssDac8ch::probe() {
#ifndef NATIVE_TEST
    if (!_bus().probe(_i2cAddr)) return false;
    uint8_t chipId = _readReg(_desc.regChipId);
    if (chipId == _desc.chipId) return true;
    if (_desc.altChipId != 0xFF && chipId == _desc.altChipId) return true;
    return false;
#else
    return true;
#endif
}

// ---------------------------------------------------------------------------

HalInitResult HalEssDac8ch::init() {
    // ---- 1. Apply per-device config overrides ----
    _applyConfigOverrides();

    LOG_I("%s Initializing (I2C addr=0x%02X bus=%u SDA=%d SCL=%d sr=%luHz bits=%u)",
          _desc.logPrefix, _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth);

    // ---- 2. Select TwoWire instance and initialize I2C bus at 400 kHz ----
    _selectWire();

    // ---- 3. Execute init sequence (soft reset + configure) ----
    _execSequence(_desc.initSeq, _desc.initSeqLen);

    // ---- 4. Verify chip ID — handle PRO/MPRO variant detection ----
    uint8_t chipId = _readReg(_desc.regChipId);
    bool isAltVariant = (_desc.altChipId != 0xFF && chipId == _desc.altChipId);

    if (chipId == _desc.chipId) {
        LOG_I("%s Chip ID OK (0x%02X)", _desc.logPrefix, chipId);
    } else if (isAltVariant) {
        // Update descriptor fields for MPRO variant
        hal_safe_strcpy(_descriptor.name,       sizeof(_descriptor.name),       _desc.altChipName);
        hal_safe_strcpy(_descriptor.compatible,  sizeof(_descriptor.compatible), _desc.altCompatible);
        LOG_I("%s Alt variant detected: %s (0x%02X)", _desc.logPrefix, _desc.altChipName, chipId);
    } else {
        LOG_W("%s Unexpected chip ID: 0x%02X (expected 0x%02X) — continuing",
              _desc.logPrefix, chipId, _desc.chipId);
    }

    // ---- 5. Per-channel volume: apply stored level ----
    // Map 100% -> 0x00 (0 dB), 0% -> 0xFF (full attenuation)
    uint8_t volReg = (uint8_t)((100U - _volume) * 255U / 100U);
    for (uint8_t ch = 0; ch < 8; ++ch) {
        _writeReg((uint8_t)(_desc.regVolCh1 + ch), volReg);
    }

    // ---- 6. Digital filter preset (bits[2:0] of regFilterMute) ----
    uint8_t preset = (_filterPreset > 7U) ? 7U : _filterPreset;
    uint8_t filterReg = (uint8_t)(preset & _desc.filterMask);
    if (_muted) filterReg |= _desc.muteBit;
    _writeReg(_desc.regFilterMute, filterReg);

    // ---- 7. Enable expansion TDM TX output (8-slot TDM) ----
    HalDeviceConfig* tdmCfg = HalDeviceManager::instance().getConfig(_slot);
    uint8_t tdmPort = (tdmCfg && tdmCfg->valid && tdmCfg->i2sPort != 255) ? tdmCfg->i2sPort : 2;
#ifndef NATIVE_TEST
    gpio_num_t doutPin = (_doutPin >= 0) ? (gpio_num_t)_doutPin : GPIO_NUM_NC;
    gpio_num_t tdmMclk = (tdmCfg && tdmCfg->valid && tdmCfg->pinMclk >= 0)
                         ? (gpio_num_t)tdmCfg->pinMclk : (gpio_num_t)ES8311_I2S_MCLK_PIN;
    gpio_num_t tdmBck  = (tdmCfg && tdmCfg->valid && tdmCfg->pinBck  >= 0)
                         ? (gpio_num_t)tdmCfg->pinBck  : (gpio_num_t)ES8311_I2S_SCLK_PIN;
    gpio_num_t tdmWs   = (tdmCfg && tdmCfg->valid && tdmCfg->pinLrc  >= 0)
                         ? (gpio_num_t)tdmCfg->pinLrc  : (gpio_num_t)ES8311_I2S_LRCK_PIN;
    bool tdmOk = i2s_port_enable_tx(tdmPort, I2S_MODE_TDM, 8, doutPin, tdmMclk, tdmBck, tdmWs);
    if (!tdmOk) {
        LOG_E("%s Expansion TDM TX enable failed (port=%u)", _desc.logPrefix, tdmPort);
        _state = HAL_STATE_ERROR;
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "TDM TX init failed");
    }
    _i2sTxEnabled = true;
#else
    _i2sTxEnabled = true;
#endif

    // ---- 8. Init TDM interleaver ----
    if (!_tdm.init(tdmPort)) {
        LOG_E("%s TDM interleaver init failed — out of memory", _desc.logPrefix);
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "TDM interleaver alloc failed");
    }

    // ---- 9. Build 4 AudioOutputSink structs (one per stereo pair) ----
    // Use the variant-detected name prefix for MPRO detection
    const char* namePrefix = isAltVariant ? _desc.altChipName : _desc.sinkNamePrefix;
    snprintf(_sinkName0, sizeof(_sinkName0), "%s CH1/2", namePrefix);
    snprintf(_sinkName1, sizeof(_sinkName1), "%s CH3/4", namePrefix);
    snprintf(_sinkName2, sizeof(_sinkName2), "%s CH5/6", namePrefix);
    snprintf(_sinkName3, sizeof(_sinkName3), "%s CH7/8", namePrefix);

    _tdm.buildSinks(_sinkName0, _sinkName1, _sinkName2, _sinkName3,
                    &_sinks[0], &_sinks[1], &_sinks[2], &_sinks[3],
                    _slot);
    _sinksBuilt = true;

    // ---- 10. Mark device ready ----
    _initialized = true;
    _state = HAL_STATE_AVAILABLE;
    setReady(true);

    LOG_I("%s Ready (vol=%u%% muted=%d filter=%u sinks=4)",
          _desc.logPrefix, _volume, (int)_muted, _filterPreset);
    return hal_init_ok();
}

// ---------------------------------------------------------------------------

void HalEssDac8ch::deinit() {
    if (!_initialized) return;

    setReady(false);

    // Execute deinit sequence (mute before shutdown)
#ifndef NATIVE_TEST
    _execSequence(_desc.deinitSeq, _desc.deinitSeqLen);
#endif

    // Disable expansion TDM TX via port-generic API
#ifndef NATIVE_TEST
    if (_i2sTxEnabled) {
        HalDeviceConfig* deiCfg = HalDeviceManager::instance().getConfig(_slot);
        uint8_t deiPort = (deiCfg && deiCfg->valid && deiCfg->i2sPort != 255) ? deiCfg->i2sPort : 2;
        i2s_port_disable_tx(deiPort);
        _i2sTxEnabled = false;
    }
#else
    _i2sTxEnabled = false;
#endif

    _tdm.deinit();
    _initialized = false;
    _sinksBuilt  = false;
    _state       = HAL_STATE_REMOVED;

    LOG_I("%s Deinitialized", _desc.logPrefix);
}

// ---------------------------------------------------------------------------

void HalEssDac8ch::dumpConfig() {
    LOG_I("%s %s by ESS Technology (compat=%s) i2c=0x%02X bus=%u sda=%d scl=%d "
          "sr=%luHz bits=%u vol=%u%% muted=%d filter=%u",
          _desc.logPrefix,
          _descriptor.name, _descriptor.compatible,
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth,
          _volume, (int)_muted, _filterPreset);
}

// ---------------------------------------------------------------------------

bool HalEssDac8ch::healthCheck() {
#ifndef NATIVE_TEST
    if (!_initialized) return false;
    uint8_t id = _readReg(_desc.regChipId);
    if (id == _desc.chipId) return true;
    if (_desc.altChipId != 0xFF && id == _desc.altChipId) return true;
    return false;
#else
    return _initialized;
#endif
}

ClockStatus HalEssDac8ch::getClockStatus() {
    ClockStatus cs = {};
    if (!_initialized || _state != HAL_STATE_AVAILABLE) {
        strncpy(cs.description, "not available", sizeof(cs.description) - 1);
        return cs;
    }
#ifndef NATIVE_TEST
    uint8_t status = _readReg(ESS_SABRE_REG_DPLL_LOCK);
    cs.available = true;
    cs.locked = (status & ESS_SABRE_DPLL_LOCKED_BIT) != 0;
    strncpy(cs.description, cs.locked ? "DPLL locked" : "DPLL unlocked", sizeof(cs.description) - 1);
#else
    cs.available = true;
    cs.locked = true;
    strncpy(cs.description, "DPLL locked", sizeof(cs.description) - 1);
#endif
    return cs;
}

// ---------------------------------------------------------------------------

bool HalEssDac8ch::configure(uint32_t sampleRate, uint8_t bitDepth) {
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
    LOG_I("%s Configured: %luHz %ubit", _desc.logPrefix, (unsigned long)sampleRate, bitDepth);
    return true;
}

// ---------------------------------------------------------------------------

bool HalEssDac8ch::setVolume(uint8_t percent) {
    if (!_initialized) return false;
    if (percent > 100) percent = 100;

    // Map 100% -> 0x00 (0 dB), 0% -> 0xFF (full attenuation)
    uint8_t volReg = (uint8_t)((100U - percent) * 255U / 100U);
    bool ok = true;
    for (uint8_t ch = 0; ch < 8; ++ch) {
        ok = _writeReg((uint8_t)(_desc.regVolCh1 + ch), volReg) && ok;
    }

    _volume = percent;
    LOG_D("%s Volume: %d%% -> reg=0x%02X", _desc.logPrefix, percent, volReg);
    return ok;
}

// ---------------------------------------------------------------------------

bool HalEssDac8ch::setMute(bool mute) {
    if (!_initialized) return false;

    // Read-modify-write to preserve filter preset in bits[2:0]
    uint8_t reg = _readReg(_desc.regFilterMute);
    if (mute) {
        reg |=  _desc.muteBit;
    } else {
        reg &= (uint8_t)(~_desc.muteBit);
    }
    bool ok = _writeReg(_desc.regFilterMute, reg);
    _muted = mute;
    LOG_I("%s %s", _desc.logPrefix, mute ? "Muted" : "Unmuted");
    return ok;
}

// ---------------------------------------------------------------------------

bool HalEssDac8ch::setFilterPreset(uint8_t preset) {
    if (preset >= ESS_SABRE_FILTER_COUNT) {
        LOG_W("%s Invalid filter preset: %u (max %u)",
              _desc.logPrefix, preset, ESS_SABRE_FILTER_COUNT - 1);
        return false;
    }

    // Preserve mute bit when updating filter bits[2:0]
    uint8_t reg = _initialized ? _readReg(_desc.regFilterMute) : 0x00;
    reg &= (uint8_t)(~_desc.filterMask);
    reg |= (uint8_t)(preset & _desc.filterMask);

    bool ok = true;
    if (_initialized) {
        ok = _writeReg(_desc.regFilterMute, reg);
    }
    _filterPreset = preset;
    LOG_I("%s Filter preset: %u", _desc.logPrefix, preset);
    return ok;
}

// ---------------------------------------------------------------------------

bool HalEssDac8ch::buildSinkAt(int idx, uint8_t sinkSlot, AudioOutputSink* out) {
    if (!out) return false;
    if (idx < 0 || idx >= 4 || !_sinksBuilt) return false;
    *out = _sinks[idx];
    out->halSlot = _slot;
    (void)sinkSlot;  // firstChannel already set by _tdm.buildSinks()
    return true;
}

#endif // DAC_ENABLED
