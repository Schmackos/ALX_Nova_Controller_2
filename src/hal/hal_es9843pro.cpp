#ifdef DAC_ENABLED
// HalEs9843pro — ESS ES9843PRO 4-channel 32-bit ADC implementation
// Register map sourced from src/drivers/es9843pro_regs.h (ESS datasheet).

#include "hal_es9843pro.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include <Wire.h>
#include "hal_ess_sabre_adc_base.h"  // for extern TwoWire Wire2
#include <Arduino.h>
#include "../debug_serial.h"
#include "../i2s_audio.h"
#include "../drivers/es9843pro_regs.h"
#else
// ===== Native test stubs — no hardware access =====
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)

// Register address constants (mirrors es9843pro_regs.h for native compilation;
// all actual hardware writes are inside #ifndef NATIVE_TEST blocks below).
#define ES9843PRO_I2C_ADDR                0x40
#define ES9843PRO_CHIP_ID                 0x8F
#define ES9843PRO_REG_SYS_CONFIG          0x00
#define ES9843PRO_REG_OUTPUT_FORMAT       0x03
#define ES9843PRO_REG_FILTER_CONFIG       0x4A
#define ES9843PRO_REG_DC_BLOCK            0x4C
#define ES9843PRO_REG_DC_BLOCK_MSB        0x4D
#define ES9843PRO_REG_CH1_VOLUME          0x51
#define ES9843PRO_REG_CH2_VOLUME          0x52
#define ES9843PRO_REG_CH3_VOLUME          0x53
#define ES9843PRO_REG_CH4_VOLUME          0x54
#define ES9843PRO_REG_GAIN_PAIR1          0x55
#define ES9843PRO_REG_GAIN_PAIR2          0x56
#define ES9843PRO_REG_ASP_CONTROL         0x6B
#define ES9843PRO_REG_ASP_BYPASS          0x6C
#define ES9843PRO_REG_CHIP_ID             0xE1
#define ES9843PRO_SOFT_RESET_CMD          0xA0
#define ES9843PRO_OUTPUT_I2S              0x00
#define ES9843PRO_TDM_FORMAT              0x10   // TDM output mode (reg 0x03 OUTPUT_SEL bits6:4 = 0b001)
#define ES9843PRO_ENABLE_4CH              0x0F
#define ES9843PRO_ASP_BYPASS_ALL          0x0F
#define ES9843PRO_VOL_0DB                 0x00
#define ES9843PRO_VOL_MUTE                0xFF
// i2s_audio stubs — TDM functions covered by hal_tdm_deinterleaver native stubs
inline uint32_t i2s_audio_get_sample_rate(void) { return 48000; }
inline bool i2s_audio_enable_expansion_tdm_rx(uint32_t, int, uint8_t) { return true; }
inline void i2s_audio_disable_expansion_rx() {}
#endif // NATIVE_TEST

// ===== Constructor =====

HalEs9843pro::HalEs9843pro() : HalAudioDevice() {
    memset(&_descriptor, 0, sizeof(_descriptor));
    strncpy(_descriptor.compatible,   "ess,es9843pro",     31);
    strncpy(_descriptor.name,         "ES9843PRO",         32);
    strncpy(_descriptor.manufacturer, "ESS Technology",    32);
    _descriptor.type            = HAL_DEV_ADC;
    _descriptor.legacyId        = 0;
    _descriptor.channelCount    = 4;
    _descriptor.i2cAddr         = 0x40;
    _descriptor.bus.type        = HAL_BUS_I2C;
    _descriptor.bus.index       = HAL_I2C_BUS_EXP;
    _descriptor.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K;
    _descriptor.capabilities    = HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME |
                                  HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL;
    _initPriority = HAL_PRIORITY_HARDWARE;
}

// ===== I2C helpers =====

bool HalEs9843pro::_writeReg(uint8_t reg, uint8_t val) {
#ifndef NATIVE_TEST
    if (!_wire) return false;
    _wire->beginTransmission(_i2cAddr);
    _wire->write(reg);
    _wire->write(val);
    uint8_t err = _wire->endTransmission();
    if (err != 0) {
        LOG_E("[HAL:ES9843PRO] I2C write failed: reg=0x%02X val=0x%02X err=%d", reg, val, err);
        return false;
    }
    return true;
#else
    (void)reg; (void)val;
    return true;
#endif
}

