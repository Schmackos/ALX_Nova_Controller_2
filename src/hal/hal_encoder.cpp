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
    memset(&_descriptor, 0, sizeof(_descriptor));
    strncpy(_descriptor.compatible, "alps,ec11", 31);
    strncpy(_descriptor.name, "Rotary Encoder", 32);
    strncpy(_descriptor.manufacturer, "Alps", 32);
    _descriptor.type = HAL_DEV_INPUT;
    _descriptor.bus.type = HAL_BUS_GPIO;
    _descriptor.bus.index = 0;
    _descriptor.bus.pinA = pinA;
    _descriptor.bus.pinB = pinB;
    _descriptor.channelCount = 3;
    _initPriority = HAL_PRIORITY_IO;
}

bool HalEncoder::probe()
{
    _state = HAL_STATE_DETECTED;
    LOG_I("[HAL] probe OK — Rotary Encoder (A=%d, B=%d, SW=%d)", _pinA, _pinB, _pinSw);
    return true;
}

bool HalEncoder::init()
{
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.claimPin(_pinA,  HAL_BUS_GPIO, 0, _slot);
    mgr.claimPin(_pinB,  HAL_BUS_GPIO, 0, _slot);
    mgr.claimPin(_pinSw, HAL_BUS_GPIO, 0, _slot);
    _state = HAL_STATE_AVAILABLE;
    _ready = true;
    LOG_I("[HAL] init — Rotary Encoder ready");
    return true;
}

void HalEncoder::deinit()
{
    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.releasePin(_pinA);
    mgr.releasePin(_pinB);
    mgr.releasePin(_pinSw);
    _ready = false;
    _state = HAL_STATE_REMOVED;
    LOG_I("[HAL] deinit — Rotary Encoder removed");
}

void HalEncoder::dumpConfig()
{
    LOG_I("[HAL] %s (%s)", _descriptor.name, _descriptor.compatible);
    LOG_I("[HAL]   manufacturer: %s", _descriptor.manufacturer);
    LOG_I("[HAL]   A=%d B=%d SW=%d", _pinA, _pinB, _pinSw);
}

bool HalEncoder::healthCheck()
{
    return _ready;
}

#endif // DAC_ENABLED
