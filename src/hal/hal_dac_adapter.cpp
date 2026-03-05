#ifdef DAC_ENABLED

#include "hal_dac_adapter.h"
#include <string.h>

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#include <Wire.h>
#else
// LOG stubs for native tests
#define LOG_I(tag, ...) ((void)0)
#define LOG_W(tag, ...) ((void)0)
#define LOG_E(tag, ...) ((void)0)
#endif

HalDacAdapter::HalDacAdapter(DacDriver* driver, const DacCapabilities& caps, uint16_t priority)
    : _driver(driver), _caps(caps) {
    // Populate descriptor from DacCapabilities
    strncpy(_descriptor.compatible, "", 31);  // Set by hal_builtin_devices
    if (caps.name) strncpy(_descriptor.name, caps.name, 32);
    _descriptor.name[32] = '\0';
    if (caps.manufacturer) strncpy(_descriptor.manufacturer, caps.manufacturer, 32);
    _descriptor.manufacturer[32] = '\0';

    // Map DacCapabilities to HAL descriptor
    _descriptor.type = HAL_DEV_DAC;
    _descriptor.legacyId = caps.deviceId;
    _descriptor.i2cAddr = caps.i2cAddress;
    _descriptor.channelCount = caps.maxChannels;

    // Build capabilities mask
    _descriptor.capabilities = 0;
    if (caps.hasHardwareVolume) _descriptor.capabilities |= HAL_CAP_HW_VOLUME;
    if (caps.hasFilterModes)    _descriptor.capabilities |= HAL_CAP_FILTERS;

    // Build sample rates mask from array
    _descriptor.sampleRatesMask = 0;
    for (uint8_t i = 0; i < caps.numSupportedRates; i++) {
        switch (caps.supportedRates[i]) {
            case 8000:   _descriptor.sampleRatesMask |= HAL_RATE_8K;   break;
            case 16000:  _descriptor.sampleRatesMask |= HAL_RATE_16K;  break;
            case 44100:  _descriptor.sampleRatesMask |= HAL_RATE_44K1; break;
            case 48000:  _descriptor.sampleRatesMask |= HAL_RATE_48K;  break;
            case 96000:  _descriptor.sampleRatesMask |= HAL_RATE_96K;  break;
            case 192000: _descriptor.sampleRatesMask |= HAL_RATE_192K; break;
        }
    }

    _initPriority = priority;
}

bool HalDacAdapter::probe() {
    if (!_driver) return false;

    // No I2C control → always available (PCM5102A path)
    if (!_caps.hasI2cControl || _caps.i2cAddress == 0) return true;

#ifndef NATIVE_TEST
    // I2C ACK check
    Wire.beginTransmission(_caps.i2cAddress);
    if (Wire.endTransmission() != 0) return false;
#endif

    return true;
}

bool HalDacAdapter::init() {
    if (!_driver) return false;
    // Driver is already initialized by dac_output_init() / dac_secondary_init()
    // We just mark as ready — the HAL adapter wraps an already-running driver
    return _driver->isReady();
}

void HalDacAdapter::deinit() {
    if (_driver) {
        _driver->deinit();
    }
    _ready = false;
    _state = HAL_STATE_REMOVED;
}

void HalDacAdapter::dumpConfig() {
#ifndef NATIVE_TEST
    LOG_I("[HAL]", "%s: type=%u ch=%u i2c=0x%02X legacy_id=0x%04X (%s, %s)",
          _descriptor.name, _descriptor.type, _descriptor.channelCount,
          _descriptor.i2cAddr, _descriptor.legacyId,
          _discovery == HAL_DISC_BUILTIN ? "BUILTIN" : "EXTERNAL",
          _state == HAL_STATE_AVAILABLE ? "AVAILABLE" : "NOT_READY");
#endif
}

bool HalDacAdapter::healthCheck() {
    if (!_driver) return false;
    return _driver->isReady();
}

bool HalDacAdapter::configure(uint32_t sampleRate, uint8_t bitDepth) {
    if (!_driver) return false;
    return _driver->configure(sampleRate, bitDepth);
}

bool HalDacAdapter::setVolume(uint8_t percent) {
    if (!_driver) return false;
    return _driver->setVolume(percent);
}

bool HalDacAdapter::setMute(bool mute) {
    if (!_driver) return false;
    return _driver->setMute(mute);
}

bool HalDacAdapter::setFilterMode(uint8_t mode) {
    if (!_driver) return false;
    return _driver->setFilterMode(mode);
}

#endif // DAC_ENABLED
