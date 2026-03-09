#ifdef DAC_ENABLED
// HalDspBridge — bridges HAL device interface to the dsp_pipeline module

#include "hal_dsp_bridge.h"

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#include "../app_state.h"
#include "../i2s_audio.h"
#include "../audio_pipeline.h"
#include "../audio_output_sink.h"
#include <math.h>
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
    LOG_I("[HAL:DspBridge] DSP pipeline bridge ready");
    return hal_init_ok();
}

void HalDspBridge::deinit() {
    _ready = false;
    _state = HAL_STATE_REMOVED;
}

void HalDspBridge::dumpConfig() {
    int dspEnabled = 0;
#ifndef NATIVE_TEST
    dspEnabled = AppState::getInstance().dsp.enabled;
#endif
    LOG_I("[HAL:DspBridge] DSP Pipeline bridge — dspEnabled=%d", dspEnabled);
}

bool HalDspBridge::healthCheck() {
    return true;
}

bool HalDspBridge::dspIsActive() const {
#ifndef NATIVE_TEST
    return AppState::getInstance().dsp.enabled;
#else
    return false;
#endif
}

bool HalDspBridge::dspSetBypassed(bool bypass) {
#ifndef NATIVE_TEST
    AppState& as = AppState::getInstance();
    // AppState has dspEnabled but no dspBypassed; bypass maps to !dspEnabled
    as.dsp.enabled = !bypass;
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
    // Return combined RMS for the requested input lane
    if (lane < AUDIO_PIPELINE_MAX_INPUTS) {
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
    // Read actual per-sink VU metering (dBFS), convert to linear 0.0-1.0
    const AudioOutputSink* sink = audio_pipeline_get_sink(lane);
    if (sink && sink->vuL > -89.0f) {
        float avgDb = (sink->vuL + sink->vuR) * 0.5f;
        return powf(10.0f, avgDb / 20.0f);
    }
    return 0.0f;
#else
    (void)lane;
    return 0.0f;
#endif
}

#endif // DAC_ENABLED
