#ifdef DAC_ENABLED
// HalEs9038pro — ESS ES9038PRO 8-channel 32-bit DAC implementation
// Register map sourced from src/drivers/es9038pro_regs.h (ESS datasheet).
// Architecture: HyperStream II, 132 dB DNR, 8-channel, PCM up to 768kHz.

#include "hal_es9038pro.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include <Wire.h>
#include "hal_ess_sabre_adc_base.h"  // for extern TwoWire Wire2
#include <Arduino.h>
#include "../debug_serial.h"
#include "../i2s_audio.h"
#include "../config.h"              // ES8311_I2S_MCLK_PIN / SCLK_PIN / LRCK_PIN defaults
#include "../drivers/es9038pro_regs.h"
#else
// ===== Native test stubs — no hardware access =====
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)

// Register address constants (mirrors es9038pro_regs.h for native compilation;
// all actual hardware writes are inside #ifndef NATIVE_TEST blocks below).
#define ES9038PRO_I2C_ADDR               0x48
#define ES9038PRO_CHIP_ID                0x03
#define ES9038PRO_REG_SYS_CONFIG         0x00
#define ES9038PRO_REG_INPUT_CFG          0x01
#define ES9038PRO_REG_FILTER_MUTE        0x07
#define ES9038PRO_REG_MASTER_MODE        0x0A
#define ES9038PRO_REG_DPLL_CFG           0x0C
#define ES9038PRO_REG_SOFT_START         0x0E
#define ES9038PRO_REG_VOL_CH1            0x0F
#define ES9038PRO_REG_VOL_CH2            0x10
#define ES9038PRO_REG_VOL_CH3            0x11
#define ES9038PRO_REG_VOL_CH4            0x12
#define ES9038PRO_REG_VOL_CH5            0x13
#define ES9038PRO_REG_VOL_CH6            0x14
#define ES9038PRO_REG_VOL_CH7            0x15
#define ES9038PRO_REG_VOL_CH8            0x16
#define ES9038PRO_REG_CHIP_ID            0xE1
#define ES9038PRO_SOFT_RESET_BIT         0x01
#define ES9038PRO_CHANNEL_MODE_8CH       0x04
#define ES9038PRO_INPUT_TDM              0x0C
#define ES9038PRO_INPUT_32BIT            0x20
#define ES9038PRO_SLAVE_MODE             0x00
#define ES9038PRO_INIT_DPLL_BW           0x05
#define ES9038PRO_FILTER_MASK            0x07
#define ES9038PRO_MUTE_BIT               0x20
#define ES9038PRO_VOL_0DB                0x00
#define ES9038PRO_VOL_MUTE               0xFF

#endif // NATIVE_TEST

// ===== Constructor =====

HalEs9038pro::HalEs9038pro() : HalEssSabreDacBase() {
    hal_init_descriptor(_descriptor, "ess,es9038pro", "ES9038PRO", "ESS Technology",
        HAL_DEV_DAC, 8, ES9038PRO_I2C_ADDR, HAL_BUS_I2C, HAL_I2C_BUS_EXP,
        HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K | HAL_RATE_192K | HAL_RATE_384K | HAL_RATE_768K,
        HAL_CAP_DAC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_FILTERS);
    _initPriority = HAL_PRIORITY_HARDWARE;
    _i2cAddr     = ES9038PRO_I2C_ADDR;
    _sdaPin      = ESS_SABRE_I2C_BUS2_SDA;
    _sclPin      = ESS_SABRE_I2C_BUS2_SCL;
    _i2cBusIndex = HAL_I2C_BUS_EXP;
}

// ===== HalDevice lifecycle =====

bool HalEs9038pro::probe() {
#ifndef NATIVE_TEST
    if (!_wire) return false;
    _wire->beginTransmission(_i2cAddr);
    uint8_t err = _wire->endTransmission();
    if (err != 0) return false;
    uint8_t chipId = _readReg(ES9038PRO_REG_CHIP_ID);
    return (chipId == ES9038PRO_CHIP_ID);
#else
    return true;
#endif
}

