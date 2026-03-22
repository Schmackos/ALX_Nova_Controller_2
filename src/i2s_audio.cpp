#include "i2s_audio.h"
#include "audio_pipeline.h"
#include "audio_input_source.h"
#include "app_state.h"
#include "config.h"
#include "debug_serial.h"
#ifdef DSP_ENABLED
#include "dsp_pipeline.h"
#endif
#ifdef DAC_ENABLED
#include "dac_hal.h"
#endif
#ifndef NATIVE_TEST
#include "dsps_fft4r.h"
#include "dsps_wind.h"
#include "dsps_wind_blackman.h"
#include "dsps_wind_blackman_harris.h"
#include "dsps_wind_blackman_nuttall.h"
#include "dsps_wind_nuttall.h"
#include "dsps_wind_flat_top.h"
#include "dsps_snr.h"
#include "dsps_sfdr.h"
#else
#include "dsps_wind.h"
#include "dsps_snr.h"
#include "dsps_sfdr.h"
#include <arduinoFFT.h>
#endif
#include <cmath>
#include <cstring>
#include "hal/hal_types.h"  // HalDeviceConfig — plain struct, no platform dependencies

#ifndef NATIVE_TEST
#include <driver/i2s_std.h>
#include <driver/i2s_tdm.h>
#include <driver/gpio.h>
#include <esp_task_wdt.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "hal/hal_settings.h"
#include "hal/hal_device_manager.h"
#endif
#include "psram_alloc.h"

// ===== Constants =====
static const int DMA_BUF_COUNT = I2S_DMA_BUF_COUNT;
static const int DMA_BUF_LEN = I2S_DMA_BUF_LEN;
static const float DBFS_FLOOR = -96.0f;

// ===== Clip Rate EMA Constants =====
static const float CLIP_RATE_ALPHA = 0.1f;      // EMA smoothing factor
static const float CLIP_RATE_HW_FAULT = 0.3f;   // >30% clipping = hardware fault
static const float CLIP_RATE_CLIPPING = 0.001f;  // >0.1% clipping = signal too hot

// ===== Shared state (written by I2S task, read by main loop) =====
static volatile AudioAnalysis _analysis = {};
static volatile bool _analysisReady = false;
static AudioDiagnostics _diagnostics = {};

// Periodic dump: audio task sets flag, main loop does the actual LOG calls
// (Serial.print at 9600-115200 baud blocks for tens-hundreds of ms, starving I2S DMA)
static volatile bool _dumpReady = false;

// Per-lane HAL config cache — populated by i2s_audio_configure_adc(), used by
// i2s_audio_set_sample_rate() and i2s_audio_enable_tx() to survive re-init with correct pins.
static HalDeviceConfig _cachedAdcCfg[3] = {};
static bool _cachedAdcCfgValid[3] = {false, false, false};

// ===== Pure computation functions (testable without hardware) =====

float audio_compute_rms(const int32_t *samples, int count, int channel, int channels) {
    if (count <= 0) return 0.0f;

    float sum_sq = 0.0f;
    int sample_count = 0;
    const float MAX_24BIT = 8388607.0f; // 2^23 - 1

    for (int i = channel; i < count * channels; i += channels) {
        int32_t raw = samples[i];
        int32_t parsed = audio_parse_24bit_sample(raw);
        float normalized = (float)parsed / MAX_24BIT;
        sum_sq += normalized * normalized;
        sample_count++;
    }

    if (sample_count == 0) return 0.0f;
    return sqrtf(sum_sq / sample_count);
}

float audio_rms_to_dbfs(float rms) {
    if (rms <= 0.0f) return DBFS_FLOOR;
    float db = 20.0f * log10f(rms);
    if (db < DBFS_FLOOR) return DBFS_FLOOR;
    return db;
}

float audio_rms_to_vrms(float rms_linear, float vref) {
    if (rms_linear < 0.0f) rms_linear = 0.0f;
    if (rms_linear > 1.0f) rms_linear = 1.0f;
    return rms_linear * vref;
}

float audio_migrate_voltage_threshold(float stored_value) {
    // If value > 0, it's old voltage format (0.1-3.3V) — convert to dBFS
    // If value <= 0, it's already dBFS — keep as-is
    if (stored_value > 0.0f) {
        float ratio = stored_value / 3.3f;
        if (ratio <= 0.0f) return DBFS_FLOOR;
        if (ratio >= 1.0f) return 0.0f;
        return 20.0f * log10f(ratio);
    }
    return stored_value;
}

bool audio_validate_sample_rate(uint32_t rate) {
    return (rate == 16000 || rate == 44100 || rate == 48000);
}

int32_t audio_parse_24bit_sample(int32_t raw_i2s_word) {
    // PCM1808 sends 24-bit data left-justified in 32-bit frame
    // Bits [31:8] contain audio data, bits [7:0] are zero
    // Shift right by 8 to get 24-bit signed value
    return raw_i2s_word >> 8;
}

float audio_vu_update(float current_vu, float new_rms, float dt_ms) {
    if (dt_ms <= 0.0f) return current_vu;
    float tau = (new_rms > current_vu) ? VU_ATTACK_MS : VU_DECAY_MS;
    float coeff = 1.0f - expf(-dt_ms / tau);
    return current_vu + coeff * (new_rms - current_vu);
}

float audio_peak_hold_update(float current_peak, float new_value,
                             unsigned long *hold_start_ms, unsigned long now_ms,
                             float dt_ms) {
    // Instant attack: new value exceeds current peak
    if (new_value >= current_peak) {
        *hold_start_ms = now_ms;
        return new_value;
    }

    // Hold period: keep peak unchanged
    unsigned long elapsed = now_ms - *hold_start_ms;
    if (elapsed < (unsigned long)PEAK_HOLD_MS) {
        return current_peak;
    }

    // Decay phase after hold expires
    float coeff = 1.0f - expf(-dt_ms / PEAK_DECAY_AFTER_HOLD_MS);
    float decayed = current_peak * (1.0f - coeff);

    // Don't decay below the current input level
    return (decayed > new_value) ? decayed : new_value;
}

uint8_t audio_quantize_sample(float normalized) {
    if (normalized > 1.0f) normalized = 1.0f;
    if (normalized < -1.0f) normalized = -1.0f;
    int val = (int)((normalized + 1.0f) * 127.5f + 0.5f);
    if (val > 255) val = 255;
    return (uint8_t)val;
}

void audio_downsample_waveform(const int32_t *stereo_frames, int frame_count,
                               uint8_t *out, int out_size) {
    const float MAX_24BIT = 8388607.0f;
    float peaks[WAVEFORM_BUFFER_SIZE];
    int bins = (out_size > WAVEFORM_BUFFER_SIZE) ? WAVEFORM_BUFFER_SIZE : out_size;

    for (int i = 0; i < bins; i++) peaks[i] = 0.0f;

    if (frame_count > 0 && bins > 0) {
        for (int f = 0; f < frame_count; f++) {
            int bin = (int)((long)f * bins / frame_count);
            if (bin >= bins) bin = bins - 1;

            float sL = (float)audio_parse_24bit_sample(stereo_frames[f * 2]) / MAX_24BIT;
            float sR = (float)audio_parse_24bit_sample(stereo_frames[f * 2 + 1]) / MAX_24BIT;
            float combined = (sL + sR) / 2.0f;

            if (fabsf(combined) > fabsf(peaks[bin])) {
                peaks[bin] = combined;
            }
        }
    }

    for (int i = 0; i < bins; i++) {
        out[i] = audio_quantize_sample(peaks[i]);
    }
}

// ===== Spectrum Band Definitions (Hz boundaries) =====
// 16 musically-spaced bands covering 0 Hz - 24 kHz
static const float BAND_EDGES[SPECTRUM_BANDS + 1] = {
    0, 40, 80, 160, 315, 630, 1250, 2500,
    5000, 8000, 10000, 12500, 14000, 16000, 18000, 20000, 24000
};

void audio_aggregate_fft_bands(const float *magnitudes, int fft_size,
                               float sample_rate, float *bands, int num_bands) {
    int half = fft_size / 2;
    float bin_width = sample_rate / (float)fft_size;

    // Find the maximum magnitude for normalization
    float max_mag = 0.0001f; // floor to avoid div-by-zero
    for (int i = 1; i < half; i++) {
        if (magnitudes[i] > max_mag) max_mag = magnitudes[i];
    }

    for (int b = 0; b < num_bands && b < SPECTRUM_BANDS; b++) {
        float low_freq = BAND_EDGES[b];
        float high_freq = BAND_EDGES[b + 1];

        // Map frequency range to bin indices
        int low_bin = (int)(low_freq / bin_width);
        int high_bin = (int)(high_freq / bin_width);
        if (low_bin < 0) low_bin = 0;
        if (high_bin >= half) high_bin = half - 1;

        if (low_bin > high_bin || low_bin >= half) {
            bands[b] = 0.0f; // Band beyond Nyquist
            continue;
        }

        // Average magnitude across bins in this band
        float sum = 0.0f;
        int count = 0;
        for (int i = low_bin; i <= high_bin; i++) {
            sum += magnitudes[i];
            count++;
        }

        bands[b] = (count > 0) ? (sum / count) / max_mag : 0.0f;
        if (bands[b] > 1.0f) bands[b] = 1.0f;
    }
}

// ===== Health status derivation (pure, testable) =====
AudioHealthStatus audio_derive_health_status(const AdcDiagnostics &diag) {
    // I2S bus errors take highest priority
    if (diag.i2sReadErrors > 10) return AUDIO_I2S_ERROR;
    // ADC not sending any data
    if (diag.consecutiveZeros > 100) return AUDIO_NO_DATA;
    // Hardware fault: sustained high clip rate (>30%) = power loss / floating pins
    if (diag.clipRate > CLIP_RATE_HW_FAULT) return AUDIO_HW_FAULT;
    // Signal clipping: occasional clips (recoverable via EMA decay)
    if (diag.clipRate > CLIP_RATE_CLIPPING) return AUDIO_CLIPPING;
    // Thermal noise only (no meaningful audio)
    if (diag.noiseFloorDbfs < -75.0f && diag.noiseFloorDbfs > -96.0f) return AUDIO_NOISE_ONLY;
    return AUDIO_OK;
}

// Legacy overload for backward compatibility
AudioHealthStatus audio_derive_health_status(const AudioDiagnostics &diag) {
    AdcDiagnostics masked = diag.adc[0];
    if (diag.sigGenActive) masked.clipRate = 0.0f; // Mask siggen-induced clipping
    return audio_derive_health_status(masked);
}

// ===== Hardware-dependent code (ESP32 only) =====
#ifndef NATIVE_TEST

// ===== Unified I2S Port State =====
#define I2S_PORT_COUNT 3
#define I2S_MODE_STD   0
#define I2S_MODE_TDM   1
#define I2S_MODE_NONE  255
#define I2S_USER_TX    0x01
#define I2S_USER_RX    0x02

struct I2sPortState {
    i2s_chan_handle_t tx;
    i2s_chan_handle_t rx;
    uint8_t          users;        // Bitmask: I2S_USER_TX=0x01, I2S_USER_RX=0x02
    uint32_t         sampleRate;
    uint8_t          txMode;       // I2S_MODE_STD / I2S_MODE_TDM / I2S_MODE_NONE
    uint8_t          rxMode;
    uint8_t          txTdmSlots;
    uint8_t          rxTdmSlots;
    gpio_num_t       txDoutPin;
    gpio_num_t       rxDinPin;
    gpio_num_t       mclkPin;
    gpio_num_t       bckPin;
    gpio_num_t       lrcPin;
    bool             isClockMaster;
    bool             autoCleared;
};
static I2sPortState _port[I2S_PORT_COUNT] = {};

// Backward-compat macros — map legacy per-port names onto the unified array
#define _rx_handle_adc1 _port[0].rx
#define _tx_handle_adc1 _port[0].tx
#define _rx_handle_adc2 _port[1].rx

static uint32_t _currentSampleRate = DEFAULT_AUDIO_SAMPLE_RATE;
static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;
static int _numAdcsDetected = 1;
static bool _adc2InitOk = false;
static bool _expansionRxOk = false;

// Per-ADC state arrays
static const float MAX_24BIT_F = 8388607.0f;

// VU meter state per ADC
static float _vuL[AUDIO_PIPELINE_MAX_INPUTS] = {};
static float _vuR[AUDIO_PIPELINE_MAX_INPUTS] = {};
static float _vuC[AUDIO_PIPELINE_MAX_INPUTS] = {};

// Peak hold state per ADC
static float _peakL[AUDIO_PIPELINE_MAX_INPUTS] = {};
static float _peakR[AUDIO_PIPELINE_MAX_INPUTS] = {};
static float _peakC[AUDIO_PIPELINE_MAX_INPUTS] = {};
static unsigned long _holdStartL[AUDIO_PIPELINE_MAX_INPUTS] = {};
static unsigned long _holdStartR[AUDIO_PIPELINE_MAX_INPUTS] = {};
static unsigned long _holdStartC[AUDIO_PIPELINE_MAX_INPUTS] = {};

