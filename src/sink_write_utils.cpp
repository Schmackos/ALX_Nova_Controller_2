#include "sink_write_utils.h"
#include <math.h>

void sink_apply_volume(float* buf, size_t len, float gain) {
    if (gain >= 0.999f && gain <= 1.001f) return;  // Unity -- skip
    for (size_t i = 0; i < len; i++) {
        buf[i] *= gain;
    }
}

void sink_apply_mute_ramp(float* buf, size_t len, float* rampState, bool muted) {
    float target = muted ? 0.0f : 1.0f;
    float g = *rampState;
    if (fabsf(g - target) < 0.001f) {
        *rampState = target;
        if (muted) {
            for (size_t i = 0; i < len; i++) buf[i] = 0.0f;
        }
        return;
    }
    for (size_t i = 0; i < len; i++) {
        if (g < target) { g += MUTE_RAMP_STEP; if (g > target) g = target; }
        else            { g -= MUTE_RAMP_STEP; if (g < target) g = target; }
        buf[i] *= g;
    }
    *rampState = g;
}

void sink_float_to_i2s_int32(const float* in, int32_t* out, size_t len) {
    // 2147483647.0f cannot be represented exactly in float32 (rounds to 2^31).
    // Use 2147483520.0f (largest exact float32 below 2^31-1) to avoid overflow.
    static const float SCALE = 2147483520.0f;
    for (size_t i = 0; i < len; i++) {
        float clamped = in[i];
        if (clamped > 1.0f) clamped = 1.0f;
        if (clamped < -1.0f) clamped = -1.0f;
        out[i] = (int32_t)(clamped * SCALE);
    }
}
