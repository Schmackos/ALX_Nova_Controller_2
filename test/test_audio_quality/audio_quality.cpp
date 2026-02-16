/**
 * @file audio_quality.cpp
 * @brief Audio quality diagnostics - stub implementation for testing
 *
 * This is a minimal stub to allow tests to compile.
 * Replace this with the actual implementation in src/audio_quality.cpp when ready.
 */

#include "audio_quality.h"
#include <cstring>
#include <cmath>

// ============================================================================
// Module State
// ============================================================================

static AudioQualityDiag s_diag;
static bool s_enabled = false;
static float s_threshold = 0.5f;

// Event timestamps for correlation
static unsigned long s_lastDspSwapMs = 0;
static unsigned long s_lastWifiEventMs = 0;
static unsigned long s_lastMqttEventMs = 0;

// External millis() declaration
extern "C" unsigned long millis();

// ============================================================================
// Private Helpers
// ============================================================================

/**
 * @brief Check if event occurred within correlation window
 */
static bool is_correlated(unsigned long eventMs, unsigned long currentMs) {
    if (eventMs == 0) return false;
    unsigned long elapsed = currentMs - eventMs;
    return (elapsed <= EVENT_CORRELATION_WINDOW_MS);
}

/**
 * @brief Detect discontinuity in buffer
 */
static bool detect_discontinuity(const int32_t *buf, int stereoFrames, float threshold, int *outChannel, int *outIndex, float *outMagnitude) {
    const float fullScale = 0x7FFFFFFF;
    const float thresholdAbs = threshold * fullScale;

    for (int i = 1; i < stereoFrames * 2; i++) {
        float diff = fabsf((float)(buf[i] - buf[i-1]));
        if (diff > thresholdAbs) {
            *outChannel = i % 2;
            *outIndex = i / 2;
            *outMagnitude = diff / fullScale;
            return true;
        }
    }
    return false;
}

/**
 * @brief Detect DC offset in buffer
 *
 * Detects sustained DC component that's a significant portion of the signal.
 * Triggers if average DC level is above threshold.
 */
static bool detect_dc_offset(const int32_t *buf, int stereoFrames, float threshold, int *outChannel, float *outMagnitude) {
    const float fullScale = 0x7FFFFFFF;
    const float thresholdAbs = threshold * fullScale;

    // Calculate DC for each channel
    double sumL = 0, sumR = 0;
    for (int i = 0; i < stereoFrames; i++) {
        sumL += buf[i * 2];
        sumR += buf[i * 2 + 1];
    }

    float dcL = fabsf((float)(sumL / stereoFrames));
    float dcR = fabsf((float)(sumR / stereoFrames));

    // Trigger if DC exceeds threshold
    if (dcL > thresholdAbs) {
        *outChannel = 0;
        *outMagnitude = dcL / fullScale;
        return true;
    }
    if (dcR > thresholdAbs) {
        *outChannel = 1;
        *outMagnitude = dcR / fullScale;
        return true;
    }
    return false;
}

/**
 * @brief Detect dropout (>50% samples near zero)
 *
 * Only triggers if there's a mix of signal and zeros (not all zeros)
 */
static bool detect_dropout(const int32_t *buf, int stereoFrames, float *outMagnitude) {
    const float zeroThreshold = 0x7FFFFFFF * 0.0001f; // 0.01% of full-scale (~214k for int32)
    int zeroCount = 0;
    int totalSamples = stereoFrames * 2;
    int nonZeroCount = 0;

    for (int i = 0; i < totalSamples; i++) {
        if (fabsf((float)buf[i]) < zeroThreshold) {
            zeroCount++;
        } else {
            nonZeroCount++;
        }
    }

    float dropoutRatio = (float)zeroCount / totalSamples;
    // Only trigger if >50% zeros AND there's some non-zero content
    // (avoids false positive on all-zero buffers)
    // Require at least 10% non-zero to indicate a real dropout (not just silence)
    if (dropoutRatio > 0.5f && (float)nonZeroCount / totalSamples >= 0.1f) {
        *outMagnitude = dropoutRatio;
        return true;
    }
    return false;
}

