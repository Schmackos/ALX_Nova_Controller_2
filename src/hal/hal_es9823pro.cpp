#ifdef DAC_ENABLED
// HalEs9823pro — ESS ES9823PRO / ES9823MPRO 2-channel 32-bit SMART ADC implementation
// Register map sourced from src/drivers/es9823pro_regs.h (ESS datasheet).
// Both chip variants share this driver: ES9823PRO (chip ID 0x8D), ES9823MPRO (chip ID 0x8C).

#include "hal_es9823pro.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include <Wire.h>
#include "hal_ess_sabre_adc_base.h"  // for extern TwoWire Wire2
#include <Arduino.h>
#include "../debug_serial.h"
#include "../i2s_audio.h"
#include "../drivers/es9823pro_regs.h"
#else
// ===== Native test stubs — no hardware access =====
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)

// Register address constants (mirrors es9823pro_regs.h for native compilation;
// all actual hardware writes are inside #ifndef NATIVE_TEST blocks below).
#define ES9823PRO_I2C_ADDR                0x40
#define ES9823PRO_CHIP_ID                 0x8D
#define ES9823MPRO_CHIP_ID                0x8C
#define ES9823PRO_REG_SYS_CONFIG          0x00
#define ES9823PRO_REG_ADC_CLOCK_CONFIG1   0x01
#define ES9823PRO_REG_FILTER_SHAPE        0x4A
#define ES9823PRO_REG_CH1_VOLUME_LSB      0x51
#define ES9823PRO_REG_CH1_VOLUME_MSB      0x52
#define ES9823PRO_REG_CH2_VOLUME_LSB      0x53
#define ES9823PRO_REG_CH2_VOLUME_MSB      0x54
#define ES9823PRO_REG_DIGITAL_GAIN        0x55
#define ES9823PRO_REG_VOL_RAMP_RATE_UP    0x58
#define ES9823PRO_REG_VOL_RAMP_RATE_DOWN  0x59
#define ES9823PRO_REG_CHIP_ID             0xE1
#define ES9823PRO_SOFT_RESET_BIT          0x80
#define ES9823PRO_OUTPUT_I2S              0x00
#define ES9823PRO_CH1_GAIN_SHIFT          0
#define ES9823PRO_CH2_GAIN_SHIFT          4
#define ES9823PRO_CH_GAIN_MASK            0x07
#define ES9823PRO_FILTER_SHIFT            5
#define ES9823PRO_FILTER_MASK             0x07
#define ES9823PRO_GAIN_MAX_DB             42
#define ES9823PRO_VOL_0DB                 0x7FFF
#define ES9823PRO_VOL_MUTE                0x0000
#define ES9823PRO_CLOCK_ENABLE_2CH        0x33
// Stub I2S port callbacks under native test
inline uint32_t i2s_audio_port0_read(int32_t*, uint32_t) { return 0; }
inline uint32_t i2s_audio_port1_read(int32_t*, uint32_t) { return 0; }
inline bool     i2s_audio_port0_active(void) { return false; }
inline bool     i2s_audio_port1_active(void) { return false; }
inline uint32_t i2s_audio_get_sample_rate(void) { return 48000; }
#endif // NATIVE_TEST

// ===== Constructor =====

HalEs9823pro::HalEs9823pro() : HalAudioDevice() {
    memset(&_descriptor, 0, sizeof(_descriptor));
    strncpy(_descriptor.compatible,   "ess,es9823pro",     31);
    strncpy(_descriptor.name,         "ES9823PRO",         32);
    strncpy(_descriptor.manufacturer, "ESS Technology",    32);
    _descriptor.type            = HAL_DEV_ADC;
    _descriptor.legacyId        = 0;
    _descriptor.channelCount    = 2;
    _descriptor.i2cAddr         = 0x40;
    _descriptor.bus.type        = HAL_BUS_I2C;
    _descriptor.bus.index       = HAL_I2C_BUS_EXP;
    _descriptor.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K;
    _descriptor.capabilities    = HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL;
    _initPriority = HAL_PRIORITY_HARDWARE;
}

