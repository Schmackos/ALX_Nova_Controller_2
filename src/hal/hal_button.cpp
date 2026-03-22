#ifdef DAC_ENABLED

#include "hal_button.h"
#include "hal_device_manager.h"
#include "../config.h"
#include <string.h>

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#else
#define LOG_I(fmt, ...) ((void)0)
#endif

HalButton::HalButton(int pin)
    : _pin(pin)
{
    hal_init_descriptor(_descriptor, "generic,tact-switch", "Reset Button", "Generic",
        HAL_DEV_INPUT, 0, 0, HAL_BUS_GPIO, 0, 0, 0);
    _descriptor.bus.pinA = pin;
    _initPriority = HAL_PRIORITY_IO;  // 900
}

bool HalButton::probe()
{
    _state = HAL_STATE_DETECTED;
    return true;
}

HalInitResult HalButton::init()
{
    // Read config override for GPIO pin
    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);
    if (cfg && cfg->valid && cfg->gpioA >= 0) {
        _pin = cfg->gpioA;
    }
    _descriptor.bus.pinA = _pin;

    HalDeviceManager::instance().claimPin(_pin, HAL_BUS_GPIO, 0, _slot);

    _state = HAL_STATE_AVAILABLE;
    _ready = true;
    LOG_I("[HAL:Button] init — GPIO%d", _pin);
    return hal_init_ok();
}

void HalButton::deinit()
{
    HalDeviceManager::instance().releasePin(_pin);
    _ready = false;
    _state = HAL_STATE_REMOVED;
    LOG_I("[HAL:Button] deinit");
}

void HalButton::dumpConfig()
{
    LOG_I("[HAL:Button] %s (%s)", _descriptor.name, _descriptor.compatible);
    LOG_I("[HAL:Button]  manufacturer: %s", _descriptor.manufacturer);
    LOG_I("[HAL:Button]  pin: GPIO%d", _pin);
}

bool HalButton::healthCheck()
{
    return _ready;
}

#endif // DAC_ENABLED
