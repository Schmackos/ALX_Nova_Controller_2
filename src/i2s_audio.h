#ifndef I2S_AUDIO_H
#define I2S_AUDIO_H

#include <stdint.h>

// ===== VU Meter Constants =====
static const float VU_ATTACK_MS = 300.0f;   // Industry-standard VU attack
static const float VU_DECAY_MS = 650.0f;    // Industry-standard VU release
static const float PEAK_HOLD_MS = 2000.0f;  // Peak hold duration
static const float PEAK_DECAY_AFTER_HOLD_MS = 300.0f; // Decay after hold expires

// ===== Audio Analysis Result (shared between I2S task and consumers) =====
struct AudioAnalysis {
    float rmsLeft;         // 0.0-1.0 (linear)
    float rmsRight;        // 0.0-1.0 (linear)
    float rmsCombined;     // 0.0-1.0 (linear)
    float vuLeft;          // 0.0-1.0 (VU-smoothed with attack/decay)
    float vuRight;         // 0.0-1.0 (VU-smoothed with attack/decay)
    float vuCombined;      // 0.0-1.0 (VU-smoothed with attack/decay)
    float peakLeft;        // 0.0-1.0 (instant attack, hold, then decay)
    float peakRight;       // 0.0-1.0 (instant attack, hold, then decay)
    float peakCombined;    // 0.0-1.0 (instant attack, hold, then decay)
    float dBFS;            // -96 to 0 (combined level)
    bool  signalDetected;  // dBFS >= threshold
    unsigned long timestamp;
};

// ===== Waveform & FFT Constants =====
static const int WAVEFORM_BUFFER_SIZE = 256;
static const int FFT_SIZE = 1024;
static const int SPECTRUM_BANDS = 16;

// ===== Public API =====
void i2s_audio_init();
AudioAnalysis i2s_audio_get_analysis();
bool i2s_audio_set_sample_rate(uint32_t rate);

// Waveform: returns true if a new 256-point snapshot is available
// out must point to WAVEFORM_BUFFER_SIZE bytes
bool i2s_audio_get_waveform(uint8_t *out);

// Spectrum: returns true if new FFT data is available
// bands must point to SPECTRUM_BANDS floats (0.0-1.0 normalized)
// dominant_freq receives the dominant frequency in Hz
bool i2s_audio_get_spectrum(float *bands, float *dominant_freq);

// ===== Pure functions exposed for unit testing =====
float audio_compute_rms(const int32_t *samples, int count, int channel, int channels);
float audio_rms_to_dbfs(float rms);
float audio_migrate_voltage_threshold(float stored_value);
bool audio_validate_sample_rate(uint32_t rate);
int32_t audio_parse_24bit_sample(int32_t raw_i2s_word);

// VU metering: exponential smoothing with attack/decay ballistics
float audio_vu_update(float current_vu, float new_rms, float dt_ms);

// Peak hold: instant attack, 2s hold, then 300ms decay
// hold_start_ms is updated in-place when a new peak is captured
float audio_peak_hold_update(float current_peak, float new_value,
                             unsigned long *hold_start_ms, unsigned long now_ms,
                             float dt_ms);

// Quantize normalized float (-1.0 to +1.0) to uint8 (0=min, 128=center, 255=max)
uint8_t audio_quantize_sample(float normalized);

// Downsample stereo I2S frames into a uint8 waveform buffer
// Each output bin captures the signed-peak (by absolute value) of its source frames
void audio_downsample_waveform(const int32_t *stereo_frames, int frame_count,
                               uint8_t *out, int out_size);

// Aggregate FFT magnitude bins into musically-spaced spectrum bands
// magnitudes: FFT output after complexToMagnitude (first half: bins 0..fft_size/2-1)
// bands: output array of SPECTRUM_BANDS floats (0.0-1.0 normalized)
void audio_aggregate_fft_bands(const float *magnitudes, int fft_size,
                               float sample_rate, float *bands, int num_bands);

#endif // I2S_AUDIO_H
