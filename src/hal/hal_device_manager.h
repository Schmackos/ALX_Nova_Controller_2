#pragma once
// HAL Device Manager — singleton managing all registered HAL devices

#include "hal_device.h"

// Callback type for forEach iteration
typedef void (*HalDeviceCallback)(HalDevice* device, void* ctx);

// State change callback — fired on every _state transition
typedef void (*HalStateChangeCb)(uint8_t slot, HalDeviceState oldState, HalDeviceState newState);

// Per-device retry state for self-healing (non-blocking, timestamp-based)
struct HalRetryState {
    uint8_t  count;         // Current retry count (0-3)
    uint32_t nextRetryMs;   // millis() when next attempt is allowed
    uint16_t lastErrorCode; // From last failed HalInitResult
};

class HalDeviceManager {
public:
    // Meyers singleton — thread-safe on C++11
    static HalDeviceManager& instance();

    // Register a device. Returns slot index (0..HAL_MAX_DEVICES-1) or -1 on failure.
    int registerDevice(HalDevice* device, HalDiscovery discovery);

    // Remove a device by slot. Returns true if found and removed.
    bool removeDevice(uint8_t slot);

    // Lookup
    HalDevice* getDevice(uint8_t slot);
    HalDevice* findByCompatible(const char* compatible);
    HalDevice* findByType(HalDeviceType type, uint8_t nth = 0);
    uint8_t    getCount() const;

    // Lifecycle — main loop only
    void initAll();           // Priority-sorted init (descending priority)
    void healthCheckAll();    // Periodic health check + non-blocking retry

    // Iteration
    void forEach(HalDeviceCallback cb, void* ctx = nullptr);

    // Pin claim tracking
    bool claimPin(int8_t gpio, HalBusType bus, uint8_t busIndex, uint8_t slot);
    bool releasePin(int8_t gpio);
    bool isPinClaimed(int8_t gpio) const;

    // Per-device config
    HalDeviceConfig* getConfig(uint8_t slot);
    bool setConfig(uint8_t slot, const HalDeviceConfig& cfg);

    // Retry state accessors
    const HalRetryState* getRetryState(uint8_t slot) const;

    // NVS hardware fault counter — incremented when retry exhaustion → ERROR
    uint8_t getFaultCount(uint8_t slot) const;

    // State change callback — registered once at boot by hal_pipeline_bridge
    void setStateChangeCallback(HalStateChangeCb cb);

    // Reset all state (for testing)
    void reset();

private:
    HalDeviceManager();
    ~HalDeviceManager() {}
    HalDeviceManager(const HalDeviceManager&);
    HalDeviceManager& operator=(const HalDeviceManager&);

    void _resetRetryState(uint8_t slot);

    HalDevice*      _devices[HAL_MAX_DEVICES];
    HalDeviceConfig _configs[HAL_MAX_DEVICES];
    HalPinAlloc     _pins[HAL_MAX_PINS];
    HalRetryState   _retryState[HAL_MAX_DEVICES];
    uint8_t         _faultCount[HAL_MAX_DEVICES]; // Persistent across health checks, reset on manager reset
    uint8_t         _count;
    HalStateChangeCb _stateChangeCb = nullptr;
};