// ===== I2C helpers =====

bool HalEs9823pro::_writeReg(uint8_t reg, uint8_t val) {
#ifndef NATIVE_TEST
    if (!_wire) return false;
    _wire->beginTransmission(_i2cAddr);
    _wire->write(reg);
    _wire->write(val);
    uint8_t err = _wire->endTransmission();
    if (err != 0) {
        LOG_E("[HAL:ES9823PRO] I2C write failed: reg=0x%02X val=0x%02X err=%d", reg, val, err);
        return false;
    }
    return true;
#else
    (void)reg; (void)val;
    return true;
#endif
}

// ES9823PRO multi-byte: write LSB at regLsb, MSB at regLsb+1 (MSB write latches both).
bool HalEs9823pro::_writeReg16(uint8_t regLsb, uint16_t val) {
    bool ok  = _writeReg(regLsb,     (uint8_t)(val & 0xFF));
    ok       = ok && _writeReg(regLsb + 1, (uint8_t)((val >> 8) & 0xFF));
    return ok;
}

uint8_t HalEs9823pro::_readReg(uint8_t reg) {
#ifndef NATIVE_TEST
    if (!_wire) return 0xFF;
    _wire->beginTransmission(_i2cAddr);
    _wire->write(reg);
    _wire->endTransmission(false);
    _wire->requestFrom(_i2cAddr, (uint8_t)1);
    if (_wire->available()) return _wire->read();
    LOG_E("[HAL:ES9823PRO] I2C read failed: reg=0x%02X", reg);
    return 0xFF;
#else
    (void)reg;
    return 0x00;
#endif
}

// ===== HalDevice lifecycle =====

bool HalEs9823pro::probe() {
#ifndef NATIVE_TEST
    if (!_wire) return false;
    _wire->beginTransmission(_i2cAddr);
    uint8_t err = _wire->endTransmission();
    if (err != 0) return false;
    uint8_t chipId = _readReg(ES9823PRO_REG_CHIP_ID);
    return (chipId == ES9823PRO_CHIP_ID || chipId == ES9823MPRO_CHIP_ID);
#else
    return true;
#endif
}

HalInitResult HalEs9823pro::init() {
    // ---- 1. Read per-device config overrides from HAL Device Manager ----
    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);
    if (cfg && cfg->valid) {
        if (cfg->i2cAddr != 0)     _i2cAddr      = cfg->i2cAddr;
        if (cfg->i2cBusIndex != 0) _i2cBusIndex  = cfg->i2cBusIndex;
        if (cfg->pinSda >= 0)      _sdaPin        = cfg->pinSda;
        if (cfg->pinScl >= 0)      _sclPin        = cfg->pinScl;
        if (cfg->sampleRate > 0)   _sampleRate    = cfg->sampleRate;
        if (cfg->bitDepth > 0)     _bitDepth      = cfg->bitDepth;
        if (cfg->pgaGain <= ES9823PRO_GAIN_MAX_DB) _gainDb = cfg->pgaGain;
        _hpfEnabled   = cfg->hpfEnabled;
        _filterPreset = cfg->filterMode;   // 0-7 stored in filterMode field
    }

    LOG_I("[HAL:ES9823PRO] Initializing (I2C addr=0x%02X bus=%u SDA=%d SCL=%d sr=%luHz bits=%u)",
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin, (unsigned long)_sampleRate, _bitDepth);

#ifndef NATIVE_TEST
    // ---- 2. Select TwoWire instance based on i2cBusIndex ----
    switch (_i2cBusIndex) {
        case 1:  _wire = &Wire1; break;
        case 2:  _wire = &Wire2; break;
        default: _wire = &Wire;  break;
    }

    // ---- 3. Initialize I2C bus at 400 kHz ----
    _wire->begin((int)_sdaPin, (int)_sclPin, (uint32_t)400000);
    LOG_I("[HAL:ES9823PRO] I2C initialized (bus %u SDA=%d SCL=%d 400kHz)",
          _i2cBusIndex, _sdaPin, _sclPin);
