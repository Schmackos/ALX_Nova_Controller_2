#ifndef DSP_CONVOLUTION_H
#define DSP_CONVOLUTION_H

#ifdef DSP_ENABLED

#include <stdint.h>

#define CONV_PARTITION_SIZE 256   // Match DSP buffer size
#define CONV_MAX_PARTITIONS 96   // 96 x 256 = 24,576 samples = 0.51s @ 48kHz
#define CONV_MAX_IR_SLOTS   2    // 2 IR slots (one per stereo pair)

struct ConvState {
    int numPartitions;
    int irLength;                   // Original IR length in samples
    float **irPartitions;           // [numPartitions][CONV_PARTITION_SIZE] time-domain partitions
    float *overlapBuf;              // [CONV_PARTITION_SIZE] overlap-add state
    bool active;                    // Slot is loaded and ready
};

// Initialize a convolution slot with an IR buffer.
// Returns 0 on success, -1 on failure (memory, too long, etc.)
int conv_init_slot(int slot, const float *ir, int irLength);

// Free all resources for a convolution slot.
void conv_free_slot(int slot);

// Process one buffer through convolution (overlap-add, time-domain).
// buf must have exactly CONV_PARTITION_SIZE samples.
void conv_process(int slot, float *buf, int len);

// Check if a slot is active.
bool conv_is_active(int slot);

// Get the IR length (in samples) for a slot.
int conv_get_ir_length(int slot);

#endif // DSP_ENABLED
#endif // DSP_CONVOLUTION_H
