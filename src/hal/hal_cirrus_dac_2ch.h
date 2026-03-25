#pragma once
#ifdef DAC_ENABLED
// HalCirrusDac2ch — Generic Cirrus Logic 2-channel DAC driver (Pattern C)
// Replaces 5 individual drivers (CS43198, CS43131, CS4398, CS4399, CS43130)
// with a single class driven by per-chip CirrusDac2chDescriptor tables.
//
// Chip-specific behaviour (register addressing type, I2C address, filter count,
// mute path, speed mode type, and optional feature registers) is fully described
// by the descriptor — no per-chip subclasses required.
//
// Compatible strings handled: "cirrus,cs43198", "cirrus,cs43131", "cirrus,cs4398",
//                              "cirrus,cs4399", "cirrus,cs43130"

#include "hal_cirrus_dac_base.h"

// ---------------------------------------------------------------------------
// Descriptor enums
// ---------------------------------------------------------------------------

// Register addressing model:
//   REG_8BIT         — 1-byte register address (CS4398 legacy)
//   REG_16BIT_PAGED  — 2-byte paged register address (CS43198, CS43131, CS4399, CS43130)
enum CirrusRegType : uint8_t { REG_8BIT = 0, REG_16BIT_PAGED = 1 };

// How mute is implemented:
//   MUTE_DEDICATED_REG  — write to separate mute register (CS4398 REG_MUTE_CTL)
//   MUTE_PCM_PATH_RMW   — read-modify-write bits in PCM path register (all paged chips)
enum CirrusMuteType : uint8_t { MUTE_DEDICATED_REG = 0, MUTE_PCM_PATH_RMW = 1 };

// How speed mode register is updated during configure():
//   SPEED_FM_BITS    — FM bits in MODE_CTL register (CS4398)
//   SPEED_CLOCK_CTL  — dedicated clock control register (paged chips)
enum CirrusSpeedType : uint8_t { SPEED_FM_BITS = 0, SPEED_CLOCK_CTL = 1 };

// ---------------------------------------------------------------------------
// Register write entry (supports both 8-bit and 16-bit paged addresses)
// ---------------------------------------------------------------------------
// A {0xFFFF, N} entry in an init/deinit sequence means delay(N * 5 ms).
struct CirrusDac2chRegWrite { uint16_t reg; uint8_t val; };

// ---------------------------------------------------------------------------
// Descriptor struct
// ---------------------------------------------------------------------------
struct CirrusDac2chDescriptor {
    // Identity
    const char* compatible;
    const char* chipName;
    uint8_t     chipId;          // Expected chip ID value
    uint8_t     chipIdMask;      // Mask to apply before comparing chip ID (0xF0 for CS4398, 0xFF for others)
    uint32_t    capabilities;    // HAL_CAP_* flags
    uint32_t    sampleRateMask;  // HAL_RATE_* flags

    // Supported sample rates for _validateSampleRate()
    const uint32_t* supportedRates;
    uint8_t         supportedRateCount;

    // I2C configuration
    uint8_t i2cAddr;             // Default I2C address (0x48 for most, 0x4C for CS4398)
    uint8_t maxBitDepth;         // Max bit depth (24 for CS4398, 32 for others)

    // Register addressing type
    CirrusRegType regType;       // 8-bit or 16-bit paged

    // Core register addresses (uint16_t to accommodate both 8-bit and 16-bit paged)
    uint16_t regChipId;          // Chip ID register
    uint16_t regPowerCtl;        // Power control register
    uint16_t regIfaceCtl;        // Interface control (I2S format + word length)
    uint16_t regPcmPath;         // PCM path register (filter + mute bits for paged chips)
    uint16_t regVolA;            // Channel A volume register
    uint16_t regVolB;            // Channel B volume register
    uint16_t regClockCtl;        // Clock control (speed mode) — 0xFFFF if using regModeCtl FM bits
    uint16_t regModeCtl;         // Mode control register (CS4398 only, 0xFFFF for paged chips)
    uint16_t regMuteCtl;         // Dedicated mute register (0xFFFF if mute embedded in PCM path)

    // Interface/word-length bits (used during init and configure())
    uint8_t ifaceDefault;        // Default interface register value (I2S + max bit depth)
    uint8_t wl16Bit;             // Word length bits for 16-bit audio
    uint8_t wl24Bit;             // Word length bits for 24-bit audio
    uint8_t wl32Bit;             // Word length bits for 32-bit audio

    // Mute configuration
    CirrusMuteType muteType;
    uint8_t        muteABit;     // Bit for channel A mute
    uint8_t        muteBBit;     // Bit for channel B mute
    uint8_t        muteBoth;     // Combined A+B mute mask

    // Filter configuration
    uint8_t filterCount;         // Total number of filter presets
    uint8_t filterMask;          // Mask for filter bits (e.g. 0x07)
    // Filter bits are always at bits[2:0] of the PCM path reg (or REG_RAMP_FILT for CS4398)
    uint8_t filterShift;         // Left shift for filter value (0 for paged chips, 2 for CS4398)
    uint16_t regFilter;          // Register that contains filter bits (may differ from regPcmPath for CS4398)

