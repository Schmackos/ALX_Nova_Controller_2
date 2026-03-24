#pragma once
#ifdef DAC_ENABLED

/**
 * HalCirrusDacBase -- Abstract base class for all Cirrus Logic DAC family drivers.
 *
 * Provides shared I2C helpers (8-bit + 16-bit paged addressing), config override
 * reading, Wire selection, buildSink() infrastructure, and expansion I2S TX lifecycle.
 * Derived classes implement device-specific probe(), init(), deinit(), dumpConfig(),
 * and healthCheck().
 *
 * Adding a new Cirrus Logic DAC driver:
 *   1. Create src/drivers/csXXXXX_regs.h with register map
 *   2. Create src/hal/hal_csXXXXX.h inheriting HalCirrusDacBase
 *   3. Create src/hal/hal_csXXXXX.cpp implementing the 5 lifecycle methods
 *   4. Add factory + registration in hal_builtin_devices.cpp
 *   5. Add descriptor in hal_device_db.cpp
 *   6. Add test module in test/test_hal_csXXXXX/
 */

#include "hal_audio_device.h"
#include "hal_audio_interfaces.h"
#include "hal_i2c_bus.h"
#include "../drivers/cirrus_dac_common.h"
#include "../audio_output_sink.h"

class HalCirrusDacBase : public HalAudioDevice, public HalAudioDacInterface {
public:
    virtual ~HalCirrusDacBase() = default;

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
    // --- I2C bus accessor ---
    HalI2cBus& _bus() { return HalI2cBus::get(_i2cBusIndex); }

    // --- I2C helpers: 8-bit register addressing (CS4398) ---
    bool    _writeReg8(uint8_t reg, uint8_t val)    { return _bus().writeReg(_i2cAddr, reg, val); }
    uint8_t _readReg8(uint8_t reg)                  { return _bus().readReg(_i2cAddr, reg); }

    // --- I2C helpers: 16-bit paged register addressing (CS43198, CS43131, CS4399, CS43130) ---
    // Sends 2-byte register address (high byte, low byte) then 1-byte data
    bool    _writeRegPaged(uint16_t reg, uint8_t val) { return _bus().writeRegPaged(_i2cAddr, reg, val); }
    uint8_t _readRegPaged(uint16_t reg)               { return _bus().readRegPaged(_i2cAddr, reg); }

    // --- Config override reading ---
    void _applyConfigOverrides();

    // --- Wire selection based on _i2cBusIndex ---
    void _selectWire();

    // --- Sample rate validation ---
    bool _validateSampleRate(uint32_t hz, const uint32_t* supported, uint8_t count);

    // --- I2S TX lifecycle (expansion DAC output) ---
    bool _enableI2sTx();
    void _disableI2sTx();

    // --- Common member fields ---
    uint8_t  _i2cAddr      = CIRRUS_DAC_I2C_ADDR_BASE;
    int8_t   _sdaPin       = CIRRUS_DAC_I2C_BUS2_SDA;
    int8_t   _sclPin       = CIRRUS_DAC_I2C_BUS2_SCL;
    uint8_t  _i2cBusIndex  = 2;    // HAL_I2C_BUS_EXP (configurable)
    uint32_t _sampleRate   = 48000;
    uint8_t  _bitDepth     = 32;
    uint8_t  _filterPreset = 0;
    uint8_t  _volume       = 100;
    bool     _muted        = false;
    bool     _initialized  = false;
    bool     _i2sTxEnabled = false;
public:
    float    _muteRampState = 1.0f;  // Mute ramp envelope (public: accessed by static write callback)
protected:
    int8_t   _doutPin      = -1;    // I2S DOUT GPIO (from HalDeviceConfig.pinData)
};

#endif // DAC_ENABLED
