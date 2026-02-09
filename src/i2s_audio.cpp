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
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

// ===== Constants =====
static const int I2S_PORT = 1; // I2S_NUM_1
static const int DMA_BUF_COUNT = 4;
static const int DMA_BUF_LEN = 256;
static const float DBFS_FLOOR = -96.0f;

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
AudioHealthStatus audio_derive_health_status(const AudioDiagnostics &diag) {
    // I2S bus errors take highest priority
    if (diag.i2sReadErrors > 10) return AUDIO_I2S_ERROR;
    // ADC not sending any data
    if (diag.consecutiveZeros > 100) return AUDIO_NO_DATA;
    // Clipping (only flag when siggen is off — siggen can intentionally clip)
    if (diag.clippedSamples > 0 && !diag.sigGenActive) return AUDIO_CLIPPING;
    // Thermal noise only (no meaningful audio)
    if (diag.noiseFloorDbfs < -75.0f && diag.noiseFloorDbfs > -96.0f) return AUDIO_NOISE_ONLY;
    return AUDIO_OK;
}

// ===== Hardware-dependent code (ESP32 only) =====
#ifndef NATIVE_TEST

static uint32_t _currentSampleRate = DEFAULT_AUDIO_SAMPLE_RATE;
static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;

// Waveform accumulation state (written by capture task only)
static const float MAX_24BIT_F = 8388607.0f;
static float _wfAccum[WAVEFORM_BUFFER_SIZE];
static uint8_t _wfOutput[WAVEFORM_BUFFER_SIZE];
static volatile bool _wfReady = false;
static int _wfFramesSeen = 0;
static int _wfTargetFrames = 2400; // recalculated from audioUpdateRate

// FFT state (written by capture task only)
static ArduinoFFT<float> _fft;
static float _fftRing[FFT_SIZE];           // Ring buffer of mono samples
static int _fftRingPos = 0;
static float _fftReal[FFT_SIZE];           // FFT working buffers
static float _fftImag[FFT_SIZE];
static float _spectrumOutput[SPECTRUM_BANDS];
static float _dominantFreqOutput = 0.0f;
static volatile bool _spectrumReady = false;
static unsigned long _lastFftTime = 0;

static void i2s_configure(uint32_t sample_rate) {
    i2s_config_t i2s_config = {};
    i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    i2s_config.sample_rate = sample_rate;
    i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    i2s_config.dma_buf_count = DMA_BUF_COUNT;
    i2s_config.dma_buf_len = DMA_BUF_LEN;
    i2s_config.use_apll = true;
    i2s_config.tx_desc_auto_clear = false;
    i2s_config.fixed_mclk = sample_rate * 256;

    i2s_driver_install((i2s_port_t)I2S_PORT, &i2s_config, 0, NULL);

    i2s_pin_config_t pin_config = {};
    pin_config.bck_io_num = I2S_BCK_PIN;
    pin_config.ws_io_num = I2S_LRC_PIN;
    pin_config.data_in_num = I2S_DOUT_PIN;
    pin_config.data_out_num = I2S_PIN_NO_CHANGE;
    pin_config.mck_io_num = I2S_MCLK_PIN;

    i2s_set_pin((i2s_port_t)I2S_PORT, &pin_config);
    i2s_zero_dma_buffer((i2s_port_t)I2S_PORT);
}

