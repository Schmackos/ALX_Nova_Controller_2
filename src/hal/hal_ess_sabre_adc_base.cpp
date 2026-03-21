#ifdef DAC_ENABLED
// HalEssSabreAdcBase -- shared I2C helpers and config override reading for ESS SABRE ADC family.
// Hardware operations are guarded by #ifndef NATIVE_TEST throughout.

#include "hal_ess_sabre_adc_base.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include <Wire.h>
#include <Arduino.h>
#include "../debug_serial.h"
// Wire2 is not predefined in the ESP32-P4 Arduino framework — declare it here
// as a global so all ESS SABRE ADC drivers can use it.
TwoWire Wire2(2);
#else
// ===== Native test stubs -- no hardware access =====
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)
#endif // NATIVE_TEST

// ===== I2C helpers =====

bool HalEssSabreAdcBase::_writeReg(uint8_t reg, uint8_t val) {
#ifndef NATIVE_TEST
    if (!_wire) return false;
    _wire->beginTransmission(_i2cAddr);
    _wire->write(reg);
    _wire->write(val);
    uint8_t err = _wire->endTransmission();
    if (err != 0) {
        LOG_E("[HAL:ESS] I2C write failed: reg=0x%02X val=0x%02X err=%d", reg, val, err);
        return false;
    }
    return true;
#else
    (void)reg; (void)val;
    return true;
#endif
}

uint8_t HalEssSabreAdcBase::_readReg(uint8_t reg) {
#ifndef NATIVE_TEST
    if (!_wire) return 0xFF;
    _wire->beginTransmission(_i2cAddr);
    _wire->write(reg);
    _wire->endTransmission(false);
    _wire->requestFrom(_i2cAddr, (uint8_t)1);
    if (_wire->available()) return _wire->read();
    LOG_E("[HAL:ESS] I2C read failed: reg=0x%02X", reg);
    return 0xFF;
#else
    (void)reg;
    return 0x00;
#endif
}

// Multi-byte write: LSB at regLsb, MSB at regLsb+1 (MSB write latches both).
bool HalEssSabreAdcBase::_writeReg16(uint8_t regLsb, uint16_t val) {
    bool ok = _writeReg(regLsb,     (uint8_t)(val & 0xFF));
    ok      = ok && _writeReg(regLsb + 1, (uint8_t)((val >> 8) & 0xFF));
    return ok;
}

// ===== Config override reading =====

void HalEssSabreAdcBase::_applyConfigOverrides() {
    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);
    if (!cfg || !cfg->valid) return;

    if (cfg->i2cAddr != 0)     _i2cAddr      = cfg->i2cAddr;
    if (cfg->i2cBusIndex != 0) _i2cBusIndex  = cfg->i2cBusIndex;
    if (cfg->pinSda >= 0)      _sdaPin        = cfg->pinSda;
    if (cfg->pinScl >= 0)      _sclPin        = cfg->pinScl;
    if (cfg->sampleRate > 0)   _sampleRate    = cfg->sampleRate;
    if (cfg->bitDepth > 0)     _bitDepth      = cfg->bitDepth;
    if (cfg->pgaGain <= 42)    _gainDb        = cfg->pgaGain;
    _hpfEnabled   = cfg->hpfEnabled;
    _filterPreset = cfg->filterMode;   // 0-7 stored in filterMode field
}

// ===== Wire selection =====

void HalEssSabreAdcBase::_selectWire() {
#ifndef NATIVE_TEST
    switch (_i2cBusIndex) {
        case 0:  _wire = &Wire;  break;
        case 1:  _wire = &Wire1; break;
        case 2:  _wire = &Wire2; break;
        default: _wire = &Wire2; break;
    }
    _wire->begin((int)_sdaPin, (int)_sclPin, (uint32_t)ESS_SABRE_I2C_FREQ_HZ);
#endif
}

// ===== Sample rate validation =====

bool HalEssSabreAdcBase::_validateSampleRate(uint32_t hz, const uint32_t* supported, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        if (hz == supported[i]) return true;
    }
    return false;
}

#endif // DAC_ENABLED
