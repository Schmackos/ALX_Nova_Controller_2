#ifdef DSP_ENABLED

#include "thd_measurement.h"
#include <math.h>
#include <string.h>

#ifndef NATIVE_TEST
#include "debug_serial.h"
#else
#define LOG_I(...)
#define LOG_W(...)
#endif

static bool _measuring = false;
static float _testFreqHz = 0.0f;
static uint16_t _targetFrames = 8;
static uint16_t _processedFrames = 0;

// Accumulation buffers for averaging
static float _fundamentalPowerSum = 0.0f;
static float _harmonicPowerSum[THD_MAX_HARMONICS];
static float _noisePowerSum = 0.0f;
static float _totalPowerSum = 0.0f;

static ThdResult _result;

void thd_start_measurement(float testFreqHz, uint16_t numAverages) {
    if (testFreqHz <= 0.0f || numAverages == 0) {
        memset(&_result, 0, sizeof(_result));
        _result.valid = false;
        return;
    }

    _testFreqHz = testFreqHz;
    _targetFrames = numAverages;
    _processedFrames = 0;
    _fundamentalPowerSum = 0.0f;
    memset(_harmonicPowerSum, 0, sizeof(_harmonicPowerSum));
    _noisePowerSum = 0.0f;
    _totalPowerSum = 0.0f;
    memset(&_result, 0, sizeof(_result));
    _result.framesTarget = numAverages;
    _measuring = true;

    LOG_I("[THD] Measurement started: %.0f Hz, %d averages", testFreqHz, numAverages);
}

void thd_stop_measurement() {
    _measuring = false;
    LOG_I("[THD] Measurement stopped");
}

void thd_process_fft_buffer(const float *fftMag, int numBins, float binFreqHz, float sampleRate) {
    if (!_measuring || !fftMag || numBins <= 0 || binFreqHz <= 0.0f) return;

    // Find the fundamental bin
    int fundamentalBin = (int)(_testFreqHz / binFreqHz + 0.5f);
    if (fundamentalBin <= 0 || fundamentalBin >= numBins) {
        _measuring = false;
        _result.valid = false;
        return;
    }

    // Fundamental power (peak bin +/- 1 for spectral leakage)
    float fundPower = 0.0f;
    for (int b = fundamentalBin - 1; b <= fundamentalBin + 1 && b < numBins; b++) {
        if (b >= 0) fundPower += fftMag[b] * fftMag[b];
    }
    _fundamentalPowerSum += fundPower;

    // Harmonic powers (2nd through 9th)
    float harmonicTotalPower = 0.0f;
    for (int h = 0; h < THD_MAX_HARMONICS; h++) {
        int harmBin = (int)((_testFreqHz * (h + 2)) / binFreqHz + 0.5f);
        float harmPower = 0.0f;
        if (harmBin > 0 && harmBin < numBins) {
            // Peak bin +/- 1
            for (int b = harmBin - 1; b <= harmBin + 1 && b < numBins; b++) {
                if (b >= 0) harmPower += fftMag[b] * fftMag[b];
            }
        }
        _harmonicPowerSum[h] += harmPower;
        harmonicTotalPower += harmPower;
    }

    // Total power (all bins)
    float totalPower = 0.0f;
    for (int b = 1; b < numBins; b++) { // Skip DC bin
        totalPower += fftMag[b] * fftMag[b];
    }
    _totalPowerSum += totalPower;

    // Noise = total - fundamental - harmonics
    float noisePower = totalPower - fundPower - harmonicTotalPower;
    if (noisePower < 0.0f) noisePower = 0.0f;
    _noisePowerSum += noisePower;

    _processedFrames++;
    _result.framesProcessed = _processedFrames;

    // Check if measurement is complete
    if (_processedFrames >= _targetFrames) {
        _measuring = false;

        // Average
        float avgFund = _fundamentalPowerSum / _targetFrames;
        float avgNoise = _noisePowerSum / _targetFrames;
        float avgTotal = _totalPowerSum / _targetFrames;

        // THD+N = sqrt((harmonics + noise) / total)
        float distortionPower = 0.0f;
        for (int h = 0; h < THD_MAX_HARMONICS; h++) {
            distortionPower += _harmonicPowerSum[h] / _targetFrames;
        }
        distortionPower += avgNoise;

        if (avgTotal > 0.0f) {
            _result.thdPlusNPercent = sqrtf(distortionPower / avgTotal) * 100.0f;
        } else {
            _result.thdPlusNPercent = 0.0f;
        }

        if (_result.thdPlusNPercent > 0.0f) {
            _result.thdPlusNDb = 20.0f * log10f(_result.thdPlusNPercent / 100.0f);
        } else {
            _result.thdPlusNDb = -120.0f;
        }

        // Fundamental level in dBFS
        if (avgFund > 0.0f) {
            _result.fundamentalDbfs = 10.0f * log10f(avgFund);
        } else {
            _result.fundamentalDbfs = -120.0f;
        }

        // Harmonic levels relative to fundamental
        for (int h = 0; h < THD_MAX_HARMONICS; h++) {
            float avgHarm = _harmonicPowerSum[h] / _targetFrames;
            if (avgHarm > 0.0f && avgFund > 0.0f) {
                _result.harmonicLevels[h] = 10.0f * log10f(avgHarm / avgFund);
            } else {
                _result.harmonicLevels[h] = -120.0f;
            }
        }

        _result.valid = true;
        LOG_I("[THD] Measurement complete: THD+N=%.3f%% (%.1f dB)", _result.thdPlusNPercent, _result.thdPlusNDb);
    }
}

ThdResult thd_get_result() {
    return _result;
}

bool thd_is_measuring() {
    return _measuring;
}

float thd_get_test_freq() {
    return _testFreqHz;
}

#endif // DSP_ENABLED
