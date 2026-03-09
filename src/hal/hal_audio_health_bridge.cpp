#ifdef DAC_ENABLED

#include "hal_audio_health_bridge.h"
#include "hal_pipeline_bridge.h"
#include "hal_device_manager.h"
#include "hal_types.h"
#include "../diag_journal.h"

// ===== Platform includes =====
#ifndef NATIVE_TEST
#include <Arduino.h>
#include "../debug_serial.h"
#include "../app_state.h"
#include "../i2s_audio.h"   // AudioHealthStatus, AUDIO_PIPELINE_MAX_INPUTS
#else
#include "../../test/test_mocks/Arduino.h"
#define LOG_I(...)
#define LOG_W(...)
#define LOG_D(...)
#ifndef AUDIO_PIPELINE_MAX_INPUTS
#define AUDIO_PIPELINE_MAX_INPUTS 8
#endif
#endif

#include <string.h>

// ===== Flap guard constants =====
static const uint8_t  FLAP_MAX_TRANSITIONS = 2;   // >2 transitions → ERROR
static const uint32_t FLAP_WINDOW_MS       = 30000UL; // Rolling 30-second window

// ===== Per-lane flap guard state =====
struct FlapState {
    uint32_t transitionTimes[3]; // Timestamps of last 3 AVAIL↔UNAVAIL transitions
    uint8_t  transitionCount;    // Number of transitions recorded in the window
};

static FlapState _flapState[AUDIO_PIPELINE_MAX_INPUTS];

// ===== Rule 1: I2S Recovery Storm =====
// Emit DIAG_AUDIO_I2S_RECOVERY (WARN) when >3 I2S recoveries occur within 60s.
static const uint8_t  I2S_RECOVERY_STORM_THRESHOLD = 3;
static const uint32_t I2S_RECOVERY_STORM_WINDOW_MS  = 60000UL;

static uint32_t _prevRecoveries[AUDIO_PIPELINE_MAX_INPUTS];           // Last seen recovery counter value
static uint32_t _recoveryTimestamps[AUDIO_PIPELINE_MAX_INPUTS][4];    // Rolling timestamps of last 4 increments
static uint8_t  _recoveryCount[AUDIO_PIPELINE_MAX_INPUTS];            // How many timestamps are valid

// ===== Rule 4: Audio No-Data with HAL AVAILABLE =====
// Emit DIAG_AUDIO_PIPELINE_STALL (WARN) once per episode when AUDIO_NO_DATA
// is observed while the HAL device is still AVAILABLE.
static uint8_t _noDataWithAvailCount[AUDIO_PIPELINE_MAX_INPUTS];  // 0=idle, >0=episode in progress

// ===== Rule 5: DC Offset Drift =====
// Emit DIAG_AUDIO_DC_OFFSET_HIGH (WARN) when dcOffset > 0.05 for >10 consecutive
// checks (50 s at the nominal 5 s check interval).
static const float    DC_OFFSET_THRESHOLD    = 0.05f;
static const uint8_t  DC_OFFSET_HOLD_CHECKS  = 10;

static uint8_t _dcOffsetHighCount[AUDIO_PIPELINE_MAX_INPUTS];

// ===== Firmware health source =====
// Returns the AudioHealthStatus for the given ADC lane.
// In firmware: reads appState. In native tests: reads _mockAdcHealth[].

#ifdef NATIVE_TEST

static int      _mockAdcHealth[AUDIO_PIPELINE_MAX_INPUTS]     = {};
static uint32_t _mockI2sRecoveries[AUDIO_PIPELINE_MAX_INPUTS] = {};
static float    _mockDcOffset[AUDIO_PIPELINE_MAX_INPUTS]      = {};

void hal_audio_health_bridge_set_mock_status(uint8_t lane, int status) {
    if (lane < AUDIO_PIPELINE_MAX_INPUTS) _mockAdcHealth[lane] = status;
}

void hal_audio_health_bridge_set_mock_i2s_recoveries(uint8_t lane, uint32_t count) {
    if (lane < AUDIO_PIPELINE_MAX_INPUTS) _mockI2sRecoveries[lane] = count;
}

void hal_audio_health_bridge_set_mock_dc_offset(uint8_t lane, float offset) {
    if (lane < AUDIO_PIPELINE_MAX_INPUTS) _mockDcOffset[lane] = offset;
}

static int _get_adc_health(uint8_t lane) {
    if (lane >= AUDIO_PIPELINE_MAX_INPUTS) return AUDIO_OK;
    return _mockAdcHealth[lane];
}

static uint32_t _get_i2s_recoveries(uint8_t lane) {
    if (lane >= AUDIO_PIPELINE_MAX_INPUTS) return 0;
    return _mockI2sRecoveries[lane];
}

