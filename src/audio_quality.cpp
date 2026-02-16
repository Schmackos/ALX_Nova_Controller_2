#include "audio_quality.h"
#include "debug_serial.h"
#include <math.h>
#include <string.h>

#ifndef NATIVE_TEST
#include <ESP.h>
#endif

// Correlation window (100ms)
#define CORRELATION_WINDOW_MS 100

// Glitch detection constants
#define DISCONTINUITY_THRESHOLD_DEFAULT 0.5f
#define DC_OFFSET_THRESHOLD 0.7f
#define DROPOUT_THRESHOLD 0.05f
#define MAX_24BIT 8388607.0f

// Static state
static AudioQualityDiag _diag = {
    .enabled = false,
    .glitchThreshold = DISCONTINUITY_THRESHOLD_DEFAULT,
    .glitchHistory = {.writePos = 0, .totalGlitches = 0, .glitchesLastMinute = 0, .lastMinuteResetMs = 0},
    .timingHist = {.avgLatencyMs = 0.0f, .maxLatencyMs = 0.0f, .sampleCount = 0},
    .correlation = {.dspSwapRelated = false, .wifiRelated = false, .mqttRelated = false,
                    .lastDspSwapMs = 0, .lastWifiEventMs = 0, .lastMqttBurstMs = 0},
    .memoryHist = {.writePos = 0},
    .lastGlitchType = GLITCH_NONE,
    .lastGlitchMs = 0
};

// Forward declarations
static void _record_glitch(GlitchType type, int adcIndex, int channel, float magnitude, uint32_t sampleIndex);
static void _update_correlation();
static void _decay_minute_counter();

// === Public API Implementation ===

void audio_quality_init() {
    memset(&_diag, 0, sizeof(AudioQualityDiag));
    _diag.enabled = false;
    _diag.glitchThreshold = DISCONTINUITY_THRESHOLD_DEFAULT;
    _diag.lastGlitchType = GLITCH_NONE;
}

void audio_quality_enable(bool enable) {
    if (_diag.enabled == enable) return;

    _diag.enabled = enable;

    if (enable) {
        LOG_I("[AudioQuality] Diagnostics enabled (threshold: %.2f)", _diag.glitchThreshold);
    } else {
        LOG_I("[AudioQuality] Diagnostics disabled");
    }
}

bool audio_quality_is_enabled() {
    return _diag.enabled;
}

void audio_quality_set_threshold(float threshold) {
    if (threshold < 0.1f) threshold = 0.1f;
    if (threshold > 1.0f) threshold = 1.0f;
    _diag.glitchThreshold = threshold;
}

float audio_quality_get_threshold() {
    return _diag.glitchThreshold;
}

void audio_quality_scan_buffer(int adcIndex, const int32_t *buf, int stereoFrames, unsigned long latencyUs) {
    if (!_diag.enabled || !buf || stereoFrames <= 0) return;

    // Update timing histogram
    float latencyMs = (float)latencyUs / 1000.0f;
    int bucket = (int)latencyMs;

    if (bucket >= TIMING_HISTOGRAM_BUCKETS) {
        _diag.timingHist.overflows++;
    } else {
        _diag.timingHist.buckets[bucket]++;
    }

    // Update rolling average
    _diag.timingHist.sampleCount++;
    float alpha = 1.0f / (float)_diag.timingHist.sampleCount;
    if (alpha > 0.1f) alpha = 0.1f; // Clamp for stability

    _diag.timingHist.avgLatencyMs = _diag.timingHist.avgLatencyMs * (1.0f - alpha) + latencyMs * alpha;

    if (latencyMs > _diag.timingHist.maxLatencyMs) {
        _diag.timingHist.maxLatencyMs = latencyMs;
    }

    // Glitch detection on left and right channels
    for (int ch = 0; ch < 2; ch++) {
        float prevSample = 0.0f;
        float dcSum = 0.0f;
        float maxSample = 0.0f;
        int dropoutCount = 0;

        for (int i = 0; i < stereoFrames; i++) {
            float sample = (float)buf[i * 2 + ch] / MAX_24BIT;

            // Track max and DC
            float absSample = fabsf(sample);
            if (absSample > maxSample) maxSample = absSample;
            dcSum += sample;

            // Discontinuity detection (derivative threshold)
            if (i > 0) {
                float derivative = fabsf(sample - prevSample);
                if (derivative > _diag.glitchThreshold) {
                    _record_glitch(GLITCH_DISCONTINUITY, adcIndex, ch, derivative, i);
                }
            }

            // Dropout detection
            if (absSample < DROPOUT_THRESHOLD && i > 10) { // Skip initial samples
                dropoutCount++;
            }

            prevSample = sample;
        }

        // DC offset detection
        float dcAvg = dcSum / (float)stereoFrames;
        if (fabsf(dcAvg) > DC_OFFSET_THRESHOLD) {
            _record_glitch(GLITCH_DC_OFFSET, adcIndex, ch, fabsf(dcAvg), 0);
        }

        // Overload detection
        if (maxSample > 0.95f) {
            _record_glitch(GLITCH_OVERLOAD, adcIndex, ch, maxSample, 0);
        }

        // Dropout detection (>50% of samples near zero)
        if (dropoutCount > stereoFrames / 2) {
            _record_glitch(GLITCH_DROPOUT, adcIndex, ch, (float)dropoutCount / stereoFrames, 0);
        }
    }

    // Decay minute counter
    _decay_minute_counter();
}

