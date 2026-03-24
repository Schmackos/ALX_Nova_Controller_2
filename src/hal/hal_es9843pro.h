#pragma once
#ifdef DAC_ENABLED
// HalEs9843pro — ESS ES9843PRO 4-channel 32-bit ADC (HAL_DEV_ADC)
// Implements: HalAudioDevice + HalAudioAdcInterface
// I2C-controlled: sample rate, PGA gain (0–42 dB in 6 dB steps), HPF per-channel,
// global digital filter preset (0-7), per-channel 8-bit volume. I2S output slave
// to ESP32-P4 I2S master.
// Default I2C bus: Bus 2 (expansion), GPIO28 SDA / GPIO29 SCL.
// Compatible string: "ess,es9843pro"
//
// TDM deinterleaver
// -----------------
// The ES9843PRO outputs all 4 channels on a single I2S DATA line in TDM mode:
//   [SLOT0=CH1][SLOT1=CH2][SLOT2=CH3][SLOT3=CH4] per frame, 32-bit per slot.
//
// This driver exposes TWO AudioInputSource entries to the bridge:
//   source index 0 → "ES9843PRO CH1/2"  (SLOT0 + SLOT1)
//   source index 1 → "ES9843PRO CH3/4"  (SLOT2 + SLOT3)
//
// The bridge discovers the dual-source nature via getInputSourceCount() /
// getInputSourceAt(idx) and registers each at its own pipeline lane.
// getInputSource() returns the first source for backward compatibility with
// existing bridge code that calls it unconditionally.

#include "hal_ess_sabre_adc_base.h"
#include "hal_types.h"
#include "hal_tdm_deinterleaver.h"
#include "../audio_input_source.h"

class HalEs9843pro : public HalEssSabreAdcBase {
public:
    HalEs9843pro();
    virtual ~HalEs9843pro() {}

    // HalDevice lifecycle
    bool          probe()       override;
    HalInitResult init()        override;
    void          deinit()      override;
    void          dumpConfig()  override;
    bool          healthCheck() override;

    // HalAudioDevice
    bool configure(uint32_t sampleRate, uint8_t bitDepth) override;
    bool setVolume(uint8_t percent) override;
    bool setMute(bool mute) override;

    // HalAudioAdcInterface
    bool     adcSetGain(uint8_t gainDb)    override;
    bool     adcSetHpfEnabled(bool en)     override;
    bool     adcSetSampleRate(uint32_t hz) override;
    uint32_t adcGetSampleRate() const      override { return _sampleRate; }

    // Multi-source ADC interface — bridge queries count then each source by index.
    // Returns 2 when initialized (CH1/2 and CH3/4 stereo pairs); 0 otherwise.
    // Both sources are valid after init() regardless of whether a DMA read has occurred.
    int getInputSourceCount() const override { return _initialized ? 2 : 0; }
    const AudioInputSource* getInputSourceAt(int idx) const override;

    // Backward-compat single-source accessor (returns source 0).
    const AudioInputSource* getInputSource() const override {
        return getInputSourceAt(0);
    }

    // ES9843PRO-specific extensions
    bool setFilterPreset(uint8_t preset);                      // 0-7 global filter shapes
    bool setChannelVolume(uint8_t ch, uint8_t vol8);           // per-channel 8-bit volume

private:
    // TDM deinterleaver — owns the ping-pong DMA split and both AudioInputSource structs
    HalTdmDeinterleaver _tdm;

    // Source structs populated by _tdm.buildSources() during init()
    AudioInputSource _srcA = {};   // CH1/CH2 — registered at pipeline lane N
    AudioInputSource _srcB = {};   // CH3/CH4 — registered at pipeline lane N+1

    // Human-readable names (stored here so pointers in AudioInputSource stay valid)
    static constexpr const char* _NAME_A = "ES9843PRO CH1/2";
    static constexpr const char* _NAME_B = "ES9843PRO CH3/4";
};

#endif // DAC_ENABLED
