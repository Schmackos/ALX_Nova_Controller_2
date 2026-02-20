#ifndef I2S_AUDIO_H
#define I2S_AUDIO_H

#include <stdint.h>
#include <stddef.h>  // size_t

#ifndef NUM_AUDIO_ADCS
#define NUM_AUDIO_ADCS 2
#endif

#ifndef NUM_AUDIO_INPUTS
#define NUM_AUDIO_INPUTS 3  // ADC1 + ADC2 + USB Audio
#endif

// ===== VU Meter Constants =====
static const float VU_ATTACK_MS = 300.0f;   // Industry-standard VU attack
static const float VU_DECAY_MS = 300.0f;    // Fast digital VU release
static const float PEAK_HOLD_MS = 2000.0f;  // Peak hold duration
static const float PEAK_DECAY_AFTER_HOLD_MS = 300.0f; // Decay after hold expires

// ===== ADC Clock Sync Constants =====
static const int    ADC_SYNC_CHECK_FRAMES     = 64;     // Window for cross-correlation
static const int    ADC_SYNC_SEARCH_RANGE     = 8;      // Search ±8 samples lag
static const float  ADC_SYNC_OFFSET_THRESHOLD = 2.0f;   // inSync if |offset| <= 2 samples
static const uint32_t ADC_SYNC_CHECK_INTERVAL_MS = 5000;

// ===== ADC Clock Sync Diagnostics =====
struct AdcSyncDiag {
    float phaseOffsetSamples = 0.0f;  // Measured delay ADC1->ADC2 (samples)
    float phaseOffsetUs = 0.0f;       // Same in microseconds
    float correlationPeak = 0.0f;     // Peak cross-corr value (0.0-1.0 normalized)
    bool  inSync = true;              // true if |offset| <= ADC_SYNC_OFFSET_THRESHOLD
    unsigned long lastCheckMs = 0;
    uint32_t checkCount = 0;
    uint32_t outOfSyncCount = 0;
};

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
    AdcAnalysis adc[NUM_AUDIO_INPUTS]; // Per-input data (ADC1, ADC2, USB)
    float dBFS;            // -96 to 0 (overall max across all inputs)
    bool  signalDetected;  // any input above threshold
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
    AdcDiagnostics adc[NUM_AUDIO_INPUTS]; // Per-input diagnostics (ADC1, ADC2, USB)
    bool sigGenActive = false;
    int numAdcsDetected = 1;   // How many I2S ADCs are producing data
    int numInputsDetected = 1; // How many audio inputs total (ADCs + USB)

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

// Tear down / restore I2S drivers to free ~16KB DMA buffers during OTA
void i2s_audio_uninstall_drivers();
void i2s_audio_reinstall_drivers();

// I2S TX (DAC full-duplex) management — called by dac_hal.cpp.
// i2s_audio_enable_tx() reinstalls I2S0 in TX+RX mode (pauses audio task).
// i2s_audio_disable_tx() reverts I2S0 to RX-only mode.
// i2s_audio_write_tx() writes audio samples to the TX channel.
// All three are no-ops on NATIVE_TEST.
bool i2s_audio_enable_tx(uint32_t sampleRate);
void i2s_audio_disable_tx();
void i2s_audio_write_tx(const void* buf, size_t bytes, size_t* bytes_written, uint32_t timeout_ms);

// Waveform: returns true if a new 256-point snapshot is available
// out must point to WAVEFORM_BUFFER_SIZE bytes
// inputIndex: 0 = ADC1 (default), 1 = ADC2, 2 = USB
bool i2s_audio_get_waveform(uint8_t *out, int inputIndex = 0);

// Spectrum: returns true if new FFT data is available
// bands must point to SPECTRUM_BANDS floats (0.0-1.0 normalized)
// dominant_freq receives the dominant frequency in Hz
// inputIndex: 0 = ADC1 (default), 1 = ADC2, 2 = USB
bool i2s_audio_get_spectrum(float *bands, float *dominant_freq, int inputIndex = 0);

// Returns number of ADCs currently detected and producing data
int i2s_audio_get_num_adcs();

// Returns current ADC clock sync diagnostics (thread-safe copy)
AdcSyncDiag i2s_audio_get_sync_diag();

// Pure function — testable without hardware
// adc1_samples and adc2_samples: left-channel only, normalized float [-1,1], length=frames
// Returns filled AdcSyncDiag (does not touch global state)
AdcSyncDiag compute_adc_sync_diag(const float* adc1_samples, const float* adc2_samples,
                                   int frames, float sampleRateHz);

// Call from main loop to flush periodic audio/DAC diagnostic logs.
// The audio task sets a flag every 5s; this function does the actual
// Serial output from the main loop context to avoid blocking I2S DMA.
void audio_periodic_dump();

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
