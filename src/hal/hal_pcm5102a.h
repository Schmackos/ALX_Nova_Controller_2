#pragma once
#ifdef DAC_ENABLED
// HalPcm5102a — PCM5102A passive I2S DAC (HAL_DEV_DAC)
// No I2C control — passive device. probe() always returns true.
// Volume/mute via XSMT pin if paControlPin configured.
// Phase 1-3: New file — no existing files modified

#include "hal_audio_device.h"
#include "hal_audio_interfaces.h"
#include "hal_i2s_bridge.h"
#include "hal_types.h"

class HalPcm5102a : public HalAudioDevice, public HalAudioDacInterface {
public:
    HalPcm5102a();
    virtual ~HalPcm5102a() {}

    // HalDevice lifecycle
    bool probe() override;
    HalInitResult init() override;
    void deinit() override;
    void dumpConfig() override;
    bool healthCheck() override;

    // HalAudioDevice
    bool configure(uint32_t sampleRate, uint8_t bitDepth) override;
    bool setVolume(uint8_t percent) override;
    bool setMute(bool mute) override;
    bool buildSink(uint8_t sinkSlot, AudioOutputSink* out) override;

    // HalAudioDacInterface
    bool dacSetVolume(uint8_t pct) override { return setVolume(pct); }
    bool dacSetMute(bool m) override { return setMute(m); }
    uint8_t dacGetVolume() const override { return _volume; }
    bool dacIsMuted() const override { return _muted; }
    bool dacSetSampleRate(uint32_t hz) override;
    bool dacSetBitDepth(uint8_t bits) override;
    uint32_t dacGetSampleRate() const override { return _sampleRate; }

private:
    uint8_t  _volume     = 100;
    bool     _muted      = false;
    uint32_t _sampleRate = 48000;
    uint8_t  _bitDepth   = 32;
    int8_t   _paPin      = -1;  // XSMT / mute pin (-1 = not used)
    void*    _txHandle   = nullptr;
};
#endif // DAC_ENABLED
