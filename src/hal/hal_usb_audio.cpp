#ifdef DAC_ENABLED

#include "hal_usb_audio.h"
#include "hal_device_manager.h"

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#include "../usb_audio.h"
#include "../app_state.h"
#else
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
// Stubs for native test
inline bool usb_audio_is_streaming() { return false; }
inline bool usb_audio_is_connected() { return false; }
inline uint32_t usb_audio_read(int32_t*, uint32_t) { return 0; }
inline uint32_t usb_audio_get_negotiated_rate() { return 48000; }
inline float usb_audio_get_volume_linear() { return 1.0f; }
inline bool usb_audio_get_mute() { return false; }
#endif

// ===== Static callbacks for AudioInputSource =====

static uint32_t _usb_read(int32_t *dst, uint32_t frames) {
    uint32_t got = usb_audio_read(dst, frames);
    // Apply host volume/mute inline — pipeline gainLinear stays 1.0
    float vol = usb_audio_get_mute() ? 0.0f : usb_audio_get_volume_linear();
    if (vol != 1.0f && got > 0) {
        for (uint32_t i = 0; i < got * 2; i++) {
            dst[i] = (int32_t)((float)dst[i] * vol);
        }
    }
    return got;
}

static bool _usb_isActive(void) {
    return usb_audio_is_streaming();
}

static uint32_t _usb_getSampleRate(void) {
    return usb_audio_get_negotiated_rate();
}

// ===== HalUsbAudio implementation =====

HalUsbAudio::HalUsbAudio() : HalDevice() {
    memset(&_descriptor, 0, sizeof(_descriptor));
    strncpy(_descriptor.compatible, "alx,usb-audio", 31);
    strncpy(_descriptor.name, "USB Audio", 32);
    strncpy(_descriptor.manufacturer, "ALX", 32);
    _descriptor.type = HAL_DEV_ADC;
    _descriptor.legacyId = 0;
    _descriptor.channelCount = 2;
    _descriptor.bus.type = HAL_BUS_INTERNAL;
    _descriptor.bus.index = 0;
    _descriptor.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K;
    _descriptor.capabilities = HAL_CAP_ADC_PATH;
    _initPriority = HAL_PRIORITY_DATA;

    // Initialize AudioInputSource
    memset(&_source, 0, sizeof(_source));
    _source.name = "USB Audio";
    _source.lane = 0;           // Set by bridge before registration
    _source.read = _usb_read;
    _source.isActive = _usb_isActive;
    _source.getSampleRate = _usb_getSampleRate;
    _source.gainLinear = 1.0f;  // Volume handled inside read callback
    _source.vuL = -90.0f;
    _source.vuR = -90.0f;
    _source._vuSmoothedL = 0.0f;
    _source._vuSmoothedR = 0.0f;
    _source.halSlot = 0xFF;     // Set by bridge
}

bool HalUsbAudio::probe() {
    // Software device — always available
    return true;
}

HalInitResult HalUsbAudio::init() {
    _state = HAL_STATE_AVAILABLE;
    _ready = true;
    LOG_I("[HAL:USB Audio] Initialized (instance %u, priority %u)",
          _descriptor.instanceId, _initPriority);
    return hal_init_ok();
}

void HalUsbAudio::deinit() {
    _ready = false;
    _state = HAL_STATE_REMOVED;
    LOG_I("[HAL:USB Audio] Deinitialized");
}

void HalUsbAudio::dumpConfig() {
    LOG_I("[HAL:USB Audio] %s (compat=%s) bus=INTERNAL cap=ADC_PATH",
          _descriptor.name, _descriptor.compatible);
}

bool HalUsbAudio::healthCheck() {
    // Always healthy — USB connection state doesn't affect device health
    // (disconnection = no data, not hardware failure)
    return true;
}

const AudioInputSource* HalUsbAudio::getInputSource() const {
    return &_source;
}

#endif // DAC_ENABLED
