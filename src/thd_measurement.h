#ifndef THD_MEASUREMENT_H
#define THD_MEASUREMENT_H

#ifdef DSP_ENABLED

#include <stdint.h>

#define THD_MAX_HARMONICS 8

struct ThdResult {
    float thdPlusNPercent;      // THD+N as percentage
    float thdPlusNDb;           // THD+N in dB
    float fundamentalDbfs;      // Fundamental level in dBFS
    float harmonicLevels[THD_MAX_HARMONICS]; // Harmonic levels in dB relative to fundamental
    bool valid;                 // True if measurement is complete and valid
    uint16_t framesProcessed;  // Number of FFT frames averaged so far
    uint16_t framesTarget;     // Target number of frames
};

// Start a THD+N measurement.
// testFreqHz: frequency of the test tone (signal generator will be started)
// numAverages: number of FFT frames to average (4, 8, or 16)
void thd_start_measurement(float testFreqHz, uint16_t numAverages);

// Stop/cancel an ongoing measurement.
void thd_stop_measurement();

// Process one FFT magnitude buffer. Called once per FFT frame during measurement.
// fftMag: array of magnitude values (one per bin)
// numBins: number of FFT bins
// binFreqHz: frequency resolution per bin (sampleRate / fftSize)
// sampleRate: current sample rate
void thd_process_fft_buffer(const float *fftMag, int numBins, float binFreqHz, float sampleRate);

// Get current measurement result.
ThdResult thd_get_result();

// Check if a measurement is currently in progress.
bool thd_is_measuring();

// Get the test frequency of the current measurement.
float thd_get_test_freq();

#endif // DSP_ENABLED
#endif // THD_MEASUREMENT_H
