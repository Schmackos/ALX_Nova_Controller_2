#ifdef DAC_ENABLED
// HalEs9069Q — ESS ES9069Q 2-channel DAC with integrated MQA hardware renderer.
// Register map sourced from src/drivers/es9069q_regs.h.
// Inherits shared I2C helpers, config override reading, buildSink() infrastructure,
// and I2S TX lifecycle management from HalEssSabreDacBase.

#include "hal_es9069q.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include <Wire.h>
#include "hal_ess_sabre_adc_base.h"  // for extern TwoWire Wire2
#include <Arduino.h>
#include "../debug_serial.h"
#include "../drivers/es9069q_regs.h"
#else
// ===== Native test stubs — no hardware access =====
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)

// Register address constants (mirrors es9069q_regs.h for native compilation;
// all actual hardware writes are inside #ifndef NATIVE_TEST blocks below).
#define ES9069Q_I2C_ADDR              0x48
#define ES9069Q_CHIP_ID               0x94
#define ES9069Q_REG_SYSTEM_SETTINGS   0x00
#define ES9069Q_REG_INPUT_CONFIG      0x01
#define ES9069Q_REG_DSD_CONFIG        0x06
#define ES9069Q_REG_FILTER_SHAPE      0x07
#define ES9069Q_REG_GENERAL_CONFIG    0x08
#define ES9069Q_REG_DPLL_BANDWIDTH    0x0C
#define ES9069Q_REG_VOLUME_L          0x0F
#define ES9069Q_REG_VOLUME_R          0x10
#define ES9069Q_REG_MQA_CONTROL       0x17
#define ES9069Q_REG_CHIP_ID           0xE1
#define ES9069Q_REG_DPLL_LOCK         0xE2
#define ES9069Q_INPUT_I2S             0x00
#define ES9069Q_SOFT_START_BIT        0x02
#define ES9069Q_I2S_LEN_32            0x00
#define ES9069Q_MQA_ENABLE_BIT        0x01
#define ES9069Q_MQA_STATUS_MASK       0x0E
#define ES9069Q_MQA_STATUS_NONE       0x00
#define ES9069Q_DPLL_BW_DEFAULT       0x04
#define ES9069Q_VOL_0DB               0x00
#define ES9069Q_VOL_MUTE              0xFF
#define ESS_SABRE_RESET_DELAY_MS      5
#define ESS_SABRE_FILTER_COUNT        8
#define ESS_SABRE_DAC_I2C_ADDR_BASE   0x48
#endif // NATIVE_TEST

// ===== Supported sample rates =====
const uint32_t HalEs9069Q::_kSupportedRates[6] = {
    44100, 48000, 96000, 192000, 384000, 768000
};

// ===== Constructor =====

HalEs9069Q::HalEs9069Q() : HalEssSabreDacBase() {
    hal_init_descriptor(_descriptor, "ess,es9069q", "ES9069Q", "ESS Technology",
        HAL_DEV_DAC, 2, ESS_SABRE_DAC_I2C_ADDR_BASE, HAL_BUS_I2C, HAL_I2C_BUS_EXP,
        HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K,
        HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_FILTERS | HAL_CAP_MUTE | HAL_CAP_MQA);
    _initPriority = HAL_PRIORITY_HARDWARE;
}

// ===== HalDevice lifecycle =====

bool HalEs9069Q::probe() {
#ifndef NATIVE_TEST
    if (!_wire) return false;
    _wire->beginTransmission(_i2cAddr);
    if (_wire->endTransmission() != 0) return false;
    uint8_t chipId = _readReg(ES9069Q_REG_CHIP_ID);
    return (chipId == ES9069Q_CHIP_ID);
#else
    return true;
#endif
}

