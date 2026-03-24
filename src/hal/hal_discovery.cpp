#ifdef DAC_ENABLED

#include "hal_discovery.h"
#include "hal_device_manager.h"
#include "hal_device_db.h"
#include "hal_driver_registry.h"
#include "hal_eeprom_v3.h"
#include "hal_i2c_bus.h"
// HalDacAdapter removed -- EEPROM-discovered devices use HalDriverRegistry factories

// ===== Unmatched address tracking =====
// Updated at the end of each bus scan. Records addresses that responded
// to an I2C probe but were not claimed by any registered driver.
// HAL_UNMATCHED_MAX defined in hal_discovery.h
static HalUnmatchedAddr _unmatchedAddrs[HAL_UNMATCHED_MAX];
static int              _unmatchedCount = 0;

int hal_get_unmatched_addresses(HalUnmatchedAddr* out, int maxOut) {
    if (!out || maxOut <= 0) return 0;
    int n = (_unmatchedCount < maxOut) ? _unmatchedCount : maxOut;
    for (int i = 0; i < n; i++) out[i] = _unmatchedAddrs[i];
    return n;
}

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#include "../dac_eeprom.h"
#include "../app_state.h"
#include "../diag_journal.h"
#include <sdkconfig.h>
#include <Wire.h>
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

// ===== Expansion EEPROM scan (Bus 2 / Wire2, always safe) =====

#ifndef NATIVE_TEST
// Read a block of bytes from EEPROM on the expansion bus (Bus 2).
// Uses Wire2 directly for the repeated-start I2C sequence (write addr ptr, then read).
// Wire2 is defined in hal_i2c_bus.cpp.
extern TwoWire Wire2;
static bool exp_eeprom_read_block(uint8_t i2cAddr, uint8_t memAddr, uint8_t* buf, int len) {
    Wire2.beginTransmission(i2cAddr);
    Wire2.write(memAddr);
    if (Wire2.endTransmission(false) != 0) return false;
    int received = Wire2.requestFrom(i2cAddr, (uint8_t)len);
    if (received != len) return false;
    for (int i = 0; i < len; i++) {
        buf[i] = Wire2.read();
    }
    return true;
}

