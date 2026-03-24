#ifdef DAC_ENABLED
// HalCs4399 — Cirrus Logic CS4399 2-channel MasterHIFI DAC with NOS filter mode.
// Register map sourced from src/drivers/cs4399_regs.h.
// Inherits shared 16-bit paged I2C helpers, config override reading,
// buildSink() infrastructure, and I2S TX lifecycle management from
// HalCirrusDacBase.

#include "hal_cs4399.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include <Arduino.h>
#include "../debug_serial.h"
#include "../drivers/cs4399_regs.h"
#else
// ===== Native test stubs — no hardware access =====
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)

// Register address constants (mirrors cs4399_regs.h for native compilation;
// all actual hardware writes are inside #ifndef NATIVE_TEST blocks below).
#define CS4399_I2C_ADDR               0x48
#define CS4399_CHIP_ID                0x97
#define CS4399_REG_DEVID_REVID        0x0001
#define CS4399_REG_POWER_CTL          0x0006
#define CS4399_REG_FUNC_MODE          0x0007
#define CS4399_REG_IFACE_CTL          0x0009
#define CS4399_REG_PCM_PATH           0x0020
#define CS4399_REG_VOL_A              0x001E
#define CS4399_REG_VOL_B              0x001F
#define CS4399_REG_MASTER_VOL         0x001A
#define CS4399_REG_CLOCK_CTL          0x000C
#define CS4399_REG_NOS_CTL            0x001C
#define CS4399_NOS_ENABLE             0x01
#define CS4399_NOS_DISABLE            0x00
#define CS4399_PDN_BIT                0x01
#define CS4399_FMT_I2S                0x00
#define CS4399_WL_32BIT               0x00
#define CS4399_WL_24BIT               0x10
#define CS4399_WL_20BIT               0x20
#define CS4399_WL_16BIT               0x30
#define CS4399_MUTE_A_BIT             0x40
#define CS4399_MUTE_B_BIT             0x80
#define CS4399_MUTE_BOTH              (CS4399_MUTE_A_BIT | CS4399_MUTE_B_BIT)
#define CS4399_FILTER_MASK            0x07
#define CS4399_SPEED_NORMAL           0x00
#define CS4399_SPEED_DOUBLE           0x10
#define CS4399_SPEED_QUAD             0x20
#define CS4399_SPEED_OCTAL            0x30
#define CS4399_FILTER_COUNT           5
#define CS4399_VOL_0DB                0x00
#define CS4399_VOL_MUTE               0xFF
#define CS4399_IFACE_DEFAULT          (CS4399_FMT_I2S | CS4399_WL_32BIT)
#define CIRRUS_DAC_RESET_DELAY_MS     10
#define CIRRUS_DAC_I2C_ADDR_BASE      0x48
#endif // NATIVE_TEST

// ===== Supported sample rates =====
const uint32_t HalCs4399::_kSupportedRates[5] = {
    44100, 48000, 96000, 192000, 384000
};

// ===== Constructor =====

HalCs4399::HalCs4399() : HalCirrusDacBase() {
    hal_init_descriptor(_descriptor, "cirrus,cs4399", "CS4399", "Cirrus Logic",
        HAL_DEV_DAC, 2, CIRRUS_DAC_I2C_ADDR_BASE, HAL_BUS_I2C, HAL_I2C_BUS_EXP,
        HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K,
        HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS);
    _initPriority = HAL_PRIORITY_HARDWARE;
}

// ===== HalDevice lifecycle =====

bool HalCs4399::probe() {
#ifndef NATIVE_TEST
    if (!_bus().probe(_i2cAddr)) return false;
    uint8_t chipId = _readRegPaged(CS4399_REG_DEVID_REVID);
    return (chipId == CS4399_CHIP_ID);
#else
    return true;
#endif
}