void audio_quality_mark_event(const char *eventName) {
    if (!_diag.enabled) return;

    unsigned long now = millis();

    if (strcmp(eventName, "dsp_swap") == 0) {
        _diag.correlation.lastDspSwapMs = now;
        _update_correlation();
        LOG_D("[AudioQuality] DSP swap event marked");
    }
    else if (strcmp(eventName, "wifi_connected") == 0 || strcmp(eventName, "wifi_disconnected") == 0) {
        _diag.correlation.lastWifiEventMs = now;
        _update_correlation();
        LOG_D("[AudioQuality] WiFi event marked: %s", eventName);
    }
    else if (strcmp(eventName, "mqtt_burst") == 0) {
        _diag.correlation.lastMqttBurstMs = now;
        _update_correlation();
    }
}

AudioQualityDiag audio_quality_get_diagnostics() {
    return _diag;
}

void audio_quality_reset_stats() {
    LOG_I("[AudioQuality] Resetting statistics");

    // Preserve enable state and threshold
    bool wasEnabled = _diag.enabled;
    float threshold = _diag.glitchThreshold;

    // Clear everything
    memset(&_diag, 0, sizeof(AudioQualityDiag));

    // Restore settings
    _diag.enabled = wasEnabled;
    _diag.glitchThreshold = threshold;
    _diag.lastGlitchType = GLITCH_NONE;
}

void audio_quality_update_memory() {
    if (!_diag.enabled) return;

#ifndef NATIVE_TEST
    unsigned long now = millis();
    MemorySnapshot snapshot = {
        .timestampMs = now,
        .freeHeap = ESP.getFreeHeap(),
        .maxAllocHeap = ESP.getMaxAllocHeap()
    };

    _diag.memoryHist.snapshots[_diag.memoryHist.writePos] = snapshot;
    _diag.memoryHist.writePos = (_diag.memoryHist.writePos + 1) % MEMORY_SNAPSHOT_COUNT;
#endif
}

const char* audio_quality_glitch_type_to_string(GlitchType type) {
    switch (type) {
        case GLITCH_NONE: return "None";
        case GLITCH_DISCONTINUITY: return "Discontinuity";
        case GLITCH_DC_OFFSET: return "DC Offset";
        case GLITCH_DROPOUT: return "Dropout";
        case GLITCH_OVERLOAD: return "Overload";
        default: return "Unknown";
    }
}

// === Private Helper Functions ===

static void _record_glitch(GlitchType type, int adcIndex, int channel, float magnitude, uint32_t sampleIndex) {
    unsigned long now = millis();

    // Throttle logging (max 1 log per type per second)
    static unsigned long lastLogMs[5] = {0}; // One per GlitchType
    if (now - lastLogMs[type] < 1000) {
        // Still increment counters, just don't log
    } else {
        LOG_W("[AudioQuality] Glitch detected: %s on ADC%d CH%d (mag: %.3f, sample: %u)",
              audio_quality_glitch_type_to_string(type), adcIndex + 1, channel, magnitude, sampleIndex);
        lastLogMs[type] = now;
    }

    // Record in ring buffer
    GlitchEvent event = {
        .timestampMs = now,
        .type = type,
        .adcIndex = (uint8_t)adcIndex,
        .channel = (uint8_t)channel,
        .magnitude = magnitude,
        .sampleIndex = sampleIndex
    };

    _diag.glitchHistory.events[_diag.glitchHistory.writePos] = event;
    _diag.glitchHistory.writePos = (_diag.glitchHistory.writePos + 1) % GLITCH_HISTORY_SIZE;
    _diag.glitchHistory.totalGlitches++;
    _diag.glitchHistory.glitchesLastMinute++;

    _diag.lastGlitchType = type;
    _diag.lastGlitchMs = now;

    // Update correlation
    _update_correlation();
}

static void _update_correlation() {
    unsigned long now = millis();

    // Check if recent glitch correlates with events
    if (_diag.lastGlitchMs > 0 && now - _diag.lastGlitchMs < CORRELATION_WINDOW_MS) {
        // Check DSP swap correlation
        if (_diag.correlation.lastDspSwapMs > 0 &&
            abs((long)(_diag.lastGlitchMs - _diag.correlation.lastDspSwapMs)) < CORRELATION_WINDOW_MS) {
            _diag.correlation.dspSwapRelated = true;
        }

        // Check WiFi correlation
        if (_diag.correlation.lastWifiEventMs > 0 &&
            abs((long)(_diag.lastGlitchMs - _diag.correlation.lastWifiEventMs)) < CORRELATION_WINDOW_MS) {
            _diag.correlation.wifiRelated = true;
        }

        // Check MQTT correlation
        if (_diag.correlation.lastMqttBurstMs > 0 &&
            abs((long)(_diag.lastGlitchMs - _diag.correlation.lastMqttBurstMs)) < CORRELATION_WINDOW_MS) {
            _diag.correlation.mqttRelated = true;
        }
    } else {
        // Clear correlation flags if no recent glitches
        _diag.correlation.dspSwapRelated = false;
        _diag.correlation.wifiRelated = false;
        _diag.correlation.mqttRelated = false;
    }
}

static void _decay_minute_counter() {
    unsigned long now = millis();

    if (_diag.glitchHistory.lastMinuteResetMs == 0) {
        _diag.glitchHistory.lastMinuteResetMs = now;
        return;
    }

    // Reset minute counter every 60 seconds
    if (now - _diag.glitchHistory.lastMinuteResetMs >= 60000) {
        _diag.glitchHistory.glitchesLastMinute = 0;
        _diag.glitchHistory.lastMinuteResetMs = now;
    }
}
