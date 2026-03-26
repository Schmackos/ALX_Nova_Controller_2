#ifndef AUDIO_PIPELINE_H
#define AUDIO_PIPELINE_H

#include <stdint.h>
#include <stdbool.h>

// ===== Dimensions (override via config.h build flags) =====
#ifndef AUDIO_PIPELINE_MAX_INPUTS
#define AUDIO_PIPELINE_MAX_INPUTS  8
#endif
#ifndef AUDIO_PIPELINE_MAX_OUTPUTS
#define AUDIO_PIPELINE_MAX_OUTPUTS 8
#endif
#ifndef AUDIO_PIPELINE_MATRIX_SIZE
#define AUDIO_PIPELINE_MATRIX_SIZE 16
#endif

// ===== Public API =====
void audio_pipeline_init();

// Per-input bypass controls
void audio_pipeline_bypass_input(int lane, bool bypass);
void audio_pipeline_bypass_dsp(int lane, bool bypass);

// Matrix and output bypass
void audio_pipeline_bypass_matrix(bool bypass);
void audio_pipeline_bypass_output(bool bypass);

// Matrix gain setters (linear and dB)
void audio_pipeline_set_matrix_gain(int out_ch, int in_ch, float gain_linear);
void audio_pipeline_set_matrix_gain_db(int out_ch, int in_ch, float gain_db);

// Matrix gain getter and bypass state
float audio_pipeline_get_matrix_gain(int out_ch, int in_ch);
bool  audio_pipeline_is_matrix_bypass();

// Called from dsp_swap_config() before _swapRequested is set — arms the
// PSRAM hold buffer so pipeline_write_output() uses last good frame during the swap gap
#ifdef NATIVE_TEST
inline void audio_pipeline_notify_dsp_swap() {}  // No-op in native test (single-threaded)
#else
void audio_pipeline_notify_dsp_swap();
#endif

// ===== Pipeline Timing Metrics =====
// Updated every frame by the audio pipeline task (Core 1).
// Read from main-loop context via audio_pipeline_get_timing() — values are
// snap-read uint32/float so no synchronisation primitive is needed on ESP32-P4.
struct PipelineTimingMetrics {
    uint32_t totalFrameUs;    // Matrix+outputDSP+sink time (backward compat, us)
    uint32_t matrixMixUs;     // Matrix mixing stage time (us)
    uint32_t outputDspUs;     // Output DSP stage time (us)
    float    totalCpuPercent; // CPU load based on totalFrameUs (0-100 %)
    // Per-stage breakdown (added in foundation hardening)
    uint32_t inputReadUs;     // All-lane I2S read time (us)
    uint32_t perInputDspUs;   // Per-input DSP processing time (us)
    uint32_t sinkWriteUs;     // All-sink write time (us)
    uint32_t totalE2eUs;      // Full end-to-end: input read through sink write (us)
};

PipelineTimingMetrics audio_pipeline_get_timing();

// Diagnostic — call from main-loop context only (not from audio task)
void audio_pipeline_dump_raw_diag();

// Slot-indexed input source API — preferred for HAL-managed devices.
// audio_pipeline_set_source()    atomically places a source at a lane index.
// audio_pipeline_remove_source() atomically clears a lane; no-ops if empty.
struct AudioInputSource;  // Forward declaration
bool audio_pipeline_set_source(int lane, const AudioInputSource *src);
void audio_pipeline_remove_source(int lane);

// DEPRECATED: alias for audio_pipeline_set_source() — use set_source for new code.
void audio_pipeline_register_source(int lane, const AudioInputSource *src);

// Read-only accessor for a registered input source. Returns NULL if lane is
// out of range or has no source registered (read callback is NULL).
const AudioInputSource* audio_pipeline_get_source(int lane);

// Set pre-matrix gain on a registered input source (linear scale, applied per-frame by pipeline).
// No-op if lane is out of range or has no source registered.
void audio_pipeline_set_source_gain(int lane, float gainLinear);

