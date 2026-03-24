#ifdef DAC_ENABLED
// HalEs9033Q — ESS ES9033Q 2-channel DAC with integrated 2Vrms ground-centered line drivers.
// Register map sourced from src/drivers/es9033q_regs.h.
// Inherits shared I2C helpers, config override reading, buildSink() infrastructure,
// and I2S TX lifecycle management from HalEssSabreDacBase.

#include "hal_es9033q.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include <Arduino.h>
#include "../debug_serial.h"
#include "../drivers/es9033q_regs.h"
#else
// ===== Native test stubs — no hardware access =====
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)

// Register address constants (mirrors es9033q_regs.h for native compilation;
// all actual hardware writes are inside #ifndef NATIVE_TEST blocks below).
#define ES9033Q_I2C_ADDR              0x48
#define ES9033Q_CHIP_ID               0x88
#define ES9033Q_REG_SYSTEM_SETTINGS   0x00
#define ES9033Q_REG_INPUT_CONFIG      0x01
#define ES9033Q_REG_DSD_CONFIG        0x06
#define ES9033Q_REG_FILTER_SHAPE      0x07
#define ES9033Q_REG_GENERAL_CONFIG    0x08
#define ES9033Q_REG_DPLL_BANDWIDTH    0x0C
#define ES9033Q_REG_VOLUME_L          0x0F
#define ES9033Q_REG_VOLUME_R          0x10
#define ES9033Q_REG_LINE_DRIVER       0x14
#define ES9033Q_REG_CHIP_ID           0xE1
#define ES9033Q_REG_DPLL_LOCK         0xE2
#define ES9033Q_INPUT_I2S             0x00
#define ES9033Q_SOFT_START_BIT        0x02
#define ES9033Q_I2S_LEN_32            0x00
#define ES9033Q_LINE_DRIVER_ENABLE    0x01
#define ES9033Q_LINE_DRIVER_ILIMIT    0x20
#define ES9033Q_LINE_DRIVER_INIT      (ES9033Q_LINE_DRIVER_ENABLE | ES9033Q_LINE_DRIVER_ILIMIT)
#define ES9033Q_DPLL_BW_DEFAULT       0x04
#define ES9033Q_VOL_0DB               0x00
#define ES9033Q_VOL_MUTE              0xFF
#define ESS_SABRE_RESET_DELAY_MS      5
#define ESS_SABRE_FILTER_COUNT        8
#define ESS_SABRE_DAC_I2C_ADDR_BASE   0x48
#endif // NATIVE_TEST

// ===== Supported sample rates =====
const uint32_t HalEs9033Q::_kSupportedRates[6] = {
    44100, 48000, 96000, 192000, 384000, 768000
};

// ===== Constructor =====

HalEs9033Q::HalEs9033Q() : HalEssSabreDacBase() {
    hal_init_descriptor(_descriptor, "ess,es9033q", "ES9033Q", "ESS Technology",
        HAL_DEV_DAC, 2, ESS_SABRE_DAC_I2C_ADDR_BASE, HAL_BUS_I2C, HAL_I2C_BUS_EXP,
        HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K,
        HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE | HAL_CAP_LINE_DRIVER);
    _initPriority = HAL_PRIORITY_HARDWARE;
}

// ===== HalDevice lifecycle =====

bool HalEs9033Q::probe() {
#ifndef NATIVE_TEST
    if (!_bus().probe(_i2cAddr)) return false;
    uint8_t chipId = _readReg(ES9033Q_REG_CHIP_ID);
    return (chipId == ES9033Q_CHIP_ID);
#else
    return true;
#endif
}

