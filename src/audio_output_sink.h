#ifndef AUDIO_OUTPUT_SINK_H
#define AUDIO_OUTPUT_SINK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Removed in Phase 4: AUDIO_SINK_SLOT_PRIMARY and AUDIO_SINK_SLOT_ES8311 constants
// Use hal_pipeline_get_sink_slot() for dynamic assignment instead.

// Maximum sinks
#ifndef AUDIO_OUT_MAX_SINKS
#define AUDIO_OUT_MAX_SINKS 8
#endif

// Output sink interface — mirrors AudioInputSource pattern.
// Each audio output (DAC, codec, amp) registers itself via this struct.
// The pipeline iterates registered sinks in pipeline_write_output().
typedef struct AudioOutputSink {
    const char *name;           // Human-readable: "PCM5102A", "ES8311"
    uint8_t firstChannel;       // First mono matrix output channel (0, 2, 4, 6)
    uint8_t channelCount;       // 1=mono, 2=stereo (usually 2)

    // Write stereo frames from interleaved int32 buffer.
    // buf contains stereoFrames * 2 interleaved L/R int32 samples.
    void (*write)(const int32_t *buf, int stereoFrames);

    // Returns true if the sink hardware is ready to accept audio.
    bool (*isReady)(void);

    // Post-matrix gain trim (1.0 = unity). Separate from HW volume in the driver.
    float gainLinear;

    // Software volume gain (0.0–1.0, log-perceptual curve). Applied by the DAC
    // write path. Set via audio_pipeline_set_sink_volume(). Default 1.0 = unity.
    float volumeGain;

    // Mute flag — sink skipped when true (mute ramp handled inside write callback)
    bool muted;

    // VU metering output (dBFS), updated by pipeline after each write.
    float vuL;
    float vuR;

    // Internal VU ballistics state (smoothed RMS) — do not access directly.
    float _vuSmoothedL;
    float _vuSmoothedR;

    // HAL device slot index that owns this sink. 0xFF = not bound to any HAL device.
    uint8_t halSlot;

    // Opaque context pointer — used by buildSink() to associate the sink with
    // its owning HalDevice*. Current write/isReady callbacks don't take ctx
    // (legacy dispatch tables), but bridge/driver code can read it for lookups.
    void *ctx;
} AudioOutputSink;

// Default initializer
#define AUDIO_OUTPUT_SINK_INIT { \
    NULL,   /* name */           \
    0,      /* firstChannel */   \
    2,      /* channelCount */   \
    NULL,   /* write */          \
    NULL,   /* isReady */        \
    1.0f,   /* gainLinear */     \
    1.0f,   /* volumeGain */     \
    false,  /* muted */          \
    -90.0f, /* vuL */            \
    -90.0f, /* vuR */            \
    0.0f,   /* _vuSmoothedL */   \
    0.0f,   /* _vuSmoothedR */   \
    0xFF,   /* halSlot */        \
    NULL    /* ctx */            \
}

#ifdef __cplusplus
}
#endif

#endif // AUDIO_OUTPUT_SINK_H
