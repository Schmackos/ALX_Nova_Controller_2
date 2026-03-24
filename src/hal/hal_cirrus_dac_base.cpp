#ifdef DAC_ENABLED
// HalCirrusDacBase -- shared I2C helpers, config override reading, buildSink()
// infrastructure, and expansion I2S TX lifecycle for Cirrus Logic DAC family.

#include "hal_cirrus_dac_base.h"
#include "hal_device_manager.h"
#include "../i2s_audio.h"
#include "../audio_pipeline.h"
#include "../sink_write_utils.h"

#ifndef NATIVE_TEST
#include <Arduino.h>
#include "../debug_serial.h"
#include "../config.h"   // ES8311_I2S_MCLK_PIN / SCLK_PIN / LRCK_PIN defaults
#else
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)
#endif

// ===== Config override reading =====

void HalCirrusDacBase::_applyConfigOverrides() {
    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);
    if (!cfg || !cfg->valid) return;

    if (cfg->i2cAddr != 0)     _i2cAddr      = cfg->i2cAddr;
    if (cfg->i2cBusIndex != 0) _i2cBusIndex  = cfg->i2cBusIndex;
    if (cfg->pinSda >= 0)      _sdaPin       = cfg->pinSda;
    if (cfg->pinScl >= 0)      _sclPin       = cfg->pinScl;
    if (cfg->sampleRate > 0)   _sampleRate   = cfg->sampleRate;
    if (cfg->bitDepth > 0)     _bitDepth     = cfg->bitDepth;
    if (cfg->volume > 0)       _volume       = cfg->volume;
    if (cfg->pinData >= 0)     _doutPin      = cfg->pinData;
    _muted        = cfg->mute;
    _filterPreset = cfg->filterMode;
}

// ===== Wire selection =====

void HalCirrusDacBase::_selectWire() {
    _bus().begin(_sdaPin, _sclPin, (uint32_t)CIRRUS_DAC_I2C_FREQ_HZ);
}

// ===== Sample rate validation =====

bool HalCirrusDacBase::_validateSampleRate(uint32_t hz, const uint32_t* supported, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        if (hz == supported[i]) return true;
    }
    return false;
}

// ===== I2S TX lifecycle =====

bool HalCirrusDacBase::_enableI2sTx() {
#ifndef NATIVE_TEST
    // Resolve port: use config override if valid, otherwise default to port 2 (expansion)
    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);
    uint8_t port = (cfg && cfg->valid && cfg->i2sPort != 255) ? cfg->i2sPort : 2;

    gpio_num_t dout  = (_doutPin >= 0) ? (gpio_num_t)_doutPin : GPIO_NUM_NC;
    gpio_num_t mclk  = (cfg && cfg->valid && cfg->pinMclk >= 0) ? (gpio_num_t)cfg->pinMclk
                                                                 : (gpio_num_t)ES8311_I2S_MCLK_PIN;
    gpio_num_t bck   = (cfg && cfg->valid && cfg->pinBck  >= 0) ? (gpio_num_t)cfg->pinBck
                                                                 : (gpio_num_t)ES8311_I2S_SCLK_PIN;
    gpio_num_t ws    = (cfg && cfg->valid && cfg->pinLrc  >= 0) ? (gpio_num_t)cfg->pinLrc
                                                                 : (gpio_num_t)ES8311_I2S_LRCK_PIN;

    // Build I2sPortConfig from HAL device config (0/default fields = keep IDF defaults)
    I2sPortConfig i2sCfg = {};
    if (cfg && cfg->valid) {
        i2sCfg.format       = cfg->i2sFormat;
        i2sCfg.bitDepth     = cfg->bitDepth;
        i2sCfg.mclkMultiple = cfg->mclkMultiple;
    }

    if (!i2s_port_enable_tx(port, I2S_MODE_STD, 0, dout, mclk, bck, ws, &i2sCfg)) {
        LOG_E("[HAL:Cirrus-DAC] Expansion TX enable failed (port=%u sr=%lu dout=%d)",
              port, (unsigned long)_sampleRate, _doutPin);
        return false;
    }
    _i2sTxEnabled = true;
    return true;
#else
    _i2sTxEnabled = true;
    return true;
#endif
}

void HalCirrusDacBase::_disableI2sTx() {
    if (_i2sTxEnabled) {
#ifndef NATIVE_TEST
        HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);
        uint8_t port = (cfg && cfg->valid && cfg->i2sPort != 255) ? cfg->i2sPort : 2;
        i2s_port_disable_tx(port);
#endif
        _i2sTxEnabled = false;
    }
}

// ===== DAC interface defaults =====

bool HalCirrusDacBase::dacSetSampleRate(uint32_t hz) {
    return configure(hz, _bitDepth);
}

bool HalCirrusDacBase::dacSetBitDepth(uint8_t bits) {
    return configure(_sampleRate, bits);
}

bool HalCirrusDacBase::setFilterPreset(uint8_t preset) {
    if (preset >= 8) return false;  // 8 max; individual drivers may tighten this
    _filterPreset = preset;
    return true;
}

// ===== buildSink() infrastructure =====

