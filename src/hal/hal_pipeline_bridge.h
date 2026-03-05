#pragma once
// HAL Pipeline Bridge — synchronizes HAL device states with audio pipeline
// Phase 4: Sync layer only — actual audio routing unchanged (legacy path)
// Future: will register AudioOutputSink per AVAILABLE device

#ifdef DAC_ENABLED

#include "hal_types.h"

// Sync HAL device states after boot — called once after all devices registered
void hal_pipeline_sync();

// Notify bridge when a device becomes available (state → AVAILABLE)
void hal_pipeline_on_device_available(uint8_t slot);

// Notify bridge when a device is removed (state → REMOVED/ERROR)
void hal_pipeline_on_device_removed(uint8_t slot);

// Get count of pipeline-ready audio output devices
int hal_pipeline_output_count();

// Get count of pipeline-ready audio input devices
int hal_pipeline_input_count();

#endif // DAC_ENABLED
