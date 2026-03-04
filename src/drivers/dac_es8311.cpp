#ifdef DAC_ENABLED
#ifndef NATIVE_TEST
#include <sdkconfig.h>
#endif
#if CONFIG_IDF_TARGET_ESP32P4

#include "dac_es8311.h"
#include "es8311_regs.h"
#include <Wire.h>
#include <Arduino.h>

#ifndef UNIT_TEST
#include "../debug_serial.h"
#else
// Stub logging for native tests
#define LOG_I(fmt, ...)
#define LOG_W(fmt, ...)
#define LOG_E(fmt, ...)
#define LOG_D(fmt, ...)
#endif

// ===== Supported Sample Rates =====
static const uint32_t ES8311_RATES[] = {
    8000, 11025, 16000, 22050, 32000, 44100, 48000, 96000
};
static const uint8_t ES8311_NUM_RATES = sizeof(ES8311_RATES) / sizeof(ES8311_RATES[0]);

// ===== Capabilities =====
static const DacCapabilities ES8311_CAPS = {
    "ES8311",                // name
    "Everest Semiconductor", // manufacturer
    DAC_ID_ES8311,           // deviceId
    2,                       // maxChannels (stereo I2S, mono DAC internally)
    true,                    // hasHardwareVolume
    true,                    // hasI2cControl
    false,                   // needsIndependentClock (uses I2S2 MCLK from P4)
    ES8311_I2C_ADDR,         // i2cAddress = 0x18
    ES8311_RATES,            // supportedRates
    ES8311_NUM_RATES,        // numSupportedRates
    false,                   // hasFilterModes
    0                        // numFilterModes
};

// Default MCLK frequency — ESP32-P4 I2S2 provides 12.288 MHz
static const uint32_t ES8311_DEFAULT_MCLK = 12288000;

// ===== I2C Helpers =====

bool DacEs8311::writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(ES8311_I2C_ADDR);
    Wire.write(reg);
    Wire.write(val);
    uint8_t err = Wire.endTransmission();
    if (err != 0) {
        LOG_E("[ES8311] I2C write failed: reg=0x%02X val=0x%02X err=%d", reg, val, err);
        return false;
    }
    return true;
}

uint8_t DacEs8311::readReg(uint8_t reg) {
    Wire.beginTransmission(ES8311_I2C_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);  // Repeated start
    Wire.requestFrom((uint8_t)ES8311_I2C_ADDR, (uint8_t)1);
    if (Wire.available()) {
        return Wire.read();
    }
    LOG_E("[ES8311] I2C read failed: reg=0x%02X", reg);
    return 0xFF;
}

bool DacEs8311::verifyChipId() {
    uint8_t id1 = readReg(ES8311_REG_CHIP_ID1);
    uint8_t id2 = readReg(ES8311_REG_CHIP_ID2);
    uint8_t ver = readReg(ES8311_REG_CHIP_VER);

    LOG_I("[ES8311] Chip ID: 0x%02X 0x%02X, version: 0x%02X", id1, id2, ver);

    // ES8311 returns 0x83 0x11 for chip ID
    if (id1 == 0x83 && id2 == 0x11) {
        return true;
    }

    LOG_W("[ES8311] Unexpected chip ID (expected 0x83 0x11, got 0x%02X 0x%02X)", id1, id2);
    // Continue anyway — some revisions may have different IDs but still work
    return true;
}

// ===== Clock Configuration =====

