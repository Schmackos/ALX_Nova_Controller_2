#ifndef DAC_ES8311_H
#define DAC_ES8311_H

#ifdef DAC_ENABLED
#ifndef NATIVE_TEST
#include <sdkconfig.h>
#endif
#if CONFIG_IDF_TARGET_ESP32P4

#include "../dac_hal.h"

class DacEs8311 : public DacDriver {
public:
    const DacCapabilities& getCapabilities() const override;
    bool init(const DacPinConfig& pins) override;
    void deinit() override;
    bool configure(uint32_t sampleRate, uint8_t bitDepth) override;
    bool setVolume(uint8_t volume) override;  // Hardware volume via I2C register 0x32
    bool setMute(bool mute) override;         // Hardware mute via I2C + PA control
    bool isReady() const override;

private:
    bool _initialized = false;
    bool _configured = false;
    uint32_t _sampleRate = 0;
    uint8_t _bitDepth = 0;
    bool _muted = false;

    // I2C helpers
    bool writeReg(uint8_t reg, uint8_t val);
    uint8_t readReg(uint8_t reg);
    bool verifyChipId();
    void initClocks(uint32_t sampleRate);
    void powerUp();
    void powerDown();
};

// Factory function for registry
DacDriver* createDacEs8311();

#endif // CONFIG_IDF_TARGET_ESP32P4
#endif // DAC_ENABLED
#endif // DAC_ES8311_H