/**
 * @brief Detect overload/clipping (samples >95% full-scale)
 */
static bool detect_overload(const int32_t *buf, int stereoFrames, int *outChannel, int *outIndex, float *outMagnitude) {
    const float fullScale = 0x7FFFFFFF;
    const float clipThreshold = fullScale * 0.95f;

    for (int i = 0; i < stereoFrames * 2; i++) {
        if (fabsf((float)buf[i]) > clipThreshold) {
            *outChannel = i % 2;
            *outIndex = i / 2;
            *outMagnitude = fabsf((float)buf[i]) / fullScale;
            return true;
        }
    }
    return false;
}

/**
 * @brief Add glitch event to history
 */
static void add_glitch_event(GlitchType type, uint8_t adcIndex, uint8_t channel, float magnitude, uint16_t sampleIndex) {
    unsigned long now = millis();

    // Create event
    GlitchEvent *event = &s_diag.glitchHistory.events[s_diag.glitchHistory.writePos];
    event->timestamp = now;
    event->type = type;
    event->adcIndex = adcIndex;
    event->channel = channel;
    event->magnitude = magnitude;
    event->sampleIndex = sampleIndex;

    // Check correlations
    event->correlation.dspSwap = is_correlated(s_lastDspSwapMs, now);
    event->correlation.wifiEvent = is_correlated(s_lastWifiEventMs, now);
    event->correlation.mqttEvent = is_correlated(s_lastMqttEventMs, now);

    // Advance write position
    s_diag.glitchHistory.writePos = (s_diag.glitchHistory.writePos + 1) % GLITCH_HISTORY_SIZE;
    s_diag.glitchHistory.totalCount++;
    s_diag.glitchHistory.lastMinuteCount++;
}

/**
 * @brief Update timing histogram
 */
static void update_timing_histogram(unsigned long latencyUs) {
    s_diag.timingHistogram.sampleCount++;

    // Update buckets (1ms per bucket)
    unsigned long latencyMs = latencyUs / 1000;
    if (latencyMs < TIMING_HISTOGRAM_BUCKETS) {
        s_diag.timingHistogram.buckets[latencyMs]++;
    } else {
        s_diag.timingHistogram.overflowCount++;
    }

    // Update max
    if (latencyUs > s_diag.timingHistogram.maxLatencyUs) {
        s_diag.timingHistogram.maxLatencyUs = latencyUs;
    }

    // Update running average
    uint64_t totalUs = (uint64_t)s_diag.timingHistogram.avgLatencyUs * (s_diag.timingHistogram.sampleCount - 1) + latencyUs;
    s_diag.timingHistogram.avgLatencyUs = (uint32_t)(totalUs / s_diag.timingHistogram.sampleCount);
}

/**
 * @brief Decay last-minute counter
 */
static void decay_last_minute_counter() {
    unsigned long now = millis();
    uint32_t newCount = 0;

    // Count glitches in last 60 seconds
    for (int i = 0; i < GLITCH_HISTORY_SIZE; i++) {
        if (s_diag.glitchHistory.events[i].timestamp > 0 &&
            (now - s_diag.glitchHistory.events[i].timestamp) < 60000) {
            newCount++;
        }
    }

    s_diag.glitchHistory.lastMinuteCount = newCount;
}

// ============================================================================
// Public API Implementation
// ============================================================================

void audio_quality_init(void) {
    memset(&s_diag, 0, sizeof(s_diag));
    s_enabled = false;
    s_threshold = 0.5f;
    s_lastDspSwapMs = 0;
    s_lastWifiEventMs = 0;
    s_lastMqttEventMs = 0;
}

void audio_quality_enable(bool enabled) {
    s_enabled = enabled;
}

