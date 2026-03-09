#ifdef DAC_ENABLED

#include "hal_buzzer.h"
#include "hal_device_manager.h"
#include "../buzzer_handler.h"
#include "../config.h"
#include <string.h>

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#else
#define LOG_I(fmt, ...) ((void)0)
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
    _descriptor.channelCount = 0;
    _initPriority = HAL_PRIORITY_IO;  // 900
}

bool HalBuzzer::probe()
{
    _state = HAL_STATE_DETECTED;
    LOG_I("[HAL:Buzzer] probe OK — Piezo Buzzer (GPIO%d)", _pin);
    return true;
}

HalInitResult HalBuzzer::init()
{
    // Read config override for GPIO pin
    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);
    if (cfg && cfg->valid && cfg->gpioA >= 0) {
        _pin = cfg->gpioA;
    }
    _descriptor.bus.pinA = _pin;

    HalDeviceManager::instance().claimPin(_pin, HAL_BUS_GPIO, 0, _slot);
    buzzer_init(_pin);

    _state = HAL_STATE_AVAILABLE;
    _ready = true;
    LOG_I("[HAL:Buzzer] init — GPIO%d", _pin);
    return hal_init_ok();
}

void HalBuzzer::deinit()
{
    buzzer_deinit();
    HalDeviceManager::instance().releasePin(_pin);
    _ready = false;
    _state = HAL_STATE_REMOVED;
    LOG_I("[HAL:Buzzer] deinit");
}

void HalBuzzer::dumpConfig()
{
    LOG_I("[HAL:Buzzer] %s (%s)", _descriptor.name, _descriptor.compatible);
    LOG_I("[HAL:Buzzer]  manufacturer: %s", _descriptor.manufacturer);
    LOG_I("[HAL:Buzzer]  pin: GPIO%d", _pin);
}

bool HalBuzzer::healthCheck()
{
    return _ready;
}

#endif // DAC_ENABLED
