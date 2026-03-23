#ifdef DAC_ENABLED
// HalEs9842pro — ESS ES9842PRO 4-channel 32-bit ADC implementation
// Register map sourced from src/drivers/es9842pro_regs.h (ESS datasheet).
// Architecture: per-channel 16-bit volume, 2-bit gain (0-18 dB), per-channel
// HPF and filter, TDM output with HalTdmDeinterleaver for stereo pair splitting.

#include "hal_es9842pro.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include <Wire.h>
#include "hal_ess_sabre_adc_base.h"  // for extern TwoWire Wire2
#include <Arduino.h>
#include "../debug_serial.h"
#include "../i2s_audio.h"
#include "../drivers/es9842pro_regs.h"
#else
// ===== Native test stubs — no hardware access =====
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)

// Register address constants (mirrors es9842pro_regs.h for native compilation)
#define ES9842PRO_I2C_ADDR                0x40
#define ES9842PRO_CHIP_ID                 0x83
#define ES9842PRO_REG_SYS_CONFIG          0x00
#define ES9842PRO_REG_CHIP_ID             0xE1
#define ES9842PRO_REG_CH1_DC_BLOCKING     0x65
#define ES9842PRO_REG_CH1_VOLUME_LSB      0x6D
#define ES9842PRO_REG_CH1_VOLUME_MSB      0x6E
#define ES9842PRO_REG_CH1_GAIN            0x70
#define ES9842PRO_REG_CH1_FILTER          0x71
#define ES9842PRO_REG_CH2_DC_BLOCKING     0x76
#define ES9842PRO_REG_CH2_VOLUME_LSB      0x7E
#define ES9842PRO_REG_CH2_VOLUME_MSB      0x7F
#define ES9842PRO_REG_CH2_GAIN            0x81
#define ES9842PRO_REG_CH2_FILTER          0x82
#define ES9842PRO_REG_CH3_DC_BLOCKING     0x87
#define ES9842PRO_REG_CH3_VOLUME_LSB      0x8F
#define ES9842PRO_REG_CH3_VOLUME_MSB      0x90
#define ES9842PRO_REG_CH3_GAIN            0x92
#define ES9842PRO_REG_CH3_FILTER          0x93
#define ES9842PRO_REG_CH4_DC_BLOCKING     0x98
#define ES9842PRO_REG_CH4_VOLUME_LSB      0xA0
#define ES9842PRO_REG_CH4_VOLUME_MSB      0xA1
#define ES9842PRO_REG_CH4_GAIN            0xA3
#define ES9842PRO_REG_CH4_FILTER          0xA4
#define ES9842PRO_OUTPUT_TDM              0x40
#define ES9842PRO_SOFT_RESET_CMD          0x80
#define ES9842PRO_HPF_ENABLE_BIT          0x04
#define ES9842PRO_GAIN_MASK               0x03
#define ES9842PRO_FILTER_MASK             0x1C
#define ES9842PRO_FILTER_SHIFT            2
#define ES9842PRO_VOL_MUTE                0x0000
#define ES9842PRO_VOL_0DB                 0x7FFF
// Port-generic API stubs for native test (real i2s_audio.h provides these inline)
inline uint32_t i2s_audio_get_sample_rate(void) { return 48000; }
#endif // NATIVE_TEST

// ===== Constructor =====

HalEs9842pro::HalEs9842pro() : HalAudioDevice() {
    hal_init_descriptor(_descriptor, "ess,es9842pro", "ES9842PRO", "ESS Technology",
        HAL_DEV_ADC, 4, 0x40, HAL_BUS_I2C, HAL_I2C_BUS_EXP,
        HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K,
        HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL);
    _initPriority = HAL_PRIORITY_HARDWARE;
}

// ===== I2C helpers =====

bool HalEs9842pro::_writeReg(uint8_t reg, uint8_t val) {
#ifndef NATIVE_TEST
    if (!_wire) return false;
    _wire->beginTransmission(_i2cAddr);
    _wire->write(reg);
    _wire->write(val);
    uint8_t err = _wire->endTransmission();
    if (err != 0) {
        LOG_E("[HAL:ES9842PRO] I2C write failed: reg=0x%02X val=0x%02X err=%d", reg, val, err);
        return false;
    }
    return true;
#else
    (void)reg; (void)val;
    return true;
#endif
}

