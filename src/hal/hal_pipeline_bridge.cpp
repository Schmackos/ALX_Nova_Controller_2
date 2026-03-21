#ifdef DAC_ENABLED

#include "hal_pipeline_bridge.h"
#include "hal_device_manager.h"
#include "hal_device.h"
#include "hal_audio_device.h"
#include "hal_ns4150b.h"
#include "hal_types.h"

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#include "../app_state.h"
#include "../audio_pipeline.h"
#include "../audio_input_source.h"
#include "../audio_output_sink.h"
#include "../diag_journal.h"
#include "../diag_error_codes.h"
#else
#define LOG_I(...)
#define LOG_W(...)
#define LOG_D(...)
#define LOG_E(...)
#define diag_emit(...) ((void)0)
#ifndef AUDIO_OUT_MAX_SINKS
#define AUDIO_OUT_MAX_SINKS 8
#endif
#ifndef AUDIO_PIPELINE_MAX_INPUTS
#define AUDIO_PIPELINE_MAX_INPUTS 8
#endif
// Stubs for source registration in native test builds
struct AudioInputSource;
inline void audio_pipeline_set_source(int, const AudioInputSource*) {}
inline void audio_pipeline_remove_source(int) {}
// Mock stubs for pipeline sink API -- counters allow tests to verify calls
static int _mock_set_sink_count = 0;
static int _mock_remove_sink_count = 0;
static int _mock_last_set_sink_slot = -1;
static int _mock_last_remove_sink_slot = -1;
inline void audio_pipeline_set_sink(int slot, const AudioOutputSink*) {
    _mock_set_sink_count++;
    _mock_last_set_sink_slot = slot;
}
inline void audio_pipeline_remove_sink(int slot) {
    _mock_remove_sink_count++;
    _mock_last_remove_sink_slot = slot;
}
// Reset all mock counters -- called from test setUp()
inline void hal_pipeline_reset_mock_counters() {
    _mock_set_sink_count = 0;
    _mock_remove_sink_count = 0;
    _mock_last_set_sink_slot = -1;
    _mock_last_remove_sink_slot = -1;
}
#endif

// ---------------------------------------------------------------------------
// Slot mapping tables
//   _halSlotToSinkSlot[n]    >= 0 : HAL slot n owns pipeline sink slot n_sink
//   _halSlotToAdcLane[n]     >= 0 : HAL slot n owns the FIRST ADC lane in its group
//   _halSlotAdcLaneCount[n]  > 0  : number of consecutive lanes owned by HAL slot n
//                                   (1 for all current devices except ES9843PRO = 2)
// Both are initialised to -1 / 0 (unmapped).
// ---------------------------------------------------------------------------
static int8_t  _halSlotToSinkSlot[HAL_MAX_DEVICES];
static int8_t  _halSlotToAdcLane[HAL_MAX_DEVICES];
static uint8_t _halSlotAdcLaneCount[HAL_MAX_DEVICES];

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Infer effective capability flags for a device.  When the device has
// explicit caps (non-zero) we trust them; otherwise fall back to type-based
// inference for backward compatibility with devices registered before the
// capabilities field was populated.
static uint8_t _effectiveCaps(HalDevice* dev) {
    uint8_t caps = dev->getDescriptor().capabilities;
    if (caps != 0) return caps;
    // Infer from device type (conservative defaults)
    switch (dev->getType()) {
        case HAL_DEV_DAC:   return HAL_CAP_DAC_PATH;
        case HAL_DEV_ADC:   return HAL_CAP_ADC_PATH;
        case HAL_DEV_CODEC: return HAL_CAP_DAC_PATH;  // Conservative: DAC only until explicit caps set
        default:            return 0;
    }
}

// Map a HAL device to a pipeline sink slot index using ordinal counting.
// Uses HAL_CAP_DAC_PATH capability — not device type.
// Returns -1 if the device has no DAC path or all slots are taken.
static int8_t _sinkSlotForDevice(HalDevice* dev) {
    uint8_t caps = _effectiveCaps(dev);
    if (!(caps & HAL_CAP_DAC_PATH)) return -1;

    // If this HAL slot already has a mapping, return it (idempotent)
    uint8_t halSlot = dev->getSlot();
    if (_halSlotToSinkSlot[halSlot] >= 0) return _halSlotToSinkSlot[halSlot];

    // Find first free sink slot
    bool taken[AUDIO_OUT_MAX_SINKS] = {};
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        if (_halSlotToSinkSlot[i] >= 0 && _halSlotToSinkSlot[i] < (int8_t)AUDIO_OUT_MAX_SINKS) {
            taken[(int)_halSlotToSinkSlot[i]] = true;
        }
    }
    for (int8_t s = 0; s < (int8_t)AUDIO_OUT_MAX_SINKS; s++) {
        if (!taken[s]) return s;
    }
    return -1;  // All slots taken
}

