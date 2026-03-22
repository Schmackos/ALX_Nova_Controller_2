#ifdef DAC_ENABLED
// HalEs9039q2m — ESS ES9039Q2M 2-channel 32-bit DAC implementation
// Register map sourced from src/drivers/es9039q2m_regs.h (ESS datasheet rev 1.0).
// Architecture: Hyperstream IV (130dB DNR), PCM up to 768kHz, DSD1024.
// Filter presets 6 and 7 are Hyperstream IV hybrid modes (enhanced phase linearity).

#include "hal_es9039q2m.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include <Wire.h>
#include "hal_ess_sabre_adc_base.h"  // for extern TwoWire Wire2
#include <Arduino.h>
#include "../debug_serial.h"
#include "../drivers/es9039q2m_regs.h"
#else
// ===== Native test stubs — no hardware access =====
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)

// Register address constants (mirrors es9039q2m_regs.h for native compilation;
// all actual hardware writes are inside #ifndef NATIVE_TEST blocks below).
#define ES9039Q2M_I2C_ADDR               0x48
#define ES9039Q2M_CHIP_ID                0x92
#define ES9039Q2M_REG_SYS_CONFIG         0x00
#define ES9039Q2M_REG_INPUT_CFG          0x01
#define ES9039Q2M_REG_AUTOMUTE_CFG       0x04
#define ES9039Q2M_REG_FILTER_MUTE        0x07
#define ES9039Q2M_REG_GPIO_CFG1          0x08
#define ES9039Q2M_REG_GPIO_CTRL          0x09
#define ES9039Q2M_REG_MASTER_MODE        0x0A
#define ES9039Q2M_REG_DPLL_CFG           0x0B
#define ES9039Q2M_REG_CLOCK_GEAR         0x0D
#define ES9039Q2M_REG_VOL_CH1            0x0F
#define ES9039Q2M_REG_VOL_CH2            0x10
#define ES9039Q2M_REG_CHIP_ID            0xE1
#define ES9039Q2M_SOFT_RESET_BIT         0x01
#define ES9039Q2M_I2S_LEN_32BIT          0x03
#define ES9039Q2M_I2S_FMT_PHILIPS        0x00
#define ES9039Q2M_SLAVE_MODE             0x00
#define ES9039Q2M_INIT_DPLL_BW           0x05
#define ES9039Q2M_CLK_GEAR_1X            0x00
#define ES9039Q2M_CLK_GEAR_2X            0x01
#define ES9039Q2M_CLK_GEAR_4X            0x02
#define ES9039Q2M_MUTE_BIT               0x01
#define ES9039Q2M_FILTER_SHIFT           2
#define ES9039Q2M_FILTER_MASK            0x1C
#define ES9039Q2M_VOL_0DB                0x00
#define ES9039Q2M_VOL_MUTE               0xFF
#define ES9039Q2M_INIT_INPUT_CFG         (ES9039Q2M_I2S_LEN_32BIT | ES9039Q2M_I2S_FMT_PHILIPS)

// I2S TX stubs for native (provided by HalEssSabreDacBase in real build)
inline bool i2s_audio_enable_expansion_tx(uint32_t, int) { return true; }
inline void i2s_audio_disable_expansion_tx(void) {}
inline void i2s_audio_write_expansion_tx(const void*, size_t, size_t*, uint32_t) {}
#endif // NATIVE_TEST

// ===== Constructor =====

HalEs9039q2m::HalEs9039q2m() : HalEssSabreDacBase() {
    hal_init_descriptor(_descriptor, "ess,es9039q2m", "ES9039Q2M", "ESS Technology",
        HAL_DEV_DAC, 2, ES9039Q2M_I2C_ADDR, HAL_BUS_I2C, HAL_I2C_BUS_EXP,
        HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K,
        HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS);
    _initPriority = HAL_PRIORITY_HARDWARE;
    _i2cAddr     = ES9039Q2M_I2C_ADDR;
    _sdaPin      = ESS_SABRE_I2C_BUS2_SDA;
    _sclPin      = ESS_SABRE_I2C_BUS2_SCL;
    _i2cBusIndex = HAL_I2C_BUS_EXP;
}

// ===== HalDevice lifecycle =====

bool HalEs9039q2m::probe() {
#ifndef NATIVE_TEST
    if (!_wire) return false;
    _wire->beginTransmission(_i2cAddr);
    uint8_t err = _wire->endTransmission();
    if (err != 0) return false;
    uint8_t chipId = _readReg(ES9039Q2M_REG_CHIP_ID);
    return (chipId == ES9039Q2M_CHIP_ID);
#else
    return true;
#endif
}

