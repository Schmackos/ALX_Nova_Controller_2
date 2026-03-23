#pragma once
#ifdef DAC_ENABLED
// HalCs4399 — Cirrus Logic CS4399 2-channel 32-bit MasterHIFI DAC (HAL_DEV_DAC)
// Implements: HalCirrusDacBase (HalAudioDevice + HalAudioDacInterface)
// I2C-controlled via 16-bit paged register addressing: per-channel attenuation
// volume (0.5dB steps), 5 digital filter presets (including NOS mode), mute.
// NOS (Non-OverSampling) mode routes through 512 single-bit elements directly
// to the analog output stage, bypassing the digital interpolation filter.
// I2S input slave to ESP32-P4 I2S master.
// Expansion I2S TX lifecycle managed via _enableI2sTx() / _disableI2sTx().
// Default I2C bus: Bus 2 (expansion), GPIO28 SDA / GPIO29 SCL.
// Compatible string: "cirrus,cs4399"
//
// Key differences from CS43198:
//   - Chip ID: 0x97 (CS43198 is 0x98)
//   - 5 digital filter presets (0-4), not 7
//   - NOS filter mode with 512 single-bit elements (CS4399_REG_NOS_CTL, 0x001C)
//   - setNosMode(bool) / isNosMode() device-specific API
//   - 130dBA dynamic range (same as CS43198)
//   - No DSD support (CS43198 has HAL_CAP_DSD)

#include "hal_cirrus_dac_base.h"

class HalCs4399 : public HalCirrusDacBase {
public:
    HalCs4399();
    virtual ~HalCs4399() {}

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

    // Filter preset (override base; CS4399 has 5 presets, 0-4)
    bool setFilterPreset(uint8_t preset) override;

    // CS4399-specific: NOS filter mode with 512 single-bit elements
    bool setNosMode(bool enable);   // Enable/disable 512-element NOS filter engine
    bool isNosMode() const;         // True when NOS register bit is set

private:
    bool _nosEnabled = false;

    static const uint32_t _kSupportedRates[5];
    static const uint8_t  _kRateCount = 5;
};

#endif // DAC_ENABLED
