#ifdef DAC_ENABLED
// HalCs4398 — Cirrus Logic CS4398 2-channel 24-bit CLASSIC DAC.
// Register map sourced from src/drivers/cs4398_regs.h.
// Inherits shared 8-bit I2C helpers (_writeReg8 / _readReg8), config override
// reading, buildSink() infrastructure, and I2S TX lifecycle management from
// HalCirrusDacBase.
//
// NOTE: This is a LEGACY chip using 8-bit register addressing.
// Do NOT use _writeRegPaged() / _readRegPaged() here — those are for the
// paged 16-bit register map used by CS43198/CS43131/CS4399/CS43130.

#include "hal_cs4398.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include <Wire.h>
#include "hal_cirrus_dac_base.h"
#include <Arduino.h>
#include "../debug_serial.h"
#include "../drivers/cs4398_regs.h"
#else
// ===== Native test stubs — no hardware access =====
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)

// Register address constants (mirrors cs4398_regs.h for native compilation;
// all actual hardware writes are inside #ifndef NATIVE_TEST blocks below).
#define CS4398_I2C_ADDR            0x4C
#define CS4398_CHIP_ID             0x72
#define CS4398_REG_CHIP_ID         0x01
#define CS4398_REG_MODE_CTL        0x02
#define CS4398_REG_VOL_MIX_CTL    0x03
#define CS4398_REG_MUTE_CTL        0x04
#define CS4398_REG_VOL_A           0x05
#define CS4398_REG_VOL_B           0x06
#define CS4398_REG_RAMP_FILT       0x07
#define CS4398_REG_MISC_CTL        0x08
#define CS4398_REG_MISC_CTL2       0x09
#define CS4398_MUTE_A_BIT          0x01
#define CS4398_MUTE_B_BIT          0x02
#define CS4398_MUTE_BOTH           (CS4398_MUTE_A_BIT | CS4398_MUTE_B_BIT)
#define CS4398_FILTER_MASK         0x0C
#define CS4398_FILTER_SHIFT        2
#define CS4398_PDN_BIT             0x80
#define CS4398_FM_MASK             0x30
#define CS4398_FM_SINGLE           0x00
#define CS4398_FM_DOUBLE           0x10
#define CS4398_FM_QUAD             0x20
#define CS4398_DSD_BIT             0x40
#define CS4398_CP_EN_BIT           0x80
#define CS4398_DIF1_I2S            0x80
#define CS4398_FILTER_COUNT        3
#define CS4398_VOL_0DB             0x00
#define CS4398_VOL_MUTE            0xFF
#define CS4398_MODE_CTL_DEFAULT    (CS4398_CP_EN_BIT | CS4398_FM_SINGLE)
#define CS4398_VOL_MIX_DEFAULT     (CS4398_DIF1_I2S)
#define CIRRUS_DAC_RESET_DELAY_MS  10
#define CIRRUS_DAC_I2C_ADDR_BASE   0x48
#endif // NATIVE_TEST

// ===== Supported sample rates =====
// CS4398 is limited to 192kHz maximum — no 384kHz or 768kHz.
const uint32_t HalCs4398::_kSupportedRates[4] = {
    44100, 48000, 96000, 192000
};

// ===== Constructor =====

HalCs4398::HalCs4398() : HalCirrusDacBase() {
    hal_init_descriptor(_descriptor, "cirrus,cs4398", "CS4398", "Cirrus Logic",
        HAL_DEV_DAC, 2, CS4398_I2C_ADDR, HAL_BUS_I2C, HAL_I2C_BUS_EXP,
        HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K,
        HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS | HAL_CAP_DSD);
    _i2cAddr   = CS4398_I2C_ADDR;   // 0x4C — different from CS43198 default 0x48
    _bitDepth  = 24;                 // CS4398 maximum is 24-bit
    _initPriority = HAL_PRIORITY_HARDWARE;
}

// ===== HalDevice lifecycle =====

bool HalCs4398::probe() {
#ifndef NATIVE_TEST
    if (!_wire) return false;
    _wire->beginTransmission(_i2cAddr);
    if (_wire->endTransmission() != 0) return false;
    uint8_t chipId = _readReg8(CS4398_REG_CHIP_ID);
    // Upper nibble contains the chip family ID; mask off the revision nibble.
    return ((chipId & 0xF0) == (CS4398_CHIP_ID & 0xF0));
#else
    return true;
#endif
}

