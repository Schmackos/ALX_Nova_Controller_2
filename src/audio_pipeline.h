#ifndef AUDIO_PIPELINE_H
#define AUDIO_PIPELINE_H

#include <stdint.h>
#include <stdbool.h>

// ===== Dimensions (override via config.h build flags) =====
#ifndef AUDIO_PIPELINE_MAX_INPUTS
#define AUDIO_PIPELINE_MAX_INPUTS  4
#endif
#ifndef AUDIO_PIPELINE_MAX_OUTPUTS
#define AUDIO_PIPELINE_MAX_OUTPUTS 4
#endif
#ifndef AUDIO_PIPELINE_MATRIX_SIZE
#define AUDIO_PIPELINE_MATRIX_SIZE 8
#endif

// ===== Input Lane Identifiers =====
enum AudioInputType : uint8_t {
    AUDIO_INPUT_ADC1   = 0,
    AUDIO_INPUT_ADC2   = 1,
    AUDIO_INPUT_SIGGEN = 2,
    AUDIO_INPUT_USB    = 3,
    AUDIO_INPUT_COUNT  = 4
};

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

// Diagnostic — call from main-loop context only (not from audio task)
void audio_pipeline_dump_raw_diag();

// Register an AudioInputSource for a pipeline lane (used by USB, future dynamic inputs)
struct AudioInputSource;  // Forward declaration
void audio_pipeline_register_source(int lane, const AudioInputSource *src);

// Get per-lane VU metering (dBFS). Returns -90.0f if lane has no registered source.
float audio_pipeline_get_lane_vu_l(int lane);
float audio_pipeline_get_lane_vu_r(int lane);

// Output sink registration (called from dac_hal.cpp after driver init)
struct AudioOutputSink;  // Forward declaration
void audio_pipeline_register_sink(const AudioOutputSink *sink);
void audio_pipeline_clear_sinks();   // Remove all sinks — caller must set audioPaused=true first
int  audio_pipeline_get_sink_count();
const AudioOutputSink* audio_pipeline_get_sink(int idx);

// Matrix persistence
void audio_pipeline_save_matrix();
void audio_pipeline_load_matrix();

#endif // AUDIO_PIPELINE_H