HalInitResult HalEs9069Q::init() {
    // ---- 1. Read per-device config overrides from HAL Device Manager ----
    _applyConfigOverrides();

    LOG_I("[HAL:ES9069Q] Initializing (I2C addr=0x%02X bus=%u SDA=%d SCL=%d sr=%luHz bits=%u)",
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth);

#ifndef NATIVE_TEST
    // ---- 2. Select TwoWire instance and initialize bus at 400 kHz ----
    _selectWire();
    LOG_I("[HAL:ES9069Q] I2C initialized (bus %u SDA=%d SCL=%d 400kHz)",
          _i2cBusIndex, _sdaPin, _sclPin);
#endif

    // ---- 3. Verify chip ID (reg 0xE1, expect 0x94) ----
    uint8_t chipId = _readReg(ES9069Q_REG_CHIP_ID);
    if (chipId != ES9069Q_CHIP_ID) {
        LOG_W("[HAL:ES9069Q] Unexpected chip ID: 0x%02X (expected 0x%02X) — continuing",
              chipId, ES9069Q_CHIP_ID);
    } else {
        LOG_I("[HAL:ES9069Q] Chip ID OK (0x%02X)", chipId);
    }

    // ---- 4. Configure I2S input: I2S/Philips, 32-bit word length ----
    // REG_SYSTEM_SETTINGS (0x00): INPUT_SEL=I2S, SOFT_START enabled
    _writeReg(ES9069Q_REG_SYSTEM_SETTINGS, (uint8_t)(ES9069Q_INPUT_I2S | ES9069Q_SOFT_START_BIT));

    // REG_INPUT_CONFIG (0x01): 32-bit I2S, Philips format
    _writeReg(ES9069Q_REG_INPUT_CONFIG, (uint8_t)ES9069Q_I2S_LEN_32);

    // ---- 5. DPLL bandwidth: default balanced setting ----
    _writeReg(ES9069Q_REG_DPLL_BANDWIDTH, ES9069Q_DPLL_BW_DEFAULT);

#ifndef NATIVE_TEST
    // ---- 6. Allow DPLL to lock before proceeding ----
    delay(ESS_SABRE_RESET_DELAY_MS);
#endif

    // ---- 7. Set filter preset (bits[2:0] of REG_FILTER_SHAPE) ----
    uint8_t preset = (_filterPreset >= ESS_SABRE_FILTER_COUNT) ? 0 : _filterPreset;
    _writeReg(ES9069Q_REG_FILTER_SHAPE, (uint8_t)(preset & 0x07));

    // ---- 8. Set initial volume (0x00 = 0 dB; muted state applies attenuation) ----
    uint8_t volReg = _muted ? ES9069Q_VOL_MUTE :
                    (uint8_t)(((uint32_t)(100u - _volume) * 0xFF) / 100u);
    _writeReg(ES9069Q_REG_VOLUME_L, volReg);
    _writeReg(ES9069Q_REG_VOLUME_R, volReg);

    // ---- 9. MQA control register: initialize to disabled at startup ----
    // Caller may subsequently enable via setMqaEnabled(true).
    _mqaEnabled = false;
    _writeReg(ES9069Q_REG_MQA_CONTROL, 0x00);

    // ---- 10. Enable expansion I2S TX ----
    if (!_enableI2sTx()) {
        LOG_E("[HAL:ES9069Q] Failed to enable expansion I2S TX");
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "I2S TX enable failed");
    }

    // ---- 11. Mark device ready ----
    _initialized = true;
    _state = HAL_STATE_AVAILABLE;
    _ready = true;

    LOG_I("[HAL:ES9069Q] Ready (vol=%u%% muted=%d filter=%u mqa=%d)",
          _volume, (int)_muted, _filterPreset, (int)_mqaEnabled);
    return hal_init_ok();
}

void HalEs9069Q::deinit() {
    if (!_initialized) return;

    _ready = false;

    // Mute outputs before disabling
    _writeReg(ES9069Q_REG_VOLUME_L, ES9069Q_VOL_MUTE);
    _writeReg(ES9069Q_REG_VOLUME_R, ES9069Q_VOL_MUTE);

    // Disable MQA renderer
    _writeReg(ES9069Q_REG_MQA_CONTROL, 0x00);
    _mqaEnabled = false;

    // Shut down expansion I2S TX
    _disableI2sTx();

    _initialized = false;
    _state = HAL_STATE_REMOVED;

    LOG_I("[HAL:ES9069Q] Deinitialized");
}

void HalEs9069Q::dumpConfig() {
    LOG_I("[HAL:ES9069Q] %s by %s (compat=%s) i2c=0x%02X bus=%u sda=%d scl=%d "
          "sr=%luHz bits=%u vol=%u%% muted=%d filter=%u mqa=%d",
          _descriptor.name, _descriptor.manufacturer, _descriptor.compatible,
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth,
          _volume, (int)_muted, _filterPreset, (int)_mqaEnabled);
}

bool HalEs9069Q::healthCheck() {
#ifndef NATIVE_TEST
    if (!_wire || !_initialized) return false;
    uint8_t id = _readReg(ES9069Q_REG_CHIP_ID);
    return (id == ES9069Q_CHIP_ID);
#else
    return _initialized;
#endif
}

