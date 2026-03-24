#ifdef DAC_ENABLED

#include "hal_eeprom_api.h"
#include "../dac_eeprom.h"
#include "../app_state.h"
#include "../globals.h"
#include "../auth_handler.h"
#include "../debug_serial.h"
#include "hal_device_manager.h"
#include "hal_types.h"
#include "../http_security.h"
#include <ArduinoJson.h>
extern bool requireAuth();

void registerHalEepromApiEndpoints() {
    // GET /api/hal/eeprom — Read EEPROM state, parsed fields, raw hex dump
    server.on("/api/hal/eeprom", HTTP_GET, []() {
        if (!requireAuth()) return;

        JsonDocument doc;
        doc["success"] = true;
        const EepromDiag& ed = appState.dac.eepromDiag;
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
                appState.dac.eepromDiag.readErrors++;
            }
        }
#endif

        String json;
        serializeJson(doc, json);
        server_send(200, "application/json", json);
    });

    // POST /api/hal/eeprom — Program EEPROM
    server.on("/api/hal/eeprom", HTTP_POST, []() {
        if (!requireAuth()) return;

        if (!server.hasArg("plain")) {
            server_send(400, "application/json",
                        "{\"success\":false,\"message\":\"No data\"}");
            return;
        }

        JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain"))) {
            server_send(400, "application/json",
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
        hal_safe_strcpy(eepData.deviceName, sizeof(eepData.deviceName), name);

        const char* mfr = doc["manufacturer"] | "";
        hal_safe_strcpy(eepData.manufacturer, sizeof(eepData.manufacturer), mfr);

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

        LOG_I("[HAL EEPROM] API: Program EEPROM at 0x%02X — %s by %s (ID=0x%04X)",
              targetAddr, eepData.deviceName, eepData.manufacturer, eepData.deviceId);

#ifndef NATIVE_TEST
        // Serialize
        uint8_t buf[DAC_EEPROM_DATA_SIZE];
        int serialized = dac_eeprom_serialize(&eepData, buf, sizeof(buf));
        if (serialized == 0) {
            server_send(500, "application/json",
                        "{\"success\":false,\"message\":\"Serialize failed\"}");
            return;
        }

        // Write + verify
        if (!dac_eeprom_write(targetAddr, buf, serialized)) {
            appState.dac.eepromDiag.writeErrors++;
            appState.markEepromDirty();
            server_send(500, "application/json",
                        "{\"success\":false,\"message\":\"Write/verify failed\"}");
            return;
        }

        // Re-scan to update diagnostics (use cached mask from prior scan)
        DacEepromData scanned;
        EepromDiag& ed = appState.dac.eepromDiag;
        if (dac_eeprom_scan(&scanned, ed.i2cDevicesMask)) {
            ed.found = true;
            ed.eepromAddr = scanned.i2cAddress;
            ed.deviceId = scanned.deviceId;
            ed.hwRevision = scanned.hwRevision;
            hal_safe_strcpy(ed.deviceName, sizeof(ed.deviceName), scanned.deviceName);
            hal_safe_strcpy(ed.manufacturer, sizeof(ed.manufacturer), scanned.manufacturer);
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

        server_send(200, "application/json", "{\"success\":true}");
    });

    // POST /api/hal/eeprom/erase — Erase EEPROM
    server.on("/api/hal/eeprom/erase", HTTP_POST, []() {
        if (!requireAuth()) return;

        // Get target address from body or use stored address
        uint8_t targetAddr = appState.dac.eepromDiag.eepromAddr;
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

        LOG_I("[HAL EEPROM] API: Erase EEPROM at 0x%02X", targetAddr);

#ifndef NATIVE_TEST
        if (!dac_eeprom_erase(targetAddr)) {
            appState.dac.eepromDiag.writeErrors++;
            appState.markEepromDirty();
            server_send(500, "application/json",
                        "{\"success\":false,\"message\":\"Erase failed\"}");
            return;
        }

        // Update diagnostics
        EepromDiag& ed = appState.dac.eepromDiag;
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

        server_send(200, "application/json", "{\"success\":true}");
    });

    // POST /api/hal/eeprom/scan — Re-scan I2C bus + EEPROM
    server.on("/api/hal/eeprom/scan", HTTP_POST, []() {
        if (!requireAuth()) return;

        LOG_I("[HAL EEPROM] API: Re-scan I2C bus + EEPROM");

#ifndef NATIVE_TEST
        EepromDiag& ed = appState.dac.eepromDiag;
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
            hal_safe_strcpy(ed.deviceName, sizeof(ed.deviceName), eepData.deviceName);
            hal_safe_strcpy(ed.manufacturer, sizeof(ed.manufacturer), eepData.manufacturer);
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
        doc["scanned"] = appState.dac.eepromDiag.scanned;
        doc["found"] = appState.dac.eepromDiag.found;
        doc["eepromAddr"] = appState.dac.eepromDiag.eepromAddr;
        doc["i2cTotalDevices"] = appState.dac.eepromDiag.i2cTotalDevices;
        doc["i2cDevicesMask"] = appState.dac.eepromDiag.i2cDevicesMask;
        if (appState.dac.eepromDiag.found) {
            doc["deviceName"] = appState.dac.eepromDiag.deviceName;
            doc["manufacturer"] = appState.dac.eepromDiag.manufacturer;
            doc["deviceId"] = appState.dac.eepromDiag.deviceId;
        }
        String json;
        serializeJson(doc, json);
        server_send(200, "application/json", json);
    });

    // GET /api/hal/eeprom/presets — Pre-fill data from HAL device DB
    server.on("/api/hal/eeprom/presets", HTTP_GET, []() {
        if (!requireAuth()) return;

        JsonDocument doc;
        doc["success"] = true;
        JsonArray presets = doc["presets"].to<JsonArray>();

        // Enumerate all DAC-path devices in HAL
        HalDeviceManager::instance().forEach([](HalDevice* dev, void* ctx) {
            JsonArray* a = static_cast<JsonArray*>(ctx);
            const HalDeviceDescriptor& desc = dev->getDescriptor();
            if (desc.capabilities & HAL_CAP_DAC_PATH) {
                JsonObject preset = a->add<JsonObject>();
                preset["deviceId"] = desc.legacyId;
                preset["deviceName"] = desc.name;
                preset["manufacturer"] = desc.manufacturer;
                preset["maxChannels"] = desc.channelCount;
                preset["dacI2cAddress"] = desc.i2cAddr;
                uint8_t flags = 0;
                if (desc.i2cAddr != 0) flags |= DAC_FLAG_HW_VOLUME;
                preset["flags"] = flags;
                // Sample rates from descriptor mask (simplified)
                JsonArray rates = preset["sampleRates"].to<JsonArray>();
                if (desc.sampleRatesMask & 0x01) rates.add(8000);
                if (desc.sampleRatesMask & 0x02) rates.add(16000);
                if (desc.sampleRatesMask & 0x04) rates.add(32000);
                if (desc.sampleRatesMask & 0x08) rates.add(44100);
                if (desc.sampleRatesMask & 0x10) rates.add(48000);
                if (desc.sampleRatesMask & 0x20) rates.add(88200);
                if (desc.sampleRatesMask & 0x40) rates.add(96000);
                if (desc.sampleRatesMask & 0x80) rates.add(192000);
            }
        }, (void*)&presets);

        String json;
        serializeJson(doc, json);
        server_send(200, "application/json", json);
    });

    LOG_I("[HAL EEPROM] REST API endpoints registered");
}

#endif // DAC_ENABLED
