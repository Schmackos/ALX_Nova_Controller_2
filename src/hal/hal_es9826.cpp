#ifdef DAC_ENABLED
// HalEs9826 — ESS ES9826 2-channel ADC implementation
// Register map sourced from src/drivers/es9826_regs.h (ESS datasheet).

#include "hal_es9826.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include <Wire.h>
#include <Arduino.h>
#include "../debug_serial.h"
#include "../i2s_audio.h"
#include "../drivers/es9826_regs.h"
#else
// ===== Native test stubs — no hardware access =====
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)

// Register address constants (mirrors es9826_regs.h for native compilation;
// all actual hardware writes are inside #ifndef NATIVE_TEST blocks below).
#define ES9826_I2C_ADDR           0x40
#define ES9826_CHIP_ID            0x8A
#define ES9826_REG_SYS_CONFIG     0x00
#define ES9826_REG_CH1_VOL_LSB    0x2D
#define ES9826_REG_CH1_VOL_MSB    0x2E
#define ES9826_REG_CH2_VOL_LSB    0x2F
#define ES9826_REG_CH2_VOL_MSB    0x30
#define ES9826_REG_PGA_GAIN       0x44
#define ES9826_REG_FILTER         0x3B
#define ES9826_REG_CHIP_ID        0xE1
#define ES9826_SOFT_RESET_BIT     0x80
#define ES9826_FILTER_SHAPE_SHIFT 2
#define ES9826_FILTER_SHAPE_MASK  0x1C
#define ES9826_VOL_MUTE           0x0000
#define ES9826_VOL_0DB            0x7FFF
#define ES9826_PGA_MAX_DB         30
#define ES9826_PGA_STEP_DB        3
#define ES9826_PGA_MAX_NIBBLE     10
// Stub I2S port callbacks under native test
inline uint32_t i2s_audio_port0_read(int32_t*, uint32_t) { return 0; }
inline uint32_t i2s_audio_port1_read(int32_t*, uint32_t) { return 0; }
inline bool     i2s_audio_port0_active(void) { return false; }
inline bool     i2s_audio_port1_active(void) { return false; }
inline uint32_t i2s_audio_get_sample_rate(void) { return 48000; }
#endif // NATIVE_TEST

// ===== Constructor =====

HalEs9826::HalEs9826() : HalEssSabreAdcBase() {
    hal_init_descriptor(_descriptor, "ess,es9826", "ES9826", "ESS Technology",
        HAL_DEV_ADC, 2, ES9826_I2C_ADDR, HAL_BUS_I2C, HAL_I2C_BUS_EXP,
        HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K,
        HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL);
    _initPriority = HAL_PRIORITY_HARDWARE;
}

// ===== HalDevice lifecycle =====

bool HalEs9826::probe() {
#ifndef NATIVE_TEST
    if (!_wire) return false;
    _wire->beginTransmission(_i2cAddr);
    uint8_t err = _wire->endTransmission();
    if (err != 0) return false;
    uint8_t chipId = _readReg(ES9826_REG_CHIP_ID);
    return (chipId == ES9826_CHIP_ID);
#else
    return true;
#endif
}