// Waveform accumulation state per ADC — PSRAM-allocated on ESP32, static on native
#ifdef NATIVE_TEST
static float _wfAccum[AUDIO_PIPELINE_MAX_INPUTS][WAVEFORM_BUFFER_SIZE];
static uint8_t _wfOutput[AUDIO_PIPELINE_MAX_INPUTS][WAVEFORM_BUFFER_SIZE];
#else
static float *_wfAccum[AUDIO_PIPELINE_MAX_INPUTS] = {};
static uint8_t *_wfOutput[AUDIO_PIPELINE_MAX_INPUTS] = {};
#endif
static volatile bool _wfReady[AUDIO_PIPELINE_MAX_INPUTS] = {};
static int _wfFramesSeen[AUDIO_PIPELINE_MAX_INPUTS] = {};
static int _wfTargetFrames = 2400; // shared, recalculated from audioUpdateRate

// FFT state per ADC — PSRAM-allocated on ESP32, static on native
#ifdef NATIVE_TEST
static float _fftRing[AUDIO_PIPELINE_MAX_INPUTS][FFT_SIZE];
static float _fftData[FFT_SIZE * 2];
static float _fftWindow[FFT_SIZE];
#else
static float *_fftRing[AUDIO_PIPELINE_MAX_INPUTS] = {};
static float *_fftData = nullptr;
static float *_fftWindow = nullptr;
#endif
static int _fftRingPos[AUDIO_PIPELINE_MAX_INPUTS] = {};
static FftWindowType _currentWindowType = FFT_WINDOW_HANN;
static bool _fftInitialized = false;
static float _spectrumOutput[AUDIO_PIPELINE_MAX_INPUTS][SPECTRUM_BANDS];
static float _dominantFreqOutput[AUDIO_PIPELINE_MAX_INPUTS] = {};
static volatile bool _spectrumReady[AUDIO_PIPELINE_MAX_INPUTS] = {};
static unsigned long _lastFftTime[AUDIO_PIPELINE_MAX_INPUTS] = {};

// Apply the selected FFT window function to the window buffer
static void i2s_audio_apply_window(FftWindowType type) {
    switch (type) {
        case FFT_WINDOW_BLACKMAN:
            dsps_wind_blackman_f32(_fftWindow, FFT_SIZE);
            break;
        case FFT_WINDOW_BLACKMAN_HARRIS:
            dsps_wind_blackman_harris_f32(_fftWindow, FFT_SIZE);
            break;
        case FFT_WINDOW_BLACKMAN_NUTTALL:
            dsps_wind_blackman_nuttall_f32(_fftWindow, FFT_SIZE);
            break;
        case FFT_WINDOW_NUTTALL:
            dsps_wind_nuttall_f32(_fftWindow, FFT_SIZE);
            break;
        case FFT_WINDOW_FLAT_TOP:
            dsps_wind_flat_top_f32(_fftWindow, FFT_SIZE);
            break;
        default: // FFT_WINDOW_HANN
            dsps_wind_hann_f32(_fftWindow, FFT_SIZE);
            break;
    }
    _currentWindowType = type;
}

// Resolve an I2S GPIO pin: use config value if > 0, otherwise use compile-time fallback.
// Convention: HAL config stores -1 for "use board default"; 0 is a strapping pin (never I2S).
static inline gpio_num_t _resolveI2sPin(int8_t cfgValue, int fallback) {
    return (cfgValue > 0) ? (gpio_num_t)cfgValue : (gpio_num_t)fallback;
}

// ===== Port-generic I2S helpers =====
// All helpers validate port < I2S_PORT_COUNT and operate on _port[port].

// Disable + delete one direction of an I2S port.
// tx=true targets the TX handle; tx=false targets the RX handle.
static void _i2s_port_teardown_dir(uint8_t port, bool tx) {
    if (port >= I2S_PORT_COUNT) return;
    i2s_chan_handle_t* h = tx ? &_port[port].tx : &_port[port].rx;
    if (*h) {
        i2s_channel_disable(*h);
        i2s_del_channel(*h);
        *h = NULL;
    }
}

// Full teardown of both directions; resets all state fields.
static void _i2s_port_teardown(uint8_t port) {
    if (port >= I2S_PORT_COUNT) return;
    _i2s_port_teardown_dir(port, true);   // TX
    _i2s_port_teardown_dir(port, false);  // RX
    _port[port] = I2sPortState{};         // Reset all state
}

// Allocate I2S channels for a port via i2s_new_channel().
// needTx / needRx control whether TX and/or RX handles are requested.
// autoClr enables auto_clear on the channel config (zero-fill TX DMA on underrun).
// Returns true on success.
static bool _i2s_port_alloc(uint8_t port, bool needTx, bool needRx, bool autoClr) {
    if (port >= I2S_PORT_COUNT) return false;

    // Clean up any existing handles before reallocating
    _i2s_port_teardown_dir(port, true);
    _i2s_port_teardown_dir(port, false);

    i2s_port_t idfPort = (i2s_port_t)port;
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(idfPort, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num  = DMA_BUF_COUNT;
    chan_cfg.dma_frame_num = DMA_BUF_LEN;
    chan_cfg.auto_clear    = autoClr;

    i2s_chan_handle_t* pTx = needTx ? &_port[port].tx : nullptr;
    i2s_chan_handle_t* pRx = needRx ? &_port[port].rx : nullptr;

    esp_err_t err = i2s_new_channel(&chan_cfg, pTx, pRx);
    if (err != ESP_OK) {
        LOG_E("[Audio] Port %u channel alloc failed: %d", port, err);
        if (pTx) *pTx = NULL;
        if (pRx) *pRx = NULL;
        return false;
    }
    _port[port].autoCleared = autoClr;
    return true;
}

// Init TX direction in standard (STD) I2S mode.
// Updates _port[port].{txMode, sampleRate, txDoutPin, mclkPin, bckPin, lrcPin}.
static bool _i2s_port_init_tx_std(uint8_t port, uint32_t rate,
                                   gpio_num_t doutPin, gpio_num_t mclk,
                                   gpio_num_t bck, gpio_num_t ws) {
    if (port >= I2S_PORT_COUNT || !_port[port].tx) return false;

    i2s_std_config_t cfg = {};
    cfg.clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(rate);
    cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,
                                                        I2S_SLOT_MODE_STEREO);
    cfg.gpio_cfg.mclk = mclk;
    cfg.gpio_cfg.bclk = bck;
    cfg.gpio_cfg.ws   = ws;
    cfg.gpio_cfg.dout = doutPin;
    cfg.gpio_cfg.din  = I2S_GPIO_UNUSED;
    cfg.gpio_cfg.invert_flags.mclk_inv = false;
    cfg.gpio_cfg.invert_flags.bclk_inv = false;
    cfg.gpio_cfg.invert_flags.ws_inv   = false;

    esp_err_t err = i2s_channel_init_std_mode(_port[port].tx, &cfg);
    if (err != ESP_OK) {
        LOG_E("[Audio] Port %u TX STD init failed: %d", port, err);
        return false;
    }
    _port[port].txMode    = I2S_MODE_STD;
    _port[port].sampleRate = rate;
    _port[port].txDoutPin = doutPin;
    _port[port].mclkPin   = mclk;
    _port[port].bckPin    = bck;
    _port[port].lrcPin    = ws;
    return true;
}

// Init RX direction in standard (STD) I2S mode.
// Updates _port[port].{rxMode, rxDinPin} and inherits clock fields already set by TX.
static bool _i2s_port_init_rx_std(uint8_t port, uint32_t rate,
                                   gpio_num_t dinPin, gpio_num_t mclk,
                                   gpio_num_t bck, gpio_num_t ws) {
    if (port >= I2S_PORT_COUNT || !_port[port].rx) return false;

    i2s_std_config_t cfg = {};
    cfg.clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(rate);
    cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,
                                                        I2S_SLOT_MODE_STEREO);
    cfg.gpio_cfg.mclk = mclk;
    cfg.gpio_cfg.bclk = bck;
    cfg.gpio_cfg.ws   = ws;
    cfg.gpio_cfg.dout = I2S_GPIO_UNUSED;
    cfg.gpio_cfg.din  = dinPin;
    cfg.gpio_cfg.invert_flags.mclk_inv = false;
    cfg.gpio_cfg.invert_flags.bclk_inv = false;
    cfg.gpio_cfg.invert_flags.ws_inv   = false;

    esp_err_t err = i2s_channel_init_std_mode(_port[port].rx, &cfg);
    if (err != ESP_OK) {
        LOG_E("[Audio] Port %u RX STD init failed: %d", port, err);
        return false;
    }
    _port[port].rxMode   = I2S_MODE_STD;
    _port[port].rxDinPin = dinPin;
    // Propagate rate/clock pins in case TX was not initialized
    _port[port].sampleRate = rate;
    _port[port].mclkPin    = mclk;
    _port[port].bckPin     = bck;
    _port[port].lrcPin     = ws;
    return true;
}

// Init TX direction in TDM mode.
static bool _i2s_port_init_tx_tdm(uint8_t port, uint32_t rate,
                                    gpio_num_t doutPin, uint8_t slots,
                                    gpio_num_t mclk, gpio_num_t bck, gpio_num_t ws) {
    if (port >= I2S_PORT_COUNT || !_port[port].tx) return false;

    uint32_t slotMask = (1u << slots) - 1u;
    i2s_tdm_config_t cfg = {};
    cfg.clk_cfg  = I2S_TDM_CLK_DEFAULT_CONFIG(rate);
    cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    cfg.slot_cfg = I2S_TDM_MSB_SLOT_DEFAULT_CONFIG(
        I2S_DATA_BIT_WIDTH_32BIT,
        I2S_SLOT_MODE_STEREO,
        (i2s_tdm_slot_mask_t)slotMask);
    cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;
    cfg.gpio_cfg.mclk = mclk;
    cfg.gpio_cfg.bclk = bck;
    cfg.gpio_cfg.ws   = ws;
    cfg.gpio_cfg.dout = doutPin;
    cfg.gpio_cfg.din  = I2S_GPIO_UNUSED;
    cfg.gpio_cfg.invert_flags.mclk_inv = false;
    cfg.gpio_cfg.invert_flags.bclk_inv = false;
    cfg.gpio_cfg.invert_flags.ws_inv   = false;

    esp_err_t err = i2s_channel_init_tdm_mode(_port[port].tx, &cfg);
    if (err != ESP_OK) {
        LOG_E("[Audio] Port %u TX TDM init failed: %d", port, err);
        return false;
    }
    _port[port].txMode     = I2S_MODE_TDM;
    _port[port].txTdmSlots = slots;
    _port[port].sampleRate = rate;
    _port[port].txDoutPin  = doutPin;
    _port[port].mclkPin    = mclk;
    _port[port].bckPin     = bck;
    _port[port].lrcPin     = ws;
    return true;
}

// Init RX direction in TDM mode.
static bool _i2s_port_init_rx_tdm(uint8_t port, uint32_t rate,
                                    gpio_num_t dinPin, uint8_t slots,
                                    gpio_num_t mclk, gpio_num_t bck, gpio_num_t ws) {
    if (port >= I2S_PORT_COUNT || !_port[port].rx) return false;

    uint32_t slotMask = (1u << slots) - 1u;
    i2s_tdm_config_t cfg = {};
    cfg.clk_cfg  = I2S_TDM_CLK_DEFAULT_CONFIG(rate);
    cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    cfg.slot_cfg = I2S_TDM_MSB_SLOT_DEFAULT_CONFIG(
        I2S_DATA_BIT_WIDTH_32BIT,
        I2S_SLOT_MODE_STEREO,
        (i2s_tdm_slot_mask_t)slotMask);
    cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;
    cfg.gpio_cfg.mclk = mclk;
    cfg.gpio_cfg.bclk = bck;
    cfg.gpio_cfg.ws   = ws;
    cfg.gpio_cfg.dout = I2S_GPIO_UNUSED;
    cfg.gpio_cfg.din  = dinPin;
    cfg.gpio_cfg.invert_flags.mclk_inv = false;
    cfg.gpio_cfg.invert_flags.bclk_inv = false;
    cfg.gpio_cfg.invert_flags.ws_inv   = false;

    esp_err_t err = i2s_channel_init_tdm_mode(_port[port].rx, &cfg);
    if (err != ESP_OK) {
        LOG_E("[Audio] Port %u RX TDM init failed: %d", port, err);
        return false;
    }
    _port[port].rxMode     = I2S_MODE_TDM;
    _port[port].rxTdmSlots = slots;
    _port[port].rxDinPin   = dinPin;
    _port[port].sampleRate = rate;
    _port[port].mclkPin    = mclk;
    _port[port].bckPin     = bck;
    _port[port].lrcPin     = ws;
    return true;
}

