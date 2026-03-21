#pragma once
#ifdef DAC_ENABLED
// HalEs9843pro — ESS ES9843PRO 4-channel 32-bit ADC (HAL_DEV_ADC)
// Implements: HalAudioDevice + HalAudioAdcInterface
// I2C-controlled: sample rate, PGA gain (0–42 dB in 6 dB steps), HPF per-channel,
// global digital filter preset (0-7), per-channel 8-bit volume. I2S output slave
// to ESP32-P4 I2S master.
// Default I2C bus: Bus 2 (expansion), GPIO28 SDA / GPIO29 SCL.
// Compatible string: "ess,es9843pro"
//
// TDM deinterleaver
// -----------------
// The ES9843PRO outputs all 4 channels on a single I2S DATA line in TDM mode:
//   [SLOT0=CH1][SLOT1=CH2][SLOT2=CH3][SLOT3=CH4] per frame, 32-bit per slot.
//
// This driver exposes TWO AudioInputSource entries to the bridge:
//   source index 0 → "ES9843PRO CH1/2"  (SLOT0 + SLOT1)
//   source index 1 → "ES9843PRO CH3/4"  (SLOT2 + SLOT3)
//
// The bridge discovers the dual-source nature via getInputSourceCount() /
// getInputSourceAt(idx) and registers each at its own pipeline lane.
// getInputSource() returns the first source for backward compatibility with
// existing bridge code that calls it unconditionally.

#include "hal_audio_device.h"
#include "hal_audio_interfaces.h"
#include "hal_types.h"
#include "hal_tdm_deinterleaver.h"
#include "../audio_input_source.h"
#ifndef NATIVE_TEST
class TwoWire;  // Forward declaration — defined in Wire.h (Arduino)
#endif

class HalEs9843pro : public HalAudioDevice, public HalAudioAdcInterface {
public:
    HalEs9843pro();
    virtual ~HalEs9843pro() {}

    // HalDevice lifecycle
    bool          probe()       override;
    HalInitResult init()        override;
    void          deinit()      override;
    void          dumpConfig()  override;
    bool          healthCheck() override;

    // HalAudioDevice
    bool configure(uint32_t sampleRate, uint8_t bitDepth) override;
    bool setVolume(uint8_t percent) override;
    bool setMute(bool mute) override;

    // HalAudioAdcInterface
    bool     adcSetGain(uint8_t gainDb)    override;
    bool     adcSetHpfEnabled(bool en)     override;
    bool     adcSetSampleRate(uint32_t hz) override;
    uint32_t adcGetSampleRate() const      override { return _sampleRate; }

    // Multi-source ADC interface — bridge queries count then each source by index.
    // Returns 2 when initialized (CH1/2 and CH3/4 stereo pairs); 0 otherwise.
    // Both sources are valid after init() regardless of whether a DMA read has occurred.
    int getInputSourceCount() const override { return _initialized ? 2 : 0; }
    const AudioInputSource* getInputSourceAt(int idx) const override;

    // Backward-compat single-source accessor (returns source 0).
    const AudioInputSource* getInputSource() const override {
        return getInputSourceAt(0);
    }

    // ES9843PRO-specific extensions
    bool setFilterPreset(uint8_t preset);                      // 0-7 global filter shapes
    bool setChannelVolume(uint8_t ch, uint8_t vol8);           // per-channel 8-bit volume

private:
    bool    _writeReg(uint8_t reg, uint8_t val);
    uint8_t _readReg(uint8_t reg);

    uint8_t  _i2cAddr      = 0x40;   // ES9843PRO default (ADDR1=LOW, ADDR2=LOW)
    int8_t   _sdaPin       = 28;     // Bus 2 expansion SDA
    int8_t   _sclPin       = 29;     // Bus 2 expansion SCL
    uint8_t  _i2cBusIndex  = 2;      // HAL_I2C_BUS_EXP
    uint32_t _sampleRate   = 48000;
    uint8_t  _bitDepth     = 32;
    uint8_t  _gainDb       = 0;      // 0–42 dB (maps to gain register 0-7)
    uint8_t  _filterPreset = 0;      // 0-7 global filter shape
    bool     _hpfEnabled   = true;
    bool     _initialized  = false;

#ifndef NATIVE_TEST
    TwoWire* _wire = nullptr;
#endif

    // TDM deinterleaver — owns the ping-pong DMA split and both AudioInputSource structs
    HalTdmDeinterleaver _tdm;

    // Source structs populated by _tdm.buildSources() during init()
    AudioInputSource _srcA = {};   // CH1/CH2 — registered at pipeline lane N
    AudioInputSource _srcB = {};   // CH3/CH4 — registered at pipeline lane N+1

    // Human-readable names (stored here so pointers in AudioInputSource stay valid)
    static constexpr const char* _NAME_A = "ES9843PRO CH1/2";
    static constexpr const char* _NAME_B = "ES9843PRO CH3/4";
};

#endif // DAC_ENABLED