// Map a HAL device to an ADC lane index using ordinal counting.
// Uses HAL_CAP_ADC_PATH capability — not device type.
// needCount: number of consecutive lanes required (1 for mono/stereo ADCs,
//            2 for ES9843PRO 4-channel TDM which exposes 2 stereo pairs).
// Returns the first free lane index if needCount consecutive free lanes exist,
// or -1 if the device has no ADC path or insufficient lanes are available.
static int8_t _adcLaneForDevice(HalDevice* dev, int needCount = 1) {
    uint8_t caps = _effectiveCaps(dev);
    if (!(caps & HAL_CAP_ADC_PATH)) return -1;

    // If this HAL slot already has a mapping, return it (idempotent)
    uint8_t halSlot = dev->getSlot();
    if (_halSlotToAdcLane[halSlot] >= 0) return _halSlotToAdcLane[halSlot];

    if (needCount < 1) needCount = 1;

    // Build a 'taken' bitmap of all currently claimed lanes
    bool taken[AUDIO_PIPELINE_MAX_INPUTS] = {};
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        if (_halSlotToAdcLane[i] >= 0) {
            int base  = (int)_halSlotToAdcLane[i];
            int count = (int)_halSlotAdcLaneCount[i];
            if (count < 1) count = 1;
            for (int c = 0; c < count; c++) {
                int lane = base + c;
                if (lane < AUDIO_PIPELINE_MAX_INPUTS) taken[lane] = true;
            }
        }
    }

    // Find first run of needCount consecutive free lanes
    for (int8_t l = 0; l <= (int8_t)(AUDIO_PIPELINE_MAX_INPUTS - needCount); l++) {
        bool ok = true;
        for (int c = 0; c < needCount; c++) {
            if (taken[l + c]) { ok = false; break; }
        }
        if (ok) return l;
    }
    return -1;  // Insufficient consecutive lanes
}

// Recount active inputs/outputs and update appState (Phase 2).
// For multi-source devices (ES9843PRO) each lane counts independently.
static void _updateActiveCounts() {
#ifndef NATIVE_TEST
    int outputs = 0, inputs = 0;
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        if (_halSlotToSinkSlot[i] >= 0) outputs++;
        if (_halSlotToAdcLane[i]  >= 0) {
            int count = (int)_halSlotAdcLaneCount[i];
            inputs += (count > 0) ? count : 1;
        }
    }
    appState.audio.activeOutputCount = outputs;
    appState.audio.activeInputCount  = inputs;
#endif
}

// ---------------------------------------------------------------------------
// Amp auto-enable/disable gated by DAC availability
// ---------------------------------------------------------------------------

// Returns true if at least one DAC-path device has a registered sink slot
static bool _anyDacSinkActive() {
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        if (_halSlotToSinkSlot[i] >= 0) return true;
    }
    return false;
}

// Enable/disable all AMP-type devices based on DAC availability
static void _updateAmpGating() {
    bool dacActive = _anyDacSinkActive();
    HalDeviceManager& mgr = HalDeviceManager::instance();
    for (uint8_t i = 0; i < HAL_MAX_DEVICES; i++) {
        HalDevice* d = mgr.getDevice(i);
        if (!d || d->getType() != HAL_DEV_AMP) continue;
        if (d->_state != HAL_STATE_AVAILABLE) continue;
        HalNs4150b* amp = static_cast<HalNs4150b*>(d);
        if (dacActive && !amp->isEnabled()) {
            amp->setEnable(true);
            LOG_I("[HAL:Bridge] Amp auto-enabled: DAC sink active");
        } else if (!dacActive && amp->isEnabled()) {
            amp->setEnable(false);
            LOG_I("[HAL:Bridge] Amp auto-disabled: no DAC sinks active");
        }
    }
}