// DEPRECATED: use i2s_audio_configure_adc(0, cfg) for new code.
static void i2s_configure_adc1(uint32_t sample_rate, const HalDeviceConfig* cfg = nullptr) {
    // Teardown any existing handles (recovery path or full-duplex toggle)
    if (_rx_handle_adc1) {
        i2s_channel_disable(_rx_handle_adc1);
        i2s_del_channel(_rx_handle_adc1);
        _rx_handle_adc1 = NULL;
    }
    if (_tx_handle_adc1) {
        i2s_channel_disable(_tx_handle_adc1);
        i2s_del_channel(_tx_handle_adc1);
        _tx_handle_adc1 = NULL;
    }

    // Always full-duplex: TX+RX created together from boot.
    // auto_clear fills TX DMA with zeros (silence) until dac_output_write() starts.
    // This keeps MCLK continuous — PCM1808 PLL never loses lock between DAC enable/disable.
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = DMA_BUF_COUNT;
    chan_cfg.dma_frame_num = DMA_BUF_LEN;
    chan_cfg.auto_clear = true; // TX: auto-fill zeros on underrun

    esp_err_t err = i2s_new_channel(&chan_cfg, &_tx_handle_adc1, &_rx_handle_adc1);
    if (err != ESP_OK) {
        LOG_E("[Audio] ADC1 channel alloc failed: %d", err);
        return;
    }

    // ADC pin overrides from passed config; DAC TX pin looked up separately.
    const HalDeviceConfig* _adcHalCfg = cfg;
    HalDeviceConfig* _dacHalCfg = hal_get_config_for_type(HAL_DEV_DAC);
    gpio_num_t _txDataPin = (_dacHalCfg && _dacHalCfg->pinData > 0)
        ? (gpio_num_t)_dacHalCfg->pinData : (gpio_num_t)I2S_TX_DATA_PIN;
    gpio_num_t _rxDataPin = (_adcHalCfg && _adcHalCfg->pinData > 0)
        ? (gpio_num_t)_adcHalCfg->pinData : (gpio_num_t)I2S_DOUT_PIN;

    // Resolve clock pins from HAL config (pinMclk/pinBck/pinLrc > 0 means HAL override;
    // -1 or 0 means use board default from config.h). The > 0 convention matches all
    // existing I2S pin checks in this file and hal_i2s_bridge.cpp.
    gpio_num_t mclkPin = _resolveI2sPin((_adcHalCfg && _adcHalCfg->valid) ? _adcHalCfg->pinMclk : -1, I2S_MCLK_PIN);
    gpio_num_t bckPin  = _resolveI2sPin((_adcHalCfg && _adcHalCfg->valid) ? _adcHalCfg->pinBck  : -1, I2S_BCK_PIN);
    gpio_num_t lrcPin  = _resolveI2sPin((_adcHalCfg && _adcHalCfg->valid) ? _adcHalCfg->pinLrc  : -1, I2S_LRC_PIN);

    // TX config: full clock master — MCLK/BCK/WS output, DOUT to DAC.
    // TX is initialized FIRST so clocks are live before the RX DMA starts.
    i2s_std_config_t tx_cfg = {};
    tx_cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate);
    tx_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
    tx_cfg.gpio_cfg.mclk = mclkPin;
    tx_cfg.gpio_cfg.bclk = bckPin;
    tx_cfg.gpio_cfg.ws   = lrcPin;
    tx_cfg.gpio_cfg.dout = _txDataPin;                   // DAC data out (HAL-overridable)
    tx_cfg.gpio_cfg.din  = I2S_GPIO_UNUSED;
    tx_cfg.gpio_cfg.invert_flags.mclk_inv = false;
    tx_cfg.gpio_cfg.invert_flags.bclk_inv = false;
    tx_cfg.gpio_cfg.invert_flags.ws_inv   = false;

    // RX config: mclkPin (same as TX). Both configs pointing to the same GPIO causes the
    // IDF5 GPIO matrix to re-route that pin to the same I2S MCLK output — no clearing occurs.
    // Setting MCLK=I2S_GPIO_UNUSED in the second init call clears the pin's routing, removing
    // MCLK from the PCM1808 SCKI → PLL never locks → noise floor only (~-68 dBFS).
    i2s_std_config_t rx_cfg = tx_cfg;
    rx_cfg.gpio_cfg.mclk = mclkPin;   // Same as TX — re-routes to same signal
    rx_cfg.gpio_cfg.bclk = bckPin;    // Keep — RX DMA needs BCK sync
    rx_cfg.gpio_cfg.ws   = lrcPin;    // Keep — RX DMA needs WS sync
    rx_cfg.gpio_cfg.dout = I2S_GPIO_UNUSED;
    rx_cfg.gpio_cfg.din  = _rxDataPin;                   // ADC1 data in (HAL-overridable)

    // TX first, RX second — official Espressif init order (i2s_es8311 example, IDF v5.5.3).
    // MCLK=GPIO3 in both configs: RX init re-routes GPIO3 to the same I2S MCLK output (no clearing).
    err = i2s_channel_init_std_mode(_tx_handle_adc1, &tx_cfg);
    if (err != ESP_OK) {
        LOG_E("[Audio] ADC1 TX init failed: %d", err);
        i2s_del_channel(_rx_handle_adc1); _rx_handle_adc1 = NULL;
        i2s_del_channel(_tx_handle_adc1); _tx_handle_adc1 = NULL;
        return;
    }
    err = i2s_channel_init_std_mode(_rx_handle_adc1, &rx_cfg);
    if (err != ESP_OK) {
        LOG_E("[Audio] ADC1 RX init failed: %d", err);
        i2s_channel_disable(_tx_handle_adc1);
        i2s_del_channel(_rx_handle_adc1); _rx_handle_adc1 = NULL;
        i2s_del_channel(_tx_handle_adc1); _tx_handle_adc1 = NULL;
        return;
    }

    // Boost MCLK drive strength to GPIO_DRIVE_CAP_3 (~40 mA).
    // IDF5 I2S driver leaves GPIO drive at the default (CAP_2, ~10 mA).
    // At 12.288 MHz, marginally driven signals can cause PCM1808 SCKI PLL instability.
    gpio_set_drive_capability(mclkPin, GPIO_DRIVE_CAP_3);

    // Enable TX then RX — no delay between enables required.
    // PCM1808 PLL stabilisation (2048 LRCK cycles = ~43 ms) completes during the
    // caller's post-init delay before audio_pipeline_task starts reading.
    i2s_channel_enable(_tx_handle_adc1);
    i2s_channel_enable(_rx_handle_adc1);
    LOG_I("[Audio] ADC1 TX+RX enabled — MCLK=GPIO%d @%lu Hz, drive=CAP_3",
          (int)mclkPin, _currentSampleRate * 256UL);
}

