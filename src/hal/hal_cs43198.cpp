#ifdef DAC_ENABLED
// HalCs43198 — Cirrus Logic CS43198 2-channel MasterHIFI DAC.
// Register map sourced from src/drivers/cs43198_regs.h.
// Inherits shared 16-bit paged I2C helpers, config override reading,
// buildSink() infrastructure, and I2S TX lifecycle management from
// HalCirrusDacBase.

#include "hal_cs43198.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include <Wire.h>
#include "hal_cirrus_dac_base.h"  // for extern TwoWire Wire2
#include <Arduino.h>
#include "../debug_serial.h"
#include "../drivers/cs43198_regs.h"
#else
// ===== Native test stubs — no hardware access =====
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)

// Register address constants (mirrors cs43198_regs.h for native compilation;
// all actual hardware writes are inside #ifndef NATIVE_TEST blocks below).
#define CS43198_I2C_ADDR              0x48
#define CS43198_CHIP_ID               0x98
#define CS43198_REG_DEVID_REVID       0x0001
#define CS43198_REG_POWER_CTL         0x0006
#define CS43198_REG_FUNC_MODE         0x0007
#define CS43198_REG_IFACE_CTL         0x0009
#define CS43198_REG_PCM_PATH          0x0020
#define CS43198_REG_VOL_A             0x001E
#define CS43198_REG_VOL_B             0x001F
#define CS43198_REG_MASTER_VOL        0x001A
#define CS43198_REG_CLOCK_CTL         0x000C
#define CS43198_PDN_BIT               0x01
#define CS43198_FMT_I2S               0x00
#define CS43198_WL_32BIT              0x00
#define CS43198_WL_24BIT              0x10
#define CS43198_WL_20BIT              0x20
#define CS43198_WL_16BIT              0x30
#define CS43198_MUTE_A_BIT            0x40
#define CS43198_MUTE_B_BIT            0x80
#define CS43198_MUTE_BOTH             (CS43198_MUTE_A_BIT | CS43198_MUTE_B_BIT)
#define CS43198_FILTER_MASK           0x07
#define CS43198_SPEED_NORMAL          0x00
#define CS43198_SPEED_DOUBLE          0x10
#define CS43198_SPEED_QUAD            0x20
#define CS43198_SPEED_OCTAL           0x30
#define CS43198_FILTER_COUNT          7
#define CS43198_VOL_0DB               0x00
#define CS43198_VOL_MUTE              0xFF
#define CS43198_IFACE_DEFAULT         (CS43198_FMT_I2S | CS43198_WL_32BIT)
#define CIRRUS_DAC_RESET_DELAY_MS     10
#define CIRRUS_DAC_I2C_ADDR_BASE      0x48
#endif // NATIVE_TEST

// ===== Supported sample rates =====
const uint32_t HalCs43198::_kSupportedRates[5] = {
    44100, 48000, 96000, 192000, 384000
};

// ===== Constructor =====

HalCs43198::HalCs43198() : HalCirrusDacBase() {
    hal_init_descriptor(_descriptor, "cirrus,cs43198", "CS43198", "Cirrus Logic",
        HAL_DEV_DAC, 2, CIRRUS_DAC_I2C_ADDR_BASE, HAL_BUS_I2C, HAL_I2C_BUS_EXP,
        HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K,
        HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS | HAL_CAP_DSD);
    _initPriority = HAL_PRIORITY_HARDWARE;
}

// ===== HalDevice lifecycle =====

bool HalCs43198::probe() {
#ifndef NATIVE_TEST
    if (!_wire) return false;
    _wire->beginTransmission(_i2cAddr);
    if (_wire->endTransmission() != 0) return false;
    uint8_t chipId = _readRegPaged(CS43198_REG_DEVID_REVID);
    return (chipId == CS43198_CHIP_ID);
#else
    return true;
#endif
}

