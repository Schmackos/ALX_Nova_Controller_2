#pragma once
#ifdef DAC_ENABLED
// HalEs9033Q — ESS ES9033Q 2-channel 32-bit DAC with integrated 2Vrms line drivers (HAL_DEV_DAC)
// Implements: HalEssSabreDacBase (HalAudioDevice + HalAudioDacInterface)
// I2C-controlled: attenuation volume (0.5dB steps), 8 filter presets,
// integrated ground-centered line output drivers (no external op-amp required).
// Expansion I2S TX lifecycle managed via _enableI2sTx() / _disableI2sTx().
// Default I2C bus: Bus 2 (expansion), GPIO28 SDA / GPIO29 SCL.
// Compatible string: "ess,es9033q"
//
// Device-specific features:
//   - Integrated 2Vrms ground-centered line-level output stage (28-pin QFN)
//   - Eliminates external op-amp output buffers for compact designs
//   - HAL_CAP_LINE_DRIVER capability flag set
//   - Volume: 0.5dB/step attenuation via I2C (0x00=0dB, 0xFF=mute)
//   - Sample rates: 44.1K, 48K, 96K, 192K, 384K, 768K Hz

#include "hal_ess_sabre_dac_base.h"

class HalEs9033Q : public HalEssSabreDacBase {
public:
    HalEs9033Q();
    virtual ~HalEs9033Q() {}

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

    // Filter preset (override base; ES9033Q stores preset in bits[2:0] of REG_FILTER_SHAPE)
    bool setFilterPreset(uint8_t preset) override;

    // ES9033Q-specific: integrated line driver control
    bool setLineDriverEnabled(bool enable);  // Enable/disable on-chip 2Vrms line output stage

private:
    bool _lineDriverEnabled = true;  // Line drivers enabled by default

    static const uint32_t _kSupportedRates[6];
    static const uint8_t  _kRateCount = 6;
};

#endif // DAC_ENABLED
