#pragma once
// HAL Device Manager — singleton managing all registered HAL devices
// Phase 0: Purely additive — no existing files modified

#include "hal_device.h"

// Callback type for forEach iteration
typedef void (*HalDeviceCallback)(HalDevice* device, void* ctx);

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
    void healthCheckAll();    // Periodic health check (30s timer)

    // Iteration
    void forEach(HalDeviceCallback cb, void* ctx = nullptr);

    // Pin claim tracking
    bool claimPin(int8_t gpio, HalBusType bus, uint8_t busIndex, uint8_t slot);
    bool releasePin(int8_t gpio);
    bool isPinClaimed(int8_t gpio) const;

    // Reset all state (for testing)
    void reset();

private:
    HalDeviceManager();
    ~HalDeviceManager() {}
    HalDeviceManager(const HalDeviceManager&);
    HalDeviceManager& operator=(const HalDeviceManager&);

    HalDevice*  _devices[HAL_MAX_DEVICES];
    HalPinAlloc _pins[HAL_MAX_PINS];
    uint8_t     _count;
};
