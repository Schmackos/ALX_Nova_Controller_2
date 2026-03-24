#pragma once
#ifdef DAC_ENABLED
// HalEs9826 — ESS ES9826 2-channel ADC (HAL_DEV_ADC)
// Implements: HalEssSabreAdcBase (HalAudioDevice + HalAudioAdcInterface)
// I2C-controlled: PGA gain (0-30dB in 3dB steps), 8 digital filter presets,
// per-channel 16-bit volume. I2S output slave to ESP32-P4 I2S master.
// Default I2C bus: Bus 2 (expansion), GPIO28 SDA / GPIO29 SCL.
// Compatible string: "ess,es9826"
//
// Chip specifics vs ES9822PRO:
//   - Chip ID: 0x8A (reg 0xE1)
//   - Volume: reg 0x2D/0x2E (CH1), 0x2F/0x30 (CH2), 16-bit LSB+MSB
//   - PGA gain: reg 0x44 nibble-packed (CH2 bits[7:4], CH1 bits[3:0]),
//               nibble = gainDb / 3, max nibble 10 (30 dB)
//   - Filter: reg 0x3B bits[4:2] = FILTER_SHAPE (3-bit, 8 presets 0-7)
//   - No dedicated HPF register (adcSetHpfEnabled stores flag, no-op on HW)

#include "hal_ess_sabre_adc_base.h"
#include "hal_types.h"
#include "../audio_input_source.h"

class HalEs9826 : public HalEssSabreAdcBase {
public:
    HalEs9826();
    virtual ~HalEs9826() {}

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

    // ADC input source (bridge-registered into audio pipeline)
    const AudioInputSource* getInputSource() const override;

    // ES9826-specific extensions
    bool setFilterPreset(uint8_t preset);                    // 0-7 filter shapes

private:
    AudioInputSource _inputSrc      = {};
    bool             _inputSrcReady = false;
};

#endif // DAC_ENABLED
