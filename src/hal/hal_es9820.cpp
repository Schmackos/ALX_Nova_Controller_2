#ifdef DAC_ENABLED
// HalEs9820 — ESS ES9820 2-channel 32-bit ADC implementation
// Register map sourced from src/drivers/es9820_regs.h (ESS datasheet).
// Register layout for CH1/CH2 data path matches ES9822PRO (HPF at 0x65 bit2, 0x76 bit2;
// gain at 0x70/0x81 bits[1:0]; filter at 0x71/0x82 bits[4:2]).

#include "hal_es9820.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include <Wire.h>
#include "hal_ess_sabre_adc_base.h"  // for extern TwoWire Wire2
#include <Arduino.h>
#include "../debug_serial.h"
#include "../i2s_audio.h"
#include "../drivers/es9820_regs.h"
#else
// ===== Native test stubs — no hardware access =====
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)

// Register address constants (mirrors es9820_regs.h for native compilation;
// all actual hardware writes are inside #ifndef NATIVE_TEST blocks below).
#define ES9820_I2C_ADDR                   0x40
#define ES9820_CHIP_ID                    0x84
#define ES9820_REG_SYS_CONFIG             0x00
#define ES9820_REG_CH1_DATAPATH           0x65
#define ES9820_REG_CH1_VOLUME_LSB         0x6D
#define ES9820_REG_CH1_VOLUME_MSB         0x6E
#define ES9820_REG_CH1_GAIN               0x70
#define ES9820_REG_CH1_FILTER             0x71
#define ES9820_REG_CH2_DATAPATH           0x76
#define ES9820_REG_CH2_VOLUME_LSB         0x7E
#define ES9820_REG_CH2_VOLUME_MSB         0x7F
#define ES9820_REG_CH2_GAIN               0x81
#define ES9820_REG_CH2_FILTER             0x82
#define ES9820_REG_CHIP_ID                0xE1
#define ES9820_SOFT_RESET_BIT             0x80
#define ES9820_OUTPUT_I2S                 0x00
#define ES9820_HPF_ENABLE_BIT             0x04
#define ES9820_FILTER_SHIFT               2
#define ES9820_FILTER_MASK                0x07
#define ES9820_GAIN_MAX_DB                18
#define ES9820_VOL_0DB                    0x7FFF
#define ES9820_VOL_MUTE                   0x0000
#define ES9820_CLOCK_ENABLE_2CH           0x33
// Stub I2S port callbacks under native test
inline uint32_t i2s_audio_port0_read(int32_t*, uint32_t) { return 0; }
inline uint32_t i2s_audio_port1_read(int32_t*, uint32_t) { return 0; }
inline bool     i2s_audio_port0_active(void) { return false; }
inline bool     i2s_audio_port1_active(void) { return false; }
inline uint32_t i2s_audio_get_sample_rate(void) { return 48000; }
#endif // NATIVE_TEST

// ===== Constructor =====

HalEs9820::HalEs9820() : HalAudioDevice() {
    hal_init_descriptor(_descriptor, "ess,es9820", "ES9820", "ESS Technology",
        HAL_DEV_ADC, 2, 0x40, HAL_BUS_I2C, HAL_I2C_BUS_EXP,
        HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K,
        HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL);
    _initPriority = HAL_PRIORITY_HARDWARE;
}

// ===== I2C helpers =====

bool HalEs9820::_writeReg(uint8_t reg, uint8_t val) {
#ifndef NATIVE_TEST
    if (!_wire) return false;
    _wire->beginTransmission(_i2cAddr);
    _wire->write(reg);
    _wire->write(val);
    uint8_t err = _wire->endTransmission();
    if (err != 0) {
        LOG_E("[HAL:ES9820] I2C write failed: reg=0x%02X val=0x%02X err=%d", reg, val, err);
        return false;
    }
    return true;
#else
    (void)reg; (void)val;
    return true;
#endif
}

// ES9820 multi-byte: write LSB at regLsb, MSB at regLsb+1 (MSB write latches both).
bool HalEs9820::_writeReg16(uint8_t regLsb, uint16_t val) {
    bool ok  = _writeReg(regLsb,     (uint8_t)(val & 0xFF));
    ok       = ok && _writeReg(regLsb + 1, (uint8_t)((val >> 8) & 0xFF));
    return ok;
}