HalInitResult HalEs9038pro::init() {
    // ---- 1. Apply per-device config overrides from HAL Device Manager ----
    _applyConfigOverrides();

    LOG_I("[HAL:ES9038PRO] Initializing (I2C addr=0x%02X bus=%u SDA=%d SCL=%d sr=%luHz bits=%u)",
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth);

    // ---- 2. Select TwoWire instance and initialize I2C bus at 400 kHz ----
    _selectWire();

    // ---- 3. Soft reset (reg 0x00 bit0, self-clearing) ----
    _writeReg(ES9038PRO_REG_SYS_CONFIG, ES9038PRO_SOFT_RESET_BIT);
#ifndef NATIVE_TEST
    delay(ESS_SABRE_RESET_DELAY_MS);
#endif

    // ---- 4. Verify chip ID (reg 0xE1, expect 0x03) ----
    uint8_t chipId = _readReg(ES9038PRO_REG_CHIP_ID);
    if (chipId != ES9038PRO_CHIP_ID) {
        LOG_W("[HAL:ES9038PRO] Unexpected chip ID: 0x%02X (expected 0x%02X) — continuing",
              chipId, ES9038PRO_CHIP_ID);
    } else {
        LOG_I("[HAL:ES9038PRO] Chip ID OK (0x%02X)", chipId);
    }

    // ---- 5. Configure 8-channel mode (reg 0x00 bits[3:2] = 0b01) ----
    _writeReg(ES9038PRO_REG_SYS_CONFIG, ES9038PRO_CHANNEL_MODE_8CH);

    // ---- 6. Configure TDM input format: 32-bit TDM slave mode (reg 0x01) ----
    _writeReg(ES9038PRO_REG_INPUT_CFG, (uint8_t)(ES9038PRO_INPUT_TDM | ES9038PRO_INPUT_32BIT));

    // ---- 7. I2S slave mode (ESP32-P4 is I2S master providing BCK/WS/MCLK) ----
    _writeReg(ES9038PRO_REG_MASTER_MODE, ES9038PRO_SLAVE_MODE);

    // ---- 8. DPLL bandwidth for jitter rejection ----
    _writeReg(ES9038PRO_REG_DPLL_CFG, ES9038PRO_INIT_DPLL_BW);

    // ---- 9. Soft start configuration (default — leave at power-on value) ----
    _writeReg(ES9038PRO_REG_SOFT_START, 0x00);

    // ---- 10. Per-channel volume: apply stored level (0-100% -> 0x00-0xFF attenuation) ----
    uint8_t volReg = (uint8_t)((100U - _volume) * 255U / 100U);
    _writeReg(ES9038PRO_REG_VOL_CH1, volReg);
    _writeReg(ES9038PRO_REG_VOL_CH2, volReg);
    _writeReg(ES9038PRO_REG_VOL_CH3, volReg);
    _writeReg(ES9038PRO_REG_VOL_CH4, volReg);
    _writeReg(ES9038PRO_REG_VOL_CH5, volReg);
    _writeReg(ES9038PRO_REG_VOL_CH6, volReg);
    _writeReg(ES9038PRO_REG_VOL_CH7, volReg);
    _writeReg(ES9038PRO_REG_VOL_CH8, volReg);

    // ---- 11. Digital filter preset (bits[2:0] of reg 0x07) ----
    uint8_t preset = (_filterPreset > 7) ? 7 : _filterPreset;
    uint8_t filterReg = (uint8_t)(preset & ES9038PRO_FILTER_MASK);
    if (_muted) filterReg |= ES9038PRO_MUTE_BIT;
    _writeReg(ES9038PRO_REG_FILTER_MUTE, filterReg);

    // ---- 12. Enable expansion TDM TX output (8-slot TDM) ----
    // Resolve port from HAL device config (default port 2 for expansion devices).
    HalDeviceConfig* _tdmCfg = HalDeviceManager::instance().getConfig(_slot);
    uint8_t tdmPort = (_tdmCfg && _tdmCfg->valid && _tdmCfg->i2sPort != 255) ? _tdmCfg->i2sPort : 2;
#ifndef NATIVE_TEST
    gpio_num_t doutPin = (_doutPin >= 0) ? (gpio_num_t)_doutPin : GPIO_NUM_NC;
    gpio_num_t tdmMclk = (_tdmCfg && _tdmCfg->valid && _tdmCfg->pinMclk >= 0)
                         ? (gpio_num_t)_tdmCfg->pinMclk : (gpio_num_t)ES8311_I2S_MCLK_PIN;
    gpio_num_t tdmBck  = (_tdmCfg && _tdmCfg->valid && _tdmCfg->pinBck  >= 0)
                         ? (gpio_num_t)_tdmCfg->pinBck  : (gpio_num_t)ES8311_I2S_SCLK_PIN;
    gpio_num_t tdmWs   = (_tdmCfg && _tdmCfg->valid && _tdmCfg->pinLrc  >= 0)
                         ? (gpio_num_t)_tdmCfg->pinLrc  : (gpio_num_t)ES8311_I2S_LRCK_PIN;
    bool tdmOk = i2s_port_enable_tx(tdmPort, I2S_MODE_TDM, 8, doutPin, tdmMclk, tdmBck, tdmWs);
    if (!tdmOk) {
        LOG_E("[HAL:ES9038PRO] Expansion TDM TX enable failed (port=%u)", tdmPort);
        _state = HAL_STATE_ERROR;
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "TDM TX init failed");
    }
    _i2sTxEnabled = true;
