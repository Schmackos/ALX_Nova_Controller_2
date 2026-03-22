#ifdef DAC_ENABLED

#include "hal_encoder.h"
#include "hal_device_manager.h"
#include <string.h>

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#else
#define LOG_I(tag, ...) ((void)0)
#endif

HalEncoder::HalEncoder(int pinA, int pinB, int pinSw)
    : _pinA(pinA), _pinB(pinB), _pinSw(pinSw)
{
    hal_init_descriptor(_descriptor, "alps,ec11", "Rotary Encoder", "Alps",
        HAL_DEV_INPUT, 3, 0, HAL_BUS_GPIO, 0, 0, 0);
    _descriptor.bus.pinA = pinA;
    _descriptor.bus.pinB = pinB;
    _initPriority = HAL_PRIORITY_IO;
}

bool HalEncoder::probe()
{
    _state = HAL_STATE_DETECTED;
    LOG_I("[HAL:Encoder] probe OK — Rotary Encoder (A=%d, B=%d, SW=%d)", _pinA, _pinB, _pinSw);
    return true;
}

HalInitResult HalEncoder::init()
{
    HalDeviceManager& mgr = HalDeviceManager::instance();
    HalDeviceConfig* cfg = mgr.getConfig(_slot);
    if (cfg && cfg->valid) {
        if (cfg->gpioA >= 0) _pinA = cfg->gpioA;
        if (cfg->gpioB >= 0) _pinB = cfg->gpioB;
        if (cfg->gpioC >= 0) _pinSw = cfg->gpioC;
    }
    _descriptor.bus.pinA = _pinA;
    _descriptor.bus.pinB = _pinB;
    mgr.claimPin(_pinA,  HAL_BUS_GPIO, 0, _slot);
    mgr.claimPin(_pinB,  HAL_BUS_GPIO, 0, _slot);
    mgr.claimPin(_pinSw, HAL_BUS_GPIO, 0, _slot);
    _state = HAL_STATE_AVAILABLE;
    _ready = true;
    LOG_I("[HAL:Encoder] init — Rotary Encoder ready (A=%d, B=%d, SW=%d)", _pinA, _pinB, _pinSw);
    return hal_init_ok();
}

void HalEncoder::deinit()
{
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.releasePin(_pinA);
    mgr.releasePin(_pinB);
    mgr.releasePin(_pinSw);
    _ready = false;
    _state = HAL_STATE_REMOVED;
    LOG_I("[HAL:Encoder] deinit — Rotary Encoder removed");
}

void HalEncoder::dumpConfig()
{
    LOG_I("[HAL:Encoder] %s (%s)", _descriptor.name, _descriptor.compatible);
    LOG_I("[HAL:Encoder]  manufacturer: %s", _descriptor.manufacturer);
    LOG_I("[HAL:Encoder]  A=%d B=%d SW=%d", _pinA, _pinB, _pinSw);
}

bool HalEncoder::healthCheck()
{
    return _ready;
}

#endif // DAC_ENABLED
