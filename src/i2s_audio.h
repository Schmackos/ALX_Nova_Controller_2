#ifndef I2S_AUDIO_H
#define I2S_AUDIO_H

#include <stdint.h>
#include <stddef.h>  // size_t

#ifndef AUDIO_PIPELINE_MAX_INPUTS
#define AUDIO_PIPELINE_MAX_INPUTS 8
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
    AdcAnalysis adc[AUDIO_PIPELINE_MAX_INPUTS]; // Per-ADC data
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
    AdcDiagnostics adc[AUDIO_PIPELINE_MAX_INPUTS];
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
    bool pllEnabled;
    uint32_t mclkHz;             // sampleRate * 256, or 0 for slave
    const char *commFormat;      // "Standard I2S"
};
struct I2sStaticConfig {
    I2sAdcConfig adc[AUDIO_PIPELINE_MAX_INPUTS];
};
I2sStaticConfig i2s_audio_get_static_config();

// ===== Public API =====
void i2s_audio_init();
// Create I2S channels — MUST be called from Core 1 (audio_pipeline_task) so the
// DMA ISR is pinned to Core 1, isolated from WiFi interrupts on Core 0.
void i2s_audio_init_channels();
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

// Call from main loop to flush periodic audio/DAC diagnostic logs.
// The audio task sets a flag every 5s; this function does the actual
// Serial output from the main loop context to avoid blocking I2S DMA.
void audio_periodic_dump();

// Called by the audio pipeline task every 5s to schedule a dump.
// Safe to call from any FreeRTOS task — sets a volatile flag only.
void i2s_audio_request_dump();

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

// ===== ADC Read API (used by audio_pipeline) =====
#ifndef NATIVE_TEST
bool i2s_audio_read_adc1(void *buf, size_t size, size_t *bytes_read, uint32_t timeout_ms);
bool i2s_audio_read_adc2(void *buf, size_t size, size_t *bytes_read, uint32_t timeout_ms);
bool i2s_audio_adc2_ok();
// Simple dBFS-only update (kept for backward compatibility).
void i2s_audio_update_analysis_dbfs(float dbfs_adc1);
// Full metering update — RMS/VU/peak/dBFS computed by audio_pipeline per buffer.
// Uses same volatile-cast pattern as i2s_audio_get_analysis().
void i2s_audio_update_analysis_metering(const AdcAnalysis &adc0);
// Waveform + FFT accumulation — called once per DMA buffer from audio_pipeline_task.
// rawLJ: left-justified int32 stereo interleaved (pre-float-conversion ADC data).
// frames: DMA_BUF_LEN stereo frames. adcIndex: 0=ADC1, 1=ADC2.
void i2s_audio_push_waveform_fft(const int32_t *rawLJ, int frames, int adcIndex);
#else
inline bool i2s_audio_read_adc1(void*, size_t, size_t* br, uint32_t) { if (br) *br = 0; return false; }
inline bool i2s_audio_read_adc2(void*, size_t, size_t* br, uint32_t) { if (br) *br = 0; return false; }
inline bool i2s_audio_adc2_ok() { return false; }
inline void i2s_audio_update_analysis_dbfs(float) {}
inline void i2s_audio_update_analysis_metering(const AdcAnalysis &) {}
inline void i2s_audio_push_waveform_fft(const int32_t *, int, int) {}
#endif

// ===== Port-indexed read/active callbacks for AudioInputSource (Phase 1 ADC componentization) =====
// These are pre-baked thunks: port is encoded in function identity, not parameter.
// HalPcm1808::init() selects the appropriate thunk based on i2sPort config.
#ifdef NATIVE_TEST
// Stub implementations for native tests (no actual I2S reads)
inline uint32_t i2s_audio_port0_read(int32_t* dst, uint32_t frames) { return 0; }
inline uint32_t i2s_audio_port1_read(int32_t* dst, uint32_t frames) { return 0; }
inline uint32_t i2s_audio_port2_read(int32_t* dst, uint32_t frames) { return 0; }
inline bool i2s_audio_port0_active(void) { return false; }
inline bool i2s_audio_port1_active(void) { return false; }
inline bool i2s_audio_port2_active(void) { return false; }
inline uint32_t i2s_audio_get_sample_rate(void) { return 48000; }
#else
// Real implementations in i2s_audio.cpp
uint32_t i2s_audio_port0_read(int32_t* dst, uint32_t frames);
uint32_t i2s_audio_port1_read(int32_t* dst, uint32_t frames);
uint32_t i2s_audio_port2_read(int32_t* dst, uint32_t frames);
bool i2s_audio_port0_active(void);
bool i2s_audio_port1_active(void);
bool i2s_audio_port2_active(void);
uint32_t i2s_audio_get_sample_rate(void);
#endif

// Generic port-indexed dispatch (not used by Phase 1, but useful for future multi-ADC)
uint32_t i2s_audio_read_port(int port, int32_t *dst, uint32_t frames);
bool     i2s_audio_is_port_active(int port);

#ifdef NATIVE_TEST
// Test hooks: expose per-lane config cache internals for native unit tests
void _test_i2s_cache_set(int lane, const struct HalDeviceConfig* cfg);
const struct HalDeviceConfig* _test_i2s_cache_get(int lane);
bool _test_i2s_cache_valid(int lane);
void _test_i2s_cache_reset();
#endif

