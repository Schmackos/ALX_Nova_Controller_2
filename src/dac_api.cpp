#ifdef DAC_ENABLED

#include "dac_api.h"
#include "dac_hal.h"
#include "dac_eeprom.h"
#include "audio_pipeline.h"
#include "app_state.h"
#include "globals.h"
#include "auth_handler.h"
#include "debug_serial.h"
#include "hal/hal_device_manager.h"
#include "hal/hal_pipeline_bridge.h"
#include "hal/hal_audio_device.h"
#include "hal/hal_settings.h"
#include <ArduinoJson.h>
extern bool requireAuth();

// Get HalAudioDevice* for a given pipeline sink slot (nullptr if not found)
static HalAudioDevice* _dacApiAudioDeviceForSlot(uint8_t sinkSlot) {
    int8_t halSlot = hal_pipeline_get_slot_for_sink(sinkSlot);
    if (halSlot < 0) return nullptr;
    HalDevice* dev = HalDeviceManager::instance().getDevice((uint8_t)halSlot);
    if (!dev) return nullptr;
    if (dev->getType() != HAL_DEV_DAC && dev->getType() != HAL_DEV_CODEC) return nullptr;
    return static_cast<HalAudioDevice*>(dev);
}

void registerDacApiEndpoints() {
    // GET /api/dac — Full DAC state + capabilities (queries HAL)
    server.on("/api/dac", HTTP_GET, []() {
        if (!requireAuth()) return;

        JsonDocument doc;
        doc["success"] = true;

        HalDeviceManager& mgr = HalDeviceManager::instance();
        HalDevice* dev = mgr.findByCompatible("ti,pcm5102a");
        HalDeviceConfig* cfg = dev ? mgr.getConfig(dev->getSlot()) : nullptr;

        doc["enabled"] = cfg ? cfg->enabled : false;
        doc["volume"] = cfg ? cfg->volume : 80;
        doc["mute"] = cfg ? cfg->mute : false;
        doc["deviceId"] = dev ? dev->getDescriptor().legacyId : 0x0001;
        doc["modelName"] = dev ? dev->getDescriptor().name : "PCM5102A";
        doc["outputChannels"] = dev ? dev->getDescriptor().channelCount : 2;
        doc["detected"] = (dev != nullptr);
        doc["ready"] = dev ? dev->_ready : false;
        doc["filterMode"] = cfg ? cfg->filterMode : 0;
        doc["txUnderruns"] = appState.dac.txUnderruns;

        // Capabilities from HAL device descriptor
        if (dev) {
            const HalDeviceDescriptor& desc = dev->getDescriptor();
            HalAudioDevice* audioDev = _dacApiAudioDeviceForSlot(0);
            JsonObject capsObj = doc["capabilities"].to<JsonObject>();
            capsObj["name"] = desc.name;
            capsObj["manufacturer"] = desc.manufacturer;
            capsObj["maxChannels"] = desc.channelCount;
            capsObj["hasHardwareVolume"] = audioDev ? audioDev->hasHardwareVolume() : false;
            capsObj["hasI2cControl"] = (desc.i2cAddr != 0);
            capsObj["needsIndependentClock"] = false;
            capsObj["hasFilterModes"] = false;
            capsObj["numFilterModes"] = 0;
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

        // HAL device lookup for PCM5102A
        HalDevice* dev = HalDeviceManager::instance().findByCompatible("ti,pcm5102a");
        uint8_t halSlot = dev ? dev->getSlot() : 0xFF;
        HalDeviceConfig* cfg = (halSlot < 0xFF) ? HalDeviceManager::instance().getConfig(halSlot) : nullptr;

        if (doc["enabled"].is<bool>()) {
            bool en = doc["enabled"].as<bool>();
            bool was = cfg ? cfg->enabled : false;
            if (en != was) {
                LOG_I("[DAC] API: enabled %s -> %s (deferred)", was ? "ON" : "OFF", en ? "ON" : "OFF");
                if (cfg) cfg->enabled = en;
                if (halSlot < 0xFF) {
                    if (en && !was) {
                        appState.dac.requestDeviceToggle(halSlot, 1);
                    } else if (!en && was) {
                        appState.dac.requestDeviceToggle(halSlot, -1);
                    }
                } else {
                    LOG_W("[DAC] API: enabled toggle requested but no HAL device found (halSlot=0xFF)");
                }
                changed = true;
            }
        }

        if (doc["volume"].is<int>()) {
            int v = doc["volume"].as<int>();
            if (v >= 0 && v <= 100) {
                if (cfg) cfg->volume = (uint8_t)v;
                int8_t sinkSlot = (halSlot < 0xFF) ? hal_pipeline_get_sink_slot(halSlot) : -1;
                if (sinkSlot >= 0) {
                    audio_pipeline_set_sink_volume((uint8_t)sinkSlot, dac_volume_to_linear((uint8_t)v));
                    HalAudioDevice* audioDev = _dacApiAudioDeviceForSlot((uint8_t)sinkSlot);
                    if (audioDev && audioDev->hasHardwareVolume()) {
                        audioDev->setVolume((uint8_t)v);
                    }
                }
                changed = true;
            }
        }

        if (doc["mute"].is<bool>()) {
            bool newMute = doc["mute"].as<bool>();
            bool prev = cfg ? cfg->mute : false;
            if (cfg) cfg->mute = newMute;
            int8_t sinkSlot = (halSlot < 0xFF) ? hal_pipeline_get_sink_slot(halSlot) : -1;
            if (sinkSlot >= 0) {
                audio_pipeline_set_sink_muted((uint8_t)sinkSlot, newMute);
                HalAudioDevice* audioDev = _dacApiAudioDeviceForSlot((uint8_t)sinkSlot);
                if (audioDev) audioDev->setMute(newMute);
            }
            if (prev != newMute) {
                LOG_I("[DAC] API: mute %s -> %s", prev ? "ON" : "OFF", newMute ? "ON" : "OFF");
            }
            changed = true;
        }

        if (doc["deviceId"].is<int>()) {
            uint16_t id = (uint16_t)doc["deviceId"].as<int>();
            // Runtime DAC model switching is not supported via HAL — device type is fixed at boot
            LOG_W("[DAC] API: deviceId change (0x%04X) ignored — use HAL device config", id);
        }

        if (doc["filterMode"].is<int>()) {
            uint8_t fm = (uint8_t)doc["filterMode"].as<int>();
            if (cfg) cfg->filterMode = fm;
            HalAudioDevice* audioDev = _dacApiAudioDeviceForSlot(0);
            if (audioDev) audioDev->setFilterMode(fm);
            changed = true;
        }

        if (changed) {
            if (halSlot < 0xFF) {
                hal_save_device_config_deferred(halSlot);
            } else {
                LOG_W("[DAC API] No HAL device for POST /api/dac — settings not persisted");
            }
            appState.markDacDirty();
        }

        server.send(200, "application/json", "{\"success\":true}");
    });

    // GET /api/dac/drivers -- List all DAC-path devices from HAL
    server.on("/api/dac/drivers", HTTP_GET, []() {
        if (!requireAuth()) return;

        JsonDocument doc;
        doc["success"] = true;
        JsonArray drivers = doc["drivers"].to<JsonArray>();

        HalDeviceManager::instance().forEach([](HalDevice* dev, void* ctx) {
            JsonArray* a = static_cast<JsonArray*>(ctx);
            const HalDeviceDescriptor& desc = dev->getDescriptor();
            if (desc.capabilities & HAL_CAP_DAC_PATH) {
                JsonObject drv = a->add<JsonObject>();
                drv["id"] = desc.legacyId;
                drv["name"] = desc.name;
                drv["manufacturer"] = desc.manufacturer;
                drv["maxChannels"] = desc.channelCount;
                drv["hasHardwareVolume"] = (desc.i2cAddr != 0);
                drv["hasI2cControl"] = (desc.i2cAddr != 0);
                drv["needsIndependentClock"] = false;
                drv["hasFilterModes"] = false;
            }
        }, (void*)&drivers);

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
            appState.dac.eepromDiag.writeErrors++;
            appState.markEepromDirty();
            server.send(500, "application/json",
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

        LOG_I("[DAC] API: Erase EEPROM at 0x%02X", targetAddr);

#ifndef NATIVE_TEST
        if (!dac_eeprom_erase(targetAddr)) {
            appState.dac.eepromDiag.writeErrors++;
            appState.markEepromDirty();
            server.send(500, "application/json",
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

        server.send(200, "application/json", "{\"success\":true}");
    });

    // POST /api/dac/eeprom/scan — Re-scan I2C bus + EEPROM
    server.on("/api/dac/eeprom/scan", HTTP_POST, []() {
        if (!requireAuth()) return;

        LOG_I("[DAC] API: Re-scan I2C bus + EEPROM");

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
        server.send(200, "application/json", json);
    });

    // GET /api/dac/eeprom/presets -- Pre-fill data from HAL device DB
    server.on("/api/dac/eeprom/presets", HTTP_GET, []() {
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
        server.send(200, "application/json", json);
    });

    LOG_I("[DAC] REST API endpoints registered");
}

#endif // DAC_ENABLED