uint8_t HalEs9820::_readReg(uint8_t reg) {
#ifndef NATIVE_TEST
    if (!_wire) return 0xFF;
    _wire->beginTransmission(_i2cAddr);
    _wire->write(reg);
    _wire->endTransmission(false);
    _wire->requestFrom(_i2cAddr, (uint8_t)1);
    if (_wire->available()) return _wire->read();
    LOG_E("[HAL:ES9820] I2C read failed: reg=0x%02X", reg);
    return 0xFF;
#else
    (void)reg;
    return 0x00;
#endif
}

// ===== HalDevice lifecycle =====

bool HalEs9820::probe() {
#ifndef NATIVE_TEST
    if (!_wire) return false;
    _wire->beginTransmission(_i2cAddr);
    uint8_t err = _wire->endTransmission();
    if (err != 0) return false;
    uint8_t chipId = _readReg(ES9820_REG_CHIP_ID);
    return (chipId == ES9820_CHIP_ID);
#else
    return true;
#endif
}

HalInitResult HalEs9820::init() {
    // ---- 1. Read per-device config overrides from HAL Device Manager ----
    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);
    if (cfg && cfg->valid) {
        if (cfg->i2cAddr != 0)     _i2cAddr     = cfg->i2cAddr;
        if (cfg->i2cBusIndex != 0) _i2cBusIndex = cfg->i2cBusIndex;
        if (cfg->pinSda >= 0)      _sdaPin       = cfg->pinSda;
        if (cfg->pinScl >= 0)      _sclPin       = cfg->pinScl;
        if (cfg->sampleRate > 0)   _sampleRate   = cfg->sampleRate;
        if (cfg->bitDepth > 0)     _bitDepth     = cfg->bitDepth;
        if (cfg->pgaGain <= ES9820_GAIN_MAX_DB)  _gainDb = cfg->pgaGain;
        _hpfEnabled   = cfg->hpfEnabled;
        _filterPreset = cfg->filterMode;   // 0-7 stored in filterMode field
    }

    LOG_I("[HAL:ES9820] Initializing (I2C addr=0x%02X bus=%u SDA=%d SCL=%d sr=%luHz bits=%u)",
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
    LOG_I("[HAL:ES9820] I2C initialized (bus %u SDA=%d SCL=%d 400kHz)",
          _i2cBusIndex, _sdaPin, _sclPin);
#endif

    // ---- 4. Verify chip ID (read reg 0xE1, expect 0x84) ----
    uint8_t chipId = _readReg(ES9820_REG_CHIP_ID);
    if (chipId != ES9820_CHIP_ID) {
        LOG_W("[HAL:ES9820] Unexpected chip ID: 0x%02X (expected 0x%02X) — continuing",
              chipId, ES9820_CHIP_ID);
    } else {
        LOG_I("[HAL:ES9820] Chip ID OK (0x%02X)", chipId);
    }

    // ---- 5. Soft reset (reg 0x00 bit7, self-clearing) ----
    _writeReg(ES9820_REG_SYS_CONFIG, ES9820_SOFT_RESET_BIT);
#ifndef NATIVE_TEST
    delay(5);   // Allow reset to complete before continuing
#endif

    // ---- 6. Configure I2S output format (bits6:5 = 0b00 → I2S/Philips slave) ----
    _writeReg(ES9820_REG_SYS_CONFIG, ES9820_OUTPUT_I2S);

    // ---- 7. Set unity gain volume on both channels (0x7FFF = 0 dB) ----
    _writeReg16(ES9820_REG_CH1_VOLUME_LSB, ES9820_VOL_0DB);
    _writeReg16(ES9820_REG_CH2_VOLUME_LSB, ES9820_VOL_0DB);

    // ---- 8. DATA_GAIN per channel (bits[1:0] of reg 0x70 / 0x81) ----
    uint8_t gainReg = (_gainDb >= ES9820_GAIN_MAX_DB) ? 3 : (_gainDb / 6);
    if (gainReg > 3) gainReg = 3;
    _writeReg(ES9820_REG_CH1_GAIN, gainReg & 0x03);
    _writeReg(ES9820_REG_CH2_GAIN, gainReg & 0x03);

    // ---- 9. HPF (DC blocking) — same register layout as ES9822PRO ----
    // REG_CH1_DATAPATH (0x65) / REG_CH2_DATAPATH (0x76) bit2 = ENABLE_DC_BLOCKING
    uint8_t datapath = _hpfEnabled ? ES9820_HPF_ENABLE_BIT : 0x00;
    _writeReg(ES9820_REG_CH1_DATAPATH, datapath);
    _writeReg(ES9820_REG_CH2_DATAPATH, datapath);

    // ---- 10. Digital filter preset (bits[4:2] of reg 0x71 / 0x82) ----
    uint8_t preset = (_filterPreset > 7) ? 7 : _filterPreset;
    uint8_t filterReg = (uint8_t)((preset & ES9820_FILTER_MASK) << ES9820_FILTER_SHIFT);
    _writeReg(ES9820_REG_CH1_FILTER, filterReg);
    _writeReg(ES9820_REG_CH2_FILTER, filterReg);

    // ---- 11. Populate AudioInputSource (port-indexed I2S callbacks) ----
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

    // ---- 12. Mark device ready ----
    _initialized = true;
    _state = HAL_STATE_AVAILABLE;
    _ready = true;

    LOG_I("[HAL:ES9820] Ready (i2s port=%u gain=%ddB hpf=%d filter=%u)",
          port, _gainDb, (int)_hpfEnabled, _filterPreset);
    return hal_init_ok();
}