// Static device-pointer table indexed by sink slot. Required because
// isReady callback signature is bool(*)(void) — no context parameter.
static HalCirrusDacBase* _cirrus_dac_slot_dev[AUDIO_OUT_MAX_SINKS] = {};

// Write callback — converts pipeline int32 to float, applies volume/mute ramp,
// converts back to int32, writes to expansion I2S TX.
static void _cirrus_dac_write(const int32_t* buf, int stereoFrames) {
    if (!buf || stereoFrames <= 0) return;

#ifndef NATIVE_TEST
    int totalSamples = stereoFrames * 2;
    float fBuf[512];
    int32_t txBuf[512];
    const int32_t* src = buf;
    int remaining = totalSamples;

    // Find the device for mute ramp state — search the slot table
    HalCirrusDacBase* dev = nullptr;
    uint8_t sinkSlot = 0;
    for (uint8_t s = 0; s < AUDIO_OUT_MAX_SINKS; s++) {
        if (_cirrus_dac_slot_dev[s]) {
            dev = _cirrus_dac_slot_dev[s];
            sinkSlot = s;
            break;
        }
    }

    while (remaining > 0) {
        int chunk = (remaining > 512) ? 512 : remaining;

        // int32 -> float [-1.0, +1.0]
        for (int i = 0; i < chunk; i++) {
            fBuf[i] = (float)src[i] / 2147483520.0f;
        }

        // Apply software volume (DACs with HW volume may want to skip this)
        float volGain = audio_pipeline_get_sink_volume(sinkSlot);
        sink_apply_volume(fBuf, chunk, volGain);

        // Apply mute ramp (click-free fade)
        if (dev) {
            bool muted = audio_pipeline_is_sink_muted(sinkSlot);
            sink_apply_mute_ramp(fBuf, chunk, &dev->_muteRampState, muted);
        }

        // float -> int32 left-justified
        sink_float_to_i2s_int32(fBuf, txBuf, chunk);

        // Write to expansion I2S TX via port-generic API.
        // The port is resolved from the device's HAL config (default port 2).
        size_t bytesWritten = 0;
        uint8_t txPort = 2;
        if (dev) {
            HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(dev->getSlot());
            if (cfg && cfg->valid && cfg->i2sPort != 255) txPort = cfg->i2sPort;
        }
        i2s_port_write(txPort, txBuf, (size_t)chunk * sizeof(int32_t), &bytesWritten, 20);

        src += chunk;
        remaining -= chunk;
    }
#else
    (void)stereoFrames;
#endif
}

// isReady callback template — looks up device via static table
#define CIRRUS_DAC_READY_FN(N) \
    static bool _cirrus_dac_ready_##N(void) { \
        return _cirrus_dac_slot_dev[N] && _cirrus_dac_slot_dev[N]->isReady(); \
    }

CIRRUS_DAC_READY_FN(0)
CIRRUS_DAC_READY_FN(1)
CIRRUS_DAC_READY_FN(2)
CIRRUS_DAC_READY_FN(3)
CIRRUS_DAC_READY_FN(4)
CIRRUS_DAC_READY_FN(5)
CIRRUS_DAC_READY_FN(6)
CIRRUS_DAC_READY_FN(7)
CIRRUS_DAC_READY_FN(8)
CIRRUS_DAC_READY_FN(9)
CIRRUS_DAC_READY_FN(10)
CIRRUS_DAC_READY_FN(11)
CIRRUS_DAC_READY_FN(12)
CIRRUS_DAC_READY_FN(13)
CIRRUS_DAC_READY_FN(14)
CIRRUS_DAC_READY_FN(15)

static bool (*const _cirrus_dac_ready_fn[AUDIO_OUT_MAX_SINKS])(void) = {
    _cirrus_dac_ready_0,  _cirrus_dac_ready_1,  _cirrus_dac_ready_2,  _cirrus_dac_ready_3,
    _cirrus_dac_ready_4,  _cirrus_dac_ready_5,  _cirrus_dac_ready_6,  _cirrus_dac_ready_7,
    _cirrus_dac_ready_8,  _cirrus_dac_ready_9,  _cirrus_dac_ready_10, _cirrus_dac_ready_11,
    _cirrus_dac_ready_12, _cirrus_dac_ready_13, _cirrus_dac_ready_14, _cirrus_dac_ready_15,
};

bool HalCirrusDacBase::buildSink(uint8_t sinkSlot, AudioOutputSink* out) {
    if (!out) return false;
    if (sinkSlot >= AUDIO_OUT_MAX_SINKS) return false;

    *out = AUDIO_OUTPUT_SINK_INIT;
    out->name         = _descriptor.name;
    uint8_t fc = (uint8_t)(sinkSlot * 2);
    if (fc + _descriptor.channelCount > AUDIO_PIPELINE_MATRIX_SIZE) return false;
    out->firstChannel = fc;
    out->channelCount = _descriptor.channelCount;
    out->halSlot      = _slot;
    out->write        = _cirrus_dac_write;
    out->isReady      = _cirrus_dac_ready_fn[sinkSlot];
    out->ctx          = this;

    // Register in static table for isReady/write callback lookup
    _cirrus_dac_slot_dev[sinkSlot] = this;

    return true;
}

#endif // DAC_ENABLED