// ---------------------------------------------------------------------------
// hal_pipeline_activate_device -- bridge owns sink registration
//
// For any device with HAL_CAP_DAC_PATH:
//   1. Get or allocate a sink slot (HC-5: idempotent mapping)
//   2. Call dev->buildSink() -- device populates the AudioOutputSink struct
//   3. Call audio_pipeline_set_sink() with the populated sink struct
//
// HC-1: No calloc under vTaskSuspendAll -- buildSink runs outside scheduler suspend
// ---------------------------------------------------------------------------
void hal_pipeline_activate_device(uint8_t halSlot) {
    if (halSlot >= HAL_MAX_DEVICES) return;

    HalDeviceManager& mgr = HalDeviceManager::instance();
    HalDevice* dev = mgr.getDevice(halSlot);
    if (!dev) return;

    uint8_t caps = _effectiveCaps(dev);
    if (!(caps & HAL_CAP_DAC_PATH)) return;

    // HC-5: Get or allocate a sink slot -- idempotent (returns existing mapping)
    int8_t sinkSlot = _sinkSlotForDevice(dev);
    if (sinkSlot < 0) {
        LOG_W("[HAL:Bridge] No free sink slot for device at HAL slot %u", halSlot);
        diag_emit(DIAG_HAL_SLOT_FULL, DIAG_SEV_WARN, halSlot, dev->getDescriptor().name, "sink slots full");
        return;
    }

    // Record the mapping
    _halSlotToSinkSlot[halSlot] = sinkSlot;

    const char* name = dev->getDescriptor().name;

    // Call buildSink() -- devices with DAC path override this virtual method.
    // buildSink() is virtual on HalDevice base class (returns false by default).
    AudioOutputSink sink = AUDIO_OUTPUT_SINK_INIT;
    if (dev->buildSink((uint8_t)sinkSlot, &sink)) {
        // buildSink succeeded -- bridge registers the sink with the pipeline
        sink.halSlot = halSlot;
        audio_pipeline_set_sink((int)sinkSlot, &sink);
        LOG_I("[HAL:Bridge] Activated via buildSink: %s (HAL slot %u) -> sink slot %d",
              name, halSlot, (int)sinkSlot);
    } else {
        LOG_W("[HAL:Bridge] Activation failed for %s (HAL slot %u) -- buildSink returned false",
              name, halSlot);
        // Clear the mapping on failure so the slot can be reused
        _halSlotToSinkSlot[halSlot] = -1;
    }
}

// ---------------------------------------------------------------------------
// hal_pipeline_deactivate_device — bridge owns sink removal (DEBT-6 Phase 1.4)
//
// HC-2: audioPaused semaphore taken exactly once per deinit batch
// HC-3: device deinit never touches RX channel / MCLK (device's responsibility)
// ---------------------------------------------------------------------------
void hal_pipeline_deactivate_device(uint8_t halSlot) {
    if (halSlot >= HAL_MAX_DEVICES) return;

    int8_t sinkSlot = _halSlotToSinkSlot[halSlot];
    if (sinkSlot < 0) {
        LOG_W("[HAL:Bridge] deactivate_device: HAL slot %u has no sink mapping", halSlot);
        return;
    }

    HalDeviceManager& mgr = HalDeviceManager::instance();
    HalDevice* dev = mgr.getDevice(halSlot);
    const char* name = dev ? dev->getDescriptor().name : "unknown";

    // HC-2: Take audioPaused semaphore ONCE before deinit batch
#ifndef NATIVE_TEST
    AppState& as = AppState::getInstance();
    as.audio.paused = true;
    if (as.audio.taskPausedAck) {
        xSemaphoreTake(as.audio.taskPausedAck, pdMS_TO_TICKS(50));
    }
#endif

    // Device deinit (HC-3: device only touches its own TX path, never RX/MCLK)
    if (dev) {
        dev->deinit();
    }

    // Remove sink from pipeline
    audio_pipeline_remove_sink((int)sinkSlot);

    // Clear the mapping
    _halSlotToSinkSlot[halSlot] = -1;

    // Release audioPaused
#ifndef NATIVE_TEST
    as.audio.paused = false;
#endif

    _updateActiveCounts();

    // Auto-disable amps if no DAC sinks remain
    _updateAmpGating();

    LOG_I("[HAL:Bridge] Deactivated: %s (HAL slot %u, sink slot %d)", name, halSlot, (int)sinkSlot);

    // Broadcast updated state
#ifndef NATIVE_TEST
    as.markHalDeviceDirty();
    as.markChannelMapDirty();
#endif
}