void HalEs9820::deinit() {
    if (!_initialized) return;

    _ready = false;

#ifndef NATIVE_TEST
    // Power down: zero volume on both channels before removing clocks
    _writeReg16(ES9820_REG_CH1_VOLUME_LSB, ES9820_VOL_MUTE);
    _writeReg16(ES9820_REG_CH2_VOLUME_LSB, ES9820_VOL_MUTE);
#endif

    _initialized   = false;
    _inputSrcReady = false;
    _state = HAL_STATE_REMOVED;

    LOG_I("[HAL:ES9820] Deinitialized");
}

void HalEs9820::dumpConfig() {
    LOG_I("[HAL:ES9820] %s by %s (compat=%s) i2c=0x%02X bus=%u sda=%d scl=%d "
          "sr=%luHz bits=%u gain=%ddB hpf=%d filter=%u",
          _descriptor.name, _descriptor.manufacturer, _descriptor.compatible,
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth, _gainDb,
          (int)_hpfEnabled, _filterPreset);
}

bool HalEs9820::healthCheck() {
#ifndef NATIVE_TEST
    if (!_wire || !_initialized) return false;
    uint8_t id = _readReg(ES9820_REG_CHIP_ID);
    return (id == ES9820_CHIP_ID);
#else
    return _initialized;
#endif
}

// ===== HalAudioDevice =====

bool HalEs9820::configure(uint32_t sampleRate, uint8_t bitDepth) {
    const uint32_t supported[] = { 44100, 48000, 96000, 192000 };
    bool valid = false;
    for (uint8_t i = 0; i < 4; i++) {
        if (sampleRate == supported[i]) { valid = true; break; }
    }
    if (!valid) {
        LOG_W("[HAL:ES9820] Unsupported sample rate: %luHz", (unsigned long)sampleRate);
        return false;
    }
    if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32) {
        LOG_W("[HAL:ES9820] Unsupported bit depth: %u", bitDepth);
        return false;
    }
    _sampleRate = sampleRate;
    _bitDepth   = bitDepth;
    LOG_I("[HAL:ES9820] Configured: %luHz %ubit", (unsigned long)sampleRate, bitDepth);
    return true;
}

bool HalEs9820::setVolume(uint8_t percent) {
    if (!_initialized) return false;
    if (percent > 100) percent = 100;
    // Map 0% = 0x0000 (mute), 100% = 0x7FFF (0 dB), linear
    uint16_t vol16 = (uint16_t)(((uint32_t)percent * ES9820_VOL_0DB) / 100);
    bool ok  = _writeReg16(ES9820_REG_CH1_VOLUME_LSB, vol16);
    ok       = ok && _writeReg16(ES9820_REG_CH2_VOLUME_LSB, vol16);
    LOG_D("[HAL:ES9820] Volume: %d%% -> 0x%04X", percent, vol16);
    return ok;
}

bool HalEs9820::setMute(bool mute) {
    if (!_initialized) return false;
    uint16_t vol16 = mute ? ES9820_VOL_MUTE : ES9820_VOL_0DB;
    bool ok  = _writeReg16(ES9820_REG_CH1_VOLUME_LSB, vol16);
    ok       = ok && _writeReg16(ES9820_REG_CH2_VOLUME_LSB, vol16);
    LOG_I("[HAL:ES9820] %s", mute ? "Muted" : "Unmuted");
    return ok;
}

// ===== HalAudioAdcInterface =====