bool audio_quality_is_enabled(void) {
    return s_enabled;
}

void audio_quality_set_threshold(float threshold) {
    // Clamp to [0.1, 1.0]
    if (threshold < 0.1f) threshold = 0.1f;
    if (threshold > 1.0f) threshold = 1.0f;
    s_threshold = threshold;
}

float audio_quality_get_threshold(void) {
    return s_threshold;
}

void audio_quality_scan_buffer(int adcIndex, const int32_t *buf, int stereoFrames, unsigned long latencyUs) {
    if (!s_enabled || buf == nullptr || stereoFrames <= 0) {
        return;
    }

    // Update timing histogram
    update_timing_histogram(latencyUs);

    // Decay last-minute counter
    decay_last_minute_counter();

    // Detect glitches (priority order)
    int channel = 0, index = 0;
    float magnitude = 0.0f;

    // 1. Discontinuity (highest priority for sample-to-sample jumps)
    if (detect_discontinuity(buf, stereoFrames, s_threshold, &channel, &index, &magnitude)) {
        add_glitch_event(GLITCH_TYPE_DISCONTINUITY, adcIndex, channel, magnitude, index);
        return;
    }

    // 2. Overload
    if (detect_overload(buf, stereoFrames, &channel, &index, &magnitude)) {
        add_glitch_event(GLITCH_TYPE_OVERLOAD, adcIndex, channel, magnitude, index);
        return;
    }

    // 3. DC offset
    if (detect_dc_offset(buf, stereoFrames, s_threshold, &channel, &magnitude)) {
        add_glitch_event(GLITCH_TYPE_DC_OFFSET, adcIndex, channel, magnitude, 0);
        return;
    }

    // 4. Dropout (lowest priority)
    if (detect_dropout(buf, stereoFrames, &magnitude)) {
        add_glitch_event(GLITCH_TYPE_DROPOUT, adcIndex, 0, magnitude, 0);
        return;
    }
}

void audio_quality_mark_event(const char *eventName) {
    if (eventName == nullptr) return;

    unsigned long now = millis();

    if (strcmp(eventName, "dsp_swap") == 0) {
        s_lastDspSwapMs = now;
    } else if (strcmp(eventName, "wifi_connected") == 0 || strcmp(eventName, "wifi_disconnected") == 0) {
        s_lastWifiEventMs = now;
    } else if (strcmp(eventName, "mqtt_connected") == 0 || strcmp(eventName, "mqtt_disconnected") == 0) {
        s_lastMqttEventMs = now;
    }
}

const AudioQualityDiag* audio_quality_get_diagnostics(void) {
    return &s_diag;
}

void audio_quality_reset_stats(void) {
    memset(&s_diag, 0, sizeof(s_diag));
    s_lastDspSwapMs = 0;
    s_lastWifiEventMs = 0;
    s_lastMqttEventMs = 0;
}

void audio_quality_update_memory(void) {
    MemorySnapshot *snap = &s_diag.memoryHistory.snapshots[s_diag.memoryHistory.writePos];
    snap->timestamp = millis();
    snap->freeHeap = 0;       // Mock values for native test
    snap->maxAllocHeap = 0;
    snap->freePsram = 0;

    s_diag.memoryHistory.writePos = (s_diag.memoryHistory.writePos + 1) % MEMORY_HISTORY_SIZE;
}

const char* audio_quality_glitch_type_to_string(GlitchType type) {
    switch (type) {
        case GLITCH_TYPE_NONE:          return "NONE";
        case GLITCH_TYPE_DISCONTINUITY: return "DISCONTINUITY";
        case GLITCH_TYPE_DC_OFFSET:     return "DC_OFFSET";
        case GLITCH_TYPE_DROPOUT:       return "DROPOUT";
        case GLITCH_TYPE_OVERLOAD:      return "OVERLOAD";
        default:                        return "UNKNOWN";
    }
}
