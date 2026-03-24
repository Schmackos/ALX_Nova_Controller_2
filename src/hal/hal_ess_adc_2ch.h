#pragma once
#ifdef DAC_ENABLED
// HalEssAdc2ch — Generic ESS SABRE 2-channel ADC driver (Pattern A)
// Replaces 5 individual drivers (ES9822PRO, ES9826, ES9823PRO/MPRO, ES9821, ES9820)
// with a single class driven by per-chip EssAdc2chDescriptor tables.
//
// Chip-specific behaviour (gain range/encoding, HPF register path, filter placement,
// variant auto-detect) is fully described by the descriptor — no per-chip subclasses.
//
// Compatible strings handled: "ess,es9822pro", "ess,es9826", "ess,es9823pro",
//                              "ess,es9823mpro", "ess,es9821", "ess,es9820"

#include "hal_ess_sabre_adc_base.h"
#include "../audio_input_source.h"

// ---------------------------------------------------------------------------
// Descriptor enums
// ---------------------------------------------------------------------------

// How the PGA gain register(s) are encoded:
//   GAIN_NONE         — no hardware PGA, only 0 dB supported (ES9821)
//   GAIN_2BIT_6DB     — 2-bit field, 6 dB steps, max 18 dB (ES9820, ES9822PRO)
//   GAIN_3BIT_6DB     — 3-bit field, 6 dB steps, max 42 dB, two channels
//                       packed in one register (ES9823PRO; CH1 bits[2:0],
//                       CH2 bits[6:4])
//   GAIN_NIBBLE_3DB   — nibble-packed per-channel, 3 dB steps, max 30 dB
//                       (ES9826; both nibbles = same gain, high nibble = CH1,
//                       low nibble = CH2)
enum EssAdc2chGainType : uint8_t {
    GAIN_NONE        = 0,
    GAIN_2BIT_6DB    = 1,
    GAIN_3BIT_6DB    = 2,
    GAIN_NIBBLE_3DB  = 3
};

// How the HPF (DC blocking) register works:
//   HPF_NONE         — no dedicated register; flag is stored for API compat only
//   HPF_BIT_IN_DATAPATH — bit in per-channel datapath register (ES9822PRO, ES9820)
enum EssAdc2chHpfType : uint8_t {
    HPF_NONE              = 0,
    HPF_BIT_IN_DATAPATH   = 1
};

// How the digital filter preset field is placed in the filter register:
//   FILTER_SHIFT2_CH_PAIR — bits[4:2], two per-channel registers (ES9822PRO, ES9820,
//                            ES9826, ES9821)
//   FILTER_SHIFT5_SINGLE  — bits[7:5], single register (ES9823PRO)
enum EssAdc2chFilterType : uint8_t {
    FILTER_SHIFT2_CH_PAIR = 0,
    FILTER_SHIFT5_SINGLE  = 1
};

// ---------------------------------------------------------------------------
// Register write / delay sentinel  ({0xFF, N} = delay N*5 ms)
// ---------------------------------------------------------------------------
struct EssAdc2chRegWrite { uint8_t reg; uint8_t val; };

// ---------------------------------------------------------------------------
// Descriptor struct
// ---------------------------------------------------------------------------
struct EssAdc2chDescriptor {
    // Identity
    const char* compatible;
    const char* chipName;
    uint8_t     chipId;          // Expected value at regChipId
    // Alt variant (ES9823PRO/ES9823MPRO dual-variant auto-detect; 0 = none)
    uint8_t     altChipId;
    const char* altCompatible;   // nullptr = no alt
    const char* altChipName;     // nullptr = no alt

    uint16_t    capabilities;    // HAL_CAP_* flags
    uint32_t    sampleRateMask;  // HAL_RATE_* flags

    // Supported sample rates (for _validateSampleRate)
    const uint32_t* supportedRates;
    uint8_t         supportedRateCount;

    // Core register addresses
    uint8_t regChipId;           // Chip ID register (0xE1 on all ESS ADCs)
    uint8_t regSysConfig;        // System config / reset register (0x00)
    uint8_t softResetVal;        // Value to write for soft reset

    // Volume registers (16-bit; all 2ch ADCs use 16-bit volume)
    uint8_t regVolLsbCh1;        // CH1 volume LSB
    uint8_t regVolLsbCh2;        // CH2 volume LSB (consecutive MSB = regVolLsbCh + 1)
    // vol16: 0x0000 = mute, 0x7FFF = 0 dB

    // Gain
    EssAdc2chGainType gainType;
    uint8_t           regGainCh1;   // CH1 gain register (0xFF = no register)
    uint8_t           regGainCh2;   // CH2 gain register (0xFF = no register / single-reg)
    uint8_t           gainMax;      // Max dB value
    uint8_t           gainStep;     // Step in dB (3 or 6)
    uint8_t           gainMax_nibble; // Max nibble value for GAIN_NIBBLE_3DB (0 = unused)

    // HPF
    EssAdc2chHpfType hpfType;
    uint8_t          regHpfCh1;    // CH1 datapath / HPF register
    uint8_t          regHpfCh2;    // CH2 datapath / HPF register
    uint8_t          hpfEnableBit; // Bit mask to set/clear for HPF enable

    // Filter
    EssAdc2chFilterType filterType;
    uint8_t             regFilterCh1; // CH1 filter register (FILTER_SHIFT2_CH_PAIR)
    uint8_t             regFilterCh2; // CH2 filter register (0xFF = same as ch1 for single)

    // Init/deinit register sequences ({0xFF, N} = delay N*5 ms)
    const EssAdc2chRegWrite* initSeq;
    uint8_t                  initSeqLen;
    const EssAdc2chRegWrite* deinitSeq;
    uint8_t                  deinitSeqLen;

    // Logging prefix (e.g. "[HAL:ES9822PRO]")
    const char* logPrefix;
};

// ---------------------------------------------------------------------------
// Generic driver class
// ---------------------------------------------------------------------------
class HalEssAdc2ch : public HalEssSabreAdcBase {
public:
    explicit HalEssAdc2ch(const EssAdc2chDescriptor& desc);
    virtual ~HalEssAdc2ch() = default;

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

    // AudioInputSource
    const AudioInputSource* getInputSource() const override;

    // Filter preset (0-7)
    bool setFilterPreset(uint8_t preset);

private:
    const EssAdc2chDescriptor& _desc;
    AudioInputSource _inputSrc      = {};
    bool             _inputSrcReady = false;
    bool             _isAltVariant  = false;   // true when altChipId detected

    // Execute a {reg, val} sequence; sentinel {0xFF, N} delays N*5 ms
    void _execSequence(const EssAdc2chRegWrite* seq, uint8_t len);

    // Compute gain register value from gainDb
    uint8_t _computeGainReg(uint8_t gainDb) const;
    uint8_t _clampGainDb(uint8_t gainDb) const;
};

// ---------------------------------------------------------------------------
// Extern descriptor tables (defined in hal_ess_adc_2ch.cpp)
// ---------------------------------------------------------------------------
extern const EssAdc2chDescriptor kDescES9822PRO;
extern const EssAdc2chDescriptor kDescES9826;
extern const EssAdc2chDescriptor kDescES9823PRO;
extern const EssAdc2chDescriptor kDescES9821;
extern const EssAdc2chDescriptor kDescES9820;

#endif // DAC_ENABLED
