#pragma once
// HAL Pipeline Bridge — synchronizes HAL device states with the audio pipeline
// Phase 3.4: State-driven sink management and ADC lane control

#ifdef DAC_ENABLED

#include "hal_types.h"

// Sync HAL device states after boot — registers the state change callback then
// scans all AVAILABLE devices to update sink/ADC state. Called once from setup().
void hal_pipeline_sync();

// State change callback handler — registered with HalDeviceManager at boot.
// Dispatches to on_device_available, on_device_unavailable, or on_device_removed.
void hal_pipeline_state_change(uint8_t slot, HalDeviceState oldState, HalDeviceState newState);

// Notify bridge when a device becomes available (state → AVAILABLE).
// DAC/CODEC: dac_hal.cpp already registered the sink at init time — update dirty flags.
// ADC: enable adcEnabled lane and mark dirty.
void hal_pipeline_on_device_available(uint8_t slot);

// Notify bridge when a device becomes transiently unavailable (state → UNAVAILABLE).
// Does NOT unregister the sink — isReady() returns false via volatile _ready.
// ADC: no action (hardware may recover at next healthCheck).
void hal_pipeline_on_device_unavailable(uint8_t slot);

// Notify bridge when a device is permanently gone (state → ERROR/REMOVED/MANUAL).
// DAC/CODEC: calls audio_pipeline_remove_sink() for the mapped sink slot.
// ADC: disables adcEnabled lane and marks dirty.
void hal_pipeline_on_device_removed(uint8_t slot);

// Get count of pipeline-ready audio output devices (slots with an active sink mapping)
int hal_pipeline_output_count();

// Get count of pipeline-ready audio input devices (slots with an active ADC lane mapping)
int hal_pipeline_input_count();

// Reverse-lookup: return the HAL slot that owns the given ADC lane.
// Returns -1 when no device is mapped to that lane.
int8_t hal_pipeline_get_slot_for_adc_lane(uint8_t lane);

// Reverse-lookup: return the HAL slot that owns the given pipeline sink slot.
// Returns -1 when no device is mapped to that sink slot.
int8_t hal_pipeline_get_slot_for_sink(uint8_t sinkSlot);

// Forward-lookup: return the pipeline sink slot assigned to a HAL device.
// Returns -1 when the HAL slot has no sink mapping.
int8_t hal_pipeline_get_sink_slot(uint8_t halSlot);

// Forward-lookup: return the input lane assigned to a HAL device.
// Returns -1 when the HAL slot has no input lane mapping.
int8_t hal_pipeline_get_input_lane(uint8_t halSlot);

// Cascade correlation ID support —————————————————————————————————————————
// Call hal_pipeline_begin_correlation() before a multi-step operation that
// should share one corrId across all diag_emit() calls.
// Returns the new corrId (monotonically incrementing, wraps at 0xFFFF).
// Call hal_pipeline_end_correlation() when the operation completes.
// hal_pipeline_active_corr_id() returns 0 when no correlation is active.
uint16_t hal_pipeline_begin_correlation();
void     hal_pipeline_end_correlation();
uint16_t hal_pipeline_active_corr_id();

// Activate a DAC-path device: call dev->buildSink() then audio_pipeline_set_sink().
// Falls back to dac_activate_for_hal() for devices whose buildSink() returns false.
// HC-5: halSlot→sinkSlot mapping is idempotent across state cycles.
void hal_pipeline_activate_device(uint8_t halSlot);

// Deactivate a DAC-path device: remove sink, deinit device.
// HC-2: audioPaused semaphore taken exactly once per deinit batch.
// HC-3: device deinit never touches RX channel / MCLK (device responsibility).
void hal_pipeline_deactivate_device(uint8_t halSlot);

// Reset all bridge state — for unit tests only.
// Clears the HAL-slot→sink-slot and HAL-slot→ADC-lane mapping tables.
void hal_pipeline_reset();

#endif // DAC_ENABLED
