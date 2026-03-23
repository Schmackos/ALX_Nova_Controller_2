#pragma once
#ifdef DAC_ENABLED
// HalCs43131 — Cirrus Logic CS43131 2-channel 32-bit MasterHIFI DAC + Headphone Amp (HAL_DEV_DAC)
// Implements: HalCirrusDacBase (HalAudioDevice + HalAudioDacInterface)
// I2C-controlled via 16-bit paged register addressing: per-channel attenuation
// volume (0.5dB steps), 7 digital filter presets, mute, DSD support.
// Integrated headphone amplifier controlled via CS43131_REG_HP_CTL (0x0032).
// I2S input slave to ESP32-P4 I2S master.
// Expansion I2S TX lifecycle managed via _enableI2sTx() / _disableI2sTx().
// Default I2C bus: Bus 2 (expansion), GPIO28 SDA / GPIO29 SCL.
// Compatible string: "cirrus,cs43131"
//
// Key differences from CS43198:
//   - Chip ID: 0x99 (CS43198 is 0x98)
//   - HAL_CAP_HP_AMP capability flag set (integrated headphone amplifier)
//   - Headphone amp control register at 0x0032 (setHeadphoneAmpEnabled / isHeadphoneAmpEnabled)
//   - Dynamic range: 127dB (CS43198 is 130dBA)
//   - Output: 2 VRMS into 600Ω headphones

#include "hal_cirrus_dac_base.h"

class HalCs43131 : public HalCirrusDacBase {
public:
    HalCs43131();
    virtual ~HalCs43131() {}

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

    // Filter preset (override base; CS43131 has 7 presets, 0-6)
    bool setFilterPreset(uint8_t preset) override;

    // CS43131-specific: Integrated headphone amplifier control
    bool setHeadphoneAmpEnabled(bool enable);
    bool isHeadphoneAmpEnabled() const;

private:
    static const uint32_t _kSupportedRates[5];
    static const uint8_t  _kRateCount = 5;

    bool _hpAmpEnabled = false;
};

#endif // DAC_ENABLED