HalInitResult HalCs43198::init() {
    // ---- 1. Read per-device config overrides from HAL Device Manager ----
    _applyConfigOverrides();

    LOG_I("[HAL:CS43198] Initializing (I2C addr=0x%02X bus=%u SDA=%d SCL=%d sr=%luHz bits=%u)",
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth);

#ifndef NATIVE_TEST
    // ---- 2. Select TwoWire instance and initialize bus at 400 kHz ----
    _selectWire();
    LOG_I("[HAL:CS43198] I2C initialized (bus %u SDA=%d SCL=%d 400kHz)",
          _i2cBusIndex, _sdaPin, _sclPin);
#endif

    // ---- 3. Verify chip ID (reg 0x0001, expect 0x98) ----
    uint8_t chipId = _readRegPaged(CS43198_REG_DEVID_REVID);
    if (chipId != CS43198_CHIP_ID) {
        LOG_W("[HAL:CS43198] Unexpected chip ID: 0x%02X (expected 0x%02X) — continuing",
              chipId, CS43198_CHIP_ID);
    } else {
        LOG_I("[HAL:CS43198] Chip ID OK (0x%02X)", chipId);
    }

    // ---- 4. Power up: clear PDN bit in power control register ----
    _writeRegPaged(CS43198_REG_POWER_CTL, 0x00);

#ifndef NATIVE_TEST
    // ---- 5. Allow power-on transient to settle ----
    delay(CIRRUS_DAC_RESET_DELAY_MS);
#endif

    // ---- 6. Configure I2S interface: Philips/I2S slave, 32-bit word length ----
    _writeRegPaged(CS43198_REG_IFACE_CTL, (uint8_t)CS43198_IFACE_DEFAULT);

    // ---- 7. Set speed mode based on sample rate ----
    uint8_t speedMode;
    if (_sampleRate <= 48000) {
        speedMode = CS43198_SPEED_NORMAL;
    } else if (_sampleRate <= 96000) {
        speedMode = CS43198_SPEED_DOUBLE;
    } else if (_sampleRate <= 192000) {
        speedMode = CS43198_SPEED_QUAD;
    } else {
        speedMode = CS43198_SPEED_OCTAL;  // ≤384kHz
    }
    _writeRegPaged(CS43198_REG_CLOCK_CTL, speedMode);

    // ---- 8. Set filter preset (bits[2:0] of PCM path register) ----
    uint8_t preset = (_filterPreset >= CS43198_FILTER_COUNT) ? 0 : _filterPreset;
    // Read-modify-write: preserve mute bits while setting filter
    uint8_t pcmPath = (uint8_t)(preset & CS43198_FILTER_MASK);
    if (_muted) {
        pcmPath = (uint8_t)(pcmPath | CS43198_MUTE_BOTH);
    }
    _writeRegPaged(CS43198_REG_PCM_PATH, pcmPath);

    // ---- 9. Set initial volume ----
    // Volume register: 0x00=0dB, 0xFF=mute (inverted attenuation).
    // When muted, use full attenuation instead of the volume register's mute bits
    // to ensure analog silence during init.
    uint8_t volReg = _muted ? CS43198_VOL_MUTE :
                    (uint8_t)(((uint32_t)(100u - _volume) * 0xFF) / 100u);
    _writeRegPaged(CS43198_REG_VOL_A, volReg);
    _writeRegPaged(CS43198_REG_VOL_B, volReg);

    // ---- 10. Enable expansion I2S TX ----
    if (!_enableI2sTx()) {
        LOG_E("[HAL:CS43198] Failed to enable expansion I2S TX");
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "I2S TX enable failed");
    }

    // ---- 11. Mark device ready ----
    _initialized = true;
    _state = HAL_STATE_AVAILABLE;
    setReady(true);

    LOG_I("[HAL:CS43198] Ready (vol=%u%% muted=%d filter=%u)",
          _volume, (int)_muted, _filterPreset);
    return hal_init_ok();
}

void HalCs43198::deinit() {
    if (!_initialized) return;

    setReady(false);

    // Mute outputs before disabling — use both volume and mute-bit registers
    _writeRegPaged(CS43198_REG_VOL_A, CS43198_VOL_MUTE);
    _writeRegPaged(CS43198_REG_VOL_B, CS43198_VOL_MUTE);

    // Set mute bits in PCM path register
    uint8_t pcmPath = _readRegPaged(CS43198_REG_PCM_PATH);
    _writeRegPaged(CS43198_REG_PCM_PATH, (uint8_t)(pcmPath | CS43198_MUTE_BOTH));

    // Power down the device
    _writeRegPaged(CS43198_REG_POWER_CTL, CS43198_PDN_BIT);

    // Shut down expansion I2S TX
    _disableI2sTx();

    _initialized = false;
    _state = HAL_STATE_REMOVED;

    LOG_I("[HAL:CS43198] Deinitialized");
}

