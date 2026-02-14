#ifndef DELAY_ALIGNMENT_H
#define DELAY_ALIGNMENT_H

#ifdef DSP_ENABLED

#include <stdint.h>

struct DelayAlignResult {
    int delaySamples;       // Measured delay (positive = ADC2 leads)
    float confidence;       // Peak correlation / RMS (>3.0 = reliable)
    float delayMs;          // Delay in milliseconds
    bool valid;             // True if confidence > threshold
};

// Measure delay between two buffers using cross-correlation.
// Buffers are int32 (24-bit left-justified) stereo-interleaved.
// Uses left channel only. maxLag = max search range in samples.
DelayAlignResult delay_align_measure(const int32_t *adc1_buf, int len1,
                                      const int32_t *adc2_buf, int len2,
                                      uint32_t sampleRate, int maxLag);

// Apply measured delay to DSP pipeline: adds/updates delay stage on the earlier channel.
void delay_align_auto_apply(const DelayAlignResult &result, int adcIndex);

#endif // DSP_ENABLED
#endif // DELAY_ALIGNMENT_H
