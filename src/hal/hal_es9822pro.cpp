#ifdef DAC_ENABLED
// HalEs9822pro — ESS ES9822PRO 2-channel 32-bit ADC implementation
// Register map sourced from src/drivers/es9822pro_regs.h (ESS datasheet v0.5.2).

#include "hal_es9822pro.h"
#include "hal_ess_sabre_adc_base.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include <Wire.h>
#include <Arduino.h>
#include "../debug_serial.h"
#include "../i2s_audio.h"
#include "../drivers/es9822pro_regs.h"
#else
// ===== Native test stubs — no hardware access =====
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)

// Register address constants (mirrors es9822pro_regs.h for native compilation;
// all actual hardware writes are inside #ifndef NATIVE_TEST blocks below).
#define ES9822PRO_I2C_ADDR                0x40
#define ES9822PRO_CHIP_ID                 0x81
#define ES9822PRO_REG_SYS_CONFIG          0x00
#define ES9822PRO_REG_ADC_CLOCK_CONFIG1   0x01
#define ES9822PRO_REG_SYNC_CLK_SELECT     0xC1
#define ES9822PRO_REG_ADC_CH1A_CFG1       0x3F
#define ES9822PRO_REG_ADC_CH1A_CFG2       0x40
#define ES9822PRO_REG_ADC_CH2A_CFG1       0x41
#define ES9822PRO_REG_ADC_CH2A_CFG2       0x42
#define ES9822PRO_REG_ADC_COMMON_MODE     0x47
#define ES9822PRO_REG_CH1_DATAPATH        0x65
#define ES9822PRO_REG_CH1_VOLUME_LSB      0x6D
#define ES9822PRO_REG_CH1_VOLUME_MSB      0x6E
#define ES9822PRO_REG_CH1_GAIN            0x70
#define ES9822PRO_REG_CH1_FILTER          0x71
#define ES9822PRO_REG_CH2_DATAPATH        0x76
#define ES9822PRO_REG_CH2_VOLUME_LSB      0x7E
#define ES9822PRO_REG_CH2_VOLUME_MSB      0x7F
#define ES9822PRO_REG_CH2_GAIN            0x81
#define ES9822PRO_REG_CH2_FILTER          0x82
#define ES9822PRO_REG_CHIP_ID             0xE1
#define ES9822PRO_SOFT_RESET_BIT          0x80
#define ES9822PRO_OUTPUT_I2S              0x00
#define ES9822PRO_HPF_ENABLE_BIT          0x04
#define ES9822PRO_CLOCK_ENABLE_2CH        0x33
#define ES9822PRO_OPTIMAL_CH1A_CFG1       0xBA
#define ES9822PRO_OPTIMAL_CH1A_CFG2       0x3A
#define ES9822PRO_OPTIMAL_COMMON_MODE     0xFF
#define ES9822PRO_VOL_0DB                 0x7FFF
#define ES9822PRO_GAIN_0DB                0x00
#define ES9822PRO_VOL_MUTE                0x0000
// Stub I2S port callbacks under native test
inline uint32_t i2s_audio_port0_read(int32_t*, uint32_t) { return 0; }
inline uint32_t i2s_audio_port1_read(int32_t*, uint32_t) { return 0; }
inline bool     i2s_audio_port0_active(void) { return false; }
inline bool     i2s_audio_port1_active(void) { return false; }
inline uint32_t i2s_audio_get_sample_rate(void) { return 48000; }
#endif // NATIVE_TEST

// ===== Constructor =====

HalEs9822pro::HalEs9822pro() : HalEssSabreAdcBase() {
    hal_init_descriptor(_descriptor, "ess,es9822pro", "ES9822PRO", "ESS Technology",
        HAL_DEV_ADC, 2, 0x40, HAL_BUS_I2C, HAL_I2C_BUS_EXP,
        HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K,
        HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL);
    _initPriority = HAL_PRIORITY_HARDWARE;
}

// ===== HalDevice lifecycle =====

bool HalEs9822pro::probe() {
#ifndef NATIVE_TEST
    if (!_wire) return false;
    _wire->beginTransmission(_i2cAddr);
    uint8_t err = _wire->endTransmission();
    if (err != 0) return false;
    uint8_t chipId = _readReg(ES9822PRO_REG_CHIP_ID);
    return (chipId == ES9822PRO_CHIP_ID);
#else
    return true;
#endif
}

