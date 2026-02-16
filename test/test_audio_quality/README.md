# Audio Quality Diagnostics - Test Suite

## Overview

Comprehensive unit tests for the audio quality diagnostics module (`src/audio_quality.h/.cpp`).

**Total Tests:** 33
**Framework:** Unity
**Platform:** Native (host machine, no ESP32 hardware required)
**Status:** ✅ All tests passing

## Test Coverage

### Group 1: Initialization & State (5 tests)
- ✅ `test_init_sets_defaults` - Verifies default state (enabled=false, threshold=0.5)
- ✅ `test_enable_disable_transitions` - State transitions work correctly
- ✅ `test_threshold_validation_clamps` - Threshold clamped to [0.1, 1.0] range
- ✅ `test_threshold_get_set_roundtrip` - Get/set threshold preserves value
- ✅ `test_multiple_init_calls_safe` - Re-initialization resets to defaults

### Group 2: Glitch Detection (8 tests)
- ✅ `test_discontinuity_detection_large_jump` - Large sample-to-sample jumps detected
- ✅ `test_dc_offset_detection` - Sustained DC component detected
- ✅ `test_dropout_detection_silent_samples` - >50% near-zero samples detected
- ✅ `test_overload_detection_clipping` - Samples >95% full-scale detected
- ✅ `test_below_threshold_no_false_positives` - Small variations ignored
- ✅ `test_ring_buffer_wraps_after_32_events` - Ring buffer wraps correctly
- ✅ `test_per_adc_and_per_channel_tracking` - ADC index and channel tracked
- ✅ `test_glitch_type_enum_to_string` - Enum to string conversion

### Group 3: Timing Histogram (5 tests)
- ✅ `test_timing_buckets_increment_correctly` - 0-19ms buckets (1ms resolution)
- ✅ `test_timing_overflow_bucket_over_20ms` - Overflow counter for >20ms
- ✅ `test_timing_average_latency_calculation` - Running average computed
- ✅ `test_timing_max_latency_tracking` - Maximum latency tracked
- ✅ `test_timing_sample_count_increments` - Sample counter increments

### Group 4: Event Correlation (6 tests)
- ✅ `test_dsp_swap_correlation_within_100ms` - DSP swap events correlate with glitches
- ✅ `test_wifi_event_correlation_within_100ms` - WiFi events correlate
- ✅ `test_mqtt_event_correlation_within_100ms` - MQTT events correlate
- ✅ `test_event_over_100ms_no_correlation` - Events >100ms don't correlate
- ✅ `test_multiple_events_correlate_correctly` - Multiple events tracked
- ✅ `test_correlation_flags_clear_when_no_recent_glitches` - Flags clear when appropriate

### Group 5: Memory Monitoring (3 tests)
- ✅ `test_memory_snapshots_ring_buffer` - 60-second ring buffer
- ✅ `test_memory_write_position_wraps_correctly` - Write position wraps at 60
- ✅ `test_memory_timestamps_increment` - Timestamps increment correctly

### Group 6: Statistics & Reset (3 tests)
- ✅ `test_reset_clears_all_counters` - Reset clears all statistics
- ✅ `test_reset_preserves_settings` - Reset preserves enabled/threshold settings
- ✅ `test_last_minute_counter_decay` - Last-minute counter decays after 60s

### Group 7: Integration (3 tests)
- ✅ `test_disabled_state_no_processing_overhead` - Disabled state is no-op
- ✅ `test_real_audio_buffer_scan` - Realistic sine wave scan (no false positives)
- ✅ `test_full_diagnostics_struct_retrieval` - Full diagnostics struct accessible

## Running Tests

```bash
# Run all audio quality tests
pio test -e native -f test_audio_quality

# Verbose output
pio test -e native -f test_audio_quality -v

# Run all native tests (includes this module)
pio test -e native
```

## Implementation Notes

### Glitch Detection Algorithms

**Discontinuity Detection:**
- Detects sample-to-sample jumps exceeding threshold * full-scale
- Uses floating-point comparison to avoid overflow
- Highest priority (checked first)

**DC Offset Detection:**
- Calculates average (DC) level per channel
- Triggers when DC exceeds threshold
- Uses double precision to avoid accumulation errors

**Dropout Detection:**
- Counts samples below 0.01% of full-scale (~214k for int32)
- Triggers when >50% samples near zero AND >10% non-zero
- Avoids false positives on all-zero buffers

