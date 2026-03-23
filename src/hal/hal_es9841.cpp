#ifdef DAC_ENABLED
// HalEs9841 — ESS ES9841 4-channel 32-bit ADC implementation
// Register map sourced from src/drivers/es9841_regs.h (ESS datasheet).
// Architecture: 8-bit per-channel volume (0xFF=0dB, 0x00=mute), 3-bit gain
// per channel (0-42 dB in 6 dB steps), global filter preset (single register),
// per-channel HPF at same offsets as ES9842PRO, TDM output.

#include "hal_es9841.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include <Wire.h>
#include "hal_ess_sabre_adc_base.h"  // for extern TwoWire Wire2
#include <Arduino.h>
#include "../debug_serial.h"
#include "../i2s_audio.h"
#include "../drivers/es9841_regs.h"
#else
// ===== Native test stubs — no hardware access =====
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)

// Register address constants (mirrors es9841_regs.h for native compilation)
#define ES9841_I2C_ADDR                   0x40
#define ES9841_CHIP_ID                    0x91
#define ES9841_REG_SYS_CONFIG             0x00
#define ES9841_REG_FILTER_CONFIG          0x4A
#define ES9841_REG_CH1_VOLUME             0x51
#define ES9841_REG_CH2_VOLUME             0x52
#define ES9841_REG_CH3_VOLUME             0x53
#define ES9841_REG_CH4_VOLUME             0x54
#define ES9841_REG_GAIN_PAIR1             0x55
#define ES9841_REG_GAIN_PAIR2             0x56
#define ES9841_REG_CH1_DC_BLOCKING        0x65
#define ES9841_REG_CH2_DC_BLOCKING        0x76
#define ES9841_REG_CH3_DC_BLOCKING        0x87
#define ES9841_REG_CH4_DC_BLOCKING        0x98
#define ES9841_REG_CHIP_ID                0xE1
#define ES9841_OUTPUT_TDM                 0x40
#define ES9841_SOFT_RESET_CMD             0x80
#define ES9841_HPF_ENABLE_BIT             0x04
#define ES9841_FILTER_MASK                0xE0
#define ES9841_FILTER_SHIFT               5
#define ES9841_VOL_0DB                    0xFF
#define ES9841_VOL_MUTE                   0x00
// Port-generic API stubs for native test (real i2s_audio.h provides these inline)
inline uint32_t i2s_audio_get_sample_rate(void) { return 48000; }
#endif // NATIVE_TEST

// ===== Constructor =====

HalEs9841::HalEs9841() : HalAudioDevice() {
    hal_init_descriptor(_descriptor, "ess,es9841", "ES9841", "ESS Technology",
        HAL_DEV_ADC, 4, 0x40, HAL_BUS_I2C, HAL_I2C_BUS_EXP,
        HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K,
        HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL);
    _initPriority = HAL_PRIORITY_HARDWARE;
}

// ===== I2C helpers =====

bool HalEs9841::_writeReg(uint8_t reg, uint8_t val) {
#ifndef NATIVE_TEST
    if (!_wire) return false;
    _wire->beginTransmission(_i2cAddr);
    _wire->write(reg);
    _wire->write(val);
    uint8_t err = _wire->endTransmission();
    if (err != 0) {
        LOG_E("[HAL:ES9841] I2C write failed: reg=0x%02X val=0x%02X err=%d", reg, val, err);
        return false;
    }
    return true;
#else
    (void)reg; (void)val;
    return true;
#endif
}

uint8_t HalEs9841::_readReg(uint8_t reg) {
#ifndef NATIVE_TEST
    if (!_wire) return 0xFF;
    _wire->beginTransmission(_i2cAddr);
    _wire->write(reg);
    _wire->endTransmission(false);
    _wire->requestFrom(_i2cAddr, (uint8_t)1);
    if (_wire->available()) return _wire->read();
    LOG_E("[HAL:ES9841] I2C read failed: reg=0x%02X", reg);
    return 0xFF;
#else
    (void)reg;
    return 0x00;
#endif
}

