#ifdef DAC_ENABLED

#include "dac_api.h"
#include "dac_hal.h"
#include "dac_registry.h"
#include "dac_eeprom.h"
#include "app_state.h"
#include "auth_handler.h"
#include "debug_serial.h"
#include <ArduinoJson.h>
#include <WebServer.h>

extern WebServer server;
extern bool requireAuth();

void registerDacApiEndpoints() {
    // GET /api/dac — Full DAC state + capabilities
    server.on("/api/dac", HTTP_GET, []() {
        if (!requireAuth()) return;

        JsonDocument doc;
        doc["success"] = true;
        doc["enabled"] = appState.dacEnabled;
        doc["volume"] = appState.dacVolume;
        doc["mute"] = appState.dacMute;
        doc["deviceId"] = appState.dacDeviceId;
        doc["modelName"] = appState.dacModelName;
        doc["outputChannels"] = appState.dacOutputChannels;
        doc["detected"] = appState.dacDetected;
        doc["ready"] = appState.dacReady;
        doc["filterMode"] = appState.dacFilterMode;
        doc["txUnderruns"] = appState.dacTxUnderruns;

        // Capabilities from current driver
        DacDriver* drv = dac_get_driver();
        if (drv) {
            const DacCapabilities& caps = drv->getCapabilities();
            JsonObject capsObj = doc["capabilities"].to<JsonObject>();
            capsObj["name"] = caps.name;
            capsObj["manufacturer"] = caps.manufacturer;
            capsObj["maxChannels"] = caps.maxChannels;
            capsObj["hasHardwareVolume"] = caps.hasHardwareVolume;
            capsObj["hasI2cControl"] = caps.hasI2cControl;
            capsObj["needsIndependentClock"] = caps.needsIndependentClock;
            capsObj["hasFilterModes"] = caps.hasFilterModes;
            capsObj["numFilterModes"] = caps.numFilterModes;
            if (caps.hasFilterModes) {
                JsonArray filters = capsObj["filterModes"].to<JsonArray>();
                for (uint8_t f = 0; f < caps.numFilterModes; f++) {
                    const char* name = drv->getFilterModeName(f);
                    filters.add(name ? name : "Unknown");
                }
            }
            JsonArray rates = capsObj["supportedRates"].to<JsonArray>();
            for (uint8_t i = 0; i < caps.numSupportedRates; i++) {
                rates.add(caps.supportedRates[i]);
            }
        }

        String json;
        serializeJson(doc, json);
        server.send(200, "application/json", json);
    });

    // POST /api/dac — Update DAC settings
    server.on("/api/dac", HTTP_POST, []() {
        if (!requireAuth()) return;

        if (!server.hasArg("plain")) {
            server.send(400, "application/json",
                        "{\"success\":false,\"message\":\"No data\"}");
            return;
        }

        JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain"))) {
            server.send(400, "application/json",
                        "{\"success\":false,\"message\":\"Invalid JSON\"}");
            return;
        }

        bool changed = false;

        if (doc["enabled"].is<bool>()) {
            bool en = doc["enabled"].as<bool>();
            if (en != appState.dacEnabled) {
                LOG_I("[DAC] API: enabled %s -> %s", appState.dacEnabled ? "ON" : "OFF", en ? "ON" : "OFF");
                appState.dacEnabled = en;
                if (en && !appState.dacReady) {
                    dac_output_init();
                } else if (!en) {
                    dac_output_deinit();
                }
                changed = true;
            }
        }

        if (doc["volume"].is<int>()) {
            int v = doc["volume"].as<int>();
            if (v >= 0 && v <= 100) {
                appState.dacVolume = (uint8_t)v;
                dac_update_volume(appState.dacVolume);
                changed = true;
            }
        }

        if (doc["mute"].is<bool>()) {
            bool prev = appState.dacMute;
            appState.dacMute = doc["mute"].as<bool>();
            DacDriver* drv = dac_get_driver();
            if (drv) drv->setMute(appState.dacMute);
            if (prev != appState.dacMute) {
                LOG_I("[DAC] API: mute %s -> %s", prev ? "ON" : "OFF", appState.dacMute ? "ON" : "OFF");
            }
            changed = true;
        }

        if (doc["deviceId"].is<int>()) {
            uint16_t id = (uint16_t)doc["deviceId"].as<int>();
            if (id != appState.dacDeviceId) {
                if (dac_select_driver(id)) {
                    changed = true;
                } else {
                    server.send(400, "application/json",
                                "{\"success\":false,\"message\":\"Unknown device ID\"}");
                    return;
                }
            }
        }

        if (doc["filterMode"].is<int>()) {
            appState.dacFilterMode = (uint8_t)doc["filterMode"].as<int>();
            DacDriver* drv = dac_get_driver();
            if (drv) drv->setFilterMode(appState.dacFilterMode);
            changed = true;
        }

        if (changed) {
            dac_save_settings();
            appState.markDacDirty();
        }

        server.send(200, "application/json", "{\"success\":true}");
    });

    // GET /api/dac/drivers — List all registered drivers
    server.on("/api/dac/drivers", HTTP_GET, []() {
        if (!requireAuth()) return;

        JsonDocument doc;
        doc["success"] = true;
        JsonArray drivers = doc["drivers"].to<JsonArray>();

        const DacRegistryEntry* entries = dac_registry_get_entries();
        int count = dac_registry_get_count();

        for (int i = 0; i < count; i++) {
            JsonObject drv = drivers.add<JsonObject>();
            drv["id"] = entries[i].deviceId;
            drv["name"] = entries[i].name;

            // Create a temporary driver to get capabilities
            DacDriver* tmpDrv = entries[i].factory();
            if (tmpDrv) {
                const DacCapabilities& caps = tmpDrv->getCapabilities();
                drv["manufacturer"] = caps.manufacturer;
                drv["maxChannels"] = caps.maxChannels;
                drv["hasHardwareVolume"] = caps.hasHardwareVolume;
                drv["hasI2cControl"] = caps.hasI2cControl;
                drv["needsIndependentClock"] = caps.needsIndependentClock;
                drv["hasFilterModes"] = caps.hasFilterModes;
                delete tmpDrv;
            }
        }

        String json;
        serializeJson(doc, json);
        server.send(200, "application/json", json);
    });

    // ===== EEPROM Endpoints =====

    // GET /api/dac/eeprom — Read EEPROM state, parsed fields, raw hex dump
    server.on("/api/dac/eeprom", HTTP_GET, []() {
        if (!requireAuth()) return;

        JsonDocument doc;
        doc["success"] = true;
        const AppState::EepromDiag& ed = appState.eepromDiag;
        doc["scanned"] = ed.scanned;
        doc["found"] = ed.found;
        doc["eepromAddr"] = ed.eepromAddr;
        doc["i2cDevicesMask"] = ed.i2cDevicesMask;
        doc["i2cTotalDevices"] = ed.i2cTotalDevices;
        doc["readErrors"] = ed.readErrors;
        doc["writeErrors"] = ed.writeErrors;
        doc["lastScanMs"] = ed.lastScanMs;

        if (ed.found) {
            JsonObject parsed = doc["parsed"].to<JsonObject>();
            parsed["deviceId"] = ed.deviceId;
            parsed["hwRevision"] = ed.hwRevision;
            parsed["deviceName"] = ed.deviceName;
            parsed["manufacturer"] = ed.manufacturer;
            parsed["maxChannels"] = ed.maxChannels;
            parsed["dacI2cAddress"] = ed.dacI2cAddress;
            parsed["flags"] = ed.flags;
            parsed["independentClock"] = (bool)(ed.flags & DAC_FLAG_INDEPENDENT_CLOCK);
            parsed["hwVolume"] = (bool)(ed.flags & DAC_FLAG_HW_VOLUME);
            parsed["filters"] = (bool)(ed.flags & DAC_FLAG_FILTERS);
            JsonArray rates = parsed["sampleRates"].to<JsonArray>();
            for (int i = 0; i < ed.numSampleRates; i++) {
                rates.add(ed.sampleRates[i]);
            }
        }

#ifndef NATIVE_TEST
        // Raw hex dump (256 bytes)
        if (ed.found && ed.eepromAddr >= DAC_EEPROM_ADDR_START) {
            uint8_t raw[DAC_EEPROM_TOTAL_SIZE];
            if (dac_eeprom_read_raw(ed.eepromAddr, 0, raw, DAC_EEPROM_TOTAL_SIZE)) {
                String hexStr;
                hexStr.reserve(DAC_EEPROM_TOTAL_SIZE * 2);
                for (int i = 0; i < DAC_EEPROM_TOTAL_SIZE; i++) {
                    char hex[3];
                    snprintf(hex, sizeof(hex), "%02X", raw[i]);
                    hexStr += hex;
                }
                doc["rawHex"] = hexStr;
            } else {
                doc["rawHex"] = (const char*)nullptr;
                appState.eepromDiag.readErrors++;
            }
        }
#endif

        String json;
        serializeJson(doc, json);
        server.send(200, "application/json", json);
    });

    // POST /api/dac/eeprom — Program EEPROM
    server.on("/api/dac/eeprom", HTTP_POST, []() {
        if (!requireAuth()) return;

        if (!server.hasArg("plain")) {
            server.send(400, "application/json",
                        "{\"success\":false,\"message\":\"No data\"}");
            return;
        }

        JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain"))) {
            server.send(400, "application/json",
                        "{\"success\":false,\"message\":\"Invalid JSON\"}");
            return;
        }

        // Build DacEepromData from JSON
        DacEepromData eepData;
        memset(&eepData, 0, sizeof(eepData));

        eepData.deviceId = (uint16_t)doc["deviceId"].as<int>();
        eepData.hwRevision = (uint8_t)doc["hwRevision"].as<int>();
        eepData.maxChannels = (uint8_t)doc["maxChannels"].as<int>();
        eepData.dacI2cAddress = (uint8_t)doc["dacI2cAddress"].as<int>();

        const char* name = doc["deviceName"] | "";
        strncpy(eepData.deviceName, name, 32);
        eepData.deviceName[32] = '\0';

        const char* mfr = doc["manufacturer"] | "";
        strncpy(eepData.manufacturer, mfr, 32);
        eepData.manufacturer[32] = '\0';

        // Flags
        uint8_t flags = 0;
        JsonObject flagsObj = doc["flags"].as<JsonObject>();
        if (flagsObj) {
            if (flagsObj["independentClock"].as<bool>()) flags |= DAC_FLAG_INDEPENDENT_CLOCK;
            if (flagsObj["hwVolume"].as<bool>()) flags |= DAC_FLAG_HW_VOLUME;
            if (flagsObj["filters"].as<bool>()) flags |= DAC_FLAG_FILTERS;
        }
        eepData.flags = flags;

        // Sample rates
        JsonArray ratesArr = doc["sampleRates"].as<JsonArray>();
        if (ratesArr) {
            int count = 0;
            for (JsonVariant v : ratesArr) {
                if (count >= DAC_EEPROM_MAX_RATES) break;
                eepData.sampleRates[count++] = v.as<uint32_t>();
            }
            eepData.numSampleRates = count;
        }

        // Target I2C address
        uint8_t targetAddr = (uint8_t)doc["address"].as<int>();
        if (targetAddr < DAC_EEPROM_ADDR_START || targetAddr > DAC_EEPROM_ADDR_END) {
            targetAddr = DAC_EEPROM_ADDR_START; // Default to 0x50
        }

        LOG_I("[DAC] API: Program EEPROM at 0x%02X — %s by %s (ID=0x%04X)",
              targetAddr, eepData.deviceName, eepData.manufacturer, eepData.deviceId);

#ifndef NATIVE_TEST
        // Serialize
        uint8_t buf[DAC_EEPROM_DATA_SIZE];
        int serialized = dac_eeprom_serialize(&eepData, buf, sizeof(buf));
        if (serialized == 0) {
            server.send(500, "application/json",
                        "{\"success\":false,\"message\":\"Serialize failed\"}");
            return;
        }

        // Write + verify
        if (!dac_eeprom_write(targetAddr, buf, serialized)) {
            appState.eepromDiag.writeErrors++;
            appState.markEepromDirty();
            server.send(500, "application/json",
                        "{\"success\":false,\"message\":\"Write/verify failed\"}");
            return;
        }

        // Re-scan to update diagnostics (use cached mask from prior scan)
        DacEepromData scanned;
        AppState::EepromDiag& ed = appState.eepromDiag;
        if (dac_eeprom_scan(&scanned, ed.i2cDevicesMask)) {
            ed.found = true;
            ed.eepromAddr = scanned.i2cAddress;
            ed.deviceId = scanned.deviceId;
            ed.hwRevision = scanned.hwRevision;
            strncpy(ed.deviceName, scanned.deviceName, 32);
            ed.deviceName[32] = '\0';
            strncpy(ed.manufacturer, scanned.manufacturer, 32);
            ed.manufacturer[32] = '\0';
            ed.maxChannels = scanned.maxChannels;
            ed.dacI2cAddress = scanned.dacI2cAddress;
            ed.flags = scanned.flags;
            ed.numSampleRates = scanned.numSampleRates;
            for (int i = 0; i < scanned.numSampleRates && i < 4; i++) {
                ed.sampleRates[i] = scanned.sampleRates[i];
            }
        }
        ed.lastScanMs = millis();
        appState.markEepromDirty();
#endif

        server.send(200, "application/json", "{\"success\":true}");
    });

    // POST /api/dac/eeprom/erase — Erase EEPROM
    server.on("/api/dac/eeprom/erase", HTTP_POST, []() {
        if (!requireAuth()) return;

        // Get target address from body or use stored address
        uint8_t targetAddr = appState.eepromDiag.eepromAddr;
        if (server.hasArg("plain")) {
            JsonDocument doc;
            if (!deserializeJson(doc, server.arg("plain"))) {
                if (doc["address"].is<int>()) {
                    targetAddr = (uint8_t)doc["address"].as<int>();
                }
            }
        }
        if (targetAddr < DAC_EEPROM_ADDR_START || targetAddr > DAC_EEPROM_ADDR_END) {
            targetAddr = DAC_EEPROM_ADDR_START;
        }

        LOG_I("[DAC] API: Erase EEPROM at 0x%02X", targetAddr);

#ifndef NATIVE_TEST
        if (!dac_eeprom_erase(targetAddr)) {
            appState.eepromDiag.writeErrors++;
            appState.markEepromDirty();
            server.send(500, "application/json",
                        "{\"success\":false,\"message\":\"Erase failed\"}");
            return;
        }

        // Update diagnostics
        AppState::EepromDiag& ed = appState.eepromDiag;
        ed.found = false;
        ed.eepromAddr = 0;
        memset(ed.deviceName, 0, sizeof(ed.deviceName));
        memset(ed.manufacturer, 0, sizeof(ed.manufacturer));
        ed.deviceId = 0;
        ed.hwRevision = 0;
        ed.maxChannels = 0;
        ed.dacI2cAddress = 0;
        ed.flags = 0;
        ed.numSampleRates = 0;
        memset(ed.sampleRates, 0, sizeof(ed.sampleRates));
        ed.lastScanMs = millis();
        appState.markEepromDirty();
#endif

        server.send(200, "application/json", "{\"success\":true}");
    });

    // POST /api/dac/eeprom/scan — Re-scan I2C bus + EEPROM
    server.on("/api/dac/eeprom/scan", HTTP_POST, []() {
        if (!requireAuth()) return;

        LOG_I("[DAC] API: Re-scan I2C bus + EEPROM");

#ifndef NATIVE_TEST
        AppState::EepromDiag& ed = appState.eepromDiag;
        uint8_t eepMask = 0;
        ed.i2cTotalDevices = dac_i2c_scan(&eepMask);
        ed.i2cDevicesMask = eepMask;
        ed.scanned = true;
        ed.lastScanMs = millis();

        DacEepromData eepData;
        if (dac_eeprom_scan(&eepData, eepMask)) {
            ed.found = true;
            ed.eepromAddr = eepData.i2cAddress;
            ed.deviceId = eepData.deviceId;
            ed.hwRevision = eepData.hwRevision;
            strncpy(ed.deviceName, eepData.deviceName, 32);
            ed.deviceName[32] = '\0';
            strncpy(ed.manufacturer, eepData.manufacturer, 32);
            ed.manufacturer[32] = '\0';
            ed.maxChannels = eepData.maxChannels;
            ed.dacI2cAddress = eepData.dacI2cAddress;
            ed.flags = eepData.flags;
            ed.numSampleRates = eepData.numSampleRates;
            for (int i = 0; i < eepData.numSampleRates && i < 4; i++) {
                ed.sampleRates[i] = eepData.sampleRates[i];
            }
        } else {
            ed.found = false;
            ed.eepromAddr = 0;
            memset(ed.deviceName, 0, sizeof(ed.deviceName));
            memset(ed.manufacturer, 0, sizeof(ed.manufacturer));
            ed.deviceId = 0;
        }
        appState.markEepromDirty();
#endif

        // Return current state
        JsonDocument doc;
        doc["success"] = true;
        doc["scanned"] = appState.eepromDiag.scanned;
        doc["found"] = appState.eepromDiag.found;
        doc["eepromAddr"] = appState.eepromDiag.eepromAddr;
        doc["i2cTotalDevices"] = appState.eepromDiag.i2cTotalDevices;
        doc["i2cDevicesMask"] = appState.eepromDiag.i2cDevicesMask;
        if (appState.eepromDiag.found) {
            doc["deviceName"] = appState.eepromDiag.deviceName;
            doc["manufacturer"] = appState.eepromDiag.manufacturer;
            doc["deviceId"] = appState.eepromDiag.deviceId;
        }
        String json;
        serializeJson(doc, json);
        server.send(200, "application/json", json);
    });

    // GET /api/dac/eeprom/presets — Pre-fill data from driver registry
    server.on("/api/dac/eeprom/presets", HTTP_GET, []() {
        if (!requireAuth()) return;

        JsonDocument doc;
        doc["success"] = true;
        JsonArray presets = doc["presets"].to<JsonArray>();

        const DacRegistryEntry* entries = dac_registry_get_entries();
        int count = dac_registry_get_count();

        for (int i = 0; i < count; i++) {
            DacDriver* tmpDrv = entries[i].factory();
            if (!tmpDrv) continue;

            const DacCapabilities& caps = tmpDrv->getCapabilities();
            JsonObject preset = presets.add<JsonObject>();
            preset["deviceId"] = entries[i].deviceId;
            preset["deviceName"] = caps.name;
            preset["manufacturer"] = caps.manufacturer;
            preset["maxChannels"] = caps.maxChannels;
            preset["dacI2cAddress"] = caps.i2cAddress;
            uint8_t flags = 0;
            if (caps.needsIndependentClock) flags |= DAC_FLAG_INDEPENDENT_CLOCK;
            if (caps.hasHardwareVolume) flags |= DAC_FLAG_HW_VOLUME;
            if (caps.hasFilterModes) flags |= DAC_FLAG_FILTERS;
            preset["flags"] = flags;
            JsonArray rates = preset["sampleRates"].to<JsonArray>();
            for (uint8_t r = 0; r < caps.numSupportedRates; r++) {
                rates.add(caps.supportedRates[r]);
            }

            delete tmpDrv;
        }

        String json;
        serializeJson(doc, json);
        server.send(200, "application/json", json);
    });

    LOG_I("[DAC] REST API endpoints registered");
}

#endif // DAC_ENABLED
