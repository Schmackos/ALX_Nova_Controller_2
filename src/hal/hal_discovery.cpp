#ifdef DAC_ENABLED

#include "hal_discovery.h"
#include "hal_device_manager.h"
#include "hal_device_db.h"
#include "hal_driver_registry.h"
#include "hal_eeprom_v3.h"
// HalDacAdapter removed -- EEPROM-discovered devices use HalDriverRegistry factories

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#include "../dac_eeprom.h"
#include "../app_state.h"
#include "../diag_journal.h"
#include <Wire.h>
#include <sdkconfig.h>
#include "hal_ess_sabre_adc_base.h"  // for extern TwoWire Wire2
#else
#define LOG_I(tag, ...) ((void)0)
#define LOG_W(tag, ...) ((void)0)
#define LOG_E(tag, ...) ((void)0)
#endif

bool hal_wifi_sdio_active() {
#ifndef NATIVE_TEST
    // WiFi SDIO pins (GPIO 48/54) are in use when:
    // 1. WiFi is connected (connectSuccess)
    // 2. WiFi is in the process of connecting (SDIO active during handshake)
    // 3. activeInterface is NET_WIFI (Ethernet failover path)
    return appState.wifi.connectSuccess
        || appState.wifi.connecting
        || appState.ethernet.activeInterface == NET_WIFI;
#else
    return false;  // Native tests use test-local mock
#endif
}

int hal_discover_devices() {
    int newDevices = 0;

#ifndef NATIVE_TEST
    HalDeviceManager& mgr = HalDeviceManager::instance();

    LOG_I("[HAL:Discovery]", "Starting device discovery...");

    // Phase 1: I2C bus scan
    // Skip HAL_I2C_BUS_EXT (GPIO48/54) if WiFi is active (SDIO conflict)
    bool wifiActive = hal_wifi_sdio_active();

    if (!wifiActive) {
        uint8_t extCount = hal_i2c_scan_bus(HAL_I2C_BUS_EXT);
        LOG_I("[HAL:Discovery]", "Bus EXT scan: %u device(s)", extCount);
    } else {
        LOG_I("[HAL:Discovery]", "Skipping Bus EXT (WiFi active, SDIO conflict)");
        diag_emit(DIAG_HAL_I2C_BUS_CONFLICT, DIAG_SEV_INFO, 0, "Bus0", "WiFi SDIO active");
    }

    // HAL_I2C_BUS_EXP (GPIO28/29) — always safe to scan
    uint8_t expCount = hal_i2c_scan_bus(HAL_I2C_BUS_EXP);
    LOG_I("[HAL:Discovery]", "Bus EXP scan: %u device(s)", expCount);

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
                        // Use HalDriverRegistry factory to create the device
                        HalDevice* dev = entry->factory ? entry->factory() : nullptr;
                        if (dev) {
                            int slot = mgr.registerDevice(dev, HAL_DISC_EEPROM);
                            if (slot < 0) {
                                LOG_W("[HAL:Discovery]", "Device registration failed (slots full): %s", desc.name);
                                delete dev;
                            } else if (appState.halAutoDiscovery) {
                                dev->_state = HAL_STATE_AVAILABLE;
                                LOG_I("[HAL:Discovery]", "Device auto-registered: %s (slot %d)", desc.name, slot);
                                newDevices++;
                            } else {
                                dev->_state = HAL_STATE_CONFIGURING;
                                LOG_I("[HAL:Discovery]", "Device registered, awaiting init: %s (slot %d)", desc.name, slot);
                                newDevices++;
                            }
                        } else {
                            LOG_W("[HAL:Discovery]", "Factory returned null for %s", desc.compatible);
                        }
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
        diag_emit(DIAG_HAL_I2C_BUS_CONFLICT, DIAG_SEV_INFO, 0, "EEPROM", "WiFi SDIO active");
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
    TwoWire *bus = nullptr;
    bool needsInit = false;

    switch (busIndex) {
        case HAL_I2C_BUS_EXT:
            // Bus 0: GPIO 48/54 — SDIO conflict check is done by caller
            bus = &Wire1;
            if (!Wire1.begin(48, 54, 100000)) {
                LOG_W("[HAL:Discovery]", "Wire1.begin(48,54) failed — bus EXT unavailable");
                return 0;
            }
            needsInit = true;
            break;
        case HAL_I2C_BUS_ONBOARD:
            // Bus 1: GPIO 7/8 — already initialized by ES8311 driver
            bus = &Wire;
            break;
        case HAL_I2C_BUS_EXP: {
            // Bus 2: GPIO 28/29 — expansion bus (Wire2 defined in hal_ess_sabre_adc_base.cpp)
            if (!Wire2.begin(28, 29, 100000)) {
                LOG_W("[HAL:Discovery]", "Wire2.begin(28,29) failed — bus EXP unavailable");
                return 0;
            }
            bus = &Wire2;
            needsInit = true;
            break;
        }
        default:
            return 0;
    }

    // Track addresses that return timeout for retry
    uint8_t timeoutAddrs[HAL_PROBE_RETRY_MAX_ADDRS];
    uint8_t timeoutCount = 0;
    memset(timeoutAddrs, 0, sizeof(timeoutAddrs));

    // Scan standard I2C address range (0x08-0x77, skip reserved)
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        bus->beginTransmission(addr);
        uint8_t err = bus->endTransmission();
        if (err == 0) {
            found++;
            LOG_I("[HAL:Discovery]", "Bus %u: device at 0x%02X", busIndex, addr);
        } else if ((err == 4 || err == 5) && timeoutCount < HAL_PROBE_RETRY_MAX_ADDRS) {
            timeoutAddrs[timeoutCount++] = addr;
        }
    }

    // Retry pass: only addresses that returned timeout (err 4/5)
    // NACK (err 2) means "nobody home" — no point retrying
    if (timeoutCount > 0) {
        LOG_I("[HAL:Discovery]", "Bus %u: retrying %u timeout addresses", busIndex, timeoutCount);
        for (uint8_t retry = 0; retry < HAL_PROBE_RETRY_COUNT; retry++) {
            vTaskDelay(pdMS_TO_TICKS(HAL_PROBE_RETRY_BACKOFF_MS * (retry + 1)));
            uint8_t remaining = 0;
            for (uint8_t i = 0; i < timeoutCount; i++) {
                if (timeoutAddrs[i] == 0) continue; // already found
                bus->beginTransmission(timeoutAddrs[i]);
                uint8_t retryErr = bus->endTransmission();
                if (retryErr == 0) {
                    found++;
                    LOG_I("[HAL:Discovery]", "Bus %u: device at 0x%02X (retry %u)",
                          busIndex, timeoutAddrs[i], retry + 1);
                    char diagMsg[24];
                    snprintf(diagMsg, sizeof(diagMsg), "0x%02X retry %u", timeoutAddrs[i], (unsigned)(retry + 1));
                    diag_emit(DIAG_HAL_PROBE_RETRY_OK, DIAG_SEV_INFO, 0xFF, "I2C", diagMsg);
                    timeoutAddrs[i] = 0; // mark as found
                } else {
                    remaining++;
                }
            }
            if (remaining == 0) break; // all found
        }
    }

    // Release bus if we initialized it (avoid holding pins)
    if (needsInit) {
        bus->end();
    }
#else
    (void)busIndex;
#endif

    return found;
}

#endif // DAC_ENABLED
