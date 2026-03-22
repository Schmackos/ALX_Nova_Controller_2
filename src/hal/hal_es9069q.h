#pragma once
#ifdef DAC_ENABLED
// HalEs9069Q — ESS ES9069Q 2-channel 32-bit DAC with integrated MQA hardware renderer (HAL_DEV_DAC)
// Implements: HalEssSabreDacBase (HalAudioDevice + HalAudioDacInterface)
// I2C-controlled: attenuation volume (0.5dB steps), 8 filter presets,
// per-channel hardware volume. I2S input slave to ESP32-P4 I2S master.
// Expansion I2S TX lifecycle managed via _enableI2sTx() / _disableI2sTx().
// Default I2C bus: Bus 2 (expansion), GPIO28 SDA / GPIO29 SCL.
// Compatible string: "ess,es9069q"
//
// Device-specific features vs ES9039Q2M:
//   - Integrated MQA hardware renderer (PCM → MQA unfold in silicon)
//   - DSD1024 support (vs DSD512 on ES9039Q2M)
//   - MQA control register at 0x17 (enable + status)
//   - HAL_CAP_MQA capability flag set

#include "hal_ess_sabre_dac_base.h"

class HalEs9069Q : public HalEssSabreDacBase {
public:
    HalEs9069Q();
    virtual ~HalEs9069Q() {}

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

    // Filter preset (override base; ES9069Q stores preset in bits[2:0] of REG_FILTER_SHAPE)
    bool setFilterPreset(uint8_t preset) override;

    // ES9069Q-specific: MQA hardware renderer control
    bool setMqaEnabled(bool enable);    // Enable/disable integrated MQA hardware renderer
    bool isMqaActive() const;           // True when MQA decode status != NONE (silicon is rendering)

private:
    bool _mqaEnabled = false;   // Tracks requested MQA enable state

    static const uint32_t _kSupportedRates[6];
    static const uint8_t  _kRateCount = 6;
};

#endif // DAC_ENABLED
