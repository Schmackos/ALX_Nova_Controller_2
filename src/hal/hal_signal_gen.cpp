#ifdef DAC_ENABLED

#include "hal_signal_gen.h"
#include "hal_device_manager.h"
#include <string.h>

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#include "../signal_generator.h"
#else
#define LOG_I(tag, ...) ((void)0)
inline void siggen_init(int) {}
inline void siggen_deinit() {}
#endif

HalSignalGen::HalSignalGen(int pin)
    : _pin(pin)
{
    memset(&_descriptor, 0, sizeof(_descriptor));
    strncpy(_descriptor.compatible, "generic,signal-gen", 31);
    strncpy(_descriptor.name, "Signal Generator", 32);
    strncpy(_descriptor.manufacturer, "Generic", 32);
    _descriptor.type = HAL_DEV_GPIO;
    _descriptor.bus.type = HAL_BUS_GPIO;
    _descriptor.bus.index = 0;
    _descriptor.bus.pinA = pin;
    _descriptor.channelCount = 1;
    _initPriority = HAL_PRIORITY_LATE;
}

bool HalSignalGen::probe()
{
    _state = HAL_STATE_DETECTED;
    LOG_I("[HAL:SigGen] probe OK — Signal Generator (GPIO%d)", _pin);
    return true;
}

HalInitResult HalSignalGen::init()
{
    HalDeviceManager& mgr = HalDeviceManager::instance();
    HalDeviceConfig* cfg = mgr.getConfig(_slot);
    if (cfg && cfg->valid && cfg->gpioA >= 0) {
        mgr.releasePin(_pin);
        _pin = cfg->gpioA;
    }
    _descriptor.bus.pinA = _pin;
    mgr.claimPin(_pin, HAL_BUS_GPIO, 0, _slot);
    siggen_init(_pin);
    _state = HAL_STATE_AVAILABLE;
    setReady(true);
    LOG_I("[HAL:SigGen] init — Signal Generator ready on GPIO%d", _pin);
    return hal_init_ok();
}

void HalSignalGen::deinit()
{
    siggen_deinit();
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.releasePin(_pin);
    setReady(false);
    _state = HAL_STATE_REMOVED;
    LOG_I("[HAL:SigGen] deinit — Signal Generator removed");
}

void HalSignalGen::dumpConfig()
{
    LOG_I("[HAL:SigGen] %s (%s)", _descriptor.name, _descriptor.compatible);
    LOG_I("[HAL:SigGen]  manufacturer: %s", _descriptor.manufacturer);
    LOG_I("[HAL:SigGen]  pin: GPIO%d", _pin);
}

bool HalSignalGen::healthCheck()
{
    return _ready;
}

int HalSignalGen::getPin() const
{
    return _pin;
}

#endif // DAC_ENABLED