static float _get_dc_offset(uint8_t lane) {
    if (lane >= AUDIO_PIPELINE_MAX_INPUTS) return 0.0f;
    return _mockDcOffset[lane];
}

#else // Firmware

static int _get_adc_health(uint8_t lane) {
    if (lane >= AUDIO_PIPELINE_MAX_INPUTS) return (int)AUDIO_OK;
    return (int)appState.audio.adc[lane].healthStatus;
}

static uint32_t _get_i2s_recoveries(uint8_t lane) {
    if (lane >= AUDIO_PIPELINE_MAX_INPUTS) return 0;
    return appState.audio.adc[lane].i2sRecoveries;
}

static float _get_dc_offset(uint8_t lane) {
    if (lane >= AUDIO_PIPELINE_MAX_INPUTS) return 0.0f;
    return appState.audio.adc[lane].dcOffset;
}

#endif // NATIVE_TEST

// ===== Flap guard helpers =====

// Record a new AVAIL↔UNAVAIL transition for a lane, pruning stale entries.
// Returns true when the transition count in the window exceeds FLAP_MAX_TRANSITIONS.
static bool _flap_record_transition(uint8_t lane, uint32_t nowMs) {
    if (lane >= AUDIO_PIPELINE_MAX_INPUTS) return false;
    FlapState& fs = _flapState[lane];

    // Shift new timestamp into circular storage
    // Guard against uint32_t underflow when nowMs < window size.
    uint32_t cutoff = (nowMs >= FLAP_WINDOW_MS) ? (nowMs - FLAP_WINDOW_MS) : 0;
    uint8_t validCount = 0;
    for (uint8_t i = 0; i < fs.transitionCount && i < 3; i++) {
        if (fs.transitionTimes[i] >= cutoff) {
            fs.transitionTimes[validCount++] = fs.transitionTimes[i];
        }
    }
    // Append new transition (cap at 3 stored entries)
    if (validCount < 3) {
        fs.transitionTimes[validCount++] = nowMs;
    } else {
        // Shift oldest out and append
        fs.transitionTimes[0] = fs.transitionTimes[1];
        fs.transitionTimes[1] = fs.transitionTimes[2];
        fs.transitionTimes[2] = nowMs;
        validCount = 3;
    }
    fs.transitionCount = validCount;

    return (fs.transitionCount > FLAP_MAX_TRANSITIONS);
}

// Count transitions still within the window without adding a new one.
static uint8_t _flap_count_in_window(uint8_t lane, uint32_t nowMs) {
    if (lane >= AUDIO_PIPELINE_MAX_INPUTS) return 0;
    FlapState& fs = _flapState[lane];
    uint32_t cutoff = (nowMs >= FLAP_WINDOW_MS) ? (nowMs - FLAP_WINDOW_MS) : 0;
    uint8_t count = 0;
    for (uint8_t i = 0; i < fs.transitionCount && i < 3; i++) {
        if (fs.transitionTimes[i] >= cutoff) count++;
    }
    return count;
}

// ===== Public API =====

void hal_audio_health_bridge_init() {
    memset(_flapState,            0, sizeof(_flapState));
    memset(_prevRecoveries,       0, sizeof(_prevRecoveries));
    memset(_recoveryTimestamps,   0, sizeof(_recoveryTimestamps));
    memset(_recoveryCount,        0, sizeof(_recoveryCount));
    memset(_noDataWithAvailCount, 0, sizeof(_noDataWithAvailCount));
    memset(_dcOffsetHighCount,    0, sizeof(_dcOffsetHighCount));
    LOG_I("[HAL:HealthBridge] Audio health bridge initialised");
}

void hal_health_flap_reset() {
    memset(_flapState, 0, sizeof(_flapState));
}