**Overload Detection:**
- Detects samples >95% of full-scale (clipping)
- Per-channel and per-sample tracking

### Detection Priority Order

1. **Discontinuity** (highest) - Sample-to-sample jumps
2. **Overload** - Clipping/saturation
3. **DC Offset** - Sustained DC component
4. **Dropout** (lowest) - Silence/loss of signal

Only the first matching glitch type is reported per scan.

### Event Correlation

System events are tracked with 100ms correlation window:
- `dsp_swap` - DSP configuration swap
- `wifi_connected` / `wifi_disconnected` - WiFi state changes
- `mqtt_connected` / `mqtt_disconnected` - MQTT state changes

Glitches occurring within 100ms of an event have correlation flags set.

### Memory Monitoring

60-second rolling window of heap snapshots (1 per second):
- `freeHeap` - ESP.getFreeHeap()
- `maxAllocHeap` - ESP.getMaxAllocHeap()
- `freePsram` - ESP.getFreePsram()

## API Contract

The test suite defines the complete API contract for the audio quality module:

```c
void audio_quality_init(void);
void audio_quality_enable(bool enabled);
bool audio_quality_is_enabled(void);
void audio_quality_set_threshold(float threshold);
float audio_quality_get_threshold(void);
void audio_quality_scan_buffer(int adcIndex, const int32_t *buf, int stereoFrames, unsigned long latencyUs);
void audio_quality_mark_event(const char *eventName);
const AudioQualityDiag* audio_quality_get_diagnostics(void);
void audio_quality_reset_stats(void);
void audio_quality_update_memory(void);
const char* audio_quality_glitch_type_to_string(GlitchType type);
```

## Data Structures

```c
typedef enum {
    GLITCH_TYPE_NONE = 0,
    GLITCH_TYPE_DISCONTINUITY = 1,
    GLITCH_TYPE_DC_OFFSET = 2,
    GLITCH_TYPE_DROPOUT = 3,
    GLITCH_TYPE_OVERLOAD = 4
} GlitchType;

typedef struct {
    unsigned long timestamp;
    GlitchType type;
    uint8_t adcIndex;           // 0 or 1
    uint8_t channel;            // 0=left, 1=right
    float magnitude;            // 0.0-1.0 normalized
    uint16_t sampleIndex;
    EventCorrelation correlation;
} GlitchEvent;

typedef struct {
    GlitchEvent events[32];     // Ring buffer
    uint8_t writePos;
    uint32_t totalCount;
    uint32_t lastMinuteCount;
} GlitchHistory;

typedef struct {
    uint32_t buckets[20];       // 0-19ms, 1ms per bucket
    uint32_t overflowCount;     // >20ms
    uint32_t sampleCount;
    uint32_t avgLatencyUs;
    uint32_t maxLatencyUs;
} TimingHistogram;

typedef struct {
    MemorySnapshot snapshots[60]; // 60-second ring buffer
    uint8_t writePos;
} MemoryHistory;

typedef struct {
    GlitchHistory glitchHistory;
    TimingHistogram timingHistogram;
    MemoryHistory memoryHistory;
} AudioQualityDiag;
```

## Test-Driven Development

These tests were written **before** the production implementation to define the API contract and expected behavior. This is a TDD (Test-Driven Development) approach:

1. ✅ **Tests written** - Defines complete API and behavior
2. ⏳ **Implementation** - Write production code in `src/audio_quality.h/.cpp`
3. ⏳ **Validation** - Tests verify production code matches contract

Current stub implementation in `test/test_audio_quality/audio_quality.cpp` is for testing only. Production code will be in `src/`.

## Files

- `test_audio_quality.cpp` - Main test file (33 tests)
- `audio_quality.h` - API specification (test-local version)
- `audio_quality.cpp` - Stub implementation (for tests to compile)
- `README.md` - This documentation

## Next Steps

1. Move header to `src/audio_quality.h`
2. Implement production code in `src/audio_quality.cpp`
3. Add ESP32-specific memory monitoring (ESP.getFreeHeap(), etc.)
4. Integrate with main loop (call `audio_quality_scan_buffer()` from audio task)
5. Add WebSocket/MQTT/REST API endpoints for diagnostics
6. Add GUI screen for real-time glitch monitoring
