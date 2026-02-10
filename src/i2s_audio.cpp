#include "i2s_audio.h"
#include "app_state.h"
#include "config.h"
#include "debug_serial.h"
#include "signal_generator.h"
#include <arduinoFFT.h>
#include <cmath>
#include <cstring>

#ifndef NATIVE_TEST
#include <driver/i2s.h>
#include <esp_rom_gpio.h>
#include <soc/gpio_struct.h>
#include <soc/i2s_periph.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

// ===== Constants =====
static const int DMA_BUF_COUNT = 4;
static const int DMA_BUF_LEN = 256;
static const float DBFS_FLOOR = -96.0f;

// ===== Clip Rate EMA Constants =====
static const float CLIP_RATE_ALPHA = 0.1f;      // EMA smoothing factor
static const float CLIP_RATE_HW_FAULT = 0.3f;   // >30% clipping = hardware fault
static const float CLIP_RATE_CLIPPING = 0.001f;  // >0.1% clipping = signal too hot

// ===== Shared state (written by I2S task, read by main loop) =====
static volatile AudioAnalysis _analysis = {};
static volatile bool _analysisReady = false;
static AudioDiagnostics _diagnostics = {};

// ===== Pure computation functions (testable without hardware) =====

float audio_compute_rms(const int32_t *samples, int count, int channel, int channels) {
    if (count <= 0) return 0.0f;

    double sum_sq = 0.0;
    int sample_count = 0;
    const float MAX_24BIT = 8388607.0f; // 2^23 - 1

    for (int i = channel; i < count * channels; i += channels) {
        // Parse 24-bit left-justified sample
        int32_t raw = samples[i];
        int32_t parsed = audio_parse_24bit_sample(raw);
        float normalized = (float)parsed / MAX_24BIT;
        sum_sq += (double)normalized * (double)normalized;
        sample_count++;
    }

    if (sample_count == 0) return 0.0f;
    return (float)sqrt(sum_sq / sample_count);
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
// 16 musically-spaced bands covering 20 Hz - 20 kHz
static const float BAND_EDGES[SPECTRUM_BANDS + 1] = {
    20, 40, 80, 160, 315, 630, 1250, 2500,
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
        if (low_bin < 1) low_bin = 1;           // skip DC bin
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

static const int I2S_PORT_MASTER = 0; // I2S_NUM_0 — master RX (ADC1)
static const int I2S_PORT_SLAVE = 1;  // I2S_NUM_1 — slave RX (ADC2)

static uint32_t _currentSampleRate = DEFAULT_AUDIO_SAMPLE_RATE;
static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t _audioTaskHandle = NULL;
static int _numAdcsDetected = 1;
static bool _adc2InitOk = false;

// Per-ADC state arrays
static const float MAX_24BIT_F = 8388607.0f;

// VU meter state per ADC
static float _vuL[NUM_AUDIO_ADCS] = {};
static float _vuR[NUM_AUDIO_ADCS] = {};
static float _vuC[NUM_AUDIO_ADCS] = {};

// Peak hold state per ADC
static float _peakL[NUM_AUDIO_ADCS] = {};
static float _peakR[NUM_AUDIO_ADCS] = {};
static float _peakC[NUM_AUDIO_ADCS] = {};
static unsigned long _holdStartL[NUM_AUDIO_ADCS] = {};
static unsigned long _holdStartR[NUM_AUDIO_ADCS] = {};
static unsigned long _holdStartC[NUM_AUDIO_ADCS] = {};

// DC-blocking IIR filter state per ADC
static int32_t _dcPrevInL[NUM_AUDIO_ADCS] = {};
static int32_t _dcPrevInR[NUM_AUDIO_ADCS] = {};
static float _dcPrevOutL[NUM_AUDIO_ADCS] = {};
static float _dcPrevOutR[NUM_AUDIO_ADCS] = {};

// Waveform accumulation state per ADC
static float _wfAccum[NUM_AUDIO_ADCS][WAVEFORM_BUFFER_SIZE];
static uint8_t _wfOutput[NUM_AUDIO_ADCS][WAVEFORM_BUFFER_SIZE];
static volatile bool _wfReady[NUM_AUDIO_ADCS] = {};
static int _wfFramesSeen[NUM_AUDIO_ADCS] = {};
static int _wfTargetFrames = 2400; // shared, recalculated from audioUpdateRate

// FFT state per ADC
static ArduinoFFT<float> _fft;
static float _fftRing[NUM_AUDIO_ADCS][FFT_SIZE];
static int _fftRingPos[NUM_AUDIO_ADCS] = {};
static float _fftReal[FFT_SIZE]; // Shared working buffer (used sequentially)
static float _fftImag[FFT_SIZE];
static float _spectrumOutput[NUM_AUDIO_ADCS][SPECTRUM_BANDS];
static float _dominantFreqOutput[NUM_AUDIO_ADCS] = {};
static volatile bool _spectrumReady[NUM_AUDIO_ADCS] = {};
static unsigned long _lastFftTime[NUM_AUDIO_ADCS] = {};

static void i2s_configure_master(uint32_t sample_rate) {
    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    cfg.sample_rate = sample_rate;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count = DMA_BUF_COUNT;
    cfg.dma_buf_len = DMA_BUF_LEN;
    cfg.use_apll = true;
    cfg.tx_desc_auto_clear = false;
    cfg.fixed_mclk = sample_rate * 256;

    i2s_driver_install((i2s_port_t)I2S_PORT_MASTER, &cfg, 0, NULL);

    i2s_pin_config_t pins = {};
    pins.bck_io_num = I2S_BCK_PIN;
    pins.ws_io_num = I2S_LRC_PIN;
    pins.data_in_num = I2S_DOUT_PIN;
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.mck_io_num = I2S_MCLK_PIN;

    i2s_set_pin((i2s_port_t)I2S_PORT_MASTER, &pins);
    i2s_zero_dma_buffer((i2s_port_t)I2S_PORT_MASTER);
}

static bool i2s_configure_slave(uint32_t sample_rate) {
    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_SLAVE | I2S_MODE_RX);
    cfg.sample_rate = sample_rate;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count = DMA_BUF_COUNT;
    cfg.dma_buf_len = DMA_BUF_LEN;
    // ESP32-S3 requires internal clock with bclk_div >= 8 even in slave mode.
    // Without APLL, bclk_div calculates to 4 (12.288MHz / 3.072MHz BCK) which
    // silently prevents the RX state machine/DMA from running.
    // Both peripherals share the same APLL frequency (sample_rate * 256).
    cfg.use_apll = true;
    cfg.fixed_mclk = sample_rate * 256;

    esp_err_t err = i2s_driver_install((i2s_port_t)I2S_PORT_SLAVE, &cfg, 0, NULL);
    if (err != ESP_OK) {
        LOG_E("[Audio] ADC2 I2S slave driver install failed: %d", err);
        return false;
    }

    // Configure BCK/WS/DOUT2 pins. Because slave is initialized BEFORE master,
    // i2s_set_pin's internal gpio_set_direction(INPUT) is safe — no master
    // output exists yet to disconnect. Master's subsequent i2s_set_pin restores
    // GPIO direction to OUTPUT and routes gpio_matrix_out to master clock signals.
    // The esp_rom_gpio_connect_in_signal calls in i2s_audio_init then re-route
    // slave input after master's i2s_set_pin overwrites the input matrix.
    i2s_pin_config_t pins = {};
    pins.bck_io_num = I2S_BCK_PIN;
    pins.ws_io_num = I2S_LRC_PIN;
    pins.data_in_num = I2S_DOUT2_PIN;
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.mck_io_num = I2S_PIN_NO_CHANGE;

    i2s_set_pin((i2s_port_t)I2S_PORT_SLAVE, &pins);
    i2s_zero_dma_buffer((i2s_port_t)I2S_PORT_SLAVE);
    return true;
}

// Process a single ADC's buffer: diagnostics, DC filter, RMS, VU, peak, waveform, FFT
static void process_adc_buffer(int a, int32_t *buffer, int stereo_frames,
                                unsigned long now, float dt_ms, bool sigGenSw) {
    int total_samples = stereo_frames * 2;
    AdcDiagnostics &diag = _diagnostics.adc[a];

    // --- Diagnostics: scan raw buffer BEFORE siggen overwrites it ---
    diag.totalBuffersRead++;
    diag.lastReadMs = now;
    {
        bool allZero = true;
        uint32_t clipCount = 0;
        const int32_t CLIP_THRESHOLD = 8300000;
        for (int i = 0; i < total_samples; i++) {
            int32_t parsed = audio_parse_24bit_sample(buffer[i]);
            if (parsed != 0) allZero = false;
            if (parsed > CLIP_THRESHOLD || parsed < -CLIP_THRESHOLD) clipCount++;
        }
        if (allZero) {
            diag.allZeroBuffers++;
            diag.consecutiveZeros++;
        } else {
            diag.consecutiveZeros = 0;
            diag.lastNonZeroMs = now;
        }
        diag.clippedSamples += clipCount;
        // EMA clip rate: naturally decays when clipping stops
        float bufferClipRate = (total_samples > 0) ? (float)clipCount / (float)total_samples : 0.0f;
        diag.clipRate = diag.clipRate * (1.0f - CLIP_RATE_ALPHA) + bufferClipRate * CLIP_RATE_ALPHA;
    }

    // DC offset tracking
    {
        double sum = 0.0;
        for (int i = 0; i < total_samples; i++) {
            sum += (double)audio_parse_24bit_sample(buffer[i]);
        }
        float mean = (float)(sum / total_samples) / 8388607.0f;
        diag.dcOffset += (mean - diag.dcOffset) * 0.01f;
    }

    // DC-blocking IIR filter
    static const float DC_BLOCK_ALPHA = 0.9987f;
    for (int f = 0; f < stereo_frames; f++) {
        int32_t inL = buffer[f * 2];
        float outL = (float)(inL - _dcPrevInL[a]) + DC_BLOCK_ALPHA * _dcPrevOutL[a];
        _dcPrevInL[a] = inL;
        _dcPrevOutL[a] = outL;
        buffer[f * 2] = (int32_t)outL;

        int32_t inR = buffer[f * 2 + 1];
        float outR = (float)(inR - _dcPrevInR[a]) + DC_BLOCK_ALPHA * _dcPrevOutR[a];
        _dcPrevInR[a] = inR;
        _dcPrevOutR[a] = outR;
        buffer[f * 2 + 1] = (int32_t)outR;
    }

    // Compute RMS
    float rmsL = audio_compute_rms(buffer, stereo_frames, 0, 2);
    float rmsR = audio_compute_rms(buffer, stereo_frames, 1, 2);
    float rmsC = sqrtf((rmsL * rmsL + rmsR * rmsR) / 2.0f);
    float dBFS = audio_rms_to_dbfs(rmsC);

    // VU metering
    if (AppState::getInstance().vuMeterEnabled) {
        _vuL[a] = audio_vu_update(_vuL[a], rmsL, dt_ms);
        _vuR[a] = audio_vu_update(_vuR[a], rmsR, dt_ms);
        _vuC[a] = audio_vu_update(_vuC[a], rmsC, dt_ms);
        _peakL[a] = audio_peak_hold_update(_peakL[a], rmsL, &_holdStartL[a], now, dt_ms);
        _peakR[a] = audio_peak_hold_update(_peakR[a], rmsR, &_holdStartR[a], now, dt_ms);
        _peakC[a] = audio_peak_hold_update(_peakC[a], rmsC, &_holdStartC[a], now, dt_ms);
    } else {
        _vuL[a] = _vuR[a] = _vuC[a] = 0.0f;
        _peakL[a] = _peakR[a] = _peakC[a] = 0.0f;
    }

    // Waveform accumulation
    if (AppState::getInstance().waveformEnabled) {
        for (int f = 0; f < stereo_frames; f++) {
            int bin = (int)((long)(_wfFramesSeen[a] + f) * WAVEFORM_BUFFER_SIZE / _wfTargetFrames);
            if (bin >= WAVEFORM_BUFFER_SIZE) break;
            float sL = (float)audio_parse_24bit_sample(buffer[f * 2]) / MAX_24BIT_F;
            float sR = (float)audio_parse_24bit_sample(buffer[f * 2 + 1]) / MAX_24BIT_F;
            float combined = (sL + sR) / 2.0f;
            if (fabsf(combined) > fabsf(_wfAccum[a][bin])) {
                _wfAccum[a][bin] = combined;
            }
        }
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

    // FFT ring buffer + compute
    if (AppState::getInstance().spectrumEnabled) {
        for (int f = 0; f < stereo_frames; f++) {
            float sL = (float)audio_parse_24bit_sample(buffer[f * 2]) / MAX_24BIT_F;
            float sR = (float)audio_parse_24bit_sample(buffer[f * 2 + 1]) / MAX_24BIT_F;
            _fftRing[a][_fftRingPos[a]] = (sL + sR) / 2.0f;
            _fftRingPos[a] = (_fftRingPos[a] + 1) % FFT_SIZE;
        }
        if (now - _lastFftTime[a] >= AppState::getInstance().audioUpdateRate) {
            _lastFftTime[a] = now;
            for (int i = 0; i < FFT_SIZE; i++) {
                _fftReal[i] = _fftRing[a][(_fftRingPos[a] + i) % FFT_SIZE];
                _fftImag[i] = 0.0f;
            }
            _fft.windowing(_fftReal, FFT_SIZE, FFTWindow::Hamming, FFTDirection::Forward);
            _fft.compute(_fftReal, _fftImag, FFT_SIZE, FFTDirection::Forward);
            _fft.complexToMagnitude(_fftReal, _fftImag, FFT_SIZE);
            _dominantFreqOutput[a] = _fft.majorPeak(_fftReal, FFT_SIZE, (float)_currentSampleRate);
            audio_aggregate_fft_bands(_fftReal, FFT_SIZE, (float)_currentSampleRate,
                                      _spectrumOutput[a], SPECTRUM_BANDS);
            _spectrumReady[a] = true;
        }
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
}

static void audio_capture_task(void *param) {
    const int BUFFER_SAMPLES = DMA_BUF_LEN * 2; // stereo
    int32_t buf1[BUFFER_SAMPLES];
    int32_t buf2[BUFFER_SAMPLES];

    unsigned long prevTime = millis();
    unsigned long lastDumpTime = 0;
    bool adc2FirstReadLogged = false;

    // Metrics counters for throughput and latency
    uint32_t bufCount[NUM_AUDIO_ADCS] = {};
    unsigned long readLatencyAccumUs[NUM_AUDIO_ADCS] = {};
    uint32_t readLatencyCount[NUM_AUDIO_ADCS] = {};
    unsigned long lastMetricsTime = millis();

    while (true) {
        // Read ADC1 (master) — blocks until DMA ready
        size_t bytes_read1 = 0;
        unsigned long t0 = micros();
        esp_err_t err1 = i2s_read((i2s_port_t)I2S_PORT_MASTER, buf1,
                                   sizeof(buf1), &bytes_read1, portMAX_DELAY);
        unsigned long t1 = micros();

        if (err1 != ESP_OK || bytes_read1 == 0) {
            if (err1 != ESP_OK) _diagnostics.adc[0].i2sReadErrors++;
            if (bytes_read1 == 0) _diagnostics.adc[0].zeroByteReads++;
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        readLatencyAccumUs[0] += (t1 - t0);
        readLatencyCount[0]++;
        bufCount[0]++;

        // Read ADC2 (slave) — near-instant if synced DMA is ready
        size_t bytes_read2 = 0;
        bool adc2Ok = false;
        if (_adc2InitOk) {
            unsigned long t2 = micros();
            esp_err_t err2 = i2s_read((i2s_port_t)I2S_PORT_SLAVE, buf2,
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
        }

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
            if (targetAdc == 0 || targetAdc == 2)
                siggen_fill_buffer(buf1, stereo_frames1, _currentSampleRate);
            if ((targetAdc == 1 || targetAdc == 2) && adc2Ok)
                siggen_fill_buffer(buf2, stereo_frames2, _currentSampleRate);
        }

        // Process ADC1
        process_adc_buffer(0, buf1, stereo_frames1, now, dt_ms, sigGenSw);

        // Process ADC2 (if available)
        if (adc2Ok) {
            process_adc_buffer(1, buf2, stereo_frames2, now, dt_ms, sigGenSw);
        } else if (_adc2InitOk) {
            // Slave read failed — still update diagnostics so health status reflects reality
            AdcDiagnostics &diag = _diagnostics.adc[1];
            diag.consecutiveZeros++;
            diag.allZeroBuffers++;
            diag.status = audio_derive_health_status(diag);
        }

        // Periodic serial dump
        bool doDump = (now - lastDumpTime >= 5000);
        if (doDump) {
            lastDumpTime = now;
            LOG_I("[Audio] ADC1=%.1fdB flr=%.1f adcs=%d",
                  _analysis.adc[0].dBFS, _diagnostics.adc[0].noiseFloorDbfs,
                  _numAdcsDetected);
            if (_adc2InitOk) {
                // Enhanced ADC2 log — distinguishes failure modes:
                // zb high + az=0 + tot=0 → DMA timeout, slave not clocking (GPIO matrix / wiring)
                // zb low  + az high      → Slave clocking OK, no audio (no PCM1808 #2)
                // errs > 0               → I2S driver error (bus fault, DMA overflow)
                LOG_I("[Audio] ADC2=%.1fdB flr=%.1f st=%d errs=%lu zb=%lu az=%lu cz=%lu tot=%lu",
                      _analysis.adc[1].dBFS, _diagnostics.adc[1].noiseFloorDbfs,
                      _diagnostics.adc[1].status,
                      _diagnostics.adc[1].i2sReadErrors,
                      _diagnostics.adc[1].zeroByteReads,
                      _diagnostics.adc[1].allZeroBuffers,
                      _diagnostics.adc[1].consecutiveZeros,
                      _diagnostics.adc[1].totalBuffersRead);
            }
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

        // Combined analysis: overall dBFS = max across ADCs, signal detected = any
        float overallDbfs = _analysis.adc[0].dBFS;
        if (_numAdcsDetected >= 2 && _analysis.adc[1].dBFS > overallDbfs) {
            overallDbfs = _analysis.adc[1].dBFS;
        }
        float threshold = AppState::getInstance().audioThreshold_dBFS;

        portENTER_CRITICAL_ISR(&spinlock);
        _analysis.dBFS = overallDbfs;
        _analysis.signalDetected = (overallDbfs >= threshold);
        _analysis.timestamp = now;
        _analysisReady = true;
        portEXIT_CRITICAL_ISR(&spinlock);
    }
}

void i2s_audio_init() {
    _currentSampleRate = AppState::getInstance().audioSampleRate;
    if (!audio_validate_sample_rate(_currentSampleRate)) {
        _currentSampleRate = DEFAULT_AUDIO_SAMPLE_RATE;
    }

    // Reset diagnostics
    _diagnostics = AudioDiagnostics{};

    _wfTargetFrames = _currentSampleRate * AppState::getInstance().audioUpdateRate / 1000;
    for (int a = 0; a < NUM_AUDIO_ADCS; a++) {
        memset(_wfAccum[a], 0, sizeof(_wfAccum[a]));
        _wfFramesSeen[a] = 0;
        _wfReady[a] = false;
        memset(_fftRing[a], 0, sizeof(_fftRing[a]));
        _fftRingPos[a] = 0;
        _spectrumReady[a] = false;
        _lastFftTime[a] = 0;
    }

    // Pull-down on DOUT2 so an unconnected pin reads clean zeros (→ NO_DATA)
    // instead of floating noise that looks like valid audio (→ false OK status)
    pinMode(I2S_DOUT2_PIN, INPUT_PULLDOWN);

    // Install slave I2S driver (ADC2) first. Only DOUT2 is configured via
    // i2s_set_pin — BCK/LRC are routed manually after master setup.
    _adc2InitOk = i2s_configure_slave(_currentSampleRate);

    // Configure master I2S (ADC1) with full pin config — BCK/LRC as outputs.
    i2s_configure_master(_currentSampleRate);

    // Now manually route BCK/LRC to slave I2S input signals.
    // esp_rom_gpio_connect_in_signal only writes the input MUX register —
    // does NOT touch gpio_matrix_out (master clock) or GPIO direction.
    if (_adc2InitOk) {
        esp_rom_gpio_connect_in_signal(I2S_BCK_PIN,
            i2s_periph_signal[I2S_PORT_SLAVE].s_rx_bck_sig, false);
        esp_rom_gpio_connect_in_signal(I2S_LRC_PIN,
            i2s_periph_signal[I2S_PORT_SLAVE].s_rx_ws_sig, false);
        // Restart slave so DMA picks up the newly routed clocks
        i2s_stop((i2s_port_t)I2S_PORT_SLAVE);
        i2s_start((i2s_port_t)I2S_PORT_SLAVE);
        LOG_I("[Audio] ADC2 slave routed: BCK sig=%d, WS sig=%d",
              i2s_periph_signal[I2S_PORT_SLAVE].s_rx_bck_sig,
              i2s_periph_signal[I2S_PORT_SLAVE].s_rx_ws_sig);

        // Verify GPIO matrix input routing took effect via peripheral struct
        // Expected: BCK_IN=GPIO16, WS_IN=GPIO18, DIN=GPIO9
        // If any shows 0x3F (63) = disconnected / not routed
        uint32_t bck_sel = GPIO.func_in_sel_cfg[i2s_periph_signal[I2S_PORT_SLAVE].s_rx_bck_sig].func_sel;
        uint32_t ws_sel  = GPIO.func_in_sel_cfg[i2s_periph_signal[I2S_PORT_SLAVE].s_rx_ws_sig].func_sel;
        uint32_t din_sel = GPIO.func_in_sel_cfg[i2s_periph_signal[I2S_PORT_SLAVE].data_in_sig].func_sel;
        LOG_I("[Audio] ADC2 GPIO verify: BCK_IN=GPIO%lu (expect %d), WS_IN=GPIO%lu (expect %d), DIN=GPIO%lu (expect %d)",
              bck_sel, I2S_BCK_PIN, ws_sel, I2S_LRC_PIN, din_sel, I2S_DOUT2_PIN);
    }
    _numAdcsDetected = 1; // Will be updated once data flows

    xTaskCreatePinnedToCore(
        audio_capture_task,
        "audio_cap",
        TASK_STACK_SIZE_AUDIO,
        NULL,
        TASK_PRIORITY_AUDIO,
        &_audioTaskHandle,
        0  // Core 0
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

bool i2s_audio_get_waveform(uint8_t *out, int adcIndex) {
    if (adcIndex < 0 || adcIndex >= NUM_AUDIO_ADCS) return false;
    if (!_wfReady[adcIndex]) return false;
    memcpy(out, (const void *)_wfOutput[adcIndex], WAVEFORM_BUFFER_SIZE);
    _wfReady[adcIndex] = false;
    return true;
}

bool i2s_audio_get_spectrum(float *bands, float *dominant_freq, int adcIndex) {
    if (adcIndex < 0 || adcIndex >= NUM_AUDIO_ADCS) return false;
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
    // ADC2 — Slave
    cfg.adc[1].isMaster = false;
    cfg.adc[1].sampleRate = _currentSampleRate;
    cfg.adc[1].bitsPerSample = 32;
    cfg.adc[1].channelFormat = "Stereo R/L";
    cfg.adc[1].dmaBufCount = DMA_BUF_COUNT;
    cfg.adc[1].dmaBufLen = DMA_BUF_LEN;
    cfg.adc[1].apllEnabled = false;
    cfg.adc[1].mclkHz = 0;
    cfg.adc[1].commFormat = "Standard I2S";
    return cfg;
}

bool i2s_audio_set_sample_rate(uint32_t rate) {
    if (!audio_validate_sample_rate(rate)) return false;
    if (rate == _currentSampleRate) return true;

    LOG_I("[Audio] Changing sample rate: %lu -> %lu Hz", _currentSampleRate, rate);

    i2s_driver_uninstall((i2s_port_t)I2S_PORT_MASTER);
    if (_adc2InitOk) i2s_driver_uninstall((i2s_port_t)I2S_PORT_SLAVE);

    _currentSampleRate = rate;
    _wfTargetFrames = rate * AppState::getInstance().audioUpdateRate / 1000;
    for (int a = 0; a < NUM_AUDIO_ADCS; a++) {
        _wfFramesSeen[a] = 0;
        memset(_wfAccum[a], 0, sizeof(_wfAccum[a]));
    }

    if (_adc2InitOk) _adc2InitOk = i2s_configure_slave(rate);
    i2s_configure_master(rate);
    if (_adc2InitOk) {
        esp_rom_gpio_connect_in_signal(I2S_BCK_PIN,
            i2s_periph_signal[I2S_PORT_SLAVE].s_rx_bck_sig, false);
        esp_rom_gpio_connect_in_signal(I2S_LRC_PIN,
            i2s_periph_signal[I2S_PORT_SLAVE].s_rx_ws_sig, false);
        i2s_stop((i2s_port_t)I2S_PORT_SLAVE);
        i2s_start((i2s_port_t)I2S_PORT_SLAVE);
    }

    LOG_I("[Audio] Sample rate changed to %lu Hz", rate);
    return true;
}

#else
// Native test stubs
static int _nativeNumAdcs = 1;
void i2s_audio_init() {}
AudioAnalysis i2s_audio_get_analysis() { return AudioAnalysis{}; }
AudioDiagnostics i2s_audio_get_diagnostics() { return AudioDiagnostics{}; }
bool i2s_audio_get_waveform(uint8_t *out, int adcIndex) { return false; }
bool i2s_audio_get_spectrum(float *bands, float *dominant_freq, int adcIndex) { return false; }
bool i2s_audio_set_sample_rate(uint32_t rate) {
    return audio_validate_sample_rate(rate);
}
int i2s_audio_get_num_adcs() { return _nativeNumAdcs; }
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
    cfg.adc[1].isMaster = false;
    cfg.adc[1].sampleRate = 48000;
    cfg.adc[1].bitsPerSample = 32;
    cfg.adc[1].channelFormat = "Stereo R/L";
    cfg.adc[1].dmaBufCount = 4;
    cfg.adc[1].dmaBufLen = 256;
    cfg.adc[1].apllEnabled = false;
    cfg.adc[1].mclkHz = 0;
    cfg.adc[1].commFormat = "Standard I2S";
    return cfg;
}
#endif // NATIVE_TEST
