#ifdef DAC_ENABLED
// HalEs8311 — ES8311 codec platform class implementation
// Extracts register init sequence from drivers/dac_es8311.cpp

#include "hal_es8311.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include <Wire.h>
#include <Arduino.h>
#include "../debug_serial.h"
#include "../drivers/es8311_regs.h"
#else
// Native test stubs — no hardware access
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)
// Stub register constants for native compilation (all hardware calls are #ifndef guarded)
#define ES8311_I2C_ADDR           0x18
#define ES8311_REG_RESET          0x00
#define ES8311_REG_CLK_MANAGER1   0x01
#define ES8311_REG_CLK_MANAGER2   0x02
#define ES8311_REG_CLK_MANAGER3   0x03
#define ES8311_REG_CLK_MANAGER4   0x04
#define ES8311_REG_CLK_MANAGER5   0x05
#define ES8311_REG_CLK_MANAGER6   0x06
#define ES8311_REG_CLK_MANAGER7   0x07
#define ES8311_REG_CLK_MANAGER8   0x08
#define ES8311_REG_SDPIN          0x09
#define ES8311_REG_SDPOUT         0x0A
#define ES8311_REG_SYSTEM1        0x0B
#define ES8311_REG_SYSTEM2        0x0C
#define ES8311_REG_SYSTEM3        0x0D
#define ES8311_REG_SYSTEM4        0x0E
#define ES8311_REG_SYSTEM5        0x10
#define ES8311_REG_SYSTEM6        0x11
#define ES8311_REG_SYSTEM7        0x12
#define ES8311_REG_SYSTEM8        0x13
#define ES8311_REG_SYSTEM9        0x14
#define ES8311_REG_ADC_MIC_GAIN   0x16
#define ES8311_REG_ADC_EQ_DC      0x1C
#define ES8311_REG_DAC_CTRL       0x31
#define ES8311_REG_DAC_VOLUME     0x32
#define ES8311_REG_DAC_RAMP       0x37
#define ES8311_REG_GPIO_CFG       0x44
#define ES8311_REG_GP_CTRL        0x45
#define ES8311_REG_CHIP_ID1       0xFD
#define ES8311_REG_CHIP_ID2       0xFE
#define ES8311_CSM_ON             0x80
#define ES8311_SDP_TRISTATE       0x40
#define ES8311_DAC_SOFT_MUTE      0x40
#define ES8311_DAC_MUTE           0x20
#define ES8311_WL_16BIT           0x0C
#define ES8311_WL_24BIT           0x00
#define ES8311_WL_32BIT           0x10
#define ES8311_FMT_I2S            0x00
#define ES8311_VOL_0DB            0xBF
// Clock coeff lookup stub
struct Es8311ClockCoeff {
    uint32_t mclk, sampleRate;
    uint8_t pre_div, pre_multi, adc_div, dac_div, fs_mode, lrck_h, lrck_l, bclk_div, adc_osr, dac_osr;
};
static const Es8311ClockCoeff* es8311_find_coeff(uint32_t, uint32_t) { return nullptr; }
#endif // NATIVE_TEST

HalEs8311::HalEs8311() : HalAudioDevice() {
    memset(&_descriptor, 0, sizeof(_descriptor));
    strncpy(_descriptor.compatible, "everest-semi,es8311", 31);
    strncpy(_descriptor.name, "ES8311", 32);
    strncpy(_descriptor.manufacturer, "Everest Semiconductor", 32);
    _descriptor.type = HAL_DEV_CODEC;
    _descriptor.legacyId = 0x0004;
    _descriptor.channelCount = 2;
    _descriptor.i2cAddr = 0x18;
    _descriptor.bus.type = HAL_BUS_I2C;
    _descriptor.bus.index = HAL_I2C_BUS_ONBOARD;
    _descriptor.sampleRatesMask = HAL_RATE_8K | HAL_RATE_16K | HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K;
    _descriptor.capabilities = HAL_CAP_CODEC | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE |
                                HAL_CAP_ADC_PATH | HAL_CAP_DAC_PATH;
    _initPriority = HAL_PRIORITY_HARDWARE;
}