HalInitResult HalCs4399::init() {
    // ---- 1. Read per-device config overrides from HAL Device Manager ----
    _applyConfigOverrides();

    LOG_I("[HAL:CS4399] Initializing (I2C addr=0x%02X bus=%u SDA=%d SCL=%d sr=%luHz bits=%u)",
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth);

#ifndef NATIVE_TEST
    // ---- 2. Select TwoWire instance and initialize bus at 400 kHz ----
    _selectWire();
    LOG_I("[HAL:CS4399] I2C initialized (bus %u SDA=%d SCL=%d 400kHz)",
          _i2cBusIndex, _sdaPin, _sclPin);
#endif

    // ---- 3. Verify chip ID (reg 0x0001, expect 0x97) ----
    uint8_t chipId = _readRegPaged(CS4399_REG_DEVID_REVID);
    if (chipId != CS4399_CHIP_ID) {
        LOG_W("[HAL:CS4399] Unexpected chip ID: 0x%02X (expected 0x%02X) — continuing",
              chipId, CS4399_CHIP_ID);
    } else {
        LOG_I("[HAL:CS4399] Chip ID OK (0x%02X)", chipId);
    }

    // ---- 4. Power up: clear PDN bit in power control register ----
    _writeRegPaged(CS4399_REG_POWER_CTL, 0x00);

#ifndef NATIVE_TEST
    // ---- 5. Allow power-on transient to settle ----
    delay(CIRRUS_DAC_RESET_DELAY_MS);
#endif

    // ---- 6. Configure I2S interface: Philips/I2S slave, 32-bit word length ----
    _writeRegPaged(CS4399_REG_IFACE_CTL, (uint8_t)CS4399_IFACE_DEFAULT);

    // ---- 7. Set speed mode based on sample rate ----
    uint8_t speedMode;
    if (_sampleRate <= 48000) {
        speedMode = CS4399_SPEED_NORMAL;
    } else if (_sampleRate <= 96000) {
        speedMode = CS4399_SPEED_DOUBLE;
    } else if (_sampleRate <= 192000) {
        speedMode = CS4399_SPEED_QUAD;
    } else {
        speedMode = CS4399_SPEED_OCTAL;  // ≤384kHz
    }
    _writeRegPaged(CS4399_REG_CLOCK_CTL, speedMode);

    // ---- 8. Set filter preset (bits[2:0] of PCM path register) ----
    uint8_t preset = (_filterPreset >= CS4399_FILTER_COUNT) ? 0 : _filterPreset;
    // Read-modify-write: preserve mute bits while setting filter
    uint8_t pcmPath = (uint8_t)(preset & CS4399_FILTER_MASK);
    if (_muted) {
        pcmPath = (uint8_t)(pcmPath | CS4399_MUTE_BOTH);
    }
    _writeRegPaged(CS4399_REG_PCM_PATH, pcmPath);

    // ---- 9. NOS control register: initialize to disabled at startup ----
    // Caller may subsequently enable via setNosMode(true).
    _nosEnabled = false;
    _writeRegPaged(CS4399_REG_NOS_CTL, CS4399_NOS_DISABLE);

    // ---- 10. Set initial volume ----
    // Volume register: 0x00=0dB, 0xFF=mute (inverted attenuation).
    // When muted, use full attenuation instead of the volume register's mute bits
    // to ensure analog silence during init.
    uint8_t volReg = _muted ? CS4399_VOL_MUTE :
                    (uint8_t)(((uint32_t)(100u - _volume) * 0xFF) / 100u);
    _writeRegPaged(CS4399_REG_VOL_A, volReg);
    _writeRegPaged(CS4399_REG_VOL_B, volReg);

    // ---- 11. Enable expansion I2S TX ----
    if (!_enableI2sTx()) {
        LOG_E("[HAL:CS4399] Failed to enable expansion I2S TX");
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "I2S TX enable failed");
    }

    // ---- 12. Mark device ready ----
    _initialized = true;
    _state = HAL_STATE_AVAILABLE;
    setReady(true);

    LOG_I("[HAL:CS4399] Ready (vol=%u%% muted=%d filter=%u nos=%d)",
          _volume, (int)_muted, _filterPreset, (int)_nosEnabled);
    return hal_init_ok();
}

void HalCs4399::deinit() {
    if (!_initialized) return;

    setReady(false);

    // Mute outputs before disabling — use both volume and mute-bit registers
    _writeRegPaged(CS4399_REG_VOL_A, CS4399_VOL_MUTE);
    _writeRegPaged(CS4399_REG_VOL_B, CS4399_VOL_MUTE);

    // Set mute bits in PCM path register
    uint8_t pcmPath = _readRegPaged(CS4399_REG_PCM_PATH);
    _writeRegPaged(CS4399_REG_PCM_PATH, (uint8_t)(pcmPath | CS4399_MUTE_BOTH));

    // Disable NOS filter engine
    _writeRegPaged(CS4399_REG_NOS_CTL, CS4399_NOS_DISABLE);
    _nosEnabled = false;

    // Power down the device
    _writeRegPaged(CS4399_REG_POWER_CTL, CS4399_PDN_BIT);

    // Shut down expansion I2S TX
    _disableI2sTx();

    _initialized = false;
    _state = HAL_STATE_REMOVED;

    LOG_I("[HAL:CS4399] Deinitialized");
}

void HalCs4399::dumpConfig() {
    LOG_I("[HAL:CS4399] %s by %s (compat=%s) i2c=0x%02X bus=%u sda=%d scl=%d "
          "sr=%luHz bits=%u vol=%u%% muted=%d filter=%u nos=%d",
          _descriptor.name, _descriptor.manufacturer, _descriptor.compatible,
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth,
          _volume, (int)_muted, _filterPreset, (int)_nosEnabled);
}