#endif

    // ---- 4. Verify chip ID (reg 0xE1, accept 0x8C or 0x8D) ----
    uint8_t chipId = _readReg(ES9823PRO_REG_CHIP_ID);
    if (chipId == ES9823MPRO_CHIP_ID) {
        _isMonolithic = true;
        // Update compatible + name to reflect actual variant detected
        strncpy(_descriptor.compatible, "ess,es9823mpro", 31);
        strncpy(_descriptor.name,       "ES9823MPRO",     32);
        LOG_I("[HAL:ES9823PRO] ES9823MPRO detected (chip ID 0x8C)");
    } else if (chipId == ES9823PRO_CHIP_ID) {
        _isMonolithic = false;
        LOG_I("[HAL:ES9823PRO] Chip ID OK (0x%02X)", chipId);
    } else {
        LOG_W("[HAL:ES9823PRO] Unexpected chip ID: 0x%02X (expected 0x8C or 0x8D) — continuing",
              chipId);
    }

    // ---- 5. Soft reset (reg 0x00 bit7, self-clearing) ----
    _writeReg(ES9823PRO_REG_SYS_CONFIG, ES9823PRO_SOFT_RESET_BIT);
#ifndef NATIVE_TEST
    delay(5);   // Allow reset to complete before continuing
#endif

    // ---- 6. Enable ADC clocks: CH1 + CH2 data input + decimation ----
    _writeReg(ES9823PRO_REG_ADC_CLOCK_CONFIG1, ES9823PRO_CLOCK_ENABLE_2CH);

    // ---- 7. Configure I2S output format (bits6:5 = 0b00 → I2S/Philips slave) ----
    _writeReg(ES9823PRO_REG_SYS_CONFIG, ES9823PRO_OUTPUT_I2S);

    // ---- 8. Disable volume ramp (instant response) ----
    _writeReg(ES9823PRO_REG_VOL_RAMP_RATE_UP,   0x00);
    _writeReg(ES9823PRO_REG_VOL_RAMP_RATE_DOWN, 0x00);

    // ---- 9. Set unity gain volume on both channels (0x7FFF = 0 dB) ----
    _writeReg16(ES9823PRO_REG_CH1_VOLUME_LSB, ES9823PRO_VOL_0DB);
    _writeReg16(ES9823PRO_REG_CH2_VOLUME_LSB, ES9823PRO_VOL_0DB);

    // ---- 10. Digital gain (reg 0x55: bits[2:0]=CH1_GAIN, bits[6:4]=CH2_GAIN) ----
    // Round to nearest 6 dB step, clamp to 42 dB max (register max = 7)
    uint8_t gainStep = (_gainDb + 3) / 6;   // round to nearest step
    if (gainStep > 7) gainStep = 7;
    uint8_t gainReg = (uint8_t)((gainStep & ES9823PRO_CH_GAIN_MASK) |
                                ((gainStep & ES9823PRO_CH_GAIN_MASK) << ES9823PRO_CH2_GAIN_SHIFT));
    _writeReg(ES9823PRO_REG_DIGITAL_GAIN, gainReg);
    _gainDb = (uint8_t)(gainStep * 6);

    // ---- 11. HPF: no dedicated register — flag stored for API compatibility ----
    // ES9823PRO does not expose a DC-blocking register in the known map;
    // _hpfEnabled is tracked but no register write is performed.

    // ---- 12. Digital filter preset (reg 0x4A bits[7:5] = FILTER_SHAPE) ----
    uint8_t preset = (_filterPreset > 7) ? 7 : _filterPreset;
    uint8_t filterReg = (uint8_t)((preset & ES9823PRO_FILTER_MASK) << ES9823PRO_FILTER_SHIFT);
    _writeReg(ES9823PRO_REG_FILTER_SHAPE, filterReg);

    // ---- 13. Populate AudioInputSource (port-indexed I2S callbacks) ----
    memset(&_inputSrc, 0, sizeof(_inputSrc));
    _inputSrc.name          = _descriptor.name;
    _inputSrc.isHardwareAdc = true;
    _inputSrc.gainLinear    = 1.0f;
    _inputSrc.vuL           = -90.0f;
    _inputSrc.vuR           = -90.0f;

    uint8_t port = (cfg && cfg->valid && cfg->i2sPort != 255) ? cfg->i2sPort : 2; // Default port 2 for expansion
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

    // ---- 14. Mark device ready ----
    _initialized = true;
    _state = HAL_STATE_AVAILABLE;
    _ready = true;

    LOG_I("[HAL:ES9823PRO] Ready (%s i2s port=%u gain=%ddB hpf=%d filter=%u)",
          _isMonolithic ? "ES9823MPRO" : "ES9823PRO",
          port, _gainDb, (int)_hpfEnabled, _filterPreset);
    return hal_init_ok();
}