// ===== I2C helpers =====

bool HalEs8311::_writeReg(uint8_t reg, uint8_t val) {
#ifndef NATIVE_TEST
    Wire.beginTransmission(_i2cAddr);
    Wire.write(reg);
    Wire.write(val);
    uint8_t err = Wire.endTransmission();
    if (err != 0) {
        LOG_E("[HAL:ES8311] I2C write failed: reg=0x%02X val=0x%02X err=%d", reg, val, err);
        return false;
    }
    return true;
#else
    (void)reg; (void)val;
    return true;
#endif
}

uint8_t HalEs8311::_readReg(uint8_t reg) {
#ifndef NATIVE_TEST
    Wire.beginTransmission(_i2cAddr);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(_i2cAddr, (uint8_t)1);
    if (Wire.available()) return Wire.read();
    LOG_E("[HAL:ES8311] I2C read failed: reg=0x%02X", reg);
    return 0xFF;
#else
    (void)reg;
    return 0x00;
#endif
}

// ===== Clock configuration (extracted from DacEs8311::initClocks) =====

void HalEs8311::_initClocks(uint32_t sampleRate) {
#ifndef NATIVE_TEST
    static const uint32_t ES8311_DEFAULT_MCLK = 12288000;
    const Es8311ClockCoeff* coeff = es8311_find_coeff(ES8311_DEFAULT_MCLK, sampleRate);

    if (!coeff) {
        coeff = es8311_find_coeff(ES8311_DEFAULT_MCLK, 48000);
        if (!coeff) {
            LOG_E("[HAL:ES8311] Fatal: no 48kHz clock coefficients found");
            return;
        }
        LOG_W("[HAL:ES8311] No coeff for %luHz, defaulting to 48kHz", (unsigned long)sampleRate);
    }

    LOG_I("[HAL:ES8311] Clock config: MCLK=%lu SR=%lu",
          (unsigned long)coeff->mclk, (unsigned long)coeff->sampleRate);

    _writeReg(ES8311_REG_CLK_MANAGER1, 0x3F);
    _writeReg(ES8311_REG_CLK_MANAGER2, (coeff->pre_multi << 5) | (coeff->pre_div & 0x1F));
    _writeReg(ES8311_REG_CLK_MANAGER3, ((coeff->fs_mode & 0x01) << 6) | (coeff->adc_osr & 0x3F));
    _writeReg(ES8311_REG_CLK_MANAGER4, coeff->dac_osr & 0x3F);
    _writeReg(ES8311_REG_CLK_MANAGER5, ((coeff->adc_div & 0x0F) << 4) | (coeff->dac_div & 0x0F));
    _writeReg(ES8311_REG_CLK_MANAGER6, coeff->bclk_div & 0x1F);
    _writeReg(ES8311_REG_CLK_MANAGER7, coeff->lrck_h);
    _writeReg(ES8311_REG_CLK_MANAGER8, coeff->lrck_l);
#else
    (void)sampleRate;
#endif
}

// ===== Power sequencing (extracted from DacEs8311::powerUp/powerDown) =====

void HalEs8311::_powerUp() {
    _writeReg(ES8311_REG_SYSTEM1, 0x00);
    _writeReg(ES8311_REG_SYSTEM2, 0x00);
    _writeReg(ES8311_REG_SYSTEM5, 0x1F);
    _writeReg(ES8311_REG_SYSTEM6, 0x7F);
    _writeReg(ES8311_REG_SYSTEM3, 0x10);
    _writeReg(ES8311_REG_SYSTEM7, 0x00);
    _writeReg(ES8311_REG_SYSTEM8, 0x10);
    _writeReg(ES8311_REG_SYSTEM4, 0x02);
}