// Get per-lane VU metering (dBFS). Returns -90.0f if lane has no registered source.
float audio_pipeline_get_lane_vu_l(int lane);
float audio_pipeline_get_lane_vu_r(int lane);

// Output sink registration (called from dac_hal.cpp after driver init)
struct AudioOutputSink;  // Forward declaration
void audio_pipeline_register_sink(const AudioOutputSink *sink);
void audio_pipeline_clear_sinks();   // Remove all sinks — caller must call audio_pipeline_request_pause() first
int  audio_pipeline_get_sink_count();
const AudioOutputSink* audio_pipeline_get_sink(int idx);

// Slot-indexed sink API — preferred for HAL-managed devices.
// audio_pipeline_set_sink()    places a sink at a fixed slot index using lock-free atomic sentinel pattern.
// audio_pipeline_remove_sink() clears a slot via atomic sentinel null; no-ops if already empty.
// _sinkCount is updated after each call to reflect the highest occupied slot + 1,
// keeping backward compatibility with legacy _sinkCount-based iteration.
bool audio_pipeline_set_sink(int slot, const AudioOutputSink *sink);
void audio_pipeline_remove_sink(int slot);

// Atomic mute control on sink structs. Single aligned bool write — atomic on RISC-V.
// Bounds-checked: slots >= AUDIO_OUT_MAX_SINKS are ignored silently.
void audio_pipeline_set_sink_muted(uint8_t slot, bool muted);
bool audio_pipeline_is_sink_muted(uint8_t slot);

// Software volume gain on sink structs (0.0-1.0, log-perceptual curve).
// Single aligned float write — atomic on ESP32-P4 RISC-V, no scheduler suspend needed.
// Bounds-checked: slots >= AUDIO_OUT_MAX_SINKS are ignored silently.
void  audio_pipeline_set_sink_volume(uint8_t slot, float gain);
float audio_pipeline_get_sink_volume(uint8_t slot);

// Matrix persistence
void audio_pipeline_save_matrix();
void audio_pipeline_load_matrix();

// Format negotiation — call from main-loop context (not audio task).
// Reads each active source's getSampleRate(), compares against registered
// sink sampleRates, and updates appState.audio.rateMismatch + laneSampleRates.
// Emits DIAG_AUDIO_RATE_MISMATCH once on first detection; clears on resolution.
// Returns true if any mismatch is currently active.
bool audio_pipeline_check_format();

// ASRC lane configuration — call from main-loop context when a rate mismatch is detected.
// Sets the input/output sample rate ratio for a specific lane's ASRC engine.
// srcRate == dstRate deactivates ASRC for that lane (passthrough, zero overhead).
// srcRate == 0 deactivates ASRC for all lanes (e.g., on source removal).
// Also updates appState.audio.laneSrcActive[] for WS broadcast.
void audio_pipeline_set_lane_src(int lane, uint32_t srcRate, uint32_t dstRate);

// Cross-core audio pause/resume protocol.
// Callers that teardown/reinstall I2S drivers MUST use these instead of
// directly setting appState.audio.paused.
//
// request_pause() sets paused=true, then waits for the audio task to
// acknowledge via the binary semaphore (taskPausedAck).  This guarantees the
// audio task has exited i2s_read() before the caller proceeds with driver
// teardown.  Returns true if the audio task acknowledged within timeout_ms,
// false on timeout (caller may LOG_W but should proceed).
//
// resume() clears the paused flag, allowing the audio task to run again.
#ifdef NATIVE_TEST
inline bool audio_pipeline_request_pause(uint32_t timeout_ms = 50) { (void)timeout_ms; return true; }
inline void audio_pipeline_resume() {}
#else
bool audio_pipeline_request_pause(uint32_t timeout_ms = 50);
void audio_pipeline_resume();
#endif

#endif // AUDIO_PIPELINE_H