HalInitResult HalEs9033Q::init() {
    // ---- 1. Read per-device config overrides from HAL Device Manager ----
    _applyConfigOverrides();

    LOG_I("[HAL:ES9033Q] Initializing (I2C addr=0x%02X bus=%u SDA=%d SCL=%d sr=%luHz bits=%u)",
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth);

#ifndef NATIVE_TEST
    // ---- 2. Select TwoWire instance and initialize bus at 400 kHz ----
    _selectWire();
    LOG_I("[HAL:ES9033Q] I2C initialized (bus %u SDA=%d SCL=%d 400kHz)",
          _i2cBusIndex, _sdaPin, _sclPin);
#endif

    // ---- 3. Verify chip ID (reg 0xE1, expect 0x88) ----
    uint8_t chipId = _readReg(ES9033Q_REG_CHIP_ID);
    if (chipId != ES9033Q_CHIP_ID) {
        LOG_W("[HAL:ES9033Q] Unexpected chip ID: 0x%02X (expected 0x%02X) — continuing",
              chipId, ES9033Q_CHIP_ID);
    } else {
        LOG_I("[HAL:ES9033Q] Chip ID OK (0x%02X)", chipId);
    }

    // ---- 4. Configure I2S input: I2S/Philips, 32-bit word length ----
    // REG_SYSTEM_SETTINGS (0x00): INPUT_SEL=I2S, SOFT_START enabled
    _writeReg(ES9033Q_REG_SYSTEM_SETTINGS, (uint8_t)(ES9033Q_INPUT_I2S | ES9033Q_SOFT_START_BIT));

    // REG_INPUT_CONFIG (0x01): 32-bit I2S, Philips format
    _writeReg(ES9033Q_REG_INPUT_CONFIG, (uint8_t)ES9033Q_I2S_LEN_32);

    // ---- 5. DPLL bandwidth: default balanced setting ----
    _writeReg(ES9033Q_REG_DPLL_BANDWIDTH, ES9033Q_DPLL_BW_DEFAULT);

#ifndef NATIVE_TEST
    // ---- 6. Allow DPLL to lock before proceeding ----
    delay(ESS_SABRE_RESET_DELAY_MS);
#endif

    // ---- 7. Set filter preset (bits[2:0] of REG_FILTER_SHAPE) ----
    uint8_t preset = (_filterPreset >= ESS_SABRE_FILTER_COUNT) ? 0 : _filterPreset;
    _writeReg(ES9033Q_REG_FILTER_SHAPE, (uint8_t)(preset & 0x07));

    // ---- 8. Set initial volume ----
    // Attenuation 0x00=0dB, 0xFF=mute. Map _volume% → attenuation.
    uint8_t volReg = _muted ? ES9033Q_VOL_MUTE :
                    (uint8_t)(((uint32_t)(100u - _volume) * 0xFF) / 100u);
    _writeReg(ES9033Q_REG_VOLUME_L, volReg);
    _writeReg(ES9033Q_REG_VOLUME_R, volReg);

    // ---- 9. Integrated line driver: enable with current limiting ----
    // REG_LINE_DRIVER (0x14): LINE_DRIVER_ENABLE=1, CURRENT_LIMIT_EN=1, IMP=lowest (75Ω)
    _lineDriverEnabled = true;
    _writeReg(ES9033Q_REG_LINE_DRIVER, (uint8_t)ES9033Q_LINE_DRIVER_INIT);

    // ---- 10. Enable expansion I2S TX ----
    if (!_enableI2sTx()) {
        LOG_E("[HAL:ES9033Q] Failed to enable expansion I2S TX");
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "I2S TX enable failed");
    }

    // ---- 11. Mark device ready ----
    _initialized = true;
    _state = HAL_STATE_AVAILABLE;
    setReady(true);

    LOG_I("[HAL:ES9033Q] Ready (vol=%u%% muted=%d filter=%u linedrv=%d)",
          _volume, (int)_muted, _filterPreset, (int)_lineDriverEnabled);
    return hal_init_ok();
}

void HalEs9033Q::deinit() {
    if (!_initialized) return;

    setReady(false);

    // Mute outputs before disabling
    _writeReg(ES9033Q_REG_VOLUME_L, ES9033Q_VOL_MUTE);
    _writeReg(ES9033Q_REG_VOLUME_R, ES9033Q_VOL_MUTE);

    // Disable integrated line drivers (power down output stage)
    _writeReg(ES9033Q_REG_LINE_DRIVER, 0x00);
    _lineDriverEnabled = false;

    // Shut down expansion I2S TX
    _disableI2sTx();

    _initialized = false;
    _state = HAL_STATE_REMOVED;

    LOG_I("[HAL:ES9033Q] Deinitialized");
}

