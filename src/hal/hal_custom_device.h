#pragma once
#ifdef DAC_ENABLED
// HAL Custom Device — loads user-defined device schemas from LittleFS
// Schema format: /hal/custom/<compatible_name>.json
//
// Schema JSON example:
// {
//   "compatible": "vendor,model",
//   "name": "My Custom DAC",
//   "bus": "i2s",
//   "capabilities": ["volume_control", "mute", "dac_path"],
//   "defaults": { "sample_rate": 48000, "bits_per_sample": 16, "mclk_multiple": 256 }
// }

#include "hal_audio_device.h"

// Load all custom device schemas from LittleFS /hal/custom/ directory
// and register them with HalDeviceManager.
// Safe to call multiple times — removes stale custom (HAL_DISC_MANUAL) devices first.
void hal_load_custom_devices();

class HalCustomDevice : public HalAudioDevice {
public:
    HalCustomDevice(const char* compatible, const char* name,
                    uint8_t caps, uint8_t busType);
    virtual ~HalCustomDevice() {}

    bool probe() override;
    HalInitResult init() override;
    void deinit() override;
    void dumpConfig() override;
    bool healthCheck() override;
    bool configure(uint32_t sampleRate, uint8_t bitDepth) override;
    bool setVolume(uint8_t percent) override;
    bool setMute(bool mute) override;

};

#endif // DAC_ENABLED