// ---------------------------------------------------------------------------
// on_device_available — called when a device reaches HAL_STATE_AVAILABLE
// ---------------------------------------------------------------------------
void hal_pipeline_on_device_available(uint8_t slot) {
    if (slot >= HAL_MAX_DEVICES) return;

    HalDeviceManager& mgr = HalDeviceManager::instance();
    HalDevice* dev = mgr.getDevice(slot);
    if (!dev) return;

    (void)dev->getType();   // Type used only for logging; capability flags drive logic
    const char* name = dev->getDescriptor().name;

    // --- Output sink (DAC / CODEC with DAC path) ---
    // Delegate to hal_pipeline_activate_device() which owns buildSink() + set_sink()
    uint8_t caps = _effectiveCaps(dev);
    if (caps & HAL_CAP_DAC_PATH) {
        hal_pipeline_activate_device(slot);
    }
    int8_t sinkSlot = _halSlotToSinkSlot[slot]; // Read back mapping (set by activate)

    // --- Input lane (ADC / CODEC with ADC path) ---
    // Multi-source devices (e.g. ES9843PRO in TDM mode) expose N stereo pairs.
    // We query getInputSourceCount() and allocate N consecutive lanes.
    if (caps & HAL_CAP_ADC_PATH) {
        int srcCount = dev->getInputSourceCount();
        if (srcCount < 1) srcCount = 1;  // Defensive: always try at least one lane

        int8_t firstLane = _adcLaneForDevice(dev, srcCount);
        if (firstLane >= 0) {
            _halSlotToAdcLane[slot]      = firstLane;
            _halSlotAdcLaneCount[slot]   = (uint8_t)srcCount;

            LOG_I("[HAL:Bridge] Pipeline bridge: input %s (HAL slot %d) → ADC lane%s %d..%d (%d source%s)",
                  name, slot,
                  (srcCount > 1) ? "s" : "",
                  (int)firstLane, (int)(firstLane + srcCount - 1),
                  srcCount, (srcCount > 1) ? "s" : "");

            for (int si = 0; si < srcCount; si++) {
                int lane = (int)firstLane + si;
#ifndef NATIVE_TEST
                if (lane < AUDIO_PIPELINE_MAX_INPUTS) {
                    appState.audio.adcEnabled[lane] = true;
                }
#endif
                // Register AudioInputSource with the pipeline
#ifndef NATIVE_TEST
                const AudioInputSource* src = dev->getInputSourceAt(si);
                if (src) {
                    AudioInputSource regSrc = *src;  // Value copy
                    regSrc.lane    = (uint8_t)lane;
                    regSrc.halSlot = slot;
                    audio_pipeline_set_source(lane, &regSrc);
                    LOG_I("[HAL:Bridge] Pipeline bridge: registered '%s' at lane %d",
                          regSrc.name ? regSrc.name : "?", lane);
                }
#endif
            }
#ifndef NATIVE_TEST
            appState.markAdcEnabledDirty();
#endif
        } else {
            LOG_W("[HAL:Bridge] No free input lane(s) for device at HAL slot %u (need %d)",
                  slot, srcCount);
            diag_emit(DIAG_HAL_SLOT_FULL, DIAG_SEV_WARN, slot, dev->getDescriptor().name, "input lanes full");
        }
    }

    _updateActiveCounts();

    // Auto-enable amps if a DAC sink just became available
    if (sinkSlot >= 0) {
        _updateAmpGating();
    }

    // Broadcast device list and audio channel map to all web clients
#ifndef NATIVE_TEST
    appState.markHalDeviceDirty();
    appState.markChannelMapDirty();
#endif
}