uint8_t HalEs9843pro::_readReg(uint8_t reg) {
#ifndef NATIVE_TEST
    if (!_wire) return 0xFF;
    _wire->beginTransmission(_i2cAddr);
    _wire->write(reg);
    _wire->endTransmission(false);
    _wire->requestFrom(_i2cAddr, (uint8_t)1);
    if (_wire->available()) return _wire->read();
    LOG_E("[HAL:ES9843PRO] I2C read failed: reg=0x%02X", reg);
    return 0xFF;
#else
    (void)reg;
    return 0x00;
#endif
}

// ===== HalDevice lifecycle =====

bool HalEs9843pro::probe() {
#ifndef NATIVE_TEST
    if (!_wire) return false;
    _wire->beginTransmission(_i2cAddr);
    uint8_t err = _wire->endTransmission();
    if (err != 0) return false;
    uint8_t chipId = _readReg(ES9843PRO_REG_CHIP_ID);
    return (chipId == ES9843PRO_CHIP_ID);
#else
    return true;
#endif
}

HalInitResult HalEs9843pro::init() {
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
        _filterPreset = cfg->filterMode;   // 0-7 stored in filterMode field
    }

    LOG_I("[HAL:ES9843PRO] Initializing (I2C addr=0x%02X bus=%u SDA=%d SCL=%d sr=%luHz bits=%u)",
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
    LOG_I("[HAL:ES9843PRO] I2C initialized (bus %u SDA=%d SCL=%d 400kHz)",
          _i2cBusIndex, _sdaPin, _sclPin);
#endif

    // ---- 4. Soft reset: write 0xA0 to reg 0x00 (SOFT_RESET + EN_MCLK_IN) ----
    _writeReg(ES9843PRO_REG_SYS_CONFIG, ES9843PRO_SOFT_RESET_CMD);
#ifndef NATIVE_TEST
    delay(5);   // Allow reset to complete before continuing
#endif

    // ---- 5. Read chip ID at 0xE1, verify == 0x8F ----
    uint8_t chipId = _readReg(ES9843PRO_REG_CHIP_ID);
    if (chipId != ES9843PRO_CHIP_ID) {
        LOG_W("[HAL:ES9843PRO] Unexpected chip ID: 0x%02X (expected 0x%02X) — continuing",
              chipId, ES9843PRO_CHIP_ID);
    } else {
        LOG_I("[HAL:ES9843PRO] Chip ID OK (0x%02X)", chipId);
    }

    // ---- 6. Enable all 4 ADC channels: reg 0x00 bits3:0 = 0x0F ----
    _writeReg(ES9843PRO_REG_SYS_CONFIG, ES9843PRO_ENABLE_4CH);

    // ---- 7. Configure output format: TDM mode (reg 0x03 bits2:0 = 0b100) ----
    // ES9843PRO_TDM_FORMAT enables 4-slot TDM output on a single data line.
    // This replaces ES9843PRO_OUTPUT_I2S (0x00 = standard stereo I2S).
    _writeReg(ES9843PRO_REG_OUTPUT_FORMAT, ES9843PRO_TDM_FORMAT);

    // ---- 8. Set all 4 channels to 0 dB volume: regs 0x51-0x54 = 0x00 ----
    _writeReg(ES9843PRO_REG_CH1_VOLUME, ES9843PRO_VOL_0DB);
    _writeReg(ES9843PRO_REG_CH2_VOLUME, ES9843PRO_VOL_0DB);
    _writeReg(ES9843PRO_REG_CH3_VOLUME, ES9843PRO_VOL_0DB);
    _writeReg(ES9843PRO_REG_CH4_VOLUME, ES9843PRO_VOL_0DB);

    // ---- 9. Set PGA gain: regs 0x55-0x56 (3-bit packed per channel) ----
    // gainReg range 0-7 (0=0dB, 7=+42dB, 6 dB per step); clamp to 7
    uint8_t gainStep = (_gainDb / 6);
    if (gainStep > 7) gainStep = 7;
    // REG_GAIN_PAIR1 (0x55): bits2:0=CH1, bits5:3=CH2
    uint8_t gainPair1 = (uint8_t)((gainStep & 0x07) | ((gainStep & 0x07) << 3));
    // REG_GAIN_PAIR2 (0x56): bits2:0=CH3, bits5:3=CH4 (bit7=MONO_VOL_MODE, leave 0)
    uint8_t gainPair2 = (uint8_t)((gainStep & 0x07) | ((gainStep & 0x07) << 3));
    _writeReg(ES9843PRO_REG_GAIN_PAIR1, gainPair1);
    _writeReg(ES9843PRO_REG_GAIN_PAIR2, gainPair2);

    // ---- 10. Configure HPF (DC blocking): reg 0x4C bits15:12=DC_BLOCK_EN_CH4..CH1 ----
    // The HPF enable bits live in the MSB register (0x4D), bits7:4 = CH4..CH1
    // Write LSB (0x4C) to 0x00 first (FIR coefficient bits — leave default),
    // then write MSB (0x4D) with the enable nibble in bits7:4.
    uint8_t dcBlockMsb = _hpfEnabled ? 0xF0 : 0x00;  // All 4 channel bits set/cleared
    _writeReg(ES9843PRO_REG_DC_BLOCK,     0x00);
    _writeReg(ES9843PRO_REG_DC_BLOCK_MSB, dcBlockMsb);

    // ---- 11. Set global filter shape: reg 0x4A bits7:5 = FILTER_SHAPE ----
    uint8_t preset = (_filterPreset > 7) ? 7 : _filterPreset;
    _writeReg(ES9843PRO_REG_FILTER_CONFIG, (uint8_t)(preset << 5));

    // ---- 12. Disable ASP2: reg 0x6B = 0x00, reg 0x6C = 0x0F (all channels bypass) ----
    _writeReg(ES9843PRO_REG_ASP_CONTROL, 0x00);
    _writeReg(ES9843PRO_REG_ASP_BYPASS,  ES9843PRO_ASP_BYPASS_ALL);

    // ---- 13. Configure I2S2 in TDM mode and init the deinterleaver ----
    // The i2sPort config field selects the I2S peripheral.  The ES9843PRO
    // always uses port 2 (I2S2) as the expansion mezzanine ADC RX path.
    uint8_t port = (cfg && cfg->valid && cfg->i2sPort != 255) ? cfg->i2sPort : 2;

    // Resolve the DATA_IN pin: cfg->pinData if set, otherwise no default is
    // available for the expansion bus — the user must configure it.
    int8_t dinPinRaw = (cfg && cfg->valid && cfg->pinData > 0) ? cfg->pinData : -1;
    if (dinPinRaw < 0) {
        LOG_W("[HAL:ES9843PRO] No DATA_IN pin configured — set pinData in HAL config");
        // Non-fatal: continue init so I2C registers are programmed.  TDM reads
        // will return 0 frames until the user provides the correct pin and reinits.
        dinPinRaw = 0;  // I2S_GPIO_UNUSED equivalent for log readability
    }

#ifndef NATIVE_TEST
    // Enable I2S2 in TDM mode with 4 slots (one per ES9843PRO output channel)
    bool tdmOk = i2s_audio_enable_expansion_tdm_rx(
        _sampleRate,
        (gpio_num_t)dinPinRaw,
        4   // 4 TDM slots: CH1/CH2/CH3/CH4
    );
    if (!tdmOk) {
        LOG_E("[HAL:ES9843PRO] I2S2 TDM init failed — audio will be silent");
        // Non-fatal: device still registers; operator can reinit via REST API.
    }
#endif

    // Allocate deinterleaver ping-pong buffers (PSRAM preferred)
    if (!_tdm.init((uint8_t)port)) {
        LOG_E("[HAL:ES9843PRO] TDM deinterleaver init failed — out of memory");
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "TDM deinterleaver alloc failed");
    }

    // Build the two AudioInputSource structs from the deinterleaver
    _tdm.buildSources(_NAME_A, _NAME_B, &_srcA, &_srcB);

    // ---- 14. Mark device ready ----
    _initialized = true;
    _state = HAL_STATE_AVAILABLE;
    _ready = true;

    LOG_I("[HAL:ES9843PRO] Ready — TDM mode, port=%u DIN=GPIO%d gain=%ddB hpf=%d filter=%u",
          port, dinPinRaw, _gainDb, (int)_hpfEnabled, _filterPreset);
    LOG_I("[HAL:ES9843PRO] Registered sources: '%s' (pair A) + '%s' (pair B)",
          _NAME_A, _NAME_B);
    return hal_init_ok();
}

