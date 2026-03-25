#pragma once
#ifdef DAC_ENABLED
// HalEssAdc4ch — Generic ESS SABRE 4-channel TDM ADC driver (Pattern B)
// Replaces 4 individual drivers (ES9843PRO, ES9842PRO, ES9841, ES9840)
// with a single class driven by per-chip EssAdc4chDescriptor tables.
//
// All Pattern B chips output 4 channels on a single TDM data line and register
// TWO AudioInputSources (CH1/2 + CH3/4) via HalTdmDeinterleaver.
//
// Compatible strings handled: "ess,es9843pro", "ess,es9842pro",
//                              "ess,es9841", "ess,es9840"

#include "hal_ess_sabre_adc_base.h"
#include "hal_tdm_deinterleaver.h"
#include "../audio_input_source.h"

// ---------------------------------------------------------------------------
// Descriptor enums
// ---------------------------------------------------------------------------

// How the 4-channel volume registers are encoded:
//   VOL4_16BIT   — 16-bit per-channel (0x7FFF=0dB, 0x0000=mute): ES9842PRO, ES9840
//   VOL4_8BIT    — 8-bit per-channel  (0x00=0dB,   0xFF=mute)  : ES9843PRO
//   VOL4_8BIT_INV — 8-bit per-channel (0xFF=0dB,   0x00=mute)  : ES9841
enum EssAdc4chVolType : uint8_t {
    VOL4_16BIT    = 0,
    VOL4_8BIT     = 1,    // 0x00=0dB, 0xFF=mute
    VOL4_8BIT_INV = 2     // 0xFF=0dB, 0x00=mute (ES9841)
};

// How the PGA gain register(s) are encoded:
//   GAIN4_2BIT_6DB  — 2-bit per-channel separate registers, 6 dB steps (ES9842PRO, ES9840)
//   GAIN4_3BIT_6DB  — 3-bit per channel pair packed in 2 registers,
//                     6 dB steps (ES9843PRO, ES9841)
enum EssAdc4chGainType : uint8_t {
    GAIN4_2BIT_6DB = 0,   // 2-bit separate regs per ch: bits[1:0]
    GAIN4_3BIT_6DB = 1    // 3-bit packed pairs:
                          //   ES9843PRO: reg0x55 bits[2:0]=CH1, bits[5:3]=CH2
                          //              reg0x56 bits[2:0]=CH3, bits[5:3]=CH4
                          //   ES9841:    reg0x55 bits[2:0]=CH1, bits[6:4]=CH2
                          //              reg0x56 bits[2:0]=CH3, bits[6:4]=CH4
};

// Subtype for 3-bit packed gain register layout:
//   GAIN4_PACK_3_5  — pair packing: CH1=bits[2:0], CH2=bits[5:3] (ES9843PRO)
//   GAIN4_PACK_0_4  — pair packing: CH1=bits[2:0], CH2=bits[6:4] (ES9841)
enum EssAdc4chGainPack : uint8_t {
    GAIN4_PACK_3_5 = 0,   // CH2 in bits[5:3]
    GAIN4_PACK_0_4 = 1    // CH2 in bits[6:4]
};

// How the filter preset is encoded:
//   FILTER4_GLOBAL_SHIFT5 — single global register, bits[7:5] (ES9843PRO, ES9841)
//   FILTER4_PER_CH_SHIFT2 — per-channel registers, bits[4:2] (ES9842PRO, ES9840)
enum EssAdc4chFilterType : uint8_t {
    FILTER4_GLOBAL_SHIFT5 = 0,
    FILTER4_PER_CH_SHIFT2 = 1
};

// ---------------------------------------------------------------------------
// Register write / delay sentinel  ({0xFF, N} = delay N*5 ms)
// ---------------------------------------------------------------------------
struct EssAdc4chRegWrite { uint8_t reg; uint8_t val; };

// ---------------------------------------------------------------------------
// Descriptor struct
// ---------------------------------------------------------------------------
struct EssAdc4chDescriptor {
    // Identity
    const char* compatible;
    const char* chipName;
    uint8_t     chipId;

    uint32_t    capabilities;    // HAL_CAP_* flags
    uint32_t    sampleRateMask;  // HAL_RATE_* flags

    // Supported sample rates
    const uint32_t* supportedRates;
    uint8_t         supportedRateCount;

    // Core register addresses
    uint8_t regChipId;           // Chip ID register (0xE1)
    uint8_t regSysConfig;        // System config / reset / TDM enable (0x00)
    uint8_t softResetVal;        // Value to write for soft reset
    uint8_t tdmEnableVal;        // Value to write to enable TDM output

    // Volume registers (4 channels, consecutive from volCh1)
    EssAdc4chVolType volType;
    uint8_t          regVolCh1;  // CH1 volume register
    // CH2=regVolCh1+1, CH3=regVolCh1+2, CH4=regVolCh1+3 (consecutive for all 4ch chips)