// ===== HalAudioDevice =====

bool HalEs9069Q::configure(uint32_t sampleRate, uint8_t bitDepth) {
    if (!_validateSampleRate(sampleRate, _kSupportedRates, _kRateCount)) {
        LOG_W("[HAL:ES9069Q] Unsupported sample rate: %luHz", (unsigned long)sampleRate);
        return false;
    }
    if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32) {
        LOG_W("[HAL:ES9069Q] Unsupported bit depth: %u", bitDepth);
        return false;
    }
    _sampleRate = sampleRate;
    _bitDepth   = bitDepth;

    if (_initialized) {
        // Update I2S word length register
        uint8_t lenBits = (bitDepth == 16) ? 0xC0 :
                          (bitDepth == 24) ? 0x40 : 0x00;  // ES9069Q_I2S_LEN_*
        _writeReg(ES9069Q_REG_INPUT_CONFIG, lenBits);
    }

    LOG_I("[HAL:ES9069Q] Configured: %luHz %ubit", (unsigned long)sampleRate, bitDepth);
    return true;
}

bool HalEs9069Q::setVolume(uint8_t percent) {
    if (!_initialized) return false;
    if (percent > 100) percent = 100;
    _volume = percent;

    // ES9069Q attenuation: 0x00=0dB, 0xFF=mute, 0.5dB per step
    // Map 100% → 0x00 (no attenuation), 0% → 0xFF (full attenuation)
    uint8_t atten = (uint8_t)(((uint32_t)(100u - percent) * 0xFF) / 100u);
    bool ok  = _writeReg(ES9069Q_REG_VOLUME_L, atten);
    ok       = ok && _writeReg(ES9069Q_REG_VOLUME_R, atten);
    LOG_D("[HAL:ES9069Q] Volume: %d%% -> atten=0x%02X", percent, atten);
    return ok;
}

bool HalEs9069Q::setMute(bool mute) {
    if (!_initialized) return false;
    _muted = mute;
    uint8_t atten = mute ? ES9069Q_VOL_MUTE :
                    (uint8_t)(((uint32_t)(100u - _volume) * 0xFF) / 100u);
    bool ok  = _writeReg(ES9069Q_REG_VOLUME_L, atten);
    ok       = ok && _writeReg(ES9069Q_REG_VOLUME_R, atten);
    LOG_I("[HAL:ES9069Q] %s", mute ? "Muted" : "Unmuted");
    return ok;
}

// ===== Filter preset (override base) =====

bool HalEs9069Q::setFilterPreset(uint8_t preset) {
    if (preset >= ESS_SABRE_FILTER_COUNT) return false;
    _filterPreset = preset;
    if (_initialized) {
        _writeReg(ES9069Q_REG_FILTER_SHAPE, (uint8_t)(preset & 0x07));
    }
    LOG_I("[HAL:ES9069Q] Filter preset: %u", preset);
    return true;
}

// ===== ES9069Q-specific: MQA hardware renderer =====

bool HalEs9069Q::setMqaEnabled(bool enable) {
    _mqaEnabled = enable;
    if (!_initialized) return true;  // Deferred — applied at next init

    // REG_MQA_CONTROL (0x17) bit0 = MQA_ENABLE; preserve other bits
    uint8_t cur = _readReg(ES9069Q_REG_MQA_CONTROL);
    uint8_t next = enable ? (uint8_t)(cur | ES9069Q_MQA_ENABLE_BIT)
                          : (uint8_t)(cur & (uint8_t)~ES9069Q_MQA_ENABLE_BIT);
    bool ok = _writeReg(ES9069Q_REG_MQA_CONTROL, next);
    LOG_I("[HAL:ES9069Q] MQA renderer: %s", enable ? "enabled" : "disabled");
    return ok;
}

bool HalEs9069Q::isMqaActive() const {
    if (!_initialized) return false;

    // Read MQA status from bits[3:1] of REG_MQA_CONTROL
    uint8_t reg = const_cast<HalEs9069Q*>(this)->_readReg(ES9069Q_REG_MQA_CONTROL);
    uint8_t status = (uint8_t)((reg & ES9069Q_MQA_STATUS_MASK) >> ES9069Q_MQA_STATUS_SHIFT);
    return (status != ES9069Q_MQA_STATUS_NONE);
}

#endif // DAC_ENABLED