bool HalCs4399::healthCheck() {
#ifndef NATIVE_TEST
    if (!_initialized) return false;
    uint8_t id = _readRegPaged(CS4399_REG_DEVID_REVID);
    return (id == CS4399_CHIP_ID);
#else
    return _initialized;
#endif
}

// ===== HalAudioDevice =====

bool HalCs4399::configure(uint32_t sampleRate, uint8_t bitDepth) {
    if (!_validateSampleRate(sampleRate, _kSupportedRates, _kRateCount)) {
        LOG_W("[HAL:CS4399] Unsupported sample rate: %luHz", (unsigned long)sampleRate);
        return false;
    }
    if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32) {
        LOG_W("[HAL:CS4399] Unsupported bit depth: %u", bitDepth);
        return false;
    }
    _sampleRate = sampleRate;
    _bitDepth   = bitDepth;

    if (_initialized) {
        // Update interface word length register
        uint8_t wlBits = (bitDepth == 16) ? CS4399_WL_16BIT :
                         (bitDepth == 24) ? CS4399_WL_24BIT : CS4399_WL_32BIT;
        _writeRegPaged(CS4399_REG_IFACE_CTL, (uint8_t)(CS4399_FMT_I2S | wlBits));

        // Update speed mode
        uint8_t speedMode;
        if (sampleRate <= 48000) {
            speedMode = CS4399_SPEED_NORMAL;
        } else if (sampleRate <= 96000) {
            speedMode = CS4399_SPEED_DOUBLE;
        } else if (sampleRate <= 192000) {
            speedMode = CS4399_SPEED_QUAD;
        } else {
            speedMode = CS4399_SPEED_OCTAL;
        }
        _writeRegPaged(CS4399_REG_CLOCK_CTL, speedMode);
    }

    LOG_I("[HAL:CS4399] Configured: %luHz %ubit", (unsigned long)sampleRate, bitDepth);
    return true;
}

bool HalCs4399::setVolume(uint8_t percent) {
    if (!_initialized) return false;
    if (percent > 100) percent = 100;
    _volume = percent;

    // CS4399 attenuation: 0x00=0dB, 0xFF=mute, 0.5dB per step
    // Map 100% → 0x00 (no attenuation), 0% → 0xFF (full attenuation)
    uint8_t atten = (uint8_t)(((uint32_t)(100u - percent) * 0xFF) / 100u);
    bool ok  = _writeRegPaged(CS4399_REG_VOL_A, atten);
    ok       = ok && _writeRegPaged(CS4399_REG_VOL_B, atten);
    LOG_D("[HAL:CS4399] Volume: %d%% -> atten=0x%02X", percent, atten);
    return ok;
}

bool HalCs4399::setMute(bool mute) {
    if (!_initialized) return false;
    _muted = mute;

    // Mute via read-modify-write on the PCM path register (mute bits + filter sel)
    uint8_t pcmPath = _readRegPaged(CS4399_REG_PCM_PATH);
    if (mute) {
        pcmPath = (uint8_t)(pcmPath | CS4399_MUTE_BOTH);
    } else {
        pcmPath = (uint8_t)(pcmPath & (uint8_t)~CS4399_MUTE_BOTH);
    }
    bool ok = _writeRegPaged(CS4399_REG_PCM_PATH, pcmPath);
    LOG_I("[HAL:CS4399] %s", mute ? "Muted" : "Unmuted");
    return ok;
}

// ===== Filter preset (override base) =====

bool HalCs4399::setFilterPreset(uint8_t preset) {
    if (preset >= CS4399_FILTER_COUNT) return false;  // CS4399 has 5 presets (0-4)
    _filterPreset = preset;
    if (_initialized) {
        // Read-modify-write: preserve mute bits while updating filter selection
        uint8_t pcmPath = _readRegPaged(CS4399_REG_PCM_PATH);
        pcmPath = (uint8_t)((pcmPath & (uint8_t)~CS4399_FILTER_MASK) | (preset & CS4399_FILTER_MASK));
        _writeRegPaged(CS4399_REG_PCM_PATH, pcmPath);
    }
    LOG_I("[HAL:CS4399] Filter preset: %u", preset);
    return true;
}

// ===== CS4399-specific: NOS filter mode =====

bool HalCs4399::setNosMode(bool enable) {
    _nosEnabled = enable;
    if (!_initialized) return true;  // Deferred — applied at next init

    // CS4399_REG_NOS_CTL (0x001C) bit0 = NOS_ENABLE; no other bits defined
    uint8_t val = enable ? CS4399_NOS_ENABLE : CS4399_NOS_DISABLE;
    bool ok = _writeRegPaged(CS4399_REG_NOS_CTL, val);
    LOG_I("[HAL:CS4399] NOS filter: %s", enable ? "enabled" : "disabled");
    return ok;
}

bool HalCs4399::isNosMode() const {
    return _nosEnabled;
}

#endif // DAC_ENABLED
