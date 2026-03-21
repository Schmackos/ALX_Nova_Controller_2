#pragma once
#ifdef DAC_ENABLED
// HalEs9822pro — ESS ES9822PRO 2-channel 32-bit ADC (HAL_DEV_ADC)
// Implements: HalAudioDevice + HalAudioAdcInterface
// I2C-controlled: sample rate, PGA gain (0/6/12/18 dB), HPF, digital filter presets,
// per-channel 16-bit volume. I2S output slave to ESP32-P4 I2S master.
// Default I2C bus: Bus 2 (expansion), GPIO28 SDA / GPIO29 SCL.
// Compatible string: "ess,es9822pro"

#include "hal_audio_device.h"
#include "hal_audio_interfaces.h"
#include "hal_types.h"
#include "../audio_input_source.h"

class HalEs9822pro : public HalAudioDevice, public HalAudioAdcInterface {
public:
    HalEs9822pro();
    virtual ~HalEs9822pro() {}

    // HalDevice lifecycle
    bool         probe()       override;
    HalInitResult init()       override;
    void         deinit()      override;
    void         dumpConfig()  override;
    bool         healthCheck() override;

    // HalAudioDevice
    bool configure(uint32_t sampleRate, uint8_t bitDepth) override;
    bool setVolume(uint8_t percent) override;
    bool setMute(bool mute) override;

    // HalAudioAdcInterface
    bool     adcSetGain(uint8_t gainDb)       override;
    bool     adcSetHpfEnabled(bool en)        override;
    bool     adcSetSampleRate(uint32_t hz)    override;
    uint32_t adcGetSampleRate() const         override { return _sampleRate; }

    // ADC input source (bridge-registered into audio pipeline)
    const AudioInputSource* getInputSource() const override;

    // ES9822PRO-specific extensions
    bool setFilterPreset(uint8_t preset);                         // 0-7 filter shapes
    bool setChannelVolume(uint8_t channel, uint16_t vol16);       // per-channel 16-bit volume

private:
    bool    _writeReg(uint8_t reg, uint8_t val);
    bool    _writeReg16(uint8_t regLsb, uint16_t val);           // LSB first, MSB latches
    uint8_t _readReg(uint8_t reg);

    uint8_t  _i2cAddr      = 0x40;   // ES9822PRO default (ADDR1=LOW, ADDR2=LOW)
    int8_t   _sdaPin       = 28;     // Bus 2 expansion SDA
    int8_t   _sclPin       = 29;     // Bus 2 expansion SCL
    uint8_t  _i2cBusIndex  = 2;      // HAL_I2C_BUS_EXP
    uint32_t _sampleRate   = 48000;
    uint8_t  _bitDepth     = 32;
    uint8_t  _gainDb       = 0;      // 0/6/12/18 dB (maps to gain register 0-3)
    uint8_t  _filterPreset = 0;      // 0-7
    bool     _hpfEnabled   = true;
    bool     _initialized  = false;

#ifndef NATIVE_TEST
    TwoWire* _wire = nullptr;
#endif

    AudioInputSource _inputSrc      = {};
    bool             _inputSrcReady = false;
};

#endif // DAC_ENABLED
