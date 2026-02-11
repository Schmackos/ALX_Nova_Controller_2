#ifndef I2S_AUDIO_H
#define I2S_AUDIO_H

#include <stdint.h>

#ifndef NUM_AUDIO_ADCS
#define NUM_AUDIO_ADCS 2
#endif

// ===== VU Meter Constants =====
static const float VU_ATTACK_MS = 300.0f;   // Industry-standard VU attack
static const float VU_DECAY_MS = 300.0f;    // Fast digital VU release
static const float PEAK_HOLD_MS = 2000.0f;  // Peak hold duration
static const float PEAK_DECAY_AFTER_HOLD_MS = 300.0f; // Decay after hold expires

// ===== Per-ADC Analysis Sub-struct =====
struct AdcAnalysis {
    float rms1;         // 0.0-1.0 (linear)
    float rms2;        // 0.0-1.0 (linear)
    float rmsCombined;     // 0.0-1.0 (linear)
    float vu1;          // 0.0-1.0 (VU-smoothed with attack/decay)
    float vu2;         // 0.0-1.0 (VU-smoothed with attack/decay)
    float vuCombined;      // 0.0-1.0 (VU-smoothed with attack/decay)
    float peak1;        // 0.0-1.0 (instant attack, hold, then decay)
    float peak2;       // 0.0-1.0 (instant attack, hold, then decay)
    float peakCombined;    // 0.0-1.0 (instant attack, hold, then decay)
    float dBFS;            // -96 to 0 (this ADC's combined level)
};

// ===== Audio Analysis Result (shared between I2S task and consumers) =====
struct AudioAnalysis {
    AdcAnalysis adc[NUM_AUDIO_ADCS]; // Per-ADC data
    float dBFS;            // -96 to 0 (overall max across all ADCs)
    bool  signalDetected;  // any ADC above threshold
    unsigned long timestamp;

};

// ===== Audio Health Diagnostics =====
enum AudioHealthStatus {
    AUDIO_OK = 0,
    AUDIO_NO_DATA = 1,
    AUDIO_NOISE_ONLY = 2,
    AUDIO_CLIPPING = 3,
    AUDIO_I2S_ERROR = 4,
    AUDIO_HW_FAULT = 5
};

// Per-ADC diagnostics sub-struct
struct AdcDiagnostics {
    AudioHealthStatus status = AUDIO_OK;
    uint32_t i2sReadErrors = 0;
    uint32_t zeroByteReads = 0;
    uint32_t allZeroBuffers = 0;
    uint32_t consecutiveZeros = 0;
    uint32_t clippedSamples = 0;
    float clipRate = 0.0f;         // EMA clip rate (0.0-1.0), decays when clipping stops
    float noiseFloorDbfs = -96.0f;
    float peakDbfs = -96.0f;
    float dcOffset = 0.0f;           // DC mean as fraction of full-scale (-1.0 to 1.0)
    unsigned long lastNonZeroMs = 0;
    unsigned long lastReadMs = 0;
    uint32_t totalBuffersRead = 0;
    uint32_t i2sRecoveries = 0;      // I2S driver restart count (timeout recovery)
};

struct AudioDiagnostics {
    AdcDiagnostics adc[NUM_AUDIO_ADCS];
    bool sigGenActive = false;
    int numAdcsDetected = 1;  // How many ADCs are actually producing data

};

// ===== Waveform & FFT Constants =====
static const int WAVEFORM_BUFFER_SIZE = 256;
static const int FFT_SIZE = 1024;
static const int SPECTRUM_BANDS = 16;

// ===== I2S Static Configuration (for diagnostics display) =====
struct I2sAdcConfig {
    bool isMaster;
    uint32_t sampleRate;
    int bitsPerSample;           // 32
    const char *channelFormat;   // "Stereo R/L"
    int dmaBufCount;
    int dmaBufLen;
    bool apllEnabled;
    uint32_t mclkHz;             // sampleRate * 256, or 0 for slave
    const char *commFormat;      // "Standard I2S"
};
struct I2sStaticConfig {
    I2sAdcConfig adc[NUM_AUDIO_ADCS];
};
I2sStaticConfig i2s_audio_get_static_config();

// ===== Public API =====
void i2s_audio_init();
AudioAnalysis i2s_audio_get_analysis();
AudioDiagnostics i2s_audio_get_diagnostics();
bool i2s_audio_set_sample_rate(uint32_t rate);

// Waveform: returns true if a new 256-point snapshot is available
// out must point to WAVEFORM_BUFFER_SIZE bytes
// adcIndex: 0 = ADC1 (default), 1 = ADC2
bool i2s_audio_get_waveform(uint8_t *out, int adcIndex = 0);

// Spectrum: returns true if new FFT data is available
// bands must point to SPECTRUM_BANDS floats (0.0-1.0 normalized)
// dominant_freq receives the dominant frequency in Hz
// adcIndex: 0 = ADC1 (default), 1 = ADC2
bool i2s_audio_get_spectrum(float *bands, float *dominant_freq, int adcIndex = 0);

// Returns number of ADCs currently detected and producing data
int i2s_audio_get_num_adcs();

// ===== Pure functions exposed for unit testing =====
AudioHealthStatus audio_derive_health_status(const AdcDiagnostics &diag);
AudioHealthStatus audio_derive_health_status(const AudioDiagnostics &diag); // Legacy overload (uses adc[0])

float audio_compute_rms(const int32_t *samples, int count, int channel, int channels);
float audio_rms_to_dbfs(float rms);
float audio_migrate_voltage_threshold(float stored_value);
float audio_rms_to_vrms(float rms_linear, float vref);
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