HalInitResult HalEs9039q2m::init() {
    // ---- 1. Apply per-device config overrides from HAL Device Manager ----
    _applyConfigOverrides();

    LOG_I("[HAL:ES9039Q2M] Initializing (I2C addr=0x%02X bus=%u SDA=%d SCL=%d sr=%luHz bits=%u)",
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth);

    // ---- 2. Select TwoWire instance and initialize I2C bus at 400 kHz ----
    _selectWire();

    // ---- 3. Verify chip ID (read reg 0xE1, expect 0x92) ----
    uint8_t chipId = _readReg(ES9039Q2M_REG_CHIP_ID);
    if (chipId != ES9039Q2M_CHIP_ID) {
        LOG_W("[HAL:ES9039Q2M] Unexpected chip ID: 0x%02X (expected 0x%02X) — continuing",
              chipId, ES9039Q2M_CHIP_ID);
    } else {
        LOG_I("[HAL:ES9039Q2M] Chip ID OK (0x%02X)", chipId);
    }

    // ---- 4. Soft reset (reg 0x00 bit0, self-clearing) ----
    _writeReg(ES9039Q2M_REG_SYS_CONFIG, ES9039Q2M_SOFT_RESET_BIT);
#ifndef NATIVE_TEST
    delay(ESS_SABRE_RESET_DELAY_MS);
#endif

    // ---- 5. I2S input format: 32-bit Philips/I2S, slave mode ----
    _writeReg(ES9039Q2M_REG_INPUT_CFG, ES9039Q2M_INIT_INPUT_CFG);

    // ---- 6. I2S slave mode (ESP32-P4 is I2S master providing BCK/WS/MCLK) ----
    _writeReg(ES9039Q2M_REG_MASTER_MODE, ES9039Q2M_SLAVE_MODE);

    // ---- 7. DPLL bandwidth — tighter than ES9038Q2M for Hyperstream IV stability ----
    _writeReg(ES9039Q2M_REG_DPLL_CFG, ES9039Q2M_INIT_DPLL_BW);

    // ---- 8. Clock gear based on sample rate ----
    uint8_t clkGear = ES9039Q2M_CLK_GEAR_1X;
    if (_sampleRate > 384000) {
        clkGear = ES9039Q2M_CLK_GEAR_4X;
    } else if (_sampleRate > 192000) {
        clkGear = ES9039Q2M_CLK_GEAR_2X;
    }
    _writeReg(ES9039Q2M_REG_CLOCK_GEAR, clkGear);

    // ---- 9. Volume: apply stored level (0-100% → 0x00-0xFF attenuation) ----
    uint8_t volReg = (uint8_t)((100U - _volume) * 255U / 100U);
    _writeReg(ES9039Q2M_REG_VOL_CH1, volReg);
    _writeReg(ES9039Q2M_REG_VOL_CH2, volReg);

    // ---- 10. Digital filter preset (bits[4:2] of reg 0x07) ----
    uint8_t preset = (_filterPreset > 7) ? 7 : _filterPreset;
    uint8_t filterMuteReg = (uint8_t)((preset & 0x07) << ES9039Q2M_FILTER_SHIFT);
    if (_muted) filterMuteReg |= ES9039Q2M_MUTE_BIT;
    _writeReg(ES9039Q2M_REG_FILTER_MUTE, filterMuteReg);

    // ---- 11. Enable expansion I2S TX output ----
    if (!_enableI2sTx()) {
        LOG_E("[HAL:ES9039Q2M] Expansion I2S TX enable failed");
        _state = HAL_STATE_ERROR;
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "I2S TX init failed");
    }

    // ---- 12. Mark device ready ----
    _initialized = true;
    _state = HAL_STATE_AVAILABLE;
    _ready = true;

    LOG_I("[HAL:ES9039Q2M] Ready (vol=%u%% muted=%d filter=%u clkGear=0x%02X)",
          _volume, (int)_muted, _filterPreset, clkGear);
    return hal_init_ok();
}

void HalEs9039q2m::deinit() {
    if (!_initialized) return;

    _ready = false;

    // Disable I2S TX and silence the DAC before removal
    _disableI2sTx();

#ifndef NATIVE_TEST
    // Mute output (both channels) before shutdown
    _writeReg(ES9039Q2M_REG_FILTER_MUTE, ES9039Q2M_MUTE_BIT);
#endif

    _initialized  = false;
    _i2sTxEnabled = false;
    _state = HAL_STATE_REMOVED;

    LOG_I("[HAL:ES9039Q2M] Deinitialized");
}