void HalEs9823pro::deinit() {
    if (!_initialized) return;

    _ready = false;

#ifndef NATIVE_TEST
    // Power down: disable ADC clocks (reg 0x01 = 0x00)
    _writeReg(ES9823PRO_REG_ADC_CLOCK_CONFIG1, 0x00);
#endif

    _initialized   = false;
    _inputSrcReady = false;
    _state = HAL_STATE_REMOVED;

    LOG_I("[HAL:ES9823PRO] Deinitialized");
}

void HalEs9823pro::dumpConfig() {
    LOG_I("[HAL:ES9823PRO] %s by %s (compat=%s) i2c=0x%02X bus=%u sda=%d scl=%d "
          "sr=%luHz bits=%u gain=%ddB hpf=%d filter=%u monolithic=%d",
          _descriptor.name, _descriptor.manufacturer, _descriptor.compatible,
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth, _gainDb,
          (int)_hpfEnabled, _filterPreset, (int)_isMonolithic);
}

bool HalEs9823pro::healthCheck() {
#ifndef NATIVE_TEST
    if (!_wire || !_initialized) return false;
    uint8_t id = _readReg(ES9823PRO_REG_CHIP_ID);
    return (id == ES9823PRO_CHIP_ID || id == ES9823MPRO_CHIP_ID);
#else
    return _initialized;
#endif
}

// ===== HalAudioDevice =====

bool HalEs9823pro::configure(uint32_t sampleRate, uint8_t bitDepth) {
    // ES9823PRO derives sample rate from MCLK ratio; validate against supported set.
    const uint32_t supported[] = { 44100, 48000, 96000, 192000 };
    bool valid = false;
    for (uint8_t i = 0; i < 4; i++) {
        if (sampleRate == supported[i]) { valid = true; break; }
    }
    if (!valid) {
        LOG_W("[HAL:ES9823PRO] Unsupported sample rate: %luHz", (unsigned long)sampleRate);
        return false;
    }
    if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32) {
        LOG_W("[HAL:ES9823PRO] Unsupported bit depth: %u", bitDepth);
        return false;
    }
    _sampleRate = sampleRate;
    _bitDepth   = bitDepth;
    LOG_I("[HAL:ES9823PRO] Configured: %luHz %ubit", (unsigned long)sampleRate, bitDepth);
    return true;
}

bool HalEs9823pro::setVolume(uint8_t percent) {
    if (!_initialized) return false;
    if (percent > 100) percent = 100;
    // Map 0% = 0x0000 (mute), 100% = 0x7FFF (0 dB), linear
    uint16_t vol16 = (uint16_t)(((uint32_t)percent * ES9823PRO_VOL_0DB) / 100);
    bool ok  = _writeReg16(ES9823PRO_REG_CH1_VOLUME_LSB, vol16);
    ok       = ok && _writeReg16(ES9823PRO_REG_CH2_VOLUME_LSB, vol16);
    LOG_D("[HAL:ES9823PRO] Volume: %d%% -> 0x%04X", percent, vol16);
    return ok;
}