void HalEs8311::_powerDown() {
    _writeReg(ES8311_REG_DAC_CTRL, ES8311_DAC_SOFT_MUTE | ES8311_DAC_MUTE);
#ifndef NATIVE_TEST
    delay(20);
#endif
    _writeReg(ES8311_REG_SYSTEM7, 0x02);
    _writeReg(ES8311_REG_SYSTEM3, 0x00);
    _writeReg(ES8311_REG_SYSTEM1, 0xFF);
    _writeReg(ES8311_REG_SYSTEM2, 0xFF);
}

// ===== HalDevice lifecycle =====

bool HalEs8311::probe() {
#ifndef NATIVE_TEST
    Wire.beginTransmission(_i2cAddr);
    uint8_t err = Wire.endTransmission();
    return err == 0;
#else
    return true;
#endif
}

HalInitResult HalEs8311::init() {
    // Read config from HAL Device Manager (may override defaults)
    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);
    if (cfg && cfg->valid) {
        if (cfg->i2cAddr != 0)      _i2cAddr    = cfg->i2cAddr;
        if (cfg->pinSda >= 0)       _sdaPin     = cfg->pinSda;
        if (cfg->pinScl >= 0)       _sclPin     = cfg->pinScl;
        if (cfg->paControlPin >= 0) _paPin      = cfg->paControlPin;
        if (cfg->sampleRate > 0)    _sampleRate = cfg->sampleRate;
        if (cfg->bitDepth > 0)      _bitDepth   = cfg->bitDepth;
        if (cfg->volume <= 100)     _volume     = cfg->volume;
        _muted = cfg->mute;
    }

    LOG_I("[HAL:ES8311] Initializing (I2C addr=0x%02X, SDA=%d, SCL=%d, PA=%d)",
          _i2cAddr, _sdaPin, _sclPin, _paPin);

#ifndef NATIVE_TEST
    // Configure PA control pin — keep disabled during init
    pinMode(_paPin, OUTPUT);
    digitalWrite(_paPin, LOW);

    // Initialize I2C on the onboard bus (Wire, not Wire1)
    Wire.begin(_sdaPin, _sclPin, 100000);
    LOG_I("[HAL:ES8311] I2C initialized (SDA=%d, SCL=%d, 100kHz)", _sdaPin, _sclPin);

    // I2C noise immunity — write GPIO_CFG twice (ES8311 datasheet recommendation)
    _writeReg(ES8311_REG_GPIO_CFG, 0x08);
    _writeReg(ES8311_REG_GPIO_CFG, 0x08);

    // Verify chip is present via ID registers
    uint8_t id1 = _readReg(ES8311_REG_CHIP_ID1);
    uint8_t id2 = _readReg(ES8311_REG_CHIP_ID2);
    LOG_I("[HAL:ES8311] Chip ID: 0x%02X 0x%02X (expected 0x83 0x11)", id1, id2);
    if (id1 != 0x83 || id2 != 0x11) {
        LOG_W("[HAL:ES8311] Unexpected chip ID — continuing anyway");
    }
