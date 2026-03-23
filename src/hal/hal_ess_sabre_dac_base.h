#pragma once
#ifdef DAC_ENABLED

/**
 * HalEssSabreDacBase -- Abstract base class for all ESS SABRE DAC family drivers.
 *
 * Provides shared I2C helpers, config override reading, Wire selection,
 * buildSink() infrastructure, and expansion I2S TX lifecycle management.
 * Derived classes implement device-specific probe(), init(), deinit(),
 * dumpConfig(), and healthCheck().
 *
 * Adding a new ESS SABRE DAC driver:
 *   1. Create src/drivers/esXXXX_regs.h with chip ID + register map
 *   2. Create src/hal/hal_esXXXX.h inheriting HalEssSabreDacBase
 *   3. Create src/hal/hal_esXXXX.cpp implementing the 5 lifecycle methods
 *   4. Add factory + registration in hal_builtin_devices.cpp
 *   5. Add descriptor in hal_device_db.cpp
 *   6. Add test module in test/test_hal_esXXXX/
 */

#include "hal_audio_device.h"
#include "hal_audio_interfaces.h"
#include "../drivers/ess_sabre_common.h"
#include "../audio_output_sink.h"

#ifndef NATIVE_TEST
#include <Wire.h>
extern TwoWire Wire2;  // Defined in hal_ess_sabre_adc_base.cpp
#endif

class HalEssSabreDacBase : public HalAudioDevice, public HalAudioDacInterface {
public:
    virtual ~HalEssSabreDacBase() = default;

    // --- HalDevice lifecycle (pure virtual -- implemented per device) ---
    virtual bool          probe()        override = 0;
    virtual HalInitResult init()         override = 0;
    virtual void          deinit()       override = 0;
    virtual void          dumpConfig()   override = 0;
    virtual bool          healthCheck()  override = 0;

    // --- HalAudioDevice (pure virtual -- implemented per device) ---
    virtual bool configure(uint32_t sampleRate, uint8_t bitDepth) override = 0;
    virtual bool setVolume(uint8_t percent)                        override = 0;
    virtual bool setMute(bool mute)                                override = 0;

    // --- HalAudioDevice DAC-specific ---
    bool buildSink(uint8_t sinkSlot, AudioOutputSink* out) override;
    bool hasHardwareVolume() const override { return true; }

    // --- HalAudioDacInterface (delegating defaults) ---
    bool     dacSetVolume(uint8_t pct) override   { return setVolume(pct); }
    bool     dacSetMute(bool m) override          { return setMute(m); }
    uint8_t  dacGetVolume() const override        { return _volume; }
    bool     dacIsMuted() const override          { return _muted; }
    bool     dacSetSampleRate(uint32_t hz) override;
    bool     dacSetBitDepth(uint8_t bits) override;
    uint32_t dacGetSampleRate() const override    { return _sampleRate; }

    // --- Filter preset (optional per-device override) ---
    virtual bool setFilterPreset(uint8_t preset);
    uint8_t getFilterPreset() const { return _filterPreset; }

protected:
    // --- I2C helpers (implemented in .cpp, shared by all derived classes) ---
    bool    _writeReg(uint8_t reg, uint8_t val);
    uint8_t _readReg(uint8_t reg);
    bool    _writeReg16(uint8_t regLsb, uint16_t val);  // LSB write then MSB

    // --- Config override reading ---
    // Reads HalDeviceConfig overrides into member fields. Call at start of init().
    void _applyConfigOverrides();

    // --- Wire selection based on _i2cBusIndex ---
    void _selectWire();

    // --- Sample rate validation ---
    bool _validateSampleRate(uint32_t hz, const uint32_t* supported, uint8_t count);

    // --- I2S TX lifecycle (expansion DAC output) ---
    bool _enableI2sTx();      // Calls i2s_port_enable_tx() via port-generic API
    void _disableI2sTx();     // Calls i2s_port_disable_tx() via port-generic API

    // --- Common member fields ---
    uint8_t  _i2cAddr      = ESS_SABRE_DAC_I2C_ADDR_BASE;  // 0x48 default for DACs
    int8_t   _sdaPin       = ESS_SABRE_I2C_BUS2_SDA;       // GPIO 28
    int8_t   _sclPin       = ESS_SABRE_I2C_BUS2_SCL;       // GPIO 29
    uint8_t  _i2cBusIndex  = 2;    // HAL_I2C_BUS_EXP (configurable)
    uint32_t _sampleRate   = 48000;
    uint8_t  _bitDepth     = 32;
    uint8_t  _filterPreset = 0;
    uint8_t  _volume       = 100;
    bool     _muted        = false;
    bool     _initialized  = false;
    bool     _i2sTxEnabled = false;
public:
    float    _muteRampState = 1.0f;  // Mute ramp envelope [0.0 .. 1.0] (public: accessed by static write callback)
protected:
    int8_t   _doutPin      = -1;    // I2S DOUT GPIO (from HalDeviceConfig.pinData)

#ifndef NATIVE_TEST
    TwoWire* _wire = nullptr;
#endif
};

#endif // DAC_ENABLED