uint8_t HalEs9842pro::_readReg(uint8_t reg) {
#ifndef NATIVE_TEST
    if (!_wire) return 0xFF;
    _wire->beginTransmission(_i2cAddr);
    _wire->write(reg);
    _wire->endTransmission(false);
    _wire->requestFrom(_i2cAddr, (uint8_t)1);
    if (_wire->available()) return _wire->read();
    LOG_E("[HAL:ES9842PRO] I2C read failed: reg=0x%02X", reg);
    return 0xFF;
#else
    (void)reg;
    return 0x00;
#endif
}

bool HalEs9842pro::_writeReg16(uint8_t regLsb, uint16_t val) {
    // Write LSB first, then MSB. MSB write latches both on ESS SABRE ADCs.
    bool ok = _writeReg(regLsb, (uint8_t)(val & 0xFF));
    ok = ok && _writeReg((uint8_t)(regLsb + 1), (uint8_t)((val >> 8) & 0xFF));
    return ok;
}

// ===== HalDevice lifecycle =====

bool HalEs9842pro::probe() {
#ifndef NATIVE_TEST
    if (!_wire) return false;
    _wire->beginTransmission(_i2cAddr);
    uint8_t err = _wire->endTransmission();
    if (err != 0) return false;
    uint8_t chipId = _readReg(ES9842PRO_REG_CHIP_ID);
    return (chipId == ES9842PRO_CHIP_ID);
#else
    return true;
#endif
}

