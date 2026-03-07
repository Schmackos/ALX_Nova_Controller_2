#pragma once
#ifdef DAC_ENABLED
// HalDspBridge — HAL_DEV_DSP builtin that bridges to dsp_pipeline
// Phase 1-3: New file — no existing files modified

#include "hal_device.h"
#include "hal_audio_interfaces.h"
#include "hal_types.h"

class HalDspBridge : public HalDevice, public HalAudioDspInterface {
public:
    HalDspBridge();
    virtual ~HalDspBridge() {}

    // HalDevice lifecycle
    bool probe() override;
    HalInitResult init() override;
    void deinit() override;
    void dumpConfig() override;
    bool healthCheck() override;

    // HalAudioDspInterface
    bool dspIsActive() const override;
    bool dspSetBypassed(bool bypass) override;
    float dspGetInputLevel(uint8_t lane) const override;
    float dspGetOutputLevel(uint8_t lane) const override;
};
#endif // DAC_ENABLED
