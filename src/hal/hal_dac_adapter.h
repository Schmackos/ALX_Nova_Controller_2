#pragma once
// HAL DAC Adapter — wraps existing DacDriver as HalAudioDevice
// Phase 1: Bridges legacy DAC interface to HAL lifecycle

#ifdef DAC_ENABLED

#include "hal_audio_device.h"
#include "../dac_hal.h"

class HalDacAdapter : public HalAudioDevice {
public:
    // Construct with an existing DacDriver and its capabilities
    HalDacAdapter(DacDriver* driver, const DacCapabilities& caps,
                  uint16_t priority = HAL_PRIORITY_HARDWARE);
    ~HalDacAdapter() override {}

    // ----- HalDevice lifecycle -----
    bool probe() override;
    bool init() override;
    void deinit() override;
    void dumpConfig() override;
    bool healthCheck() override;

    // ----- HalAudioDevice methods -----
    bool configure(uint32_t sampleRate, uint8_t bitDepth) override;
    bool setVolume(uint8_t percent) override;
    bool setMute(bool mute) override;
    bool setFilterMode(uint8_t mode) override;

    const DacCapabilities* getLegacyCapabilities() const override { return &_caps; }

    // Access the wrapped driver (for legacy code paths)
    DacDriver* getDriver() { return _driver; }

private:
    DacDriver*      _driver;
    DacCapabilities _caps;
};

#endif // DAC_ENABLED