void HalEs9033Q::dumpConfig() {
    LOG_I("[HAL:ES9033Q] %s by %s (compat=%s) i2c=0x%02X bus=%u sda=%d scl=%d "
          "sr=%luHz bits=%u vol=%u%% muted=%d filter=%u linedrv=%d",
          _descriptor.name, _descriptor.manufacturer, _descriptor.compatible,
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth,
          _volume, (int)_muted, _filterPreset, (int)_lineDriverEnabled);
}

bool HalEs9033Q::healthCheck() {
#ifndef NATIVE_TEST
    if (!_initialized) return false;
    uint8_t id = _readReg(ES9033Q_REG_CHIP_ID);
    return (id == ES9033Q_CHIP_ID);
#else
    return _initialized;
#endif
}

// ===== HalAudioDevice =====

bool HalEs9033Q::configure(uint32_t sampleRate, uint8_t bitDepth) {
    if (!_validateSampleRate(sampleRate, _kSupportedRates, _kRateCount)) {
        LOG_W("[HAL:ES9033Q] Unsupported sample rate: %luHz", (unsigned long)sampleRate);
        return false;
    }
    if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32) {
        LOG_W("[HAL:ES9033Q] Unsupported bit depth: %u", bitDepth);
        return false;
    }
    _sampleRate = sampleRate;
    _bitDepth   = bitDepth;

    if (_initialized) {
        // Update I2S word length register
        uint8_t lenBits = (bitDepth == 16) ? 0xC0 :
                          (bitDepth == 24) ? 0x40 : 0x00;  // ES9033Q_I2S_LEN_*
        _writeReg(ES9033Q_REG_INPUT_CONFIG, lenBits);
    }

    LOG_I("[HAL:ES9033Q] Configured: %luHz %ubit", (unsigned long)sampleRate, bitDepth);
    return true;
}

bool HalEs9033Q::setVolume(uint8_t percent) {
    if (!_initialized) return false;
    if (percent > 100) percent = 100;
    _volume = percent;

    // ES9033Q attenuation: 0x00=0dB, 0xFF=mute, 0.5dB per step
    uint8_t atten = (uint8_t)(((uint32_t)(100u - percent) * 0xFF) / 100u);
    bool ok  = _writeReg(ES9033Q_REG_VOLUME_L, atten);
    ok       = ok && _writeReg(ES9033Q_REG_VOLUME_R, atten);
    LOG_D("[HAL:ES9033Q] Volume: %d%% -> atten=0x%02X", percent, atten);
    return ok;
}

bool HalEs9033Q::setMute(bool mute) {
    if (!_initialized) return false;
    _muted = mute;
    uint8_t atten = mute ? ES9033Q_VOL_MUTE :
                    (uint8_t)(((uint32_t)(100u - _volume) * 0xFF) / 100u);
    bool ok  = _writeReg(ES9033Q_REG_VOLUME_L, atten);
    ok       = ok && _writeReg(ES9033Q_REG_VOLUME_R, atten);
    LOG_I("[HAL:ES9033Q] %s", mute ? "Muted" : "Unmuted");
    return ok;
}

// ===== Filter preset (override base) =====

bool HalEs9033Q::setFilterPreset(uint8_t preset) {
    if (preset >= ESS_SABRE_FILTER_COUNT) return false;
    _filterPreset = preset;
    if (_initialized) {
        _writeReg(ES9033Q_REG_FILTER_SHAPE, (uint8_t)(preset & 0x07));
    }
    LOG_I("[HAL:ES9033Q] Filter preset: %u", preset);
    return true;
}

// ===== ES9033Q-specific: integrated line driver =====

bool HalEs9033Q::setLineDriverEnabled(bool enable) {
    _lineDriverEnabled = enable;
    if (!_initialized) return true;  // Deferred — applied at next init

    if (enable) {
        // Enable with current limiting, lowest impedance (75 Ω), ground-centered
        _writeReg(ES9033Q_REG_LINE_DRIVER, (uint8_t)ES9033Q_LINE_DRIVER_INIT);
    } else {
        // Power down output stage
        _writeReg(ES9033Q_REG_LINE_DRIVER, 0x00);
    }

    LOG_I("[HAL:ES9033Q] Integrated line drivers: %s", enable ? "enabled" : "disabled");
    return true;
}

#endif // DAC_ENABLED
