#ifdef DAC_ENABLED
// HalEssSabreDacBase -- shared I2C helpers, config override reading, buildSink()
// infrastructure, and expansion I2S TX lifecycle for ESS SABRE DAC family.

#include "hal_ess_sabre_dac_base.h"
#include "hal_device_manager.h"
#include "../i2s_audio.h"
#include "../audio_pipeline.h"
#include "../sink_write_utils.h"

#ifndef NATIVE_TEST
#include <Wire.h>
#include <Arduino.h>
#include "../debug_serial.h"
// Wire2 is defined in hal_ess_sabre_adc_base.cpp — we extern it in the header
#else
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)
#endif

// ===== I2C helpers =====

bool HalEssSabreDacBase::_writeReg(uint8_t reg, uint8_t val) {
#ifndef NATIVE_TEST
    if (!_wire) return false;
    _wire->beginTransmission(_i2cAddr);
    _wire->write(reg);
    _wire->write(val);
    uint8_t err = _wire->endTransmission();
    if (err != 0) {
        LOG_E("[HAL:ESS-DAC] I2C write failed: reg=0x%02X val=0x%02X err=%d", reg, val, err);
        return false;
    }
    return true;
#else
    (void)reg; (void)val;
    return true;
#endif
}

uint8_t HalEssSabreDacBase::_readReg(uint8_t reg) {
#ifndef NATIVE_TEST
    if (!_wire) return 0xFF;
    _wire->beginTransmission(_i2cAddr);
    _wire->write(reg);
    _wire->endTransmission(false);
    _wire->requestFrom(_i2cAddr, (uint8_t)1);
    if (_wire->available()) return _wire->read();
    LOG_E("[HAL:ESS-DAC] I2C read failed: reg=0x%02X", reg);
    return 0xFF;
#else
    (void)reg;
    return 0x00;
#endif
}

bool HalEssSabreDacBase::_writeReg16(uint8_t regLsb, uint16_t val) {
    bool ok = _writeReg(regLsb,     (uint8_t)(val & 0xFF));
    ok      = ok && _writeReg(regLsb + 1, (uint8_t)((val >> 8) & 0xFF));
    return ok;
}

// ===== Config override reading =====

void HalEssSabreDacBase::_applyConfigOverrides() {
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

void HalEssSabreDacBase::_selectWire() {
#ifndef NATIVE_TEST
    switch (_i2cBusIndex) {
        case 0:  _wire = &Wire;  break;
        case 1:  _wire = &Wire1; break;
        case 2:  _wire = &Wire2; break;
        default: _wire = &Wire2; break;
    }
    _wire->begin((int)_sdaPin, (int)_sclPin, (uint32_t)ESS_SABRE_I2C_FREQ_HZ);
#endif
}

// ===== Sample rate validation =====

bool HalEssSabreDacBase::_validateSampleRate(uint32_t hz, const uint32_t* supported, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        if (hz == supported[i]) return true;
    }
    return false;
}

// ===== I2S TX lifecycle =====

bool HalEssSabreDacBase::_enableI2sTx() {
#ifndef NATIVE_TEST
    gpio_num_t dout = (_doutPin >= 0) ? (gpio_num_t)_doutPin : (gpio_num_t)-1;
    if (!i2s_audio_enable_expansion_tx(_sampleRate, dout)) {
        LOG_E("[HAL:ESS-DAC] Expansion TX enable failed (sr=%lu dout=%d)",
              (unsigned long)_sampleRate, _doutPin);
        return false;
    }
    _i2sTxEnabled = true;
    return true;
#else
    _i2sTxEnabled = true;
    return true;
#endif
}

void HalEssSabreDacBase::_disableI2sTx() {
    if (_i2sTxEnabled) {
        i2s_audio_disable_expansion_tx();
        _i2sTxEnabled = false;
    }
}

// ===== DAC interface defaults =====

bool HalEssSabreDacBase::dacSetSampleRate(uint32_t hz) {
    return configure(hz, _bitDepth);
}

bool HalEssSabreDacBase::dacSetBitDepth(uint8_t bits) {
    return configure(_sampleRate, bits);
}

bool HalEssSabreDacBase::setFilterPreset(uint8_t preset) {
    if (preset >= ESS_SABRE_FILTER_COUNT) return false;
    _filterPreset = preset;
    return true;
}

// ===== buildSink() infrastructure =====

// Static device-pointer table indexed by sink slot. Required because
// isReady callback signature is bool(*)(void) — no context parameter.
static HalEssSabreDacBase* _ess_dac_slot_dev[AUDIO_OUT_MAX_SINKS] = {};

// Write callback — converts pipeline int32 to float, applies volume/mute ramp,
// converts back to int32, writes to expansion I2S TX.
static void _ess_dac_write(const int32_t* buf, int stereoFrames) {
    if (!buf || stereoFrames <= 0) return;

#ifndef NATIVE_TEST
    int totalSamples = stereoFrames * 2;
    float fBuf[512];
    int32_t txBuf[512];
    const int32_t* src = buf;
    int remaining = totalSamples;

    // Find the device for mute ramp state — search the slot table
    HalEssSabreDacBase* dev = nullptr;
    uint8_t sinkSlot = 0;
    for (uint8_t s = 0; s < AUDIO_OUT_MAX_SINKS; s++) {
        if (_ess_dac_slot_dev[s]) {
            dev = _ess_dac_slot_dev[s];
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

        // Write to expansion I2S TX
        size_t bytesWritten = 0;
        i2s_audio_write_expansion_tx(txBuf, (size_t)chunk * sizeof(int32_t), &bytesWritten, 20);

        src += chunk;
        remaining -= chunk;
    }
#else
    (void)stereoFrames;
#endif
}

// isReady callback template — looks up device via static table
#define ESS_DAC_READY_FN(N) \
    static bool _ess_dac_ready_##N(void) { \
        return _ess_dac_slot_dev[N] && _ess_dac_slot_dev[N]->_ready; \
    }

ESS_DAC_READY_FN(0)
ESS_DAC_READY_FN(1)
ESS_DAC_READY_FN(2)
ESS_DAC_READY_FN(3)
ESS_DAC_READY_FN(4)
ESS_DAC_READY_FN(5)
ESS_DAC_READY_FN(6)
ESS_DAC_READY_FN(7)

static bool (*const _ess_dac_ready_fn[AUDIO_OUT_MAX_SINKS])(void) = {
    _ess_dac_ready_0, _ess_dac_ready_1, _ess_dac_ready_2, _ess_dac_ready_3,
    _ess_dac_ready_4, _ess_dac_ready_5, _ess_dac_ready_6, _ess_dac_ready_7,
};

bool HalEssSabreDacBase::buildSink(uint8_t sinkSlot, AudioOutputSink* out) {
    if (!out) return false;
    if (sinkSlot >= AUDIO_OUT_MAX_SINKS) return false;

    *out = AUDIO_OUTPUT_SINK_INIT;
    out->name         = _descriptor.name;
    uint8_t fc = (uint8_t)(sinkSlot * 2);
    if (fc + _descriptor.channelCount > AUDIO_PIPELINE_MATRIX_SIZE) return false;
    out->firstChannel = fc;
    out->channelCount = _descriptor.channelCount;
    out->halSlot      = _slot;
    out->write        = _ess_dac_write;
    out->isReady      = _ess_dac_ready_fn[sinkSlot];
    out->ctx          = this;

    // Register in static table for isReady/write callback lookup
    _ess_dac_slot_dev[sinkSlot] = this;

    return true;
}

#endif // DAC_ENABLED
