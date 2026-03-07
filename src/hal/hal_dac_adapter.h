#pragma once
// HAL DAC Adapter — wraps existing DacDriver as a HalAudioDevice
// Phase 4: Bridges legacy DAC interface to HAL lifecycle

#ifdef DAC_ENABLED

#include "hal_audio_device.h"
#include "../dac_hal.h"

class HalDacAdapter : public HalAudioDevice {
public:
    // Takes ownership of an existing DacDriver* (already init'd or not)
    // If alreadyInitialized is true, probe()/init() become no-ops and state is set to AVAILABLE
    HalDacAdapter(DacDriver* driver, const HalDeviceDescriptor& desc, bool alreadyInitialized = false);
    virtual ~HalDacAdapter();

    // HalDevice lifecycle
    bool probe() override;
    HalInitResult init() override;
    void deinit() override;
    void dumpConfig() override;
    bool healthCheck() override;

    // HalAudioDevice methods
    bool configure(uint32_t sampleRate, uint8_t bitDepth) override;
    bool setVolume(uint8_t percent) override;
    bool setMute(bool mute) override;
    bool setFilterMode(uint8_t mode) override;

    // Legacy bridge
    const DacCapabilities* getLegacyCapabilities() const override;

    // Direct access to wrapped driver (for dac_hal.cpp interop)
    DacDriver* getDriver() const { return _driver; }

private:
    DacDriver* _driver;
    bool _ownsDriver;  // If true, destructor deletes _driver
};

#endif // DAC_ENABLED
