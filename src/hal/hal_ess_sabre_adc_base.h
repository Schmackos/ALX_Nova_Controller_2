#pragma once
#ifdef DAC_ENABLED

/**
 * HalEssSabreAdcBase -- Abstract base class for all ESS SABRE ADC family drivers.
 *
 * Provides shared I2C helpers, config override reading, Wire selection,
 * and common member fields. Derived classes implement device-specific
 * probe(), init(), deinit(), dumpConfig(), and healthCheck().
 *
 * Adding a new ESS SABRE ADC driver:
 *   1. Create src/drivers/esXXXX_regs.h with chip ID + register map
 *   2. Create src/hal/hal_esXXXX.h inheriting HalEssSabreAdcBase
 *   3. Create src/hal/hal_esXXXX.cpp implementing the 5 lifecycle methods
 *   4. Add factory + registration in hal_builtin_devices.cpp
 *   5. Add descriptor in hal_device_db.cpp
 *   6. Add test module in test/test_hal_esXXXX/
 */

#include "hal_audio_device.h"
#include "hal_audio_interfaces.h"
#include "../drivers/ess_sabre_common.h"

#ifndef NATIVE_TEST
#include <Wire.h>
#endif

class HalEssSabreAdcBase : public HalAudioDevice, public HalAudioAdcInterface {
public:
    virtual ~HalEssSabreAdcBase() = default;

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

    // --- HalAudioAdcInterface (pure virtual -- implemented per device) ---
    virtual bool     adcSetGain(uint8_t gainDb)      override = 0;
    virtual bool     adcSetHpfEnabled(bool en)       override = 0;
    virtual bool     adcSetSampleRate(uint32_t hz)   override = 0;
    virtual uint32_t adcGetSampleRate() const        override = 0;

protected:
    // --- I2C helpers (implemented in .cpp, shared by all derived classes) ---
    bool    _writeReg(uint8_t reg, uint8_t val);
    uint8_t _readReg(uint8_t reg);
    bool    _writeReg16(uint8_t regLsb, uint16_t val);  // LSB write then MSB (MSB write latches both)

    // --- Config override reading (steps 1-3 of init()) ---
    // Reads HalDeviceConfig overrides into member fields. Call at start of init().
    void _applyConfigOverrides();

    // --- Wire selection based on _i2cBusIndex ---
    // Must be called after _applyConfigOverrides() and before I2C operations.
    void _selectWire();

    // --- Sample rate validation ---
    // Returns true if hz is in the supported[] array of count entries.
    bool _validateSampleRate(uint32_t hz, const uint32_t* supported, uint8_t count);

    // --- Common member fields ---
    uint8_t  _i2cAddr      = ESS_SABRE_I2C_ADDR_BASE;
    int8_t   _sdaPin       = ESS_SABRE_I2C_BUS2_SDA;
    int8_t   _sclPin       = ESS_SABRE_I2C_BUS2_SCL;
    uint8_t  _i2cBusIndex  = 2;    // HAL_I2C_BUS_EXP (Bus 2, Expansion)
    uint32_t _sampleRate   = 48000;
    uint8_t  _bitDepth     = 32;
    uint8_t  _gainDb       = 0;
    uint8_t  _filterPreset = 0;
    bool     _hpfEnabled   = true;
    bool     _initialized  = false;

#ifndef NATIVE_TEST
    TwoWire* _wire = nullptr;
#endif
};

#endif // DAC_ENABLED
