#ifdef DAC_ENABLED
// HalDspBridge — bridges HAL device interface to the dsp_pipeline module

#include "hal_dsp_bridge.h"

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#include "../app_state.h"
#include "../i2s_audio.h"
#else
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#endif

HalDspBridge::HalDspBridge() : HalDevice() {
    memset(&_descriptor, 0, sizeof(_descriptor));
    strncpy(_descriptor.compatible, "alx,dsp-pipeline", 31);
    strncpy(_descriptor.name, "DSP Pipeline", 32);
    strncpy(_descriptor.manufacturer, "ALX Audio", 32);
    _descriptor.type = HAL_DEV_DSP;
    _descriptor.channelCount = 4;
    _descriptor.bus.type = HAL_BUS_INTERNAL;
    _descriptor.capabilities = 0;
    _initPriority = HAL_PRIORITY_DATA;
}

bool HalDspBridge::probe() {
    return true;  // DSP pipeline is always present (software module)
}

HalInitResult HalDspBridge::init() {
    // dsp_pipeline is already initialised by audio_pipeline_init()
    // Just mark ourselves available
    _state = HAL_STATE_AVAILABLE;
    _ready = true;
    LOG_I("[HalDspBridge] DSP pipeline bridge ready");
    return hal_init_ok();
}

void HalDspBridge::deinit() {
    _ready = false;
    _state = HAL_STATE_REMOVED;
}

void HalDspBridge::dumpConfig() {
    LOG_I("[HalDspBridge] DSP Pipeline bridge — dspEnabled=%d",
#ifndef NATIVE_TEST
          AppState::getInstance().dspEnabled
#else
          0
#endif
    );
}

bool HalDspBridge::healthCheck() {
    return true;
}

bool HalDspBridge::dspIsActive() const {
#ifndef NATIVE_TEST
    return AppState::getInstance().dspEnabled;
#else
    return false;
#endif
}

bool HalDspBridge::dspSetBypassed(bool bypass) {
#ifndef NATIVE_TEST
    AppState& as = AppState::getInstance();
    // AppState has dspEnabled but no dspBypassed; bypass maps to !dspEnabled
    as.dspEnabled = !bypass;
    as.markDspConfigDirty();
    return true;
#else
    (void)bypass;
    return true;
#endif
}

float HalDspBridge::dspGetInputLevel(uint8_t lane) const {
#ifndef NATIVE_TEST
    AudioAnalysis analysis = i2s_audio_get_analysis();
    // Return combined RMS for the requested ADC lane (0 or 1)
    if (lane < 2) {
        return analysis.adc[lane].rmsCombined;
    }
    return 0.0f;
#else
    (void)lane;
    return 0.0f;
#endif
}

float HalDspBridge::dspGetOutputLevel(uint8_t lane) const {
#ifndef NATIVE_TEST
    // Output level: use same analysis (post-pipeline RMS)
    // Future: read from per-output VU metering when available
    AudioAnalysis analysis = i2s_audio_get_analysis();
    if (lane < 2) {
        return analysis.adc[lane].rmsCombined;
    }
    return 0.0f;
#else
    (void)lane;
    return 0.0f;
#endif
}

#endif // DAC_ENABLED
