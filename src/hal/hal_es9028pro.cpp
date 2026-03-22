#ifdef DAC_ENABLED
// HalEs9028pro — ESS ES9028PRO 8-channel 32-bit DAC implementation
// Register map sourced from src/drivers/es9028pro_regs.h (ESS datasheet).
// Architecture: HyperStream II, 124 dB DNR, 8-channel, PCM up to 768kHz.
// Register-compatible with ES9038PRO; only the chip ID at 0xE1 differs.

#include "hal_es9028pro.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include <Wire.h>
#include "hal_ess_sabre_adc_base.h"  // for extern TwoWire Wire2
#include <Arduino.h>
#include "../debug_serial.h"
#include "../i2s_audio.h"
#include "../drivers/es9028pro_regs.h"
#else
// ===== Native test stubs — no hardware access =====
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)

#define ES9028PRO_I2C_ADDR               0x48
#define ES9028PRO_CHIP_ID                0x02
#define ES9028PRO_REG_SYS_CONFIG         0x00
#define ES9028PRO_REG_INPUT_CFG          0x01
#define ES9028PRO_REG_FILTER_MUTE        0x07
#define ES9028PRO_REG_MASTER_MODE        0x0A
#define ES9028PRO_REG_DPLL_CFG           0x0C
#define ES9028PRO_REG_SOFT_START         0x0E
#define ES9028PRO_REG_VOL_CH1            0x0F
#define ES9028PRO_REG_VOL_CH2            0x10
#define ES9028PRO_REG_VOL_CH3            0x11
#define ES9028PRO_REG_VOL_CH4            0x12
#define ES9028PRO_REG_VOL_CH5            0x13
#define ES9028PRO_REG_VOL_CH6            0x14
#define ES9028PRO_REG_VOL_CH7            0x15
#define ES9028PRO_REG_VOL_CH8            0x16
#define ES9028PRO_REG_CHIP_ID            0xE1
#define ES9028PRO_SOFT_RESET_BIT         0x01
#define ES9028PRO_CHANNEL_MODE_8CH       0x04
#define ES9028PRO_INPUT_TDM              0x0C
#define ES9028PRO_INPUT_32BIT            0x20
#define ES9028PRO_SLAVE_MODE             0x00
#define ES9028PRO_INIT_DPLL_BW           0x05
#define ES9028PRO_FILTER_MASK            0x07
#define ES9028PRO_MUTE_BIT               0x20
#define ES9028PRO_VOL_0DB                0x00
#define ES9028PRO_VOL_MUTE               0xFF

inline bool i2s_audio_enable_expansion_tdm_tx(uint32_t, int, uint8_t) { return true; }
inline void i2s_audio_disable_expansion_tdm_tx() {}
#endif // NATIVE_TEST

// ===== Constructor =====

HalEs9028pro::HalEs9028pro() : HalEssSabreDacBase() {
    hal_init_descriptor(_descriptor, "ess,es9028pro", "ES9028PRO", "ESS Technology",
        HAL_DEV_DAC, 8, ES9028PRO_I2C_ADDR, HAL_BUS_I2C, HAL_I2C_BUS_EXP,
        HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K,
        HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS);
    _initPriority = HAL_PRIORITY_HARDWARE;
    _i2cAddr     = ES9028PRO_I2C_ADDR;
    _sdaPin      = ESS_SABRE_I2C_BUS2_SDA;
    _sclPin      = ESS_SABRE_I2C_BUS2_SCL;
    _i2cBusIndex = HAL_I2C_BUS_EXP;
}

// ===== HalDevice lifecycle =====

bool HalEs9028pro::probe() {
#ifndef NATIVE_TEST
    if (!_wire) return false;
    _wire->beginTransmission(_i2cAddr);
    uint8_t err = _wire->endTransmission();
    if (err != 0) return false;
    uint8_t chipId = _readReg(ES9028PRO_REG_CHIP_ID);
    return (chipId == ES9028PRO_CHIP_ID);
#else
    return true;
#endif
}

