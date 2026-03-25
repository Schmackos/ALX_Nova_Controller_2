#pragma once
#ifdef DAC_ENABLED
// HalEssDac8ch — Generic ESS SABRE 8-channel DAC driver (Pattern D)
// Replaces 7 individual drivers (ES9038PRO, ES9028PRO, ES9039PRO/MPRO,
// ES9027PRO, ES9081, ES9082, ES9017) with a single class driven by
// per-chip EssDac8chDescriptor tables.
//
// All 7 chips share the same register layout:
//   - TDM 8-slot 32-bit input
//   - 8-bit per-channel volume (regs 0x0F-0x16), 0x00=0dB, 0xFF=mute
//   - Mute via bit5 (0x20) of the filter/mute register (0x07)
//   - Filter preset in bits[2:0] of reg 0x07
//   - Chip ID at reg 0xE1
//
// Only differences between chips:
//   - chip ID value at 0xE1
//   - DPLL bandwidth value at reg 0x0C
//   - ES9039PRO has an MPRO variant (alt chip ID 0x3A)
//
// Compatible strings handled:
//   "ess,es9038pro", "ess,es9028pro", "ess,es9039pro", "ess,es9039mpro",
//   "ess,es9027pro", "ess,es9081", "ess,es9082", "ess,es9017"

#include "hal_ess_sabre_dac_base.h"
#include "hal_tdm_interleaver.h"

// ---------------------------------------------------------------------------
// Register write / delay sentinel
// ---------------------------------------------------------------------------
// A {0xFF, N} entry in an init/deinit sequence means delay(N * 5 ms).
struct EssDac8chRegWrite { uint8_t reg; uint8_t val; };

// ---------------------------------------------------------------------------
// Descriptor struct
// ---------------------------------------------------------------------------
struct EssDac8chDescriptor {
    // Identity
    const char* compatible;
    const char* chipName;
    uint8_t     chipId;           // Expected value at reg 0xE1
    uint8_t     altChipId;        // 0xFF = no alternate (used for ES9039MPRO)
    const char* altChipName;      // nullptr = no alternate
    const char* altCompatible;    // nullptr = no alternate
    uint32_t    capabilities;     // HAL_CAP_* flags
    uint32_t    sampleRateMask;   // HAL_RATE_* flags

    // Supported sample rates for _validateSampleRate()
    const uint32_t* supportedRates;
    uint8_t         supportedRateCount;

    // Core register addresses (consistent across all 7 chips)
    uint8_t regSysConfig;         // 0x00 — soft reset + channel mode
    uint8_t regInputCfg;          // 0x01 — TDM input format
    uint8_t regFilterMute;        // 0x07 — filter bits[2:0] + mute bit5
    uint8_t regMasterMode;        // 0x0A — I2S slave mode
    uint8_t regDpllCfg;           // 0x0C — DPLL bandwidth
    uint8_t regSoftStart;         // 0x0E — soft start
    uint8_t regVolCh1;            // 0x0F — channel 1 volume
    uint8_t regChipId;            // 0xE1 — chip ID read register

    // Chip-specific values
    uint8_t softResetBit;         // Written to regSysConfig for reset (0x01)
    uint8_t channelMode8ch;       // Written to regSysConfig after reset (0x04)
    uint8_t inputTdm32bit;        // Written to regInputCfg (TDM | 32-bit)
    uint8_t slaveMode;            // Written to regMasterMode (0x00)
    uint8_t dpllBandwidth;        // Written to regDpllCfg (chip-specific)
    uint8_t filterMask;           // Mask for filter bits in regFilterMute (0x07)
    uint8_t muteBit;              // Mute bit in regFilterMute (0x20)
    uint8_t vol0dB;               // Volume register value for 0 dB (0x00)
    uint8_t volMute;              // Volume register value for full mute (0xFF)

    // Init/deinit register sequences ({0xFF, N} = delay N*5 ms)
    const EssDac8chRegWrite* initSeq;
    uint8_t                  initSeqLen;
    const EssDac8chRegWrite* deinitSeq;
    uint8_t                  deinitSeqLen;

    // Sink channel pair names (chip name prefix used to build e.g. "ES9038PRO CH1/2")
    // The descriptor provides the prefix string; HalEssDac8ch builds the 4 names at init.
    const char* sinkNamePrefix;   // e.g. "ES9038PRO"

    // Logging prefix (e.g. "[HAL:ES9038PRO]")
    const char* logPrefix;
};

// ---------------------------------------------------------------------------
// Generic driver class
// ---------------------------------------------------------------------------
class HalEssDac8ch : public HalEssSabreDacBase {
public:
    explicit HalEssDac8ch(const EssDac8chDescriptor& desc);
    virtual ~HalEssDac8ch() = default;

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
    ClockStatus getClockStatus() override;

    // Multi-sink: 4 stereo pairs from 8 channels
    int  getSinkCount() const override { return _sinksBuilt ? 4 : 0; }
    bool buildSinkAt(int idx, uint8_t sinkSlot, AudioOutputSink* out) override;

private:
    const EssDac8chDescriptor& _desc;
    HalTdmInterleaver          _tdm;
    AudioOutputSink            _sinks[4] = {};
    bool                       _sinksBuilt = false;

    // Sink name storage — built at init with per-variant prefix so pointers remain valid
    char _sinkName0[32] = {};
    char _sinkName1[32] = {};
    char _sinkName2[32] = {};
    char _sinkName3[32] = {};

    // Execute a {reg, val} sequence; sentinel {0xFF, N} delays N*5 ms
    void _execSequence(const EssDac8chRegWrite* seq, uint8_t len);
};

// ---------------------------------------------------------------------------
// Extern descriptor tables (defined in hal_ess_dac_8ch.cpp)
// ---------------------------------------------------------------------------
extern const EssDac8chDescriptor kDescES9038PRO;
extern const EssDac8chDescriptor kDescES9028PRO;
extern const EssDac8chDescriptor kDescES9039PRO;   // also handles MPRO via altChipId
extern const EssDac8chDescriptor kDescES9027PRO;
extern const EssDac8chDescriptor kDescES9081;
extern const EssDac8chDescriptor kDescES9082;
extern const EssDac8chDescriptor kDescES9017;

#endif // DAC_ENABLED