HalInitResult HalEs9842pro::init() {
    // ---- 1. Read per-device config overrides from HAL Device Manager ----
    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);
    if (cfg && cfg->valid) {
        if (cfg->i2cAddr != 0)     _i2cAddr     = cfg->i2cAddr;
        if (cfg->i2cBusIndex != 0) _i2cBusIndex = cfg->i2cBusIndex;
        if (cfg->pinSda >= 0)      _sdaPin      = cfg->pinSda;
        if (cfg->pinScl >= 0)      _sclPin      = cfg->pinScl;
        if (cfg->sampleRate > 0)   _sampleRate  = cfg->sampleRate;
        if (cfg->bitDepth > 0)     _bitDepth    = cfg->bitDepth;
        if (cfg->pgaGain <= 18)    _gainDb      = cfg->pgaGain;
        _hpfEnabled   = cfg->hpfEnabled;
        _filterPreset = cfg->filterMode;   // 0-7 stored in filterMode field
    }

    LOG_I("[HAL:ES9842PRO] Initializing (I2C addr=0x%02X bus=%u SDA=%d SCL=%d sr=%luHz bits=%u)",
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
    LOG_I("[HAL:ES9842PRO] I2C initialized (bus %u SDA=%d SCL=%d 400kHz)",
          _i2cBusIndex, _sdaPin, _sclPin);
#endif

    // ---- 4. Soft reset: write 0x80 to reg 0x00 (SOFT_RESET bit[7]) ----
    _writeReg(ES9842PRO_REG_SYS_CONFIG, ES9842PRO_SOFT_RESET_CMD);
#ifndef NATIVE_TEST
    delay(5);   // Allow reset to complete
#endif

    // ---- 5. Read chip ID at 0xE1, verify == 0x83 ----
    uint8_t chipId = _readReg(ES9842PRO_REG_CHIP_ID);
    if (chipId != ES9842PRO_CHIP_ID) {
        LOG_W("[HAL:ES9842PRO] Unexpected chip ID: 0x%02X (expected 0x%02X) — continuing",
              chipId, ES9842PRO_CHIP_ID);
    } else {
        LOG_I("[HAL:ES9842PRO] Chip ID OK (0x%02X)", chipId);
    }

    // ---- 6. Configure TDM output: reg 0x00 bits[6:5] = 0b10 ----
    _writeReg(ES9842PRO_REG_SYS_CONFIG, ES9842PRO_OUTPUT_TDM);

    // ---- 7. Set all channels to 0 dB volume: write 0x7FFF to each volume reg pair ----
    _writeReg16(ES9842PRO_REG_CH1_VOLUME_LSB, ES9842PRO_VOL_0DB);
    _writeReg16(ES9842PRO_REG_CH2_VOLUME_LSB, ES9842PRO_VOL_0DB);
    _writeReg16(ES9842PRO_REG_CH3_VOLUME_LSB, ES9842PRO_VOL_0DB);
    _writeReg16(ES9842PRO_REG_CH4_VOLUME_LSB, ES9842PRO_VOL_0DB);

    // ---- 8. Set PGA gain: 2-bit per channel (0=0dB,1=+6dB,2=+12dB,3=+18dB) ----
    // Clamp to 18 dB max (2-bit max value 3)
    uint8_t gainStep = (_gainDb / 6);
    if (gainStep > 3) gainStep = 3;
    uint8_t gainVal = gainStep & ES9842PRO_GAIN_MASK;
    _writeReg(ES9842PRO_REG_CH1_GAIN, gainVal);
    _writeReg(ES9842PRO_REG_CH2_GAIN, gainVal);
    _writeReg(ES9842PRO_REG_CH3_GAIN, gainVal);
    _writeReg(ES9842PRO_REG_CH4_GAIN, gainVal);

    // ---- 9. Configure HPF (DC blocking): bit[2] per channel register ----
    uint8_t dcBit = _hpfEnabled ? ES9842PRO_HPF_ENABLE_BIT : 0x00;
    _writeReg(ES9842PRO_REG_CH1_DC_BLOCKING, dcBit);
    _writeReg(ES9842PRO_REG_CH2_DC_BLOCKING, dcBit);
    _writeReg(ES9842PRO_REG_CH3_DC_BLOCKING, dcBit);
    _writeReg(ES9842PRO_REG_CH4_DC_BLOCKING, dcBit);

    // ---- 10. Set filter preset: bits[4:2] in each channel filter register ----
    uint8_t preset = (_filterPreset > 7) ? 7 : _filterPreset;
    uint8_t filterVal = (uint8_t)((preset << ES9842PRO_FILTER_SHIFT) & ES9842PRO_FILTER_MASK);
    _writeReg(ES9842PRO_REG_CH1_FILTER, filterVal);
    _writeReg(ES9842PRO_REG_CH2_FILTER, filterVal);
    _writeReg(ES9842PRO_REG_CH3_FILTER, filterVal);
    _writeReg(ES9842PRO_REG_CH4_FILTER, filterVal);

    // ---- 11. Configure I2S2 in TDM mode and init the deinterleaver ----
    uint8_t port = (cfg && cfg->valid && cfg->i2sPort != 255) ? cfg->i2sPort : 2;
    int8_t dinPinRaw = (cfg && cfg->valid && cfg->pinData > 0) ? cfg->pinData : -1;
    if (dinPinRaw < 0) {
        LOG_W("[HAL:ES9842PRO] No DATA_IN pin configured — set pinData in HAL config");
        dinPinRaw = 0;
    }

#ifndef NATIVE_TEST
    bool tdmOk = i2s_port_enable_rx((uint8_t)port, I2S_MODE_TDM, 4,
                                     (gpio_num_t)dinPinRaw,
                                     GPIO_NUM_NC,
                                     (gpio_num_t)I2S_BCK_PIN,
                                     (gpio_num_t)I2S_LRC_PIN);
    if (!tdmOk) {
        LOG_E("[HAL:ES9842PRO] I2S TDM init failed (port=%u) — audio will be silent", port);
    }
#endif

    // Allocate deinterleaver ping-pong buffers (PSRAM preferred)
    if (!_tdm.init((uint8_t)port)) {
        LOG_E("[HAL:ES9842PRO] TDM deinterleaver init failed — out of memory");
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "TDM deinterleaver alloc failed");
    }

    // Build the two AudioInputSource structs from the deinterleaver
    _tdm.buildSources(_NAME_A, _NAME_B, &_srcA, &_srcB);

    // ---- 12. Mark device ready ----
    _initialized = true;
    _state = HAL_STATE_AVAILABLE;
    setReady(true);

    LOG_I("[HAL:ES9842PRO] Ready — TDM mode, port=%u DIN=GPIO%d gain=%ddB hpf=%d filter=%u",
          port, dinPinRaw, _gainDb, (int)_hpfEnabled, _filterPreset);
    LOG_I("[HAL:ES9842PRO] Registered sources: '%s' (pair A) + '%s' (pair B)",
          _NAME_A, _NAME_B);
    return hal_init_ok();
}

void HalEs9842pro::deinit() {
    if (!_initialized) return;

    setReady(false);

#ifndef NATIVE_TEST
    // Power down: disable TDM output (reg 0x00 = 0x00)
    _writeReg(ES9842PRO_REG_SYS_CONFIG, 0x00);

    // Release I2S expansion TDM RX via port-generic API
    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(getSlot());
    uint8_t port = (cfg && cfg->valid && cfg->i2sPort != 255) ? cfg->i2sPort : 2;
    i2s_port_disable_rx(port);
#endif

    // Release deinterleaver ping-pong buffers
    _tdm.deinit();

    _initialized = false;
    _state       = HAL_STATE_REMOVED;

    LOG_I("[HAL:ES9842PRO] Deinitialized (TDM + I2S2 released)");
}