// ===== HalDevice lifecycle =====

bool HalEs9841::probe() {
#ifndef NATIVE_TEST
    if (!_wire) return false;
    _wire->beginTransmission(_i2cAddr);
    uint8_t err = _wire->endTransmission();
    if (err != 0) return false;
    uint8_t chipId = _readReg(ES9841_REG_CHIP_ID);
    return (chipId == ES9841_CHIP_ID);
#else
    return true;
#endif
}

HalInitResult HalEs9841::init() {
    // ---- 1. Read per-device config overrides from HAL Device Manager ----
    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);
    if (cfg && cfg->valid) {
        if (cfg->i2cAddr != 0)     _i2cAddr     = cfg->i2cAddr;
        if (cfg->i2cBusIndex != 0) _i2cBusIndex = cfg->i2cBusIndex;
        if (cfg->pinSda >= 0)      _sdaPin      = cfg->pinSda;
        if (cfg->pinScl >= 0)      _sclPin      = cfg->pinScl;
        if (cfg->sampleRate > 0)   _sampleRate  = cfg->sampleRate;
        if (cfg->bitDepth > 0)     _bitDepth    = cfg->bitDepth;
        if (cfg->pgaGain <= 42)    _gainDb      = cfg->pgaGain;
        _hpfEnabled   = cfg->hpfEnabled;
        _filterPreset = cfg->filterMode;
    }

    LOG_I("[HAL:ES9841] Initializing (I2C addr=0x%02X bus=%u SDA=%d SCL=%d sr=%luHz bits=%u)",
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin, (unsigned long)_sampleRate, _bitDepth);

#ifndef NATIVE_TEST
    switch (_i2cBusIndex) {
        case 1:  _wire = &Wire1; break;
        case 2:  _wire = &Wire2; break;
        default: _wire = &Wire;  break;
    }
    _wire->begin((int)_sdaPin, (int)_sclPin, (uint32_t)400000);
    LOG_I("[HAL:ES9841] I2C initialized (bus %u SDA=%d SCL=%d 400kHz)",
          _i2cBusIndex, _sdaPin, _sclPin);
#endif

    // ---- 4. Soft reset: write 0x80 to reg 0x00 ----
    _writeReg(ES9841_REG_SYS_CONFIG, ES9841_SOFT_RESET_CMD);
#ifndef NATIVE_TEST
    delay(5);
