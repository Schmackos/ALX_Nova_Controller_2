#pragma once
#ifdef DAC_ENABLED
// HalSigGen — Signal Generator HAL device driver
// Wraps signal_generator.cpp as a HAL-managed audio input source.

#include "hal_device.h"
#include "../audio_input_source.h"

class HalSigGen : public HalDevice {
public:
    HalSigGen();
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
