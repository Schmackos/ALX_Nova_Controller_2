#pragma once
#ifdef DAC_ENABLED
// HalEs9822pro — ESS ES9822PRO 2-channel 32-bit ADC (HAL_DEV_ADC)
// Implements: HalAudioDevice + HalAudioAdcInterface
// I2C-controlled: sample rate, PGA gain (0/6/12/18 dB), HPF, digital filter presets,
// per-channel 16-bit volume. I2S output slave to ESP32-P4 I2S master.
// Default I2C bus: Bus 2 (expansion), GPIO28 SDA / GPIO29 SCL.
// Compatible string: "ess,es9822pro"

#include "hal_ess_sabre_adc_base.h"
#include "hal_types.h"
#include "../audio_input_source.h"

class HalEs9822pro : public HalEssSabreAdcBase {
public:
    HalEs9822pro();
    virtual ~HalEs9822pro() {}

    // HalDevice lifecycle
    bool         probe()       override;
    HalInitResult init()       override;
    void         deinit()      override;
    void         dumpConfig()  override;
    bool         healthCheck() override;

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

    // ES9822PRO-specific extensions
    bool setFilterPreset(uint8_t preset);                         // 0-7 filter shapes
    bool setChannelVolume(uint8_t channel, uint16_t vol16);       // per-channel 16-bit volume

private:
    AudioInputSource _inputSrc      = {};
    bool             _inputSrcReady = false;
};

#endif // DAC_ENABLED