// ===== Generic ADC I2S Configuration =====
// Configure an I2S ADC lane with optional HAL device config for pin/port overrides.
// Lane 0: full-duplex (primary ADC + DAC TX), outputs MCLK/BCK/WS clocks.
// Lane 1+: RX-only (secondary ADC), data-in pin only.
// If cfg is NULL, uses board-default pins from config.h.
struct HalDeviceConfig;  // Forward declaration
#ifndef NATIVE_TEST
bool i2s_audio_configure_adc(int lane, const HalDeviceConfig* cfg);
#else
inline bool i2s_audio_configure_adc(int, const HalDeviceConfig*) { return true; }
#endif

// ===== I2S TX Bridge API (used by dac_hal to manage full-duplex on I2S0) =====
// Enable I2S TX full-duplex: tears down RX-only channel, recreates as RX+TX.
// Pauses audio task during reinit. Returns true on success.
#ifndef NATIVE_TEST
#include <hal/gpio_types.h>  // gpio_num_t
bool i2s_audio_enable_tx(uint32_t sample_rate);
void i2s_audio_disable_tx();
void i2s_audio_write(const void *src, size_t size, size_t *bytes_written, uint32_t timeout_ms);

// ES8311 secondary DAC output (I2S2 TX, P4 only)
bool i2s_audio_enable_es8311_tx(uint32_t sample_rate);
void i2s_audio_disable_es8311_tx();
void i2s_audio_write_es8311(const void *src, size_t size, size_t *bytes_written, uint32_t timeout_ms);

// Expansion mezzanine DAC TX (configurable I2S port)
// Enables I2S TX for expansion DAC output. dout_pin comes from HalDeviceConfig.pinData.
// Auto-selects an available I2S port unless overridden via HalDeviceConfig.i2sPort.
bool i2s_audio_enable_expansion_tx(uint32_t sample_rate, gpio_num_t dout_pin);
void i2s_audio_disable_expansion_tx();
void i2s_audio_write_expansion_tx(const void *src, size_t size, size_t *bytes_written, uint32_t timeout_ms);

// Expansion mezzanine ADC RX (I2S2 RX, P4 only)
// Enables I2S2 RX for expansion ADC input. If ES8311 TX is already active on I2S2,
// this creates a full-duplex channel sharing BCK/WS. If not, allocates I2S2 RX-only.
bool i2s_audio_enable_expansion_rx(uint32_t sample_rate, gpio_num_t din_pin);
void i2s_audio_disable_expansion_rx();
bool i2s_audio_expansion_rx_ok();

// TDM (4-slot) variant of the expansion ADC RX path (I2S2 RX, P4 only).
// Used exclusively by HalTdmDeinterleaver to read a raw 4-slot interleaved
// DMA buffer from the ES9843PRO.  The caller requests 'frames' TDM frames;
// the function reads frames × 4 × sizeof(int32_t) bytes and returns the
// number of complete TDM frames placed in dst.
// dst must be at least frames × 4 × sizeof(int32_t) bytes.
// Timeout is fixed at 5 ms (same as port2 stereo path).
uint32_t i2s_audio_port2_tdm_read(int32_t* dst, uint32_t frames);
bool     i2s_audio_port2_tdm_active(void);

// Initialize I2S2 RX in TDM mode for the ES9843PRO 4-channel ADC.
// Allocates/reallocates I2S2 using i2s_channel_init_tdm_mode() on the RX
// direction.  TX (ES8311) is reinitialized in STD mode if it was previously
// active.  Slot mask covers slots 0..(slot_count-1).
// slot_count: 4 for ES9843PRO (CH1-CH4).  Valid range 2-16.
bool i2s_audio_enable_expansion_tdm_rx(uint32_t sample_rate,
                                        gpio_num_t din_pin,
                                        uint8_t    slot_count);
#else
inline bool i2s_audio_enable_tx(uint32_t) { return true; }
inline void i2s_audio_disable_tx() {}
inline void i2s_audio_write(const void*, size_t, size_t* bw, uint32_t) { if (bw) *bw = 0; }

inline bool i2s_audio_enable_es8311_tx(uint32_t) { return false; }
inline void i2s_audio_disable_es8311_tx() {}
inline void i2s_audio_write_es8311(const void*, size_t, size_t* bw, uint32_t) { if (bw) *bw = 0; }

inline bool i2s_audio_enable_expansion_tx(uint32_t, int) { return true; }
inline void i2s_audio_disable_expansion_tx() {}
inline void i2s_audio_write_expansion_tx(const void*, size_t, size_t* bw, uint32_t) { if (bw) *bw = 0; }

inline bool i2s_audio_enable_expansion_rx(uint32_t, int) { return false; }
inline void i2s_audio_disable_expansion_rx() {}
inline bool i2s_audio_expansion_rx_ok() { return false; }

inline uint32_t i2s_audio_port2_tdm_read(int32_t*, uint32_t) { return 0; }
inline bool     i2s_audio_port2_tdm_active(void) { return false; }
inline bool     i2s_audio_enable_expansion_tdm_rx(uint32_t, int, uint8_t) { return true; }
#endif

#endif // I2S_AUDIO_H
