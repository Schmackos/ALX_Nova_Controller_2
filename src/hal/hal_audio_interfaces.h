#pragma once
// HAL Audio Component Interfaces — ESPHome-style component/platform layering
// Phase 1-3: New file — no existing files modified

#include <stdint.h>

// AudioDac component — any device that produces audio output
class HalAudioDacInterface {
public:
    virtual ~HalAudioDacInterface() {}
    virtual bool dacSetVolume(uint8_t percent) = 0;   // 0-100
    virtual bool dacSetMute(bool muted) = 0;
    virtual uint8_t dacGetVolume() const = 0;
    virtual bool dacIsMuted() const = 0;
    virtual bool dacSetSampleRate(uint32_t hz) = 0;
    virtual bool dacSetBitDepth(uint8_t bits) = 0;
    virtual uint32_t dacGetSampleRate() const = 0;
};

// AudioAdc component — any device that captures audio input
class HalAudioAdcInterface {
public:
    virtual ~HalAudioAdcInterface() {}
    virtual bool adcSetGain(uint8_t gainDb) = 0;      // 0-23 dB
    virtual bool adcSetHpfEnabled(bool enabled) = 0;
    virtual bool adcSetSampleRate(uint32_t hz) = 0;
    virtual uint32_t adcGetSampleRate() const = 0;
};

// AudioCodec — combined DAC + ADC (e.g. ES8311)
class HalAudioCodecInterface : public HalAudioDacInterface,
                                public HalAudioAdcInterface {
public:
    virtual bool codecSetMclkMultiple(uint16_t mult) = 0;  // 256, 384, 512
    virtual bool codecSetI2sFormat(uint8_t fmt) = 0;       // 0=Philips, 1=MSB, 2=LSB
    virtual bool codecSetPaEnabled(bool en) = 0;
};

// AudioDsp — bridge to the dsp_pipeline module
class HalAudioDspInterface {
public:
    virtual ~HalAudioDspInterface() {}
    virtual bool dspIsActive() const = 0;
    virtual bool dspSetBypassed(bool bypass) = 0;
    virtual float dspGetInputLevel(uint8_t lane) const = 0;
    virtual float dspGetOutputLevel(uint8_t lane) const = 0;
};
