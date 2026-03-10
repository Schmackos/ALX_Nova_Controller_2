#ifndef HAL_MCP4725_H
#define HAL_MCP4725_H

#ifdef DAC_ENABLED

#include "hal_audio_device.h"

// MCP4725 — Microchip 12-bit single-channel I2C DAC
// Outputs a voltage (0–VCC) proportional to a 12-bit code (0–4095).
// This is a voltage output DAC, not an audio streaming device.
// Default I2C address: 0x60 (ADDR pin low), 0x61 (ADDR pin high).
class HalMcp4725 : public HalAudioDevice {
public:
    explicit HalMcp4725(uint8_t i2cAddr = 0x60, uint8_t busIndex = 2);

    // Lifecycle
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

    // Raw 12-bit DAC output (0–4095); clamped to 4095 if larger
    bool setVoltageCode(uint16_t code);

    uint16_t getVoltageCode() const { return _code; }

private:
    uint8_t  _i2cAddr;
    uint8_t  _busIndex;
    uint16_t _code;   // Current DAC code (0–4095)
    bool     _muted = false;
};

#endif // DAC_ENABLED
#endif // HAL_MCP4725_H
