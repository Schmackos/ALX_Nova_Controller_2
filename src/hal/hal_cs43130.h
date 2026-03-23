#pragma once
#ifdef DAC_ENABLED
// HalCs43130 — Cirrus Logic CS43130 2-channel 32-bit MasterHIFI DAC + Headphone Amp + NOS Filter (HAL_DEV_DAC)
// Implements: HalCirrusDacBase (HalAudioDevice + HalAudioDacInterface)
// I2C-controlled via 16-bit paged register addressing: per-channel attenuation
// volume (0.5dB steps), 5 digital filter presets (including NOS mode), mute, DSD support.
// NOS (Non-OverSampling) mode routes through 512 single-bit elements directly
// to the analog output stage, bypassing the digital interpolation filter.
// Integrated headphone amplifier controlled via CS43130_REG_HP_CTL (0x0032).
// I2S input slave to ESP32-P4 I2S master.
// Expansion I2S TX lifecycle managed via _enableI2sTx() / _disableI2sTx().
// Default I2C bus: Bus 2 (expansion), GPIO28 SDA / GPIO29 SCL.
// Compatible string: "cirrus,cs43130"
//
// CS43130 is the most feature-rich Cirrus Logic MasterHIFI DAC:
//   - Chip ID: 0x96
//   - HAL_CAP_HP_AMP (integrated headphone amplifier, like CS43131)
//   - NOS filter mode with 512 single-bit elements (like CS4399)
//   - HAL_CAP_DSD: DSD native playback support
//   - 5 digital filter presets (0-4), preset 4 = NOS mode
//   - Output: 2 VRMS line + headphone amp outputs
//   - 130dBA dynamic range

#include "hal_cirrus_dac_base.h"

class HalCs43130 : public HalCirrusDacBase {
public:
    HalCs43130();
    virtual ~HalCs43130() {}

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

    // Filter preset (override base; CS43130 has 5 presets, 0-4)
    bool setFilterPreset(uint8_t preset) override;

    // CS43130-specific: headphone amplifier + NOS filter
    bool setHeadphoneAmpEnabled(bool enable);
    bool isHeadphoneAmpEnabled() const;
    bool setNosMode(bool enable);
    bool isNosMode() const;

private:
    static const uint32_t _kSupportedRates[5];
    static const uint8_t  _kRateCount = 5;

    bool _hpAmpEnabled = false;
    bool _nosEnabled   = false;
};

#endif // DAC_ENABLED
