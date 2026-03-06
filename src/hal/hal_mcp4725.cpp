#ifdef DAC_ENABLED

#include "hal_mcp4725.h"
#include "../debug_serial.h"
#include "../config.h"
#include <string.h>

#ifndef NATIVE_TEST
#include <Arduino.h>
#include <Wire.h>
#endif

HalMcp4725::HalMcp4725(uint8_t i2cAddr, uint8_t busIndex)
    : _i2cAddr(i2cAddr), _busIndex(busIndex), _code(0)
{
    memset(&_descriptor, 0, sizeof(_descriptor));
    strncpy(_descriptor.compatible, "microchip,mcp4725", 31);
    strncpy(_descriptor.name, "MCP4725", 32);
    strncpy(_descriptor.manufacturer, "Microchip Technology", 32);
    _descriptor.type         = HAL_DEV_DAC;
    _descriptor.bus.type     = HAL_BUS_I2C;
    _descriptor.bus.index    = busIndex;
    _descriptor.i2cAddr      = i2cAddr;
    _descriptor.channelCount = 1;
    _descriptor.capabilities = HAL_CAP_HW_VOLUME;
    _initPriority = HAL_PRIORITY_HARDWARE;
    _discovery    = HAL_DISC_MANUAL;
}

bool HalMcp4725::probe()
{
#ifndef NATIVE_TEST
    Wire.beginTransmission(_i2cAddr);
    uint8_t err = Wire.endTransmission();
    if (err != 0) {
        LOG_I("[MCP4725] probe FAIL — no ACK at 0x%02X (err %d)", _i2cAddr, err);
        return false;
    }
#endif
    _state = HAL_STATE_DETECTED;
    LOG_I("[MCP4725] probe OK — 0x%02X on I2C bus %d", _i2cAddr, _busIndex);
    return true;
}

bool HalMcp4725::init()
{
    // Set DAC to 0V output on init
    if (!setVoltageCode(0)) {
        LOG_I("[MCP4725] init FAIL — could not write DAC code");
        _state = HAL_STATE_ERROR;
        return false;
    }
    _ready = true;
    _state = HAL_STATE_AVAILABLE;
    LOG_I("[MCP4725] init OK — 0x%02X, output at 0V", _i2cAddr);
    return true;
}

void HalMcp4725::deinit()
{
#ifndef NATIVE_TEST
    // Drive output to 0 before releasing
    Wire.beginTransmission(_i2cAddr);
    Wire.write(0x00);
    Wire.write(0x00);
    Wire.endTransmission();
#endif
    _code  = 0;
    _ready = false;
    _state = HAL_STATE_REMOVED;
    LOG_I("[MCP4725] deinit — 0x%02X", _i2cAddr);
}

void HalMcp4725::dumpConfig()
{
    LOG_I("[MCP4725] %s (%s)", _descriptor.name, _descriptor.compatible);
    LOG_I("[MCP4725]   manufacturer: %s", _descriptor.manufacturer);
    LOG_I("[MCP4725]   I2C addr: 0x%02X, bus: %d", _i2cAddr, _busIndex);
    LOG_I("[MCP4725]   DAC code: %u (%.1f%%)", _code, (_code * 100.0f) / 4095.0f);
    LOG_I("[MCP4725]   state: %d, ready: %s", (int)_state, _ready ? "yes" : "no");
}

bool HalMcp4725::healthCheck()
{
#ifndef NATIVE_TEST
    Wire.beginTransmission(_i2cAddr);
    uint8_t err = Wire.endTransmission();
    if (err != 0) {
        LOG_I("[MCP4725] healthCheck FAIL — no ACK at 0x%02X", _i2cAddr);
        _ready = false;
        _state = HAL_STATE_UNAVAILABLE;
        return false;
    }
#endif
    return _ready;
}

bool HalMcp4725::setVolume(uint8_t percent)
{
    if (percent > 100) percent = 100;
    uint16_t code = (uint16_t)((percent * 4095UL) / 100UL);
    return setVoltageCode(code);
}

bool HalMcp4725::setVoltageCode(uint16_t code)
{
    if (code > 4095) code = 4095;
    _code = code;

#ifndef NATIVE_TEST
    Wire.beginTransmission(_i2cAddr);
    Wire.write((uint8_t)((code >> 4) & 0xFF));  // Upper 8 bits of 12-bit code
    Wire.write((uint8_t)((code & 0x0F) << 4));  // Lower 4 bits, left-aligned
    uint8_t err = Wire.endTransmission();
    if (err != 0) {
        LOG_I("[MCP4725] write FAIL — code %u, err %d", code, err);
        return false;
    }
#endif
    LOG_I("[MCP4725] DAC code = %u (%.1f%%)", code, (code * 100.0f) / 4095.0f);
    return true;
}

#endif // DAC_ENABLED
