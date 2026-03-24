#pragma once
#ifdef DAC_ENABLED
// HalEs9823pro — ESS ES9823PRO / ES9823MPRO 2-channel 32-bit SMART ADC (HAL_DEV_ADC)
// Implements: HalAudioDevice + HalAudioAdcInterface
// Both chip variants (ES9823PRO chip ID 0x8D, ES9823MPRO chip ID 0x8C) share this driver.
// I2C-controlled: sample rate, digital gain (0-42 dB in 6 dB steps), digital filter presets,
// per-channel 16-bit volume. I2S output slave to ESP32-P4 I2S master.
// Default I2C bus: Bus 2 (expansion), GPIO28 SDA / GPIO29 SCL.
// Compatible strings: "ess,es9823pro" (primary), "ess,es9823mpro" (detected by chip ID)

#include "hal_ess_sabre_adc_base.h"
#include "hal_types.h"
#include "../audio_input_source.h"

class HalEs9823pro : public HalEssSabreAdcBase {
public:
    HalEs9823pro();
    virtual ~HalEs9823pro() {}

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

    // ES9823PRO-specific extensions
    bool setFilterPreset(uint8_t preset);                      // 0-7 filter shapes
    bool setChannelVolume(uint8_t channel, uint16_t vol16);    // per-channel 16-bit volume

private:
    bool     _isMonolithic = false;  // true when ES9823MPRO (chip ID 0x8C) is detected

    AudioInputSource _inputSrc      = {};
    bool             _inputSrcReady = false;
};

#endif // DAC_ENABLED
