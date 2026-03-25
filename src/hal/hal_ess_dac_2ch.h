#pragma once
#ifdef DAC_ENABLED
// HalEssDac2ch — Generic ESS SABRE 2-channel DAC driver (Pattern C)
// Replaces 5 individual drivers (ES9038Q2M, ES9039Q2M, ES9069Q, ES9033Q, ES9020)
// with a single class driven by per-chip EssDac2chDescriptor tables.
//
// Chip-specific behaviour (vol register count, filter placement, mute path,
// sample-rate reconfig type, and optional feature register) is fully described
// by the descriptor — no per-chip subclasses required.
//
// Compatible strings handled: "ess,es9038q2m", "ess,es9039q2m", "ess,es9069q",
//                              "ess,es9033q", "ess,es9020-dac"

#include "hal_ess_sabre_dac_base.h"

// ---------------------------------------------------------------------------
// Descriptor enums
// ---------------------------------------------------------------------------

// How the volume register(s) are laid out:
//   VOL_DUAL_0xFF  — two regs (0x0F + 0x10), 0xFF range  (ES9038Q2M, ES9039Q2M, ES9069Q, ES9033Q)
//   VOL_SINGLE_128 — one reg  (0x0F only),  128 steps    (ES9020)
enum EssDac2chVolType : uint8_t { VOL_DUAL_0xFF = 0, VOL_SINGLE_128 = 1 };

// How mute is implemented:
//   MUTE_VIA_VOLUME        — write 0xFF to vol reg(s) when muting; restore on unmute
//   MUTE_VIA_DEDICATED_BIT — toggle a dedicated bit in the filter/mute register
enum EssDac2chMuteType : uint8_t { MUTE_VIA_VOLUME = 0, MUTE_VIA_DEDICATED_BIT = 1 };

// Where the filter preset bits live inside regFilter:
//   FILTER_BITS_2_0            — bits[2:0], written directly (ES9069Q, ES9033Q, ES9020)
//   FILTER_BITS_4_2_WITH_MUTE  — bits[4:2], preserve mute bit[0] (ES9038Q2M, ES9039Q2M)
enum EssDac2chFilterType : uint8_t { FILTER_BITS_2_0 = 0, FILTER_BITS_4_2_WITH_MUTE = 1 };

// What configure(sampleRate, bitDepth) updates beyond storing the values:
//   RECONFIG_NONE         — store only (no register write needed at runtime)
//   RECONFIG_CLOCK_GEAR   — write clock gear reg (ES9038Q2M, ES9039Q2M)
//   RECONFIG_WORD_LENGTH  — write word-length reg (ES9069Q, ES9033Q)
enum EssDac2chReconfigType : uint8_t {
    RECONFIG_NONE        = 0,
    RECONFIG_CLOCK_GEAR  = 1,
    RECONFIG_WORD_LENGTH = 2
};

// ---------------------------------------------------------------------------
// Register write / delay sentinel
// ---------------------------------------------------------------------------
// A {0xFF, N} entry in an init/deinit sequence means delay(N * 5 ms).
struct EssDac2chRegWrite { uint8_t reg; uint8_t val; };

// ---------------------------------------------------------------------------
// Descriptor struct
// ---------------------------------------------------------------------------
struct EssDac2chDescriptor {
    // Identity
    const char* compatible;
    const char* chipName;
    uint8_t     chipId;            // Expected value at reg 0xE1 (ESS_SABRE_REG_CHIP_ID)
    uint16_t    capabilities;      // HAL_CAP_* flags
    uint32_t    sampleRateMask;    // HAL_RATE_* flags

    // Supported sample rates for _validateSampleRate()
    const uint32_t* supportedRates;
    uint8_t         supportedRateCount;

    // Core register addresses (consistent across all 5 chips)
    uint8_t regVolL;               // 0x0F on all chips
    uint8_t regVolR;               // 0x10 (or 0xFF = single-reg chip, ES9020)
    uint8_t regFilter;             // 0x07 on all chips

