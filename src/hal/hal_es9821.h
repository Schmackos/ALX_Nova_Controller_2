#pragma once
#ifdef DAC_ENABLED
// HalEs9821 — ESS ES9821 2-channel ADC (HAL_DEV_ADC)
// Implements: HalEssSabreAdcBase (HalAudioDevice + HalAudioAdcInterface)
// I2C-controlled: 8 digital filter presets, per-channel 16-bit volume.
// I2S output slave to ESP32-P4 I2S master.
// Default I2C bus: Bus 2 (expansion), GPIO28 SDA / GPIO29 SCL.
// Compatible string: "ess,es9821"
//
// Chip specifics:
//   - Chip ID: 0x88 (reg 0xE1)
//   - Volume: reg 0x32/0x33 (CH1), 0x34/0x35 (CH2), 16-bit LSB+MSB
//   - No PGA (adcSetGain(0) accepted; non-zero returns false)
//   - Filter: reg 0x40 bits[4:2] = ADC_FILTER_SHAPE (3-bit, 8 presets 0-7)
//   - No dedicated HPF register (adcSetHpfEnabled stores flag, no-op on HW)

#include "hal_ess_sabre_adc_base.h"
#include "hal_types.h"
#include "../audio_input_source.h"

class HalEs9821 : public HalEssSabreAdcBase {
public:
    HalEs9821();
    virtual ~HalEs9821() {}

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
    // adcSetGain: only 0 dB is supported (no hardware PGA). Returns false for non-zero.
    bool     adcSetGain(uint8_t gainDb)    override;
    bool     adcSetHpfEnabled(bool en)     override;
    bool     adcSetSampleRate(uint32_t hz) override;
    uint32_t adcGetSampleRate() const      override { return _sampleRate; }

    // ADC input source (bridge-registered into audio pipeline)
    const AudioInputSource* getInputSource() const override;

    // ES9821-specific extensions
    bool setFilterPreset(uint8_t preset);                    // 0-7 filter shapes
    bool setChannelVolume(uint8_t channel, uint16_t vol16);  // per-channel 16-bit volume

private:
    AudioInputSource _inputSrc      = {};
    bool             _inputSrcReady = false;
};

#endif // DAC_ENABLED
