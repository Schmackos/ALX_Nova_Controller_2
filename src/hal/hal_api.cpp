#ifdef DAC_ENABLED
#ifndef NATIVE_TEST

#include "hal_api.h"
#include "hal_device_manager.h"
#include "hal_device_db.h"
#include "hal_driver_registry.h"
#include "hal_discovery.h"
#include "hal_settings.h"
#include "hal_custom_device.h"
#include "../debug_serial.h"
#include "../app_state.h"
#include "../settings_manager.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

// ===== Helper: Serialize device to JSON =====
static void deviceToJson(JsonObject& obj, HalDevice* dev) {
    const HalDeviceDescriptor& desc = dev->getDescriptor();
    obj["slot"] = dev->getSlot();
    obj["compatible"] = desc.compatible;
    obj["name"] = desc.name;
    obj["manufacturer"] = desc.manufacturer;
    obj["type"] = desc.type;
    obj["state"] = dev->_state;
    obj["discovery"] = dev->getDiscovery();
    obj["ready"] = (bool)dev->_ready;
    obj["i2cAddr"] = desc.i2cAddr;
    obj["channels"] = desc.channelCount;
    obj["capabilities"] = desc.capabilities;
    obj["legacyId"] = desc.legacyId;
    obj["busType"] = desc.bus.type;
    obj["busIndex"] = desc.bus.index;
    obj["pinA"] = desc.bus.pinA;
    obj["pinB"] = desc.bus.pinB;
    obj["busFreq"] = desc.bus.freqHz;
    obj["sampleRates"] = desc.sampleRatesMask;

    // Per-device runtime config
    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(dev->getSlot());
    if (cfg && cfg->valid) {
        obj["userLabel"] = cfg->userLabel;
        obj["cfgEnabled"] = cfg->enabled;
        obj["cfgI2sPort"] = cfg->i2sPort;
        obj["cfgVolume"] = cfg->volume;
        obj["cfgMute"] = cfg->mute;
        obj["cfgPinSda"] = cfg->pinSda;
        obj["cfgPinScl"] = cfg->pinScl;
        obj["cfgPinData"] = cfg->pinData;
        obj["cfgPinMclk"] = cfg->pinMclk;
        obj["cfgMclkMultiple"] = cfg->mclkMultiple;
        obj["cfgI2sFormat"]    = cfg->i2sFormat;
        obj["cfgPgaGain"]      = cfg->pgaGain;
        obj["cfgHpfEnabled"]   = cfg->hpfEnabled;
        obj["cfgPaControlPin"] = cfg->paControlPin;
        obj["cfgPinBck"]       = cfg->pinBck;
        obj["cfgPinLrc"]       = cfg->pinLrc;
        obj["cfgPinFmt"]       = cfg->pinFmt;
    }
}