void hal_audio_health_check() {
    HalDeviceManager& mgr = HalDeviceManager::instance();
    uint32_t nowMs = (uint32_t)millis();

    for (uint8_t lane = 0; lane < AUDIO_PIPELINE_MAX_INPUTS; lane++) {
        // 1. Resolve HAL slot for this ADC lane.
        int8_t halSlot = hal_pipeline_get_slot_for_adc_lane(lane);
        if (halSlot < 0) continue;  // No HAL device mapped to this lane

        HalDevice* dev = mgr.getDevice((uint8_t)halSlot);
        if (!dev) continue;

        int health = _get_adc_health(lane);
        HalDeviceState currentState = dev->_state;
        const char* name = dev->getDescriptor().name;

        // 2. Fault condition: HW_FAULT or I2S_ERROR while AVAILABLE → UNAVAILABLE
        bool isFault = (health == (int)AUDIO_HW_FAULT || health == (int)AUDIO_I2S_ERROR);
        if (isFault && currentState == HAL_STATE_AVAILABLE) {
            LOG_W("[HAL:HealthBridge] Audio health bridge: lane %u fault (status=%d) — marking HAL slot %d UNAVAILABLE",
                  (unsigned)lane, health, (int)halSlot);

            dev->_ready  = false;
            HalDeviceState old = dev->_state;
            dev->_state  = HAL_STATE_UNAVAILABLE;

            diag_emit(DIAG_HAL_HEALTH_FAIL, DIAG_SEV_WARN,
                      (uint8_t)halSlot, name, "audio health fault");

            // Notify bridge (UNAVAILABLE → _ready=false only, sink preserved)
            hal_pipeline_state_change((uint8_t)halSlot, old, HAL_STATE_UNAVAILABLE);

            // Flap guard
            if (_flap_record_transition(lane, nowMs)) {
                LOG_W("[HAL:HealthBridge] Audio health bridge: lane %u flapping — escalating to ERROR", (unsigned)lane);
                HalDeviceState old2 = dev->_state;
                dev->_state = HAL_STATE_ERROR;
                dev->_ready = false;

                diag_emit(DIAG_HAL_DEVICE_FLAPPING, DIAG_SEV_CRIT,
                          (uint8_t)halSlot, name, "AVAIL/UNAVAIL flapping >2 in 30s");

                // Notify bridge (ERROR → sink removed)
                hal_pipeline_state_change((uint8_t)halSlot, old2, HAL_STATE_ERROR);
            }
            continue;
        }

        // 3. Recovery: AUDIO_OK while UNAVAILABLE → AVAILABLE
        if (health == (int)AUDIO_OK && currentState == HAL_STATE_UNAVAILABLE) {
            LOG_I("[HAL:HealthBridge] Audio health bridge: lane %u recovered — marking HAL slot %d AVAILABLE",
                  (unsigned)lane, (int)halSlot);

            dev->_ready  = true;
            HalDeviceState old = dev->_state;
            dev->_state  = HAL_STATE_AVAILABLE;

            diag_emit(DIAG_AUDIO_ADC_RECOVERED, DIAG_SEV_INFO,
                      (uint8_t)halSlot, name, "audio ADC recovered");

            // Notify bridge (AVAILABLE → update dirty flags, keep existing sink)
            hal_pipeline_state_change((uint8_t)halSlot, old, HAL_STATE_AVAILABLE);

            // Flap guard — count recovery as a transition
            if (_flap_record_transition(lane, nowMs)) {
                LOG_W("[HAL:HealthBridge] Audio health bridge: lane %u flapping (recover side) — escalating to ERROR",
                      (unsigned)lane);
                HalDeviceState old2 = dev->_state;
                dev->_state = HAL_STATE_ERROR;
                dev->_ready = false;

                diag_emit(DIAG_HAL_DEVICE_FLAPPING, DIAG_SEV_CRIT,
                          (uint8_t)halSlot, name, "AVAIL/UNAVAIL flapping >2 in 30s");

                hal_pipeline_state_change((uint8_t)halSlot, old2, HAL_STATE_ERROR);
            }
            continue;
        }

        // 4. NOISE_ONLY, CLIPPING, NO_DATA → warning-level only, no HAL state change
        //    (these are audio-quality indicators, not hardware faults)
        (void)_flap_count_in_window; // Suppress unused-function warning in firmware build
    }

    // =========================================================
    // Post-loop diagnostic rules — run for every mapped ADC lane
    // regardless of the fault/recovery path taken above.
    // =========================================================

    for (uint8_t lane = 0; lane < AUDIO_PIPELINE_MAX_INPUTS; lane++) {
        int8_t halSlot = hal_pipeline_get_slot_for_adc_lane(lane);
        if (halSlot < 0) continue;

        HalDevice* dev = mgr.getDevice((uint8_t)halSlot);
        if (!dev) continue;

        const char* name = dev->getDescriptor().name;
        int health = _get_adc_health(lane);

        // ---------------------------------------------------------
        // Rule 1: I2S Recovery Storm
        // If i2sRecoveries increments >3 times within 60 s, emit WARN.
        // Guarded: i2sRecoveries is only meaningful on firmware;
        // in native tests the mock value starts at 0 and can be driven
        // via hal_audio_health_bridge_set_mock_i2s_recoveries().
        // ---------------------------------------------------------
        {
            uint32_t currentRecoveries = _get_i2s_recoveries(lane);
            if (currentRecoveries != _prevRecoveries[lane]) {
                // Counter incremented since last check — record the timestamp.
                // Guard against uint32_t underflow when nowMs < window size.
                uint32_t cutoff = (nowMs >= I2S_RECOVERY_STORM_WINDOW_MS)
                                  ? (nowMs - I2S_RECOVERY_STORM_WINDOW_MS) : 0;
                uint8_t valid = 0;
                for (uint8_t i = 0; i < _recoveryCount[lane] && i < 4; i++) {
                    if (_recoveryTimestamps[lane][i] >= cutoff) {
                        _recoveryTimestamps[lane][valid++] = _recoveryTimestamps[lane][i];
                    }
                }
                if (valid < 4) {
                    _recoveryTimestamps[lane][valid++] = nowMs;
                } else {
                    _recoveryTimestamps[lane][0] = _recoveryTimestamps[lane][1];
                    _recoveryTimestamps[lane][1] = _recoveryTimestamps[lane][2];
                    _recoveryTimestamps[lane][2] = _recoveryTimestamps[lane][3];
                    _recoveryTimestamps[lane][3] = nowMs;
                    valid = 4;
                }
                _recoveryCount[lane] = valid;
                _prevRecoveries[lane] = currentRecoveries;

                if (valid > I2S_RECOVERY_STORM_THRESHOLD) {
                    LOG_W("[HAL:HealthBridge] lane %u: I2S recovery storm (%u in 60s)",
                          (unsigned)lane, (unsigned)valid);
                    diag_emit(DIAG_AUDIO_I2S_RECOVERY, DIAG_SEV_WARN,
                              (uint8_t)halSlot, name, "I2S recovery storm (>3 in 60s)");
                }
            }
        }

        // ---------------------------------------------------------
        // Rule 4: Audio No-Data with HAL AVAILABLE
        // If health is AUDIO_NO_DATA but device is AVAILABLE, the
        // pipeline has stalled. Emit once per episode; reset when
        // the status changes away from NO_DATA.
        // ---------------------------------------------------------
        {
            HalDeviceState curState = dev->_state;
            if (health == (int)AUDIO_NO_DATA && curState == HAL_STATE_AVAILABLE) {
                if (_noDataWithAvailCount[lane] == 0) {
                    LOG_W("[HAL:HealthBridge] lane %u: NO_DATA while HAL AVAILABLE — pipeline stall",
                          (unsigned)lane);
                    diag_emit(DIAG_AUDIO_PIPELINE_STALL, DIAG_SEV_WARN,
                              (uint8_t)halSlot, name, "NO_DATA while HAL AVAILABLE");
                }
                if (_noDataWithAvailCount[lane] < 255) _noDataWithAvailCount[lane]++;
            } else {
                _noDataWithAvailCount[lane] = 0;
            }
        }

        // ---------------------------------------------------------
        // Rule 5: DC Offset Drift
        // If dcOffset > 0.05 for >10 consecutive checks (50 s at
        // the nominal 5 s interval), emit WARN once per episode.
        // ---------------------------------------------------------
        {
            float dc = _get_dc_offset(lane);
            if (dc > DC_OFFSET_THRESHOLD) {
                if (_dcOffsetHighCount[lane] < 255) _dcOffsetHighCount[lane]++;
                if (_dcOffsetHighCount[lane] == DC_OFFSET_HOLD_CHECKS + 1) {
                    // First check that crosses the hold threshold — emit once.
                    LOG_W("[HAL:HealthBridge] lane %u: DC offset %.3f sustained >%u checks",
                          (unsigned)lane, (double)dc, (unsigned)DC_OFFSET_HOLD_CHECKS);
                    diag_emit(DIAG_AUDIO_DC_OFFSET_HIGH, DIAG_SEV_WARN,
                              (uint8_t)halSlot, name, "DC offset > 5% sustained");
                }
            } else {
                _dcOffsetHighCount[lane] = 0;
            }
        }
    }
}

#ifdef NATIVE_TEST
void hal_audio_health_bridge_reset_for_test() {
    memset(_flapState,            0, sizeof(_flapState));
    memset(_prevRecoveries,       0, sizeof(_prevRecoveries));
    memset(_recoveryTimestamps,   0, sizeof(_recoveryTimestamps));
    memset(_recoveryCount,        0, sizeof(_recoveryCount));
    memset(_noDataWithAvailCount, 0, sizeof(_noDataWithAvailCount));
    memset(_dcOffsetHighCount,    0, sizeof(_dcOffsetHighCount));
    for (uint8_t i = 0; i < AUDIO_PIPELINE_MAX_INPUTS; i++) {
        _mockAdcHealth[i]     = (int)AUDIO_OK;
        _mockI2sRecoveries[i] = 0;
        _mockDcOffset[i]      = 0.0f;
    }
}
#endif // NATIVE_TEST

#endif // DAC_ENABLED