// ADC2 uses I2S_NUM_1 configured as MASTER (not slave) to bypass ESP32-S3
// slave mode constraints. I2S1 does NOT output any clocks — only data_in is
// connected (GPIO9). The internal RX state machine samples at the same
// frequency as I2S0's BCK, with a fixed phase offset well within PCM1808's
// data valid window (~305ns of 325ns period).
// DEPRECATED: use i2s_audio_configure_adc(1, cfg) for new code.
static bool i2s_configure_adc2(uint32_t sample_rate, const HalDeviceConfig* cfg = nullptr) {
    // Teardown any existing handle (recovery path)
    if (_rx_handle_adc2) {
        i2s_channel_disable(_rx_handle_adc2);
        i2s_del_channel(_rx_handle_adc2);
        _rx_handle_adc2 = NULL;
    }

    // I2S port from config or default (I2S_NUM_1 for secondary ADCs)
    i2s_port_t port = I2S_NUM_1;
    if (cfg && cfg->valid && cfg->i2sPort != 255 && cfg->i2sPort > 0) {
        port = (i2s_port_t)cfg->i2sPort;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(port, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = DMA_BUF_COUNT;
    chan_cfg.dma_frame_num = DMA_BUF_LEN;

    esp_err_t err = i2s_new_channel(&chan_cfg, NULL, &_rx_handle_adc2);
    if (err != ESP_OK) {
        LOG_E("[Audio] ADC2 RX channel alloc failed: %d", err);
        return false;
    }

    // Data-in pin from config or board default
    gpio_num_t dinPin = (cfg && cfg->valid && cfg->pinData > 0)
        ? (gpio_num_t)cfg->pinData : (gpio_num_t)I2S_DOUT2_PIN;

    // Only data_in pin — secondary I2S does NOT output BCK/WS/MCLK.
    // I2S0 (ADC1) provides all clock signals to both PCM1808 boards.
    i2s_std_config_t std_cfg = {};
    std_cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate);
    std_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
    std_cfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.bclk = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.ws   = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.dout = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.din  = dinPin;
    std_cfg.gpio_cfg.invert_flags.mclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.bclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.ws_inv   = false;

    err = i2s_channel_init_std_mode(_rx_handle_adc2, &std_cfg);
    if (err != ESP_OK) {
        LOG_E("[Audio] ADC2 RX init failed: %d", err);
        i2s_del_channel(_rx_handle_adc2);
        _rx_handle_adc2 = NULL;
        return false;
    }

    err = i2s_channel_enable(_rx_handle_adc2);
    if (err != ESP_OK) {
        LOG_E("[Audio] ADC2 RX enable failed: %d", err);
        i2s_del_channel(_rx_handle_adc2);
        _rx_handle_adc2 = NULL;
        return false;
    }
    return true;
}

// Public generic ADC configuration — dispatches to primary (full-duplex) or
// secondary (RX-only) based on lane index. Config provides pin/port overrides.
bool i2s_audio_configure_adc(int lane, const HalDeviceConfig* cfg) {
    // Cache the config per-lane so set_sample_rate() and enable_tx() can re-init
    // with the correct HAL pin assignments (they previously passed nullptr, losing overrides).
    if (cfg && cfg->valid && lane >= 0 && lane < 3) {
        _cachedAdcCfg[lane] = *cfg;
        _cachedAdcCfgValid[lane] = true;
    }

    // ESP32-P4 has 3 I2S peripherals: I2S0 (ADC1), I2S1 (ADC2), I2S2 (expansion mezzanine).
    // Software sources (SigGen, USB) don't use I2S and are not configured here.
    if (lane == 0) {
        i2s_configure_adc1(_currentSampleRate, cfg);
        return (_rx_handle_adc1 != NULL);
    } else if (lane == 1) {
        bool ok = i2s_configure_adc2(_currentSampleRate, cfg);
        _adc2InitOk = ok;
        return ok;
    } else if (lane == 2) {
#if CONFIG_IDF_TARGET_ESP32P4
        // Lane 2: Expansion mezzanine ADC on I2S2 RX
        gpio_num_t dinPin = (cfg && cfg->valid && cfg->pinData >= 0)
                          ? (gpio_num_t)cfg->pinData : (gpio_num_t)-1;
        if (dinPin < 0) {
            LOG_W("[Audio] Lane 2: no data pin configured for expansion ADC");
            return false;
        }
        bool ok = i2s_audio_enable_expansion_rx(_currentSampleRate, dinPin);
        _expansionRxOk = ok;
        return ok;
#else
        LOG_W("[Audio] Lane 2 (expansion) only supported on ESP32-P4");
        return false;
#endif
    }
    LOG_W("[Audio] i2s_audio_configure_adc: lane %d not supported (max 2)", lane);
    return false;
}

// Log computed I2S clock / framing parameters at boot
static void i2s_log_params(uint32_t sample_rate) {
    const uint32_t slot_bits   = 32;  // I2S_DATA_BIT_WIDTH_32BIT
    const uint32_t slots       = 2;   // Stereo
    const uint32_t mclk_mult   = 256; // I2S_STD_CLK_DEFAULT_CONFIG multiplier
    const uint32_t bclk        = sample_rate * slots * slot_bits;
    const uint32_t mclk        = sample_rate * mclk_mult;
    const uint32_t bits_frame  = slots * slot_bits;
    const uint32_t mclk_bclk   = mclk_mult / bits_frame; // MCLK/BCLK ratio

    LOG_I("[Audio] ---- I2S Parameters ----");
    LOG_I("[Audio]   Sample Rate : %lu Hz", (unsigned long)sample_rate);
    LOG_I("[Audio]   MCLK        : %lu Hz (x%lu)", (unsigned long)mclk, (unsigned long)mclk_mult);
    LOG_I("[Audio]   BCLK        : %lu Hz", (unsigned long)bclk);
    LOG_I("[Audio]   LRCLK (WS)  : %lu Hz", (unsigned long)sample_rate);
    LOG_I("[Audio]   Bits/frame  : %lu (%lu slots x %lu bits)", (unsigned long)bits_frame, (unsigned long)slots, (unsigned long)slot_bits);
    LOG_I("[Audio]   MCLK/BCLK   : %lu", (unsigned long)mclk_bclk);
    LOG_I("[Audio]   Data width  : 24-bit (in 32-bit frame, left-justified)");
    LOG_I("[Audio]   Format      : I2S Philips (MSB-first)");
    LOG_I("[Audio]   DMA         : %d bufs x %d frames (%lu ms runway)",
          DMA_BUF_COUNT, DMA_BUF_LEN,
          (unsigned long)((uint64_t)DMA_BUF_COUNT * DMA_BUF_LEN * 1000 / sample_rate));
    LOG_I("[Audio]   Clock src   : DEFAULT (PLL_F160M on S3, APLL on P4)");
    const int gpioMclk = (_cachedAdcCfgValid[0] && _cachedAdcCfg[0].pinMclk > 0) ? (int)_cachedAdcCfg[0].pinMclk : I2S_MCLK_PIN;
    const int gpioBck  = (_cachedAdcCfgValid[0] && _cachedAdcCfg[0].pinBck  > 0) ? (int)_cachedAdcCfg[0].pinBck  : I2S_BCK_PIN;
    const int gpioLrc  = (_cachedAdcCfgValid[0] && _cachedAdcCfg[0].pinLrc  > 0) ? (int)_cachedAdcCfg[0].pinLrc  : I2S_LRC_PIN;
    const int gpioDin1 = (_cachedAdcCfgValid[0] && _cachedAdcCfg[0].pinData > 0) ? (int)_cachedAdcCfg[0].pinData : I2S_DOUT_PIN;
    const int gpioDin2 = (_cachedAdcCfgValid[1] && _cachedAdcCfg[1].pinData > 0) ? (int)_cachedAdcCfg[1].pinData : I2S_DOUT2_PIN;
    LOG_I("[Audio]   GPIO        : MCLK=%d BCK=%d LRC=%d DIN1=%d DIN2=%d DOUT=%d",
          gpioMclk, gpioBck, gpioLrc, gpioDin1, gpioDin2, I2S_TX_DATA_PIN);
    LOG_I("[Audio] ----------------------------");
}


// ===== Port-indexed read/active callbacks for AudioInputSource (Phase 1 ADC componentization) =====
// Pre-baked thunks: HalPcm1808::init() assigns the appropriate one based on i2sPort

uint32_t i2s_audio_port0_read(int32_t* dst, uint32_t frames) {
    size_t bytes_read = 0;
    size_t size = frames * 2 * sizeof(int32_t);
    bool ok = i2s_audio_read_adc1(dst, size, &bytes_read, 500);
    if (!ok || bytes_read == 0) return 0;
    return (uint32_t)(bytes_read / (2 * sizeof(int32_t)));
}

uint32_t i2s_audio_port1_read(int32_t* dst, uint32_t frames) {
    if (!i2s_audio_adc2_ok()) return 0;
    size_t bytes_read = 0;
    size_t size = frames * 2 * sizeof(int32_t);
    bool ok = i2s_audio_read_adc2(dst, size, &bytes_read, 5);
    if (!ok || bytes_read == 0) return 0;
    return (uint32_t)(bytes_read / (2 * sizeof(int32_t)));
}

bool i2s_audio_port0_active(void) {
    return true;  // ADC1 always available when initialized
}

bool i2s_audio_port1_active(void) {
    return i2s_audio_adc2_ok();
}

uint32_t i2s_audio_port2_read(int32_t* dst, uint32_t frames) {
#if CONFIG_IDF_TARGET_ESP32P4
    if (!_port[2].rx || !_expansionRxOk) return 0;
    size_t bytes_read = 0;
    size_t size = frames * 2 * sizeof(int32_t);
    esp_err_t err = i2s_channel_read(_port[2].rx, dst, size, &bytes_read, 5);
    if (err != ESP_OK || bytes_read == 0) return 0;
    return (uint32_t)(bytes_read / (2 * sizeof(int32_t)));
#else
    (void)dst; (void)frames;
    return 0;
#endif
}

bool i2s_audio_port2_active(void) {
    return _expansionRxOk;
}

// ---------------------------------------------------------------------------
// TDM 4-slot read (ES9843PRO via HalTdmDeinterleaver)
//
// The ES9843PRO outputs 4 channels as contiguous 32-bit slots in each TDM
// frame.  IDF5 TDM mode packs N slots per "audio frame" — so a request for
// F frames results in F × N × sizeof(int32_t) bytes transferred by DMA.
//
// With N=4 slots the byte count is: frames × 4 × 4 = frames × 16 bytes.
// The caller (HalTdmDeinterleaver) pre-allocates dst for TDM_MAX_FRAMES_PER_BUF
// × 4 × sizeof(int32_t) bytes.
//
// Returns the number of complete TDM frames placed in dst (0 on error).
// ---------------------------------------------------------------------------
uint32_t i2s_audio_port2_tdm_read(int32_t* dst, uint32_t frames) {
#if CONFIG_IDF_TARGET_ESP32P4
    if (!_port[2].rx || !_expansionRxOk) return 0;
    size_t bytes_read = 0;
    // 4 slots per TDM frame, each slot is one int32_t
    size_t size = (size_t)frames * 4 * sizeof(int32_t);
    esp_err_t err = i2s_channel_read(_port[2].rx, dst, size, &bytes_read, pdMS_TO_TICKS(5));
    if (err != ESP_OK || bytes_read == 0) return 0;
    return (uint32_t)(bytes_read / (4 * sizeof(int32_t)));
#else
    (void)dst; (void)frames;
    return 0;
#endif
}

bool i2s_audio_port2_tdm_active(void) {
    return _expansionRxOk;
}

uint32_t i2s_audio_get_sample_rate(void) {
#ifndef NATIVE_TEST
    return AppState::getInstance().audio.sampleRate;
#else
    return 48000;
#endif
}

// Generic port-indexed dispatch
uint32_t i2s_audio_read_port(int port, int32_t *dst, uint32_t frames) {
    if (port == 0) return i2s_audio_port0_read(dst, frames);
    if (port == 1) return i2s_audio_port1_read(dst, frames);
    if (port == 2) return i2s_audio_port2_read(dst, frames);
    return 0;
}

bool i2s_audio_is_port_active(int port) {
    if (port == 0) return i2s_audio_port0_active();
    if (port == 1) return i2s_audio_port1_active();
    if (port == 2) return i2s_audio_port2_active();
    return false;
}


// Dual I2S Master Architecture:
// ADC1: master full-duplex, ADC2: master no-clock-output (S3 slave DMA workaround).
// I2S0 outputs BCK/WS/MCLK; I2S1 has data_in only (GPIO9).
// Init order: ADC2 first, then ADC1 (clock source). See i2s_configure_adc2().
void i2s_audio_init() {
    _currentSampleRate = AppState::getInstance().audio.sampleRate;
    if (!audio_validate_sample_rate(_currentSampleRate)) {
        _currentSampleRate = DEFAULT_AUDIO_SAMPLE_RATE;
    }

    // Reset diagnostics
    _diagnostics = AudioDiagnostics{};

    // Initialize analysis to floor — without this, zero-initialized dBFS=0.0f
    // looks like a 0dBFS signal to smart sensing, keeping the amplifier relay ON
    // and amplifying EMI/DAC noise floor while Stage 5 metering isn't active yet.
    portENTER_CRITICAL(&spinlock);
    _analysis.dBFS = DBFS_FLOOR;
    for (int a = 0; a < AUDIO_PIPELINE_MAX_INPUTS; a++) {
        _analysis.adc[a].dBFS = DBFS_FLOOR;
    }
    portEXIT_CRITICAL(&spinlock);

    // Allocate FFT/waveform buffers from PSRAM with SRAM fallback (one-time, ~22.5KB)
    if (!_fftData) {
        _fftData   = (float *)psram_alloc(FFT_SIZE * 2, sizeof(float), "fft_data");
        _fftWindow = (float *)psram_alloc(FFT_SIZE, sizeof(float), "fft_window");
        for (int a = 0; a < AUDIO_PIPELINE_MAX_INPUTS; a++) {
            _fftRing[a]  = (float *)psram_alloc(FFT_SIZE, sizeof(float), "fft_ring");
            _wfAccum[a]  = (float *)psram_alloc(WAVEFORM_BUFFER_SIZE, sizeof(float), "wf_accum");
            _wfOutput[a] = (uint8_t *)psram_alloc(WAVEFORM_BUFFER_SIZE, sizeof(uint8_t), "wf_output");
        }
        if (!_fftData || !_fftWindow) {
            LOG_E("[Audio] FATAL: FFT buffer allocation failed!");
            return;
        }
        LOG_I("[Audio] FFT/waveform buffers allocated");
    }

    _wfTargetFrames = _currentSampleRate * AppState::getInstance().audio.updateRate / 1000;
    for (int a = 0; a < AUDIO_PIPELINE_MAX_INPUTS; a++) {
        if (_wfAccum[a]) memset(_wfAccum[a], 0, WAVEFORM_BUFFER_SIZE * sizeof(float));
        _wfFramesSeen[a] = 0;
        _wfReady[a] = false;
        if (_fftRing[a]) memset(_fftRing[a], 0, FFT_SIZE * sizeof(float));
        _fftRingPos[a] = 0;
        _spectrumReady[a] = false;
        _lastFftTime[a] = 0;
    }

    // Initialize ESP-DSP Radix-4 FFT tables and window
    if (!_fftInitialized) {
        dsps_fft4r_init_fc32(NULL, FFT_SIZE);
        i2s_audio_apply_window(AppState::getInstance().audio.fftWindowType);
        _fftInitialized = true;
    }

    // I2S channel creation is deferred to i2s_audio_init_channels(), called from
    // audio_pipeline_task on Core 1.  This pins the I2S DMA ISR to Core 1,
    // isolating it from WiFi TX/RX interrupts on Core 0 that cause audio pops.

    // ADC input sources are now registered by HalPcm1808 via the bridge
    // (getInputSource() override in Phase 1).

    audio_pipeline_init();
}

// Called from audio_pipeline_task on Core 1 — creates I2S channels so that the
// DMA ISR is pinned to Core 1, isolated from WiFi interrupts on Core 0.
// Phase 3: Query HAL devices dynamically instead of hardcoding 2 lanes.
void i2s_audio_init_channels() {
#if !defined(NATIVE_TEST) && defined(DAC_ENABLED)
    HalDeviceManager& mgr = HalDeviceManager::instance();
    bool portOk[AUDIO_PIPELINE_MAX_INPUTS] = {};

    // Pass 1: non-clock-masters (data-only, no MCLK/BCK/WS output)
    for (uint8_t i = 0; i < HAL_MAX_DEVICES; i++) {
        HalDevice* dev = mgr.getDevice(i);
        if (!dev) continue;
        auto& desc = dev->getDescriptor();
        if (!(desc.capabilities & HAL_CAP_ADC_PATH)) continue;
        if (desc.bus.type != HAL_BUS_I2S) continue;
        HalDeviceConfig* cfg = mgr.getConfig(i);
        if (cfg && cfg->valid && cfg->isI2sClockMaster) continue;  // skip clock master in pass 1
        uint8_t port = (cfg && cfg->valid && cfg->i2sPort != 255) ? cfg->i2sPort : 1;
        portOk[port] = i2s_audio_configure_adc(port, cfg);
    }

    // Pass 2: clock master (outputs MCLK/BCK/WS — must be last)
    for (uint8_t i = 0; i < HAL_MAX_DEVICES; i++) {
        HalDevice* dev = mgr.getDevice(i);
        if (!dev) continue;
        auto& desc = dev->getDescriptor();
        if (!(desc.capabilities & HAL_CAP_ADC_PATH)) continue;
        if (desc.bus.type != HAL_BUS_I2S) continue;
        HalDeviceConfig* cfg = mgr.getConfig(i);
        if (!cfg || !cfg->valid || !cfg->isI2sClockMaster) continue;
        uint8_t port = (cfg->i2sPort != 255) ? cfg->i2sPort : 0;
        portOk[port] = i2s_audio_configure_adc(port, cfg);
    }

    _adc2InitOk = portOk[1];
    _numAdcsDetected = 0;
    for (int p = 0; p < AUDIO_PIPELINE_MAX_INPUTS; p++) {
        if (portOk[p]) _numAdcsDetected++;
    }
    gpio_pulldown_en((gpio_num_t)I2S_DOUT2_PIN);  // legacy: prevent float on unused port

    LOG_I("[Audio] I2S channels created on Core %d: %lu Hz, %d ADCs detected, clock master init order enforced",
          xPortGetCoreID(), _currentSampleRate, _numAdcsDetected);
#else
    // Fallback: hardcoded 2-lane init (safe mode or native tests)
    _adc2InitOk = false;
    i2s_audio_configure_adc(0, nullptr);
    gpio_pulldown_en((gpio_num_t)I2S_DOUT2_PIN);
    _numAdcsDetected = 1;
    LOG_I("[Audio] I2S channels created (fallback: hardcoded 2-lane mode)");
#endif

    i2s_log_params(_currentSampleRate);
}

int i2s_audio_get_num_adcs() {
    return _numAdcsDetected;
}

AudioAnalysis i2s_audio_get_analysis() {
    AudioAnalysis result;
    portENTER_CRITICAL(&spinlock);
    result = *(const AudioAnalysis *)&_analysis;
    portEXIT_CRITICAL(&spinlock);
    return result;
}

AudioDiagnostics i2s_audio_get_diagnostics() {
    AudioDiagnostics result;
    portENTER_CRITICAL(&spinlock);
    result = _diagnostics;
    portEXIT_CRITICAL(&spinlock);
    return result;
}

void i2s_audio_request_dump() {
    _dumpReady = true;
}

void audio_periodic_dump() {
    if (!_dumpReady) return;
    _dumpReady = false;

    // Per-ADC log — distinguishes failure modes:
    // zb high + az=0 + tot=0 → DMA timeout, slave not clocking
    // zb low  + az high      → Slave clocking OK, no audio
    // errs > 0               → I2S driver error (bus fault, DMA overflow)
    LOG_I("[Audio] --- adcs=%d ---", _numAdcsDetected);
    for (int i = 0; i < _numAdcsDetected; i++) {
        LOG_I("[Audio] ADC[%d]=%.1fdB flr=%.1f st=%d errs=%lu zb=%lu az=%lu cz=%lu tot=%lu",
              i, _analysis.adc[i].dBFS, _diagnostics.adc[i].noiseFloorDbfs,
              _diagnostics.adc[i].status,
              _diagnostics.adc[i].i2sReadErrors,
              _diagnostics.adc[i].zeroByteReads,
              _diagnostics.adc[i].allZeroBuffers,
              _diagnostics.adc[i].consecutiveZeros,
              _diagnostics.adc[i].totalBuffersRead);
    }
#ifdef DAC_ENABLED
    dac_periodic_log();
#endif
    audio_pipeline_dump_raw_diag();
}

bool i2s_audio_get_waveform(uint8_t *out, int adcIndex) {
    if (adcIndex < 0 || adcIndex >= AUDIO_PIPELINE_MAX_INPUTS) return false;
    if (!_wfReady[adcIndex]) return false;
    memcpy(out, (const void *)_wfOutput[adcIndex], WAVEFORM_BUFFER_SIZE);
    _wfReady[adcIndex] = false;
    return true;
}

bool i2s_audio_get_spectrum(float *bands, float *dominant_freq, int adcIndex) {
    if (adcIndex < 0 || adcIndex >= AUDIO_PIPELINE_MAX_INPUTS) return false;
    if (!_spectrumReady[adcIndex]) return false;
    memcpy(bands, (const void *)_spectrumOutput[adcIndex], SPECTRUM_BANDS * sizeof(float));
    *dominant_freq = _dominantFreqOutput[adcIndex];
    _spectrumReady[adcIndex] = false;
    return true;
}

I2sStaticConfig i2s_audio_get_static_config() {
    I2sStaticConfig cfg = {};
    // ADC1 — Master
    cfg.adc[0].isMaster = true;
    cfg.adc[0].sampleRate = _currentSampleRate;
    cfg.adc[0].bitsPerSample = 32;
    cfg.adc[0].channelFormat = "Stereo R/L";
    cfg.adc[0].dmaBufCount = DMA_BUF_COUNT;
    cfg.adc[0].dmaBufLen = DMA_BUF_LEN;
    cfg.adc[0].pllEnabled = true;
    cfg.adc[0].mclkHz = _currentSampleRate * 256;
    cfg.adc[0].commFormat = "Standard I2S";
    // ADC2 — Master (no clock output, data-only)
    cfg.adc[1].isMaster = true;
    cfg.adc[1].sampleRate = _currentSampleRate;
    cfg.adc[1].bitsPerSample = 32;
    cfg.adc[1].channelFormat = "Stereo R/L";
    cfg.adc[1].dmaBufCount = DMA_BUF_COUNT;
    cfg.adc[1].dmaBufLen = DMA_BUF_LEN;
    cfg.adc[1].pllEnabled = true;
    cfg.adc[1].mclkHz = _currentSampleRate * 256;
    cfg.adc[1].commFormat = "Standard I2S";
    return cfg;
}

bool i2s_audio_set_sample_rate(uint32_t rate) {
    if (!audio_validate_sample_rate(rate)) return false;
    if (rate == _currentSampleRate) return true;

    LOG_I("[Audio] Changing sample rate: %lu -> %lu Hz", _currentSampleRate, rate);

    // Pause audio task during channel teardown/recreate
    AppState::getInstance().audio.paused = true;
    vTaskDelay(pdMS_TO_TICKS(60));

    // Teardown all channels (configure functions handle this, but be explicit)
    if (_rx_handle_adc1) { i2s_channel_disable(_rx_handle_adc1); i2s_del_channel(_rx_handle_adc1); _rx_handle_adc1 = NULL; }
    if (_tx_handle_adc1) { i2s_channel_disable(_tx_handle_adc1); i2s_del_channel(_tx_handle_adc1); _tx_handle_adc1 = NULL; }
    if (_rx_handle_adc2) { i2s_channel_disable(_rx_handle_adc2); i2s_del_channel(_rx_handle_adc2); _rx_handle_adc2 = NULL; }

    _currentSampleRate = rate;
    _wfTargetFrames = rate * AppState::getInstance().audio.updateRate / 1000;
    for (int a = 0; a < AUDIO_PIPELINE_MAX_INPUTS; a++) {
        _wfFramesSeen[a] = 0;
        if (_wfAccum[a]) memset(_wfAccum[a], 0, WAVEFORM_BUFFER_SIZE * sizeof(float));
    }

    if (_adc2InitOk) _adc2InitOk = i2s_audio_configure_adc(1,
        _cachedAdcCfgValid[1] ? &_cachedAdcCfg[1] : nullptr);
    i2s_audio_configure_adc(0,
        _cachedAdcCfgValid[0] ? &_cachedAdcCfg[0] : nullptr);

#if CONFIG_IDF_TARGET_ESP32P4
    // Save I2S2 user state before teardown so we can restore at the new rate.
    uint8_t savedI2s2Users = _port[2].users;
    gpio_num_t savedDin    = _port[2].rxDinPin;

    if (savedI2s2Users) {
        if (_port[2].rx) { i2s_channel_disable(_port[2].rx); }
        if (_port[2].tx) { i2s_channel_disable(_port[2].tx); }
        if (_port[2].rx) { i2s_del_channel(_port[2].rx); _port[2].rx = NULL; }
        if (_port[2].tx) { i2s_del_channel(_port[2].tx); _port[2].tx = NULL; }
        _port[2].users    = 0;
        _expansionRxOk = false;
    }

    // Restore I2S2 users at the new sample rate.
    // TX (ES8311) first so the clock is stable before RX is enabled.
    if (savedI2s2Users & 0x01) {
        i2s_audio_enable_es8311_tx(rate);
    }
    if (savedI2s2Users & 0x02) {
        i2s_audio_enable_expansion_rx(rate, savedDin);
    }
#endif // CONFIG_IDF_TARGET_ESP32P4

    AppState::getInstance().audio.paused = false;
    LOG_I("[Audio] Sample rate changed to %lu Hz", rate);
    return true;
}

// ===== ADC Read API (used by audio_pipeline) =====

bool i2s_audio_read_adc1(void *buf, size_t size, size_t *bytes_read, uint32_t timeout_ms) {
    if (!_rx_handle_adc1) { if (bytes_read) *bytes_read = 0; return false; }
    esp_err_t err = i2s_channel_read(_rx_handle_adc1, buf, size, bytes_read, timeout_ms);
    if (err != ESP_OK) { _diagnostics.adc[0].i2sReadErrors++; }
    return (err == ESP_OK && bytes_read && *bytes_read > 0);
}

bool i2s_audio_read_adc2(void *buf, size_t size, size_t *bytes_read, uint32_t timeout_ms) {
    if (!_rx_handle_adc2 || !_adc2InitOk) { if (bytes_read) *bytes_read = 0; return false; }
    esp_err_t err = i2s_channel_read(_rx_handle_adc2, buf, size, bytes_read, timeout_ms);
    if (err != ESP_OK) { _diagnostics.adc[1].i2sReadErrors++; }
    return (err == ESP_OK && bytes_read && *bytes_read > 0);
}

bool i2s_audio_adc2_ok() {
    return _adc2InitOk;
}

// Called once per pipeline buffer — feeds measured dBFS into _analysis so that
// smart sensing gets a real signal level without the pipeline task touching the
// relay or any other sensing state.  This is the only coupling point between
// the audio pipeline and the sensing layer.
void i2s_audio_update_analysis_dbfs(float dbfs_adc1) {
    portENTER_CRITICAL(&spinlock);
    _analysis.dBFS          = dbfs_adc1;
    _analysis.adc[0].dBFS   = dbfs_adc1;
    _analysisReady          = true;
    portEXIT_CRITICAL(&spinlock);
}

// Full metering update — RMS/VU/peak/dBFS for ADC1, computed by audio_pipeline.
// Cast away volatile for bulk struct copy (safe: spinlock held, no concurrent write).
void i2s_audio_update_analysis_metering(const AdcAnalysis &adc0) {
    portENTER_CRITICAL(&spinlock);
    *(AdcAnalysis *)&_analysis.adc[0] = adc0;
    _analysis.dBFS  = adc0.dBFS;
    _analysisReady  = true;
    portEXIT_CRITICAL(&spinlock);
}

// Called once per pipeline buffer to accumulate waveform and FFT data for WebSocket display.
// rawLJ: left-justified int32 stereo interleaved from ADC (same as _rawBuf[adcIndex]).
// frames: number of stereo frames (== DMA_BUF_LEN).
// adcIndex: 0=ADC1, 1=ADC2.
void i2s_audio_push_waveform_fft(const int32_t *rawLJ, int frames, int adcIndex) {
    if (adcIndex < 0 || adcIndex >= AUDIO_PIPELINE_MAX_INPUTS) return;
    if (!rawLJ || frames <= 0) return;
    if (!_wfOutput[adcIndex] || !_fftRing[adcIndex]) return;

    // ---- Waveform snapshot ----
    // Count incoming frames; snapshot the current DMA buffer when the update window
    // expires, giving a ~_wfTargetFrames refresh interval at the configured rate.
    _wfFramesSeen[adcIndex] += frames;
    if (_wfFramesSeen[adcIndex] >= _wfTargetFrames && !_wfReady[adcIndex]) {
        audio_downsample_waveform(rawLJ, frames, _wfOutput[adcIndex], WAVEFORM_BUFFER_SIZE);
        _wfReady[adcIndex] = true;
        _wfFramesSeen[adcIndex] = 0;
    }

    // ---- FFT ring buffer ----
    if (!_fftInitialized || !_fftData || !_fftWindow) return;

    for (int f = 0; f < frames; f++) {
        // Mono mix: average L and R; right-shift 8 to recover signed 24-bit from LJ int32
        float L = (float)(rawLJ[f * 2]     >> 8) / 8388607.0f;
        float R = (float)(rawLJ[f * 2 + 1] >> 8) / 8388607.0f;
        _fftRing[adcIndex][_fftRingPos[adcIndex]++] = (L + R) * 0.5f;

        if (_fftRingPos[adcIndex] < FFT_SIZE) continue;
        _fftRingPos[adcIndex] = 0;  // Ring full — run FFT, then restart

        // Build windowed complex input: [Re0, 0, Re1, 0, ...]
        for (int i = 0; i < FFT_SIZE; i++) {
            _fftData[i * 2]     = _fftRing[adcIndex][i] * _fftWindow[i];
            _fftData[i * 2 + 1] = 0.0f;
        }

#ifndef NATIVE_TEST
        dsps_fft4r_fc32(_fftData, FFT_SIZE);
        dsps_bit_rev4r_fc32(_fftData, FFT_SIZE);
#endif

        // Compute magnitudes (first FFT_SIZE/2 bins); reuse ring buffer as temp
        for (int i = 0; i < FFT_SIZE / 2; i++) {
            float re = _fftData[i * 2];
            float im = _fftData[i * 2 + 1];
            _fftRing[adcIndex][i] = sqrtf(re * re + im * im);
        }

        // Aggregate magnitude bins into musically-spaced spectrum bands
        audio_aggregate_fft_bands(_fftRing[adcIndex], FFT_SIZE,
                                  (float)_currentSampleRate,
                                  _spectrumOutput[adcIndex], SPECTRUM_BANDS);

        // Find dominant frequency (skip DC bin 0)
        float maxMag = 0.0f;
        int maxBin = 1;
        for (int i = 1; i < FFT_SIZE / 2; i++) {
            if (_fftRing[adcIndex][i] > maxMag) {
                maxMag = _fftRing[adcIndex][i];
                maxBin = i;
            }
        }
        _dominantFreqOutput[adcIndex] = (float)maxBin * ((float)_currentSampleRate / FFT_SIZE);
        _spectrumReady[adcIndex] = true;
        // Ring reset to 0 above; remaining frames of current DMA buffer fill the fresh ring
    }
}

// ===== TX Bridge API (called by dac_hal to manage full-duplex on I2S0) =====

bool i2s_audio_enable_tx(uint32_t sample_rate) {
    if (_tx_handle_adc1) return true; // Already enabled

    // Query HAL config for TX data pin override (matches i2s_configure_adc1 pattern)
    HalDeviceConfig* _dacHalCfg = hal_get_config_for_type(HAL_DEV_DAC);
    gpio_num_t _txDataPin = (_dacHalCfg && _dacHalCfg->pinData > 0)
        ? (gpio_num_t)_dacHalCfg->pinData : (gpio_num_t)I2S_TX_DATA_PIN;

    LOG_I("[Audio] Enabling I2S TX full-duplex on I2S0, data_out=GPIO%d", (int)_txDataPin);

    // Pause audio task during channel reinit
    AppState::getInstance().audio.paused = true;
    vTaskDelay(pdMS_TO_TICKS(60));

    // Teardown existing RX-only channel
    if (_rx_handle_adc1) {
        i2s_channel_disable(_rx_handle_adc1);
        i2s_del_channel(_rx_handle_adc1);
        _rx_handle_adc1 = NULL;
    }

    // Allocate TX+RX channel pair on I2S0
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = DMA_BUF_COUNT;
    chan_cfg.dma_frame_num = DMA_BUF_LEN;
    chan_cfg.auto_clear = true; // TX: auto-fill zeros on underrun

    esp_err_t err = i2s_new_channel(&chan_cfg, &_tx_handle_adc1, &_rx_handle_adc1);
    if (err != ESP_OK) {
        LOG_E("[Audio] Full-duplex channel alloc failed: %d", err);
        _tx_handle_adc1 = NULL; _rx_handle_adc1 = NULL;
        AppState::getInstance().audio.paused = false;
        return false;
    }

    // Resolve ADC1 clock and data-in pins from the lane-0 HAL config cache.
    // The cache was populated at boot by i2s_audio_configure_adc(0, cfg); falling back
    // to board defaults ensures this function works even if called before the cache is set.
    const HalDeviceConfig* adcCfg = _cachedAdcCfgValid[0] ? &_cachedAdcCfg[0] : nullptr;
    gpio_num_t mclkPin = _resolveI2sPin((adcCfg && adcCfg->valid) ? adcCfg->pinMclk : -1, I2S_MCLK_PIN);
    gpio_num_t bckPin  = _resolveI2sPin((adcCfg && adcCfg->valid) ? adcCfg->pinBck  : -1, I2S_BCK_PIN);
    gpio_num_t lrcPin  = _resolveI2sPin((adcCfg && adcCfg->valid) ? adcCfg->pinLrc  : -1, I2S_LRC_PIN);
    gpio_num_t dinPin  = _resolveI2sPin((adcCfg && adcCfg->valid) ? adcCfg->pinData : -1, I2S_DOUT_PIN);

    // IDF5 full-duplex: BOTH handles must be initialized with the same config
    i2s_std_config_t std_cfg = {};
    std_cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate);
    std_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
    std_cfg.gpio_cfg.mclk = mclkPin;
    std_cfg.gpio_cfg.bclk = bckPin;
    std_cfg.gpio_cfg.ws   = lrcPin;
    std_cfg.gpio_cfg.dout = _txDataPin;
    std_cfg.gpio_cfg.din  = dinPin;
    std_cfg.gpio_cfg.invert_flags.mclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.bclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.ws_inv   = false;

    // TX first, RX second — matches i2s_configure_adc1() canonical order (MCLK must route from TX init)
    err = i2s_channel_init_std_mode(_tx_handle_adc1, &std_cfg);
    if (err != ESP_OK) {
        LOG_E("[Audio] Full-duplex TX init failed: %d", err);
        i2s_del_channel(_rx_handle_adc1); _rx_handle_adc1 = NULL;
        i2s_del_channel(_tx_handle_adc1); _tx_handle_adc1 = NULL;
        AppState::getInstance().audio.paused = false;
        return false;
    }
    err = i2s_channel_init_std_mode(_rx_handle_adc1, &std_cfg);
    if (err != ESP_OK) {
        LOG_E("[Audio] Full-duplex RX init failed: %d", err);
        i2s_channel_disable(_tx_handle_adc1);
        i2s_del_channel(_rx_handle_adc1); _rx_handle_adc1 = NULL;
        i2s_del_channel(_tx_handle_adc1); _tx_handle_adc1 = NULL;
        AppState::getInstance().audio.paused = false;
        return false;
    }

    i2s_channel_enable(_tx_handle_adc1);
    i2s_channel_enable(_rx_handle_adc1);

    AppState::getInstance().audio.paused = false;
    LOG_I("[Audio] I2S TX full-duplex enabled: rate=%luHz data_out=GPIO%d MCLK=%luHz DMA=%dx%d",
          (unsigned long)sample_rate, (int)_txDataPin,
          (unsigned long)(sample_rate * 256),
          DMA_BUF_COUNT, DMA_BUF_LEN);
    return true;
}

