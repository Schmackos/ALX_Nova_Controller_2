#pragma once
#ifdef DAC_ENABLED
// HalPcm1808 — PCM1808 ADC (HAL_DEV_ADC)
// No I2C — passive. adcSetGain/adcSetHpfEnabled are hardware stubs.
// PCM1808 uses FMT0/FMT1 GPIO pins for mode selection, not register I2C.
// Phase 1-3: New file — no existing files modified

#include "hal_audio_device.h"
#include "hal_audio_interfaces.h"
#include "hal_i2s_bridge.h"
#include "hal_types.h"

class HalPcm1808 : public HalAudioDevice, public HalAudioAdcInterface {
public:
    HalPcm1808();
    virtual ~HalPcm1808() {}

    // HalDevice lifecycle
    bool probe() override;
    HalInitResult init() override;
    void deinit() override;
    void dumpConfig() override;
    bool healthCheck() override;

    // HalAudioDevice (stubs — PCM1808 has no I2C control)
    bool configure(uint32_t sampleRate, uint8_t bitDepth) override;
    bool setVolume(uint8_t percent) override { (void)percent; return false; }
    bool setMute(bool mute) override { (void)mute; return false; }

    // HalAudioAdcInterface
    bool adcSetGain(uint8_t gainDb) override;
    bool adcSetHpfEnabled(bool en) override;
    bool adcSetSampleRate(uint32_t hz) override;
    uint32_t adcGetSampleRate() const override { return _sampleRate; }

private:
    uint32_t _sampleRate = 48000;
    uint8_t  _bitDepth   = 32;
    void*    _rxHandle   = nullptr;
};
#endif // DAC_ENABLED