bool HalEs9823pro::setMute(bool mute) {
    if (!_initialized) return false;
    uint16_t vol16 = mute ? ES9823PRO_VOL_MUTE : ES9823PRO_VOL_0DB;
    bool ok  = _writeReg16(ES9823PRO_REG_CH1_VOLUME_LSB, vol16);
    ok       = ok && _writeReg16(ES9823PRO_REG_CH2_VOLUME_LSB, vol16);
    LOG_I("[HAL:ES9823PRO] %s", mute ? "Muted" : "Unmuted");
    return ok;
}

// ===== HalAudioAdcInterface =====

bool HalEs9823pro::adcSetGain(uint8_t gainDb) {
    if (!_initialized) return false;
    // Valid steps: 0, 6, 12, 18, 24, 30, 36, 42 dB (6 dB increments, 3-bit register)
    if (gainDb > ES9823PRO_GAIN_MAX_DB) {
        LOG_W("[HAL:ES9823PRO] adcSetGain: %ddB exceeds max %ddB", gainDb, ES9823PRO_GAIN_MAX_DB);
        return false;
    }
    // Round to nearest 6 dB step
    uint8_t gainStep = (gainDb + 3) / 6;
    if (gainStep > 7) gainStep = 7;
    uint8_t gainReg = (uint8_t)((gainStep & ES9823PRO_CH_GAIN_MASK) |
                                ((gainStep & ES9823PRO_CH_GAIN_MASK) << ES9823PRO_CH2_GAIN_SHIFT));
    bool ok = _writeReg(ES9823PRO_REG_DIGITAL_GAIN, gainReg);
    _gainDb = (uint8_t)(gainStep * 6);
    LOG_I("[HAL:ES9823PRO] ADC gain: %ddB (reg=0x%02X)", _gainDb, gainReg);
    return ok;
}

bool HalEs9823pro::adcSetHpfEnabled(bool en) {
    if (!_initialized) return false;
    // ES9823PRO does not expose a DC-blocking register in the known register map.
    // Flag is stored for API compatibility and dumpConfig() reporting.
    _hpfEnabled = en;
    LOG_I("[HAL:ES9823PRO] HPF: %s (flag only — no dedicated HPF register)", en ? "enabled" : "disabled");
    return true;
}

bool HalEs9823pro::adcSetSampleRate(uint32_t hz) {
    return configure(hz, _bitDepth);
}

// ===== ES9823PRO-specific extensions =====

bool HalEs9823pro::setFilterPreset(uint8_t preset) {
    if (!_initialized) return false;
    if (preset > 7) {
        LOG_W("[HAL:ES9823PRO] setFilterPreset: preset %u out of range (0-7)", preset);
        return false;
    }
    // FILTER_SHAPE lives in bits[7:5] of REG_FILTER_SHAPE (0x4A)
    uint8_t filterReg = (uint8_t)((preset & ES9823PRO_FILTER_MASK) << ES9823PRO_FILTER_SHIFT);
    bool ok = _writeReg(ES9823PRO_REG_FILTER_SHAPE, filterReg);
    _filterPreset = preset;
    LOG_I("[HAL:ES9823PRO] Filter preset: %u", preset);
    return ok;
}

bool HalEs9823pro::setChannelVolume(uint8_t channel, uint16_t vol16) {
    if (!_initialized) return false;
    bool ok = false;
    if (channel == 0 || channel == 1) {
        uint8_t regLsb = (channel == 0) ? ES9823PRO_REG_CH1_VOLUME_LSB
                                        : ES9823PRO_REG_CH2_VOLUME_LSB;
        ok = _writeReg16(regLsb, vol16);
        LOG_D("[HAL:ES9823PRO] Channel %u volume: 0x%04X", channel, vol16);
    } else {
        LOG_W("[HAL:ES9823PRO] setChannelVolume: invalid channel %u (0 or 1 only)", channel);
    }
    return ok;
}

// ===== AudioInputSource =====

const AudioInputSource* HalEs9823pro::getInputSource() const {
    return _inputSrcReady ? &_inputSrc : nullptr;
}

#endif // DAC_ENABLED