#else
    _i2sTxEnabled = true;
#endif

    // ---- 13. Init TDM interleaver (allocate ping-pong buffers) ----
    if (!_tdm.init(tdmPort)) {
        LOG_E("[HAL:ES9038PRO] TDM interleaver init failed — out of memory");
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "TDM interleaver alloc failed");
    }

    // ---- 14. Build 4 AudioOutputSink structs (one per stereo pair) ----
    _tdm.buildSinks("ES9038PRO CH1/2", "ES9038PRO CH3/4",
                    "ES9038PRO CH5/6", "ES9038PRO CH7/8",
                    &_sinks[0], &_sinks[1], &_sinks[2], &_sinks[3],
                    _slot);
    _sinksBuilt = true;

    // ---- 15. Mark device ready ----
    _initialized = true;
    _state = HAL_STATE_AVAILABLE;
    _ready = true;

    LOG_I("[HAL:ES9038PRO] Ready (vol=%u%% muted=%d filter=%u sinks=4)",
          _volume, (int)_muted, _filterPreset);
    return hal_init_ok();
}

void HalEs9038pro::deinit() {
    if (!_initialized) return;

    _ready = false;

    // Mute all channels before shutdown
#ifndef NATIVE_TEST
    _writeReg(ES9038PRO_REG_FILTER_MUTE, ES9038PRO_MUTE_BIT);
#endif

    // Disable expansion TDM TX via port-generic API
#ifndef NATIVE_TEST
    if (_i2sTxEnabled) {
        HalDeviceConfig* _deiCfg = HalDeviceManager::instance().getConfig(_slot);
        uint8_t deiPort = (_deiCfg && _deiCfg->valid && _deiCfg->i2sPort != 255) ? _deiCfg->i2sPort : 2;
        i2s_port_disable_tx(deiPort);
        _i2sTxEnabled = false;
    }
#else
    _i2sTxEnabled = false;
#endif

    // Release TDM interleaver buffers
    _tdm.deinit();

    _initialized = false;
    _sinksBuilt  = false;
    _state       = HAL_STATE_REMOVED;

    LOG_I("[HAL:ES9038PRO] Deinitialized");
}

void HalEs9038pro::dumpConfig() {
    LOG_I("[HAL:ES9038PRO] %s by %s (compat=%s) i2c=0x%02X bus=%u sda=%d scl=%d "
          "sr=%luHz bits=%u vol=%u%% muted=%d filter=%u",
          _descriptor.name, _descriptor.manufacturer, _descriptor.compatible,
          _i2cAddr, _i2cBusIndex, _sdaPin, _sclPin,
          (unsigned long)_sampleRate, _bitDepth,
          _volume, (int)_muted, _filterPreset);
}

bool HalEs9038pro::healthCheck() {
#ifndef NATIVE_TEST
    if (!_initialized) return false;
    uint8_t id = _readReg(ES9038PRO_REG_CHIP_ID);
    return (id == ES9038PRO_CHIP_ID);
#else
    return _initialized;
#endif
}

