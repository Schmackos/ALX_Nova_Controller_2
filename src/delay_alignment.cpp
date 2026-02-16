#ifdef DSP_ENABLED

#include "delay_alignment.h"
#include "dsps_corr.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#ifndef NATIVE_TEST
#include "dsp_pipeline.h"
#include "dsp_coefficients.h"
#include "app_state.h"
#include "debug_serial.h"
#else
#include "../../src/dsp_pipeline.h"
#include "../../src/dsp_coefficients.h"
#define LOG_I(...)
#define LOG_W(...)
#endif

static const float MAX_24BIT = 8388607.0f;
static const float CONFIDENCE_THRESHOLD = 3.0f;

DelayAlignResult delay_align_measure(const int32_t *adc1_buf, int len1,
                                      const int32_t *adc2_buf, int len2,
                                      uint32_t sampleRate, int maxLag) {
    DelayAlignResult result = {0, 0.0f, 0.0f, false};
    if (!adc1_buf || !adc2_buf || len1 <= 0 || len2 <= 0 || sampleRate == 0 || maxLag <= 0)
        return result;

    // Use the shorter buffer length
    int len = len1 < len2 ? len1 : len2;
    if (len < maxLag * 2) return result; // Not enough data

    // Extract left channel and normalize to float
    int monoLen = len;
    float *sig1 = (float *)calloc(monoLen, sizeof(float));
    float *sig2 = (float *)calloc(monoLen, sizeof(float));
    if (!sig1 || !sig2) {
        free(sig1); free(sig2);
        return result;
    }

    for (int i = 0; i < monoLen; i++) {
        sig1[i] = (float)adc1_buf[i * 2] / MAX_24BIT;
        sig2[i] = (float)adc2_buf[i * 2] / MAX_24BIT;
    }

    // Cross-correlate: sig2 as signal, sig1 as pattern (shorter window).
    // Peak at index N means sig2 is delayed by N samples relative to sig1.
    int patLen = monoLen - maxLag;
    if (patLen <= 0) { free(sig1); free(sig2); return result; }

    int corrLen = monoLen - patLen + 1; // = maxLag + 1
    float *corr = (float *)calloc(corrLen, sizeof(float));
    if (!corr) { free(sig1); free(sig2); return result; }

    dsps_corr_f32(sig2, monoLen, sig1, patLen, corr);

    // Find peak in correlation
    float maxCorr = -1e30f;
    int maxIdx = 0;
    float corrSum = 0.0f;
    float corrSumSq = 0.0f;

    for (int i = 0; i < corrLen; i++) {
        float absCorr = fabsf(corr[i]);
        corrSum += absCorr;
        corrSumSq += absCorr * absCorr;
        if (absCorr > maxCorr) {
            maxCorr = absCorr;
            maxIdx = i;
        }
    }

    // Compute confidence: peak / RMS
    float corrRms = 0.0f;
    if (corrLen > 0) {
        corrRms = sqrtf(corrSumSq / (float)corrLen);
    }

    result.delaySamples = maxIdx;
    result.delayMs = (float)maxIdx / (float)sampleRate * 1000.0f;
    result.confidence = (corrRms > 1e-10f) ? maxCorr / corrRms : 0.0f;
    result.valid = result.confidence >= CONFIDENCE_THRESHOLD;

    free(sig1); free(sig2); free(corr);
    return result;
}

void delay_align_auto_apply(const DelayAlignResult &result, int adcIndex) {
#ifndef NATIVE_TEST
    if (!result.valid || result.delaySamples == 0) return;

    // Apply delay to the earlier channel pair
    // Positive delaySamples = ADC2 leads → delay ADC2 (channels 2,3)
    int targetCh = (result.delaySamples > 0) ? adcIndex * 2 : 0;

    dsp_copy_active_to_inactive();
    DspState *cfg = dsp_get_inactive_config();
    int absSamples = abs(result.delaySamples);
    if (absSamples > DSP_MAX_DELAY_SAMPLES) absSamples = DSP_MAX_DELAY_SAMPLES;

    // Check if channel already has a delay stage, update it
    for (int i = 0; i < cfg->channels[targetCh].stageCount; i++) {
        if (cfg->channels[targetCh].stages[i].type == DSP_DELAY) {
            cfg->channels[targetCh].stages[i].delay.delaySamples = (uint16_t)absSamples;
            cfg->channels[targetCh].stages[i].enabled = true;
            if (!dsp_swap_config()) { appState.dspSwapFailures++; appState.lastDspSwapFailure = millis(); }
            LOG_I("[Align] Updated delay on ch%d: %d samples (%.2f ms)",
                  targetCh, absSamples, result.delayMs);
            return;
        }
    }

    // No existing delay stage — add one
    int pos = dsp_add_stage(targetCh, DSP_DELAY);
    if (pos >= 0) {
        cfg->channels[targetCh].stages[pos].delay.delaySamples = (uint16_t)absSamples;
        if (!dsp_swap_config()) { appState.dspSwapFailures++; appState.lastDspSwapFailure = millis(); }
        LOG_I("[Align] Added delay stage on ch%d: %d samples (%.2f ms)",
              targetCh, absSamples, result.delayMs);
    }
#endif
}

#endif // DSP_ENABLED
