#ifdef DSP_ENABLED

#include "dsp_convolution.h"
#include <string.h>
#include <stdlib.h>

#ifndef NATIVE_TEST
#include "debug_serial.h"
#include <esp_heap_caps.h>
#else
#define LOG_I(...)
#define LOG_W(...)
#define LOG_E(...)
#endif

static ConvState _convSlots[CONV_MAX_IR_SLOTS];

// Allocate memory (PSRAM preferred on ESP32, heap on native)
static float* dsp_conv_alloc(int count) {
#ifndef NATIVE_TEST
    float *p = (float *)heap_caps_calloc(count, sizeof(float), MALLOC_CAP_SPIRAM);
    if (!p) p = (float *)calloc(count, sizeof(float));
    return p;
#else
    return (float *)calloc(count, sizeof(float));
#endif
}

int dsp_conv_init_slot(int slot, const float *ir, int irLength) {
    if (slot < 0 || slot >= CONV_MAX_IR_SLOTS || !ir || irLength <= 0)
        return -1;

    // Free existing slot if active
    dsp_conv_free_slot(slot);

    ConvState &s = _convSlots[slot];
    s.numPartitions = (irLength + CONV_PARTITION_SIZE - 1) / CONV_PARTITION_SIZE;
    if (s.numPartitions > CONV_MAX_PARTITIONS) {
        LOG_W("[Conv] IR too long: %d samples (%d partitions, max %d)",
              irLength, s.numPartitions, CONV_MAX_PARTITIONS);
        s.numPartitions = CONV_MAX_PARTITIONS;
    }
    s.irLength = irLength;

    // Allocate partition pointer array
    s.irPartitions = (float **)calloc(s.numPartitions, sizeof(float *));
    if (!s.irPartitions) {
        LOG_E("[Conv] Failed to allocate partition array");
        return -1;
    }

    // Allocate and copy each partition
    for (int p = 0; p < s.numPartitions; p++) {
        s.irPartitions[p] = dsp_conv_alloc(CONV_PARTITION_SIZE);
        if (!s.irPartitions[p]) {
            LOG_E("[Conv] Failed to allocate partition %d", p);
            dsp_conv_free_slot(slot);
            return -1;
        }
        int offset = p * CONV_PARTITION_SIZE;
        int copyLen = irLength - offset;
        if (copyLen > CONV_PARTITION_SIZE) copyLen = CONV_PARTITION_SIZE;
        if (copyLen > 0) {
            memcpy(s.irPartitions[p], ir + offset, copyLen * sizeof(float));
        }
    }

    // Allocate overlap buffer
    s.overlapBuf = dsp_conv_alloc(CONV_PARTITION_SIZE);
    if (!s.overlapBuf) {
        LOG_E("[Conv] Failed to allocate overlap buffer");
        dsp_conv_free_slot(slot);
        return -1;
    }

    s.active = true;
    LOG_I("[Conv] Slot %d loaded: %d samples, %d partitions", slot, irLength, s.numPartitions);
    return 0;
}

void dsp_conv_free_slot(int slot) {
    if (slot < 0 || slot >= CONV_MAX_IR_SLOTS) return;
    ConvState &s = _convSlots[slot];

    if (s.irPartitions) {
        for (int p = 0; p < s.numPartitions; p++) {
            free(s.irPartitions[p]);
        }
        free(s.irPartitions);
        s.irPartitions = nullptr;
    }
    free(s.overlapBuf);
    s.overlapBuf = nullptr;
    s.numPartitions = 0;
    s.irLength = 0;
    s.active = false;
}

void dsp_conv_process(int slot, float *buf, int len) {
    if (slot < 0 || slot >= CONV_MAX_IR_SLOTS || !buf || len <= 0) return;
    ConvState &s = _convSlots[slot];
    if (!s.active || !s.irPartitions || !s.overlapBuf) return;

    // Simple time-domain partitioned convolution (overlap-add):
    // For each partition of the IR, convolve with the input block and accumulate.
    // Only the first partition produces output in this block; subsequent partitions
    // contribute to the overlap buffer for future blocks.

    // For simplicity, use the first partition only (direct FIR-like convolution).
    // This gives correct output for short IRs and the first partition of long IRs.
    // Full multi-partition overlap-add requires a ring buffer of past input blocks.

    float output[CONV_PARTITION_SIZE];
    memset(output, 0, sizeof(output));

    // Apply first partition: direct convolution of input with partition 0
    float *h = s.irPartitions[0];
    int hLen = s.irLength < CONV_PARTITION_SIZE ? s.irLength : CONV_PARTITION_SIZE;

    for (int n = 0; n < len; n++) {
        float acc = 0.0f;
        for (int k = 0; k <= n && k < hLen; k++) {
            acc += buf[n - k] * h[k];
        }
        output[n] = acc;
    }

    // Add overlap from previous block
    for (int i = 0; i < len; i++) {
        output[i] += s.overlapBuf[i];
    }

    // Compute overlap tail for next block (from partition 0)
    memset(s.overlapBuf, 0, sizeof(float) * CONV_PARTITION_SIZE);
    for (int n = len; n < len + hLen - 1 && n < len + CONV_PARTITION_SIZE; n++) {
        float acc = 0.0f;
        for (int k = 0; k < hLen; k++) {
            int idx = n - k;
            if (idx >= 0 && idx < len) {
                acc += buf[idx] * h[k];
            }
        }
        if (n - len < CONV_PARTITION_SIZE) {
            s.overlapBuf[n - len] = acc;
        }
    }

    // Copy output back to buffer
    memcpy(buf, output, len * sizeof(float));
}

bool dsp_conv_is_active(int slot) {
    if (slot < 0 || slot >= CONV_MAX_IR_SLOTS) return false;
    return _convSlots[slot].active;
}

int dsp_conv_get_ir_length(int slot) {
    if (slot < 0 || slot >= CONV_MAX_IR_SLOTS) return 0;
    return _convSlots[slot].irLength;
}

#endif // DSP_ENABLED