void HalEs9843pro::deinit() {
    if (!_initialized) return;

    _ready = false;

#ifndef NATIVE_TEST
    // Power down: disable all ADC channels (reg 0x00 = 0x00)
    _writeReg(ES9843PRO_REG_SYS_CONFIG, 0x00);

    // Release I2S2 expansion TDM RX (decrements _i2s2.users bit 0x02)
    i2s_audio_disable_expansion_rx();
#endif

    // Release deinterleaver ping-pong buffers
    _tdm.deinit();

    _initialized = false;
    _state       = HAL_STATE_REMOVED;

    LOG_I("[HAL:ES9843PRO] Deinitialized (TDM + I2S2 released)");
}

void HalEs9843pro::dumpConfig() {
    LOG_I("[HAL:ES9843PRO] %s by %s (compat=%s) i2c=0x%02X bus=%u sda=%d scl=%d "
          "sr=%luHz bits=%u gain=%ddB hpf=%d filter=%u",
          _descriptor.name, _descriptor.manufacturer, _descriptor.compatible,
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth, _gainDb,
          (int)_hpfEnabled, _filterPreset);
}

bool HalEs9843pro::healthCheck() {
#ifndef NATIVE_TEST
    if (!_wire || !_initialized) return false;
    uint8_t id = _readReg(ES9843PRO_REG_CHIP_ID);
    return (id == ES9843PRO_CHIP_ID);
#else
    return _initialized;
#endif
}

