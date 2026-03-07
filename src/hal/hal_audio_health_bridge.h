#pragma once
// HAL Audio Health Bridge — feeds audio ADC health status into HAL device state.
//
// Runs in the main loop every 5s. Reads AudioHealthStatus for each ADC lane,
// reverse-looks up the owning HAL device slot via hal_pipeline_get_slot_for_adc_lane(),
// and drives HAL state transitions:
//
//   AUDIO_HW_FAULT / AUDIO_I2S_ERROR  → AVAILABLE → UNAVAILABLE + DIAG_HAL_HEALTH_FAIL
//   AUDIO_OK (after UNAVAILABLE)      → UNAVAILABLE → AVAILABLE + DIAG_AUDIO_ADC_RECOVERED
//   >2 AVAIL↔UNAVAIL in 30s           → ERROR + DIAG_HAL_DEVICE_FLAPPING
//
// Flap guard: tracks per-device transition timestamps in a rolling 30s window.
// On firmware: reads appState.audioAdc[lane].healthStatus.
// On native test: reads _mockAdcHealth[lane] set via hal_audio_health_bridge_set_mock_status().

#ifdef DAC_ENABLED

// Call once from setup() — initialises flap guard state.
void hal_audio_health_bridge_init();

// Call from main loop every 5s — checks health and drives HAL state transitions.
void hal_audio_health_check();

// Reset flap guard transition counters and timestamps (all devices).
// Useful in tests and after a factory reset.
void hal_health_flap_reset();

#ifdef NATIVE_TEST
// Mirror of AudioHealthStatus for use in native test builds
// (avoids including i2s_audio.h / app_state.h which pull in firmware headers).
enum AudioHealthStatus {
    AUDIO_OK         = 0,
    AUDIO_NO_DATA    = 1,
    AUDIO_NOISE_ONLY = 2,
    AUDIO_CLIPPING   = 3,
    AUDIO_I2S_ERROR  = 4,
    AUDIO_HW_FAULT   = 5
};

// Set mock health status for an ADC lane (lane 0 or 1).
void hal_audio_health_bridge_set_mock_status(uint8_t lane, int status);

// Set mock i2sRecoveries counter for an ADC lane (Rule 1 — I2S Recovery Storm).
void hal_audio_health_bridge_set_mock_i2s_recoveries(uint8_t lane, uint32_t count);

// Set mock dcOffset for an ADC lane (Rule 5 — DC Offset Drift).
void hal_audio_health_bridge_set_mock_dc_offset(uint8_t lane, float offset);

// Reset all bridge internal state for a clean test.
void hal_audio_health_bridge_reset_for_test();
#endif // NATIVE_TEST

#endif // DAC_ENABLED
