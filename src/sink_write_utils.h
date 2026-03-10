#ifndef SINK_WRITE_UTILS_H
#define SINK_WRITE_UTILS_H

#include <stddef.h>
#include <stdint.h>

#define MUTE_RAMP_STEP 0.02f

// Apply software volume gain to a float buffer in-place.
// Skips processing if gain == 1.0f (unity).
void sink_apply_volume(float* buf, size_t len, float gain);

// Apply mute ramp to a float buffer in-place.
// rampState tracks the current ramp position [0.0 .. 1.0].
// muted=true ramps toward 0.0; muted=false ramps toward 1.0.
void sink_apply_mute_ramp(float* buf, size_t len, float* rampState, bool muted);

// Convert float [-1.0,+1.0] buffer to int32 left-justified for I2S DMA.
void sink_float_to_i2s_int32(const float* in, int32_t* out, size_t len);

#endif