// ===== HalAudioDevice =====

bool HalEs9843pro::configure(uint32_t sampleRate, uint8_t bitDepth) {
    // ES9843PRO derives sample rate from MCLK ratio via hardware clock dividers;
    // the register path only gates clocks. Store requested values and validate.
    const uint32_t supported[] = { 44100, 48000, 96000, 192000 };
    bool valid = false;
    for (uint8_t i = 0; i < 4; i++) {
        if (sampleRate == supported[i]) { valid = true; break; }
    }
    if (!valid) {
        LOG_W("[HAL:ES9843PRO] Unsupported sample rate: %luHz", (unsigned long)sampleRate);
        return false;
    }
    _sampleRate = sampleRate;
    _bitDepth   = bitDepth;
    LOG_I("[HAL:ES9843PRO] Configured: %luHz %ubit", (unsigned long)sampleRate, bitDepth);
    return true;
}

bool HalEs9843pro::setVolume(uint8_t percent) {
    if (!_initialized) return false;
    if (percent > 100) percent = 100;
    // Map 0% → 0xFF (mute), 100% → 0x00 (0 dB).
    // Linear interpolation: 0% gives full attenuation (mute byte 0xFF),
    // 100% gives 0x00 (unity). Values in between scale linearly.
    // vol8 = 0xFF - round((percent * 0xFF) / 100)
    uint8_t vol8;
    if (percent == 0) {
        vol8 = ES9843PRO_VOL_MUTE;
    } else if (percent == 100) {
        vol8 = ES9843PRO_VOL_0DB;
    } else {
        vol8 = (uint8_t)(0xFE - (uint8_t)(((uint32_t)(percent - 1) * 0xFE) / 99));
    }
    bool ok  = _writeReg(ES9843PRO_REG_CH1_VOLUME, vol8);
    ok       = ok && _writeReg(ES9843PRO_REG_CH2_VOLUME, vol8);
    ok       = ok && _writeReg(ES9843PRO_REG_CH3_VOLUME, vol8);
    ok       = ok && _writeReg(ES9843PRO_REG_CH4_VOLUME, vol8);
    LOG_D("[HAL:ES9843PRO] Volume: %d%% -> 0x%02X", percent, vol8);
    return ok;
}

bool HalEs9843pro::setMute(bool mute) {
    if (!_initialized) return false;
    uint8_t vol8 = mute ? ES9843PRO_VOL_MUTE : ES9843PRO_VOL_0DB;
    bool ok  = _writeReg(ES9843PRO_REG_CH1_VOLUME, vol8);
    ok       = ok && _writeReg(ES9843PRO_REG_CH2_VOLUME, vol8);
    ok       = ok && _writeReg(ES9843PRO_REG_CH3_VOLUME, vol8);
    ok       = ok && _writeReg(ES9843PRO_REG_CH4_VOLUME, vol8);
    LOG_I("[HAL:ES9843PRO] %s", mute ? "Muted" : "Unmuted");
    return ok;
}

