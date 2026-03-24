#ifdef DAC_ENABLED
// HalEssSabreAdcBase -- shared helpers and config override reading for ESS SABRE ADC family.
// I2C operations now delegate to HalI2cBus::get(_i2cBusIndex).

#include "hal_ess_sabre_adc_base.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#else
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)
#endif // NATIVE_TEST

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

// ===== Wire selection / bus initialisation =====

void HalEssSabreAdcBase::_selectWire() {
    _bus().begin(_sdaPin, _sclPin, (uint32_t)ESS_SABRE_I2C_FREQ_HZ);
}

// ===== Sample rate validation =====

bool HalEssSabreAdcBase::_validateSampleRate(uint32_t hz, const uint32_t* supported, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        if (hz == supported[i]) return true;
    }
    return false;
}

#endif // DAC_ENABLED
