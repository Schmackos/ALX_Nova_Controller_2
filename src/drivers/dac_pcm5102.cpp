#ifdef DAC_ENABLED

#include "dac_pcm5102.h"

// PCM5102A supports these sample rates (auto-detected from I2S clocks)
static const uint32_t PCM5102_RATES[] = {
    8000, 16000, 32000, 44100, 48000, 88200, 96000, 176400, 192000
};
static const uint8_t PCM5102_NUM_RATES = sizeof(PCM5102_RATES) / sizeof(PCM5102_RATES[0]);

static const DacCapabilities PCM5102_CAPS = {
    "PCM5102A",              // name
    "Texas Instruments",     // manufacturer
    DAC_ID_PCM5102A,         // deviceId
    2,                       // maxChannels (stereo)
    false,                   // hasHardwareVolume
    false,                   // hasI2cControl
    false,                   // needsIndependentClock (uses shared I2S clocks)
    0x00,                    // i2cAddress (none)
    PCM5102_RATES,           // supportedRates
    PCM5102_NUM_RATES,       // numSupportedRates
    false,                   // hasFilterModes
    0                        // numFilterModes
};

const DacCapabilities& DacPcm5102::getCapabilities() const {
    return PCM5102_CAPS;
}

bool DacPcm5102::init(const DacPinConfig& pins) {
    (void)pins;
    // PCM5102A is I2S-only — no I2C init needed.
    // Hardware is ready as soon as I2S clocks are present.
    _initialized = true;
    return true;
}

void DacPcm5102::deinit() {
    _initialized = false;
    _configured = false;
    _sampleRate = 0;
    _bitDepth = 0;
}

bool DacPcm5102::configure(uint32_t sampleRate, uint8_t bitDepth) {
    if (!_initialized) return false;

    // Validate sample rate against supported list
    bool validRate = false;
    for (uint8_t i = 0; i < PCM5102_NUM_RATES; i++) {
        if (PCM5102_RATES[i] == sampleRate) {
            validRate = true;
            break;
        }
    }
    if (!validRate) return false;

    // PCM5102A supports 16/24/32-bit
    if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32) return false;

    _sampleRate = sampleRate;
    _bitDepth = bitDepth;
    _configured = true;
    return true;
}

bool DacPcm5102::setVolume(uint8_t volume) {
    (void)volume;
    // PCM5102A has no hardware volume — software volume handled by DAC manager
    return true;
}

bool DacPcm5102::setMute(bool mute) {
    (void)mute;
    // PCM5102A: mute handled by zeroing I2S output or software volume=0
    return true;
}

bool DacPcm5102::isReady() const {
    return _initialized && _configured;
}

DacDriver* createDacPcm5102() {
    return new DacPcm5102();
}

#endif // DAC_ENABLED
