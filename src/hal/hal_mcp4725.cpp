#ifdef DAC_ENABLED

#include "hal_mcp4725.h"
#include "../config.h"
#include <string.h>

#ifndef NATIVE_TEST
#include <Arduino.h>
#include <Wire.h>
#include "../debug_serial.h"
#else
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#endif

HalMcp4725::HalMcp4725(uint8_t i2cAddr, uint8_t busIndex)
    : _i2cAddr(i2cAddr), _busIndex(busIndex), _code(0)
{
    hal_init_descriptor(_descriptor, "microchip,mcp4725", "MCP4725", "Microchip Technology",
        HAL_DEV_DAC, 1, i2cAddr, HAL_BUS_I2C, busIndex,
        0,
        HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME);
    _initPriority = HAL_PRIORITY_HARDWARE;
    _discovery    = HAL_DISC_MANUAL;
}

bool HalMcp4725::probe()
{
#ifndef NATIVE_TEST
    Wire.beginTransmission(_i2cAddr);
    uint8_t err = Wire.endTransmission();
    if (err != 0) {
        LOG_I("[HAL:MCP4725] probe FAIL — no ACK at 0x%02X (err %d)", _i2cAddr, err);
        return false;
    }
#endif
    _state = HAL_STATE_DETECTED;
    LOG_I("[HAL:MCP4725] probe OK — 0x%02X on I2C bus %d", _i2cAddr, _busIndex);
    return true;
}

HalInitResult HalMcp4725::init()
{
    // Set DAC to 0V output on init
    if (!setVoltageCode(0)) {
        LOG_I("[HAL:MCP4725] init FAIL — could not write DAC code");
        _state = HAL_STATE_ERROR;
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "DAC write failed");
    }
    _ready = true;
    _state = HAL_STATE_AVAILABLE;
    LOG_I("[HAL:MCP4725] init OK — 0x%02X, output at 0V", _i2cAddr);
    return hal_init_ok();
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
    LOG_I("[HAL:MCP4725] deinit — 0x%02X", _i2cAddr);
}

void HalMcp4725::dumpConfig()
{
    LOG_I("[HAL:MCP4725] %s (%s)", _descriptor.name, _descriptor.compatible);
    LOG_I("[HAL:MCP4725]   manufacturer: %s", _descriptor.manufacturer);
    LOG_I("[HAL:MCP4725]   I2C addr: 0x%02X, bus: %d", _i2cAddr, _busIndex);
    LOG_I("[HAL:MCP4725]   DAC code: %u (%.1f%%)", _code, (_code * 100.0f) / 4095.0f);
    LOG_I("[HAL:MCP4725]   state: %d, ready: %s", (int)_state, _ready ? "yes" : "no");
}

bool HalMcp4725::healthCheck()
{
#ifndef NATIVE_TEST
    Wire.beginTransmission(_i2cAddr);
    uint8_t err = Wire.endTransmission();
    if (err != 0) {
        LOG_I("[HAL:MCP4725] healthCheck FAIL — no ACK at 0x%02X", _i2cAddr);
        _ready = false;
        _state = HAL_STATE_UNAVAILABLE;
        return false;
    }
#endif
    return _ready;
}

bool HalMcp4725::configure(uint32_t sampleRate, uint8_t bitDepth)
{
    // MCP4725 is a voltage-output DAC, not a streaming audio device.
    // Sample rate / bit depth are not applicable — accept any configuration.
    (void)sampleRate; (void)bitDepth;
    return true;
}

bool HalMcp4725::setVolume(uint8_t percent)
{
    if (percent > 100) percent = 100;
    uint16_t code = (uint16_t)((percent * 4095UL) / 100UL);
    return setVoltageCode(code);
}

bool HalMcp4725::setMute(bool mute)
{
    _muted = mute;
    if (mute) {
        return setVoltageCode(0);
    }
    return true;
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
        LOG_I("[HAL:MCP4725] write FAIL — code %u, err %d", code, err);
        return false;
    }
#endif
    LOG_I("[HAL:MCP4725] DAC code = %u (%.1f%%)", code, (code * 100.0f) / 4095.0f);
    return true;
}

// ===== buildSink() — populate AudioOutputSink for pipeline registration =====
// Static device-pointer table indexed by sink slot. Required because the
// isReady callback signature is bool(*)(void) — no context parameter.
static HalMcp4725* _mcp4725_slot_dev[AUDIO_OUT_MAX_SINKS] = {};

// Stub write callback — real I2C voltage write will be wired later
static void _mcp4725_write_stub(const int32_t* buf, int stereoFrames) {
    (void)buf; (void)stereoFrames;
}

// isReady callback template for each slot — looks up device via static table
#define MCP4725_READY_FN(N) \
    static bool _mcp4725_ready_##N(void) { \
        return _mcp4725_slot_dev[N] && _mcp4725_slot_dev[N]->_ready; \
    }

MCP4725_READY_FN(0)
MCP4725_READY_FN(1)
MCP4725_READY_FN(2)
MCP4725_READY_FN(3)
MCP4725_READY_FN(4)
MCP4725_READY_FN(5)
MCP4725_READY_FN(6)
MCP4725_READY_FN(7)

static bool (*const _mcp4725_ready_fn[AUDIO_OUT_MAX_SINKS])(void) = {
    _mcp4725_ready_0, _mcp4725_ready_1, _mcp4725_ready_2, _mcp4725_ready_3,
    _mcp4725_ready_4, _mcp4725_ready_5, _mcp4725_ready_6, _mcp4725_ready_7,
};

bool HalMcp4725::buildSink(uint8_t sinkSlot, AudioOutputSink* out) {
    if (!out) return false;
    if (sinkSlot >= AUDIO_OUT_MAX_SINKS) return false;

    *out = AUDIO_OUTPUT_SINK_INIT;
    out->name         = _descriptor.name;
    uint8_t fc = (uint8_t)(sinkSlot * 2);
    if (fc + _descriptor.channelCount > AUDIO_PIPELINE_MATRIX_SIZE) return false;
    out->firstChannel = fc;
    out->channelCount = _descriptor.channelCount;
    out->halSlot      = _slot;
    out->write        = _mcp4725_write_stub;
    out->isReady      = _mcp4725_ready_fn[sinkSlot];
    out->ctx          = this;

    // Register in static table for isReady callback lookup
    _mcp4725_slot_dev[sinkSlot] = this;

    return true;
}

#endif // DAC_ENABLED
