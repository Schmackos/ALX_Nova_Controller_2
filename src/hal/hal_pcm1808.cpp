#ifdef DAC_ENABLED
// HalPcm1808 — PCM1808 ADC implementation

#include "hal_pcm1808.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#include "../i2s_audio.h"
#include <Arduino.h>
#else
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#endif

HalPcm1808::HalPcm1808() : HalAudioDevice() {
    memset(&_descriptor, 0, sizeof(_descriptor));
    strncpy(_descriptor.compatible, "ti,pcm1808", 31);
    strncpy(_descriptor.name, "PCM1808", 32);
    strncpy(_descriptor.manufacturer, "Texas Instruments", 32);
    _descriptor.type = HAL_DEV_ADC;
    _descriptor.legacyId = 0;
    _descriptor.channelCount = 2;
    _descriptor.bus.type = HAL_BUS_I2S;
    _descriptor.bus.index = 0;
    _descriptor.sampleRatesMask = HAL_RATE_48K | HAL_RATE_96K;
    _descriptor.capabilities = HAL_CAP_ADC_PATH;
    _initPriority = HAL_PRIORITY_HARDWARE;
}

bool HalPcm1808::probe() {
    // PCM1808 is I2S-only passive — always true
    return true;
}

HalInitResult HalPcm1808::init() {
    // Read config from HAL Device Manager
    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);
    if (cfg && cfg->valid) {
        if (cfg->sampleRate > 0) _sampleRate = cfg->sampleRate;
        if (cfg->bitDepth > 0)   _bitDepth   = cfg->bitDepth;
    }

    LOG_I("[HAL:PCM1808] Initializing (sr=%luHz bits=%u)",
          (unsigned long)_sampleRate, _bitDepth);

#ifndef NATIVE_TEST
    if (cfg && cfg->valid) {
        // Apply FMT pin: controls serial data format (LOW=Philips/I2S, HIGH=MSB/left-justified)
        if (cfg->pinFmt >= 0) {
            bool msb = (cfg->i2sFormat == 1);
            pinMode(cfg->pinFmt, OUTPUT);
            digitalWrite(cfg->pinFmt, msb ? HIGH : LOW);
            LOG_I("[HAL:PCM1808] FMT pin GPIO%d set %s", cfg->pinFmt, msb ? "HIGH (MSB)" : "LOW (Philips)");
        }
        // Log configured clock pins (managed by i2s_audio.cpp legacy path)
        if (cfg->pinBck >= 0 || cfg->pinLrc >= 0) {
            LOG_I("[HAL:PCM1808] I2S clock pins: BCK=GPIO%d LRC=GPIO%d (managed by i2s_audio)",
                  cfg->pinBck, cfg->pinLrc);
        }
    }
#endif

    // I2S channel setup is handled by i2s_audio_configure_adc() called from Core 1
    // (audio_pipeline_task). HalPcm1808 provides device config for pin/port overrides;
    // the bridge registers the AudioInputSource when this device transitions to AVAILABLE.

    // Initialize AudioInputSource with port-indexed callbacks
    memset(&_inputSrc, 0, sizeof(_inputSrc));
    _inputSrc.name          = _descriptor.name;   // from HAL descriptor
    _inputSrc.isHardwareAdc = true;
    _inputSrc.gainLinear    = 1.0f;
    _inputSrc.vuL           = -90.0f;
    _inputSrc.vuR           = -90.0f;

    uint8_t port = (cfg && cfg->valid && cfg->i2sPort != 255) ? cfg->i2sPort : 0;
#ifndef NATIVE_TEST
    // Port-indexed thunks will be defined in i2s_audio.h/cpp
    _inputSrc.read          = (port == 0) ? i2s_audio_port0_read : i2s_audio_port1_read;
    _inputSrc.isActive      = (port == 0) ? i2s_audio_port0_active : i2s_audio_port1_active;
    _inputSrc.getSampleRate = i2s_audio_get_sample_rate;
#endif
    _inputSrcReady = true;

    _state = HAL_STATE_AVAILABLE;
    _ready = true;

    LOG_I("[HAL:PCM1808] Ready (I2S via i2s_audio_configure_adc, instance %u)",
          _descriptor.instanceId);
    return hal_init_ok();
}

void HalPcm1808::deinit() {
    _ready = false;
    _state = HAL_STATE_REMOVED;
    _rxHandle = nullptr;
    LOG_I("[HAL:PCM1808] Deinitialized");
}

void HalPcm1808::dumpConfig() {
    LOG_I("[HAL:PCM1808] %s by %s (compat=%s) sr=%luHz bits=%u",
          _descriptor.name, _descriptor.manufacturer, _descriptor.compatible,
          (unsigned long)_sampleRate, _bitDepth);
}

bool HalPcm1808::healthCheck() {
    return _ready;
}

bool HalPcm1808::configure(uint32_t sampleRate, uint8_t bitDepth) {
    bool validRate = (sampleRate == 48000 || sampleRate == 96000);
    if (!validRate) {
        LOG_W("[HAL:PCM1808] Unsupported sample rate: %luHz", (unsigned long)sampleRate);
        return false;
    }
    _sampleRate = sampleRate;
    _bitDepth   = bitDepth;
    return true;
}

bool HalPcm1808::adcSetGain(uint8_t gainDb) {
    (void)gainDb;
    // PCM1808 gain is set via FMT0/FMT1 GPIO mode pins, not register writes.
    // Provide this control via DSP gain stage instead.
    LOG_W("[HAL:PCM1808] adcSetGain: PCM1808 gain via GPIO not yet wired — adjust DSP gain instead");
    return false;
}

bool HalPcm1808::adcSetHpfEnabled(bool en) {
    (void)en;
    // PCM1808 HPF is always on in master mode; no register to control it.
    LOG_W("[HAL:PCM1808] adcSetHpfEnabled: PCM1808 HPF not controllable via software");
    return false;
}

bool HalPcm1808::adcSetSampleRate(uint32_t hz) {
    return configure(hz, _bitDepth);
}

const AudioInputSource* HalPcm1808::getInputSource() const {
    return _inputSrcReady ? &_inputSrc : nullptr;
}

#endif // DAC_ENABLED