    // Speed mode configuration
    CirrusSpeedType speedType;
    uint8_t speedNormal;         // Value for single speed (≤48kHz)
    uint8_t speedDouble;         // Value for double speed (≤96kHz)
    uint8_t speedQuad;           // Value for quad speed (≤192kHz)
    uint8_t speedOctal;          // Value for octal speed (≤384kHz)
    // CS4398 FM speed mask (applied via read-modify-write on regModeCtl)
    uint8_t speedFmMask;         // 0x00 if not applicable

    // Init/deinit register sequences ({0xFFFF, N} = delay N*5 ms)
    const CirrusDac2chRegWrite* initSeq;
    uint8_t                     initSeqLen;
    const CirrusDac2chRegWrite* deinitSeq;
    uint8_t                     deinitSeqLen;

    // Optional feature registers (HP amp and/or NOS mode)
    // 0xFFFF in regHpAmp means "no HP amp"
    uint16_t regHpAmp;           // Headphone amp control register (CS43131/CS43130)
    uint8_t  hpAmpEnableVal;     // Value to write when enabling HP amp
    uint8_t  hpAmpDisableVal;    // Value to write when disabling HP amp

    // 0xFFFF in regNos means "no NOS mode"
    uint16_t regNos;             // NOS filter control register (CS4399/CS43130)
    uint8_t  nosEnableVal;       // Value to write when enabling NOS
    uint8_t  nosDisableVal;      // Value to write when disabling NOS

    // DSD mode control (present on chips with HAL_CAP_DSD)
    // 0xFFFF in regDsdPath means "no DSD support" (e.g. CS4399)
    // For CS4398: regDsdPath = regModeCtl (CHSL bit), regDsdInt = 0xFFFF (no iface reg)
    uint16_t regDsdPath;         // DSD path enable register (0xFFFF = no DSD)
    uint8_t  dsdPathEnable;      // Value to enable DSD path
    uint8_t  dsdPathDisable;     // Value to disable DSD path (return to PCM)
    uint8_t  dsdPathMask;        // Mask for read-modify-write (0xFF for CS4398 single-bit)
    uint16_t regDsdInt;          // DSD interface control register (0xFFFF = not applicable)
    uint8_t  dsdIntDefault;      // Default DSD interface config (written on DSD enable)
    uint8_t  dsdFuncMode;        // FUNC_MODE value for DSD (0xFF = not applicable)

    // Logging prefix (e.g. "[HAL:CS43198]")
    const char* logPrefix;
};

// ---------------------------------------------------------------------------
// Generic driver class
// ---------------------------------------------------------------------------
class HalCirrusDac2ch : public HalCirrusDacBase {
public:
    explicit HalCirrusDac2ch(const CirrusDac2chDescriptor& desc);
    virtual ~HalCirrusDac2ch() = default;

    // HalDevice lifecycle
    bool          probe()       override;
    HalInitResult init()        override;
    void          deinit()      override;
    void          dumpConfig()  override;
    bool          healthCheck() override;

    // HalAudioDevice
    bool configure(uint32_t sampleRate, uint8_t bitDepth) override;
    bool setVolume(uint8_t percent)                        override;
    bool setMute(bool mute)                                override;

    // Filter preset override
    bool setFilterPreset(uint8_t preset) override;

    // Generic optional feature APIs (HP amp / NOS mode)
    // Calls with no corresponding register in descriptor are no-ops returning false.
    bool setHeadphoneAmpEnabled(bool enable);
    bool isHeadphoneAmpEnabled() const;

    bool setNosMode(bool enable);
    bool isNosMode() const;

    // DSD mode switching — mute → write DSD regs → unmute sequence.
    // Returns false immediately if chip has no DSD support (regDsdPath == 0xFFFF).
    bool setDsdMode(bool enable);
    bool isDsdMode() const;

private:
    const CirrusDac2chDescriptor& _desc;
    bool _hpAmpEnabled = false;
    bool _nosEnabled   = false;
    bool _dsdEnabled   = false;

    // Dispatch write/read based on descriptor regType
    bool    _writeReg(uint16_t reg, uint8_t val);
    uint8_t _readReg(uint16_t reg);

    // Execute a {reg, val} sequence; sentinel {0xFFFF, N} delays N*5 ms
    void _execSequence(const CirrusDac2chRegWrite* seq, uint8_t len);

    // Compute speed mode value from sample rate
    uint8_t _computeSpeedMode(uint32_t sampleRate) const;

    // Compute word-length bits for configure()
    uint8_t _computeWordLengthBits(uint8_t bitDepth) const;
};

// ---------------------------------------------------------------------------
// Extern descriptor tables (defined in hal_cirrus_dac_2ch.cpp)
// ---------------------------------------------------------------------------
extern const CirrusDac2chDescriptor kDescCS43198;
extern const CirrusDac2chDescriptor kDescCS43131;
extern const CirrusDac2chDescriptor kDescCS4398;
extern const CirrusDac2chDescriptor kDescCS4399;
extern const CirrusDac2chDescriptor kDescCS43130;

#endif // DAC_ENABLED