void HalEs9842pro::dumpConfig() {
    LOG_I("[HAL:ES9842PRO] %s by %s (compat=%s) i2c=0x%02X bus=%u sda=%d scl=%d "
          "sr=%luHz bits=%u gain=%ddB hpf=%d filter=%u",
          _descriptor.name, _descriptor.manufacturer, _descriptor.compatible,
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth, _gainDb,
          (int)_hpfEnabled, _filterPreset);
}

bool HalEs9842pro::healthCheck() {
#ifndef NATIVE_TEST
    if (!_wire || !_initialized) return false;
    uint8_t id = _readReg(ES9842PRO_REG_CHIP_ID);
    return (id == ES9842PRO_CHIP_ID);
#else
    return _initialized;
#endif
}

// ===== HalAudioDevice =====

bool HalEs9842pro::configure(uint32_t sampleRate, uint8_t bitDepth) {
    const uint32_t supported[] = { 44100, 48000, 96000, 192000 };
    bool valid = false;
    for (uint8_t i = 0; i < 4; i++) {
        if (sampleRate == supported[i]) { valid = true; break; }
    }
    if (!valid) {
        LOG_W("[HAL:ES9842PRO] Unsupported sample rate: %luHz", (unsigned long)sampleRate);
        return false;
    }
    if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32) {
        LOG_W("[HAL:ES9842PRO] Unsupported bit depth: %u", bitDepth);
        return false;
    }
    _sampleRate = sampleRate;
    _bitDepth   = bitDepth;
    LOG_I("[HAL:ES9842PRO] Configured: %luHz %ubit", (unsigned long)sampleRate, bitDepth);
    return true;
}

bool HalEs9842pro::setVolume(uint8_t percent) {
    if (!_initialized) return false;
    if (percent > 100) percent = 100;
    // Map 0% → 0x0000 (mute), 100% → 0x7FFF (0 dB).
    uint16_t vol16;
    if (percent == 0) {
        vol16 = ES9842PRO_VOL_MUTE;
    } else if (percent == 100) {
        vol16 = ES9842PRO_VOL_0DB;
    } else {
        vol16 = (uint16_t)(((uint32_t)(percent) * 0x7FFF) / 100);
    }
    bool ok  = _writeReg16(ES9842PRO_REG_CH1_VOLUME_LSB, vol16);
    ok       = ok && _writeReg16(ES9842PRO_REG_CH2_VOLUME_LSB, vol16);
    ok       = ok && _writeReg16(ES9842PRO_REG_CH3_VOLUME_LSB, vol16);
    ok       = ok && _writeReg16(ES9842PRO_REG_CH4_VOLUME_LSB, vol16);
    LOG_D("[HAL:ES9842PRO] Volume: %d%% -> 0x%04X", percent, vol16);
    return ok;
}

bool HalEs9842pro::setMute(bool mute) {
    if (!_initialized) return false;
    uint16_t vol16 = mute ? ES9842PRO_VOL_MUTE : ES9842PRO_VOL_0DB;
    bool ok  = _writeReg16(ES9842PRO_REG_CH1_VOLUME_LSB, vol16);
    ok       = ok && _writeReg16(ES9842PRO_REG_CH2_VOLUME_LSB, vol16);
    ok       = ok && _writeReg16(ES9842PRO_REG_CH3_VOLUME_LSB, vol16);
    ok       = ok && _writeReg16(ES9842PRO_REG_CH4_VOLUME_LSB, vol16);
    LOG_I("[HAL:ES9842PRO] %s", mute ? "Muted" : "Unmuted");
    return ok;
}

// ===== HalAudioAdcInterface =====

bool HalEs9842pro::adcSetGain(uint8_t gainDb) {
    if (!_initialized) return false;
    // 2-bit gain field: 0=0dB, 1=+6dB, 2=+12dB, 3=+18dB; max 18 dB
    if (gainDb > 18) gainDb = 18;
    uint8_t gainStep = gainDb / 6;
    if (gainStep > 3) gainStep = 3;
    uint8_t gainVal = gainStep & ES9842PRO_GAIN_MASK;
    bool ok  = _writeReg(ES9842PRO_REG_CH1_GAIN, gainVal);
    ok       = ok && _writeReg(ES9842PRO_REG_CH2_GAIN, gainVal);
    ok       = ok && _writeReg(ES9842PRO_REG_CH3_GAIN, gainVal);
    ok       = ok && _writeReg(ES9842PRO_REG_CH4_GAIN, gainVal);
    _gainDb  = (uint8_t)(gainStep * 6);
    LOG_I("[HAL:ES9842PRO] ADC gain: %ddB (step=%u reg=0x%02X)", _gainDb, gainStep, gainVal);
    return ok;
}