void DacEs8311::initClocks(uint32_t sampleRate) {
    const Es8311ClockCoeff* coeff = es8311_find_coeff(ES8311_DEFAULT_MCLK, sampleRate);

    if (!coeff) {
        // Fallback: try to find any matching sample rate
        for (uint8_t i = 0; i < ES8311_COEFF_COUNT; i++) {
            if (ES8311_COEFF_TABLE[i].sampleRate == sampleRate) {
                coeff = &ES8311_COEFF_TABLE[i];
                break;
            }
        }
    }

    if (!coeff) {
        LOG_W("[ES8311] No clock coefficients for %lu Hz, defaulting to 48kHz", (unsigned long)sampleRate);
        coeff = es8311_find_coeff(ES8311_DEFAULT_MCLK, 48000);
        if (!coeff) {
            LOG_E("[ES8311] Fatal: no 48kHz coefficients found");
            return;
        }
    }

    LOG_I("[ES8311] Configuring clocks: MCLK=%lu, SR=%lu",
          (unsigned long)coeff->mclk, (unsigned long)coeff->sampleRate);

    // CLK_MANAGER1: MCLK from pin (bit7=0), enable all clocks (bits5:0=0x3F)
    writeReg(ES8311_REG_CLK_MANAGER1, 0x3F);

    // CLK_MANAGER2: pre_multi (bits7:5) | pre_div (bits4:0)
    writeReg(ES8311_REG_CLK_MANAGER2, (coeff->pre_multi << 5) | (coeff->pre_div & 0x1F));

    // CLK_MANAGER3: fs_mode (bit6) | adc_osr (bits5:0)
    writeReg(ES8311_REG_CLK_MANAGER3, ((coeff->fs_mode & 0x01) << 6) | (coeff->adc_osr & 0x3F));

    // CLK_MANAGER4: dac_osr (bits5:0)
    writeReg(ES8311_REG_CLK_MANAGER4, coeff->dac_osr & 0x3F);

    // CLK_MANAGER5: adc_div (bits7:4) | dac_div (bits3:0)
    writeReg(ES8311_REG_CLK_MANAGER5, ((coeff->adc_div & 0x0F) << 4) | (coeff->dac_div & 0x0F));

    // CLK_MANAGER6: SCLK_INV=0 (bit5), BCLK_DIV (bits4:0)
    writeReg(ES8311_REG_CLK_MANAGER6, coeff->bclk_div & 0x1F);

    // CLK_MANAGER7/8: LRCK divider (high/low bytes)
    writeReg(ES8311_REG_CLK_MANAGER7, coeff->lrck_h);
    writeReg(ES8311_REG_CLK_MANAGER8, coeff->lrck_l);
}

// ===== Power Sequencing =====

void DacEs8311::powerUp() {
    // System power registers — bring up analog blocks
    writeReg(ES8311_REG_SYSTEM1, 0x00);   // Power on analog 1
    writeReg(ES8311_REG_SYSTEM2, 0x00);   // Power on analog 2

    // Digital reference
    writeReg(ES8311_REG_SYSTEM5, 0x10);   // Digital reference 1
    writeReg(ES8311_REG_SYSTEM6, 0x00);   // Digital reference 2

    // Analog reference: VMIDSEL=01 (normal mode), enable reference
    writeReg(ES8311_REG_SYSTEM3, 0x10);

    // Power up DAC: PDN_DAC=0, ENREFR=0
    writeReg(ES8311_REG_SYSTEM7, 0x00);

    // Headphone switch / analog reference
    writeReg(ES8311_REG_SYSTEM8, 0x10);

    // Power down PGA and ADC modulator (we only use DAC)
    writeReg(ES8311_REG_SYSTEM4, 0x0A);   // PDN_PGA=1, PDN_MOD_ADC=1, PDN_MOD_DAC=0
}

void DacEs8311::powerDown() {
    // Mute DAC first
    writeReg(ES8311_REG_DAC_CTRL, ES8311_DAC_SOFT_MUTE | ES8311_DAC_MUTE);
    delay(20);

    // Power down DAC
    writeReg(ES8311_REG_SYSTEM7, 0x02);   // PDN_DAC=1

    // Power down analog reference
    writeReg(ES8311_REG_SYSTEM3, 0x00);

    // Power down all system blocks
    writeReg(ES8311_REG_SYSTEM1, 0xFF);
    writeReg(ES8311_REG_SYSTEM2, 0xFF);
}

