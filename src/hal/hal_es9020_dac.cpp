#ifdef DAC_ENABLED
// HalEs9020Dac — ESS ES9020 2-channel 32-bit DAC implementation
// Register map sourced from src/drivers/es9020_dac_regs.h.
// Architecture: Hyperstream IV, 122 dB DNR, integrated APLL for BCK clock recovery.
// I2S input slave, 2-channel stereo, 8-bit attenuation (0.5 dB/step).

#include "hal_es9020_dac.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include <Wire.h>
#include <Arduino.h>
#include "../debug_serial.h"
#include "../drivers/es9020_dac_regs.h"
#else
// ===== Native test stubs — no hardware access =====
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)

// Register address constants (mirrors es9020_dac_regs.h for native compilation;
// all actual hardware writes are inside #ifndef NATIVE_TEST blocks below).
#define ES9020_DAC_I2C_ADDR           0x48
#define ES9020_CHIP_ID                0x86
#define ES9020_REG_SOFT_RESET         0x00
#define ES9020_REG_INPUT_CONFIG       0x01
#define ES9020_REG_FILTER             0x07
#define ES9020_REG_APLL_CTRL          0x0C
#define ES9020_REG_CLK_SOURCE         0x0D
#define ES9020_REG_VOLUME             0x0F
#define ES9020_REG_CHIP_ID            0xE1
#define ES9020_SOFT_RESET_BIT         0x80
#define ES9020_TDM_SLOTS_2            0x00
#define ES9020_FILTER_SHAPE_MASK      0x07
#define ES9020_APLL_ENABLE_BIT        0x01
#define ES9020_APLL_LOCK_BIT          0x10
#define ES9020_CLK_BCK_RECOVERY       0x00
#define ES9020_CLK_MCLK               0x02
#define ES9020_VOL_0DB                0x00
#define ES9020_VOL_MUTE               0xFF
#endif // NATIVE_TEST

// ===== Constructor =====

HalEs9020Dac::HalEs9020Dac() : HalEssSabreDacBase() {
    hal_init_descriptor(_descriptor, "ess,es9020-dac", "ES9020", "ESS Technology",
        HAL_DEV_DAC, 2, ESS_SABRE_DAC_I2C_ADDR_BASE, HAL_BUS_I2C, HAL_I2C_BUS_EXP,
        HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K,
        HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS | HAL_CAP_APLL);
    _initPriority = HAL_PRIORITY_HARDWARE;
}

// ===== HalDevice lifecycle =====

bool HalEs9020Dac::probe() {
#ifndef NATIVE_TEST
    if (!_wire) return false;
    _wire->beginTransmission(_i2cAddr);
    uint8_t err = _wire->endTransmission();
    if (err != 0) return false;
    uint8_t chipId = _readReg(ES9020_REG_CHIP_ID);
    return (chipId == ES9020_CHIP_ID);
#else
    return true;
#endif
}