bool HalEs9842pro::adcSetHpfEnabled(bool en) {
    if (!_initialized) return false;
    uint8_t dcBit = en ? ES9842PRO_HPF_ENABLE_BIT : 0x00;
    bool ok  = _writeReg(ES9842PRO_REG_CH1_DC_BLOCKING, dcBit);
    ok       = ok && _writeReg(ES9842PRO_REG_CH2_DC_BLOCKING, dcBit);
    ok       = ok && _writeReg(ES9842PRO_REG_CH3_DC_BLOCKING, dcBit);
    ok       = ok && _writeReg(ES9842PRO_REG_CH4_DC_BLOCKING, dcBit);
    _hpfEnabled = en;
    LOG_I("[HAL:ES9842PRO] HPF: %s", en ? "enabled" : "disabled");
    return ok;
}

bool HalEs9842pro::adcSetSampleRate(uint32_t hz) {
    return configure(hz, _bitDepth);
}

// ===== ES9842PRO-specific extensions =====

bool HalEs9842pro::setFilterPreset(uint8_t preset) {
    if (!_initialized) return false;
    if (preset > 7) return false;
    uint8_t filterVal = (uint8_t)((preset << ES9842PRO_FILTER_SHIFT) & ES9842PRO_FILTER_MASK);
    // Preserve bits outside the filter field by read-modify-write
    uint8_t cur1 = _readReg(ES9842PRO_REG_CH1_FILTER);
    bool ok  = _writeReg(ES9842PRO_REG_CH1_FILTER,
                         (uint8_t)((cur1 & ~ES9842PRO_FILTER_MASK) | filterVal));
    uint8_t cur2 = _readReg(ES9842PRO_REG_CH2_FILTER);
    ok       = ok && _writeReg(ES9842PRO_REG_CH2_FILTER,
                               (uint8_t)((cur2 & ~ES9842PRO_FILTER_MASK) | filterVal));
    uint8_t cur3 = _readReg(ES9842PRO_REG_CH3_FILTER);
    ok       = ok && _writeReg(ES9842PRO_REG_CH3_FILTER,
                               (uint8_t)((cur3 & ~ES9842PRO_FILTER_MASK) | filterVal));
    uint8_t cur4 = _readReg(ES9842PRO_REG_CH4_FILTER);
    ok       = ok && _writeReg(ES9842PRO_REG_CH4_FILTER,
                               (uint8_t)((cur4 & ~ES9842PRO_FILTER_MASK) | filterVal));
    _filterPreset = preset;
    LOG_I("[HAL:ES9842PRO] Filter preset: %u", preset);
    return ok;
}

bool HalEs9842pro::setChannelVolume16(uint8_t ch, uint16_t vol) {
    if (!_initialized) return false;
    // Channel volume register pairs: CH1=0x6D/0x6E, CH2=0x7E/0x7F, CH3=0x8F/0x90, CH4=0xA0/0xA1
    const uint8_t volLsbRegs[4] = {
        ES9842PRO_REG_CH1_VOLUME_LSB,
        ES9842PRO_REG_CH2_VOLUME_LSB,
        ES9842PRO_REG_CH3_VOLUME_LSB,
        ES9842PRO_REG_CH4_VOLUME_LSB
    };
    if (ch >= 4) {
        LOG_W("[HAL:ES9842PRO] setChannelVolume16: invalid channel %u (0-3 only)", ch);
        return false;
    }
    bool ok = _writeReg16(volLsbRegs[ch], vol);
    LOG_D("[HAL:ES9842PRO] Channel %u volume: 0x%04X", ch, vol);
    return ok;
}

// ===== AudioInputSource — dual-source TDM accessor =====

const AudioInputSource* HalEs9842pro::getInputSourceAt(int idx) const {
    if (!_initialized) return nullptr;
    if (idx == 0) return &_srcA;
    if (idx == 1) return &_srcB;
    return nullptr;
}

#endif // DAC_ENABLED
