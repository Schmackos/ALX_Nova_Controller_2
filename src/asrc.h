#pragma once
// asrc.h — Software Asynchronous Sample Rate Converter (ASRC).
//
// Architecture:
//   Polyphase FIR interpolation, 160 phases × 32 taps = 5120 coefficients.
//   Coefficients are stored as const float in flash (not PSRAM).
//   Per-lane state (fractional phase accumulator + history ring buffer)
//   lives in PSRAM via psram_alloc().
//
// Insertion point: pipeline_resample_inputs() is called between
//   pipeline_to_float() and pipeline_run_dsp() in audio_pipeline.cpp.
//
// Passthrough: when srcRate == dstRate, resample is a zero-cost no-op.
// DSD lanes: isDsd == true → skip entirely (DoP must not be filtered).
//
// Supported input→output ratios (v1 — rational only):
//   44100→48000 (160/147), 48000→44100 (147/160)
//   88200→48000 (80/147),  96000→48000 (1/2)
//   176400→48000 (40/147), 192000→48000 (1/4)
//   Any equal-rate pair → passthrough (no computation)
//
// Memory budget: ~1.5 KB PSRAM per active lane
//   (ASRC_MAX_TAPS * sizeof(float) * 2 channels + phase state)
//
// Thread safety: asrc_init() and asrc_set_ratio() are main-loop only.
//   asrc_process_lane() is called exclusively from audio_pipeline_task
//   (Core 1) — no locking required.

#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Capacity constants — checked with static_assert against dimensions
// ---------------------------------------------------------------------------

#ifndef ASRC_MAX_TAPS
#define ASRC_MAX_TAPS   32    // FIR taps per polyphase branch
#endif
#ifndef ASRC_MAX_PHASES
#define ASRC_MAX_PHASES 160   // Number of polyphase branches
#endif

// ---------------------------------------------------------------------------
// Per-lane ASRC state (opaque to callers)
// ---------------------------------------------------------------------------

struct AsrcLaneState {
    float*   histL;          // Left-channel history ring buffer (ASRC_MAX_TAPS floats, PSRAM)
    float*   histR;          // Right-channel history buffer (ASRC_MAX_TAPS floats, PSRAM)
    uint32_t histHead;       // Ring buffer write position [0, ASRC_MAX_TAPS)
    uint32_t phaseAccum;     // Fixed-point phase accumulator (Q32.ASRC_MAX_PHASES scale)
    uint32_t phaseStep;      // Fixed-point step per output sample
    uint32_t interpNumer;    // Interpolation factor L (L/M ratio)
    uint32_t interpDenom;    // Decimation factor M
    bool     active;         // True when SRC is needed (srcRate != dstRate)
};

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

// Initialize ASRC subsystem. Call once from audio_pipeline_init() (main-loop,
// before audio task starts). Allocates per-lane history buffers from PSRAM.
void asrc_init(void);

// Free all ASRC resources. Called from audio_pipeline deinit path if needed.
void asrc_deinit(void);

// Set the input/output sample rate ratio for a specific lane.
// Call from main-loop context when audio_pipeline_check_format() detects a mismatch.
// srcRate == dstRate deactivates SRC for this lane (passthrough).
// Unknown ratios also deactivate SRC and log a warning.
void asrc_set_ratio(int lane, uint32_t srcRate, uint32_t dstRate);

// Bypass SRC for a lane unconditionally (used for DSD lanes).
void asrc_bypass(int lane);

// Process one frame buffer in-place on a lane.
// laneL/laneR: interleaved stereo float buffers, each 'frames' elements.
// outFrames: output buffer length (may differ from input if ratio != 1:1).
// Returns the number of output frames written to laneL/laneR.
// If lane is bypassed (DSD or equal rates), returns frames unchanged.
//
// IMPORTANT: laneL and laneR must have capacity >= frames * max(L/M ratio).
// At 44100→48000 (160/147 ≈ 1.09×), 256 input → ceil(256*160/147) = 279 output.
// The pipeline float buffers are sized ASRC_OUTPUT_FRAMES_MAX for this.
int asrc_process_lane(int lane, float* laneL, float* laneR, int frames);

// Get state for a lane (used for WS broadcast of laneSrcActive[]).
bool asrc_is_active(int lane);

// Reset per-lane history (call when a source is deregistered or during pause).
void asrc_reset_lane(int lane);

// Maximum output frames per 256-input frame (160/147 ≈ 1.088 → ceil = 279).
// Pipeline float buffers must be at least this size.
#define ASRC_OUTPUT_FRAMES_MAX  280

#ifdef __cplusplus
}
#endif