HalInitResult HalEs9020Dac::init() {
    // ---- 1. Apply per-device config overrides from HAL Device Manager ----
    _applyConfigOverrides();

    LOG_I("[HAL:ES9020] Initializing (I2C addr=0x%02X bus=%u SDA=%d SCL=%d sr=%luHz bits=%u vol=%u mute=%d)",
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth, _volume, (int)_muted);

#ifndef NATIVE_TEST
    // ---- 2. Select TwoWire instance and initialise I2C bus at 400 kHz ----
    _selectWire();
    LOG_I("[HAL:ES9020] I2C initialized (bus %u SDA=%d SCL=%d 400kHz)",
          _i2cBusIndex, _sdaPin, _sclPin);
#endif

    // ---- 3. Verify chip ID (read reg 0xE1, expect 0x86) ----
    uint8_t chipId = _readReg(ES9020_REG_CHIP_ID);
    if (chipId != ES9020_CHIP_ID) {
        LOG_W("[HAL:ES9020] Unexpected chip ID: 0x%02X (expected 0x%02X) — continuing",
              chipId, ES9020_CHIP_ID);
    } else {
        LOG_I("[HAL:ES9020] Chip ID OK (0x%02X)", chipId);
    }

    // ---- 4. Soft reset (reg 0x00 bit7, self-clearing) ----
    _writeReg(ES9020_REG_SOFT_RESET, ES9020_SOFT_RESET_BIT);
#ifndef NATIVE_TEST
    delay(ESS_SABRE_RESET_DELAY_MS);   // Allow reset to complete
#endif

    // ---- 5. Input format: standard 2-slot I2S (stereo) ----
    // REG_INPUT_CONFIG (0x01): TDM_SLOTS=0b00 (2 slots), TDM_ENABLE=0 (off for plain I2S)
    _writeReg(ES9020_REG_INPUT_CONFIG, ES9020_TDM_SLOTS_2);

    // ---- 6. Clock source: external MCLK (APLL off by default) ----
    // setApllEnabled() can switch this to BCK recovery after init
    _writeReg(ES9020_REG_CLK_SOURCE, ES9020_CLK_MCLK);

    // ---- 7. Digital filter preset (bits[2:0] in REG_FILTER) ----
    uint8_t preset = (_filterPreset > 7) ? 7 : _filterPreset;
    _writeReg(ES9020_REG_FILTER, (uint8_t)(preset & ES9020_FILTER_SHAPE_MASK));

    // ---- 8. Initial volume (8-bit attenuation, 0.5 dB/step, 0x00 = 0 dB) ----
    // Map 0-100% linearly to attenuation steps: 100% → 0x00, 0% → 0x7F (63.5 dB atten)
    // Full mute (0xFF) is applied separately via setMute().
    if (_muted) {
        _writeReg(ES9020_REG_VOLUME, ES9020_VOL_MUTE);
    } else {
        uint8_t attenReg = (uint8_t)(((uint16_t)(100 - _volume) * ESS_SABRE_DAC_VOL_STEPS) / 100);
        _writeReg(ES9020_REG_VOLUME, attenReg);
    }

    // ---- 9. Enable I2S TX expansion output ----
    if (!_enableI2sTx()) {
        LOG_E("[HAL:ES9020] Failed to enable expansion I2S TX");
        _state = HAL_STATE_ERROR;
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "I2S TX enable failed");
    }

    // ---- 10. Mark device ready ----
    _initialized = true;
    _state = HAL_STATE_AVAILABLE;
    _ready = true;

    LOG_I("[HAL:ES9020] Ready (filter=%u vol=%u mute=%d apll=off)",
          _filterPreset, _volume, (int)_muted);
    return hal_init_ok();
}

void HalEs9020Dac::deinit() {
    if (!_initialized) return;

    _ready = false;

    // Mute output before tearing down
    _writeReg(ES9020_REG_VOLUME, ES9020_VOL_MUTE);

    _disableI2sTx();

    _initialized = false;
    _state = HAL_STATE_REMOVED;

    LOG_I("[HAL:ES9020] Deinitialized");
}

void HalEs9020Dac::dumpConfig() {
    LOG_I("[HAL:ES9020] %s by %s (compat=%s) i2c=0x%02X bus=%u sda=%d scl=%d "
          "sr=%luHz bits=%u vol=%u mute=%d filter=%u apll=%d",
          _descriptor.name, _descriptor.manufacturer, _descriptor.compatible,
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth, _volume, (int)_muted, _filterPreset,
          (int)_i2sTxEnabled);
}

bool HalEs9020Dac::healthCheck() {
#ifndef NATIVE_TEST
    if (!_wire || !_initialized) return false;
    uint8_t id = _readReg(ES9020_REG_CHIP_ID);
    return (id == ES9020_CHIP_ID);
#else
    return _initialized;
#endif
}

// ===== HalAudioDevice =====