#endif

    // ---- 5. Read chip ID ----
    uint8_t chipId = _readReg(ES9841_REG_CHIP_ID);
    if (chipId != ES9841_CHIP_ID) {
        LOG_W("[HAL:ES9841] Unexpected chip ID: 0x%02X (expected 0x%02X) — continuing",
              chipId, ES9841_CHIP_ID);
    } else {
        LOG_I("[HAL:ES9841] Chip ID OK (0x%02X)", chipId);
    }

    // ---- 6. Configure TDM output: reg 0x00 bits[6:5] = 0b10 ----
    _writeReg(ES9841_REG_SYS_CONFIG, ES9841_OUTPUT_TDM);

    // ---- 7. Set all channels to 0 dB volume (0xFF) ----
    _savedVol = ES9841_VOL_0DB;
    _writeReg(ES9841_REG_CH1_VOLUME, ES9841_VOL_0DB);
    _writeReg(ES9841_REG_CH2_VOLUME, ES9841_VOL_0DB);
    _writeReg(ES9841_REG_CH3_VOLUME, ES9841_VOL_0DB);
    _writeReg(ES9841_REG_CH4_VOLUME, ES9841_VOL_0DB);

    // ---- 8. Set PGA gain: 3-bit packed per channel (0-7, 6 dB steps, 0-42 dB) ----
    uint8_t gainStep = (_gainDb / 6);
    if (gainStep > 7) gainStep = 7;
    // REG_GAIN_PAIR1 (0x55): bits[6:4]=CH2_GAIN, bits[2:0]=CH1_GAIN
    uint8_t gainPair1 = (uint8_t)((gainStep & 0x07) | ((gainStep & 0x07) << 4));
    // REG_GAIN_PAIR2 (0x56): bits[6:4]=CH4_GAIN, bits[2:0]=CH3_GAIN
    uint8_t gainPair2 = (uint8_t)((gainStep & 0x07) | ((gainStep & 0x07) << 4));
    _writeReg(ES9841_REG_GAIN_PAIR1, gainPair1);
    _writeReg(ES9841_REG_GAIN_PAIR2, gainPair2);

    // ---- 9. Configure HPF (DC blocking): bit[2] per channel register ----
    uint8_t dcBit = _hpfEnabled ? ES9841_HPF_ENABLE_BIT : 0x00;
    _writeReg(ES9841_REG_CH1_DC_BLOCKING, dcBit);
    _writeReg(ES9841_REG_CH2_DC_BLOCKING, dcBit);
    _writeReg(ES9841_REG_CH3_DC_BLOCKING, dcBit);
    _writeReg(ES9841_REG_CH4_DC_BLOCKING, dcBit);

    // ---- 10. Set global filter preset: reg 0x4A bits[7:5] ----
    uint8_t preset = (_filterPreset > 7) ? 7 : _filterPreset;
    _writeReg(ES9841_REG_FILTER_CONFIG,
              (uint8_t)((preset << ES9841_FILTER_SHIFT) & ES9841_FILTER_MASK));

    // ---- 11. Configure I2S2 TDM and deinterleaver ----
    uint8_t port = (cfg && cfg->valid && cfg->i2sPort != 255) ? cfg->i2sPort : 2;
    int8_t dinPinRaw = (cfg && cfg->valid && cfg->pinData > 0) ? cfg->pinData : -1;
    if (dinPinRaw < 0) {
        LOG_W("[HAL:ES9841] No DATA_IN pin configured — set pinData in HAL config");
        dinPinRaw = 0;
    }

#ifndef NATIVE_TEST
    bool tdmOk = i2s_port_enable_rx((uint8_t)port, I2S_MODE_TDM, 4,
                                     (gpio_num_t)dinPinRaw,
                                     I2S_GPIO_UNUSED,
                                     (gpio_num_t)I2S_BCK_PIN,
                                     (gpio_num_t)I2S_LRC_PIN);
    if (!tdmOk) {
        LOG_E("[HAL:ES9841] I2S TDM init failed (port=%u) — audio will be silent", port);
    }
#endif

    if (!_tdm.init((uint8_t)port)) {
        LOG_E("[HAL:ES9841] TDM deinterleaver init failed — out of memory");
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "TDM deinterleaver alloc failed");
    }

    _tdm.buildSources(_NAME_A, _NAME_B, &_srcA, &_srcB);

    // ---- 12. Mark device ready ----
    _initialized = true;
    _state = HAL_STATE_AVAILABLE;
    _ready = true;

    LOG_I("[HAL:ES9841] Ready — TDM mode, port=%u DIN=GPIO%d gain=%ddB hpf=%d filter=%u",
          port, dinPinRaw, _gainDb, (int)_hpfEnabled, _filterPreset);
    LOG_I("[HAL:ES9841] Registered sources: '%s' (pair A) + '%s' (pair B)",
          _NAME_A, _NAME_B);
    return hal_init_ok();
}

void HalEs9841::deinit() {
    if (!_initialized) return;

    _ready = false;

#ifndef NATIVE_TEST
    _writeReg(ES9841_REG_SYS_CONFIG, 0x00);
    // Release I2S expansion TDM RX via port-generic API
    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(getSlot());
    uint8_t port = (cfg && cfg->valid && cfg->i2sPort != 255) ? cfg->i2sPort : 2;
    i2s_port_disable_rx(port);
#endif

    _tdm.deinit();

    _initialized = false;
    _state       = HAL_STATE_REMOVED;

    LOG_I("[HAL:ES9841] Deinitialized (TDM + I2S2 released)");
}

