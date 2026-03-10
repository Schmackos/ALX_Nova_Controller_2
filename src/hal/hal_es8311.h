#pragma once
#ifdef DAC_ENABLED
// HalEs8311 — ES8311 codec platform class (HAL_DEV_CODEC)
// Implements: HalAudioDevice + HalAudioCodecInterface
// Phase 1-3: New file — no existing files modified

#include "hal_audio_device.h"
#include "hal_audio_interfaces.h"
#include "hal_types.h"

class HalEs8311 : public HalAudioDevice, public HalAudioCodecInterface {
public:
    HalEs8311();
    virtual ~HalEs8311() {}

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

    // HalAudioAdcInterface
    bool adcSetGain(uint8_t gainDb) override;
    bool adcSetHpfEnabled(bool en) override;
    bool adcSetSampleRate(uint32_t hz) override { return dacSetSampleRate(hz); }
    uint32_t adcGetSampleRate() const override { return _sampleRate; }

    // HalAudioCodecInterface
    bool codecSetMclkMultiple(uint16_t mult) override;
    bool codecSetI2sFormat(uint8_t fmt) override;
    bool codecSetPaEnabled(bool en) override;

private:
    bool _writeReg(uint8_t reg, uint8_t val);
    uint8_t _readReg(uint8_t reg);
    void _initClocks(uint32_t sampleRate);
    void _powerUp();
    void _powerDown();

    uint8_t  _volume     = 0;
    bool     _muted      = false;
    uint32_t _sampleRate = 48000;
    uint8_t  _bitDepth   = 16;
    uint16_t _mclkMult   = 256;
    uint8_t  _i2sFormat  = 0;   // 0=Philips
    uint8_t  _i2cAddr    = 0x18;
    int8_t   _sdaPin     = 7;
    int8_t   _sclPin     = 8;
    int8_t   _paPin      = 53;
    bool     _initialized = false;
};
#endif // DAC_ENABLED
