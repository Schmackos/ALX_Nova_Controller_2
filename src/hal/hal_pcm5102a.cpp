#ifdef DAC_ENABLED
// HalPcm5102a — PCM5102A passive I2S DAC implementation

#include "hal_pcm5102a.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include <Arduino.h>
#include "../debug_serial.h"
#else
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
// Only define stubs if not already provided by Arduino mock
#ifndef OUTPUT
#define OUTPUT 1
#define LOW    0
#define HIGH   1
static void pinMode(int, int) {}
static void digitalWrite(int, int) {}
#endif
#endif

HalPcm5102a::HalPcm5102a() : HalAudioDevice() {
    memset(&_descriptor, 0, sizeof(_descriptor));
    strncpy(_descriptor.compatible, "ti,pcm5102a", 31);
    strncpy(_descriptor.name, "PCM5102A", 32);
    strncpy(_descriptor.manufacturer, "Texas Instruments", 32);
    _descriptor.type = HAL_DEV_DAC;
    _descriptor.legacyId = 0x0001;
    _descriptor.channelCount = 2;
    _descriptor.bus.type = HAL_BUS_I2S;
    _descriptor.bus.index = 0;
    _descriptor.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K;
    // PCM5102A has no hardware volume control via I2C — HAL_CAP_HW_VOLUME NOT set
    _descriptor.capabilities = HAL_CAP_DAC_PATH | HAL_CAP_MUTE;
    _initPriority = HAL_PRIORITY_HARDWARE;
}

bool HalPcm5102a::probe() {
    // PCM5102A is I2S-only passive — always present if physically connected
    return true;
}

HalInitResult HalPcm5102a::init() {
    // Read config from HAL Device Manager
    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);
    if (cfg && cfg->valid) {
        if (cfg->paControlPin >= 0) _paPin      = cfg->paControlPin;
        if (cfg->sampleRate > 0)    _sampleRate = cfg->sampleRate;
        if (cfg->bitDepth > 0)      _bitDepth   = cfg->bitDepth;
        _volume = cfg->volume;
        _muted  = cfg->mute;
    }

    LOG_I("[HalPcm5102a] Initializing (sr=%luHz bits=%u xsmt=%d)",
          (unsigned long)_sampleRate, _bitDepth, _paPin);

    // Configure XSMT (soft-mute) pin if provided
    if (_paPin >= 0) {
#ifndef NATIVE_TEST
        pinMode(_paPin, OUTPUT);
        digitalWrite(_paPin, _muted ? LOW : HIGH);
#endif
    }

    // NOTE: I2S TX channel is owned by i2s_audio.cpp (full-duplex shared with PCM1808 ADC).
    // HalPcm5102a only manages device-level state; I2S setup stays in the legacy path.

    _state = HAL_STATE_AVAILABLE;
    _ready = true;

    LOG_I("[HalPcm5102a] Ready (I2S channel managed by legacy i2s_audio path)");
    return hal_init_ok();
}

void HalPcm5102a::deinit() {
    if (_paPin >= 0) {
#ifndef NATIVE_TEST
        digitalWrite(_paPin, LOW);  // Mute output on deinit
#endif
    }
    _ready = false;
    _state = HAL_STATE_REMOVED;
    _txHandle = nullptr;
    LOG_I("[HalPcm5102a] Deinitialized");
}

void HalPcm5102a::dumpConfig() {
    LOG_I("[HalPcm5102a] %s by %s (compat=%s) sr=%luHz bits=%u xsmt=%d vol=%d%% mute=%d",
          _descriptor.name, _descriptor.manufacturer, _descriptor.compatible,
          (unsigned long)_sampleRate, _bitDepth, _paPin, _volume, _muted);
}

bool HalPcm5102a::healthCheck() {
    // Passive device — no I2C to ping. Report ready if state is AVAILABLE.
    return _ready;
}

bool HalPcm5102a::configure(uint32_t sampleRate, uint8_t bitDepth) {
    // Validate sample rate against sampleRatesMask
    bool validRate = (sampleRate == 44100 || sampleRate == 48000 || sampleRate == 96000);
    if (!validRate) {
        LOG_W("[HalPcm5102a] Unsupported sample rate: %luHz", (unsigned long)sampleRate);
        return false;
    }
    if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32) {
        LOG_W("[HalPcm5102a] Unsupported bit depth: %u", bitDepth);
        return false;
    }
    _sampleRate = sampleRate;
    _bitDepth   = bitDepth;
    // Reconfigure I2S if handle exists (managed via bridge)
    if (_txHandle) {
        hal_i2s_reconfigure(_txHandle, sampleRate, bitDepth);
    }
    LOG_I("[HalPcm5102a] Configured: %luHz %ubit", (unsigned long)sampleRate, bitDepth);
    return true;
}

bool HalPcm5102a::setVolume(uint8_t percent) {
    // PCM5102A has no hardware I2C volume control
    // Volume is handled by software gain in the audio pipeline
    _volume = percent;
    return false;  // false = no hardware volume applied
}

bool HalPcm5102a::setMute(bool mute) {
    _muted = mute;
    if (_paPin >= 0) {
        // XSMT pin: HIGH = play, LOW = soft mute
#ifndef NATIVE_TEST
        digitalWrite(_paPin, mute ? LOW : HIGH);
#endif
        return true;
    }
    // No XSMT pin — muting handled by zeroing I2S output in pipeline
    return false;
}

bool HalPcm5102a::dacSetSampleRate(uint32_t hz) {
    return configure(hz, _bitDepth);
}

bool HalPcm5102a::dacSetBitDepth(uint8_t bits) {
    return configure(_sampleRate, bits);
}

#endif // DAC_ENABLED
