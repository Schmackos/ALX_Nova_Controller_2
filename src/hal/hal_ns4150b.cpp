#ifdef DAC_ENABLED

#include "hal_ns4150b.h"
#include "../debug_serial.h"
#include "../config.h"
#include <string.h>

#ifndef NATIVE_TEST
#include <Arduino.h>
#endif

// ES8311_PA_PIN (GPIO 53) is defined in drivers/es8311_regs.h for the
// Waveshare ESP32-P4-WiFi6-DEV-Kit.  The pin is passed via constructor
// so callers can use that define or any other GPIO.

HalNs4150b::HalNs4150b(int paPin)
    : _paPin(paPin), _enabled(false)
{
    memset(&_descriptor, 0, sizeof(_descriptor));
    strncpy(_descriptor.compatible, "ns,ns4150b-amp", 31);
    strncpy(_descriptor.name, "NS4150B Amp", 32);
    strncpy(_descriptor.manufacturer, "Nsiway", 32);
    _descriptor.type = HAL_DEV_AMP;
    _descriptor.bus.type = HAL_BUS_GPIO;
    _descriptor.bus.index = 0;
    _descriptor.bus.pinA = paPin;
    _descriptor.channelCount = 1;
    _initPriority = HAL_PRIORITY_IO;  // 900
}

bool HalNs4150b::probe()
{
#ifndef NATIVE_TEST
    pinMode(_paPin, OUTPUT);
    digitalWrite(_paPin, LOW);  // Keep amp disabled during probe
#endif
    // GPIO-only device — no read-back possible, always succeeds
    _state = HAL_STATE_DETECTED;
    LOG_I("[NS4150B] probe OK — PA pin GPIO%d configured as output", _paPin);
    return true;
}

HalInitResult HalNs4150b::init()
{
#ifndef NATIVE_TEST
    digitalWrite(_paPin, HIGH);  // Enable amplifier
#endif
    _enabled = true;
    _ready = true;
    _state = HAL_STATE_AVAILABLE;
    LOG_I("[NS4150B] init — amplifier enabled on GPIO%d", _paPin);
    return hal_init_ok();
}

void HalNs4150b::deinit()
{
#ifndef NATIVE_TEST
    digitalWrite(_paPin, LOW);  // Disable amplifier
#endif
    _enabled = false;
    _ready = false;
    _state = HAL_STATE_REMOVED;
    LOG_I("[NS4150B] deinit — amplifier disabled on GPIO%d", _paPin);
}

void HalNs4150b::dumpConfig()
{
    LOG_I("[NS4150B] %s (%s)", _descriptor.name, _descriptor.compatible);
    LOG_I("[NS4150B]   manufacturer: %s", _descriptor.manufacturer);
    LOG_I("[NS4150B]   PA pin: GPIO%d, enabled: %s, state: %d",
          _paPin, _enabled ? "yes" : "no", (int)_state);
}

bool HalNs4150b::healthCheck()
{
    // GPIO-only device with no read-back — always healthy if initialised
    return _ready;
}

void HalNs4150b::setEnable(bool enabled)
{
#ifndef NATIVE_TEST
    digitalWrite(_paPin, enabled ? HIGH : LOW);
#endif
    _enabled = enabled;
    LOG_I("[NS4150B] amplifier %s (GPIO%d)", enabled ? "enabled" : "disabled", _paPin);
}

#endif // DAC_ENABLED