// Scan Bus 2 (expansion) for EEPROMs with ALXD magic.
// Uses v3 compatible string matching first, falls back to legacy ID.
static int hal_eeprom_scan_expansion(HalDeviceManager& mgr) {
    int newDevices = 0;

    if (!HalI2cBus::get(HAL_I2C_BUS_EXP).begin(28, 29, 100000)) {
        LOG_W("[HAL:Discovery]", "Wire2.begin(28,29) failed for expansion EEPROM scan");
        return 0;
    }

    for (uint8_t addr = DAC_EEPROM_ADDR_START; addr <= DAC_EEPROM_ADDR_END; addr++) {
        // Check EEPROM presence
        if (!HalI2cBus::get(HAL_I2C_BUS_EXP).probe(addr)) continue;

        // Read magic first (4 bytes)
        uint8_t magic[DAC_EEPROM_MAGIC_LEN];
        if (!exp_eeprom_read_block(addr, 0x00, magic, DAC_EEPROM_MAGIC_LEN)) continue;
        if (memcmp(magic, DAC_EEPROM_MAGIC, DAC_EEPROM_MAGIC_LEN) != 0) {
            LOG_D("[HAL:Discovery]", "EXP EEPROM 0x%02X: no ALXD magic", addr);
            continue;
        }

        LOG_I("[HAL:Discovery]", "EXP EEPROM with ALXD magic at 0x%02X", addr);

        // Read full 128 bytes (v3 size) in 16-byte chunks
        uint8_t rawData[DAC_EEPROM_V3_DATA_SIZE];
        memcpy(rawData, magic, DAC_EEPROM_MAGIC_LEN);
        int remaining = DAC_EEPROM_V3_DATA_SIZE - DAC_EEPROM_MAGIC_LEN;
        int offset = DAC_EEPROM_MAGIC_LEN;
        bool readOk = true;
        while (remaining > 0) {
            int chunk = (remaining > 16) ? 16 : remaining;
            if (!exp_eeprom_read_block(addr, (uint8_t)offset, &rawData[offset], chunk)) {
                LOG_W("[HAL:Discovery]", "EXP EEPROM read failed at 0x%02X offset 0x%02X", addr, offset);
                readOk = false;
                break;
            }
            offset += chunk;
            remaining -= chunk;
        }
        if (!readOk) continue;

        // Try v3 compatible string matching first
        const HalDriverEntry* entry = nullptr;
        char compatible[32] = {0};

        uint8_t version = rawData[0x04];
        if (version >= DAC_EEPROM_VERSION_V3 &&
            hal_eeprom_parse_v3(rawData, DAC_EEPROM_V3_DATA_SIZE, compatible)) {
            entry = hal_registry_find(compatible);
            if (entry) {
                LOG_I("[HAL:Discovery]", "EXP EEPROM 0x%02X: v3 match '%s'", addr, compatible);
            } else {
                LOG_I("[HAL:Discovery]", "EXP EEPROM 0x%02X: v3 compatible '%s' — no driver",
                      addr, compatible);
            }
        }

        // Fallback: try v1/v2 legacy ID matching
        if (!entry) {
            DacEepromData eepromData;
            if (dac_eeprom_parse(rawData, DAC_EEPROM_V3_DATA_SIZE, &eepromData)) {
                if (eepromData.deviceId > 0) {
                    entry = hal_registry_find_by_legacy_id(eepromData.deviceId);
                    if (entry) {
                        strncpy(compatible, entry->compatible, 31);
                        compatible[31] = '\0';
                        LOG_I("[HAL:Discovery]", "EXP EEPROM 0x%02X: legacy ID 0x%04X → %s",
                              addr, eepromData.deviceId, compatible);
                    }
                }
            }
        }

        if (!entry) {
            LOG_I("[HAL:Discovery]", "EXP EEPROM at 0x%02X: no matching driver — manual config required",
                  addr);
            appState.markHalDeviceDirty();
            continue;
        }

        // Look up device descriptor from database
        HalDeviceDescriptor desc;
        if (!hal_db_lookup(entry->compatible, &desc)) {
            LOG_W("[HAL:Discovery]", "EXP EEPROM: driver '%s' not in device DB", entry->compatible);
            continue;
        }

        // Check if already registered (don't duplicate)
        bool alreadyRegistered = false;
        for (uint8_t i = 0; i < HAL_MAX_DEVICES; i++) {
            HalDevice* existing = mgr.getDevice(i);
            if (existing && existing->getDiscovery() == HAL_DISC_EEPROM &&
                strcmp(existing->getDescriptor().compatible, desc.compatible) == 0) {
                alreadyRegistered = true;
                break;
            }
        }
        if (alreadyRegistered) {
            LOG_I("[HAL:Discovery]", "EXP device already registered: %s", desc.compatible);
            continue;
        }

        // Create and register device
        HalDevice* dev = entry->factory ? entry->factory() : nullptr;
        if (!dev) {
            LOG_W("[HAL:Discovery]", "EXP factory returned null for %s", entry->compatible);
            continue;
        }

        int slot = mgr.registerDevice(dev, HAL_DISC_EEPROM);
        if (slot < 0) {
            LOG_W("[HAL:Discovery]", "EXP registration failed (slots full): %s", desc.name);
            delete dev;
        } else if (appState.halAutoDiscovery) {
            dev->_state = HAL_STATE_AVAILABLE;
            LOG_I("[HAL:Discovery]", "EXP device auto-registered: %s (slot %d)", desc.name, slot);
            newDevices++;
        } else {
            dev->_state = HAL_STATE_CONFIGURING;
            LOG_I("[HAL:Discovery]", "EXP device registered, awaiting init: %s (slot %d)", desc.name, slot);
            newDevices++;
        }
    }

    HalI2cBus::get(HAL_I2C_BUS_EXP).end();
    return newDevices;
}
#endif // NATIVE_TEST