void registerHalApiEndpoints(WebServer& server) {
    // GET /api/hal/devices — list all registered devices
    server.on("/api/hal/devices", HTTP_GET, [&server]() {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();

        HalDeviceManager::instance().forEach([](HalDevice* dev, void* ctx) {
            JsonArray* a = static_cast<JsonArray*>(ctx);
            JsonObject obj = a->add<JsonObject>();
            deviceToJson(obj, dev);
        }, &arr);

        String json;
        serializeJson(doc, json);
        server.send(200, "application/json", json);
    });

    // POST /api/hal/scan — trigger device rescan
    server.on("/api/hal/scan", HTTP_POST, [&server]() {
        if (appState._halScanInProgress) {
            server.send(409, "application/json", "{\"error\":\"Scan already in progress\"}");
            return;
        }
        appState._halScanInProgress = true;
        appState.markHalDeviceDirty();  // Broadcast scanning=true
        int found = hal_rescan();
        appState._halScanInProgress = false;
        appState.markHalDeviceDirty();  // Broadcast scanning=false
        JsonDocument doc;
        doc["status"] = "ok";
        doc["devicesFound"] = found;
        String json;
        serializeJson(doc, json);
        server.send(200, "application/json", json);
    });

    // GET /api/hal/db — list device database entries
    server.on("/api/hal/db", HTTP_GET, [&server]() {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();

        for (int i = 0; i < hal_db_count(); i++) {
            const HalDeviceDescriptor* d = hal_db_get(i);
            if (!d) continue;
            JsonObject obj = arr.add<JsonObject>();
            obj["compatible"] = d->compatible;
            obj["name"] = d->name;
            obj["manufacturer"] = d->manufacturer;
            obj["type"] = d->type;
            obj["i2cAddr"] = d->i2cAddr;
            obj["channels"] = d->channelCount;
        }

        String json;
        serializeJson(doc, json);
        server.send(200, "application/json", json);
    });

    // GET /api/hal/db/presets — list available device presets
    server.on("/api/hal/db/presets", HTTP_GET, [&server]() {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();

        for (int i = 0; i < hal_db_count(); i++) {
            const HalDeviceDescriptor* d = hal_db_get(i);
            if (!d) continue;
            JsonObject obj = arr.add<JsonObject>();
            obj["compatible"] = d->compatible;
            obj["name"] = d->name;
            obj["type"] = d->type;
        }

        String json;
        serializeJson(doc, json);
        server.send(200, "application/json", json);
    });

    // POST /api/hal/devices — manually register a device by compatible string
    server.on("/api/hal/devices", HTTP_POST, [&server]() {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, server.arg("plain"));
        if (err) { server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }

        const char* compatible = doc["compatible"] | "";
        if (!compatible[0]) { server.send(400, "application/json", "{\"error\":\"Missing compatible\"}"); return; }

        HalDeviceDescriptor desc;
        if (!hal_db_lookup(compatible, &desc)) {
            server.send(404, "application/json", "{\"error\":\"Unknown device\"}"); return;
        }

        const HalDriverEntry* entry = hal_registry_find(compatible);
        if (!entry || !entry->factory) {
            server.send(422, "application/json", "{\"error\":\"No driver for this device\"}"); return;
        }

        HalDeviceManager& mgr = HalDeviceManager::instance();
        if (mgr.findByCompatible(compatible)) {
            server.send(409, "application/json", "{\"error\":\"Device already registered\"}"); return;
        }
        if (mgr.getCount() >= HAL_MAX_DEVICES) {
            server.send(409, "application/json", "{\"error\":\"No free slots\"}"); return;
        }

        HalDevice* dev = entry->factory();
        if (!dev) { server.send(500, "application/json", "{\"error\":\"Factory failed\"}"); return; }

        int slot = mgr.registerDevice(dev, HAL_DISC_MANUAL);
        if (slot < 0) { delete dev; server.send(500, "application/json", "{\"error\":\"No free slots\"}"); return; }

        dev->_state = HAL_STATE_CONFIGURING;
        bool ok = dev->probe() && dev->init();
        dev->_state = ok ? HAL_STATE_AVAILABLE : HAL_STATE_ERROR;
        dev->_ready = ok;

        hal_save_device_config(static_cast<uint8_t>(slot));
        appState.markHalDeviceDirty();
        LOG_I("[HAL:API]", "Manual register: %s slot %d (%s)", compatible, slot, ok ? "ok" : "init failed");

        JsonDocument resp;
        resp["status"] = "ok";
        resp["slot"] = slot;
        resp["name"] = desc.name;
        resp["state"] = static_cast<uint8_t>(dev->_state);
        String json; serializeJson(resp, json);
        server.send(201, "application/json", json);
    });

    // PUT /api/hal/devices — update device config
    server.on("/api/hal/devices", HTTP_PUT, [&server]() {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, server.arg("plain"));
        if (err) { server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }

        uint8_t slot = doc["slot"] | 255;
        if (slot >= HAL_MAX_DEVICES) { server.send(400, "application/json", "{\"error\":\"Invalid slot\"}"); return; }

        HalDeviceManager& mgr = HalDeviceManager::instance();
        HalDevice* dev = mgr.getDevice(slot);
        if (!dev) { server.send(404, "application/json", "{\"error\":\"No device in slot\"}"); return; }

        // Get or create config
        HalDeviceConfig* cfg = mgr.getConfig(slot);
        HalDeviceConfig newCfg;
        if (cfg && cfg->valid) {
            newCfg = *cfg;
        } else {
            memset(&newCfg, 0, sizeof(newCfg));
            newCfg.pinSda = -1; newCfg.pinScl = -1; newCfg.pinMclk = -1; newCfg.pinData = -1;
            newCfg.pinBck = -1; newCfg.pinLrc = -1; newCfg.pinFmt = -1;
            newCfg.paControlPin = -1;
            newCfg.i2sPort = 255; newCfg.enabled = true;
        }

        // Apply updates from JSON
        if (doc.containsKey("i2cAddr"))    newCfg.i2cAddr = doc["i2cAddr"].as<uint8_t>();
        if (doc.containsKey("i2cBus"))     newCfg.i2cBusIndex = doc["i2cBus"].as<uint8_t>();
        if (doc.containsKey("i2cSpeed"))   newCfg.i2cSpeedHz = doc["i2cSpeed"].as<uint32_t>();
        if (doc.containsKey("pinSda"))     newCfg.pinSda = doc["pinSda"].as<int8_t>();
        if (doc.containsKey("pinScl"))     newCfg.pinScl = doc["pinScl"].as<int8_t>();
        if (doc.containsKey("pinMclk"))    newCfg.pinMclk = doc["pinMclk"].as<int8_t>();
        if (doc.containsKey("pinData"))    newCfg.pinData = doc["pinData"].as<int8_t>();
        if (doc.containsKey("i2sPort"))    newCfg.i2sPort = doc["i2sPort"].as<uint8_t>();
        if (doc.containsKey("sampleRate")) newCfg.sampleRate = doc["sampleRate"].as<uint32_t>();
        if (doc.containsKey("bitDepth"))   newCfg.bitDepth = doc["bitDepth"].as<uint8_t>();
        if (doc.containsKey("volume"))     newCfg.volume = doc["volume"].as<uint8_t>();
        if (doc.containsKey("mute"))       newCfg.mute = doc["mute"].as<bool>();
        if (doc.containsKey("enabled"))    newCfg.enabled = doc["enabled"].as<bool>();
        if (doc.containsKey("label")) {
            const char* lbl = doc["label"] | "";
            strncpy(newCfg.userLabel, lbl, 32);
            newCfg.userLabel[32] = '\0';
        }
        if (doc.containsKey("cfgMclkMultiple")) newCfg.mclkMultiple = doc["cfgMclkMultiple"].as<uint16_t>();
        if (doc.containsKey("cfgI2sFormat"))    newCfg.i2sFormat    = doc["cfgI2sFormat"].as<uint8_t>();
        if (doc.containsKey("cfgPgaGain"))      newCfg.pgaGain      = doc["cfgPgaGain"].as<uint8_t>();
        if (doc.containsKey("cfgHpfEnabled"))   newCfg.hpfEnabled   = doc["cfgHpfEnabled"].as<bool>();
        if (doc.containsKey("cfgPaControlPin")) newCfg.paControlPin = doc["cfgPaControlPin"].as<int8_t>();
        if (doc.containsKey("pinBck"))          newCfg.pinBck       = doc["pinBck"].as<int8_t>();
        if (doc.containsKey("pinLrc"))          newCfg.pinLrc       = doc["pinLrc"].as<int8_t>();
        if (doc.containsKey("pinFmt"))          newCfg.pinFmt       = doc["pinFmt"].as<int8_t>();
        newCfg.valid = true;

        mgr.setConfig(slot, newCfg);
        hal_save_device_config(slot);
        hal_apply_config(slot);
        appState.markHalDeviceDirty();

        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // DELETE /api/hal/devices — remove a device
    server.on("/api/hal/devices", HTTP_DELETE, [&server]() {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, server.arg("plain"));
        if (err) { server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }

        uint8_t slot = doc["slot"] | 255;
        if (slot >= HAL_MAX_DEVICES) { server.send(400, "application/json", "{\"error\":\"Invalid slot\"}"); return; }

        HalDeviceManager& mgr = HalDeviceManager::instance();
        HalDevice* dev = mgr.getDevice(slot);
        if (!dev) { server.send(404, "application/json", "{\"error\":\"No device in slot\"}"); return; }

        // DAC-path devices: trigger proper legacy teardown before removal
        const HalDeviceDescriptor& delDesc = dev->getDescriptor();
        if (delDesc.capabilities & HAL_CAP_DAC_PATH) {
            if (delDesc.type == HAL_DEV_DAC) {
                appState.dacEnabled = false;
                appState.requestDacToggle(-1);
            } else if (delDesc.type == HAL_DEV_CODEC) {
                appState.requestEs8311Toggle(-1);
            }
        }

        // Deinit and remove
        dev->deinit();
        mgr.removeDevice(slot);

        // Clear config
        HalDeviceConfig emptyCfg;
        memset(&emptyCfg, 0, sizeof(emptyCfg));
        emptyCfg.valid = false;
        mgr.setConfig(slot, emptyCfg);
        hal_save_device_config(slot);

        appState.markHalDeviceDirty();
        LOG_I("[HAL:API]", "Device removed from slot %d", slot);

        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // POST /api/hal/devices/reinit — re-initialize a device
    server.on("/api/hal/devices/reinit", HTTP_POST, [&server]() {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, server.arg("plain"));
        if (err) { server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }

        uint8_t slot = doc["slot"] | 255;
        if (slot >= HAL_MAX_DEVICES) { server.send(400, "application/json", "{\"error\":\"Invalid slot\"}"); return; }

        HalDeviceManager& mgr = HalDeviceManager::instance();
        HalDevice* dev = mgr.getDevice(slot);
        if (!dev) { server.send(404, "application/json", "{\"error\":\"No device in slot\"}"); return; }

        // Deinit then re-init
        dev->deinit();
        dev->_state = HAL_STATE_CONFIGURING;
        dev->_ready = false;
        appState.markHalDeviceDirty();

        bool ok = dev->probe() && dev->init();
        if (ok) {
            dev->_state = HAL_STATE_AVAILABLE;
            dev->_ready = true;
        } else {
            dev->_state = HAL_STATE_ERROR;
            dev->_ready = false;
        }
        appState.markHalDeviceDirty();

        JsonDocument resp;
        resp["status"] = ok ? "ok" : "error";
        resp["state"] = dev->_state;
        String json;
        serializeJson(resp, json);
        server.send(200, "application/json", json);
    });

    // GET /api/hal/settings — auto-discovery toggle
    server.on("/api/hal/settings", HTTP_GET, [&server]() {
        JsonDocument doc;
        doc["halAutoDiscovery"] = appState.halAutoDiscovery;
        String json;
        serializeJson(doc, json);
        server.send(200, "application/json", json);
    });

    // PUT /api/hal/settings — save auto-discovery toggle
    server.on("/api/hal/settings", HTTP_PUT, [&server]() {
        JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain")) == DeserializationError::Ok) {
            if (doc["halAutoDiscovery"].is<bool>()) {
                appState.halAutoDiscovery = doc["halAutoDiscovery"].as<bool>();
                saveSettings();
            }
        }
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // GET /api/hal/devices/custom — list custom device schemas stored in LittleFS
    server.on("/api/hal/devices/custom", HTTP_GET, [&server]() {
        JsonDocument doc;
        JsonArray arr = doc["schemas"].to<JsonArray>();

        File dir = LittleFS.open("/hal/custom");
        if (dir && dir.isDirectory()) {
            File f = dir.openNextFile();
            while (f) {
                if (!f.isDirectory()) {
                    JsonObject schema = arr.add<JsonObject>();
                    schema["name"] = String(f.name());
                    schema["size"] = f.size();
                }
                f = dir.openNextFile();
            }
        }

        String out;
        serializeJson(doc, out);
        server.send(200, "application/json", out);
    });

    // POST /api/hal/devices/custom — upload a JSON device schema
    server.on("/api/hal/devices/custom", HTTP_POST, [&server]() {
        String bodyBuf = server.arg("plain");

        JsonDocument doc;
        if (deserializeJson(doc, bodyBuf) != DeserializationError::Ok ||
            !doc.containsKey("compatible")) {
            server.send(400, "application/json", "{\"error\":\"Invalid schema\"}");
            return;
        }

        const char* compatible = doc["compatible"];
        String filename = String("/hal/custom/") + String(compatible) + ".json";
        // Replace commas with underscores for filesystem safety
        filename.replace(',', '_');

        if (!LittleFS.exists("/hal/custom")) {
            LittleFS.mkdir("/hal/custom");
        }

        File f = LittleFS.open(filename, "w");
        if (f) {
            f.print(bodyBuf);
            f.close();
            hal_load_custom_devices();  // Reload custom devices
            server.send(200, "application/json", "{\"ok\":true}");
        } else {
            server.send(500, "application/json", "{\"error\":\"Write failed\"}");
        }
    });

    // DELETE /api/hal/devices/custom — remove a schema by name query param (?name=<compatible>)
    server.on("/api/hal/devices/custom", HTTP_DELETE, [&server]() {
        if (!server.hasArg("name")) {
            server.send(400, "application/json", "{\"error\":\"Missing name parameter\"}");
            return;
        }
        String name = server.arg("name");
        String filename = "/hal/custom/" + name + ".json";

        if (LittleFS.remove(filename)) {
            hal_load_custom_devices();
            server.send(200, "application/json", "{\"ok\":true}");
        } else {
            server.send(404, "application/json", "{\"error\":\"Not found\"}");
        }
    });

    LOG_I("[HAL:API]", "Registered HAL REST endpoints");
}

#endif // NATIVE_TEST
#endif // DAC_ENABLED