bool HalEs9020Dac::configure(uint32_t sampleRate, uint8_t bitDepth) {
    static const uint32_t kSupported[] = { 44100, 48000, 96000, 192000 };
    if (!_validateSampleRate(sampleRate, kSupported, 4)) {
        LOG_W("[HAL:ES9020] Unsupported sample rate: %luHz", (unsigned long)sampleRate);
        return false;
    }
    _sampleRate = sampleRate;
    _bitDepth   = bitDepth;
    LOG_I("[HAL:ES9020] Configured: %luHz %ubit", (unsigned long)sampleRate, bitDepth);
    return true;
}

bool HalEs9020Dac::setVolume(uint8_t percent) {
    if (!_initialized) return false;
    if (percent > 100) percent = 100;
    _volume = percent;
    if (_muted) return true;  // Defer register write; mute holds full attenuation
    // Map 0% → 0x7F (63.5 dB atten), 100% → 0x00 (0 dB)
    // Using 128-step range (ESS_SABRE_DAC_VOL_STEPS) mapped over 0-100%
    uint8_t attenReg = (uint8_t)(((uint16_t)(100 - percent) * ESS_SABRE_DAC_VOL_STEPS) / 100);
    bool ok = _writeReg(ES9020_REG_VOLUME, attenReg);
    LOG_D("[HAL:ES9020] Volume: %d%% -> attenReg=0x%02X", percent, attenReg);
    return ok;
}

bool HalEs9020Dac::setMute(bool mute) {
    if (!_initialized) return false;
    _muted = mute;
    if (mute) {
        bool ok = _writeReg(ES9020_REG_VOLUME, ES9020_VOL_MUTE);
        LOG_I("[HAL:ES9020] Muted");
        return ok;
    } else {
        // Restore current volume level
        uint8_t attenReg = (uint8_t)(((uint16_t)(100 - _volume) * ESS_SABRE_DAC_VOL_STEPS) / 100);
        bool ok = _writeReg(ES9020_REG_VOLUME, attenReg);
        LOG_I("[HAL:ES9020] Unmuted (vol=%u attenReg=0x%02X)", _volume, attenReg);
        return ok;
    }
}

// ===== Filter preset override =====

bool HalEs9020Dac::setFilterPreset(uint8_t preset) {
    if (preset >= ESS_SABRE_FILTER_COUNT) return false;
    if (_initialized) {
        // REG_FILTER (0x07): bits[2:0] = FILTER_SHAPE
        bool ok = _writeReg(ES9020_REG_FILTER, (uint8_t)(preset & ES9020_FILTER_SHAPE_MASK));
        if (!ok) return false;
    }
    _filterPreset = preset;
    LOG_I("[HAL:ES9020] Filter preset: %u", preset);
    return true;
}

// ===== ES9020-specific: APLL clock recovery =====

bool HalEs9020Dac::setApllEnabled(bool enable) {
    if (!_initialized) return false;

    // Select clock source before toggling APLL
    uint8_t clkSrc = enable ? ES9020_CLK_BCK_RECOVERY : ES9020_CLK_MCLK;
    if (!_writeReg(ES9020_REG_CLK_SOURCE, clkSrc)) return false;

    // Write APLL enable bit in REG_APLL_CTRL (0x0C)
    uint8_t apllCtrl = enable ? ES9020_APLL_ENABLE_BIT : 0x00;
    bool ok = _writeReg(ES9020_REG_APLL_CTRL, apllCtrl);

    LOG_I("[HAL:ES9020] APLL %s (clkSrc=0x%02X)", enable ? "enabled" : "disabled", clkSrc);
    return ok;
}

bool HalEs9020Dac::isApllLocked() const {
    if (!_initialized) return false;
    // APLL_LOCK_STATUS is bit4 of REG_APLL_CTRL (0x0C), read-only
    uint8_t val = const_cast<HalEs9020Dac*>(this)->_readReg(ES9020_REG_APLL_CTRL);
    return (val & ES9020_APLL_LOCK_BIT) != 0;
}

#endif // DAC_ENABLED
