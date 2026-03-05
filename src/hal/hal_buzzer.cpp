#ifdef DAC_ENABLED

#include "hal_buzzer.h"
#include "hal_device_manager.h"
#include <string.h>

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#else
#define LOG_I(tag, ...) ((void)0)
#endif

HalBuzzer::HalBuzzer(int pin)
    : _pin(pin)
{
    memset(&_descriptor, 0, sizeof(_descriptor));
    strncpy(_descriptor.compatible, "generic,piezo-buzzer", 31);
    strncpy(_descriptor.name, "Piezo Buzzer", 32);
    strncpy(_descriptor.manufacturer, "Generic", 32);
    _descriptor.type = HAL_DEV_GPIO;
    _descriptor.bus.type = HAL_BUS_GPIO;
    _descriptor.bus.index = 0;
    _descriptor.bus.pinA = pin;
    _descriptor.channelCount = 1;
    _initPriority = HAL_PRIORITY_LATE;
}

bool HalBuzzer::probe()
{
    _state = HAL_STATE_DETECTED;
    LOG_I("[HAL] probe OK — Piezo Buzzer (GPIO%d)", _pin);
    return true;
}

bool HalBuzzer::init()
{
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.claimPin(_pin, HAL_BUS_GPIO, 0, _slot);
    _state = HAL_STATE_AVAILABLE;
    _ready = true;
    LOG_I("[HAL] init — Piezo Buzzer ready on GPIO%d", _pin);
    return true;
}

void HalBuzzer::deinit()
{
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.releasePin(_pin);
    _ready = false;
    _state = HAL_STATE_REMOVED;
    LOG_I("[HAL] deinit — Piezo Buzzer removed");
}

void HalBuzzer::dumpConfig()
{
    LOG_I("[HAL] %s (%s)", _descriptor.name, _descriptor.compatible);
    LOG_I("[HAL]   manufacturer: %s", _descriptor.manufacturer);
    LOG_I("[HAL]   pin: GPIO%d", _pin);
}

bool HalBuzzer::healthCheck()
{
    return _ready;
}

#endif // DAC_ENABLED