#endif

    // Initial clock setup for default sample rate
    _initClocks(_sampleRate);

    // Power up system blocks
    _powerUp();

    // Reset codec state machine + set slave mode
    // CSM_ON=1 (bit7), MSC=0 (slave — I2S master is ESP32-P4)
    _writeReg(ES8311_REG_RESET, ES8311_CSM_ON);

    // Ensure all clocks enabled, MCLK from pin (bit7=0)
    _writeReg(ES8311_REG_CLK_MANAGER1, 0x3F);

    // Configure I2S input format: default 16-bit Philips
    _writeReg(ES8311_REG_SDPIN, ES8311_WL_16BIT | ES8311_FMT_I2S);

    // Tri-state ADC serial output (DAC-only mode on this board)
    uint8_t sdpout = _readReg(ES8311_REG_SDPOUT);
    _writeReg(ES8311_REG_SDPOUT, sdpout | ES8311_SDP_TRISTATE);

    // Start-phase config (from Espressif reference driver es8311_start)
    _writeReg(ES8311_REG_SYSTEM9, 0x1A);
    _writeReg(ES8311_REG_SYSTEM3, 0x01);
    _writeReg(ES8311_REG_GP_CTRL, 0x00);

    // DAC ramp rate (smooth transitions)
    _writeReg(ES8311_REG_DAC_RAMP, 0x08);

    // Internal reference configuration
    _writeReg(ES8311_REG_GPIO_CFG, 0x58);

    // Set initial volume to 0 dB (unity gain)
    _writeReg(ES8311_REG_DAC_VOLUME, ES8311_VOL_0DB);

    // Unmute DAC
    uint8_t dacCtrl = _readReg(ES8311_REG_DAC_CTRL);
    dacCtrl &= ~(ES8311_DAC_SOFT_MUTE | ES8311_DAC_MUTE);
    _writeReg(ES8311_REG_DAC_CTRL, dacCtrl);

#ifndef NATIVE_TEST
    // Small delay before enabling PA (prevent pop noise)
    delay(20);

    // Enable PA (NS4150B class-D amplifier)
    if (!_muted) {
        digitalWrite(_paPin, HIGH);
    }
#endif

    _initialized = true;
    _muted = false;
    _state = HAL_STATE_AVAILABLE;
    _ready = true;

    LOG_I("[HAL:ES8311] Initialization complete — PA enabled, DAC unmuted, vol=%d%%", _volume);
    return hal_init_ok();
}

void HalEs8311::deinit() {
    if (!_initialized) return;

    setMute(true);

#ifndef NATIVE_TEST
    delay(20);
    digitalWrite(_paPin, LOW);
#endif

    _powerDown();

#ifndef NATIVE_TEST
    Wire.end();
#endif

    _initialized = false;
    _ready = false;
    _state = HAL_STATE_REMOVED;

    LOG_I("[HAL:ES8311] Deinitialized");
}

void HalEs8311::dumpConfig() {
    LOG_I("[HAL:ES8311] %s by %s (compat=%s) i2c=0x%02X sda=%d scl=%d pa=%d vol=%d%% mute=%d sr=%luHz",
          _descriptor.name, _descriptor.manufacturer, _descriptor.compatible,
          _i2cAddr, _sdaPin, _sclPin, _paPin, _volume, _muted, (unsigned long)_sampleRate);
}

bool HalEs8311::healthCheck() {
    return probe();
}

// ===== HalAudioDevice =====

bool HalEs8311::configure(uint32_t sampleRate, uint8_t bitDepth) {
    if (!_initialized) return false;
    _initClocks(sampleRate);

    uint8_t wl;
    switch (bitDepth) {
        case 24: wl = ES8311_WL_24BIT; break;
        case 32: wl = ES8311_WL_32BIT; break;
        default: wl = ES8311_WL_16BIT; break;
    }
    _writeReg(ES8311_REG_SDPIN, wl | ES8311_FMT_I2S);

    _sampleRate = sampleRate;
    _bitDepth   = bitDepth;
    LOG_I("[HAL:ES8311] Configured: %luHz %ubit", (unsigned long)sampleRate, bitDepth);
    return true;
}

bool HalEs8311::setVolume(uint8_t percent) {
    if (!_initialized) return false;
    if (percent > 100) percent = 100;
    uint8_t regVal = (uint8_t)(((uint16_t)percent * ES8311_VOL_0DB) / 100);
    _writeReg(ES8311_REG_DAC_VOLUME, regVal);
    _volume = percent;
    LOG_D("[HAL:ES8311] Volume: %d%% -> reg 0x%02X", percent, regVal);
    return true;
}

