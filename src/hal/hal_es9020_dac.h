#pragma once
#ifdef DAC_ENABLED
// HalEs9020Dac — ESS ES9020 2-channel 32-bit DAC (HAL_DEV_DAC)
// Implements: HalEssSabreDacBase (HalAudioDevice + HalAudioDacInterface)
// I2C-controlled: attenuation volume (0.5 dB/step, 8-bit), mute, 8 digital filter presets,
// integrated APLL for clock recovery from BCK or external MCLK.
// I2S input slave — ESP32-P4 drives BCK/WS/MCLK as master, ES9020 receives.
// Default I2C bus: Bus 2 (expansion), GPIO28 SDA / GPIO29 SCL.
// Default I2C address: 0x48 (ESS_SABRE_DAC_I2C_ADDR_BASE, ADDR1=LOW, ADDR2=LOW).
// Compatible string: "ess,es9020-dac"
//
// ES9020 highlights:
//   - 122 dB DNR, Hyperstream IV architecture
//   - Integrated analog PLL (APLL) for jitter-immune clock recovery from BCK or DSD clock
//   - Flexible TDM support (2/4/8/16 slots via REG_INPUT_CONFIG bits[5:4])
//   - 28-pin QFN package
//
// APLL usage:
//   - setApllEnabled(true)  — enables APLL; call after init() if jitter reduction is wanted
//   - isApllLocked()        — polls APLL_LOCK_STATUS bit; hardware-locked within ~1ms of enable
//   - When APLL is enabled, clock source is set to BCK recovery (REG_CLK_SOURCE = 0x00)
//   - When APLL is disabled, clock source reverts to external MCLK (REG_CLK_SOURCE = 0x02)

#include "hal_ess_sabre_dac_base.h"

class HalEs9020Dac : public HalEssSabreDacBase {
public:
    HalEs9020Dac();
    virtual ~HalEs9020Dac() {}

    // HalDevice lifecycle
    bool          probe()       override;
    HalInitResult init()        override;
    void          deinit()      override;
    void          dumpConfig()  override;
    bool          healthCheck() override;

    // HalAudioDevice
    bool configure(uint32_t sampleRate, uint8_t bitDepth) override;
    bool setVolume(uint8_t percent)                        override;
    bool setMute(bool mute)                                override;

    // Filter preset override (ES9020 stores FILTER_SHAPE in REG_FILTER bits[2:0])
    bool setFilterPreset(uint8_t preset) override;

    // ES9020-specific: APLL clock recovery
    bool setApllEnabled(bool enable);   // Enable/disable integrated APLL
    bool isApllLocked() const;          // True when APLL has achieved lock
};

#endif // DAC_ENABLED