    // Gain
    EssAdc4chGainType gainType;
    EssAdc4chGainPack gainPack;  // Only relevant for GAIN4_3BIT_6DB
    uint8_t           regGainPair1;   // First pair gain register (CH1+CH2)
    uint8_t           regGainPair2;   // Second pair gain register (CH3+CH4)
    uint8_t           gainMax;        // Max dB (18 or 42)
    uint8_t           gainMask;       // Bit mask for per-channel gain field

    // HPF (DC blocking) — all 4ch ADCs have per-channel HPF at same offsets
    uint8_t regHpfCh1;          // CH1 DC blocking register
    uint8_t regHpfCh2;          // CH2 DC blocking register
    uint8_t regHpfCh3;          // CH3 DC blocking register
    uint8_t regHpfCh4;          // CH4 DC blocking register
    uint8_t hpfEnableBit;       // Bit to set/clear

    // Filter
    EssAdc4chFilterType filterType;
    uint8_t             regFilter;     // Global or CH1 filter register
    // For FILTER4_PER_CH_SHIFT2: regFilter=CH1, regFilter+2=CH2, etc.
    // (Actual per-ch regs are non-consecutive on ES9842PRO/ES9840, so we store them)
    uint8_t             regFilterCh2;  // Only used for FILTER4_PER_CH_SHIFT2
    uint8_t             regFilterCh3;  // Only used for FILTER4_PER_CH_SHIFT2
    uint8_t             regFilterCh4;  // Only used for FILTER4_PER_CH_SHIFT2
    uint8_t             filterMask;    // Bit mask for filter field

    // Source names for TDM pair outputs
    const char* sourceNameA;     // e.g. "ES9843PRO CH1/2"
    const char* sourceNameB;     // e.g. "ES9843PRO CH3/4"

    // Init/deinit register sequences
    const EssAdc4chRegWrite* initSeq;
    uint8_t                  initSeqLen;
    const EssAdc4chRegWrite* deinitSeq;
    uint8_t                  deinitSeqLen;

    // Logging prefix
    const char* logPrefix;
};

// ---------------------------------------------------------------------------
// Generic driver class
// ---------------------------------------------------------------------------
class HalEssAdc4ch : public HalEssSabreAdcBase {
public:
    explicit HalEssAdc4ch(const EssAdc4chDescriptor& desc);
    virtual ~HalEssAdc4ch() = default;

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

    // HalAudioAdcInterface
    bool     adcSetGain(uint8_t gainDb)    override;
    bool     adcSetHpfEnabled(bool en)     override;
    bool     adcSetSampleRate(uint32_t hz) override;
    uint32_t adcGetSampleRate() const      override { return _sampleRate; }

    // Multi-source ADC interface — bridge queries count then each source by index.
    // Returns 2 when initialized (CH1/2 and CH3/4 stereo pairs); 0 otherwise.
    int getInputSourceCount() const override { return _initialized ? 2 : 0; }
    const AudioInputSource* getInputSourceAt(int idx) const override;

    // Backward-compat single-source accessor (returns source 0).
    const AudioInputSource* getInputSource() const override {
        return getInputSourceAt(0);
    }

    // Filter preset (0-7)
    bool setFilterPreset(uint8_t preset);

private:
    const EssAdc4chDescriptor& _desc;

    // TDM deinterleaver — owns the ping-pong DMA split and both AudioInputSource structs
    HalTdmDeinterleaver _tdm;

    // Source structs populated by _tdm.buildSources() during init()
    AudioInputSource _srcA = {};
    AudioInputSource _srcB = {};

    // Saved 8-bit volume for ES9841 mute/unmute (VOL4_8BIT_INV: 0xFF=0dB)
    uint8_t _savedVol8 = 0xFF;

    // Execute a {reg, val} sequence; sentinel {0xFF, N} delays N*5 ms
    void _execSequence(const EssAdc4chRegWrite* seq, uint8_t len);

    // Write gain registers based on gainStep
    void _writeGainRegs(uint8_t gainStep);

    // Write all 4 HPF registers
    void _writeHpfRegs(bool en);

    // Write all 4 filter registers
    void _writeFilterRegs(uint8_t preset);

    // Write all 4 volume registers (8-bit)
    void _writeVol8Regs(uint8_t vol);

    // Write all 4 volume registers (16-bit)
    void _writeVol16Regs(uint16_t vol);
};

// ---------------------------------------------------------------------------
// Extern descriptor tables (defined in hal_ess_adc_4ch.cpp)
// ---------------------------------------------------------------------------
extern const EssAdc4chDescriptor kDescES9843PRO;
extern const EssAdc4chDescriptor kDescES9842PRO;
extern const EssAdc4chDescriptor kDescES9841;
extern const EssAdc4chDescriptor kDescES9840;

#endif // DAC_ENABLED
