#ifdef DAC_ENABLED

#include "hal_button.h"
#include "hal_device_manager.h"
#include <string.h>

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#else
#define LOG_I(tag, ...) ((void)0)
#endif

HalButton::HalButton(int pin)
    : _pin(pin)
{
    memset(&_descriptor, 0, sizeof(_descriptor));
    strncpy(_descriptor.compatible, "generic,tact-switch", 31);
    strncpy(_descriptor.name, "Reset Button", 32);
    strncpy(_descriptor.manufacturer, "Generic", 32);
    _descriptor.type = HAL_DEV_INPUT;
    _descriptor.bus.type = HAL_BUS_GPIO;
    _descriptor.bus.index = 0;
    _descriptor.bus.pinA = pin;
    _descriptor.channelCount = 1;
    _initPriority = HAL_PRIORITY_LATE;
}

bool HalButton::probe()
{
    _state = HAL_STATE_DETECTED;
    LOG_I("[HAL] probe OK — Reset Button (GPIO%d)", _pin);
    return true;
}

bool HalButton::init()
{
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.claimPin(_pin, HAL_BUS_GPIO, 0, _slot);
    _state = HAL_STATE_AVAILABLE;
    _ready = true;
    LOG_I("[HAL] init — Reset Button ready on GPIO%d", _pin);
    return true;
}

void HalButton::deinit()
{
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.releasePin(_pin);
    _ready = false;
    _state = HAL_STATE_REMOVED;
    LOG_I("[HAL] deinit — Reset Button removed");
}

void HalButton::dumpConfig()
{
    LOG_I("[HAL] %s (%s)", _descriptor.name, _descriptor.compatible);
    LOG_I("[HAL]   manufacturer: %s", _descriptor.manufacturer);
    LOG_I("[HAL]   pin: GPIO%d", _pin);
}

bool HalButton::healthCheck()
{
    return _ready;
}

#endif // DAC_ENABLED