HalInitResult HalEs9826::init() {
    // ---- 1. Read per-device config overrides from HAL Device Manager ----
    _applyConfigOverrides();

    LOG_I("[HAL:ES9826] Initializing (I2C addr=0x%02X bus=%u SDA=%d SCL=%d sr=%luHz bits=%u)",
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin, (unsigned long)_sampleRate, _bitDepth);

    // ---- 2. Select TwoWire instance and initialize I2C bus ----
    _selectWire();

    // ---- 3. Verify chip ID (read reg 0xE1, expect 0x8A) ----
    uint8_t chipId = _readReg(ES9826_REG_CHIP_ID);
    if (chipId != ES9826_CHIP_ID) {
        LOG_W("[HAL:ES9826] Unexpected chip ID: 0x%02X (expected 0x%02X) — continuing",
              chipId, ES9826_CHIP_ID);
    } else {
        LOG_I("[HAL:ES9826] Chip ID OK (0x%02X)", chipId);
    }

    // ---- 4. Soft reset (reg 0x00 bit7, self-clearing) ----
    _writeReg(ES9826_REG_SYS_CONFIG, ES9826_SOFT_RESET_BIT);
#ifndef NATIVE_TEST
    delay(ESS_SABRE_RESET_DELAY_MS);
#endif

    // ---- 5. Set unity gain volume on both channels (0x7FFF = 0 dB) ----
    _writeReg16(ES9826_REG_CH1_VOL_LSB, ES9826_VOL_0DB);
    _writeReg16(ES9826_REG_CH2_VOL_LSB, ES9826_VOL_0DB);

    // ---- 6. PGA gain — nibble-packed per-channel ----
    // Clamp gain to max 30dB, round down to nearest 3dB step
    uint8_t gainDb     = (_gainDb > ES9826_PGA_MAX_DB) ? ES9826_PGA_MAX_DB : _gainDb;
    gainDb             = (gainDb / ES9826_PGA_STEP_DB) * ES9826_PGA_STEP_DB;
    uint8_t nibble     = gainDb / ES9826_PGA_STEP_DB;
    if (nibble > ES9826_PGA_MAX_NIBBLE) nibble = ES9826_PGA_MAX_NIBBLE;
    uint8_t gainReg    = (uint8_t)((nibble << 4) | nibble);  // same value for CH1 and CH2
    _writeReg(ES9826_REG_PGA_GAIN, gainReg);
    _gainDb = (uint8_t)(nibble * ES9826_PGA_STEP_DB);

    // ---- 7. Digital filter preset ----
    // REG_FILTER (0x3B) bits[4:2] = FILTER_SHAPE
    uint8_t preset    = (_filterPreset > 7) ? 7 : _filterPreset;
    uint8_t filterReg = (uint8_t)((preset & 0x07) << ES9826_FILTER_SHAPE_SHIFT);
    _writeReg(ES9826_REG_FILTER, filterReg);

    // ---- 8. Populate AudioInputSource (port-indexed I2S callbacks) ----
    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);
    memset(&_inputSrc, 0, sizeof(_inputSrc));
    _inputSrc.name          = _descriptor.name;
    _inputSrc.isHardwareAdc = true;
    _inputSrc.gainLinear    = 1.0f;
    _inputSrc.vuL           = -90.0f;
    _inputSrc.vuR           = -90.0f;

    uint8_t port = (cfg && cfg->valid && cfg->i2sPort != 255) ? cfg->i2sPort : 2;
#ifndef NATIVE_TEST
    if (port == 0) {
        _inputSrc.read     = i2s_audio_port0_read;
        _inputSrc.isActive = i2s_audio_port0_active;
    } else if (port == 1) {
        _inputSrc.read     = i2s_audio_port1_read;
        _inputSrc.isActive = i2s_audio_port1_active;
    } else {
        _inputSrc.read     = i2s_audio_port2_read;
        _inputSrc.isActive = i2s_audio_port2_active;
    }
    _inputSrc.getSampleRate = i2s_audio_get_sample_rate;
#endif
    _inputSrcReady = true;

    // ---- 9. Mark device ready ----
    _initialized = true;
    _state = HAL_STATE_AVAILABLE;
    setReady(true);

    LOG_I("[HAL:ES9826] Ready (i2s port=%u gain=%ddB filter=%u)",
          port, _gainDb, _filterPreset);
    return hal_init_ok();
}

void HalEs9826::deinit() {
    if (!_initialized) return;

    setReady(false);

    _initialized   = false;
    _inputSrcReady = false;
    _state = HAL_STATE_REMOVED;

    LOG_I("[HAL:ES9826] Deinitialized");
}

void HalEs9826::dumpConfig() {
    LOG_I("[HAL:ES9826] %s by %s (compat=%s) i2c=0x%02X bus=%u sda=%d scl=%d "
          "sr=%luHz bits=%u gain=%ddB hpf=%d filter=%u",
          _descriptor.name, _descriptor.manufacturer, _descriptor.compatible,
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth, _gainDb,
          (int)_hpfEnabled, _filterPreset);
}

bool HalEs9826::healthCheck() {
#ifndef NATIVE_TEST
    if (!_wire || !_initialized) return false;
    uint8_t id = _readReg(ES9826_REG_CHIP_ID);
    return (id == ES9826_CHIP_ID);
#else
    return _initialized;
#endif
}

