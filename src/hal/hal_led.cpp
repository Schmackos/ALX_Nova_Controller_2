#ifdef DAC_ENABLED

#include "hal_led.h"
#include "hal_device_manager.h"
#include <string.h>

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#else
#define LOG_I(tag, ...) ((void)0)
#endif

HalLed::HalLed(int pin)
    : _pin(pin)
{
    memset(&_descriptor, 0, sizeof(_descriptor));
    strncpy(_descriptor.compatible, "generic,status-led", 31);
    strncpy(_descriptor.name, "Status LED", 32);
    strncpy(_descriptor.manufacturer, "Generic", 32);
    _descriptor.type = HAL_DEV_GPIO;
    _descriptor.bus.type = HAL_BUS_GPIO;
    _descriptor.bus.index = 0;
    _descriptor.bus.pinA = pin;
    _descriptor.channelCount = 1;
    _initPriority = HAL_PRIORITY_LATE;
}

bool HalLed::probe()
{
    _state = HAL_STATE_DETECTED;
    LOG_I("[HAL:LED] probe OK — Status LED (GPIO%d)", _pin);
    return true;
}

HalInitResult HalLed::init()
{
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.claimPin(_pin, HAL_BUS_GPIO, 0, _slot);
#ifndef NATIVE_TEST
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
#endif
    _state = HAL_STATE_AVAILABLE;
    _ready = true;
    LOG_I("[HAL:LED] init — Status LED ready on GPIO%d", _pin);
    return hal_init_ok();
}

void HalLed::setOn(bool state)
{
#ifndef NATIVE_TEST
    digitalWrite(_pin, state ? HIGH : LOW);
#endif
}

void HalLed::deinit()
{
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.releasePin(_pin);
    _ready = false;
    _state = HAL_STATE_REMOVED;
    LOG_I("[HAL:LED] deinit — Status LED removed");
}

void HalLed::dumpConfig()
{
    LOG_I("[HAL:LED] %s (%s)", _descriptor.name, _descriptor.compatible);
    LOG_I("[HAL:LED]  manufacturer: %s", _descriptor.manufacturer);
    LOG_I("[HAL:LED]  pin: GPIO%d", _pin);
}

bool HalLed::healthCheck()
{
    return _ready;
}

#endif // DAC_ENABLED
