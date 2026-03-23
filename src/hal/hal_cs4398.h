#pragma once
#ifdef DAC_ENABLED
// HalCs4398 — Cirrus Logic CS4398 2-channel 24-bit CLASSIC DAC (HAL_DEV_DAC)
// Implements: HalCirrusDacBase (HalAudioDevice + HalAudioDacInterface)
// I2C-controlled via simple 8-bit register addressing: per-channel attenuation
// volume (0.5dB steps), 3 digital filter presets, mute, DSD64 support.
// I2S input slave to ESP32-P4 I2S master.
// Expansion I2S TX lifecycle managed via _enableI2sTx() / _disableI2sTx().
// Default I2C bus: Bus 2 (expansion), GPIO28 SDA / GPIO29 SCL.
// Compatible string: "cirrus,cs4398"
//
// Key differences from CS43198 / CS43131:
//   - 8-bit register addressing (_writeReg8 / _readReg8, NOT _writeRegPaged)
//   - Only 3 digital filter presets (0-2), not 7
//   - Maximum sample rate 192kHz (no 384kHz or 768kHz)
//   - Maximum bit depth 24-bit (no 32-bit)
//   - DSD64 support (HAL_CAP_DSD flag set); no DSD256
//   - Default I2C address 0x4C (CS4398) vs 0x48 (CS43198/CS43131)
//   - Dynamic range: 120dB (vs 130dBA / 127dB for newer Cirrus family)
//   - Legacy register layout: separate mute register (0x04), not embedded
//     in the PCM path register as on the CS43198

#include "hal_cirrus_dac_base.h"

class HalCs4398 : public HalCirrusDacBase {
public:
    HalCs4398();
    virtual ~HalCs4398() {}

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

    // Filter preset (override base; CS4398 has only 3 presets, 0-2)
    bool setFilterPreset(uint8_t preset) override;

private:
    static const uint32_t _kSupportedRates[4];
    static const uint8_t  _kRateCount = 4;
};

#endif // DAC_ENABLED