// ===== HalAudioAdcInterface =====

bool HalEs9843pro::adcSetGain(uint8_t gainDb) {
    if (!_initialized) return false;
    // Floor to nearest 6 dB step, clamp to 42 dB max (register 7)
    if (gainDb > 42) gainDb = 42;
    uint8_t gainStep = gainDb / 6;
    if (gainStep > 7) gainStep = 7;
    // Pack all 4 channels with the same gain step
    // REG_GAIN_PAIR1 (0x55): bits2:0=CH1, bits5:3=CH2
    uint8_t gainPair1 = (uint8_t)((gainStep & 0x07) | ((gainStep & 0x07) << 3));
    // REG_GAIN_PAIR2 (0x56): bits2:0=CH3, bits5:3=CH4 (bit7=MONO_VOL_MODE, leave 0)
    uint8_t gainPair2 = (uint8_t)((gainStep & 0x07) | ((gainStep & 0x07) << 3));
    bool ok  = _writeReg(ES9843PRO_REG_GAIN_PAIR1, gainPair1);
    ok       = ok && _writeReg(ES9843PRO_REG_GAIN_PAIR2, gainPair2);
    _gainDb  = (uint8_t)(gainStep * 6);
    LOG_I("[HAL:ES9843PRO] ADC gain: %ddB (step=%u reg1=0x%02X reg2=0x%02X)",
          _gainDb, gainStep, gainPair1, gainPair2);
    return ok;
}

bool HalEs9843pro::adcSetHpfEnabled(bool en) {
    if (!_initialized) return false;
    // DC_BLOCK_EN bits for all 4 channels live in REG_DC_BLOCK_MSB (0x4D) bits7:4
    uint8_t dcBlockMsb = en ? 0xF0 : 0x00;
    bool ok = _writeReg(ES9843PRO_REG_DC_BLOCK_MSB, dcBlockMsb);
    _hpfEnabled = en;
    LOG_I("[HAL:ES9843PRO] HPF: %s", en ? "enabled" : "disabled");
    return ok;
}

bool HalEs9843pro::adcSetSampleRate(uint32_t hz) {
    return configure(hz, _bitDepth);
}

// ===== ES9843PRO-specific extensions =====

bool HalEs9843pro::setFilterPreset(uint8_t preset) {
    if (!_initialized) return false;
    if (preset > 7) preset = 7;
    // FILTER_SHAPE lives in bits7:5 of REG_FILTER_CONFIG (0x4A).
    // Preserve lower 5 bits (BYPASS_FIR2X, BYPASS_FIR4X, BYPASS_IIR, and others).
    uint8_t cur = _readReg(ES9843PRO_REG_FILTER_CONFIG);
    uint8_t val = (uint8_t)((cur & 0x1F) | (uint8_t)((preset & 0x07) << 5));
    bool ok = _writeReg(ES9843PRO_REG_FILTER_CONFIG, val);
    _filterPreset = preset;
    LOG_I("[HAL:ES9843PRO] Filter preset: %u", preset);
    return ok;
}

bool HalEs9843pro::setChannelVolume(uint8_t ch, uint8_t vol8) {
    if (!_initialized) return false;
    const uint8_t volRegs[4] = {
        ES9843PRO_REG_CH1_VOLUME,
        ES9843PRO_REG_CH2_VOLUME,
        ES9843PRO_REG_CH3_VOLUME,
        ES9843PRO_REG_CH4_VOLUME
    };
    if (ch >= 4) {
        LOG_W("[HAL:ES9843PRO] setChannelVolume: invalid channel %u (0-3 only)", ch);
        return false;
    }
    bool ok = _writeReg(volRegs[ch], vol8);
    LOG_D("[HAL:ES9843PRO] Channel %u volume: 0x%02X", ch, vol8);
    return ok;
}

// ===== AudioInputSource — dual-source TDM accessor =====

const AudioInputSource* HalEs9843pro::getInputSourceAt(int idx) const {
    // Sources are valid as soon as init() completes and buildSources() populated
    // the structs — we do not require _tdm.isReady() (which needs a real DMA read)
    // because the bridge registers sources immediately after init() returns.
    if (!_initialized) return nullptr;
    if (idx == 0) return &_srcA;
    if (idx == 1) return &_srcB;
    return nullptr;
}

#endif // DAC_ENABLED