// ===== HalAudioDevice =====

bool HalEs9038pro::configure(uint32_t sampleRate, uint8_t bitDepth) {
    const uint32_t supported[] = { 44100, 48000, 96000, 192000, 384000, 768000 };
    if (!_validateSampleRate(sampleRate, supported, 6)) {
        LOG_W("[HAL:ES9038PRO] Unsupported sample rate: %luHz", (unsigned long)sampleRate);
        return false;
    }
    if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32) {
        LOG_W("[HAL:ES9038PRO] Unsupported bit depth: %u", bitDepth);
        return false;
    }
    _sampleRate = sampleRate;
    _bitDepth   = bitDepth;
    LOG_I("[HAL:ES9038PRO] Configured: %luHz %ubit",
          (unsigned long)sampleRate, bitDepth);
    return true;
}

bool HalEs9038pro::setVolume(uint8_t percent) {
    if (!_initialized) return false;
    if (percent > 100) percent = 100;

    // Map 100% -> 0x00 (0 dB), 0% -> 0xFF (full attenuation)
    uint8_t volReg = (uint8_t)((100U - percent) * 255U / 100U);
    bool ok = _writeReg(ES9038PRO_REG_VOL_CH1, volReg);
    ok = ok && _writeReg(ES9038PRO_REG_VOL_CH2, volReg);
    ok = ok && _writeReg(ES9038PRO_REG_VOL_CH3, volReg);
    ok = ok && _writeReg(ES9038PRO_REG_VOL_CH4, volReg);
    ok = ok && _writeReg(ES9038PRO_REG_VOL_CH5, volReg);
    ok = ok && _writeReg(ES9038PRO_REG_VOL_CH6, volReg);
    ok = ok && _writeReg(ES9038PRO_REG_VOL_CH7, volReg);
    ok = ok && _writeReg(ES9038PRO_REG_VOL_CH8, volReg);

    _volume = percent;
    LOG_D("[HAL:ES9038PRO] Volume: %d%% -> reg=0x%02X", percent, volReg);
    return ok;
}

bool HalEs9038pro::setMute(bool mute) {
    if (!_initialized) return false;

    // Read-modify-write to preserve filter preset in bits[2:0]
    uint8_t reg = _readReg(ES9038PRO_REG_FILTER_MUTE);
    if (mute) {
        reg |=  ES9038PRO_MUTE_BIT;
    } else {
        reg &= (uint8_t)~ES9038PRO_MUTE_BIT;
    }
    bool ok = _writeReg(ES9038PRO_REG_FILTER_MUTE, reg);
    _muted = mute;
    LOG_I("[HAL:ES9038PRO] %s", mute ? "Muted" : "Unmuted");
    return ok;
}

bool HalEs9038pro::setFilterPreset(uint8_t preset) {
    if (preset >= ESS_SABRE_FILTER_COUNT) {
        LOG_W("[HAL:ES9038PRO] Invalid filter preset: %u (max %u)",
              preset, ESS_SABRE_FILTER_COUNT - 1);
        return false;
    }

    // Preserve mute bit (bit5) when updating filter (bits[2:0])
    uint8_t reg = _initialized ? _readReg(ES9038PRO_REG_FILTER_MUTE) : 0x00;
    reg &= (uint8_t)~ES9038PRO_FILTER_MASK;
    reg |= (uint8_t)(preset & ES9038PRO_FILTER_MASK);

    bool ok = true;
    if (_initialized) {
        ok = _writeReg(ES9038PRO_REG_FILTER_MUTE, reg);
    }
    _filterPreset = preset;
    LOG_I("[HAL:ES9038PRO] Filter preset: %u", preset);
    return ok;
}

// ===== Multi-sink interface =====

bool HalEs9038pro::buildSinkAt(int idx, uint8_t sinkSlot, AudioOutputSink* out) {
    if (!out) return false;
    if (idx < 0 || idx >= 4 || !_sinksBuilt) return false;
    *out = _sinks[idx];
    out->halSlot = _slot;
    (void)sinkSlot;  // firstChannel already set by _tdm.buildSinks()
    return true;
}

#endif // DAC_ENABLED