HalInitResult HalEs9028pro::init() {
    // ---- 1. Apply per-device config overrides ----
    _applyConfigOverrides();

    LOG_I("[HAL:ES9028PRO] Initializing (I2C addr=0x%02X bus=%u SDA=%d SCL=%d sr=%luHz bits=%u)",
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth);

    // ---- 2. Select TwoWire instance and initialize I2C bus at 400 kHz ----
    _selectWire();

    // ---- 3. Soft reset ----
    _writeReg(ES9028PRO_REG_SYS_CONFIG, ES9028PRO_SOFT_RESET_BIT);
#ifndef NATIVE_TEST
    delay(ESS_SABRE_RESET_DELAY_MS);
#endif

    // ---- 4. Verify chip ID ----
    uint8_t chipId = _readReg(ES9028PRO_REG_CHIP_ID);
    if (chipId != ES9028PRO_CHIP_ID) {
        LOG_W("[HAL:ES9028PRO] Unexpected chip ID: 0x%02X (expected 0x%02X) — continuing",
              chipId, ES9028PRO_CHIP_ID);
    } else {
        LOG_I("[HAL:ES9028PRO] Chip ID OK (0x%02X)", chipId);
    }

    // ---- 5. Configure 8-channel mode ----
    _writeReg(ES9028PRO_REG_SYS_CONFIG, ES9028PRO_CHANNEL_MODE_8CH);

    // ---- 6. TDM input format: 32-bit TDM slave ----
    _writeReg(ES9028PRO_REG_INPUT_CFG, (uint8_t)(ES9028PRO_INPUT_TDM | ES9028PRO_INPUT_32BIT));

    // ---- 7. I2S slave mode ----
    _writeReg(ES9028PRO_REG_MASTER_MODE, ES9028PRO_SLAVE_MODE);

    // ---- 8. DPLL bandwidth ----
    _writeReg(ES9028PRO_REG_DPLL_CFG, ES9028PRO_INIT_DPLL_BW);

    // ---- 9. Soft start ----
    _writeReg(ES9028PRO_REG_SOFT_START, 0x00);

    // ---- 10. Per-channel volume ----
    uint8_t volReg = (uint8_t)((100U - _volume) * 255U / 100U);
    _writeReg(ES9028PRO_REG_VOL_CH1, volReg);
    _writeReg(ES9028PRO_REG_VOL_CH2, volReg);
    _writeReg(ES9028PRO_REG_VOL_CH3, volReg);
    _writeReg(ES9028PRO_REG_VOL_CH4, volReg);
    _writeReg(ES9028PRO_REG_VOL_CH5, volReg);
    _writeReg(ES9028PRO_REG_VOL_CH6, volReg);
    _writeReg(ES9028PRO_REG_VOL_CH7, volReg);
    _writeReg(ES9028PRO_REG_VOL_CH8, volReg);

    // ---- 11. Digital filter preset ----
    uint8_t preset = (_filterPreset > 7) ? 7 : _filterPreset;
    uint8_t filterReg = (uint8_t)(preset & ES9028PRO_FILTER_MASK);
    if (_muted) filterReg |= ES9028PRO_MUTE_BIT;
    _writeReg(ES9028PRO_REG_FILTER_MUTE, filterReg);

    // ---- 12. Enable expansion TDM TX output (8-slot TDM) ----
#ifndef NATIVE_TEST
    gpio_num_t doutPin = (_doutPin >= 0) ? (gpio_num_t)_doutPin : (gpio_num_t)-1;
    bool tdmOk = i2s_audio_enable_expansion_tdm_tx(_sampleRate, doutPin, 8);
    if (!tdmOk) {
        LOG_E("[HAL:ES9028PRO] Expansion TDM TX enable failed");
        _state = HAL_STATE_ERROR;
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "TDM TX init failed");
    }
    _i2sTxEnabled = true;
#else
    _i2sTxEnabled = true;
#endif

    // ---- 13. Init TDM interleaver ----
    if (!_tdm.init(0)) {
        LOG_E("[HAL:ES9028PRO] TDM interleaver init failed — out of memory");
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "TDM interleaver alloc failed");
    }

    // ---- 14. Build 4 AudioOutputSink structs ----
    _tdm.buildSinks("ES9028PRO CH1/2", "ES9028PRO CH3/4",
                    "ES9028PRO CH5/6", "ES9028PRO CH7/8",
                    &_sinks[0], &_sinks[1], &_sinks[2], &_sinks[3],
                    _slot);
    _sinksBuilt = true;

    // ---- 15. Mark device ready ----
    _initialized = true;
    _state = HAL_STATE_AVAILABLE;
    _ready = true;

    LOG_I("[HAL:ES9028PRO] Ready (vol=%u%% muted=%d filter=%u sinks=4)",
          _volume, (int)_muted, _filterPreset);
    return hal_init_ok();
}

void HalEs9028pro::deinit() {
    if (!_initialized) return;

    _ready = false;

#ifndef NATIVE_TEST
    _writeReg(ES9028PRO_REG_FILTER_MUTE, ES9028PRO_MUTE_BIT);
#endif

#ifndef NATIVE_TEST
    if (_i2sTxEnabled) {
        i2s_audio_disable_expansion_tdm_tx();
        _i2sTxEnabled = false;
    }
#else
    _i2sTxEnabled = false;
#endif

    _tdm.deinit();
    _initialized = false;
    _sinksBuilt  = false;
    _state       = HAL_STATE_REMOVED;

    LOG_I("[HAL:ES9028PRO] Deinitialized");
}

