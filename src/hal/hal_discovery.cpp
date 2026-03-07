#ifdef DAC_ENABLED

#include "hal_discovery.h"
#include "hal_device_manager.h"
#include "hal_device_db.h"
#include "hal_driver_registry.h"
#include "hal_eeprom_v3.h"
#include "hal_dac_adapter.h"

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

    LOG_I("[HAL:Discovery]", "Starting device discovery...");

    // Phase 1: I2C bus scan
    // Skip HAL_I2C_BUS_EXT (GPIO48/54) if WiFi is active (SDIO conflict)
    bool wifiActive = (appState.activeInterface == NET_WIFI);

    if (!wifiActive) {
        uint8_t extMask = hal_i2c_scan_bus(HAL_I2C_BUS_EXT);
        LOG_I("[HAL:Discovery]", "Bus EXT scan: 0x%02X", extMask);
    } else {
        LOG_I("[HAL:Discovery]", "Skipping Bus EXT (WiFi active, SDIO conflict)");
    }

    // HAL_I2C_BUS_EXP (GPIO28/29) — always safe to scan
    uint8_t expMask = hal_i2c_scan_bus(HAL_I2C_BUS_EXP);
    LOG_I("[HAL:Discovery]", "Bus EXP scan: 0x%02X", expMask);

    // Phase 2: EEPROM probe — skip when WiFi active (same GPIO48/54 SDIO conflict)
    uint8_t eepMask = 0;
    if (!wifiActive) {
        dac_i2c_scan(&eepMask);
        DacEepromData eepromData;
        if (dac_eeprom_scan(&eepromData, eepMask)) {
            const HalDriverEntry* entry = nullptr;
            if (eepromData.deviceId > 0) {
                entry = hal_registry_find_by_legacy_id(eepromData.deviceId);
            }

            if (entry) {
                HalDeviceDescriptor desc;
                if (hal_db_lookup(entry->compatible, &desc)) {
                    LOG_I("[HAL:Discovery]", "EEPROM found at 0x%02X: %s (id=0x%04X)",
                          eepromData.i2cAddress, eepromData.deviceName, eepromData.deviceId);

                    // Check if device is already registered (don't duplicate)
                    bool alreadyRegistered = false;
                    for (uint8_t i = 0; i < HAL_MAX_DEVICES; i++) {
                        HalDevice* existing = mgr.getDevice(i);
                        if (existing && existing->getDiscovery() == HAL_DISC_EEPROM &&
                            strcmp(existing->getDescriptor().compatible, desc.compatible) == 0) {
                            alreadyRegistered = true;
                            break;
                        }
                    }

                    if (!alreadyRegistered) {
                        // Create HalDacAdapter for DAC-type devices
                        if (desc.type == HAL_DEV_DAC) {
                            HalDevice* dev = new HalDacAdapter(nullptr, desc);
                            if (dev) {
                                int slot = mgr.registerDevice(dev, HAL_DISC_EEPROM);
                                if (appState.halAutoDiscovery) {
                                    dev->_state = HAL_STATE_AVAILABLE;
                                    LOG_I("[HAL:Discovery]", "Device auto-registered: %s (slot %d)", desc.name, slot);
                                } else {
                                    dev->_state = HAL_STATE_CONFIGURING;
                                    LOG_I("[HAL:Discovery]", "Device registered, awaiting init: %s (slot %d)", desc.name, slot);
                                }
                                newDevices++;
                            }
                        }
                        // Future: add ADC, CODEC, GPIO device creation here
                    } else {
                        LOG_I("[HAL:Discovery]", "Device already registered: %s", desc.compatible);
                    }
                }
            } else {
                LOG_I("[HAL:Discovery]", "EEPROM at 0x%02X: no matching driver (id=0x%04X) — manual config required",
                      eepromData.i2cAddress, eepromData.deviceId);
                appState.markHalDeviceDirty();
            }
        }
    } else {
        LOG_I("[HAL:Discovery]", "Skipping EEPROM scan (WiFi active, SDIO conflict)");
    }

    LOG_I("[HAL:Discovery]", "Discovery complete: %d new devices", newDevices);

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