bool HalEs9820::adcSetGain(uint8_t gainDb) {
    if (!_initialized) return false;
    // Valid steps: 0, 6, 12, 18 dB (2-bit DATA_GAIN register, values 0-3)
    if (gainDb > ES9820_GAIN_MAX_DB) {
        LOG_W("[HAL:ES9820] adcSetGain: %ddB exceeds max %ddB", gainDb, ES9820_GAIN_MAX_DB);
        return false;
    }
    uint8_t gainReg = gainDb / 6;   // integer divide: 0→0, 6→1, 7-12→2, 13-18→3 (floor)
    if (gainReg > 3) gainReg = 3;
    bool ok  = _writeReg(ES9820_REG_CH1_GAIN, gainReg & 0x03);
    ok       = ok && _writeReg(ES9820_REG_CH2_GAIN, gainReg & 0x03);
    _gainDb = (uint8_t)(gainReg * 6);
    LOG_I("[HAL:ES9820] ADC gain: %ddB (reg=0x%02X)", _gainDb, gainReg);
    return ok;
}

bool HalEs9820::adcSetHpfEnabled(bool en) {
    if (!_initialized) return false;
    // REG_CH1_DATAPATH (0x65) / REG_CH2_DATAPATH (0x76) bit2 = ENABLE_DC_BLOCKING
    uint8_t ch1 = _readReg(ES9820_REG_CH1_DATAPATH);
    uint8_t ch2 = _readReg(ES9820_REG_CH2_DATAPATH);
    if (en) {
        ch1 |=  ES9820_HPF_ENABLE_BIT;
        ch2 |=  ES9820_HPF_ENABLE_BIT;
    } else {
        ch1 &= (uint8_t)~ES9820_HPF_ENABLE_BIT;
        ch2 &= (uint8_t)~ES9820_HPF_ENABLE_BIT;
    }
    bool ok  = _writeReg(ES9820_REG_CH1_DATAPATH, ch1);
    ok       = ok && _writeReg(ES9820_REG_CH2_DATAPATH, ch2);
    _hpfEnabled = en;
    LOG_I("[HAL:ES9820] HPF: %s", en ? "enabled" : "disabled");
    return ok;
}

bool HalEs9820::adcSetSampleRate(uint32_t hz) {
    return configure(hz, _bitDepth);
}

// ===== ES9820-specific extensions =====

bool HalEs9820::setFilterPreset(uint8_t preset) {
    if (!_initialized) return false;
    if (preset > 7) {
        LOG_W("[HAL:ES9820] setFilterPreset: preset %u out of range (0-7)", preset);
        return false;
    }
    // FILTER_SHAPE lives in bits[4:2] of REG_CH1_FILTER (0x71) / REG_CH2_FILTER (0x82)
    // Preserve other bits (PROG_COEFF_WE=bit1, PROG_COEFF_EN=bit0, bit7:5 reserved)
    uint8_t ch1cur = _readReg(ES9820_REG_CH1_FILTER);
    uint8_t ch2cur = _readReg(ES9820_REG_CH2_FILTER);
    uint8_t filterBits = (uint8_t)((preset & ES9820_FILTER_MASK) << ES9820_FILTER_SHIFT);
    // Mask out the filter field bits[4:2] and OR in the new value
    uint8_t ch1new = (uint8_t)((ch1cur & ~(uint8_t)(ES9820_FILTER_MASK << ES9820_FILTER_SHIFT)) | filterBits);
    uint8_t ch2new = (uint8_t)((ch2cur & ~(uint8_t)(ES9820_FILTER_MASK << ES9820_FILTER_SHIFT)) | filterBits);
    bool ok  = _writeReg(ES9820_REG_CH1_FILTER, ch1new);
    ok       = ok && _writeReg(ES9820_REG_CH2_FILTER, ch2new);
    _filterPreset = preset;
    LOG_I("[HAL:ES9820] Filter preset: %u", preset);
    return ok;
}

bool HalEs9820::setChannelVolume(uint8_t channel, uint16_t vol16) {
    if (!_initialized) return false;
    bool ok = false;
    if (channel == 0 || channel == 1) {
        uint8_t regLsb = (channel == 0) ? ES9820_REG_CH1_VOLUME_LSB
                                        : ES9820_REG_CH2_VOLUME_LSB;
        ok = _writeReg16(regLsb, vol16);
        LOG_D("[HAL:ES9820] Channel %u volume: 0x%04X", channel, vol16);
    } else {
        LOG_W("[HAL:ES9820] setChannelVolume: invalid channel %u (0 or 1 only)", channel);
    }
    return ok;
}

// ===== AudioInputSource =====

const AudioInputSource* HalEs9820::getInputSource() const {
    return _inputSrcReady ? &_inputSrc : nullptr;
}

#endif // DAC_ENABLED
