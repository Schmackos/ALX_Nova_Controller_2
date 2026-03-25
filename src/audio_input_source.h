#ifndef AUDIO_INPUT_SOURCE_H
#define AUDIO_INPUT_SOURCE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// DEPRECATED (Phase 2): Physical I2S port indices only. For dynamic lane assignment, use bridge-assigned lanes.
// Legacy hardcoded registration removed — ADC sources now registered via HalPcm1808::getInputSource().
#define AUDIO_SRC_LANE_ADC1   0
#define AUDIO_SRC_LANE_ADC2   1

// Maximum number of input sources (guarded so audio_pipeline.h can override)
#ifndef AUDIO_PIPELINE_MAX_INPUTS
#define AUDIO_PIPELINE_MAX_INPUTS 8
#endif

// Reusable input source interface.
// Each audio input (ADC, USB, signal generator, etc.) can register itself
// via this struct. The audio pipeline loops over registered sources.
// Designed for future dynamic input module system.
typedef struct AudioInputSource {
    const char *name;       // Human-readable name: "ADC1", "ADC2", "SigGen", "USB"
    uint8_t     lane;       // Pipeline lane index (AUDIO_SRC_LANE_*)

    // Read stereo frames into dst buffer. Returns number of frames actually read.
    // dst must hold at least requestedFrames * 2 int32_t elements (interleaved L/R).
    uint32_t (*read)(int32_t *dst, uint32_t requestedFrames);

    // Returns true if the source is currently active/connected and producing audio.
    bool (*isActive)(void);

    // Returns the source's current sample rate in Hz.
    uint32_t (*getSampleRate)(void);

    // Pre-matrix gain applied after read (host volume for USB, input trim for ADC).
    float gainLinear;

    // VU metering output (dBFS), updated by pipeline after each read().
    float vuL;
    float vuR;

    // Internal VU ballistics state (smoothed RMS) — do not access directly.
    float _vuSmoothedL;
    float _vuSmoothedR;

    // HAL device slot index that owns this source. 0xFF = not bound to any HAL device.
    uint8_t halSlot;

    // True for physical ADC sources (PCM1808). False for software sources (SigGen, USB).
    // Used by noise gate to determine which lanes get noise gating.
    bool isHardwareAdc;

    // Format negotiation fields (Phase 1+2 hardening)
    uint8_t  bitDepth;   // Actual bit depth produced: 16, 24, or 32 (0 = unknown/auto)
    bool     isDsd;      // True when DoP DSD content detected on this lane
} AudioInputSource;

// Default initializer — all NULLs, gain=1.0, VU=-90dBFS, smoothed=0
#define AUDIO_INPUT_SOURCE_INIT { \
    NULL,  /* name */            \
    0,     /* lane */            \
    NULL,  /* read */            \
    NULL,  /* isActive */        \
    NULL,  /* getSampleRate */   \
    1.0f,  /* gainLinear */      \
    -90.0f, /* vuL */            \
    -90.0f, /* vuR */            \
    0.0f,  /* _vuSmoothedL */    \
    0.0f,  /* _vuSmoothedR */    \
    0xFF,  /* halSlot */         \
    false, /* isHardwareAdc */   \
    0,     /* bitDepth */        \
    false  /* isDsd */           \
}

#ifdef __cplusplus
}
#endif

#endif // AUDIO_INPUT_SOURCE_H