void HalEs9841::dumpConfig() {
    LOG_I("[HAL:ES9841] %s by %s (compat=%s) i2c=0x%02X bus=%u sda=%d scl=%d "
          "sr=%luHz bits=%u gain=%ddB hpf=%d filter=%u",
          _descriptor.name, _descriptor.manufacturer, _descriptor.compatible,
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth, _gainDb,
          (int)_hpfEnabled, _filterPreset);
}

bool HalEs9841::healthCheck() {
#ifndef NATIVE_TEST
    if (!_wire || !_initialized) return false;
    uint8_t id = _readReg(ES9841_REG_CHIP_ID);
    return (id == ES9841_CHIP_ID);
#else
    return _initialized;
#endif
}

// ===== HalAudioDevice =====

bool HalEs9841::configure(uint32_t sampleRate, uint8_t bitDepth) {
    const uint32_t supported[] = { 44100, 48000, 96000, 192000 };
    bool valid = false;
    for (uint8_t i = 0; i < 4; i++) {
        if (sampleRate == supported[i]) { valid = true; break; }
    }
    if (!valid) {
        LOG_W("[HAL:ES9841] Unsupported sample rate: %luHz", (unsigned long)sampleRate);
        return false;
    }
    if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32) {
        LOG_W("[HAL:ES9841] Unsupported bit depth: %u", bitDepth);
        return false;
    }
    _sampleRate = sampleRate;
    _bitDepth   = bitDepth;
    LOG_I("[HAL:ES9841] Configured: %luHz %ubit", (unsigned long)sampleRate, bitDepth);
    return true;
}

bool HalEs9841::setVolume(uint8_t percent) {
    if (!_initialized) return false;
    if (percent > 100) percent = 100;
    // ES9841: 8-bit volume, 0xFF=0dB (100%), 0x00=mute (0%).
    // Linear mapping: percent * 0xFF / 100.
    uint8_t vol8;
    if (percent == 0) {
        vol8 = ES9841_VOL_MUTE;
    } else if (percent == 100) {
        vol8 = ES9841_VOL_0DB;
    } else {
        vol8 = (uint8_t)(((uint32_t)percent * 0xFF) / 100);
    }
    _savedVol = vol8;
    bool ok  = _writeReg(ES9841_REG_CH1_VOLUME, vol8);
    ok       = ok && _writeReg(ES9841_REG_CH2_VOLUME, vol8);
    ok       = ok && _writeReg(ES9841_REG_CH3_VOLUME, vol8);
    ok       = ok && _writeReg(ES9841_REG_CH4_VOLUME, vol8);
    LOG_D("[HAL:ES9841] Volume: %d%% -> 0x%02X", percent, vol8);
    return ok;
}

bool HalEs9841::setMute(bool mute) {
    if (!_initialized) return false;
    // Mute: write 0x00 to all volume regs. Unmute: restore _savedVol.
    uint8_t vol8 = mute ? ES9841_VOL_MUTE : _savedVol;
    bool ok  = _writeReg(ES9841_REG_CH1_VOLUME, vol8);
    ok       = ok && _writeReg(ES9841_REG_CH2_VOLUME, vol8);
    ok       = ok && _writeReg(ES9841_REG_CH3_VOLUME, vol8);
    ok       = ok && _writeReg(ES9841_REG_CH4_VOLUME, vol8);
    LOG_I("[HAL:ES9841] %s", mute ? "Muted" : "Unmuted");
    return ok;
}

// ===== HalAudioAdcInterface =====

