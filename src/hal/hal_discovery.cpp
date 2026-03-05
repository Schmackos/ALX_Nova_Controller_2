#ifdef DAC_ENABLED

#include "hal_discovery.h"
#include "hal_device_manager.h"
#include "hal_device_db.h"
#include "hal_driver_registry.h"
#include "hal_eeprom_v3.h"

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#include "../dac_eeprom.h"
#include "../app_state.h"
#include <Wire.h>
#include <sdkconfig.h>
#else
#define LOG_I(tag, ...) ((void)0)
#define LOG_W(tag, ...) ((void)0)
#define LOG_E(tag, ...) ((void)0)
#endif

int hal_discover_devices() {
    int newDevices = 0;

#ifndef NATIVE_TEST
    HalDeviceManager& mgr = HalDeviceManager::instance();

    LOG_I("[HAL Discovery]", "Starting device discovery...");

    // Phase 1: I2C bus scan
    // Skip HAL_I2C_BUS_EXT (GPIO48/54) if WiFi is active (SDIO conflict)
    bool wifiActive = appState.wifiConnectSuccess;

    if (!wifiActive) {
        uint8_t extMask = hal_i2c_scan_bus(HAL_I2C_BUS_EXT);
        LOG_I("[HAL Discovery]", "Bus EXT scan: 0x%02X", extMask);
    } else {
        LOG_I("[HAL Discovery]", "Skipping Bus EXT (WiFi active, SDIO conflict)");
    }

    // HAL_I2C_BUS_EXP (GPIO28/29) — always safe to scan
    uint8_t expMask = hal_i2c_scan_bus(HAL_I2C_BUS_EXP);
    LOG_I("[HAL Discovery]", "Bus EXP scan: 0x%02X", expMask);

    // Phase 2: EEPROM probe — scan bus first to get ACK mask, then read only responding addresses
    uint8_t eepMask = 0;
    dac_i2c_scan(&eepMask);
    DacEepromData eepromData;
    if (dac_eeprom_scan(&eepromData, eepMask)) {
        LOG_I("[HAL Discovery]", "EEPROM found at 0x%02X: %s (id=0x%04X, v%u)",
              eepromData.i2cAddress, eepromData.deviceName,
              eepromData.deviceId, eepromData.formatVersion);

        // Check for v3 compatible string
        // For v1/v2 — try legacy ID lookup via driver registry
        const HalDriverEntry* entry = nullptr;
        if (eepromData.deviceId > 0) {
            entry = hal_registry_find_by_legacy_id(eepromData.deviceId);
        }

        if (entry) {
            // Found a driver — look up descriptor from DB
            HalDeviceDescriptor desc;
            if (hal_db_lookup(entry->compatible, &desc)) {
                LOG_I("[HAL Discovery]", "Matched: %s via legacy ID 0x%04X",
                      entry->compatible, eepromData.deviceId);
                newDevices++;
            }
        }
    }

    LOG_I("[HAL Discovery]", "Discovery complete: %d new devices", newDevices);

    // Signal state change
    appState.markHalDeviceDirty();
#endif

    return newDevices;
}

int hal_rescan() {
    // Remove non-builtin external devices first
    HalDeviceManager& mgr = HalDeviceManager::instance();
    for (uint8_t i = 0; i < HAL_MAX_DEVICES; i++) {
        HalDevice* dev = mgr.getDevice(i);
        if (dev && dev->getDiscovery() != HAL_DISC_BUILTIN) {
            if (dev->_state != HAL_STATE_AVAILABLE) {
                mgr.removeDevice(i);
            }
        }
    }
    return hal_discover_devices();
}

uint8_t hal_i2c_scan_bus(uint8_t busIndex) {
    uint8_t found = 0;

#ifndef NATIVE_TEST
    // Select the correct Wire instance based on bus index
    // Bus 0 (EXT): Wire1 on P4 (GPIO48/54)
    // Bus 1 (ONBOARD): Wire (GPIO7/8) — ES8311 dedicated
    // Bus 2 (EXP): Would need Wire2 or software I2C — placeholder
    (void)busIndex;

    // Scan EEPROM range 0x50-0x57 as a proof of concept
    // Full bus scan deferred to Phase 2 hardware validation
#endif

    return found;
}

#endif // DAC_ENABLED
