#pragma once
#ifdef DAC_ENABLED
// HalEs9039q2m — ESS ES9039Q2M 2-channel 32-bit DAC (HAL_DEV_DAC)
// Implements: HalEssSabreDacBase (HalAudioDevice + HalAudioDacInterface)
// I2C-controlled: volume (128-step, 0.5dB/step), mute, 7 digital filter presets,
// I2S slave input from ESP32-P4 I2S master, clock gear for 384kHz/768kHz operation.
// Architecture: Hyperstream IV (130dB DNR, newest modulator), PCM up to 768kHz,
// DSD1024, ~40mW typical. Filter presets 6 and 7 are Hyperstream IV hybrid modes.
// Default I2C bus: Bus 2 (expansion), GPIO28 SDA / GPIO29 SCL.
// Compatible string: "ess,es9039q2m"

#include "hal_ess_sabre_dac_base.h"

class HalEs9039q2m : public HalEssSabreDacBase {
public:
    HalEs9039q2m();
    virtual ~HalEs9039q2m() {}

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

    // Filter preset (overrides base default)
    bool setFilterPreset(uint8_t preset) override;
};

#endif // DAC_ENABLED
