#pragma once
// HAL Audio Device -- extends HalDevice with audio-specific methods

#include "hal_device.h"
#include "../audio_output_sink.h"

class HalAudioDevice : public HalDevice {
public:
    virtual ~HalAudioDevice() {}

    // Audio-specific configuration
    virtual bool configure(uint32_t sampleRate, uint8_t bitDepth) = 0;
    virtual bool setVolume(uint8_t percent) = 0;   // 0-100
    virtual bool setMute(bool mute) = 0;
    virtual bool setFilterMode(uint8_t mode) { (void)mode; return false; }

    virtual bool setDsdMode(bool enable) { (void)enable; return false; }
    virtual bool isDsdMode() const { return false; }

    // Any device with HAL_CAP_DAC_PATH should override this.
    // Populates an AudioOutputSink with device-specific write/isReady callbacks.
    // Returns true if sink was populated successfully.
    // Overrides HalDevice::buildSink() base method.
    bool buildSink(uint8_t sinkSlot, AudioOutputSink* out) override {
        (void)sinkSlot; (void)out;
        return false;
    }

    // Query whether this device has hardware volume control
    virtual bool hasHardwareVolume() const { return false; }

protected:
    HalAudioDevice() : HalDevice() {}
};
