#ifndef DAC_PCM5102_H
#define DAC_PCM5102_H

#ifdef DAC_ENABLED

#include "../dac_hal.h"

class DacPcm5102 : public DacDriver {
public:
    const DacCapabilities& getCapabilities() const override;
    bool init(const DacPinConfig& pins) override;
    void deinit() override;
    bool configure(uint32_t sampleRate, uint8_t bitDepth) override;
    bool setVolume(uint8_t volume) override;
    bool setMute(bool mute) override;
    bool isReady() const override;

private:
    bool _initialized = false;
    bool _configured = false;
    uint32_t _sampleRate = 0;
    uint8_t _bitDepth = 0;
};

// Factory function for registry
DacDriver* createDacPcm5102();

#endif // DAC_ENABLED
#endif // DAC_PCM5102_H
