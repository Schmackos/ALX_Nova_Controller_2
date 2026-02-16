#ifndef AUDIO_QUALITY_H
#define AUDIO_QUALITY_H

#include <Arduino.h>

// Glitch detection types
enum GlitchType {
    GLITCH_NONE = 0,
    GLITCH_DISCONTINUITY = 1,  // Large sample-to-sample jump
    GLITCH_DC_OFFSET = 2,      // Sustained DC component
    GLITCH_DROPOUT = 3,        // Sudden drop to near-zero
    GLITCH_OVERLOAD = 4        // Clipping/overload
};

// Individual glitch event record
struct GlitchEvent {
    unsigned long timestampMs;
    GlitchType type;
    uint8_t adcIndex;
    uint8_t channel;
    float magnitude;
    uint32_t sampleIndex;
};

// Ring buffer of recent glitches
#define GLITCH_HISTORY_SIZE 32
struct GlitchHistory {
    GlitchEvent events[GLITCH_HISTORY_SIZE];
    uint8_t writePos;
    uint32_t totalGlitches;
    uint32_t glitchesLastMinute;
    unsigned long lastMinuteResetMs;
};

// Event correlation flags
struct EventCorrelation {
    bool dspSwapRelated;
    bool wifiRelated;
    bool mqttRelated;
    unsigned long lastDspSwapMs;
    unsigned long lastWifiEventMs;
    unsigned long lastMqttBurstMs;
};

// Memory snapshot
struct MemorySnapshot {
    unsigned long timestampMs;
    uint32_t freeHeap;
    uint32_t maxAllocHeap;
};

// Memory history (1-minute rolling window)
#define MEMORY_SNAPSHOT_COUNT 60
struct MemoryHistory {
    MemorySnapshot snapshots[MEMORY_SNAPSHOT_COUNT];
    uint8_t writePos;
};

// Main diagnostics structure
struct AudioQualityDiag {
    bool enabled;
    float glitchThreshold;
    GlitchHistory glitchHistory;
    EventCorrelation correlation;
    MemoryHistory memoryHist;
    GlitchType lastGlitchType;
    unsigned long lastGlitchMs;
};

// Public API
void audio_quality_init();
void audio_quality_enable(bool enable);
bool audio_quality_is_enabled();
void audio_quality_set_threshold(float threshold);
float audio_quality_get_threshold();

// Called from audio task after processing each ADC buffer
void audio_quality_scan_buffer(int adcIndex, const int32_t *buf, int stereoFrames);

// Called when events occur (for correlation tracking)
void audio_quality_mark_event(const char *eventName);

// Get current diagnostics (for WebSocket/MQTT/REST)
AudioQualityDiag audio_quality_get_diagnostics();

// Reset statistics
void audio_quality_reset_stats();

// Periodic update (call from main loop every 1s)
void audio_quality_update_memory();

// Get last glitch type as string (for GUI)
const char* audio_quality_glitch_type_to_string(GlitchType type);

#endif // AUDIO_QUALITY_H