HalInitResult HalEs9822pro::init() {
    // ---- 1. Read per-device config overrides from HAL Device Manager ----
    _applyConfigOverrides();
    if (_gainDb > 18) _gainDb = 18;

    LOG_I("[HAL:ES9822PRO] Initializing (I2C addr=0x%02X bus=%u SDA=%d SCL=%d sr=%luHz bits=%u)",
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin, (unsigned long)_sampleRate, _bitDepth);

#ifndef NATIVE_TEST
    // ---- 2+3. Select and initialize TwoWire instance ----
    _selectWire();
    LOG_I("[HAL:ES9822PRO] I2C initialized (bus %u SDA=%d SCL=%d 400kHz)",
          _i2cBusIndex, _sdaPin, _sclPin);
#endif

    // ---- 4. Verify chip ID (read reg 0xE1, expect 0x81) ----
    uint8_t chipId = _readReg(ES9822PRO_REG_CHIP_ID);
    if (chipId != ES9822PRO_CHIP_ID) {
        LOG_W("[HAL:ES9822PRO] Unexpected chip ID: 0x%02X (expected 0x%02X) — continuing",
              chipId, ES9822PRO_CHIP_ID);
    } else {
        LOG_I("[HAL:ES9822PRO] Chip ID OK (0x%02X)", chipId);
    }

    // ---- 5. Soft reset (reg 0x00 bit7, self-clearing) ----
    _writeReg(ES9822PRO_REG_SYS_CONFIG, ES9822PRO_SOFT_RESET_BIT);
#ifndef NATIVE_TEST
    delay(5);   // Allow reset to complete before continuing
#endif

    // ---- 6. Clock config: select MCLK as system clock source, enable analog clock input ----
    // REG_SYNC_CLK_SELECT (0xC1): SEL_SYSCLK_IN=0b01 (MCLK), EN_ANA_CLKIN=1 → 0x03
    _writeReg(ES9822PRO_REG_SYNC_CLK_SELECT, 0x03);

    // ---- 7. Optimal analog ADC config (from ESS datasheet v0.5.2) ----
    _writeReg(ES9822PRO_REG_ADC_CH1A_CFG1,   ES9822PRO_OPTIMAL_CH1A_CFG1);
    _writeReg(ES9822PRO_REG_ADC_CH1A_CFG2,   ES9822PRO_OPTIMAL_CH1A_CFG2);
    _writeReg(ES9822PRO_REG_ADC_CH2A_CFG1,   ES9822PRO_OPTIMAL_CH1A_CFG1);  // CH2A mirrors CH1A
    _writeReg(ES9822PRO_REG_ADC_CH2A_CFG2,   ES9822PRO_OPTIMAL_CH1A_CFG2);
    _writeReg(ES9822PRO_REG_ADC_COMMON_MODE, ES9822PRO_OPTIMAL_COMMON_MODE);

    // ---- 8. Enable ADC clocks: CH1A + CH2A data input + decimation ----
    _writeReg(ES9822PRO_REG_ADC_CLOCK_CONFIG1, ES9822PRO_CLOCK_ENABLE_2CH);

    // ---- 9. Configure I2S output format (bits6:5 = 0b00 → I2S/Philips slave) ----
    _writeReg(ES9822PRO_REG_SYS_CONFIG, ES9822PRO_OUTPUT_I2S);

    // ---- 10. Set unity gain volume on both channels (0x7FFF = 0 dB) ----
    _writeReg16(ES9822PRO_REG_CH1_VOLUME_LSB, ES9822PRO_VOL_0DB);
    _writeReg16(ES9822PRO_REG_CH2_VOLUME_LSB, ES9822PRO_VOL_0DB);

    // ---- 11. PGA gain ----
    uint8_t gainReg = (_gainDb >= 18) ? 3 : (_gainDb / 6);
    _writeReg(ES9822PRO_REG_CH1_GAIN, gainReg & 0x03);
    _writeReg(ES9822PRO_REG_CH2_GAIN, gainReg & 0x03);

    // ---- 12. HPF (DC blocking) ----
    // REG_CH1_DATAPATH (0x65) / REG_CH2_DATAPATH (0x76) bit2 = ENABLE_DC_BLOCKING
    uint8_t datapath = _hpfEnabled ? ES9822PRO_HPF_ENABLE_BIT : 0x00;
    _writeReg(ES9822PRO_REG_CH1_DATAPATH, datapath);
    _writeReg(ES9822PRO_REG_CH2_DATAPATH, datapath);

    // ---- 13. Digital filter preset ----
    // REG_CH1_FILTER (0x71) / REG_CH2_FILTER (0x82) bits4:2 = FILTER_SHAPE
    uint8_t preset = (_filterPreset > 7) ? 7 : _filterPreset;
    uint8_t filterReg = (uint8_t)((preset & 0x07) << 2);
    _writeReg(ES9822PRO_REG_CH1_FILTER, filterReg);
    _writeReg(ES9822PRO_REG_CH2_FILTER, filterReg);

    // ---- 14. Populate AudioInputSource (port-indexed I2S callbacks) ----
    memset(&_inputSrc, 0, sizeof(_inputSrc));
    _inputSrc.name          = _descriptor.name;
    _inputSrc.isHardwareAdc = true;
    _inputSrc.gainLinear    = 1.0f;
    _inputSrc.vuL           = -90.0f;
    _inputSrc.vuR           = -90.0f;

    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);
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

    // ---- 15. Mark device ready ----
    _initialized = true;
    _state = HAL_STATE_AVAILABLE;
    setReady(true);

    LOG_I("[HAL:ES9822PRO] Ready (i2s port=%u gain=%ddB hpf=%d filter=%u)",
          port, _gainDb, (int)_hpfEnabled, _filterPreset);
    return hal_init_ok();
}

