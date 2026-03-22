#ifdef DAC_ENABLED

#include "hal_relay.h"
#include "hal_device_manager.h"
#include <string.h>

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#else
#define LOG_I(tag, ...) ((void)0)
#endif

HalRelay::HalRelay(int pin)
    : _pin(pin)
{
    hal_init_descriptor(_descriptor, "generic,relay-amp", "Amplifier Relay", "Generic",
        HAL_DEV_AMP, 1, 0, HAL_BUS_GPIO, 0, 0, 0);
    _descriptor.bus.pinA = pin;
    _initPriority = HAL_PRIORITY_IO;
}

bool HalRelay::probe()
{
    _state = HAL_STATE_DETECTED;
    LOG_I("[HAL:Relay] probe OK — Amplifier Relay (GPIO%d)", _pin);
    return true;
}

HalInitResult HalRelay::init()
{
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.claimPin(_pin, HAL_BUS_GPIO, 0, _slot);
#ifndef NATIVE_TEST
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
#endif
    _enabled = false;
    _state = HAL_STATE_AVAILABLE;
    _ready = true;
    LOG_I("[HAL:Relay] init — Amplifier Relay ready on GPIO%d", _pin);
    return hal_init_ok();
}

void HalRelay::setEnabled(bool state)
{
#ifndef NATIVE_TEST
    digitalWrite(_pin, state ? HIGH : LOW);
#endif
    _enabled = state;
    LOG_I("[HAL:Relay] setEnabled=%d (GPIO%d)", state ? 1 : 0, _pin);
}

void HalRelay::deinit()
{
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.releasePin(_pin);
    _ready = false;
    _state = HAL_STATE_REMOVED;
    LOG_I("[HAL:Relay] deinit — Amplifier Relay removed");
}

void HalRelay::dumpConfig()
{
    LOG_I("[HAL:Relay] %s (%s)", _descriptor.name, _descriptor.compatible);
    LOG_I("[HAL:Relay]  manufacturer: %s", _descriptor.manufacturer);
    LOG_I("[HAL:Relay]  pin: GPIO%d", _pin);
}

bool HalRelay::healthCheck()
{
    return _ready;
}

#endif // DAC_ENABLED
