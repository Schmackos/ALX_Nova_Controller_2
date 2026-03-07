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
#include "../i2s_audio.h"   // AudioHealthStatus, NUM_AUDIO_ADCS
#else
#include "../../test/test_mocks/Arduino.h"
#define LOG_I(...)
#define LOG_W(...)
#define LOG_D(...)
#define NUM_AUDIO_ADCS 2
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

static FlapState _flapState[NUM_AUDIO_ADCS];

// ===== Firmware health source =====
// Returns the AudioHealthStatus for the given ADC lane.
// In firmware: reads appState. In native tests: reads _mockAdcHealth[].

#ifdef NATIVE_TEST

static int _mockAdcHealth[NUM_AUDIO_ADCS] = { AUDIO_OK, AUDIO_OK };

void hal_audio_health_bridge_set_mock_status(uint8_t lane, int status) {
    if (lane < NUM_AUDIO_ADCS) _mockAdcHealth[lane] = status;
}

static int _get_adc_health(uint8_t lane) {
    if (lane >= NUM_AUDIO_ADCS) return AUDIO_OK;
    return _mockAdcHealth[lane];
}

#else // Firmware

static int _get_adc_health(uint8_t lane) {
    if (lane >= NUM_AUDIO_ADCS) return (int)AUDIO_OK;
    return (int)appState.audioAdc[lane].healthStatus;
}

#endif // NATIVE_TEST

// ===== Flap guard helpers =====

// Record a new AVAIL↔UNAVAIL transition for a lane, pruning stale entries.
// Returns true when the transition count in the window exceeds FLAP_MAX_TRANSITIONS.
static bool _flap_record_transition(uint8_t lane, uint32_t nowMs) {
    if (lane >= NUM_AUDIO_ADCS) return false;
    FlapState& fs = _flapState[lane];

    // Shift new timestamp into circular storage
    uint32_t cutoff = nowMs - FLAP_WINDOW_MS;
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
    if (lane >= NUM_AUDIO_ADCS) return 0;
    FlapState& fs = _flapState[lane];
    uint32_t cutoff = nowMs - FLAP_WINDOW_MS;
    uint8_t count = 0;
    for (uint8_t i = 0; i < fs.transitionCount && i < 3; i++) {
        if (fs.transitionTimes[i] >= cutoff) count++;
    }
    return count;
}

// ===== Public API =====

void hal_audio_health_bridge_init() {
    memset(_flapState, 0, sizeof(_flapState));
    LOG_I("[HAL:HealthBridge] Audio health bridge initialised");
}

void hal_health_flap_reset() {
    memset(_flapState, 0, sizeof(_flapState));
}

void hal_audio_health_check() {
    HalDeviceManager& mgr = HalDeviceManager::instance();
    uint32_t nowMs = (uint32_t)millis();

    for (uint8_t lane = 0; lane < NUM_AUDIO_ADCS; lane++) {
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
}

#ifdef NATIVE_TEST
void hal_audio_health_bridge_reset_for_test() {
    memset(_flapState, 0, sizeof(_flapState));
    for (uint8_t i = 0; i < NUM_AUDIO_ADCS; i++) {
        _mockAdcHealth[i] = (int)AUDIO_OK;
    }
}
#endif // NATIVE_TEST

#endif // DAC_ENABLED
