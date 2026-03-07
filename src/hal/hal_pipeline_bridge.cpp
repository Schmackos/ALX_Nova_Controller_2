#ifdef DAC_ENABLED

#include "hal_pipeline_bridge.h"
#include "hal_device_manager.h"
#include "hal_types.h"

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#include "../app_state.h"
#include "../audio_pipeline.h"
#include "../audio_output_sink.h"
#else
#define LOG_I(...)
#define LOG_W(...)
#define LOG_D(...)
// Provide sink slot constants for native test builds
#ifndef AUDIO_SINK_SLOT_PRIMARY
#define AUDIO_SINK_SLOT_PRIMARY 0
#define AUDIO_SINK_SLOT_ES8311  1
#endif
#endif

// ---------------------------------------------------------------------------
// Slot mapping tables
//   _halSlotToSinkSlot[n] >= 0  : HAL slot n owns pipeline sink slot n_sink
//   _halSlotToAdcLane[n]  >= 0  : HAL slot n owns ADC lane n_lane
// Both are initialised to -1 (unmapped).
// ---------------------------------------------------------------------------
static int8_t _halSlotToSinkSlot[HAL_MAX_DEVICES];
static int8_t _halSlotToAdcLane[HAL_MAX_DEVICES];

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Map a HAL device's type to a pipeline sink slot index.
// Returns -1 if the device is not an output device.
static int8_t _sinkSlotForDevice(HalDevice* dev) {
    HalDeviceType type = dev->getType();
    if (type == HAL_DEV_DAC) {
        return AUDIO_SINK_SLOT_PRIMARY;   // 0 — PCM5102A
    }
    if (type == HAL_DEV_CODEC) {
        return AUDIO_SINK_SLOT_ES8311;    // 1 — ES8311
    }
    return -1;
}

// Map a HAL device to an ADC lane index.
// First PCM1808 in slot order → lane 0; second → lane 1.
// Returns -1 if the device is not an input device.
static int8_t _adcLaneForDevice(HalDevice* dev) {
    HalDeviceType type = dev->getType();
    if (type != HAL_DEV_ADC) return -1;
    // CODEC with ADC path (e.g. ES8311) is handled as output only for simplicity —
    // its ADC path is controlled via the codec driver directly.
    // Count how many ADC slots are already mapped to find this device's ordinal.
    uint8_t thisSlot = dev->getSlot();
    int8_t lane = 0;
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        if (_halSlotToAdcLane[i] >= 0 && (uint8_t)i < thisSlot) {
            lane++;
        }
    }
    if (lane >= 2) return -1;  // Only 2 ADC lanes (NUM_AUDIO_ADCS)
    return lane;
}

// ---------------------------------------------------------------------------
// on_device_available — called when a device reaches HAL_STATE_AVAILABLE
// ---------------------------------------------------------------------------
void hal_pipeline_on_device_available(uint8_t slot) {
    if (slot >= HAL_MAX_DEVICES) return;

    HalDeviceManager& mgr = HalDeviceManager::instance();
    HalDevice* dev = mgr.getDevice(slot);
    if (!dev) return;

    HalDeviceType type = dev->getType();
    const char* name   = dev->getDescriptor().name;

    // --- Output sink (DAC / CODEC) ---
    int8_t sinkSlot = _sinkSlotForDevice(dev);
    if (sinkSlot >= 0) {
        // dac_hal.cpp registers the AudioOutputSink during init() via
        // audio_pipeline_register_sink() / audio_pipeline_set_sink().
        // The bridge only needs to record the mapping and update dirty flags
        // so the web UI reflects the new device immediately.
        _halSlotToSinkSlot[slot] = sinkSlot;
        LOG_I("[HAL] Pipeline bridge: output %s (HAL slot %d) → sink slot %d",
              name, slot, (int)sinkSlot);
    }

    // --- Input lane (ADC) ---
    int8_t adcLane = _adcLaneForDevice(dev);
    if (adcLane >= 0) {
        _halSlotToAdcLane[slot] = adcLane;
        LOG_I("[HAL] Pipeline bridge: input %s (HAL slot %d) → ADC lane %d",
              name, slot, (int)adcLane);
#ifndef NATIVE_TEST
        appState.adcEnabled[adcLane] = true;
        appState.markAdcEnabledDirty();
#endif
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
        LOG_I("[HAL] Pipeline bridge: output %s (HAL slot %d) transiently unavailable"
              " — sink slot %d preserved", name, slot, (int)_halSlotToSinkSlot[slot]);
    }
    if (_halSlotToAdcLane[slot] >= 0) {
        // ADC lanes are hardware I2S; a transient unavailability does not warrant
        // disabling the lane — the DMA keeps running and DOUT may recover.
        LOG_I("[HAL] Pipeline bridge: input %s (HAL slot %d) transiently unavailable"
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
        int8_t sinkSlot = _halSlotToSinkSlot[slot];
        LOG_I("[HAL] Pipeline bridge: removing output %s (HAL slot %d) from sink slot %d",
              name, slot, (int)sinkSlot);
#ifndef NATIVE_TEST
        audio_pipeline_remove_sink(sinkSlot);
#endif
        _halSlotToSinkSlot[slot] = -1;
    }

    // --- Disable ADC lane ---
    if (_halSlotToAdcLane[slot] >= 0) {
        int8_t lane = _halSlotToAdcLane[slot];
        LOG_I("[HAL] Pipeline bridge: disabling ADC lane %d (HAL slot %d, %s)",
              (int)lane, slot, name);
#ifndef NATIVE_TEST
        appState.adcEnabled[lane] = false;
        appState.markAdcEnabledDirty();
#endif
        _halSlotToAdcLane[slot] = -1;
    }

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
        _halSlotToSinkSlot[i] = -1;
        _halSlotToAdcLane[i]  = -1;
    }

    // Register the state-change callback so future transitions fire automatically
    HalDeviceManager::instance().setStateChangeCallback(hal_pipeline_state_change);

    // Scan devices that are already AVAILABLE (boot-time sync)
    int outputs = 0;
    int inputs  = 0;
    HalDeviceManager::instance().forEach([](HalDevice* dev, void* ctx) {
        int* counts = static_cast<int*>(ctx);
        if (dev->_state == HAL_STATE_AVAILABLE && dev->_ready) {
            uint8_t slot = dev->getSlot();
            hal_pipeline_on_device_available(slot);
            if (_halSlotToSinkSlot[slot] >= 0) counts[0]++;
            if (_halSlotToAdcLane[slot]  >= 0) counts[1]++;
        }
    }, (void*)new int[2]{0, 0});

    // Recount from tables (forEach ctx lifetime is local above — recount here)
    outputs = 0;
    inputs  = 0;
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        if (_halSlotToSinkSlot[i] >= 0) outputs++;
        if (_halSlotToAdcLane[i]  >= 0) inputs++;
    }

    LOG_I("[HAL] Pipeline bridge sync complete: %d output(s), %d input(s)", outputs, inputs);
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
        if (_halSlotToAdcLane[i] >= 0) count++;
    }
    return count;
}

// ---------------------------------------------------------------------------
// Reset — for unit tests only
// ---------------------------------------------------------------------------
void hal_pipeline_reset() {
    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        _halSlotToSinkSlot[i] = -1;
        _halSlotToAdcLane[i]  = -1;
    }
}

#endif // DAC_ENABLED