int hal_discover_devices() {
    int newDevices = 0;

    // Reset unmatched address table at the start of each new discovery pass
    _unmatchedCount = 0;

#ifndef NATIVE_TEST
    HalDeviceManager& mgr = HalDeviceManager::instance();

    // Cross-bus I2C address dedup bitmap (addresses 0x00-0x7F).
    // Tracks addresses already claimed by a registered device across all buses.
    // An address appearing on two buses would cause I2C conflicts; we warn and skip.
    bool _seenI2cAddr[128] = {};

    // Populate bitmap from devices already registered before this discovery pass
    for (uint8_t di = 0; di < HAL_MAX_DEVICES; di++) {
        HalDevice* existing = mgr.getDevice(di);
        if (existing && existing->getDescriptor().bus.type == HAL_BUS_I2C) {
            uint8_t a = existing->getDescriptor().i2cAddr;
            if (a < 128) _seenI2cAddr[a] = true;
        }
    }

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
    // Iterate ALL EEPROM addresses (0x50-0x57) to support dual mezzanine (ADC + DAC)
    uint8_t eepMask = 0;
    if (!wifiActive) {
        dac_i2c_scan(&eepMask);
        uint8_t remainingMask = eepMask;

        // Loop: scan for each EEPROM device, clearing found addresses from mask
        while (remainingMask) {
            DacEepromData eepromData;
            if (!dac_eeprom_scan(&eepromData, remainingMask)) break;

            // Clear this address from mask so next iteration finds the next device
            uint8_t addrBit = (uint8_t)(1 << (eepromData.i2cAddress - 0x50));
            remainingMask &= ~addrBit;

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

                    // Cross-bus I2C address dedup: warn and skip if this address is
                    // already claimed by a device on another bus to prevent conflicts.
                    if (desc.i2cAddr < 128 && _seenI2cAddr[desc.i2cAddr]) {
                        LOG_W("[HAL:Discovery]", "Duplicate I2C addr 0x%02X — skipping %s",
                              desc.i2cAddr, desc.compatible);
                        alreadyRegistered = true;
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
                                if (desc.i2cAddr < 128) _seenI2cAddr[desc.i2cAddr] = true;
                                LOG_I("[HAL:Discovery]", "Device auto-registered: %s (slot %d)", desc.name, slot);
                                newDevices++;
                            } else {
                                dev->_state = HAL_STATE_CONFIGURING;
                                if (desc.i2cAddr < 128) _seenI2cAddr[desc.i2cAddr] = true;
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
        LOG_I("[HAL:Discovery]", "Skipping Bus 0 EEPROM scan (WiFi active, SDIO conflict)");
        diag_emit(DIAG_HAL_I2C_BUS_CONFLICT, DIAG_SEV_INFO, 0, "EEPROM", "WiFi SDIO active");
    }

    // Phase 3: Expansion EEPROM probe (Bus 2, always safe — not affected by WiFi SDIO)
    int expEepromDevices = hal_eeprom_scan_expansion(mgr);
    if (expEepromDevices > 0) {
        LOG_I("[HAL:Discovery]", "Expansion EEPROM: %d new devices", expEepromDevices);
        newDevices += expEepromDevices;
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

// Perform a 9-clock bus recovery sequence followed by a STOP condition.
// Used after I2C timeout errors to release a stuck SDA line.
// busIndex selects which SDA/SCL pin pair to toggle.
#ifndef NATIVE_TEST
static void hal_i2c_bus_recovery(uint8_t busIndex) {
    int pinSda = -1, pinScl = -1;
    switch (busIndex) {
        case HAL_I2C_BUS_EXT:     pinSda = 48; pinScl = 54; break;
        case HAL_I2C_BUS_ONBOARD: pinSda = 7;  pinScl = 8;  break;
        case HAL_I2C_BUS_EXP:     pinSda = 28; pinScl = 29; break;
        default: return;
    }

    // Configure both pins as GPIO output, SDA high
    pinMode(pinSda, OUTPUT);
    pinMode(pinScl, OUTPUT);
    digitalWrite(pinSda, HIGH);
    digitalWrite(pinScl, HIGH);
    delayMicroseconds(5);

    // Toggle SCL 9 times with SDA held high to force any stuck device to release
    for (int i = 0; i < 9; i++) {
        digitalWrite(pinScl, LOW);
        delayMicroseconds(5);
        digitalWrite(pinScl, HIGH);
        delayMicroseconds(5);
    }

    // Generate a STOP condition: SDA low → high while SCL is high
    digitalWrite(pinSda, LOW);
    delayMicroseconds(5);
    digitalWrite(pinSda, HIGH);
    delayMicroseconds(5);

    LOG_W("[HAL:Discovery]", "Bus %u: I2C recovery sequence applied (9-clock + STOP)", busIndex);
}
#endif // NATIVE_TEST

uint8_t hal_i2c_scan_bus(uint8_t busIndex) {
    uint8_t found = 0;

#ifndef NATIVE_TEST
    if (busIndex > 2) return 0;

    // Initialise bus if it's one we need to bring up for scanning
    bool needsInit = (busIndex == HAL_I2C_BUS_EXT || busIndex == HAL_I2C_BUS_EXP);
    HalI2cBus& bus = HalI2cBus::get(busIndex);

    if (busIndex == HAL_I2C_BUS_EXT) {
        // Bus 0: GPIO 48/54 — SDIO conflict check is done by caller
        if (!bus.begin(48, 54, 100000)) {
            LOG_W("[HAL:Discovery]", "Wire1.begin(48,54) failed — bus EXT unavailable");
            return 0;
        }
    } else if (busIndex == HAL_I2C_BUS_EXP) {
        // Bus 2: GPIO 28/29 — expansion bus
        if (!bus.begin(28, 29, 100000)) {
            LOG_W("[HAL:Discovery]", "Wire2.begin(28,29) failed — bus EXP unavailable");
            return 0;
        }
    }
    // Bus 1: GPIO 7/8 — already initialized by ES8311 driver, no begin() needed

    // Reduce Wire timeout from default ~1s to 200ms per address to limit
    // worst-case blocking time during a full 112-address bus sweep.
    bus.setTimeout(200);

    // Track addresses that return timeout for retry
    uint8_t timeoutAddrs[HAL_PROBE_RETRY_MAX_ADDRS];
    uint8_t timeoutCount = 0;
    memset(timeoutAddrs, 0, sizeof(timeoutAddrs));

    // Addresses that ACK'd (not yet filtered against registered devices)
    uint8_t ackAddrs[112];  // max addresses in 0x08-0x77 range
    uint8_t ackCount = 0;

    // Scan standard I2C address range (0x08-0x77, skip reserved).
    // Yield to the scheduler every 8 addresses so the web server can service
    // incoming HTTP/WebSocket requests during a scan (prevents total starvation).
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        if (((addr - 0x08) & 0x07) == 0) {
            vTaskDelay(1);  // yield every 8 addresses (~1 tick, <1ms)
        }
        uint8_t err = bus.probeGetError(addr);
        if (err == 0) {
            found++;
            if (ackCount < sizeof(ackAddrs)) ackAddrs[ackCount++] = addr;
            LOG_I("[HAL:Discovery]", "Bus %u: device at 0x%02X", busIndex, addr);
        } else if ((err == 4 || err == 5) && timeoutCount < HAL_PROBE_RETRY_MAX_ADDRS) {
            timeoutAddrs[timeoutCount++] = addr;
        }
    }

    // Bus recovery: if any address caused a timeout, the SDA line may be stuck.
    // Toggle SCL 9 times + STOP to release it before retrying.
    if (timeoutCount > 0) {
        hal_i2c_bus_recovery(busIndex);
        // Re-initialise the bus after GPIO toggling
        if (needsInit) {
            if (busIndex == HAL_I2C_BUS_EXT) bus.begin(48, 54, 100000);
            else if (busIndex == HAL_I2C_BUS_EXP) bus.begin(28, 29, 100000);
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
                uint8_t retryErr = bus.probeGetError(timeoutAddrs[i]);
                if (retryErr == 0) {
                    found++;
                    if (ackCount < sizeof(ackAddrs)) ackAddrs[ackCount++] = timeoutAddrs[i];
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
        bus.end();
    }

    // Record unmatched addresses: ACK'd but not claimed by any registered HAL device
    HalDeviceManager& mgr = HalDeviceManager::instance();
    for (uint8_t ai = 0; ai < ackCount; ai++) {
        uint8_t ackAddr = ackAddrs[ai];
        bool claimed = false;
        for (uint8_t di = 0; di < HAL_MAX_DEVICES; di++) {
            HalDevice* dev = mgr.getDevice(di);
            if (dev && dev->getDescriptor().i2cAddr == ackAddr &&
                (dev->getDescriptor().bus.type == HAL_BUS_I2C) &&
                dev->getDescriptor().bus.index == busIndex) {
                claimed = true;
                break;
            }
        }
        if (!claimed && _unmatchedCount < HAL_UNMATCHED_MAX) {
            // Avoid adding duplicates from multiple scans
            bool alreadyStored = false;
            for (int ui = 0; ui < _unmatchedCount; ui++) {
                if (_unmatchedAddrs[ui].addr == ackAddr &&
                    _unmatchedAddrs[ui].bus  == busIndex) {
                    alreadyStored = true;
                    break;
                }
            }
            if (!alreadyStored) {
                _unmatchedAddrs[_unmatchedCount].addr = ackAddr;
                _unmatchedAddrs[_unmatchedCount].bus  = busIndex;
                _unmatchedCount++;
                LOG_I("[HAL:Discovery]", "Bus %u: unmatched addr 0x%02X (no driver)",
                      busIndex, ackAddr);
            }
        }
    }
#else
    (void)busIndex;
#endif

    return found;
}

#endif // DAC_ENABLED