void i2s_audio_disable_tx() {
    // Keep the full-duplex channel alive to preserve MCLK continuity.
    // PCM1808 PLL stays locked; auto_clear fills TX DMA with zeros (silence).
    // dac_hal._i2sTxEnabled=false prevents dac_output_write() from feeding audio.
    if (_tx_handle_adc1) {
        LOG_I("[Audio] I2S TX write path disabled (MCLK remains active)");
    }
}

void i2s_audio_write(const void *src, size_t size, size_t *bytes_written, uint32_t timeout_ms) {
    if (!_tx_handle_adc1) {
        if (bytes_written) *bytes_written = 0;
        return;
    }
    i2s_channel_write(_tx_handle_adc1, src, size, bytes_written, timeout_ms);
}

// ===== ES8311 Secondary DAC TX Bridge (I2S2, P4 only) =====

#if CONFIG_IDF_TARGET_ESP32P4

// Legacy wrapper — tears down any existing I2S2 channels, reallocates as full-duplex,
// and initializes TX (ES8311 STD) + RX (expansion ADC STD) using the generic helpers.
// Always allocates both TX+RX handles (IDF5 requirement for a shared peripheral).
// rx_din_pin: expansion ADC data-in GPIO, or I2S_GPIO_UNUSED if no RX needed yet.
static bool _i2s2_alloc_full_duplex(uint32_t sample_rate, gpio_num_t rx_din_pin) {
    // Allocate both handles via the generic helper (port 2, TX+RX, auto_clear=true)
    if (!_i2s_port_alloc(2, /*needTx=*/true, /*needRx=*/true, /*autoClr=*/true)) {
        return false;
    }

    // TX first — activates MCLK/BCK/WS output (ES8311 clock master)
    if (!_i2s_port_init_tx_std(2, sample_rate,
                                (gpio_num_t)ES8311_I2S_DSDIN_PIN,
                                (gpio_num_t)ES8311_I2S_MCLK_PIN,
                                (gpio_num_t)ES8311_I2S_SCLK_PIN,
                                (gpio_num_t)ES8311_I2S_LRCK_PIN)) {
        // TX init failed — clean up
        _i2s_port_teardown_dir(2, false);  // del RX
        _i2s_port_teardown_dir(2, true);   // del TX
        return false;
    }
    // Apply MCLK multiple after TX init (helper sets STD defaults; override here)
    // Note: the helper already set the standard MCLK multiple; ES8311 needs x256
    // which is the IDF5 default so no additional override is required.

    // RX — expansion ADC data-in, shares clocks with TX
    if (!_i2s_port_init_rx_std(2, sample_rate,
                                rx_din_pin,
                                (gpio_num_t)ES8311_I2S_MCLK_PIN,
                                (gpio_num_t)ES8311_I2S_SCLK_PIN,
                                (gpio_num_t)ES8311_I2S_LRCK_PIN)) {
        // RX init failed — disable TX and clean up
        _i2s_port_teardown_dir(2, true);   // disable+del TX
        _i2s_port_teardown_dir(2, false);  // del RX (already failed but handle may exist)
        return false;
    }

    return true;
}