void HalCs43198::dumpConfig() {
    LOG_I("[HAL:CS43198] %s by %s (compat=%s) i2c=0x%02X bus=%u sda=%d scl=%d "
          "sr=%luHz bits=%u vol=%u%% muted=%d filter=%u",
          _descriptor.name, _descriptor.manufacturer, _descriptor.compatible,
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth,
          _volume, (int)_muted, _filterPreset);
}

bool HalCs43198::healthCheck() {
#ifndef NATIVE_TEST
    if (!_wire || !_initialized) return false;
    uint8_t id = _readRegPaged(CS43198_REG_DEVID_REVID);
    return (id == CS43198_CHIP_ID);
#else
    return _initialized;
#endif
}

// ===== HalAudioDevice =====

bool HalCs43198::configure(uint32_t sampleRate, uint8_t bitDepth) {
    if (!_validateSampleRate(sampleRate, _kSupportedRates, _kRateCount)) {
        LOG_W("[HAL:CS43198] Unsupported sample rate: %luHz", (unsigned long)sampleRate);
        return false;
    }
    if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32) {
        LOG_W("[HAL:CS43198] Unsupported bit depth: %u", bitDepth);
        return false;
    }
    _sampleRate = sampleRate;
    _bitDepth   = bitDepth;

    if (_initialized) {
        // Update interface word length register
        uint8_t wlBits = (bitDepth == 16) ? CS43198_WL_16BIT :
                         (bitDepth == 24) ? CS43198_WL_24BIT : CS43198_WL_32BIT;
        _writeRegPaged(CS43198_REG_IFACE_CTL, (uint8_t)(CS43198_FMT_I2S | wlBits));

        // Update speed mode
        uint8_t speedMode;
        if (sampleRate <= 48000) {
            speedMode = CS43198_SPEED_NORMAL;
        } else if (sampleRate <= 96000) {
            speedMode = CS43198_SPEED_DOUBLE;
        } else if (sampleRate <= 192000) {
            speedMode = CS43198_SPEED_QUAD;
        } else {
            speedMode = CS43198_SPEED_OCTAL;
        }
        _writeRegPaged(CS43198_REG_CLOCK_CTL, speedMode);
    }

    LOG_I("[HAL:CS43198] Configured: %luHz %ubit", (unsigned long)sampleRate, bitDepth);
    return true;
}

bool HalCs43198::setVolume(uint8_t percent) {
    if (!_initialized) return false;
    if (percent > 100) percent = 100;
    _volume = percent;

    // CS43198 attenuation: 0x00=0dB, 0xFF=mute, 0.5dB per step
    // Map 100% → 0x00 (no attenuation), 0% → 0xFF (full attenuation)
    uint8_t atten = (uint8_t)(((uint32_t)(100u - percent) * 0xFF) / 100u);
    bool ok  = _writeRegPaged(CS43198_REG_VOL_A, atten);
    ok       = ok && _writeRegPaged(CS43198_REG_VOL_B, atten);
    LOG_D("[HAL:CS43198] Volume: %d%% -> atten=0x%02X", percent, atten);
    return ok;
}

bool HalCs43198::setMute(bool mute) {
    if (!_initialized) return false;
    _muted = mute;

    // Mute via read-modify-write on the PCM path register (mute bits + filter sel)
    uint8_t pcmPath = _readRegPaged(CS43198_REG_PCM_PATH);
    if (mute) {
        pcmPath = (uint8_t)(pcmPath | CS43198_MUTE_BOTH);
    } else {
        pcmPath = (uint8_t)(pcmPath & (uint8_t)~CS43198_MUTE_BOTH);
    }
    bool ok = _writeRegPaged(CS43198_REG_PCM_PATH, pcmPath);
    LOG_I("[HAL:CS43198] %s", mute ? "Muted" : "Unmuted");
    return ok;
}

// ===== Filter preset (override base) =====

bool HalCs43198::setFilterPreset(uint8_t preset) {
    if (preset >= CS43198_FILTER_COUNT) return false;  // CS43198 has 7 presets (0-6)
    _filterPreset = preset;
    if (_initialized) {
        // Read-modify-write: preserve mute bits while updating filter selection
        uint8_t pcmPath = _readRegPaged(CS43198_REG_PCM_PATH);
        pcmPath = (uint8_t)((pcmPath & (uint8_t)~CS43198_FILTER_MASK) | (preset & CS43198_FILTER_MASK));
        _writeRegPaged(CS43198_REG_PCM_PATH, pcmPath);
    }
    LOG_I("[HAL:CS43198] Filter preset: %u", preset);
    return true;
}

#endif // DAC_ENABLED
