#pragma once
#ifdef DAC_ENABLED
// HalUsbAudio — USB Audio HAL device driver
// Wraps usb_audio.cpp as a HAL-managed audio input source.

#include "hal_device.h"
#include "../audio_input_source.h"

class HalUsbAudio : public HalDevice {
public:
    HalUsbAudio();
    bool probe() override;
    HalInitResult init() override;
    void deinit() override;
    void dumpConfig() override;
    bool healthCheck() override;
    const AudioInputSource* getInputSource() const override;

private:
    AudioInputSource _source;
};
#endif // DAC_ENABLED