bool i2s_audio_enable_es8311_tx(uint32_t sample_rate) {
    if (_port[2].users & 0x01) {
        LOG_I("[ES8311] I2S2 TX already enabled");
        return true;
    }

    LOG_I("[ES8311] Enabling I2S2 TX, rate=%luHz", (unsigned long)sample_rate);

    if (_port[2].users == 0) {
        // I2S2 idle — fresh full-duplex allocation (RX side idles until expansion ADC connects)
        if (!_i2s2_alloc_full_duplex(sample_rate, I2S_GPIO_UNUSED)) return false;

        esp_err_t err = i2s_channel_enable(_port[2].tx);
        if (err != ESP_OK) {
            LOG_E("[ES8311] I2S2 TX enable failed: %d", err);
            i2s_del_channel(_port[2].rx); _port[2].rx = NULL;
            i2s_del_channel(_port[2].tx); _port[2].tx = NULL;
            return false;
        }
    } else {
        // Expansion RX already active (bit 0x02) — I2S2 is up.
        // Teardown and reallocate as full-duplex with both directions.
        LOG_I("[ES8311] I2S2 RX active — upgrading to full-duplex");
        gpio_num_t savedDin = _port[2].rxDinPin;

        if (!_i2s2_alloc_full_duplex(sample_rate, savedDin)) return false;

        // Enable both TX and RX
        i2s_channel_enable(_port[2].tx);
        esp_err_t err = i2s_channel_enable(_port[2].rx);
        if (err != ESP_OK) {
            LOG_W("[ES8311] I2S2 RX re-enable after upgrade failed: %d — expansion ADC degraded", err);
            _expansionRxOk = false;
            _port[2].users &= ~0x02u;
        }
    }

    _port[2].users |= 0x01;

    LOG_I("[ES8311] I2S2 TX enabled: rate=%luHz MCLK=GPIO%d@%luHz BCK=GPIO%d WS=GPIO%d DOUT=GPIO%d DMA=%dx%d",
          (unsigned long)sample_rate,
          ES8311_I2S_MCLK_PIN, (unsigned long)(sample_rate * 256),
          ES8311_I2S_SCLK_PIN, ES8311_I2S_LRCK_PIN, ES8311_I2S_DSDIN_PIN,
          DMA_BUF_COUNT, DMA_BUF_LEN);
    return true;
}

void i2s_audio_disable_es8311_tx() {
    if (!(_port[2].users & 0x01)) return;

    _port[2].users &= ~0x01u;

    if (_port[2].users == 0) {
        // Last user — full teardown
        if (_port[2].tx) { i2s_channel_disable(_port[2].tx); i2s_del_channel(_port[2].tx); _port[2].tx = NULL; }
        if (_port[2].rx) { i2s_channel_disable(_port[2].rx); i2s_del_channel(_port[2].rx); _port[2].rx = NULL; }
        LOG_I("[ES8311] I2S2 TX disabled — peripheral released");
    } else {
        // Expansion RX still active — keep peripheral up, TX DMA fills zeros (auto_clear)
        LOG_I("[ES8311] I2S2 TX disabled — expansion RX still active, clocks remain");
    }
}

void i2s_audio_write_es8311(const void *src, size_t size, size_t *bytes_written, uint32_t timeout_ms) {
    if (!(_port[2].users & 0x01) || !_port[2].tx || !src) {
        if (bytes_written) *bytes_written = 0;
        return;
    }
    i2s_channel_write(_port[2].tx, src, size, bytes_written, timeout_ms);
}

// ===== Expansion Mezzanine ADC RX Bridge (I2S2, P4 only) =====
// Allocates I2S2 RX channel for expansion mezzanine ADC input.
// Both TX (ES8311) and RX (expansion ADC) share BCK/WS/MCLK on I2S2 (full-duplex).
// IDF5 requires both handles to be allocated together via i2s_new_channel().

bool i2s_audio_enable_expansion_rx(uint32_t sample_rate, gpio_num_t din_pin) {
    if (_port[2].users & 0x02) {
        LOG_I("[Audio] I2S2 expansion RX already enabled");
        return true;
    }

    LOG_I("[Audio] Enabling I2S2 expansion RX, rate=%luHz DIN=GPIO%d",
          (unsigned long)sample_rate, (int)din_pin);

    if (_port[2].users == 0) {
        // I2S2 idle — fresh allocation
        if (!_i2s2_alloc_full_duplex(sample_rate, din_pin)) return false;

        esp_err_t err = i2s_channel_enable(_port[2].rx);
        if (err != ESP_OK) {
            LOG_E("[Audio] I2S2 expansion RX enable failed: %d", err);
            i2s_del_channel(_port[2].rx); _port[2].rx = NULL;
            i2s_del_channel(_port[2].tx); _port[2].tx = NULL;
            return false;
        }
        // TX handle exists but not enabled — no clocks until ES8311 activates
    } else {
        // ES8311 TX already active (bit 0x01) — I2S2 is up.
        // Rate must match (shared clocks).
        if (_port[2].sampleRate != sample_rate) {
            LOG_W("[Audio] I2S2 expansion RX rate mismatch: I2S2@%luHz, requested %luHz — adopting %luHz",
                  (unsigned long)_port[2].sampleRate, (unsigned long)sample_rate,
                  (unsigned long)_port[2].sampleRate);
            sample_rate = _port[2].sampleRate;
        }

        // Teardown and reallocate with the real DIN pin
        LOG_I("[Audio] I2S2 TX active — upgrading to full-duplex with DIN=GPIO%d", (int)din_pin);
        if (!_i2s2_alloc_full_duplex(sample_rate, din_pin)) return false;

        // Re-enable both directions
        i2s_channel_enable(_port[2].tx);
        esp_err_t err = i2s_channel_enable(_port[2].rx);
        if (err != ESP_OK) {
            LOG_E("[Audio] I2S2 expansion RX enable failed after upgrade: %d", err);
            return false;
        }
        // Restore TX user bit (was cleared during teardown in _alloc)
        _port[2].users |= 0x01;
    }

    _port[2].users |= 0x02;
    _expansionRxOk = true;

    LOG_I("[Audio] I2S2 expansion RX enabled: rate=%luHz DIN=GPIO%d DMA=%dx%d",
          (unsigned long)sample_rate, (int)din_pin, DMA_BUF_COUNT, DMA_BUF_LEN);
    return true;
}