HalInitResult HalCs4398::init() {
    // ---- 1. Read per-device config overrides from HAL Device Manager ----
    _applyConfigOverrides();

    // Clamp bit depth: CS4398 maximum is 24-bit
    if (_bitDepth > 24) {
        LOG_W("[HAL:CS4398] Bit depth %u not supported — clamping to 24-bit", _bitDepth);
        _bitDepth = 24;
    }

    LOG_I("[HAL:CS4398] Initializing (I2C addr=0x%02X bus=%u SDA=%d SCL=%d sr=%luHz bits=%u)",
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth);

#ifndef NATIVE_TEST
    // ---- 2. Select TwoWire instance and initialize bus at 400 kHz ----
    _selectWire();
    LOG_I("[HAL:CS4398] I2C initialized (bus %u SDA=%d SCL=%d 400kHz)",
          _i2cBusIndex, _sdaPin, _sclPin);
#endif

    // ---- 3. Verify chip ID (reg 0x01, expect upper nibble 0x7) ----
    uint8_t chipId = _readReg8(CS4398_REG_CHIP_ID);
    if ((chipId & 0xF0) != (CS4398_CHIP_ID & 0xF0)) {
        LOG_W("[HAL:CS4398] Unexpected chip ID: 0x%02X (expected 0x7x) — continuing", chipId);
    } else {
        LOG_I("[HAL:CS4398] Chip ID OK (0x%02X, rev 0x%01X)", chipId, chipId & 0x0F);
    }

    // ---- 4. Enable control port and set PCM mode + speed ----
    uint8_t fmBits;
    if (_sampleRate <= 50000) {
        fmBits = CS4398_FM_SINGLE;
    } else if (_sampleRate <= 100000) {
        fmBits = CS4398_FM_DOUBLE;
    } else {
        fmBits = CS4398_FM_QUAD;   // ≤200kHz (192kHz fits here)
    }
    _writeReg8(CS4398_REG_MODE_CTL, (uint8_t)(CS4398_CP_EN_BIT | fmBits));

    // ---- 5. Set interface format to I2S (Philips) slave ----
    // {DIF2,DIF1} = 01 → I2S format.  DIF2 lives in REG_MODE_CTL bit2 (already 0
    // above).  DIF1 lives in REG_VOL_MIX_CTL bit7 → set it here.
    _writeReg8(CS4398_REG_VOL_MIX_CTL, CS4398_VOL_MIX_DEFAULT);

    // ---- 6. Power up: clear PDN bit in misc control register ----
    // Write 0x00 (all control features off, device powered up).
    _writeReg8(CS4398_REG_MISC_CTL, 0x00);

#ifndef NATIVE_TEST
    // ---- 7. Allow power-on transient to settle ----
    delay(CIRRUS_DAC_RESET_DELAY_MS);
#endif

    // ---- 8. Set filter preset (2-bit field in REG_RAMP_FILT bits[3:2]) ----
    uint8_t preset = (_filterPreset >= CS4398_FILTER_COUNT) ? 0 : _filterPreset;
    uint8_t rampFilt = (uint8_t)((preset & 0x03) << CS4398_FILTER_SHIFT);
    _writeReg8(CS4398_REG_RAMP_FILT, rampFilt);

    // ---- 9. Set initial volume ----
    // Volume register: 0x00=0dB, 0xFF=full attenuation (mute), 0.5dB/step.
    // Map percent 0-100 → attenuation register 0xFF-0x00.
    uint8_t volReg = _muted ? CS4398_VOL_MUTE :
                    (uint8_t)(((uint32_t)(100u - _volume) * 0xFF) / 100u);
    _writeReg8(CS4398_REG_VOL_A, volReg);
    _writeReg8(CS4398_REG_VOL_B, volReg);

    // ---- 10. Apply initial mute state via dedicated mute register ----
    _writeReg8(CS4398_REG_MUTE_CTL, _muted ? CS4398_MUTE_BOTH : 0x00);

    // ---- 11. Enable expansion I2S TX ----
    if (!_enableI2sTx()) {
        LOG_E("[HAL:CS4398] Failed to enable expansion I2S TX");
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "I2S TX enable failed");
    }

    // ---- 12. Mark device ready ----
    _initialized = true;
    _state = HAL_STATE_AVAILABLE;
    setReady(true);

    LOG_I("[HAL:CS4398] Ready (vol=%u%% muted=%d filter=%u)",
          _volume, (int)_muted, _filterPreset);
    return hal_init_ok();
}

void HalCs4398::deinit() {
    if (!_initialized) return;

    setReady(false);

    // Mute outputs before disabling — use both volume and mute registers
    _writeReg8(CS4398_REG_VOL_A, CS4398_VOL_MUTE);
    _writeReg8(CS4398_REG_VOL_B, CS4398_VOL_MUTE);
    _writeReg8(CS4398_REG_MUTE_CTL, CS4398_MUTE_BOTH);

    // Power down the device via the PDN bit in misc control
    _writeReg8(CS4398_REG_MISC_CTL, CS4398_PDN_BIT);

    // Shut down expansion I2S TX
    _disableI2sTx();

    _initialized = false;
    _state = HAL_STATE_REMOVED;

    LOG_I("[HAL:CS4398] Deinitialized");
}