void HalEs9039q2m::dumpConfig() {
    LOG_I("[HAL:ES9039Q2M] %s by %s (compat=%s) i2c=0x%02X bus=%u sda=%d scl=%d "
          "sr=%luHz bits=%u vol=%u%% muted=%d filter=%u",
          _descriptor.name, _descriptor.manufacturer, _descriptor.compatible,
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth,
          _volume, (int)_muted, _filterPreset);
}

bool HalEs9039q2m::healthCheck() {
#ifndef NATIVE_TEST
    if (!_initialized) return false;
    uint8_t id = _readReg(ES9039Q2M_REG_CHIP_ID);
    return (id == ES9039Q2M_CHIP_ID);
#else
    return _initialized;
#endif
}

// ===== HalAudioDevice =====

bool HalEs9039q2m::configure(uint32_t sampleRate, uint8_t bitDepth) {
    const uint32_t supported[] = { 44100, 48000, 96000, 192000, 384000, 768000 };
    if (!_validateSampleRate(sampleRate, supported, 6)) {
        LOG_W("[HAL:ES9039Q2M] Unsupported sample rate: %luHz", (unsigned long)sampleRate);
        return false;
    }
    if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32) {
        LOG_W("[HAL:ES9039Q2M] Unsupported bit depth: %u", bitDepth);
        return false;
    }

    _sampleRate = sampleRate;
    _bitDepth   = bitDepth;

    // Update clock gear register if already initialized
    if (_initialized) {
        uint8_t clkGear = ES9039Q2M_CLK_GEAR_1X;
        if (sampleRate > 384000) {
            clkGear = ES9039Q2M_CLK_GEAR_4X;
        } else if (sampleRate > 192000) {
            clkGear = ES9039Q2M_CLK_GEAR_2X;
        }
        _writeReg(ES9039Q2M_REG_CLOCK_GEAR, clkGear);
    }

    LOG_I("[HAL:ES9039Q2M] Configured: %luHz %ubit",
          (unsigned long)sampleRate, bitDepth);
    return true;
}

bool HalEs9039q2m::setVolume(uint8_t percent) {
    if (!_initialized) return false;
    if (percent > 100) percent = 100;

    // Map 100% → 0x00 (0 dB, no attenuation), 0% → 0xFF (full mute / max attenuation)
    uint8_t volReg = (uint8_t)((100U - percent) * 255U / 100U);
    bool ok  = _writeReg(ES9039Q2M_REG_VOL_CH1, volReg);
    ok       = ok && _writeReg(ES9039Q2M_REG_VOL_CH2, volReg);

    _volume = percent;
    LOG_D("[HAL:ES9039Q2M] Volume: %d%% -> reg=0x%02X", percent, volReg);
    return ok;
}

bool HalEs9039q2m::setMute(bool mute) {
    if (!_initialized) return false;

    // Read-modify-write to preserve filter preset in bits[4:2]
    uint8_t reg = _readReg(ES9039Q2M_REG_FILTER_MUTE);
    if (mute) {
        reg |=  ES9039Q2M_MUTE_BIT;
    } else {
        reg &= (uint8_t)~ES9039Q2M_MUTE_BIT;
    }
    bool ok = _writeReg(ES9039Q2M_REG_FILTER_MUTE, reg);

    _muted = mute;
    LOG_I("[HAL:ES9039Q2M] %s", mute ? "Muted" : "Unmuted");
    return ok;
}

// ===== Filter preset =====

bool HalEs9039q2m::setFilterPreset(uint8_t preset) {
    if (preset >= ESS_SABRE_FILTER_COUNT) {
        LOG_W("[HAL:ES9039Q2M] Invalid filter preset: %u (max %u)",
              preset, ESS_SABRE_FILTER_COUNT - 1);
        return false;
    }

    // Preserve mute bit (bit0) when updating filter (bits[4:2])
    uint8_t reg = _initialized ? _readReg(ES9039Q2M_REG_FILTER_MUTE) : 0x00;
    reg &= (uint8_t)~ES9039Q2M_FILTER_MASK;
    reg |= (uint8_t)((preset & 0x07) << ES9039Q2M_FILTER_SHIFT);

    bool ok = true;
    if (_initialized) {
        ok = _writeReg(ES9039Q2M_REG_FILTER_MUTE, reg);
    }

    _filterPreset = preset;
    LOG_I("[HAL:ES9039Q2M] Filter preset: %u", preset);
    return ok;
}

#endif // DAC_ENABLED