void i2s_audio_disable_expansion_rx() {
    if (!(_port[2].users & 0x02)) return;

    _port[2].users &= ~0x02u;
    _expansionRxOk = false;

    if (_port[2].users == 0) {
        // Last user — full teardown
        if (_port[2].rx) { i2s_channel_disable(_port[2].rx); i2s_del_channel(_port[2].rx); _port[2].rx = NULL; }
        if (_port[2].tx) { i2s_channel_disable(_port[2].tx); i2s_del_channel(_port[2].tx); _port[2].tx = NULL; }
        LOG_I("[Audio] I2S2 expansion RX disabled — peripheral released");
    } else {
        // ES8311 TX still active — disable RX DMA but keep peripheral up
        if (_port[2].rx) i2s_channel_disable(_port[2].rx);
        LOG_I("[Audio] I2S2 expansion RX disabled — ES8311 TX still active");
    }
}

bool i2s_audio_expansion_rx_ok() {
    return _expansionRxOk;
}

// ---------------------------------------------------------------------------
// i2s_audio_enable_expansion_tdm_rx() — TDM 4-slot mode for ES9843PRO
//
// The ES9843PRO outputs all 4 channels on a single DATA line as a TDM stream:
//   [SLOT0=CH1][SLOT1=CH2][SLOT2=CH3][SLOT3=CH4] per frame, each 32-bit.
//
// IDF5 TDM channel init uses i2s_channel_init_tdm_mode() instead of
// i2s_channel_init_std_mode().  The TX direction is still configured in
// standard mode for ES8311 compatibility; only the RX side switches to TDM.
//
// This function follows the same user-bit protocol as i2s_audio_enable_expansion_rx():
//   bit 0x02 = RX user active.
// It is NOT compatible with the stereo expansion RX path — only one may be
// active on I2S2 RX at a time.
//
// Parameters:
//   sample_rate  : audio sample rate in Hz (must match I2S0/ES8311 if TX active)
//   din_pin      : GPIO for TDM DATA_IN from ES9843PRO
//   slot_count   : number of TDM slots (4 for ES9843PRO)
//
// Returns true on success; leaves _expansionRxOk=true so port2_tdm_read works.
// ---------------------------------------------------------------------------
bool i2s_audio_enable_expansion_tdm_rx(uint32_t sample_rate,
                                        gpio_num_t din_pin,
                                        uint8_t    slot_count) {
    if (_port[2].users & 0x02) {
        LOG_I("[Audio] I2S2 expansion TDM RX already enabled");
        return true;
    }

    if (slot_count < 2 || slot_count > 16) {
        LOG_W("[Audio] TDM: invalid slot_count=%u (must be 2-16), defaulting to 4", slot_count);
        slot_count = 4;
    }

    LOG_I("[Audio] Enabling I2S2 expansion TDM RX: rate=%luHz DIN=GPIO%d slots=%u",
          (unsigned long)sample_rate, (int)din_pin, slot_count);

    // --- Allocate or reallocate I2S2 channels ---
    // Always tear down and rebuild because TDM RX and STD RX have different
    // i2s_chan_config_t slot settings and cannot be reconfigured in place.
    bool txWasActive = (_port[2].users & 0x01) != 0;
    _port[2].users = 0;   // Cleared — will be restored below

    // _i2s_port_alloc() handles teardown of any existing handles before realloc
    if (!_i2s_port_alloc(2, /*needTx=*/true, /*needRx=*/true, /*autoClr=*/true)) {
        return false;
    }

    // TX: standard stereo mode (ES8311 DAC — unchanged from stereo path)
    if (txWasActive) {
        if (!_i2s_port_init_tx_std(2, sample_rate,
                                    (gpio_num_t)ES8311_I2S_DSDIN_PIN,
                                    (gpio_num_t)ES8311_I2S_MCLK_PIN,
                                    (gpio_num_t)ES8311_I2S_SCLK_PIN,
                                    (gpio_num_t)ES8311_I2S_LRCK_PIN)) {
            LOG_E("[Audio] I2S2 TDM: TX (ES8311) reinit failed");
            _i2s_port_teardown(2);
            return false;
        }
    }

    // RX: TDM mode — slot_count slots × 32-bit, left-justified PCM
    // RX shares MCLK/BCK/WS with TX (ES8311 is clock master)
    if (!_i2s_port_init_rx_tdm(2, sample_rate,
                                 din_pin,
                                 slot_count,
                                 (gpio_num_t)ES8311_I2S_MCLK_PIN,
                                 (gpio_num_t)ES8311_I2S_SCLK_PIN,
                                 (gpio_num_t)ES8311_I2S_LRCK_PIN)) {
        LOG_E("[Audio] I2S2 TDM RX init failed");
        _i2s_port_teardown_dir(2, true);   // disable+del TX
        _i2s_port_teardown_dir(2, false);  // del RX
        return false;
    }

    // Enable TX first (activates MCLK/BCK/WS for TDM ADC PLL lock)
    if (txWasActive) {
        esp_err_t err = i2s_channel_enable(_port[2].tx);
        if (err != ESP_OK) {
            LOG_E("[Audio] I2S2 TDM: TX enable failed: %d", err);
        } else {
            _port[2].users |= 0x01;
        }
    }

    // Enable RX
    esp_err_t err = i2s_channel_enable(_port[2].rx);
    if (err != ESP_OK) {
        LOG_E("[Audio] I2S2 TDM RX enable failed: %d", err);
        if (txWasActive) i2s_channel_disable(_port[2].tx);
        _i2s_port_teardown(2);
        _port[2].users = 0;
        return false;
    }

    _port[2].users      |= 0x02;
    _port[2].sampleRate  = sample_rate;
    _port[2].rxDinPin    = din_pin;
    _expansionRxOk       = true;

    LOG_I("[Audio] I2S2 expansion TDM RX enabled: rate=%luHz DIN=GPIO%d slots=%u DMA=%dx%d",
          (unsigned long)sample_rate, (int)din_pin, slot_count, DMA_BUF_COUNT, DMA_BUF_LEN);
    return true;
}

// ===== Expansion Mezzanine DAC TX Bridge (port-generic, P4 only) =====
// These wrappers delegate to the port-generic API using port 2 with ES8311 clock pins.
// In Phase 3 the port and clock pin selection will become fully configurable via
// HalDeviceConfig; for now port 2 + ES8311 defaults are the only expansion TX path.

bool i2s_audio_enable_expansion_tx(uint32_t sample_rate, gpio_num_t dout_pin) {
    return i2s_port_enable_tx(2, I2S_MODE_STD, 0, dout_pin,
        (gpio_num_t)ES8311_I2S_MCLK_PIN,
        (gpio_num_t)ES8311_I2S_SCLK_PIN,
        (gpio_num_t)ES8311_I2S_LRCK_PIN);
}

void i2s_audio_disable_expansion_tx() {
    i2s_port_disable_tx(2);
}

void i2s_audio_write_expansion_tx(const void *src, size_t size, size_t *bytes_written, uint32_t timeout_ms) {
    i2s_port_write(2, src, size, bytes_written, timeout_ms);
}

bool i2s_audio_enable_expansion_tdm_tx(uint32_t sample_rate, gpio_num_t dout_pin, uint8_t slot_count) {
    return i2s_port_enable_tx(2, I2S_MODE_TDM, slot_count, dout_pin,
        (gpio_num_t)ES8311_I2S_MCLK_PIN,
        (gpio_num_t)ES8311_I2S_SCLK_PIN,
        (gpio_num_t)ES8311_I2S_LRCK_PIN);
}

void i2s_audio_disable_expansion_tdm_tx() {
    i2s_port_disable_tx(2);
}

void i2s_audio_write_expansion_tdm_tx(const void *src, size_t size, size_t *bytes_written, uint32_t timeout_ms) {
    i2s_port_write(2, src, size, bytes_written, timeout_ms);
}

// ===== Port-generic I2S Public API =====
// These functions provide uniform TX/RX lifecycle management for all 3 I2S ports.
// They wrap the Phase 1 internal helpers with user-bit tracking and full-duplex upgrade logic.

bool i2s_port_enable_tx(uint8_t port, uint8_t mode, uint8_t tdmSlots,
                         gpio_num_t doutPin, gpio_num_t mclk, gpio_num_t bck, gpio_num_t ws) {
    if (port >= I2S_PORT_COUNT) return false;

    // Already have TX — nothing to do
    if (_port[port].users & I2S_USER_TX) return true;

    uint32_t rate = _port[port].sampleRate ? _port[port].sampleRate : _currentSampleRate;

    if (_port[port].users & I2S_USER_RX) {
        // RX already active — tear down and realloc as full-duplex, then restore RX
        LOG_I("[I2S] Port%u: upgrading RX-only → full-duplex for TX", port);
        gpio_num_t savedDin     = _port[port].rxDinPin;
        uint8_t    savedRxMode  = _port[port].rxMode;
        uint8_t    savedRxSlots = _port[port].rxTdmSlots;
        gpio_num_t savedMclk    = _port[port].mclkPin;
        gpio_num_t savedBck     = _port[port].bckPin;
        gpio_num_t savedWs      = _port[port].lrcPin;
        _port[port].users = 0;

        if (!_i2s_port_alloc(port, true, true, true)) return false;

        // Restore TX
        bool txOk = false;
        if (mode == I2S_MODE_TDM) {
            txOk = _i2s_port_init_tx_tdm(port, rate, doutPin, tdmSlots, mclk, bck, ws);
        } else {
            txOk = _i2s_port_init_tx_std(port, rate, doutPin, mclk, bck, ws);
        }
        if (!txOk) {
            _i2s_port_teardown(port);
            return false;
        }

        // Restore RX
        bool rxOk = false;
        if (savedRxMode == I2S_MODE_TDM) {
            rxOk = _i2s_port_init_rx_tdm(port, rate, savedDin, savedRxSlots, savedMclk, savedBck, savedWs);
        } else {
            rxOk = _i2s_port_init_rx_std(port, rate, savedDin, savedMclk, savedBck, savedWs);
        }
        if (!rxOk) {
            LOG_W("[I2S] Port%u: RX restore failed during TX upgrade — RX degraded", port);
            _port[port].users &= ~I2S_USER_RX;
        }

        // Enable TX
        esp_err_t err = i2s_channel_enable(_port[port].tx);
        if (err != ESP_OK) {
            LOG_E("[I2S] Port%u TX enable failed: %d", port, err);
            _i2s_port_teardown(port);
            return false;
        }

        // Re-enable RX if restore succeeded
        if (rxOk && _port[port].rx) {
            err = i2s_channel_enable(_port[port].rx);
            if (err != ESP_OK) {
                LOG_W("[I2S] Port%u RX re-enable after upgrade failed: %d", port, err);
                _port[port].users &= ~I2S_USER_RX;
            } else {
                _port[port].users |= I2S_USER_RX;
            }
        }
    } else {
        // Port idle — fresh TX-only alloc
        if (!_i2s_port_alloc(port, true, false, true)) return false;

        bool txOk = false;
        if (mode == I2S_MODE_TDM) {
            txOk = _i2s_port_init_tx_tdm(port, rate, doutPin, tdmSlots, mclk, bck, ws);
        } else {
            txOk = _i2s_port_init_tx_std(port, rate, doutPin, mclk, bck, ws);
        }
        if (!txOk) {
            _i2s_port_teardown(port);
            return false;
        }

        esp_err_t err = i2s_channel_enable(_port[port].tx);
        if (err != ESP_OK) {
            LOG_E("[I2S] Port%u TX enable failed: %d", port, err);
            _i2s_port_teardown(port);
            return false;
        }
    }

    _port[port].users |= I2S_USER_TX;
    LOG_I("[I2S] Port%u TX enabled: mode=%u slots=%u DOUT=GPIO%d MCLK=GPIO%d BCK=GPIO%d WS=GPIO%d",
          port, mode, tdmSlots, (int)doutPin, (int)mclk, (int)bck, (int)ws);
    return true;
}

