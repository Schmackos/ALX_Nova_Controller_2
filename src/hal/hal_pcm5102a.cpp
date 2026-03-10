#ifdef DAC_ENABLED
// HalPcm5102a — PCM5102A passive I2S DAC implementation

#include "hal_pcm5102a.h"
#include "hal_device_manager.h"
#include "../i2s_audio.h"
#include "../audio_pipeline.h"

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

    LOG_I("[HAL:PCM5102A] Initializing (sr=%luHz bits=%u xsmt=%d)",
          (unsigned long)_sampleRate, _bitDepth, _paPin);

    // Configure XSMT (soft-mute) pin if provided
    if (_paPin >= 0) {
#ifndef NATIVE_TEST
        pinMode(_paPin, OUTPUT);
        digitalWrite(_paPin, _muted ? LOW : HIGH);
#endif
    }

    // Enable I2S TX full-duplex on port 0 (shared with PCM1808 ADC RX).
    // i2s_audio_enable_tx() tears down the RX-only channel and recreates
    // it as RX+TX — MCLK remains continuous for PCM1808 PLL stability.
    if (!i2s_audio_enable_tx(_sampleRate)) {
        LOG_E("[HAL:PCM5102A] I2S TX enable failed");
        return hal_init_fail(DIAG_HAL_INIT_FAILED, "I2S TX enable failed");
    }
    _i2sTxEnabled = true;

    _state = HAL_STATE_AVAILABLE;
    _ready = true;

    LOG_I("[HAL:PCM5102A] Ready (I2S TX enabled, sr=%luHz)", (unsigned long)_sampleRate);
    return hal_init_ok();
}

void HalPcm5102a::deinit() {
    // HC-3: ONLY disable TX. NEVER call i2s_channel_disable() on RX.
    // Use i2s_audio_disable_tx() which handles port-specific TX teardown.
    // RX and MCLK must remain active for PCM1808 ADC operation.

    if (_paPin >= 0) {
#ifndef NATIVE_TEST
        digitalWrite(_paPin, LOW);  // Mute output on deinit
#endif
    }

    // Disable I2S TX if this device enabled it
    if (_i2sTxEnabled) {
        i2s_audio_disable_tx();
        _i2sTxEnabled = false;
    }

    _ready = false;
    _state = HAL_STATE_REMOVED;
    _txHandle = nullptr;
    _muteRampState = 1.0f;  // HC-6: Reset mute ramp state on deactivation
    LOG_I("[HAL:PCM5102A] Deinitialized (TX disabled, RX/MCLK untouched)");
}

void HalPcm5102a::dumpConfig() {
    LOG_I("[HAL:PCM5102A] %s by %s (compat=%s) sr=%luHz bits=%u xsmt=%d vol=%d%% mute=%d",
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
        LOG_W("[HAL:PCM5102A] Unsupported sample rate: %luHz", (unsigned long)sampleRate);
        return false;
    }
    if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32) {
        LOG_W("[HAL:PCM5102A] Unsupported bit depth: %u", bitDepth);
        return false;
    }
    _sampleRate = sampleRate;
    _bitDepth   = bitDepth;
    // Reconfigure I2S if handle exists (managed via bridge)
    if (_txHandle) {
        hal_i2s_reconfigure(_txHandle, sampleRate, bitDepth);
    }
    LOG_I("[HAL:PCM5102A] Configured: %luHz %ubit", (unsigned long)sampleRate, bitDepth);
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

// ===== buildSink() — populate AudioOutputSink for pipeline registration =====
// Static device-pointer table indexed by sink slot. Required because the
// isReady callback signature is bool(*)(void) — no context parameter.
static HalPcm5102a* _pcm5102a_slot_dev[AUDIO_OUT_MAX_SINKS] = {};

// Write callback — converts float pipeline output to I2S int32 and writes to port 0.
// Volume and mute ramp are applied in float domain before int32 conversion.
static void _pcm5102a_write(const int32_t* buf, int stereoFrames) {
    if (!buf || stereoFrames <= 0) return;

    int totalSamples = stereoFrames * 2;

    // Look up the device from the static table to access mute ramp state
    // The slot is baked into each thunk — find which slot this write corresponds to
    // by checking which slot has a non-null device. For simplicity, iterate.
    // (The pipeline calls each sink's write directly, so only one device is active per call.)

#ifndef NATIVE_TEST
    // Convert int32 input to float, apply volume + mute ramp, convert back, write
    // Use stack buffer for small frames, heap for larger (DMA_BUF_LEN is typically 256)
    float fBuf[512];
    int32_t txBuf[512];
    const int32_t* src = buf;
    int remaining = totalSamples;

    while (remaining > 0) {
        int chunk = (remaining > 512) ? 512 : remaining;

        // int32 -> float [-1.0, +1.0]
        for (int i = 0; i < chunk; i++) {
            fBuf[i] = (float)src[i] / 2147483520.0f;
        }

        // Apply software volume (PCM5102A has no hardware volume)
        // Volume gain comes from the pipeline sink's volumeGain field
        // (set via audio_pipeline_set_sink_volume)
        // Note: sink_apply_volume skips if gain ~1.0
        float volGain = audio_pipeline_get_sink_volume(0);
        sink_apply_volume(fBuf, chunk, volGain);

        // Convert back to int32 for I2S DMA
        sink_float_to_i2s_int32(fBuf, txBuf, chunk);

        size_t bytesWritten = 0;
        i2s_audio_write(txBuf, (size_t)chunk * sizeof(int32_t), &bytesWritten, 20);

        src += chunk;
        remaining -= chunk;
    }
#else
    (void)totalSamples;
#endif
}

// isReady callback template for each slot — looks up device via static table
#define PCM5102A_READY_FN(N) \
    static bool _pcm5102a_ready_##N(void) { \
        return _pcm5102a_slot_dev[N] && _pcm5102a_slot_dev[N]->_ready; \
    }

PCM5102A_READY_FN(0)
PCM5102A_READY_FN(1)
PCM5102A_READY_FN(2)
PCM5102A_READY_FN(3)
PCM5102A_READY_FN(4)
PCM5102A_READY_FN(5)
PCM5102A_READY_FN(6)
PCM5102A_READY_FN(7)

static bool (*const _pcm5102a_ready_fn[AUDIO_OUT_MAX_SINKS])(void) = {
    _pcm5102a_ready_0, _pcm5102a_ready_1, _pcm5102a_ready_2, _pcm5102a_ready_3,
    _pcm5102a_ready_4, _pcm5102a_ready_5, _pcm5102a_ready_6, _pcm5102a_ready_7,
};

bool HalPcm5102a::buildSink(uint8_t sinkSlot, AudioOutputSink* out) {
    if (!out) return false;
    if (sinkSlot >= AUDIO_OUT_MAX_SINKS) return false;

    *out = AUDIO_OUTPUT_SINK_INIT;
    out->name         = _descriptor.name;
    out->firstChannel = (uint8_t)(sinkSlot * 2);
    out->channelCount = _descriptor.channelCount;
    out->halSlot      = _slot;
    out->write        = _pcm5102a_write;
    out->isReady      = _pcm5102a_ready_fn[sinkSlot];
    out->ctx          = this;

    // Register in static table for isReady callback lookup
    _pcm5102a_slot_dev[sinkSlot] = this;

    return true;
}

#endif // DAC_ENABLED
