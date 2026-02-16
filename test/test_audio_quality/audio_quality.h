/**
 * @file audio_quality.h
 * @brief Audio quality diagnostics module - API specification for testing
 *
 * This header defines the expected interface for the audio quality module.
 * The actual implementation will be in src/audio_quality.h/.cpp.
 *
 * This test-local version allows tests to compile before the module is implemented.
 */

#ifndef AUDIO_QUALITY_H
#define AUDIO_QUALITY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Constants
// ============================================================================

#define GLITCH_HISTORY_SIZE 32        // Ring buffer for recent glitches
#define TIMING_HISTOGRAM_BUCKETS 20   // 0-19ms, 1ms per bucket
#define MEMORY_HISTORY_SIZE 60        // 60 seconds of heap snapshots
#define EVENT_CORRELATION_WINDOW_MS 100 // Correlation window for system events

// ============================================================================
// Enums
// ============================================================================

/**
 * @brief Types of audio glitches that can be detected
 */
typedef enum {
    GLITCH_TYPE_NONE = 0,          // No glitch
    GLITCH_TYPE_DISCONTINUITY = 1, // Large sample-to-sample jump
    GLITCH_TYPE_DC_OFFSET = 2,     // Sustained DC component
    GLITCH_TYPE_DROPOUT = 3,       // >50% samples near zero
    GLITCH_TYPE_OVERLOAD = 4       // Samples >95% of full-scale (clipping)
} GlitchType;

// ============================================================================
// Structures
// ============================================================================

/**
 * @brief Event correlation flags (system events that may correlate with glitches)
 */
typedef struct {
    bool dspSwap;      // DSP config swap within 100ms
    bool wifiEvent;    // WiFi connect/disconnect within 100ms
    bool mqttEvent;    // MQTT connect/disconnect within 100ms
} EventCorrelation;

/**
 * @brief Single glitch event
 */
typedef struct {
    unsigned long timestamp;    // millis() when glitch occurred
    GlitchType type;            // Type of glitch
    uint8_t adcIndex;           // ADC index (0 or 1)
    uint8_t channel;            // 0=left, 1=right
    float magnitude;            // Magnitude (0.0-1.0 normalized to full-scale)
    uint16_t sampleIndex;       // Sample index within buffer
    EventCorrelation correlation; // Correlated system events
} GlitchEvent;

/**
 * @brief Ring buffer of recent glitch events
 */
typedef struct {
    GlitchEvent events[GLITCH_HISTORY_SIZE]; // Ring buffer
    uint8_t writePos;           // Next write position (0-31)
    uint32_t totalCount;        // Total glitches since reset
    uint32_t lastMinuteCount;   // Glitches in last 60 seconds
} GlitchHistory;

/**
 * @brief Timing histogram for audio processing latency
 */
typedef struct {
    uint32_t buckets[TIMING_HISTOGRAM_BUCKETS]; // 0-19ms, 1ms per bucket
    uint32_t overflowCount;     // Samples >20ms
    uint32_t sampleCount;       // Total samples
    uint32_t avgLatencyUs;      // Average latency in microseconds
    uint32_t maxLatencyUs;      // Maximum latency in microseconds
} TimingHistogram;

/**
 * @brief Heap memory snapshot
 */
typedef struct {
    unsigned long timestamp;    // millis() when snapshot taken
    uint32_t freeHeap;          // ESP.getFreeHeap()
    uint32_t maxAllocHeap;      // ESP.getMaxAllocHeap()
    uint32_t freePsram;         // ESP.getFreePsram()
} MemorySnapshot;

/**
 * @brief Ring buffer of memory snapshots (1 per second, 60 second window)
 */
typedef struct {
    MemorySnapshot snapshots[MEMORY_HISTORY_SIZE]; // Ring buffer
    uint8_t writePos;           // Next write position (0-59)
} MemoryHistory;

/**
 * @brief Master diagnostics structure
 */
typedef struct {
    GlitchHistory glitchHistory;
    TimingHistogram timingHistogram;
    MemoryHistory memoryHistory;
} AudioQualityDiag;

// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Initialize audio quality diagnostics system
 *
 * Sets defaults: enabled=false, threshold=0.5
 * Clears all statistics
 */
void audio_quality_init(void);

/**
 * @brief Enable or disable audio quality diagnostics
 *
 * When disabled, scan_buffer() becomes a no-op for performance
 *
 * @param enabled true to enable, false to disable
 */
void audio_quality_enable(bool enabled);

/**
 * @brief Check if audio quality diagnostics are enabled
 *
 * @return true if enabled, false if disabled
 */
bool audio_quality_is_enabled(void);

/**
 * @brief Set glitch detection threshold
 *
 * Threshold is normalized to full-scale (0.0-1.0)
 * Values outside this range are clamped to [0.1, 1.0]
 *
 * @param threshold Detection threshold (0.1-1.0)
 */
void audio_quality_set_threshold(float threshold);

/**
 * @brief Get current glitch detection threshold
 *
 * @return Current threshold (0.1-1.0)
 */
float audio_quality_get_threshold(void);

/**
 * @brief Scan audio buffer for glitches and timing
 *
 * If disabled, returns immediately without processing
 * Otherwise analyzes buffer for:
 * - Discontinuities (large sample-to-sample jumps)
 * - DC offsets (sustained DC component)
 * - Dropouts (>50% samples near zero)
 * - Overloads (samples >95% of full-scale)
 *
 * Also records processing latency in histogram
 *
 * @param adcIndex ADC index (0 or 1)
 * @param buf Pointer to interleaved stereo int32 buffer (left-justified)
 * @param stereoFrames Number of stereo frames (buffer size = stereoFrames * 2)
 * @param latencyUs Processing latency in microseconds
 */
void audio_quality_scan_buffer(int adcIndex, const int32_t *buf, int stereoFrames, unsigned long latencyUs);

/**
 * @brief Mark a system event for correlation analysis
 *
 * Glitches occurring within 100ms of a marked event will have correlation flags set
 *
 * Supported event names:
 * - "dsp_swap" - DSP configuration swap
 * - "wifi_connected" - WiFi connection established
 * - "wifi_disconnected" - WiFi disconnected
 * - "mqtt_connected" - MQTT broker connected
 * - "mqtt_disconnected" - MQTT broker disconnected
 *
 * @param eventName Name of system event
 */
void audio_quality_mark_event(const char *eventName);

/**
 * @brief Get full diagnostics structure
 *
 * @return Pointer to diagnostics struct (read-only, valid until next call)
 */
const AudioQualityDiag* audio_quality_get_diagnostics(void);

/**
 * @brief Reset all statistics
 *
 * Clears glitch history, timing histogram, memory history
 * Preserves settings (enabled, threshold)
 */
void audio_quality_reset_stats(void);

/**
 * @brief Update memory snapshot
 *
 * Called periodically (e.g., every 1 second) from main loop
 * Captures current heap/PSRAM state into ring buffer
 */
void audio_quality_update_memory(void);

/**
 * @brief Convert glitch type enum to string
 *
 * @param type Glitch type enum
 * @return String representation (e.g., "DISCONTINUITY")
 */
const char* audio_quality_glitch_type_to_string(GlitchType type);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_QUALITY_H