void HalEs9822pro::deinit() {
    if (!_initialized) return;

    setReady(false);

#ifndef NATIVE_TEST
    // Power down: disable ADC clocks (reg 0x01 = 0x00)
    _writeReg(ES9822PRO_REG_ADC_CLOCK_CONFIG1, 0x00);
#endif

    _initialized  = false;
    _inputSrcReady = false;
    _state = HAL_STATE_REMOVED;

    LOG_I("[HAL:ES9822PRO] Deinitialized");
}

void HalEs9822pro::dumpConfig() {
    LOG_I("[HAL:ES9822PRO] %s by %s (compat=%s) i2c=0x%02X bus=%u sda=%d scl=%d "
          "sr=%luHz bits=%u gain=%ddB hpf=%d filter=%u",
          _descriptor.name, _descriptor.manufacturer, _descriptor.compatible,
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth, _gainDb,
          (int)_hpfEnabled, _filterPreset);
}

bool HalEs9822pro::healthCheck() {
#ifndef NATIVE_TEST
    if (!_wire || !_initialized) return false;
    uint8_t id = _readReg(ES9822PRO_REG_CHIP_ID);
    return (id == ES9822PRO_CHIP_ID);
#else
    return _initialized;
#endif
}

// ===== HalAudioDevice =====

bool HalEs9822pro::configure(uint32_t sampleRate, uint8_t bitDepth) {
    // ES9822PRO derives sample rate from MCLK ratio via hardware clock dividers;
    // the register path only gates clocks. Store requested values and validate.
    const uint32_t supported[] = { 44100, 48000, 96000, 192000 };
    bool valid = false;
    for (uint8_t i = 0; i < 4; i++) {
        if (sampleRate == supported[i]) { valid = true; break; }
    }
    if (!valid) {
        LOG_W("[HAL:ES9822PRO] Unsupported sample rate: %luHz", (unsigned long)sampleRate);
        return false;
    }
    _sampleRate = sampleRate;
    _bitDepth   = bitDepth;
    LOG_I("[HAL:ES9822PRO] Configured: %luHz %ubit", (unsigned long)sampleRate, bitDepth);
    return true;
}

bool HalEs9822pro::setVolume(uint8_t percent) {
    if (!_initialized) return false;
    if (percent > 100) percent = 100;
    // Map 0% = 0x0000 (mute), 100% = 0x7FFF (0 dB), linear
    uint16_t vol16 = (uint16_t)(((uint32_t)percent * ES9822PRO_VOL_0DB) / 100);
    bool ok  = _writeReg16(ES9822PRO_REG_CH1_VOLUME_LSB, vol16);
    ok       = ok && _writeReg16(ES9822PRO_REG_CH2_VOLUME_LSB, vol16);
    LOG_D("[HAL:ES9822PRO] Volume: %d%% -> 0x%04X", percent, vol16);
    return ok;
}

bool HalEs9822pro::setMute(bool mute) {
    if (!_initialized) return false;
    uint16_t vol16 = mute ? ES9822PRO_VOL_MUTE : ES9822PRO_VOL_0DB;
    bool ok  = _writeReg16(ES9822PRO_REG_CH1_VOLUME_LSB, vol16);
    ok       = ok && _writeReg16(ES9822PRO_REG_CH2_VOLUME_LSB, vol16);
    LOG_I("[HAL:ES9822PRO] %s", mute ? "Muted" : "Unmuted");
    return ok;
}

