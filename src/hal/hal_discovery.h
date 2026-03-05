#pragma once
// HAL Discovery — orchestrates device discovery across buses
// Phase 2: I2C scan, EEPROM probe, three-tier lookup

#ifdef DAC_ENABLED

#include "hal_types.h"

// Discovery result codes
enum HalDiscoveryResult : uint8_t {
    HAL_DISC_OK             = 0,
    HAL_DISC_NO_DEVICES     = 1,
    HAL_DISC_PARTIAL        = 2,   // Some devices found, some failed
    HAL_DISC_BUS_ERROR      = 3,
};

// Run full device discovery:
// 1. I2C scan HAL_I2C_BUS_EXT (skip if WiFi active) + HAL_I2C_BUS_EXP (always)
// 2. EEPROM probe at 0x50-0x57
// 3. Three-tier lookup for each EEPROM device
// Returns number of new devices registered
int hal_discover_devices();

// Rescan — removes absent external devices, re-probes buses
int hal_rescan();

// I2C bus scan — returns bitmask of responding addresses
// busIndex: HAL_I2C_BUS_EXT, HAL_I2C_BUS_ONBOARD, or HAL_I2C_BUS_EXP
uint8_t hal_i2c_scan_bus(uint8_t busIndex);

#endif // DAC_ENABLED