bool HalEs8311::setMute(bool mute) {
    if (!_initialized) return false;
    uint8_t dacCtrl = _readReg(ES8311_REG_DAC_CTRL);
    if (mute) {
        dacCtrl |= (ES8311_DAC_SOFT_MUTE | ES8311_DAC_MUTE);
        _writeReg(ES8311_REG_DAC_CTRL, dacCtrl);
#ifndef NATIVE_TEST
        delay(10);
        digitalWrite(_paPin, LOW);
#endif
    } else {
#ifndef NATIVE_TEST
        digitalWrite(_paPin, HIGH);
        delay(10);
#endif
        dacCtrl &= ~(ES8311_DAC_SOFT_MUTE | ES8311_DAC_MUTE);
        _writeReg(ES8311_REG_DAC_CTRL, dacCtrl);
    }
    _muted = mute;
    LOG_I("[HAL:ES8311] %s", mute ? "Muted (PA disabled)" : "Unmuted (PA enabled)");
    return true;
}

// ===== HalAudioDacInterface =====

bool HalEs8311::dacSetSampleRate(uint32_t hz) {
    return configure(hz, _bitDepth);
}

bool HalEs8311::dacSetBitDepth(uint8_t bits) {
    return configure(_sampleRate, bits);
}

// ===== HalAudioAdcInterface =====

bool HalEs8311::adcSetGain(uint8_t gainDb) {
    if (!_initialized) return false;
    if (gainDb > 24) gainDb = 24;
    uint8_t steps = gainDb / 3;  // 3dB per step, 0-8 steps
    uint8_t regVal = (steps & 0x07) << 4;
    _writeReg(ES8311_REG_ADC_MIC_GAIN, regVal);
    LOG_I("[HAL:ES8311] ADC gain: %ddB -> reg 0x%02X", gainDb, regVal);
    return true;
}

bool HalEs8311::adcSetHpfEnabled(bool en) {
    if (!_initialized) return false;
    // ES8311 ADC EQ / DC removal register (0x1C): bit5 = HPF bypass
    // 0 = HPF active, 1 = bypassed
    uint8_t regVal = _readReg(ES8311_REG_ADC_EQ_DC);
    if (en) {
        regVal &= ~(1 << 5);  // Clear bypass -> HPF active
    } else {
        regVal |= (1 << 5);   // Set bypass -> HPF disabled
    }
    _writeReg(ES8311_REG_ADC_EQ_DC, regVal);
    LOG_I("[HAL:ES8311] ADC HPF: %s", en ? "enabled" : "disabled");
    return true;
}

// ===== HalAudioCodecInterface =====

bool HalEs8311::codecSetMclkMultiple(uint16_t mult) {
    _mclkMult = mult;
    if (_initialized) {
        _initClocks(_sampleRate);
    }
    return true;
}

bool HalEs8311::codecSetI2sFormat(uint8_t fmt) {
    _i2sFormat = fmt;
    if (!_initialized) return true;
    uint8_t fmtBits;
    switch (fmt) {
        case 1:  fmtBits = 0x01; break;  // LJ (MSB)
        case 2:  fmtBits = 0x02; break;  // RJ (LSB)
        default: fmtBits = ES8311_FMT_I2S; break;
    }
    uint8_t wl;
    switch (_bitDepth) {
        case 24: wl = ES8311_WL_24BIT; break;
        case 32: wl = ES8311_WL_32BIT; break;
        default: wl = ES8311_WL_16BIT; break;
    }
    _writeReg(ES8311_REG_SDPIN, wl | fmtBits);
    return true;
}

bool HalEs8311::codecSetPaEnabled(bool en) {
    if (_paPin < 0) return false;
#ifndef NATIVE_TEST
    digitalWrite(_paPin, en ? HIGH : LOW);
#endif
    LOG_I("[HAL:ES8311] PA %s (pin %d)", en ? "enabled" : "disabled", _paPin);
    return true;
}

#endif // DAC_ENABLED