// ===== HalAudioAdcInterface =====

bool HalEs9822pro::adcSetGain(uint8_t gainDb) {
    if (!_initialized) return false;
    // Clamp to chip-supported steps: 0/6/12/18 dB → register 0/1/2/3
    if (gainDb > 18) gainDb = 18;
    uint8_t gainReg = gainDb / 6;   // integer divide: 0→0, 6→1, 7-12→2, 13-18→3 (floor)
    gainReg = (gainReg > 3) ? 3 : gainReg;
    bool ok  = _writeReg(ES9822PRO_REG_CH1_GAIN, gainReg & 0x03);
    ok       = ok && _writeReg(ES9822PRO_REG_CH2_GAIN, gainReg & 0x03);
    _gainDb = (uint8_t)(gainReg * 6);
    LOG_I("[HAL:ES9822PRO] ADC gain: %ddB (reg=0x%02X)", _gainDb, gainReg);
    return ok;
}

bool HalEs9822pro::adcSetHpfEnabled(bool en) {
    if (!_initialized) return false;
    // REG_CH1_DATAPATH / REG_CH2_DATAPATH bit2 = ENABLE_DC_BLOCKING (1 = HPF on)
    uint8_t ch1 = _readReg(ES9822PRO_REG_CH1_DATAPATH);
    uint8_t ch2 = _readReg(ES9822PRO_REG_CH2_DATAPATH);
    if (en) {
        ch1 |=  ES9822PRO_HPF_ENABLE_BIT;
        ch2 |=  ES9822PRO_HPF_ENABLE_BIT;
    } else {
        ch1 &= (uint8_t)~ES9822PRO_HPF_ENABLE_BIT;
        ch2 &= (uint8_t)~ES9822PRO_HPF_ENABLE_BIT;
    }
    bool ok  = _writeReg(ES9822PRO_REG_CH1_DATAPATH, ch1);
    ok       = ok && _writeReg(ES9822PRO_REG_CH2_DATAPATH, ch2);
    _hpfEnabled = en;
    LOG_I("[HAL:ES9822PRO] HPF: %s", en ? "enabled" : "disabled");
    return ok;
}

bool HalEs9822pro::adcSetSampleRate(uint32_t hz) {
    return configure(hz, _bitDepth);
}

// ===== ES9822PRO-specific extensions =====

bool HalEs9822pro::setFilterPreset(uint8_t preset) {
    if (!_initialized) return false;
    if (preset > 7) preset = 7;
    // FILTER_SHAPE lives in bits4:2 of REG_CH1_FILTER / REG_CH2_FILTER
    uint8_t filterBits = (uint8_t)((preset & 0x07) << 2);
    // Preserve lower 2 bits (PROG_COEFF_WE, PROG_COEFF_EN) from current register value
    uint8_t ch1cur = _readReg(ES9822PRO_REG_CH1_FILTER);
    uint8_t ch2cur = _readReg(ES9822PRO_REG_CH2_FILTER);
    uint8_t ch1new = (uint8_t)((ch1cur & 0x03) | filterBits);
    uint8_t ch2new = (uint8_t)((ch2cur & 0x03) | filterBits);
    bool ok  = _writeReg(ES9822PRO_REG_CH1_FILTER, ch1new);
    ok       = ok && _writeReg(ES9822PRO_REG_CH2_FILTER, ch2new);
    _filterPreset = preset;
    LOG_I("[HAL:ES9822PRO] Filter preset: %u", preset);
    return ok;
}

bool HalEs9822pro::setChannelVolume(uint8_t channel, uint16_t vol16) {
    if (!_initialized) return false;
    bool ok = false;
    if (channel == 0 || channel == 1) {
        uint8_t regLsb = (channel == 0) ? ES9822PRO_REG_CH1_VOLUME_LSB
                                         : ES9822PRO_REG_CH2_VOLUME_LSB;
        ok = _writeReg16(regLsb, vol16);
        LOG_D("[HAL:ES9822PRO] Channel %u volume: 0x%04X", channel, vol16);
    } else {
        LOG_W("[HAL:ES9822PRO] setChannelVolume: invalid channel %u (0 or 1 only)", channel);
    }
    return ok;
}

// ===== AudioInputSource =====

const AudioInputSource* HalEs9822pro::getInputSource() const {
    return _inputSrcReady ? &_inputSrc : nullptr;
}

#endif // DAC_ENABLED