bool HalEs9841::adcSetGain(uint8_t gainDb) {
    if (!_initialized) return false;
    // 3-bit gain: 0=0dB, 7=42dB, 6 dB per step; max 42 dB
    if (gainDb > 42) gainDb = 42;
    uint8_t gainStep = gainDb / 6;
    if (gainStep > 7) gainStep = 7;
    // REG_GAIN_PAIR1 (0x55): bits[6:4]=CH2_GAIN, bits[2:0]=CH1_GAIN
    uint8_t gainPair1 = (uint8_t)((gainStep & 0x07) | ((gainStep & 0x07) << 4));
    // REG_GAIN_PAIR2 (0x56): bits[6:4]=CH4_GAIN, bits[2:0]=CH3_GAIN
    uint8_t gainPair2 = (uint8_t)((gainStep & 0x07) | ((gainStep & 0x07) << 4));
    bool ok  = _writeReg(ES9841_REG_GAIN_PAIR1, gainPair1);
    ok       = ok && _writeReg(ES9841_REG_GAIN_PAIR2, gainPair2);
    _gainDb  = (uint8_t)(gainStep * 6);
    LOG_I("[HAL:ES9841] ADC gain: %ddB (step=%u pair1=0x%02X pair2=0x%02X)",
          _gainDb, gainStep, gainPair1, gainPair2);
    return ok;
}

bool HalEs9841::adcSetHpfEnabled(bool en) {
    if (!_initialized) return false;
    uint8_t dcBit = en ? ES9841_HPF_ENABLE_BIT : 0x00;
    bool ok  = _writeReg(ES9841_REG_CH1_DC_BLOCKING, dcBit);
    ok       = ok && _writeReg(ES9841_REG_CH2_DC_BLOCKING, dcBit);
    ok       = ok && _writeReg(ES9841_REG_CH3_DC_BLOCKING, dcBit);
    ok       = ok && _writeReg(ES9841_REG_CH4_DC_BLOCKING, dcBit);
    _hpfEnabled = en;
    LOG_I("[HAL:ES9841] HPF: %s", en ? "enabled" : "disabled");
    return ok;
}

bool HalEs9841::adcSetSampleRate(uint32_t hz) {
    return configure(hz, _bitDepth);
}

// ===== ES9841-specific extensions =====

bool HalEs9841::setFilterPreset(uint8_t preset) {
    if (!_initialized) return false;
    if (preset > 7) return false;
    // Preserve lower bits of REG_FILTER_CONFIG outside the FILTER_SHAPE field
    uint8_t cur = _readReg(ES9841_REG_FILTER_CONFIG);
    uint8_t val = (uint8_t)((cur & ~ES9841_FILTER_MASK) |
                            ((preset << ES9841_FILTER_SHIFT) & ES9841_FILTER_MASK));
    bool ok = _writeReg(ES9841_REG_FILTER_CONFIG, val);
    _filterPreset = preset;
    LOG_I("[HAL:ES9841] Filter preset: %u", preset);
    return ok;
}

bool HalEs9841::setChannelVolume(uint8_t ch, uint8_t vol8) {
    if (!_initialized) return false;
    const uint8_t volRegs[4] = {
        ES9841_REG_CH1_VOLUME,
        ES9841_REG_CH2_VOLUME,
        ES9841_REG_CH3_VOLUME,
        ES9841_REG_CH4_VOLUME
    };
    if (ch >= 4) {
        LOG_W("[HAL:ES9841] setChannelVolume: invalid channel %u (0-3 only)", ch);
        return false;
    }
    bool ok = _writeReg(volRegs[ch], vol8);
    LOG_D("[HAL:ES9841] Channel %u volume: 0x%02X", ch, vol8);
    return ok;
}

// ===== AudioInputSource — dual-source TDM accessor =====

const AudioInputSource* HalEs9841::getInputSourceAt(int idx) const {
    if (!_initialized) return nullptr;
    if (idx == 0) return &_srcA;
    if (idx == 1) return &_srcB;
    return nullptr;
}

#endif // DAC_ENABLED