    // Volume / mute / filter behaviour
    EssDac2chVolType    volType;
    EssDac2chMuteType   muteType;
    uint8_t             muteBitMask;    // bit to toggle when MUTE_VIA_DEDICATED_BIT (e.g. 0x01)
    EssDac2chFilterType filterType;
    uint8_t             filterShift;   // bit shift for filter value (0 or 2)
    uint8_t             filterMask;    // mask for filter field (0x07 or 0x1C)

    // Runtime reconfiguration behaviour
    EssDac2chReconfigType reconfigType;
    uint8_t               regClockGear;   // reg written for RECONFIG_CLOCK_GEAR (0xFF = unused)
    uint8_t               regInputConfig; // reg written for RECONFIG_WORD_LENGTH (e.g. 0x01)

    // Init/deinit register sequences ({0xFF, N} = delay N*5 ms)
    const EssDac2chRegWrite* initSeq;
    uint8_t                  initSeqLen;
    const EssDac2chRegWrite* deinitSeq;
    uint8_t                  deinitSeqLen;

    // Chip-specific optional feature (MQA on ES9069Q, line driver on ES9033Q, APLL on ES9020)
    // 0xFF in regFeature means "no feature" — all feature calls return false.
    uint8_t regFeature;         // I2C register for the feature (0xFF = none)
    uint8_t featureEnableVal;   // Value to write when enabling
    uint8_t featureDisableVal;  // Value to write when disabling
    uint8_t regFeatureStatus;   // Status register (0xFF = none / not applicable)
    uint8_t featureStatusMask;  // Mask applied to status reg to test "active"

    // APLL clock source (ES9020 only): written before/after APLL enable
    uint8_t regClockSource;   // 0xFF = not used
    uint8_t clkSrcEnabled;    // e.g. ES9020_CLK_BCK_RECOVERY
    uint8_t clkSrcDisabled;   // e.g. ES9020_CLK_MCLK

    // Logging prefix (e.g. "[HAL:ES9038Q2M]")
    const char* logPrefix;
};

// ---------------------------------------------------------------------------
// Generic driver class
// ---------------------------------------------------------------------------
class HalEssDac2ch : public HalEssSabreDacBase {
public:
    explicit HalEssDac2ch(const EssDac2chDescriptor& desc);
    virtual ~HalEssDac2ch() = default;

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

    // Clock quality diagnostics — reads DPLL lock status from register ESS_SABRE_REG_DPLL_LOCK (0xE2).
    // Returns available=true when device is AVAILABLE; locked reflects bit0 of the status register.
    ClockStatus getClockStatus() override;

    // Generic chip-specific feature API (MQA / line driver / APLL)
    bool setFeatureEnabled(bool enable);
    bool isFeatureActive() const;

private:
    const EssDac2chDescriptor& _desc;
    bool _featureEnabled = false;

    // Execute a {reg, val} sequence; sentinel {0xFF, N} delays N*5 ms
    void _execSequence(const EssDac2chRegWrite* seq, uint8_t len);

    // Compute clock gear value from sample rate (ES9038Q2M/ES9039Q2M pattern)
    uint8_t _computeClockGear(uint32_t sampleRate) const;

    // Compute word-length bits for ES9069Q / ES9033Q reconfiguration
    uint8_t _computeWordLength(uint8_t bitDepth) const;
};

// ---------------------------------------------------------------------------
// Extern descriptor tables (defined in hal_ess_dac_2ch.cpp)
// ---------------------------------------------------------------------------
extern const EssDac2chDescriptor kDescES9038Q2M;
extern const EssDac2chDescriptor kDescES9039Q2M;
extern const EssDac2chDescriptor kDescES9069Q;
extern const EssDac2chDescriptor kDescES9033Q;
extern const EssDac2chDescriptor kDescES9020Dac;

#endif // DAC_ENABLED