void HalEs9028pro::dumpConfig() {
    LOG_I("[HAL:ES9028PRO] %s by %s (compat=%s) i2c=0x%02X bus=%u sda=%d scl=%d "
          "sr=%luHz bits=%u vol=%u%% muted=%d filter=%u",
          _descriptor.name, _descriptor.manufacturer, _descriptor.compatible,
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth,
          _volume, (int)_muted, _filterPreset);
}

bool HalEs9028pro::healthCheck() {
#ifndef NATIVE_TEST
    if (!_initialized) return false;
    uint8_t id = _readReg(ES9028PRO_REG_CHIP_ID);
    return (id == ES9028PRO_CHIP_ID);
#else
    return _initialized;
#endif
}

// ===== HalAudioDevice =====

bool HalEs9028pro::configure(uint32_t sampleRate, uint8_t bitDepth) {
    const uint32_t supported[] = { 44100, 48000, 96000, 192000, 384000, 768000 };
    if (!_validateSampleRate(sampleRate, supported, 6)) {
        LOG_W("[HAL:ES9028PRO] Unsupported sample rate: %luHz", (unsigned long)sampleRate);
        return false;
    }
    if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32) {
        LOG_W("[HAL:ES9028PRO] Unsupported bit depth: %u", bitDepth);
        return false;
    }
    _sampleRate = sampleRate;
    _bitDepth   = bitDepth;
    LOG_I("[HAL:ES9028PRO] Configured: %luHz %ubit",
          (unsigned long)sampleRate, bitDepth);
    return true;
}

bool HalEs9028pro::setVolume(uint8_t percent) {
    if (!_initialized) return false;
    if (percent > 100) percent = 100;

    uint8_t volReg = (uint8_t)((100U - percent) * 255U / 100U);
    bool ok = _writeReg(ES9028PRO_REG_VOL_CH1, volReg);
    ok = ok && _writeReg(ES9028PRO_REG_VOL_CH2, volReg);
    ok = ok && _writeReg(ES9028PRO_REG_VOL_CH3, volReg);
    ok = ok && _writeReg(ES9028PRO_REG_VOL_CH4, volReg);
    ok = ok && _writeReg(ES9028PRO_REG_VOL_CH5, volReg);
    ok = ok && _writeReg(ES9028PRO_REG_VOL_CH6, volReg);
    ok = ok && _writeReg(ES9028PRO_REG_VOL_CH7, volReg);
    ok = ok && _writeReg(ES9028PRO_REG_VOL_CH8, volReg);

    _volume = percent;
    LOG_D("[HAL:ES9028PRO] Volume: %d%% -> reg=0x%02X", percent, volReg);
    return ok;
}

bool HalEs9028pro::setMute(bool mute) {
    if (!_initialized) return false;

    uint8_t reg = _readReg(ES9028PRO_REG_FILTER_MUTE);
    if (mute) {
        reg |=  ES9028PRO_MUTE_BIT;
    } else {
        reg &= (uint8_t)~ES9028PRO_MUTE_BIT;
    }
    bool ok = _writeReg(ES9028PRO_REG_FILTER_MUTE, reg);
    _muted = mute;
    LOG_I("[HAL:ES9028PRO] %s", mute ? "Muted" : "Unmuted");
    return ok;
}

bool HalEs9028pro::setFilterPreset(uint8_t preset) {
    if (preset >= ESS_SABRE_FILTER_COUNT) {
        LOG_W("[HAL:ES9028PRO] Invalid filter preset: %u (max %u)",
              preset, ESS_SABRE_FILTER_COUNT - 1);
        return false;
    }

    uint8_t reg = _initialized ? _readReg(ES9028PRO_REG_FILTER_MUTE) : 0x00;
    reg &= (uint8_t)~ES9028PRO_FILTER_MASK;
    reg |= (uint8_t)(preset & ES9028PRO_FILTER_MASK);

    bool ok = true;
    if (_initialized) {
        ok = _writeReg(ES9028PRO_REG_FILTER_MUTE, reg);
    }
    _filterPreset = preset;
    LOG_I("[HAL:ES9028PRO] Filter preset: %u", preset);
    return ok;
}

// ===== Multi-sink interface =====

bool HalEs9028pro::buildSinkAt(int idx, uint8_t sinkSlot, AudioOutputSink* out) {
    if (!out) return false;
    if (idx < 0 || idx >= 4 || !_sinksBuilt) return false;
    *out = _sinks[idx];
    out->halSlot = _slot;
    (void)sinkSlot;
    return true;
}

#endif // DAC_ENABLED