// ===== HalAudioDevice =====

bool HalEs9826::configure(uint32_t sampleRate, uint8_t bitDepth) {
    const uint32_t supported[] = { 44100, 48000, 96000, 192000 };
    if (!_validateSampleRate(sampleRate, supported, 4)) {
        LOG_W("[HAL:ES9826] Unsupported sample rate: %luHz", (unsigned long)sampleRate);
        return false;
    }
    _sampleRate = sampleRate;
    _bitDepth   = bitDepth;
    LOG_I("[HAL:ES9826] Configured: %luHz %ubit", (unsigned long)sampleRate, bitDepth);
    return true;
}

bool HalEs9826::setVolume(uint8_t percent) {
    if (!_initialized) return false;
    if (percent > 100) percent = 100;
    uint16_t vol16 = (uint16_t)(((uint32_t)percent * ES9826_VOL_0DB) / 100);
    bool ok  = _writeReg16(ES9826_REG_CH1_VOL_LSB, vol16);
    ok       = ok && _writeReg16(ES9826_REG_CH2_VOL_LSB, vol16);
    LOG_D("[HAL:ES9826] Volume: %d%% -> 0x%04X", percent, vol16);
    return ok;
}

bool HalEs9826::setMute(bool mute) {
    if (!_initialized) return false;
    uint16_t vol16 = mute ? ES9826_VOL_MUTE : ES9826_VOL_0DB;
    bool ok  = _writeReg16(ES9826_REG_CH1_VOL_LSB, vol16);
    ok       = ok && _writeReg16(ES9826_REG_CH2_VOL_LSB, vol16);
    LOG_I("[HAL:ES9826] %s", mute ? "Muted" : "Unmuted");
    return ok;
}

// ===== HalAudioAdcInterface =====

bool HalEs9826::adcSetGain(uint8_t gainDb) {
    if (!_initialized) return false;
    // Clamp to max 30dB, round down to nearest 3dB step
    if (gainDb > ES9826_PGA_MAX_DB) gainDb = ES9826_PGA_MAX_DB;
    gainDb           = (gainDb / ES9826_PGA_STEP_DB) * ES9826_PGA_STEP_DB;
    uint8_t nibble   = gainDb / ES9826_PGA_STEP_DB;
    if (nibble > ES9826_PGA_MAX_NIBBLE) nibble = ES9826_PGA_MAX_NIBBLE;
    uint8_t gainReg  = (uint8_t)((nibble << 4) | nibble);
    bool ok          = _writeReg(ES9826_REG_PGA_GAIN, gainReg);
    _gainDb          = (uint8_t)(nibble * ES9826_PGA_STEP_DB);
    LOG_I("[HAL:ES9826] ADC gain: %ddB (reg=0x%02X)", _gainDb, gainReg);
    return ok;
}

bool HalEs9826::adcSetHpfEnabled(bool en) {
    // ES9826 has no dedicated HPF register — store flag only.
    _hpfEnabled = en;
    LOG_I("[HAL:ES9826] HPF: %s (stored, no hardware register)", en ? "enabled" : "disabled");
    return true;
}

bool HalEs9826::adcSetSampleRate(uint32_t hz) {
    return configure(hz, _bitDepth);
}

// ===== ES9826-specific extensions =====

bool HalEs9826::setFilterPreset(uint8_t preset) {
    if (preset > 7) return false;
    if (!_initialized) {
        _filterPreset = preset;
        return true;
    }
    uint8_t filterReg = (uint8_t)((preset & 0x07) << ES9826_FILTER_SHAPE_SHIFT);
    // Preserve bits outside the FILTER_SHAPE field
    uint8_t cur = _readReg(ES9826_REG_FILTER);
    uint8_t val = (uint8_t)((cur & ~ES9826_FILTER_SHAPE_MASK) | filterReg);
    bool ok = _writeReg(ES9826_REG_FILTER, val);
    if (ok) _filterPreset = preset;
    LOG_I("[HAL:ES9826] Filter preset: %u", preset);
    return ok;
}

// ===== AudioInputSource =====

const AudioInputSource* HalEs9826::getInputSource() const {
    return _inputSrcReady ? &_inputSrc : nullptr;
}

#endif // DAC_ENABLED
