#pragma once
#ifdef DAC_ENABLED
// HalEs9820 — ESS ES9820 2-channel 32-bit ADC (HAL_DEV_ADC)
// Implements: HalAudioDevice + HalAudioAdcInterface
// I2C-controlled: sample rate, data gain (0-18 dB in 6 dB steps), HPF per-channel,
// per-channel digital filter preset, per-channel 16-bit volume.
// I2S output slave to ESP32-P4 I2S master.
// Default I2C bus: Bus 2 (expansion), GPIO28 SDA / GPIO29 SCL.
// Compatible string: "ess,es9820"

#include "hal_ess_sabre_adc_base.h"
#include "hal_types.h"
#include "../audio_input_source.h"

class HalEs9820 : public HalEssSabreAdcBase {
public:
    HalEs9820();
    virtual ~HalEs9820() {}

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
    bool     adcSetGain(uint8_t gainDb)       override;
    bool     adcSetHpfEnabled(bool en)        override;
    bool     adcSetSampleRate(uint32_t hz)    override;
    uint32_t adcGetSampleRate() const         override { return _sampleRate; }

    // ADC input source (bridge-registered into audio pipeline)
    const AudioInputSource* getInputSource() const override;

    // ES9820-specific extensions
    bool setFilterPreset(uint8_t preset);                      // 0-7 per-channel filter shapes (both set simultaneously)

private:
    AudioInputSource _inputSrc      = {};
    bool             _inputSrcReady = false;
};

#endif // DAC_ENABLED
