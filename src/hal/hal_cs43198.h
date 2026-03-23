#pragma once
#ifdef DAC_ENABLED
// HalCs43198 — Cirrus Logic CS43198 2-channel 32-bit MasterHIFI DAC (HAL_DEV_DAC)
// Implements: HalCirrusDacBase (HalAudioDevice + HalAudioDacInterface)
// I2C-controlled via 16-bit paged register addressing: per-channel attenuation
// volume (0.5dB steps), 7 digital filter presets, mute, DSD support.
// I2S input slave to ESP32-P4 I2S master.
// Expansion I2S TX lifecycle managed via _enableI2sTx() / _disableI2sTx().
// Default I2C bus: Bus 2 (expansion), GPIO28 SDA / GPIO29 SCL.
// Compatible string: "cirrus,cs43198"
//
// Key differences from ESS SABRE DACs:
//   - 16-bit paged register addressing (_writeRegPaged / _readRegPaged)
//   - 7 digital filter presets (0-6), not 8
//   - Maximum sample rate 384kHz (no 768kHz)
//   - Mute via read-modify-write on PCM path register (not attenuation register)
//   - HAL_CAP_DSD capability flag set (DSD256 support)
//   - No MQA, no line driver, no APLL

#include "hal_cirrus_dac_base.h"

class HalCs43198 : public HalCirrusDacBase {
public:
    HalCs43198();
    virtual ~HalCs43198() {}

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

    // Filter preset (override base; CS43198 has 7 presets, 0-6)
    bool setFilterPreset(uint8_t preset) override;

private:
    static const uint32_t _kSupportedRates[5];
    static const uint8_t  _kRateCount = 5;
};

#endif // DAC_ENABLED