// ===== DacDriver Interface =====

const DacCapabilities& DacEs8311::getCapabilities() const {
    return ES8311_CAPS;
}

bool DacEs8311::init(const DacPinConfig& pins) {
    (void)pins;  // ES8311 uses dedicated onboard I2C, not the configurable DAC I2C pins

    LOG_I("[ES8311] Initializing ES8311 DAC driver");

    // Configure PA control pin (NS4150B class-D amplifier)
    pinMode(ES8311_PA_PIN, OUTPUT);
    digitalWrite(ES8311_PA_PIN, LOW);  // Keep amp disabled during init

    // Initialize I2C on the onboard bus (GPIO 7=SDA, GPIO 8=SCL)
    // Use Wire (not Wire1 — Wire1 is reserved for external DAC I2C on GPIO 48/54)
    Wire.begin(ES8311_I2C_SDA_PIN, ES8311_I2C_SCL_PIN, 100000);

    // I2C noise immunity: write to GPIO config register twice (ES8311 datasheet recommendation)
    writeReg(ES8311_REG_GPIO_CFG, 0x08);
    writeReg(ES8311_REG_GPIO_CFG, 0x08);

    // Verify chip is present
    if (!verifyChipId()) {
        LOG_E("[ES8311] Chip ID verification failed — aborting init");
        Wire.end();
        return false;
    }

    // Initial clock setup (default 48kHz, reconfigured in configure())
    initClocks(48000);

    // Power up system blocks
    powerUp();

    // Reset codec state machine + set slave mode
    // CSM_ON=1 (bit7), MSC=0 (slave mode — P4 I2S2 is master)
    writeReg(ES8311_REG_RESET, ES8311_CSM_ON);

    // Ensure all clocks enabled, MCLK from pin (bit7=0)
    writeReg(ES8311_REG_CLK_MANAGER1, 0x3F);

    // Configure I2S input format: 16-bit I2S standard
    // bits4:2 = WL_16BIT (0x0C), bits1:0 = FMT_I2S (0x00)
    writeReg(ES8311_REG_SDPIN, ES8311_WL_16BIT | ES8311_FMT_I2S);

    // Tri-state ADC serial output (not used in DAC-only mode)
    uint8_t sdpout = readReg(ES8311_REG_SDPOUT);
    writeReg(ES8311_REG_SDPOUT, sdpout | ES8311_SDP_TRISTATE);

    // DAC ramp rate (smooth transitions)
    writeReg(ES8311_REG_DAC_RAMP, 0x08);

    // Internal reference configuration
    writeReg(ES8311_REG_GPIO_CFG, 0x58);

    // Set initial volume to 0 dB (unity gain)
    writeReg(ES8311_REG_DAC_VOLUME, ES8311_VOL_0DB);

    // Unmute DAC: clear SOFT_MUTE and DAC_MUTE bits
    uint8_t dacCtrl = readReg(ES8311_REG_DAC_CTRL);
    dacCtrl &= ~(ES8311_DAC_SOFT_MUTE | ES8311_DAC_MUTE);
    writeReg(ES8311_REG_DAC_CTRL, dacCtrl);

    // Small delay before enabling PA to prevent pop noise
    delay(20);

    // Enable PA (NS4150B class-D amplifier)
    digitalWrite(ES8311_PA_PIN, HIGH);
    _muted = false;

    _initialized = true;
    LOG_I("[ES8311] Initialization complete — PA enabled, DAC unmuted, volume 0dB");
    return true;
}

void DacEs8311::deinit() {
    if (!_initialized) return;

    LOG_I("[ES8311] Deinitializing ES8311 DAC driver");

    // Mute first to prevent pop
    setMute(true);
    delay(20);

    // Disable PA before powering down codec
    digitalWrite(ES8311_PA_PIN, LOW);

    // Power down DAC and analog blocks
    powerDown();

    // End I2C
    Wire.end();

    _initialized = false;
    _configured = false;
    _sampleRate = 0;
    _bitDepth = 0;
    _muted = false;

    LOG_I("[ES8311] Deinitialized");
}

