#pragma once
// HAL Audio Device — extends HalDevice with audio-specific methods
// Phase 0: Purely additive

#include "hal_device.h"
#include "../audio_output_sink.h"

#ifdef DAC_ENABLED
#include "../dac_hal.h"
#endif

class HalAudioDevice : public HalDevice {
public:
    virtual ~HalAudioDevice() {}

    // Audio-specific configuration
    virtual bool configure(uint32_t sampleRate, uint8_t bitDepth) = 0;
    virtual bool setVolume(uint8_t percent) = 0;   // 0-100
    virtual bool setMute(bool mute) = 0;
    virtual bool setFilterMode(uint8_t mode) { (void)mode; return false; }

    // Any device with HAL_CAP_DAC_PATH should override this.
    // Populates an AudioOutputSink with device-specific write/isReady callbacks.
    // Returns true if sink was populated successfully.
    virtual bool buildSink(uint8_t sinkSlot, AudioOutputSink* out) {
        (void)sinkSlot; (void)out;
        return false;
    }

#ifdef DAC_ENABLED
    // Bridge to legacy DacCapabilities for backward compat
    virtual const DacCapabilities* getLegacyCapabilities() const { return nullptr; }
#endif

protected:
    HalAudioDevice() : HalDevice() {}
};