// ---------------------------------------------------------------------------
// on_device_unavailable — transient failure; volatile _ready=false is enough
// ---------------------------------------------------------------------------
void hal_pipeline_on_device_unavailable(uint8_t slot) {
    if (slot >= HAL_MAX_DEVICES) return;

    HalDeviceManager& mgr = HalDeviceManager::instance();
    HalDevice* dev = mgr.getDevice(slot);
    const char* name = dev ? dev->getDescriptor().name : "unknown";

    if (_halSlotToSinkSlot[slot] >= 0) {
        // isReady() returns false because _ready is already false on the device.
        // The pipeline skips this sink automatically — no removal needed.
        LOG_I("[HAL:Bridge] Pipeline bridge: output %s (HAL slot %d) transiently unavailable"
              " — sink slot %d preserved", name, slot, (int)_halSlotToSinkSlot[slot]);
    }
    if (_halSlotToAdcLane[slot] >= 0) {
        // ADC lanes are hardware I2S; a transient unavailability does not warrant
        // disabling the lane — the DMA keeps running and DOUT may recover.
        LOG_I("[HAL:Bridge] Pipeline bridge: input %s (HAL slot %d) transiently unavailable"
              " — ADC lane %d preserved", name, slot, (int)_halSlotToAdcLane[slot]);
    }
}

// ---------------------------------------------------------------------------
// on_device_removed — permanent removal (ERROR, REMOVED, MANUAL)
// ---------------------------------------------------------------------------
void hal_pipeline_on_device_removed(uint8_t slot) {
    if (slot >= HAL_MAX_DEVICES) return;

    HalDeviceManager& mgr = HalDeviceManager::instance();
    // Device pointer may already be null if removeDevice() was called first.
    HalDevice* dev = mgr.getDevice(slot);
    const char* name = dev ? dev->getDescriptor().name : "unknown";

    // --- Remove output sink ---
    if (_halSlotToSinkSlot[slot] >= 0) {
        LOG_I("[HAL:Bridge] Pipeline bridge: removing output %s (HAL slot %d) from sink slot %d",
              name, slot, (int)_halSlotToSinkSlot[slot]);

        // Delegate to hal_pipeline_deactivate_device() which owns the full
        // deinit sequence: audioPaused semaphore (HC-2), device deinit,
        // audio_pipeline_remove_sink(), mapping clear, amp gating update.
        hal_pipeline_deactivate_device(slot);
    }

    // --- Disable ADC lane(s) — may be more than one for multi-source devices ---
    if (_halSlotToAdcLane[slot] >= 0) {
        int8_t  firstLane = _halSlotToAdcLane[slot];
        uint8_t count     = _halSlotAdcLaneCount[slot];
        if (count < 1) count = 1;

        LOG_I("[HAL:Bridge] Pipeline bridge: disabling ADC lane%s %d..%d (HAL slot %d, %s)",
              (count > 1) ? "s" : "",
              (int)firstLane, (int)(firstLane + count - 1),
              slot, name);

        for (int c = 0; c < (int)count; c++) {
            int lane = (int)firstLane + c;
            audio_pipeline_remove_source(lane);
#ifndef NATIVE_TEST
            if (lane < AUDIO_PIPELINE_MAX_INPUTS) {
                appState.audio.adcEnabled[lane] = false;
            }
#endif
        }
#ifndef NATIVE_TEST
        appState.markAdcEnabledDirty();
#endif
        _halSlotToAdcLane[slot]    = -1;
        _halSlotAdcLaneCount[slot] = 0;
    }

    _updateActiveCounts();

    // Auto-disable amps if no DAC sinks remain
    _updateAmpGating();

    // Broadcast updated state
#ifndef NATIVE_TEST
    appState.markHalDeviceDirty();
    appState.markChannelMapDirty();
#endif
}

// ---------------------------------------------------------------------------
// State change callback — registered with HalDeviceManager
// ---------------------------------------------------------------------------
void hal_pipeline_state_change(uint8_t slot, HalDeviceState oldState, HalDeviceState newState) {
    (void)oldState;  // Unused — newState is the canonical trigger
    switch (newState) {
        case HAL_STATE_AVAILABLE:
            hal_pipeline_on_device_available(slot);
            break;
        case HAL_STATE_UNAVAILABLE:
            hal_pipeline_on_device_unavailable(slot);
            break;
        case HAL_STATE_ERROR:
        case HAL_STATE_REMOVED:
        case HAL_STATE_MANUAL:
            hal_pipeline_on_device_removed(slot);
            break;
        default:
            // UNKNOWN / DETECTED / CONFIGURING — no bridge action needed
            break;
    }
}

