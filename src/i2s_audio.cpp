#include "i2s_audio.h"
#include "app_state.h"
#include "audio_quality.h"
#include "config.h"
#include "debug_serial.h"
#include "signal_generator.h"
#ifdef DSP_ENABLED
#include "dsp_pipeline.h"
#endif
#ifdef DAC_ENABLED
#include "dac_hal.h"
#endif
#ifdef USB_AUDIO_ENABLED
#include "usb_audio.h"
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

#ifndef NATIVE_TEST
#include "driver/i2s_std.h"
#include <driver/gpio.h>
#include <esp_task_wdt.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

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

// ===== ADC Clock Sync Diagnostics (pure, testable) =====
//
// Computes cross-correlation between two L-channel float arrays to detect
// phase offset between ADC1 and ADC2. Uses a manual lag-search loop
// (simpler and equally efficient for ADC_SYNC_CHECK_FRAMES x ADC_SYNC_SEARCH_RANGE*2+1).
//
// Algorithm:
//   For each lag l in [-R, +R]:
//     corr[l] = sum(adc1[i] * adc2[i+l]) / frames
//   Peak lag = argmax(|corr|)
//   Peak value = corr[peak_lag] (normalized by frames above)
//   Normalization by RMS product is skipped intentionally: the pure division by
//   frames keeps the function branchless (avoids sqrt/zero-division guards)
//   and still produces a stable peak location even if absolute magnitude varies.
//   The caller should only trust inSync when correlationPeak > ~0.1.
AdcSyncDiag compute_adc_sync_diag(const float* adc1_samples, const float* adc2_samples,
                                   int frames, float sampleRateHz) {
    AdcSyncDiag result;
    result.inSync = true;
    result.phaseOffsetSamples = 0.0f;
    result.phaseOffsetUs = 0.0f;
    result.correlationPeak = 0.0f;

    if (!adc1_samples || !adc2_samples || frames <= 0 || sampleRateHz <= 0.0f) {
        return result;
    }

    const int R = ADC_SYNC_SEARCH_RANGE;
    // We need frames + R samples from adc2 (positive lags) and frames - R from adc1 (negative lags).
    // Guard: we need at least (R+1) frames to compute any lag.
    if (frames <= R) {
        return result;
    }

    // Compute usable window: central region where both signals have valid data for all lags
    // For lag l: adc1[i] * adc2[i+l], i in [0, frames-1], i+l in [0, frames-1]
    // => i in [max(0, -l), min(frames-1, frames-1-l)]
    // Use the inner window [R, frames-R-1] which is valid for all lags in [-R,+R]
    int innerStart = R;
    int innerEnd   = frames - R - 1;
    if (innerEnd <= innerStart) {
        return result;
    }
    int innerLen = innerEnd - innerStart + 1;

    float bestCorr = -2.0f;
    int   bestLag  = 0;

    for (int lag = -R; lag <= R; lag++) {
        float sum = 0.0f;
        for (int i = innerStart; i <= innerEnd; i++) {
            sum += adc1_samples[i] * adc2_samples[i + lag];
        }
        float corr = sum / (float)innerLen;
        float absCorr = (corr < 0.0f) ? -corr : corr;
        if (absCorr > bestCorr) {
            bestCorr = absCorr;
            bestLag  = lag;
        }
    }

    // Normalize: correlationPeak = bestCorr (already divided by innerLen)
    // Optionally further normalize by RMS product for 0-1 range, but guard for silence
    float rms1 = 0.0f, rms2 = 0.0f;
    for (int i = innerStart; i <= innerEnd; i++) {
        rms1 += adc1_samples[i] * adc1_samples[i];
        rms2 += adc2_samples[i] * adc2_samples[i];
    }
    rms1 = sqrtf(rms1 / (float)innerLen);
    rms2 = sqrtf(rms2 / (float)innerLen);
    float rmsProd = rms1 * rms2;
    if (rmsProd > 1e-9f) {
        result.correlationPeak = bestCorr / rmsProd;
        if (result.correlationPeak > 1.0f) result.correlationPeak = 1.0f;
        if (result.correlationPeak < 0.0f) result.correlationPeak = 0.0f;
    } else {
        // Both signals are silence — cannot determine offset, stay at defaults
        result.correlationPeak = 0.0f;
        return result;
    }

    result.phaseOffsetSamples = (float)bestLag;
    result.phaseOffsetUs = (float)bestLag / sampleRateHz * 1000000.0f;
    result.inSync = (result.phaseOffsetSamples < 0.0f
                     ? -result.phaseOffsetSamples
                     : result.phaseOffsetSamples) <= ADC_SYNC_OFFSET_THRESHOLD;
    return result;
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

// IDF5 channel handles (replaces legacy port-number addressing)
static i2s_chan_handle_t _i2s0_rx = NULL;  // ADC1 receive (I2S_NUM_0)
static i2s_chan_handle_t _i2s0_tx = NULL;  // DAC transmit (I2S_NUM_0, full-duplex pair)
static i2s_chan_handle_t _i2s1_rx = NULL;  // ADC2 receive (I2S_NUM_1)

static uint32_t _currentSampleRate = DEFAULT_AUDIO_SAMPLE_RATE;
static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t _audioTaskHandle = NULL;
static int _numAdcsDetected = 1;
static bool _adc2InitOk = false;

// ADC clock sync diagnostics (written by audio task, read by main loop)
static AdcSyncDiag _syncDiag = {};

// Per-ADC state arrays
static const float MAX_24BIT_F = 8388607.0f;

// VU meter state per input
static float _vuL[NUM_AUDIO_INPUTS] = {};
static float _vuR[NUM_AUDIO_INPUTS] = {};
static float _vuC[NUM_AUDIO_INPUTS] = {};

// Peak hold state per input
static float _peakL[NUM_AUDIO_INPUTS] = {};
static float _peakR[NUM_AUDIO_INPUTS] = {};
static float _peakC[NUM_AUDIO_INPUTS] = {};
static unsigned long _holdStartL[NUM_AUDIO_INPUTS] = {};
static unsigned long _holdStartR[NUM_AUDIO_INPUTS] = {};
static unsigned long _holdStartC[NUM_AUDIO_INPUTS] = {};

// DC-blocking IIR filter state per input
static int32_t _dcPrevInL[NUM_AUDIO_INPUTS] = {};
static int32_t _dcPrevInR[NUM_AUDIO_INPUTS] = {};
static float _dcPrevOutL[NUM_AUDIO_INPUTS] = {};
static float _dcPrevOutR[NUM_AUDIO_INPUTS] = {};

// Waveform accumulation state per input — PSRAM-allocated on ESP32, static on native
#ifdef NATIVE_TEST
static float _wfAccum[NUM_AUDIO_INPUTS][WAVEFORM_BUFFER_SIZE];
static uint8_t _wfOutput[NUM_AUDIO_INPUTS][WAVEFORM_BUFFER_SIZE];
#else
static float *_wfAccum[NUM_AUDIO_INPUTS] = {};
static uint8_t *_wfOutput[NUM_AUDIO_INPUTS] = {};
#endif
static volatile bool _wfReady[NUM_AUDIO_INPUTS] = {};
static int _wfFramesSeen[NUM_AUDIO_INPUTS] = {};
static int _wfTargetFrames = 2400; // shared, recalculated from audioUpdateRate

// FFT state per input — PSRAM-allocated on ESP32, static on native
#ifdef NATIVE_TEST
static float _fftRing[NUM_AUDIO_INPUTS][FFT_SIZE];
static float _fftData[FFT_SIZE * 2];
static float _fftWindow[FFT_SIZE];
#else
static float *_fftRing[NUM_AUDIO_INPUTS] = {};
static float *_fftData = nullptr;
static float *_fftWindow = nullptr;
#endif
static int _fftRingPos[NUM_AUDIO_INPUTS] = {};
static FftWindowType _currentWindowType = FFT_WINDOW_HANN;
static bool _fftInitialized = false;
static float _spectrumOutput[NUM_AUDIO_INPUTS][SPECTRUM_BANDS];
static float _dominantFreqOutput[NUM_AUDIO_INPUTS] = {};
static volatile bool _spectrumReady[NUM_AUDIO_INPUTS] = {};
static unsigned long _lastFftTime[NUM_AUDIO_INPUTS] = {};

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

static void i2s_configure_adc1(uint32_t sample_rate) {
    // Clean up existing channels before (re)installing
    if (_i2s0_rx) {
        i2s_channel_disable(_i2s0_rx);
        i2s_del_channel(_i2s0_rx);
        _i2s0_rx = NULL;
    }
    if (_i2s0_tx) {
        i2s_channel_disable(_i2s0_tx);
        i2s_del_channel(_i2s0_tx);
        _i2s0_tx = NULL;
    }

    // Check if DAC TX is active — preserve full-duplex mode during recovery
    bool dacTxActive = false;
#ifdef DAC_ENABLED
    dacTxActive = AppState::getInstance().dacEnabled && AppState::getInstance().dacReady;
#endif

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = DMA_BUF_COUNT;
    chan_cfg.dma_frame_num = DMA_BUF_LEN;
    chan_cfg.auto_clear = dacTxActive;  // Zero TX DMA when starved (avoids noise)

    esp_err_t err;
    if (dacTxActive) {
        err = i2s_new_channel(&chan_cfg, &_i2s0_tx, &_i2s0_rx);
    } else {
        err = i2s_new_channel(&chan_cfg, NULL, &_i2s0_rx);
    }
    if (err != ESP_OK) {
        LOG_E("[Audio] ADC1 channel create failed: 0x%x", err);
        return;
    }

    i2s_std_config_t std_cfg = {};
    std_cfg.clk_cfg.sample_rate_hz = sample_rate;
    std_cfg.clk_cfg.clk_src        = I2S_CLK_SRC_DEFAULT;  // PLL_D2_CLK (160MHz), deterministic
    std_cfg.clk_cfg.mclk_multiple  = I2S_MCLK_MULTIPLE_256;
    std_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
        I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
    std_cfg.gpio_cfg.mclk = (gpio_num_t)I2S_MCLK_PIN;
    std_cfg.gpio_cfg.bclk = (gpio_num_t)I2S_BCK_PIN;
    std_cfg.gpio_cfg.ws   = (gpio_num_t)I2S_LRC_PIN;
    std_cfg.gpio_cfg.dout = dacTxActive ? (gpio_num_t)I2S_TX_DATA_PIN : I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.din  = (gpio_num_t)I2S_DOUT_PIN;
    std_cfg.gpio_cfg.invert_flags.mclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.bclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.ws_inv   = false;

    if (_i2s0_rx) {
        i2s_channel_init_std_mode(_i2s0_rx, &std_cfg);
        i2s_channel_enable(_i2s0_rx);
    }
    if (dacTxActive && _i2s0_tx) {
        i2s_channel_init_std_mode(_i2s0_tx, &std_cfg);
        i2s_channel_enable(_i2s0_tx);
        LOG_I("[Audio] I2S0 recovery preserved TX full-duplex (data_out=GPIO%d)", I2S_TX_DATA_PIN);
    }
}

// ADC2 uses I2S_NUM_1 configured as MASTER (not slave) to bypass ESP32-S3
// slave mode constraints (bclk_div >= 8, DMA timeout). Both I2S peripherals
// derive from the same 160MHz PLL_D2_CLK with identical divider chains, giving
// frequency-locked BCK. I2S1 does NOT output any clocks — only data_in is
// connected (GPIO9). The internal RX state machine samples at the same
// frequency as I2S0's BCK, with a fixed phase offset that is well within
// the PCM1808's data valid window (~305ns of 325ns period).
static bool i2s_configure_adc2(uint32_t sample_rate) {
    // Clean up existing channel before reinstalling
    if (_i2s1_rx) {
        i2s_channel_disable(_i2s1_rx);
        i2s_del_channel(_i2s1_rx);
        _i2s1_rx = NULL;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = DMA_BUF_COUNT;
    chan_cfg.dma_frame_num = DMA_BUF_LEN;
    chan_cfg.auto_clear = false;

    esp_err_t err = i2s_new_channel(&chan_cfg, NULL, &_i2s1_rx);
    if (err != ESP_OK) {
        LOG_E("[Audio] ADC2 channel create failed: 0x%x", err);
        return false;
    }

    // Only route data_in pin — I2S1 does NOT output BCK/WS/MCK.
    // I2S0 (ADC1) provides all clock outputs to both PCM1808 boards.
    i2s_std_config_t std_cfg = {};
    std_cfg.clk_cfg.sample_rate_hz = sample_rate;
    std_cfg.clk_cfg.clk_src        = I2S_CLK_SRC_DEFAULT;
    std_cfg.clk_cfg.mclk_multiple  = I2S_MCLK_MULTIPLE_256;
    std_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
        I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
    std_cfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.bclk = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.ws   = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.dout = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.din  = (gpio_num_t)I2S_DOUT2_PIN;
    std_cfg.gpio_cfg.invert_flags.mclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.bclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.ws_inv   = false;

    err = i2s_channel_init_std_mode(_i2s1_rx, &std_cfg);
    if (err != ESP_OK) {
        LOG_E("[Audio] ADC2 channel init failed: 0x%x", err);
        i2s_del_channel(_i2s1_rx);
        _i2s1_rx = NULL;
        return false;
    }
    err = i2s_channel_enable(_i2s1_rx);
    if (err != ESP_OK) {
        LOG_E("[Audio] ADC2 channel enable failed: 0x%x", err);
        i2s_del_channel(_i2s1_rx);
        _i2s1_rx = NULL;
        return false;
    }

    // Apply pulldown AFTER channel enable — the I2S driver reconfigures the GPIO
    // matrix on enable, stripping any prior pulldown state. Without this, an
    // unconnected DOUT2 pin floats high → reads all-1s → false CLIPPING status.
    gpio_pulldown_en((gpio_num_t)I2S_DOUT2_PIN);
    return true;
}

// Dump key I2S channel info (IDF5: direct register access removed, use driver API)
static void i2s_dump_registers() {
    LOG_I("[Audio] I2S0_RX=%p I2S0_TX=%p I2S1_RX=%p",
          (void*)_i2s0_rx, (void*)_i2s0_tx, (void*)_i2s1_rx);
}

// Process a single ADC's buffer: diagnostics, DC filter, RMS, VU, peak, waveform, FFT
static void process_adc_buffer(int a, int32_t *buffer, int stereo_frames,
                                unsigned long now, float dt_ms, bool sigGenSw) {
    int total_samples = stereo_frames * 2;
    AdcDiagnostics &diag = _diagnostics.adc[a];

    // Pre-compute VU/peak exponential coefficients (3 expf calls instead of 12)
    float coeffAttack = (dt_ms > 0.0f) ? 1.0f - expf(-dt_ms / VU_ATTACK_MS) : 0.0f;
    float coeffDecay  = (dt_ms > 0.0f) ? 1.0f - expf(-dt_ms / VU_DECAY_MS) : 0.0f;
    float coeffPeakDecay = (dt_ms > 0.0f) ? 1.0f - expf(-dt_ms / PEAK_DECAY_AFTER_HOLD_MS) : 0.0f;

    // --- Pass 1: Diagnostics + DC offset + DC-blocking IIR (merged) ---
    diag.totalBuffersRead++;
    diag.lastReadMs = now;
    {
        bool allZero = true;
        uint32_t clipCount = 0;
        const int32_t CLIP_THRESHOLD = 8300000;
        float dcSum = 0.0f;
        static const float DC_BLOCK_ALPHA = 0.9987f;

        for (int f = 0; f < stereo_frames; f++) {
            int32_t rawL = buffer[f * 2];
            int32_t rawR = buffer[f * 2 + 1];
            int32_t parsedL = audio_parse_24bit_sample(rawL);
            int32_t parsedR = audio_parse_24bit_sample(rawR);

            // Diagnostics: zero/clip detection
            if (parsedL != 0 || parsedR != 0) allZero = false;
            if (parsedL > CLIP_THRESHOLD || parsedL < -CLIP_THRESHOLD) clipCount++;
            if (parsedR > CLIP_THRESHOLD || parsedR < -CLIP_THRESHOLD) clipCount++;

            // DC offset accumulation (float — matches 24-bit precision)
            dcSum += (float)parsedL + (float)parsedR;

            // DC-blocking IIR (modifies buffer in-place)
            float outL = (float)(rawL - _dcPrevInL[a]) + DC_BLOCK_ALPHA * _dcPrevOutL[a];
            _dcPrevInL[a] = rawL;
            _dcPrevOutL[a] = outL;
            buffer[f * 2] = (int32_t)outL;

            float outR = (float)(rawR - _dcPrevInR[a]) + DC_BLOCK_ALPHA * _dcPrevOutR[a];
            _dcPrevInR[a] = rawR;
            _dcPrevOutR[a] = outR;
            buffer[f * 2 + 1] = (int32_t)outR;
        }

        if (allZero) {
            diag.allZeroBuffers++;
            diag.consecutiveZeros++;
        } else {
            diag.consecutiveZeros = 0;
            diag.lastNonZeroMs = now;
        }
        diag.clippedSamples += clipCount;
        float bufferClipRate = (total_samples > 0) ? (float)clipCount / (float)total_samples : 0.0f;
        diag.clipRate = diag.clipRate * (1.0f - CLIP_RATE_ALPHA) + bufferClipRate * CLIP_RATE_ALPHA;

        float mean = (dcSum / (float)total_samples) / 8388607.0f;
        diag.dcOffset += (mean - diag.dcOffset) * 0.01f;
    }

    // === SILENCE FAST-PATH ===
    // When buffer is confirmed zeros and siggen is off, skip heavy processing
    // (DSP, RMS, waveform, FFT). Still decay VU/peak meters using pre-computed coefficients.
    if (diag.consecutiveZeros > 0 && !sigGenSw) {
        if (AppState::getInstance().vuMeterEnabled) {
            // VU decay toward 0 using pre-computed coefficient
            _vuL[a] += coeffDecay * (0.0f - _vuL[a]);
            _vuR[a] += coeffDecay * (0.0f - _vuR[a]);
            _vuC[a] += coeffDecay * (0.0f - _vuC[a]);
            // Peak hold: 0.0f input → instant capture, hold, or decay
            if (0.0f >= _peakL[a]) { _holdStartL[a] = now; _peakL[a] = 0.0f; }
            else if (now - _holdStartL[a] >= (unsigned long)PEAK_HOLD_MS)
                _peakL[a] *= (1.0f - coeffPeakDecay);
            if (0.0f >= _peakR[a]) { _holdStartR[a] = now; _peakR[a] = 0.0f; }
            else if (now - _holdStartR[a] >= (unsigned long)PEAK_HOLD_MS)
                _peakR[a] *= (1.0f - coeffPeakDecay);
            if (0.0f >= _peakC[a]) { _holdStartC[a] = now; _peakC[a] = 0.0f; }
            else if (now - _holdStartC[a] >= (unsigned long)PEAK_HOLD_MS)
                _peakC[a] *= (1.0f - coeffPeakDecay);
        } else {
            _vuL[a] = _vuR[a] = _vuC[a] = 0.0f;
            _peakL[a] = _peakR[a] = _peakC[a] = 0.0f;
        }
#ifdef DSP_ENABLED
        dsp_clear_cpu_load();
#endif
        diag.noiseFloorDbfs += (DBFS_FLOOR - diag.noiseFloorDbfs) * 0.001f;
        diag.status = audio_derive_health_status(diag);

        _analysis.adc[a].rms1 = 0.0f;
        _analysis.adc[a].rms2 = 0.0f;
        _analysis.adc[a].rmsCombined = 0.0f;
        _analysis.adc[a].vu1 = _vuL[a];
        _analysis.adc[a].vu2 = _vuR[a];
        _analysis.adc[a].vuCombined = _vuC[a];
        _analysis.adc[a].peak1 = _peakL[a];
        _analysis.adc[a].peak2 = _peakR[a];
        _analysis.adc[a].peakCombined = _peakC[a];
        _analysis.adc[a].dBFS = DBFS_FLOOR;
        return;
    }

    // DSP pipeline processing (after DC filter, before analysis)
    // Buffer contains raw left-justified I2S data (24-bit in bits [31:8]).
    // DSP normalizes by MAX_24BIT (8388607) so we must parse to right-justified
    // 24-bit first, then left-justify back after DSP for DAC output and analysis.
#ifdef DSP_ENABLED
    if (AppState::getInstance().dspEnabled && !AppState::getInstance().dspBypass) {
        // Parse: left-justified → right-justified 24-bit (>> 8)
        for (int i = 0; i < stereo_frames * 2; i++) {
            buffer[i] = audio_parse_24bit_sample(buffer[i]);
        }
        dsp_process_buffer(buffer, stereo_frames, a);
        // Restore: right-justified → left-justified (<< 8) for DAC + analysis
        for (int i = 0; i < stereo_frames * 2; i++) {
            buffer[i] = buffer[i] << 8;
        }
    } else {
        dsp_clear_cpu_load();
    }
#endif

    // DAC output moved to audio_capture_task after routing matrix application

    // --- Pass 2: RMS + waveform + FFT ring (merged single loop) ---
    {
        float sumSqL = 0.0f, sumSqR = 0.0f;
        bool wfEnabled = AppState::getInstance().waveformEnabled;
        bool spEnabled = AppState::getInstance().spectrumEnabled;

        for (int f = 0; f < stereo_frames; f++) {
            float nL = (float)audio_parse_24bit_sample(buffer[f * 2]) / MAX_24BIT_F;
            float nR = (float)audio_parse_24bit_sample(buffer[f * 2 + 1]) / MAX_24BIT_F;

            // RMS accumulation
            sumSqL += nL * nL;
            sumSqR += nR * nR;

            float combined = (nL + nR) * 0.5f;

            // Waveform accumulation
            if (wfEnabled) {
                int bin = (int)((long)(_wfFramesSeen[a] + f) * WAVEFORM_BUFFER_SIZE / _wfTargetFrames);
                if (bin < WAVEFORM_BUFFER_SIZE) {
                    if (fabsf(combined) > fabsf(_wfAccum[a][bin])) {
                        _wfAccum[a][bin] = combined;
                    }
                }
            }

            // FFT ring buffer fill
            if (spEnabled) {
                _fftRing[a][_fftRingPos[a]] = combined;
                _fftRingPos[a] = (_fftRingPos[a] + 1) % FFT_SIZE;
            }
        }

        float rmsL = (stereo_frames > 0) ? sqrtf(sumSqL / stereo_frames) : 0.0f;
        float rmsR = (stereo_frames > 0) ? sqrtf(sumSqR / stereo_frames) : 0.0f;
        float rmsC = sqrtf((rmsL * rmsL + rmsR * rmsR) * 0.5f);
        float dBFS = audio_rms_to_dbfs(rmsC);

        // Waveform buffer flush
        if (wfEnabled) {
            _wfFramesSeen[a] += stereo_frames;
            if (_wfFramesSeen[a] >= _wfTargetFrames) {
                for (int i = 0; i < WAVEFORM_BUFFER_SIZE; i++) {
                    _wfOutput[a][i] = audio_quantize_sample(_wfAccum[a][i]);
                    _wfAccum[a][i] = 0.0f;
                }
                _wfFramesSeen[a] = 0;
                _wfReady[a] = true;
            }
        }

        // VU metering with pre-computed coefficients (inline — avoids 12 expf calls)
        if (AppState::getInstance().vuMeterEnabled) {
            _vuL[a] += ((rmsL > _vuL[a]) ? coeffAttack : coeffDecay) * (rmsL - _vuL[a]);
            _vuR[a] += ((rmsR > _vuR[a]) ? coeffAttack : coeffDecay) * (rmsR - _vuR[a]);
            _vuC[a] += ((rmsC > _vuC[a]) ? coeffAttack : coeffDecay) * (rmsC - _vuC[a]);

            // Peak hold with pre-computed decay coefficient
            if (rmsL >= _peakL[a]) { _holdStartL[a] = now; _peakL[a] = rmsL; }
            else if (now - _holdStartL[a] >= (unsigned long)PEAK_HOLD_MS) {
                float d = _peakL[a] * (1.0f - coeffPeakDecay);
                _peakL[a] = (d > rmsL) ? d : rmsL;
            }
            if (rmsR >= _peakR[a]) { _holdStartR[a] = now; _peakR[a] = rmsR; }
            else if (now - _holdStartR[a] >= (unsigned long)PEAK_HOLD_MS) {
                float d = _peakR[a] * (1.0f - coeffPeakDecay);
                _peakR[a] = (d > rmsR) ? d : rmsR;
            }
            if (rmsC >= _peakC[a]) { _holdStartC[a] = now; _peakC[a] = rmsC; }
            else if (now - _holdStartC[a] >= (unsigned long)PEAK_HOLD_MS) {
                float d = _peakC[a] * (1.0f - coeffPeakDecay);
                _peakC[a] = (d > rmsC) ? d : rmsC;
            }
        } else {
            _vuL[a] = _vuR[a] = _vuC[a] = 0.0f;
            _peakL[a] = _peakR[a] = _peakC[a] = 0.0f;
        }

        // FFT compute (runs at audioUpdateRate, not every buffer)
        if (spEnabled && now - _lastFftTime[a] >= AppState::getInstance().audioUpdateRate) {
            _lastFftTime[a] = now;

            // Check if window type changed at runtime
            FftWindowType wantedWindow = AppState::getInstance().fftWindowType;
            if (wantedWindow != _currentWindowType) {
                i2s_audio_apply_window(wantedWindow);
            }

            // Copy ring buffer into interleaved complex format with window
            for (int i = 0; i < FFT_SIZE; i++) {
                float sample = _fftRing[a][(_fftRingPos[a] + i) % FFT_SIZE];
                _fftData[i * 2] = sample * _fftWindow[i];     // Real
                _fftData[i * 2 + 1] = 0.0f;                   // Imaginary
            }

            // ESP-DSP Radix-4 FFT + bit reversal (20-27% faster than Radix-2)
            dsps_fft4r_fc32(_fftData, FFT_SIZE);
            dsps_bit_rev4r_fc32(_fftData, FFT_SIZE);
            dsps_cplx2real_fc32(_fftData, FFT_SIZE);

            // Compute magnitudes in-place (overwrite first FFT_SIZE/2 entries)
            float maxMag = 0.0f;
            int maxBin = 0;
            for (int i = 0; i < FFT_SIZE / 2; i++) {
                float re = _fftData[i * 2];
                float im = _fftData[i * 2 + 1];
                float mag = sqrtf(re * re + im * im);
                _fftData[i] = mag;
                if (i > 0 && mag > maxMag) {
                    maxMag = mag;
                    maxBin = i;
                }
            }
            _dominantFreqOutput[a] = (float)maxBin * (float)_currentSampleRate / (float)FFT_SIZE;

            // SNR/SFDR analysis (computed from magnitude spectrum)
            AppState::getInstance().audioSnrDb[a] = dsps_snr_f32(_fftData, FFT_SIZE / 2, 0);
            AppState::getInstance().audioSfdrDb[a] = dsps_sfdr_f32(_fftData, FFT_SIZE / 2, 0);

            audio_aggregate_fft_bands(_fftData, FFT_SIZE, (float)_currentSampleRate,
                                      _spectrumOutput[a], SPECTRUM_BANDS);
            _spectrumReady[a] = true;
        }

        // Noise floor and peak tracking (only when siggen is off)
        if (!sigGenSw) {
            if (dBFS > diag.noiseFloorDbfs) {
                diag.noiseFloorDbfs += (dBFS - diag.noiseFloorDbfs) * 0.01f;
            } else {
                diag.noiseFloorDbfs += (dBFS - diag.noiseFloorDbfs) * 0.001f;
            }
            if (dBFS > diag.peakDbfs) diag.peakDbfs = dBFS;
        }
        // Clipping check for health: only flag when siggen is off
        AdcDiagnostics diagCopy = diag;
        if (sigGenSw) diagCopy.clipRate = 0.0f; // Mask siggen-induced clipping
        diag.status = audio_derive_health_status(diagCopy);

        // Write per-ADC analysis into shared struct
        _analysis.adc[a].rms1 = rmsL;
        _analysis.adc[a].rms2 = rmsR;
        _analysis.adc[a].rmsCombined = rmsC;
        _analysis.adc[a].vu1 = _vuL[a];
        _analysis.adc[a].vu2 = _vuR[a];
        _analysis.adc[a].vuCombined = _vuC[a];
        _analysis.adc[a].peak1 = _peakL[a];
        _analysis.adc[a].peak2 = _peakR[a];
        _analysis.adc[a].peakCombined = _peakC[a];
        _analysis.adc[a].dBFS = dBFS;
    } // end Pass 2 scope
}

#ifdef USB_AUDIO_ENABLED
// Apply USB host volume control to buffer (in-place, linear gain 0.0-1.0)
static void applyHostVolume(int32_t *buf, int frames, float volLinear) {
    if (volLinear >= 0.999f) return;  // Unity gain, skip
    for (int i = 0; i < frames * 2; i++) {
        buf[i] = (int32_t)((float)buf[i] * volLinear);
    }
}
#endif

static void audio_capture_task(void *param) {
    const int BUFFER_SAMPLES = DMA_BUF_LEN * 2; // stereo
    // Static to keep off task stack (4KB) — safe since only one audio task instance
    static int32_t buf1[BUFFER_SAMPLES];
    static int32_t buf2[BUFFER_SAMPLES];
#ifdef USB_AUDIO_ENABLED
    static int32_t *s_bufUsb = nullptr;
    if (!s_bufUsb) {
        s_bufUsb = (int32_t *)heap_caps_calloc(BUFFER_SAMPLES, sizeof(int32_t), MALLOC_CAP_SPIRAM);
        if (!s_bufUsb) s_bufUsb = (int32_t *)calloc(BUFFER_SAMPLES, sizeof(int32_t));
    }
#endif

    unsigned long prevTime = millis();
    unsigned long lastDumpTime = 0;
    bool adc2FirstReadLogged = false;

    // Metrics counters for throughput and latency
    uint32_t bufCount[NUM_AUDIO_ADCS] = {};
    unsigned long readLatencyAccumUs[NUM_AUDIO_ADCS] = {};
    uint32_t readLatencyCount[NUM_AUDIO_ADCS] = {};
    unsigned long lastMetricsTime = millis();

    // I2S timeout recovery state
    uint32_t consecutiveTimeouts = 0;
    static const uint32_t TIMEOUT_RECOVERY_THRESHOLD = 10; // ~5s at 500ms timeout

    // Register this task with the Task Watchdog Timer
    esp_task_wdt_add(NULL);

    static int32_t *s_dacBuf = nullptr;
    if (!s_dacBuf) {
        s_dacBuf = (int32_t *)heap_caps_calloc(DMA_BUF_LEN * 2, sizeof(int32_t), MALLOC_CAP_SPIRAM);
        if (!s_dacBuf) s_dacBuf = (int32_t *)calloc(DMA_BUF_LEN * 2, sizeof(int32_t));
    }

    while (true) {
        // Feed watchdog at the top of every iteration (even on timeout path)
        esp_task_wdt_reset();

        // Pause I2S reads when DAC is reinitializing the I2S driver
        if (AppState::getInstance().audioPaused) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        size_t bytes_read1 = 0;
        size_t bytes_read2 = 0;
        bool adc2Ok = false;

        // If both ADCs are disabled, sleep longer to reduce CPU usage
        if (!AppState::getInstance().adcEnabled[0] && !AppState::getInstance().adcEnabled[1]) {
            memset(buf1, 0, sizeof(buf1));
            memset(buf2, 0, sizeof(buf2));
            bytes_read1 = sizeof(buf1);
            vTaskDelay(pdMS_TO_TICKS(50));
            // Fall through to downstream processing with zero-filled buffers
        } else {

        // Read ADC1 (master) — 500ms timeout instead of portMAX_DELAY
        if (AppState::getInstance().adcEnabled[0]) {
            unsigned long t0 = micros();
            esp_err_t err1 = i2s_channel_read(_i2s0_rx, buf1,
                                               sizeof(buf1), &bytes_read1, pdMS_TO_TICKS(500));
            unsigned long t1 = micros();

            if (err1 != ESP_OK || bytes_read1 == 0) {
                if (err1 != ESP_OK) _diagnostics.adc[0].i2sReadErrors++;
                if (bytes_read1 == 0) _diagnostics.adc[0].zeroByteReads++;

                // Track consecutive timeouts for I2S recovery
                consecutiveTimeouts++;
                if (consecutiveTimeouts >= TIMEOUT_RECOVERY_THRESHOLD) {
                    LOG_W("[Audio] ADC1 %lu consecutive timeouts — attempting I2S recovery",
                          (unsigned long)consecutiveTimeouts);
                    // i2s_configure_adc1 disables+deletes existing channels internally
                    i2s_configure_adc1(_currentSampleRate);
                    _diagnostics.adc[0].i2sRecoveries++;
                    consecutiveTimeouts = 0;
                    LOG_I("[Audio] I2S recovery #%lu complete",
                          (unsigned long)_diagnostics.adc[0].i2sRecoveries);
                }
                vTaskDelay(pdMS_TO_TICKS(1));
                continue;
            }
            consecutiveTimeouts = 0; // Reset on successful read
            readLatencyAccumUs[0] += (t1 - t0);
            readLatencyCount[0]++;
            bufCount[0]++;
        } else {
            // ADC1 disabled — zero-fill buffer, still need ADC2 timing from I2S0 clocks
            memset(buf1, 0, sizeof(buf1));
            bytes_read1 = sizeof(buf1);
        }

        // Read ADC2 — near-instant if synced DMA is ready
        if (_adc2InitOk && AppState::getInstance().adcEnabled[1]) {
            unsigned long t2 = micros();
            esp_err_t err2 = i2s_channel_read(_i2s1_rx, buf2,
                                               sizeof(buf2), &bytes_read2, pdMS_TO_TICKS(5));
            unsigned long t3 = micros();
            if (err2 == ESP_OK && bytes_read2 > 0) {
                adc2Ok = true;
                readLatencyAccumUs[1] += (t3 - t2);
                readLatencyCount[1]++;
                bufCount[1]++;
            } else {
                if (err2 != ESP_OK) _diagnostics.adc[1].i2sReadErrors++;
                if (bytes_read2 == 0) _diagnostics.adc[1].zeroByteReads++;
            }
        } else if (!AppState::getInstance().adcEnabled[1]) {
            // ADC2 disabled — zero-fill
            memset(buf2, 0, sizeof(buf2));
        }

        // One-shot startup diagnostic for ADC2
        if (!adc2FirstReadLogged) {
            if (adc2Ok) {
                LOG_I("[Audio] ADC2 first read OK: %d bytes, samples[0..3]=%08lX %08lX %08lX %08lX",
                      (int)bytes_read2,
                      (unsigned long)buf2[0], (unsigned long)buf2[1],
                      (unsigned long)buf2[2], (unsigned long)buf2[3]);
                adc2FirstReadLogged = true;
            } else if (_diagnostics.adc[1].zeroByteReads >= 50) {
                LOG_W("[Audio] ADC2 no data after 50 reads (DMA timeout — slave not clocking)");
                adc2FirstReadLogged = true;
            }
        }

        } // end of else (at least one ADC enabled)

        unsigned long now = millis();
        float dt_ms = (float)(now - prevTime);
        prevTime = now;

        int stereo_frames1 = (bytes_read1 / sizeof(int32_t)) / 2;
        int stereo_frames2 = adc2Ok ? (bytes_read2 / sizeof(int32_t)) / 2 : 0;

        bool sigGenSw = siggen_is_active() && siggen_is_software_mode();
        _diagnostics.sigGenActive = sigGenSw;

        // Signal generator injection (before per-ADC processing)
        int targetAdc = AppState::getInstance().sigGenTargetAdc;
        if (sigGenSw) {
            if (targetAdc == SIGTARGET_ADC1 || targetAdc == SIGTARGET_BOTH || targetAdc == SIGTARGET_ALL)
                siggen_fill_buffer(buf1, stereo_frames1, _currentSampleRate);
            if ((targetAdc == SIGTARGET_ADC2 || targetAdc == SIGTARGET_BOTH || targetAdc == SIGTARGET_ALL) && adc2Ok)
                siggen_fill_buffer(buf2, stereo_frames2, _currentSampleRate);
        }

        // Process ADC1
        process_adc_buffer(0, buf1, stereo_frames1, now, dt_ms, sigGenSw);
        if (appState.audioQualityEnabled) {
            audio_quality_scan_buffer(0, buf1, stereo_frames1);
        }

        // Process ADC2 (if available)
        if (adc2Ok) {
            process_adc_buffer(1, buf2, stereo_frames2, now, dt_ms, sigGenSw);
            if (appState.audioQualityEnabled) {
                audio_quality_scan_buffer(1, buf2, stereo_frames2);
            }
        } else {
            // ADC2 not processed — zero its post-DSP channels to prevent stale data in routing
#ifdef DSP_ENABLED
            dsp_zero_channels(1);
#endif
            if (_adc2InitOk) {
                // Slave read failed — still update diagnostics so health status reflects reality
                AdcDiagnostics &diag = _diagnostics.adc[1];
                diag.consecutiveZeros++;
                diag.allZeroBuffers++;
                diag.status = audio_derive_health_status(diag);
            }
        }

        // ===== USB Audio Input Processing =====
#ifdef USB_AUDIO_ENABLED
        {
            bool usbEnabled = AppState::getInstance().adcEnabled[2];
            bool usbStreaming = usb_audio_is_streaming();
            bool sigGenTargetsUsb = sigGenSw &&
                (targetAdc == SIGTARGET_USB || targetAdc == SIGTARGET_ALL);

            if (usbEnabled && (usbStreaming || sigGenTargetsUsb)) {
                uint32_t framesRead = usb_audio_read(s_bufUsb, DMA_BUF_LEN);

                // Zero-fill remainder on underrun
                if (framesRead < (uint32_t)DMA_BUF_LEN) {
                    memset(s_bufUsb + framesRead * 2, 0,
                           ((uint32_t)DMA_BUF_LEN - framesRead) * 2 * sizeof(int32_t));
                }

                // Apply host volume/mute BEFORE DSP
                if (usb_audio_get_mute()) {
                    memset(s_bufUsb, 0, DMA_BUF_LEN * 2 * sizeof(int32_t));
                } else {
                    applyHostVolume(s_bufUsb, DMA_BUF_LEN, usb_audio_get_volume_linear());
                }

                // Signal generator injection for USB target
                if (sigGenTargetsUsb) {
                    siggen_fill_buffer(s_bufUsb, DMA_BUF_LEN, _currentSampleRate);
                }

                process_adc_buffer(2, s_bufUsb, DMA_BUF_LEN, now, dt_ms, sigGenSw);
                if (appState.audioQualityEnabled) {
                    audio_quality_scan_buffer(2, s_bufUsb, DMA_BUF_LEN);
                }
            } else {
                // USB not processed — zero its post-DSP channels to prevent stale data in routing
#ifdef DSP_ENABLED
                dsp_zero_channels(2);
#endif
                if (usbEnabled && !usbStreaming) {
                    // USB enabled but not streaming — update diagnostics
                    AdcDiagnostics &diag = _diagnostics.adc[2];
                    diag.consecutiveZeros++;
                    diag.status = AUDIO_NO_DATA;
                }
            }
        }
#endif

        // ADC clock sync check (every ADC_SYNC_CHECK_INTERVAL_MS, both ADCs active with signal)
        // Runs inside audio task on Core 1 — NO Serial/LOG here; use dirty-flag pattern.
        {
            static unsigned long lastSyncCheckMs = 0;
            if (_numAdcsDetected >= 2 &&
                _diagnostics.adc[0].status == AUDIO_OK &&
                _diagnostics.adc[1].status == AUDIO_OK &&
                now - lastSyncCheckMs >= ADC_SYNC_CHECK_INTERVAL_MS) {
                lastSyncCheckMs = now;

                // Extract up to ADC_SYNC_CHECK_FRAMES + ADC_SYNC_SEARCH_RANGE samples
                // from L-channel of each ADC buffer (normalized float)
                const int needed = ADC_SYNC_CHECK_FRAMES + ADC_SYNC_SEARCH_RANGE;
                const int avail1 = (stereo_frames1 < needed) ? stereo_frames1 : needed;
                const int avail2 = (stereo_frames2 < needed) ? stereo_frames2 : needed;
                const int usable = (avail1 < avail2) ? avail1 : avail2;

                if (usable >= ADC_SYNC_CHECK_FRAMES) {
                    // Stack buffers: 2 × (64+8) × 4 bytes = 576 bytes — safe for audio task stack
                    float s1[ADC_SYNC_CHECK_FRAMES + ADC_SYNC_SEARCH_RANGE];
                    float s2[ADC_SYNC_CHECK_FRAMES + ADC_SYNC_SEARCH_RANGE];
                    const float MAX24F = 8388607.0f;
                    for (int i = 0; i < usable; i++) {
                        s1[i] = (float)audio_parse_24bit_sample(buf1[i * 2]) / MAX24F;
                        s2[i] = (float)audio_parse_24bit_sample(buf2[i * 2]) / MAX24F;
                    }

                    AdcSyncDiag sd = compute_adc_sync_diag(s1, s2, usable,
                                                           (float)_currentSampleRate);
                    sd.lastCheckMs = now;

                    portENTER_CRITICAL_ISR(&spinlock);
                    sd.checkCount    = _syncDiag.checkCount + 1;
                    sd.outOfSyncCount = _syncDiag.outOfSyncCount + (sd.inSync ? 0u : 1u);
                    _syncDiag = sd;
                    portEXIT_CRITICAL_ISR(&spinlock);
                }
            }
        }

        // Periodic dump: just set flag, main loop does the actual LOG calls.
        // Serial.print blocks at low baud rates, starving I2S DMA buffers.
        if (now - lastDumpTime >= 5000) {
            lastDumpTime = now;
            _dumpReady = true;
        }

        // Detect number of active ADCs (check every buffer)
        if (_adc2InitOk && _diagnostics.adc[1].consecutiveZeros < 50) {
            _numAdcsDetected = 2;
        } else {
            _numAdcsDetected = 1;
        }
        static int prevNumAdcs = 1;
        if (_numAdcsDetected != prevNumAdcs) {
            LOG_I("[Audio] ADCs detected: %d -> %d", prevNumAdcs, _numAdcsDetected);
            prevNumAdcs = _numAdcsDetected;
        }
        _diagnostics.numAdcsDetected = _numAdcsDetected;

        // Track total inputs detected (ADCs + USB)
        int totalInputs = _numAdcsDetected;
#ifdef USB_AUDIO_ENABLED
        if (usb_audio_is_streaming()) totalInputs++;
#endif
        _diagnostics.numInputsDetected = totalInputs;

        // Update runtime metrics every 1 second (gated by debug toggle)
        bool metricsEnabled = AppState::getInstance().debugMode &&
                              AppState::getInstance().debugI2sMetrics;
        unsigned long metricsNow = millis();
        if (metricsNow - lastMetricsTime >= 1000) {
            float elapsed_s = (float)(metricsNow - lastMetricsTime) / 1000.0f;
            AppState &as = AppState::getInstance();
            if (metricsEnabled) {
                for (int a = 0; a < NUM_AUDIO_ADCS; a++) {
                    as.i2sMetrics.buffersPerSec[a] = bufCount[a] / elapsed_s;
                    as.i2sMetrics.avgReadLatencyUs[a] = readLatencyCount[a] > 0
                        ? (float)readLatencyAccumUs[a] / readLatencyCount[a] : 0;
                }
                if (_audioTaskHandle) {
                    as.i2sMetrics.audioTaskStackFree =
                        uxTaskGetStackHighWaterMark(_audioTaskHandle) * 4;
                }
            } else {
                // Zero out stale metrics when disabled
                memset(&as.i2sMetrics, 0, sizeof(as.i2sMetrics));
            }
            for (int a = 0; a < NUM_AUDIO_ADCS; a++) {
                bufCount[a] = 0;
                readLatencyAccumUs[a] = 0;
                readLatencyCount[a] = 0;
            }
            lastMetricsTime = metricsNow;
        }

        // Recalculate waveform target on both ADCs
        _wfTargetFrames = _currentSampleRate * AppState::getInstance().audioUpdateRate / 1000;

        // ===== DAC Output via Routing Matrix =====
#ifdef DAC_ENABLED
        if (AppState::getInstance().dacEnabled && AppState::getInstance().dacReady) {
#ifdef DSP_ENABLED
            if (AppState::getInstance().dspEnabled && !AppState::getInstance().dspBypass) {
                // DSP active: route through 6x6 matrix
                dsp_routing_execute(s_dacBuf, stereo_frames1);
                dac_output_write(s_dacBuf, stereo_frames1);
            } else
#endif
            {
                // DSP bypassed: direct source selection
                uint8_t src = AppState::getInstance().dacSourceInput;
                if (src == 0) {
                    dac_output_write(buf1, stereo_frames1);
                } else if (src == 1 && adc2Ok) {
                    dac_output_write(buf2, stereo_frames2);
                }
#ifdef USB_AUDIO_ENABLED
                else if (src == 2 && AppState::getInstance().adcEnabled[2] && usb_audio_is_streaming()) {
                    dac_output_write(s_bufUsb, DMA_BUF_LEN);
                }
#endif
            }
        }
#endif

        // Combined analysis: overall dBFS = max across all inputs, signal detected = any
        float overallDbfs = _analysis.adc[0].dBFS;
        if (_numAdcsDetected >= 2 && _analysis.adc[1].dBFS > overallDbfs) {
            overallDbfs = _analysis.adc[1].dBFS;
        }
#ifdef USB_AUDIO_ENABLED
        if (AppState::getInstance().adcEnabled[2] && usb_audio_is_streaming() &&
            _analysis.adc[2].dBFS > overallDbfs) {
            overallDbfs = _analysis.adc[2].dBFS;
        }
#endif
        float threshold = AppState::getInstance().audioThreshold_dBFS;

        portENTER_CRITICAL_ISR(&spinlock);
        _analysis.dBFS = overallDbfs;
        _analysis.signalDetected = (overallDbfs >= threshold);
        _analysis.timestamp = now;
        _analysisReady = true;
        portEXIT_CRITICAL_ISR(&spinlock);

        // Yield 2 ticks so IDLE0 can feed the Task Watchdog Timer.
        // DMA has 8 buffers = ~42ms runway, so 2ms yield is safe.
        vTaskDelay(2);
    }
}

// Dual I2S Master Architecture:
// Both PCM1808 ADCs use master-mode I2S (not slave — ESP32-S3 slave DMA issues).
// I2S0 outputs BCK/WS/MCLK; I2S1 has data_in only (GPIO9).
// Init order: ADC2 first, then ADC1 (clock source). See i2s_configure_adc2().
void i2s_audio_init() {
    _currentSampleRate = AppState::getInstance().audioSampleRate;
    if (!audio_validate_sample_rate(_currentSampleRate)) {
        _currentSampleRate = DEFAULT_AUDIO_SAMPLE_RATE;
    }

    // Reset diagnostics
    _diagnostics = AudioDiagnostics{};

    // Allocate FFT/waveform buffers from PSRAM (one-time, ~22.5KB off internal SRAM)
    // Use heap_caps_calloc directly — ps_calloc requires psramFound() which may not
    // be initialized on all board configs, but MALLOC_CAP_SPIRAM works via heap_caps.
    if (!_fftData) {
        _fftData    = (float *)heap_caps_calloc(FFT_SIZE * 2, sizeof(float), MALLOC_CAP_SPIRAM);
        _fftWindow  = (float *)heap_caps_calloc(FFT_SIZE, sizeof(float), MALLOC_CAP_SPIRAM);
        for (int a = 0; a < NUM_AUDIO_INPUTS; a++) {
            _fftRing[a]  = (float *)heap_caps_calloc(FFT_SIZE, sizeof(float), MALLOC_CAP_SPIRAM);
            _wfAccum[a]  = (float *)heap_caps_calloc(WAVEFORM_BUFFER_SIZE, sizeof(float), MALLOC_CAP_SPIRAM);
            _wfOutput[a] = (uint8_t *)heap_caps_calloc(WAVEFORM_BUFFER_SIZE, sizeof(uint8_t), MALLOC_CAP_SPIRAM);
        }
        // Fallback to internal SRAM if PSRAM unavailable
        if (!_fftData)   _fftData   = (float *)calloc(FFT_SIZE * 2, sizeof(float));
        if (!_fftWindow) _fftWindow = (float *)calloc(FFT_SIZE, sizeof(float));
        for (int a = 0; a < NUM_AUDIO_INPUTS; a++) {
            if (!_fftRing[a])  _fftRing[a]  = (float *)calloc(FFT_SIZE, sizeof(float));
            if (!_wfAccum[a])  _wfAccum[a]  = (float *)calloc(WAVEFORM_BUFFER_SIZE, sizeof(float));
            if (!_wfOutput[a]) _wfOutput[a] = (uint8_t *)calloc(WAVEFORM_BUFFER_SIZE, sizeof(uint8_t));
        }
        if (!_fftData || !_fftWindow) {
            LOG_E("[Audio] FATAL: FFT buffer allocation failed!");
            return;
        }
        LOG_I("[Audio] FFT/waveform buffers allocated (%s)",
              heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0 ? "PSRAM" : "internal");
    }

    _wfTargetFrames = _currentSampleRate * AppState::getInstance().audioUpdateRate / 1000;
    for (int a = 0; a < NUM_AUDIO_INPUTS; a++) {
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
        i2s_audio_apply_window(AppState::getInstance().fftWindowType);
        _fftInitialized = true;
    }

    // Both I2S peripherals configured as master RX. I2S1 (ADC2) does NOT output
    // any clocks — I2S0 (ADC1) provides BCK/WS/MCLK to both PCM1808 boards.
    // I2S1 uses its own internal clock chain (same PLL, same dividers) to sample
    // data from GPIO9. This bypasses ESP32-S3 slave mode DMA issues entirely.
    _adc2InitOk = i2s_configure_adc2(_currentSampleRate);
    i2s_configure_adc1(_currentSampleRate);

    // Dump channel handles for debugging (pulldown now applied inside i2s_configure_adc2)
    if (_adc2InitOk) {
        i2s_dump_registers();
    }
    _numAdcsDetected = 1; // Will be updated once data flows

#ifdef DSP_ENABLED
    dsp_init();
#endif

#ifdef DAC_ENABLED
    dac_output_init();
#endif

    xTaskCreatePinnedToCore(
        audio_capture_task,
        "audio_cap",
        TASK_STACK_SIZE_AUDIO,
        NULL,
        TASK_PRIORITY_AUDIO,
        &_audioTaskHandle,
        TASK_CORE_AUDIO  // Core 1 — isolates audio from WiFi system tasks on Core 0
    );

    LOG_I("[Audio] I2S initialized: %lu Hz, BCK=%d, DOUT1=%d, DOUT2=%d, LRC=%d, MCLK=%d, ADC2=%s",
          _currentSampleRate, I2S_BCK_PIN, I2S_DOUT_PIN, I2S_DOUT2_PIN,
          I2S_LRC_PIN, I2S_MCLK_PIN, _adc2InitOk ? "OK" : "FAIL");
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

AdcSyncDiag i2s_audio_get_sync_diag() {
    AdcSyncDiag result;
    portENTER_CRITICAL(&spinlock);
    result = _syncDiag;
    portEXIT_CRITICAL(&spinlock);
    return result;
}

void audio_periodic_dump() {
    if (!_dumpReady) return;
    _dumpReady = false;

    // Per-ADC log — distinguishes failure modes:
    // zb high + az=0 + tot=0 → DMA timeout, slave not clocking
    // zb low  + az high      → Slave clocking OK, no audio
    // errs > 0               → I2S driver error (bus fault, DMA overflow)
    LOG_I("[Audio] ADC1=%.1fdB flr=%.1f st=%d errs=%lu zb=%lu az=%lu cz=%lu tot=%lu adcs=%d",
          _analysis.adc[0].dBFS, _diagnostics.adc[0].noiseFloorDbfs,
          _diagnostics.adc[0].status,
          _diagnostics.adc[0].i2sReadErrors,
          _diagnostics.adc[0].zeroByteReads,
          _diagnostics.adc[0].allZeroBuffers,
          _diagnostics.adc[0].consecutiveZeros,
          _diagnostics.adc[0].totalBuffersRead,
          _numAdcsDetected);
    if (_adc2InitOk) {
        LOG_I("[Audio] ADC2=%.1fdB flr=%.1f st=%d errs=%lu zb=%lu az=%lu cz=%lu tot=%lu",
              _analysis.adc[1].dBFS, _diagnostics.adc[1].noiseFloorDbfs,
              _diagnostics.adc[1].status,
              _diagnostics.adc[1].i2sReadErrors,
              _diagnostics.adc[1].zeroByteReads,
              _diagnostics.adc[1].allZeroBuffers,
              _diagnostics.adc[1].consecutiveZeros,
              _diagnostics.adc[1].totalBuffersRead);
    }
#ifdef DAC_ENABLED
    dac_periodic_log();
#endif
}

bool i2s_audio_get_waveform(uint8_t *out, int adcIndex) {
    if (adcIndex < 0 || adcIndex >= NUM_AUDIO_INPUTS) return false;
    if (!_wfReady[adcIndex]) return false;
    memcpy(out, (const void *)_wfOutput[adcIndex], WAVEFORM_BUFFER_SIZE);
    _wfReady[adcIndex] = false;
    return true;
}

bool i2s_audio_get_spectrum(float *bands, float *dominant_freq, int adcIndex) {
    if (adcIndex < 0 || adcIndex >= NUM_AUDIO_INPUTS) return false;
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
    cfg.adc[0].apllEnabled = true;
    cfg.adc[0].mclkHz = _currentSampleRate * 256;
    cfg.adc[0].commFormat = "Standard I2S";
    // ADC2 — Master (no clock output, data-only)
    cfg.adc[1].isMaster = true;
    cfg.adc[1].sampleRate = _currentSampleRate;
    cfg.adc[1].bitsPerSample = 32;
    cfg.adc[1].channelFormat = "Stereo R/L";
    cfg.adc[1].dmaBufCount = DMA_BUF_COUNT;
    cfg.adc[1].dmaBufLen = DMA_BUF_LEN;
    cfg.adc[1].apllEnabled = true;
    cfg.adc[1].mclkHz = _currentSampleRate * 256;
    cfg.adc[1].commFormat = "Standard I2S";
    return cfg;
}

void i2s_audio_uninstall_drivers() {
    LOG_I("[Audio] Uninstalling I2S drivers to free DMA buffers");
    if (_i2s0_rx) { i2s_channel_disable(_i2s0_rx); i2s_del_channel(_i2s0_rx); _i2s0_rx = NULL; }
    if (_i2s0_tx) { i2s_channel_disable(_i2s0_tx); i2s_del_channel(_i2s0_tx); _i2s0_tx = NULL; }
    if (_adc2InitOk && _i2s1_rx) {
        i2s_channel_disable(_i2s1_rx);
        i2s_del_channel(_i2s1_rx);
        _i2s1_rx = NULL;
    }
}

void i2s_audio_reinstall_drivers() {
    LOG_I("[Audio] Reinstalling I2S drivers");
    // i2s_configure_adc* internally clean up any lingering handles
    if (_adc2InitOk) _adc2InitOk = i2s_configure_adc2(_currentSampleRate);
    i2s_configure_adc1(_currentSampleRate);
    LOG_I("[Audio] I2S drivers reinstalled at %lu Hz", _currentSampleRate);
}

bool i2s_audio_set_sample_rate(uint32_t rate) {
    if (!audio_validate_sample_rate(rate)) return false;
    if (rate == _currentSampleRate) return true;

    LOG_I("[Audio] Changing sample rate: %lu -> %lu Hz", _currentSampleRate, rate);

    _currentSampleRate = rate;
    _wfTargetFrames = rate * AppState::getInstance().audioUpdateRate / 1000;
    for (int a = 0; a < NUM_AUDIO_INPUTS; a++) {
        _wfFramesSeen[a] = 0;
        if (_wfAccum[a]) memset(_wfAccum[a], 0, WAVEFORM_BUFFER_SIZE * sizeof(float));
    }

    // i2s_configure_adc* disable and delete existing channels before recreating
    if (_adc2InitOk) _adc2InitOk = i2s_configure_adc2(rate);
    i2s_configure_adc1(rate);

    LOG_I("[Audio] Sample rate changed to %lu Hz", rate);
    return true;
}

// ===== I2S TX channel management (called by dac_hal.cpp) =====

bool i2s_audio_enable_tx(uint32_t sampleRate) {
    if (_i2s0_tx != NULL) return true;  // Already in full-duplex mode

    LOG_I("[Audio] Enabling I2S TX full-duplex on I2S0, data_out=GPIO%d", I2S_TX_DATA_PIN);

    // Pause audio task during driver reinit
    AppState::getInstance().audioPaused = true;
    vTaskDelay(pdMS_TO_TICKS(50));

    // i2s_configure_adc1 detects dacEnabled && dacReady from AppState.
    // Caller (dac_hal) must ensure both are true before calling this function.
    i2s_configure_adc1(sampleRate);

    AppState::getInstance().audioPaused = false;

    if (_i2s0_tx != NULL) {
        LOG_I("[Audio] I2S TX enabled: rate=%luHz data_out=GPIO%d MCLK=%luHz DMA=%dx%d",
              (unsigned long)sampleRate, I2S_TX_DATA_PIN,
              (unsigned long)(sampleRate * 256), DMA_BUF_COUNT, DMA_BUF_LEN);
        return true;
    }
    LOG_E("[Audio] I2S TX enable failed (dacEnabled/dacReady not set?)");
    return false;
}

void i2s_audio_disable_tx() {
    if (_i2s0_tx == NULL) return;

    LOG_I("[Audio] Disabling I2S TX, reverting to RX-only");

    AppState::getInstance().audioPaused = true;
    vTaskDelay(pdMS_TO_TICKS(50));

    // With dacReady=false (set by caller), i2s_configure_adc1 creates RX-only channel
    i2s_configure_adc1(_currentSampleRate);

    AppState::getInstance().audioPaused = false;
    LOG_I("[Audio] Reverted to RX-only mode");
}

void i2s_audio_write_tx(const void* buf, size_t bytes, size_t* bytes_written, uint32_t timeout_ms) {
    if (!_i2s0_tx) { if (bytes_written) *bytes_written = 0; return; }
    i2s_channel_write(_i2s0_tx, buf, bytes, bytes_written, pdMS_TO_TICKS(timeout_ms));
}

#else
// Native test stubs
static int _nativeNumAdcs = 1;
static AdcSyncDiag _nativeSyncDiag = {};
void i2s_audio_init() {}
AudioAnalysis i2s_audio_get_analysis() { return AudioAnalysis{}; }
AudioDiagnostics i2s_audio_get_diagnostics() { return AudioDiagnostics{}; }
AdcSyncDiag i2s_audio_get_sync_diag() { return _nativeSyncDiag; }
bool i2s_audio_get_waveform(uint8_t *out, int adcIndex) { return false; }
bool i2s_audio_get_spectrum(float *bands, float *dominant_freq, int adcIndex) { return false; }
bool i2s_audio_set_sample_rate(uint32_t rate) {
    return audio_validate_sample_rate(rate);
}
int i2s_audio_get_num_adcs() { return _nativeNumAdcs; }
void audio_periodic_dump() {}
void i2s_audio_uninstall_drivers() {}
void i2s_audio_reinstall_drivers() {}
bool i2s_audio_enable_tx(uint32_t) { return true; }
void i2s_audio_disable_tx() {}
void i2s_audio_write_tx(const void*, size_t, size_t* bw, uint32_t) { if (bw) *bw = 0; }
I2sStaticConfig i2s_audio_get_static_config() {
    I2sStaticConfig cfg = {};
    cfg.adc[0].isMaster = true;
    cfg.adc[0].sampleRate = 48000;
    cfg.adc[0].bitsPerSample = 32;
    cfg.adc[0].channelFormat = "Stereo R/L";
    cfg.adc[0].dmaBufCount = 4;
    cfg.adc[0].dmaBufLen = 256;
    cfg.adc[0].apllEnabled = true;
    cfg.adc[0].mclkHz = 48000 * 256;
    cfg.adc[0].commFormat = "Standard I2S";
    cfg.adc[1].isMaster = true;
    cfg.adc[1].sampleRate = 48000;
    cfg.adc[1].bitsPerSample = 32;
    cfg.adc[1].channelFormat = "Stereo R/L";
    cfg.adc[1].dmaBufCount = 4;
    cfg.adc[1].dmaBufLen = 256;
    cfg.adc[1].apllEnabled = true;
    cfg.adc[1].mclkHz = 48000 * 256;
    cfg.adc[1].commFormat = "Standard I2S";
    return cfg;
}
#endif // NATIVE_TEST
