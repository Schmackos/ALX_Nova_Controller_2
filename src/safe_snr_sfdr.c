// Safe SNR/SFDR — stack-only implementations that override the pre-built ESP-DSP
// versions which use new[] (C++ heap alloc) and crash with std::bad_alloc on OOM.
//
// These are linked from src/ object files, which take precedence over archive
// members in libespressif__esp-dsp.a at link time.

#include <math.h>
#include <stdint.h>

float dsps_snr_f32(const float *input, int32_t len, uint8_t use_dc) {
    if (!input || len <= 0) return -999.0f;

    float maxVal = 0.0f;
    int maxBin = 0;
    int start = use_dc ? 0 : 1;
    for (int i = start; i < len; i++) {
        float v = input[i] >= 0.0f ? input[i] : -input[i];
        if (v > maxVal) {
            maxVal = v;
            maxBin = i;
        }
    }
    if (maxVal <= 0.0f) return -999.0f;

    // Signal power: peak +/- 2 bins
    float signalPower = 0.0f;
    for (int i = maxBin - 2; i <= maxBin + 2; i++) {
        if (i >= start && i < len) {
            signalPower += input[i] * input[i];
        }
    }

    // Noise power: everything else
    float noisePower = 0.0f;
    for (int i = start; i < len; i++) {
        if (i < maxBin - 2 || i > maxBin + 2) {
            noisePower += input[i] * input[i];
        }
    }

    if (noisePower <= 0.0f) return 999.0f;
    return 10.0f * log10f(signalPower / noisePower);
}

float dsps_sfdr_f32(const float *input, int32_t len, int8_t use_dc) {
    if (!input || len <= 0) return -999.0f;

    float max1 = 0.0f, max2 = 0.0f;
    int start = use_dc ? 0 : 1;
    for (int i = start; i < len; i++) {
        float v = input[i] >= 0.0f ? input[i] : -input[i];
        if (v > max1) {
            max2 = max1;
            max1 = v;
        } else if (v > max2) {
            max2 = v;
        }
    }

    if (max1 <= 0.0f) return -999.0f;
    if (max2 <= 0.0f) return 999.0f;

    return 20.0f * log10f(max1 / max2);
}