// ---------------------------------------------------------------------------
// hal_pipeline_sync — called once from setup() after all devices registered
// ---------------------------------------------------------------------------
void hal_pipeline_sync() {
    // Initialise mapping tables
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        _halSlotToSinkSlot[i]   = -1;
        _halSlotToAdcLane[i]    = -1;
        _halSlotAdcLaneCount[i] = 0;
    }

    // Register the state-change callback so future transitions fire automatically
    HalDeviceManager::instance().setStateChangeCallback(hal_pipeline_state_change);

    // Scan devices that are already AVAILABLE (boot-time sync)
    int outputs = 0;
    int inputs  = 0;
    int counts[2] = {0, 0};
    HalDeviceManager::instance().forEach([](HalDevice* dev, void* ctx) {
        int* c = static_cast<int*>(ctx);
        if (dev->_state == HAL_STATE_AVAILABLE && dev->_ready) {
            uint8_t slot = dev->getSlot();
            hal_pipeline_on_device_available(slot);
            if (_halSlotToSinkSlot[slot] >= 0) c[0]++;
            if (_halSlotToAdcLane[slot]  >= 0) c[1]++;
        }
    }, (void*)counts);

    // Recount from tables (forEach ctx lifetime is local above — recount here)
    outputs = 0;
    inputs  = 0;
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        if (_halSlotToSinkSlot[i] >= 0) outputs++;
        if (_halSlotToAdcLane[i]  >= 0) inputs++;
    }

    _updateActiveCounts();

    LOG_I("[HAL:Bridge] Pipeline bridge sync complete: %d output(s), %d input(s)", outputs, inputs);
}

// ---------------------------------------------------------------------------
// Count helpers
// ---------------------------------------------------------------------------
int hal_pipeline_output_count() {
    int count = 0;
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        if (_halSlotToSinkSlot[i] >= 0) count++;
    }
    return count;
}

int hal_pipeline_input_count() {
    int count = 0;
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        if (_halSlotToAdcLane[i] >= 0) {
            int n = (int)_halSlotAdcLaneCount[i];
            count += (n > 0) ? n : 1;
        }
    }
    return count;
}

// ---------------------------------------------------------------------------
// Reverse lookups
// ---------------------------------------------------------------------------
int8_t hal_pipeline_get_slot_for_adc_lane(uint8_t lane) {
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        if (_halSlotToAdcLane[i] == (int8_t)lane) return (int8_t)i;
    }
    return -1;
}

int8_t hal_pipeline_get_slot_for_sink(uint8_t sinkSlot) {
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        if (_halSlotToSinkSlot[i] == (int8_t)sinkSlot) return (int8_t)i;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Forward lookups (Phase 1)
// ---------------------------------------------------------------------------
int8_t hal_pipeline_get_sink_slot(uint8_t halSlot) {
    if (halSlot >= HAL_MAX_DEVICES) return -1;
    return _halSlotToSinkSlot[halSlot];
}

int8_t hal_pipeline_get_input_lane(uint8_t halSlot) {
    if (halSlot >= HAL_MAX_DEVICES) return -1;
    return _halSlotToAdcLane[halSlot];
}

uint8_t hal_pipeline_get_input_lane_count(uint8_t halSlot) {
    if (halSlot >= HAL_MAX_DEVICES) return 0;
    if (_halSlotToAdcLane[halSlot] < 0) return 0;
    uint8_t n = _halSlotAdcLaneCount[halSlot];
    return (n > 0) ? n : 1;
}

// ---------------------------------------------------------------------------
// Cascade correlation IDs
// ---------------------------------------------------------------------------
static uint16_t _globalCorrCounter = 0;
static uint16_t _activeCorrId      = 0;

uint16_t hal_pipeline_begin_correlation() {
    _activeCorrId = ++_globalCorrCounter;
    return _activeCorrId;
}

void hal_pipeline_end_correlation() {
    _activeCorrId = 0;
}

uint16_t hal_pipeline_active_corr_id() {
    return _activeCorrId;
}

// ---------------------------------------------------------------------------
// Reset — for unit tests only
// ---------------------------------------------------------------------------
void hal_pipeline_reset() {
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        _halSlotToSinkSlot[i]    = -1;
        _halSlotToAdcLane[i]     = -1;
        _halSlotAdcLaneCount[i]  = 0;
    }
    _globalCorrCounter = 0;
    _activeCorrId      = 0;
}

// Clean up NATIVE_TEST no-op macros so they do not leak into subsequent files
// when this .cpp is inline-included in test translation units.
#ifdef NATIVE_TEST
#undef diag_emit
#undef LOG_I
#undef LOG_W
#undef LOG_D
#undef LOG_E
#endif

#endif // DAC_ENABLED