void HalCs4398::dumpConfig() {
    LOG_I("[HAL:CS4398] %s by %s (compat=%s) i2c=0x%02X bus=%u sda=%d scl=%d "
          "sr=%luHz bits=%u vol=%u%% muted=%d filter=%u",
          _descriptor.name, _descriptor.manufacturer, _descriptor.compatible,
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth,
          _volume, (int)_muted, _filterPreset);
}

bool HalCs4398::healthCheck() {
#ifndef NATIVE_TEST
    if (!_wire || !_initialized) return false;
    uint8_t id = _readReg8(CS4398_REG_CHIP_ID);
    return ((id & 0xF0) == (CS4398_CHIP_ID & 0xF0));
#else
    return _initialized;
#endif
}

// ===== HalAudioDevice =====

bool HalCs4398::configure(uint32_t sampleRate, uint8_t bitDepth) {
    if (!_validateSampleRate(sampleRate, _kSupportedRates, _kRateCount)) {
        LOG_W("[HAL:CS4398] Unsupported sample rate: %luHz (max 192kHz)", (unsigned long)sampleRate);
        return false;
    }
    // CS4398 maximum is 24-bit; reject 32-bit explicitly
    if (bitDepth != 16 && bitDepth != 24) {
        LOG_W("[HAL:CS4398] Unsupported bit depth: %u (CS4398 supports 16 or 24-bit only)", bitDepth);
        return false;
    }
    _sampleRate = sampleRate;
    _bitDepth   = bitDepth;

    if (_initialized) {
        // Update speed mode based on new sample rate
        uint8_t fmBits;
        if (sampleRate <= 50000) {
            fmBits = CS4398_FM_SINGLE;
        } else if (sampleRate <= 100000) {
            fmBits = CS4398_FM_DOUBLE;
        } else {
            fmBits = CS4398_FM_QUAD;
        }
        // Read-modify-write: preserve CP_EN and DSD bits while updating FM field
        uint8_t modeCtl = _readReg8(CS4398_REG_MODE_CTL);
        modeCtl = (uint8_t)((modeCtl & (uint8_t)~CS4398_FM_MASK) | fmBits);
        _writeReg8(CS4398_REG_MODE_CTL, modeCtl);
    }

    LOG_I("[HAL:CS4398] Configured: %luHz %ubit", (unsigned long)sampleRate, bitDepth);
    return true;
}

bool HalCs4398::setVolume(uint8_t percent) {
    if (!_initialized) return false;
    if (percent > 100) percent = 100;
    _volume = percent;

    // CS4398 attenuation: 0x00=0dB, 0xFF=full attenuation, 0.5dB/step.
    // Map 100% → 0x00 (no attenuation), 0% → 0xFF (full attenuation).
    uint8_t atten = (uint8_t)(((uint32_t)(100u - percent) * 0xFF) / 100u);
    bool ok  = _writeReg8(CS4398_REG_VOL_A, atten);
    ok       = ok && _writeReg8(CS4398_REG_VOL_B, atten);
    LOG_D("[HAL:CS4398] Volume: %d%% -> atten=0x%02X", percent, atten);
    return ok;
}

bool HalCs4398::setMute(bool mute) {
    if (!_initialized) return false;
    _muted = mute;

    // CS4398 has a dedicated mute register (0x04) with per-channel bits.
    // This is distinct from CS43198 which embeds mute in the PCM path register.
    bool ok = _writeReg8(CS4398_REG_MUTE_CTL, mute ? CS4398_MUTE_BOTH : 0x00);
    LOG_I("[HAL:CS4398] %s", mute ? "Muted" : "Unmuted");
    return ok;
}

// ===== Filter preset (override base) =====

bool HalCs4398::setFilterPreset(uint8_t preset) {
    if (preset >= CS4398_FILTER_COUNT) {
        // CS4398 only has 3 presets (0-2); reject invalid values
        LOG_W("[HAL:CS4398] Invalid filter preset %u (CS4398 supports 0-2 only)", preset);
        return false;
    }
    _filterPreset = preset;
    if (_initialized) {
        // Read-modify-write: preserve ramp bits while updating FILT_SEL field
        uint8_t rampFilt = _readReg8(CS4398_REG_RAMP_FILT);
        rampFilt = (uint8_t)((rampFilt & (uint8_t)~CS4398_FILTER_MASK) |
                             ((uint8_t)(preset & 0x03) << CS4398_FILTER_SHIFT));
        _writeReg8(CS4398_REG_RAMP_FILT, rampFilt);
    }
    LOG_I("[HAL:CS4398] Filter preset: %u", preset);
    return true;
}

#endif // DAC_ENABLED
