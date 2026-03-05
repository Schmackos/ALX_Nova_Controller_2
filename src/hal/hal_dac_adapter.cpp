#ifdef DAC_ENABLED

#include "hal_dac_adapter.h"

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#else
// Native test stubs
#define LOG_I(...)
#define LOG_W(...)
#define LOG_E(...)
#endif

HalDacAdapter::HalDacAdapter(DacDriver* driver, const HalDeviceDescriptor& desc, bool alreadyInitialized)
    : _driver(driver), _ownsDriver(false)
{
    _descriptor = desc;

    if (alreadyInitialized && _driver) {
        _state = HAL_STATE_AVAILABLE;
        _ready = true;
    }
}

HalDacAdapter::~HalDacAdapter() {
    if (_ownsDriver && _driver) {
        _driver->deinit();
        delete _driver;
        _driver = nullptr;
    }
}

bool HalDacAdapter::probe() {
    if (!_driver) return false;

    // No I2C control → always available (PCM5102A path)
    if (!_driver->getCapabilities().hasI2cControl) return true;

    // For I2C-controlled devices, check if driver reports ready
    // (the actual I2C probe happens in the driver's init)
    return true;  // Probe is non-destructive; actual HW check in init()
}

bool HalDacAdapter::init() {
    if (!_driver) return false;

    // If already initialized by dac_output_init() or dac_secondary_init(),
    // just verify the driver is ready
    if (_state == HAL_STATE_AVAILABLE && _ready) {
        return true;
    }

    // Driver was already init'd by the legacy path — just update state
    if (_driver->isReady()) {
        _state = HAL_STATE_AVAILABLE;
        _ready = true;
        return true;
    }

    _state = HAL_STATE_ERROR;
    _ready = false;
    return false;
}

void HalDacAdapter::deinit() {
    _ready = false;
    _state = HAL_STATE_REMOVED;
    // Don't call _driver->deinit() here — the legacy path handles that
}

void HalDacAdapter::dumpConfig() {
    if (!_driver) return;
    const DacCapabilities& caps = _driver->getCapabilities();
    LOG_I("[HAL] %s: %s by %s (legacy=0x%04X) ch=%u i2c=0x%02X hw_vol=%s",
          _descriptor.compatible,
          _descriptor.name,
          caps.manufacturer ? caps.manufacturer : "?",
          _descriptor.legacyId,
          _descriptor.channelCount,
          _descriptor.i2cAddr,
          caps.hasHardwareVolume ? "yes" : "no");
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

const DacCapabilities* HalDacAdapter::getLegacyCapabilities() const {
    if (!_driver) return nullptr;
    return &_driver->getCapabilities();
}

#endif // DAC_ENABLED