static void audio_capture_task(void *param) {
    // Buffer for one DMA read: 256 stereo samples × 4 bytes = 2048 bytes
    const int BUFFER_SAMPLES = DMA_BUF_LEN * 2; // stereo
    int32_t buffer[BUFFER_SAMPLES];

    // VU meter state (smoothed with attack/decay ballistics)
    float vuL = 0.0f, vuR = 0.0f, vuC = 0.0f;

    // Peak hold state (instant attack, 2s hold, then decay)
    float peakL = 0.0f, peakR = 0.0f, peakC = 0.0f;
    unsigned long holdStartL = 0, holdStartR = 0, holdStartC = 0;

    unsigned long prevTime = millis();
    unsigned long lastDumpTime = 0; // Periodic raw sample dump

    // DC-blocking IIR filter state (per channel)
    // y[n] = x[n] - x[n-1] + alpha * y[n-1], alpha ~0.9987 = ~10 Hz cutoff at 48kHz
    static const float DC_BLOCK_ALPHA = 0.9987f;
    int32_t dcPrevInL = 0, dcPrevInR = 0;
    float dcPrevOutL = 0.0f, dcPrevOutR = 0.0f;

    while (true) {
        size_t bytes_read = 0;
        esp_err_t err = i2s_read((i2s_port_t)I2S_PORT, buffer,
                                  sizeof(buffer), &bytes_read, portMAX_DELAY);

        if (err != ESP_OK || bytes_read == 0) {
            if (err != ESP_OK) _diagnostics.i2sReadErrors++;
            if (bytes_read == 0) _diagnostics.zeroByteReads++;
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        unsigned long now = millis();
        float dt_ms = (float)(now - prevTime);
        prevTime = now;

        int total_samples = bytes_read / sizeof(int32_t);
        int stereo_frames = total_samples / 2;

        // --- Diagnostics: scan raw PCM1808 buffer BEFORE siggen overwrites it ---
        _diagnostics.totalBuffersRead++;
        _diagnostics.lastReadMs = now;
        {
            bool allZero = true;
            uint32_t clipCount = 0;
            const int32_t CLIP_THRESHOLD = 8300000; // ~98.9% of 2^23-1
            for (int i = 0; i < total_samples; i++) {
                int32_t parsed = audio_parse_24bit_sample(buffer[i]);
                if (parsed != 0) allZero = false;
                if (parsed > CLIP_THRESHOLD || parsed < -CLIP_THRESHOLD) clipCount++;
            }
            if (allZero) {
                _diagnostics.allZeroBuffers++;
                _diagnostics.consecutiveZeros++;
            } else {
                _diagnostics.consecutiveZeros = 0;
                _diagnostics.lastNonZeroMs = now;
            }
            _diagnostics.clippedSamples += clipCount;
        }

        // DC offset: compute mean of parsed samples (tracks slowly)
        {
            double sum = 0.0;
            for (int i = 0; i < total_samples; i++) {
                sum += (double)audio_parse_24bit_sample(buffer[i]);
            }
            float mean = (float)(sum / total_samples) / 8388607.0f;
            // Slow exponential average
            _diagnostics.dcOffset += (mean - _diagnostics.dcOffset) * 0.01f;
        }

        // Flag for periodic serial dump (every 5 seconds)
        bool doDump = (now - lastDumpTime >= 5000);
        if (doDump) lastDumpTime = now;

        // Pre-filter RMS for serial dump comparison
        float preL = 0, preR = 0, preC = 0;
        if (doDump) {
            preL = audio_compute_rms(buffer, stereo_frames, 0, 2);
            preR = audio_compute_rms(buffer, stereo_frames, 1, 2);
            preC = sqrtf((preL * preL + preR * preR) / 2.0f);
        }

        bool sigGenSw = siggen_is_active() && siggen_is_software_mode();
        _diagnostics.sigGenActive = sigGenSw;

        // Inject test signal if signal generator is active in software mode
        if (siggen_is_active() && siggen_is_software_mode()) {
            siggen_fill_buffer(buffer, stereo_frames, _currentSampleRate);
        }

        // DC-blocking IIR filter: y[n] = x[n] - x[n-1] + alpha * y[n-1]
        // Removes DC offset and sub-10Hz content that masks real audio
        for (int f = 0; f < stereo_frames; f++) {
            int32_t inL = buffer[f * 2];
            float outL = (float)(inL - dcPrevInL) + DC_BLOCK_ALPHA * dcPrevOutL;
            dcPrevInL = inL;
            dcPrevOutL = outL;
            buffer[f * 2] = (int32_t)outL;

            int32_t inR = buffer[f * 2 + 1];
            float outR = (float)(inR - dcPrevInR) + DC_BLOCK_ALPHA * dcPrevOutR;
            dcPrevInR = inR;
            dcPrevOutR = outR;
            buffer[f * 2 + 1] = (int32_t)outR;
        }

        // Compute RMS for left (channel 0) and right (channel 1)
        float rmsL = audio_compute_rms(buffer, stereo_frames, 0, 2);
        float rmsR = audio_compute_rms(buffer, stereo_frames, 1, 2);
        float rmsC = sqrtf((rmsL * rmsL + rmsR * rmsR) / 2.0f);

        // Periodic serial dump: single LOG call to avoid watchdog timeout
        if (doDump) {
            int32_t pL0 = audio_parse_24bit_sample(dcPrevInL);
            int32_t pR0 = audio_parse_24bit_sample(dcPrevInR);
            LOG_I("[Audio] PRE=%.1f/%.1f/%.1f POST=%.1f/%.1f/%.1f DC=%.4f raw=%.3f/%.3f flr=%.1f clip=%lu",
                  audio_rms_to_dbfs(preL), audio_rms_to_dbfs(preR), audio_rms_to_dbfs(preC),
                  audio_rms_to_dbfs(rmsL), audio_rms_to_dbfs(rmsR), audio_rms_to_dbfs(rmsC),
                  _diagnostics.dcOffset, pL0 / 8388607.0f, pR0 / 8388607.0f,
                  _diagnostics.noiseFloorDbfs, (unsigned long)_diagnostics.clippedSamples);
        }

        // VU metering with industry-standard ballistics
        if (AppState::getInstance().vuMeterEnabled) {
            vuL = audio_vu_update(vuL, rmsL, dt_ms);
            vuR = audio_vu_update(vuR, rmsR, dt_ms);
            vuC = audio_vu_update(vuC, rmsC, dt_ms);

            // Peak hold (instant attack, 2s hold, then 300ms decay)
            peakL = audio_peak_hold_update(peakL, rmsL, &holdStartL, now, dt_ms);
            peakR = audio_peak_hold_update(peakR, rmsR, &holdStartR, now, dt_ms);
            peakC = audio_peak_hold_update(peakC, rmsC, &holdStartC, now, dt_ms);
        } else {
            vuL = vuR = vuC = 0.0f;
            peakL = peakR = peakC = 0.0f;
        }

        // Waveform accumulation: map DMA frames to 256-point waveform bins
        if (AppState::getInstance().waveformEnabled) {
            for (int f = 0; f < stereo_frames; f++) {
                int bin = (int)((long)(_wfFramesSeen + f) * WAVEFORM_BUFFER_SIZE / _wfTargetFrames);
                if (bin >= WAVEFORM_BUFFER_SIZE) break;

                float sL = (float)audio_parse_24bit_sample(buffer[f * 2]) / MAX_24BIT_F;
                float sR = (float)audio_parse_24bit_sample(buffer[f * 2 + 1]) / MAX_24BIT_F;
                float combined = (sL + sR) / 2.0f;

                if (fabsf(combined) > fabsf(_wfAccum[bin])) {
                    _wfAccum[bin] = combined;
                }
            }
            _wfFramesSeen += stereo_frames;

            if (_wfFramesSeen >= _wfTargetFrames) {
                for (int i = 0; i < WAVEFORM_BUFFER_SIZE; i++) {
                    _wfOutput[i] = audio_quantize_sample(_wfAccum[i]);
                    _wfAccum[i] = 0.0f;
                }
                _wfFramesSeen = 0;
                _wfReady = true;
                // Recalculate window from dynamic rate setting
                _wfTargetFrames = _currentSampleRate * AppState::getInstance().audioUpdateRate / 1000;
            }
        }

        // FFT ring buffer: accumulate mono samples
        if (AppState::getInstance().spectrumEnabled) {
            for (int f = 0; f < stereo_frames; f++) {
                float sL = (float)audio_parse_24bit_sample(buffer[f * 2]) / MAX_24BIT_F;
                float sR = (float)audio_parse_24bit_sample(buffer[f * 2 + 1]) / MAX_24BIT_F;
                _fftRing[_fftRingPos] = (sL + sR) / 2.0f;
                _fftRingPos = (_fftRingPos + 1) % FFT_SIZE;
            }

            // Compute FFT at configurable rate
            if (now - _lastFftTime >= AppState::getInstance().audioUpdateRate) {
                _lastFftTime = now;

                // Copy ring buffer into FFT working buffer (unwrap)
                for (int i = 0; i < FFT_SIZE; i++) {
                    _fftReal[i] = _fftRing[(_fftRingPos + i) % FFT_SIZE];
                    _fftImag[i] = 0.0f;
                }

                _fft.windowing(_fftReal, FFT_SIZE, FFTWindow::Hamming, FFTDirection::Forward);
                _fft.compute(_fftReal, _fftImag, FFT_SIZE, FFTDirection::Forward);
                _fft.complexToMagnitude(_fftReal, _fftImag, FFT_SIZE);

                _dominantFreqOutput = _fft.majorPeak(_fftReal, FFT_SIZE, (float)_currentSampleRate);
                audio_aggregate_fft_bands(_fftReal, FFT_SIZE, (float)_currentSampleRate,
                                          _spectrumOutput, SPECTRUM_BANDS);
                _spectrumReady = true;
            }
        }

        float dBFS = audio_rms_to_dbfs(rmsC);
        float threshold = AppState::getInstance().audioThreshold_dBFS;

        // Track noise floor and peak when signal generator is off
        if (!sigGenSw) {
            // Exponential moving average for noise floor (slow adaptation)
            if (dBFS > _diagnostics.noiseFloorDbfs) {
                _diagnostics.noiseFloorDbfs += (dBFS - _diagnostics.noiseFloorDbfs) * 0.01f;
            } else {
                _diagnostics.noiseFloorDbfs += (dBFS - _diagnostics.noiseFloorDbfs) * 0.001f;
            }
            if (dBFS > _diagnostics.peakDbfs) {
                _diagnostics.peakDbfs = dBFS;
            }
        }
        _diagnostics.status = audio_derive_health_status(_diagnostics);

        // Update shared analysis struct atomically
        portENTER_CRITICAL_ISR(&spinlock);
        _analysis.rmsLeft = rmsL;
        _analysis.rmsRight = rmsR;
        _analysis.rmsCombined = rmsC;
        _analysis.vuLeft = vuL;
        _analysis.vuRight = vuR;
        _analysis.vuCombined = vuC;
        _analysis.peakLeft = peakL;
        _analysis.peakRight = peakR;
        _analysis.peakCombined = peakC;
        _analysis.dBFS = dBFS;
        _analysis.signalDetected = (dBFS >= threshold);
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
    memset(_wfAccum, 0, sizeof(_wfAccum));
    _wfFramesSeen = 0;
    _wfReady = false;

    memset(_fftRing, 0, sizeof(_fftRing));
    _fftRingPos = 0;
    _spectrumReady = false;
    _lastFftTime = 0;

    i2s_configure(_currentSampleRate);

    xTaskCreatePinnedToCore(
        audio_capture_task,
        "audio_cap",
        TASK_STACK_SIZE_AUDIO,
        NULL,
        TASK_PRIORITY_AUDIO,
        NULL,
        0  // Core 0
    );

    LOG_I("[Audio] I2S initialized: %lu Hz, BCK=%d, DOUT=%d, LRC=%d, MCLK=%d",
          _currentSampleRate, I2S_BCK_PIN, I2S_DOUT_PIN, I2S_LRC_PIN, I2S_MCLK_PIN);
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

bool i2s_audio_get_waveform(uint8_t *out) {
    if (!_wfReady) return false;
    memcpy(out, (const void *)_wfOutput, WAVEFORM_BUFFER_SIZE);
    _wfReady = false;
    return true;
}

bool i2s_audio_get_spectrum(float *bands, float *dominant_freq) {
    if (!_spectrumReady) return false;
    memcpy(bands, (const void *)_spectrumOutput, SPECTRUM_BANDS * sizeof(float));
    *dominant_freq = _dominantFreqOutput;
    _spectrumReady = false;
    return true;
}

bool i2s_audio_set_sample_rate(uint32_t rate) {
    if (!audio_validate_sample_rate(rate)) return false;
    if (rate == _currentSampleRate) return true;

    LOG_I("[Audio] Changing sample rate: %lu -> %lu Hz", _currentSampleRate, rate);

    i2s_driver_uninstall((i2s_port_t)I2S_PORT);
    _currentSampleRate = rate;
    _wfTargetFrames = rate * AppState::getInstance().audioUpdateRate / 1000;
    _wfFramesSeen = 0;
    memset(_wfAccum, 0, sizeof(_wfAccum));
    i2s_configure(rate);

    LOG_I("[Audio] Sample rate changed to %lu Hz", rate);
    return true;
}

#else
// Native test stubs
void i2s_audio_init() {}
AudioAnalysis i2s_audio_get_analysis() { return AudioAnalysis{}; }
AudioDiagnostics i2s_audio_get_diagnostics() { return AudioDiagnostics{}; }
bool i2s_audio_get_waveform(uint8_t *out) { return false; }
bool i2s_audio_get_spectrum(float *bands, float *dominant_freq) { return false; }
bool i2s_audio_set_sample_rate(uint32_t rate) {
    return audio_validate_sample_rate(rate);
}
#endif // NATIVE_TEST