bool DacEs8311::configure(uint32_t sampleRate, uint8_t bitDepth) {
    if (!_initialized) return false;

    // Validate sample rate against supported list
    bool validRate = false;
    for (uint8_t i = 0; i < ES8311_NUM_RATES; i++) {
        if (ES8311_RATES[i] == sampleRate) {
            validRate = true;
            break;
        }
    }
    if (!validRate) {
        LOG_W("[ES8311] Unsupported sample rate: %lu Hz", (unsigned long)sampleRate);
        return false;
    }

    // ES8311 supports 16/24/32-bit word lengths
    if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32) {
        LOG_W("[ES8311] Unsupported bit depth: %d", bitDepth);
        return false;
    }

    // Reconfigure clocks for the target sample rate
    initClocks(sampleRate);

    // Update I2S word length setting
    uint8_t wl;
    switch (bitDepth) {
        case 16: wl = ES8311_WL_16BIT; break;
        case 24: wl = ES8311_WL_24BIT; break;
        case 32: wl = ES8311_WL_32BIT; break;
        default: wl = ES8311_WL_16BIT; break;
    }
    writeReg(ES8311_REG_SDPIN, wl | ES8311_FMT_I2S);

    _sampleRate = sampleRate;
    _bitDepth = bitDepth;
    _configured = true;

    LOG_I("[ES8311] Configured: %lu Hz, %d-bit", (unsigned long)sampleRate, bitDepth);
    return true;
}

bool DacEs8311::setVolume(uint8_t volume) {
    if (!_initialized) return false;

    // Clamp to 0-100 range
    if (volume > 100) volume = 100;

    // Map 0-100% to 0x00 - 0xBF (ES8311 -95.5dB to 0dB)
    // Don't go above 0 dB for safety (0xC0-0xFF is +0.5 to +32 dB territory)
    uint8_t regVal = (uint8_t)(((uint16_t)volume * ES8311_VOL_0DB) / 100);

    writeReg(ES8311_REG_DAC_VOLUME, regVal);

    LOG_D("[ES8311] Volume set: %d%% -> reg 0x%02X (%.1f dB)",
          volume, regVal, (float)(regVal - ES8311_VOL_0DB) * 0.5f);
    return true;
}

bool DacEs8311::setMute(bool mute) {
    if (!_initialized) return false;

    uint8_t dacCtrl = readReg(ES8311_REG_DAC_CTRL);

    if (mute) {
        // Mute: set both soft mute and hard mute bits
        dacCtrl |= (ES8311_DAC_SOFT_MUTE | ES8311_DAC_MUTE);
        writeReg(ES8311_REG_DAC_CTRL, dacCtrl);
        delay(10);  // Wait for soft mute ramp down
        // Disable PA (NS4150B) to save power and eliminate noise
        digitalWrite(ES8311_PA_PIN, LOW);
    } else {
        // Unmute: enable PA first, then unmute DAC
        digitalWrite(ES8311_PA_PIN, HIGH);
        delay(10);  // Let PA settle
        // Clear mute bits
        dacCtrl &= ~(ES8311_DAC_SOFT_MUTE | ES8311_DAC_MUTE);
        writeReg(ES8311_REG_DAC_CTRL, dacCtrl);
    }

    _muted = mute;
    LOG_I("[ES8311] %s", mute ? "Muted (PA disabled)" : "Unmuted (PA enabled)");
    return true;
}

bool DacEs8311::isReady() const {
    return _initialized && _configured;
}

// ===== Factory =====

DacDriver* createDacEs8311() {
    return new DacEs8311();
}

#endif // CONFIG_IDF_TARGET_ESP32P4
#endif // DAC_ENABLED
