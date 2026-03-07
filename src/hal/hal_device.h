#pragma once
// HAL Device — abstract base class for all HAL-managed devices

#include "hal_types.h"
#include "hal_init_result.h"

class HalDevice {
public:
    virtual ~HalDevice() {}

    // ----- Lifecycle methods (ESPHome pattern) -----
    // probe(): Non-destructive check — I2C ACK + chip ID verify
    virtual bool probe() = 0;

    // init(): Full hardware initialisation. Called once after probe succeeds.
    // Returns HalInitResult with error code and reason on failure.
    virtual HalInitResult init() = 0;

    // deinit(): Shutdown and resource release. Safe to call multiple times.
    virtual void deinit() = 0;

    // dumpConfig(): LOG_I output of full descriptor at boot
    virtual void dumpConfig() = 0;

    // healthCheck(): Periodic I2C ACK or register read (30s timer)
    virtual bool healthCheck() = 0;

    // ----- Descriptor -----
    const HalDeviceDescriptor& getDescriptor() const { return _descriptor; }
    HalDeviceType getType() const { return _descriptor.type; }
    uint8_t getSlot() const { return _slot; }
    uint16_t getInitPriority() const { return _initPriority; }
    HalDiscovery getDiscovery() const { return _discovery; }

    // ----- Hot-path state (volatile for cross-core reads) -----
    // Audio pipeline reads _ready directly — NO virtual dispatch
    volatile bool _ready;
    volatile HalDeviceState _state;

protected:
    HalDeviceDescriptor _descriptor;
    uint8_t             _slot;
    uint16_t            _initPriority;
    HalDiscovery        _discovery;

    friend class HalDeviceManager;

    HalDevice() : _ready(false), _state(HAL_STATE_UNKNOWN),
                  _slot(0), _initPriority(HAL_PRIORITY_HARDWARE),
                  _discovery(HAL_DISC_BUILTIN) {
        memset(&_descriptor, 0, sizeof(_descriptor));
    }
};