void i2s_port_disable_tx(uint8_t port) {
    if (port >= I2S_PORT_COUNT) return;
    if (!(_port[port].users & I2S_USER_TX)) return;

    // Port 0 is the primary ADC clock master — never tear down to preserve MCLK continuity
    if (port == 0 && _port[0].isClockMaster) {
        LOG_I("[I2S] Port0 TX disable skipped — clock master, MCLK continuity preserved");
        _port[port].users &= ~I2S_USER_TX;
        return;
    }

    _port[port].users &= ~I2S_USER_TX;

    if (_port[port].users == 0) {
        // Last user — full teardown
        _i2s_port_teardown(port);
        LOG_I("[I2S] Port%u TX disabled — peripheral released", port);
    } else {
        // RX still active — disable TX DMA but keep peripheral up
        if (_port[port].tx) i2s_channel_disable(_port[port].tx);
        LOG_I("[I2S] Port%u TX disabled — RX still active, clocks remain", port);
    }
}

bool i2s_port_enable_rx(uint8_t port, uint8_t mode, uint8_t tdmSlots,
                         gpio_num_t dinPin, gpio_num_t mclk, gpio_num_t bck, gpio_num_t ws) {
    if (port >= I2S_PORT_COUNT) return false;

    // Already have RX — nothing to do
    if (_port[port].users & I2S_USER_RX) return true;

    uint32_t rate = _port[port].sampleRate ? _port[port].sampleRate : _currentSampleRate;

    if (_port[port].users & I2S_USER_TX) {
        // TX already active — tear down and realloc as full-duplex, then restore TX
        LOG_I("[I2S] Port%u: upgrading TX-only → full-duplex for RX", port);
        gpio_num_t savedDout    = _port[port].txDoutPin;
        uint8_t    savedTxMode  = _port[port].txMode;
        uint8_t    savedTxSlots = _port[port].txTdmSlots;
        gpio_num_t savedMclk    = _port[port].mclkPin;
        gpio_num_t savedBck     = _port[port].bckPin;
        gpio_num_t savedWs      = _port[port].lrcPin;
        _port[port].users = 0;

        if (!_i2s_port_alloc(port, true, true, true)) return false;

        // Restore TX
        bool txOk = false;
        if (savedTxMode == I2S_MODE_TDM) {
            txOk = _i2s_port_init_tx_tdm(port, rate, savedDout, savedTxSlots, savedMclk, savedBck, savedWs);
        } else {
            txOk = _i2s_port_init_tx_std(port, rate, savedDout, savedMclk, savedBck, savedWs);
        }

        // Init RX
        bool rxOk = false;
        if (mode == I2S_MODE_TDM) {
            rxOk = _i2s_port_init_rx_tdm(port, rate, dinPin, tdmSlots, mclk, bck, ws);
        } else {
            rxOk = _i2s_port_init_rx_std(port, rate, dinPin, mclk, bck, ws);
        }

        if (!rxOk) {
            _i2s_port_teardown(port);
            return false;
        }

        if (txOk && _port[port].tx) {
            esp_err_t err = i2s_channel_enable(_port[port].tx);
            if (err != ESP_OK) {
                LOG_W("[I2S] Port%u TX re-enable after RX upgrade failed: %d", port, err);
                _port[port].users &= ~I2S_USER_TX;
            } else {
                _port[port].users |= I2S_USER_TX;
            }
        }

        esp_err_t err = i2s_channel_enable(_port[port].rx);
        if (err != ESP_OK) {
            LOG_E("[I2S] Port%u RX enable failed: %d", port, err);
            _i2s_port_teardown(port);
            return false;
        }
    } else {
        // Port idle — fresh RX-only alloc
        if (!_i2s_port_alloc(port, false, true, true)) return false;

        bool rxOk = false;
        if (mode == I2S_MODE_TDM) {
            rxOk = _i2s_port_init_rx_tdm(port, rate, dinPin, tdmSlots, mclk, bck, ws);
        } else {
            rxOk = _i2s_port_init_rx_std(port, rate, dinPin, mclk, bck, ws);
        }
        if (!rxOk) {
            _i2s_port_teardown(port);
            return false;
        }

        esp_err_t err = i2s_channel_enable(_port[port].rx);
        if (err != ESP_OK) {
            LOG_E("[I2S] Port%u RX enable failed: %d", port, err);
            _i2s_port_teardown(port);
            return false;
        }
    }

    _port[port].users |= I2S_USER_RX;
    LOG_I("[I2S] Port%u RX enabled: mode=%u slots=%u DIN=GPIO%d", port, mode, tdmSlots, (int)dinPin);
    return true;
}

void i2s_port_disable_rx(uint8_t port) {
    if (port >= I2S_PORT_COUNT) return;
    if (!(_port[port].users & I2S_USER_RX)) return;

    _port[port].users &= ~I2S_USER_RX;

    if (_port[port].users == 0) {
        _i2s_port_teardown(port);
        LOG_I("[I2S] Port%u RX disabled — peripheral released", port);
    } else {
        // TX still active — disable RX DMA but keep peripheral up
        if (_port[port].rx) i2s_channel_disable(_port[port].rx);
        LOG_I("[I2S] Port%u RX disabled — TX still active", port);
    }
}

void i2s_port_write(uint8_t port, const void *src, size_t size, size_t *bw, uint32_t timeout) {
    if (port >= I2S_PORT_COUNT || !_port[port].tx || !src) {
        if (bw) *bw = 0;
        return;
    }
    i2s_channel_write(_port[port].tx, src, size, bw, timeout);
}

uint32_t i2s_port_read(uint8_t port, int32_t *dst, uint32_t frames) {
    if (port >= I2S_PORT_COUNT || !_port[port].rx || !dst) return 0;
    size_t bytes = frames * 2 * sizeof(int32_t);
    size_t br = 0;
    i2s_channel_read(_port[port].rx, dst, bytes, &br, pdMS_TO_TICKS(5));
    return (uint32_t)(br / (2 * sizeof(int32_t)));
}

uint32_t i2s_port_tdm_read(uint8_t port, int32_t *dst, uint32_t frames, uint8_t slots) {
    if (port >= I2S_PORT_COUNT || !_port[port].rx || !dst || slots == 0) return 0;
    size_t bytes = frames * slots * sizeof(int32_t);
    size_t br = 0;
    i2s_channel_read(_port[port].rx, dst, bytes, &br, pdMS_TO_TICKS(5));
    return (uint32_t)(br / (slots * sizeof(int32_t)));
}

bool i2s_port_is_tx_active(uint8_t port) {
    if (port >= I2S_PORT_COUNT) return false;
    return (_port[port].users & I2S_USER_TX) != 0;
}

bool i2s_port_is_rx_active(uint8_t port) {
    if (port >= I2S_PORT_COUNT) return false;
    return (_port[port].users & I2S_USER_RX) != 0;
}

I2sPortInfo i2s_port_get_info(uint8_t port) {
    I2sPortInfo info = {};
    info.port         = port;
    if (port >= I2S_PORT_COUNT) return info;
    info.txActive     = (_port[port].users & I2S_USER_TX) != 0;
    info.rxActive     = (_port[port].users & I2S_USER_RX) != 0;
    info.txMode       = _port[port].txMode;
    info.rxMode       = _port[port].rxMode;
    info.txTdmSlots   = _port[port].txTdmSlots;
    info.rxTdmSlots   = _port[port].rxTdmSlots;
    info.mclkPin      = (int8_t)_port[port].mclkPin;
    info.bckPin       = (int8_t)_port[port].bckPin;
    info.lrcPin       = (int8_t)_port[port].lrcPin;
    info.txDoutPin    = (int8_t)_port[port].txDoutPin;
    info.rxDinPin     = (int8_t)_port[port].rxDinPin;
    info.isClockMaster = _port[port].isClockMaster;
    return info;
}

#else // !CONFIG_IDF_TARGET_ESP32P4

// Non-P4 targets: ES8311, expansion TX, and expansion RX functions are no-ops
bool i2s_audio_enable_es8311_tx(uint32_t) { return false; }
void i2s_audio_disable_es8311_tx() {}
void i2s_audio_write_es8311(const void*, size_t, size_t* bw, uint32_t) { if (bw) *bw = 0; }
bool i2s_audio_enable_expansion_rx(uint32_t, gpio_num_t) { return false; }
void i2s_audio_disable_expansion_rx() {}
bool i2s_audio_expansion_rx_ok() { return false; }
bool i2s_audio_enable_expansion_tx(uint32_t, gpio_num_t) { return false; }
void i2s_audio_disable_expansion_tx() {}
void i2s_audio_write_expansion_tx(const void*, size_t, size_t* bw, uint32_t) { if (bw) *bw = 0; }
bool i2s_audio_enable_expansion_tdm_tx(uint32_t, gpio_num_t, uint8_t) { return false; }
void i2s_audio_write_expansion_tdm_tx(const void*, size_t, size_t* bw, uint32_t) { if (bw) *bw = 0; }
void i2s_audio_disable_expansion_tdm_tx() {}
bool i2s_port_enable_tx(uint8_t, uint8_t, uint8_t, gpio_num_t, gpio_num_t, gpio_num_t, gpio_num_t) { return false; }
void i2s_port_disable_tx(uint8_t) {}
bool i2s_port_enable_rx(uint8_t, uint8_t, uint8_t, gpio_num_t, gpio_num_t, gpio_num_t, gpio_num_t) { return false; }
void i2s_port_disable_rx(uint8_t) {}
void i2s_port_write(uint8_t, const void*, size_t, size_t* bw, uint32_t) { if (bw) *bw = 0; }
uint32_t i2s_port_read(uint8_t, int32_t*, uint32_t) { return 0; }
uint32_t i2s_port_tdm_read(uint8_t, int32_t*, uint32_t, uint8_t) { return 0; }
bool i2s_port_is_tx_active(uint8_t) { return false; }
bool i2s_port_is_rx_active(uint8_t) { return false; }
I2sPortInfo i2s_port_get_info(uint8_t port) { I2sPortInfo info = {}; info.port = port; return info; }

#endif // CONFIG_IDF_TARGET_ESP32P4

#else
// Native test stubs
static int _nativeNumAdcs = 1;
void i2s_audio_init() {}
void i2s_audio_init_channels() {}
AudioAnalysis i2s_audio_get_analysis() { return AudioAnalysis{}; }
AudioDiagnostics i2s_audio_get_diagnostics() { return AudioDiagnostics{}; }
bool i2s_audio_get_waveform(uint8_t *out, int adcIndex) { return false; }
bool i2s_audio_get_spectrum(float *bands, float *dominant_freq, int adcIndex) { return false; }
bool i2s_audio_set_sample_rate(uint32_t rate) {
    return audio_validate_sample_rate(rate);
}
int i2s_audio_get_num_adcs() { return _nativeNumAdcs; }
void audio_periodic_dump() {}
I2sStaticConfig i2s_audio_get_static_config() {
    I2sStaticConfig cfg = {};
    cfg.adc[0].isMaster = true;
    cfg.adc[0].sampleRate = 48000;
    cfg.adc[0].bitsPerSample = 32;
    cfg.adc[0].channelFormat = "Stereo R/L";
    cfg.adc[0].dmaBufCount = 4;
    cfg.adc[0].dmaBufLen = 256;
    cfg.adc[0].pllEnabled = true;
    cfg.adc[0].mclkHz = 48000 * 256;
    cfg.adc[0].commFormat = "Standard I2S";
    cfg.adc[1].isMaster = true;
    cfg.adc[1].sampleRate = 48000;
    cfg.adc[1].bitsPerSample = 32;
    cfg.adc[1].channelFormat = "Stereo R/L";
    cfg.adc[1].dmaBufCount = 4;
    cfg.adc[1].dmaBufLen = 256;
    cfg.adc[1].pllEnabled = true;
    cfg.adc[1].mclkHz = 48000 * 256;
    cfg.adc[1].commFormat = "Standard I2S";
    return cfg;
}

// Test hooks: expose cache internals for native unit tests
void _test_i2s_cache_set(int lane, const HalDeviceConfig* cfg) {
    if (lane >= 0 && lane < 2 && cfg) {
        _cachedAdcCfg[lane] = *cfg;
        _cachedAdcCfgValid[lane] = true;
    }
}
const HalDeviceConfig* _test_i2s_cache_get(int lane) {
    return (lane >= 0 && lane < 2 && _cachedAdcCfgValid[lane]) ? &_cachedAdcCfg[lane] : nullptr;
}
bool _test_i2s_cache_valid(int lane) {
    return (lane >= 0 && lane < 2) && _cachedAdcCfgValid[lane];
}
void _test_i2s_cache_reset() {
    _cachedAdcCfgValid[0] = _cachedAdcCfgValid[1] = false;
}
#endif // NATIVE_TEST
