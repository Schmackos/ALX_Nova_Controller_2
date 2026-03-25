#ifdef DAC_ENABLED
#ifndef NATIVE_TEST

#include "hal_api.h"
#include "../http_security.h"
#include "../auth_handler.h"
#include "../rate_limiter.h"
#include "hal_device_manager.h"
#include "hal_device_db.h"
#include "hal_driver_registry.h"
#include "hal_discovery.h"
#include "hal_settings.h"
#include "hal_custom_device.h"
#include "../debug_serial.h"
#include "../app_state.h"
#include "../audio_pipeline.h"
#include "../globals.h"
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

    // Surface last init error for devices in error/unavailable state
    if (dev->_state == HAL_STATE_ERROR || dev->_state == HAL_STATE_UNAVAILABLE) {
        if (dev->_lastError[0]) {
            obj["lastError"] = dev->getLastError();
        }
    }

    // Persistent fault counter — incremented on retry exhaustion, survives reboot
    obj["faultCount"] = HalDeviceManager::instance().getFaultCount(dev->getSlot());

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
        obj["cfgGpioA"]        = cfg->gpioA;
        obj["cfgGpioB"]        = cfg->gpioB;
        obj["cfgGpioC"]        = cfg->gpioC;
        obj["cfgGpioD"]        = cfg->gpioD;
        obj["cfgUsbPid"]       = cfg->usbPid;
        obj["cfgFilterMode"]   = cfg->filterMode;
    }

    // Clock quality diagnostics — for devices with DPLL or APLL capability
    if (desc.capabilities & (HAL_CAP_DPLL | HAL_CAP_APLL)) {
        ClockStatus cs = dev->getClockStatus();
        JsonObject clk = obj["clockStatus"].to<JsonObject>();
        clk["available"] = cs.available;
        clk["locked"]    = cs.locked;
        clk["desc"]      = cs.description;
    }

    // Device dependency graph — list of slot indices this device depends on
    uint32_t deps = dev->getDependencies();
    if (deps) {
        JsonArray depArr = obj["dependsOn"].to<JsonArray>();
        uint32_t tmp = deps;
        while (tmp) {
            int s = __builtin_ctz(tmp);
            depArr.add(s);
            tmp &= tmp - 1;
        }
    }
}

void registerHalApiEndpoints(WebServer& server) {
    // Helper: register handler on both /api/<path> and /api/v1/<path>
    auto server_on_v = [&server](const char* path, HTTPMethod method, WebServer::THandlerFunction handler) {
        server.on(path, method, handler);
        String v1path = "/api/v1";
        v1path += (path + 4);  // skip "/api", prepend "/api/v1"
        server.on(v1path.c_str(), method, handler);
    };

    // GET /api/hal/devices — list all registered devices
    server_on_v("/api/hal/devices", HTTP_GET, [&server]() {
        if (!requireAuth()) return;
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();

        HalDeviceManager::instance().forEach([](HalDevice* dev, void* ctx) {
            JsonArray* a = static_cast<JsonArray*>(ctx);
            JsonObject obj = a->add<JsonObject>();
            deviceToJson(obj, dev);
        }, &arr);

        String json;
        serializeJson(doc, json);
        server_send(200, "application/json", json);
    });

    // POST /api/hal/scan — trigger device rescan (async, non-blocking)
    // Returns 202 Accepted immediately. The scan runs in a one-shot FreeRTOS
    // task on Core 0 so the web server remains responsive throughout.
    // Poll GET /api/hal/devices or watch the "halDeviceState" WebSocket
    // broadcast (scanning=false) to know when the scan is complete.
    server_on_v("/api/hal/scan", HTTP_POST, [&server]() {
        if (!requireAuth()) return;
        if (appState._halScanInProgress) {
            server_send(409, "application/json", "{\"error\":\"Scan already in progress\"}");
            return;
        }
        appState._halScanInProgress = true;
        appState.markHalDeviceDirty();  // Broadcast scanning=true

        bool partialScan = hal_wifi_sdio_active();

#ifndef NATIVE_TEST
        // One-shot task: runs hal_rescan() on Core 0 then clears the flag.
        // Stack 4096 is sufficient — hal_rescan() uses <1KB locals.
        struct HalScanTaskCtx { bool partialScan; };
        static auto halScanTask = [](void* param) {
            (void)param;
            int found = hal_rescan();
            appState._halScanInProgress = false;
            appState.markHalDeviceDirty();  // Broadcast scanning=false
            LOG_I("[HAL:API]", "Async scan complete: %d device(s) found", found);
            vTaskDelete(NULL);
        };
        BaseType_t spawned = xTaskCreatePinnedToCore(
            halScanTask, "hal_scan", 4096, nullptr, 1, nullptr, 0);
        if (spawned != pdPASS) {
            // Task creation failed — fall back to synchronous execution so the
            // scan still runs rather than leaving _halScanInProgress stuck true.
            LOG_W("[HAL:API]", "Failed to spawn async scan task — running synchronously");
            int found = hal_rescan();
            appState._halScanInProgress = false;
            appState.markHalDeviceDirty();
            LOG_I("[HAL:API]", "Sync fallback scan complete: %d device(s) found", found);
        }
#else
        // Native test environment: no FreeRTOS, run synchronously.
        hal_rescan();
        appState._halScanInProgress = false;
        appState.markHalDeviceDirty();
#endif

        JsonDocument doc;
        doc["status"] = "scanning";
        doc["partialScan"] = partialScan;
        if (partialScan) {
            doc["skippedBuses"] = "Bus 0 (WiFi SDIO conflict)";
        }
        String json;
        serializeJson(doc, json);
        server_send(202, "application/json", json);
    });

    // GET /api/hal/db — list device database entries
    server_on_v("/api/hal/db", HTTP_GET, [&server]() {
        if (!requireAuth()) return;
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
        server_send(200, "application/json", json);
    });

    // GET /api/hal/db/presets — list available device presets
    server_on_v("/api/hal/db/presets", HTTP_GET, [&server]() {
        if (!requireAuth()) return;
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
        server_send(200, "application/json", json);
    });

    // POST /api/hal/devices — manually register a device by compatible string
    server_on_v("/api/hal/devices", HTTP_POST, [&server]() {
        if (!requireAuth()) return;
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, server.arg("plain"));
        if (err) { server_send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }

        const char* compatible = doc["compatible"] | "";
        if (!compatible[0]) { server_send(400, "application/json", "{\"error\":\"Missing compatible\"}"); return; }

        HalDeviceDescriptor desc;
        if (!hal_db_lookup(compatible, &desc)) {
            server_send(404, "application/json", "{\"error\":\"Unknown device\"}"); return;
        }

        const HalDriverEntry* entry = hal_registry_find(compatible);
        if (!entry || !entry->factory) {
            server_send(422, "application/json", "{\"error\":\"No driver for this device\"}"); return;
        }

        HalDeviceManager& mgr = HalDeviceManager::instance();
        // Allow multiple instances of the same compatible string (multi-instance support).
        // Max 8 instances per compatible — matches pipeline dimension limits.
        if (mgr.countByCompatible(compatible) >= 8) {
            server_send(409, "application/json", "{\"error\":\"Max instances reached for this device\"}"); return;
        }
        if (mgr.getCount() >= HAL_MAX_DEVICES) {
            server_send(409, "application/json", "{\"error\":\"No free slots\"}"); return;
        }

        HalDevice* dev = entry->factory();
        if (!dev) { server_send(500, "application/json", "{\"error\":\"Factory failed\"}"); return; }

        int slot = mgr.registerDevice(dev, HAL_DISC_MANUAL, true);
        if (slot < 0) { delete dev; server_send(500, "application/json", "{\"error\":\"No free slots\"}"); return; }

        dev->_state = HAL_STATE_CONFIGURING;
        bool probeOk = dev->probe();
        HalInitResult initResult = probeOk ? dev->init() : hal_init_fail(DIAG_HAL_INIT_FAILED, "I2C probe failed (no device response)");
        bool ok = probeOk && initResult.success;
        if (ok) {
            dev->clearLastError();
            dev->setReady(true);
            dev->_state = HAL_STATE_AVAILABLE;
        } else {
            dev->setLastError(initResult);
            dev->setReady(false);
            dev->_state = HAL_STATE_ERROR;
            LOG_W("[HAL] Probe/init failed for %s (slot %d): %s", compatible, slot, initResult.reason);
        }

        hal_save_device_config(static_cast<uint8_t>(slot));
        appState.markHalDeviceDirty();
        LOG_I("[HAL:API]", "Manual register: %s slot %d (%s)", compatible, slot, ok ? "ok" : "init failed");

        JsonDocument resp;
        resp["status"] = ok ? "ok" : "error";
        resp["slot"] = slot;
        resp["name"] = desc.name;
        resp["state"] = static_cast<uint8_t>(dev->_state);
        if (!ok && dev->_lastError[0]) {
            resp["error"] = dev->getLastError();
        }
        String json; serializeJson(resp, json);
        server_send(201, "application/json", json);
    });

    // PUT /api/hal/devices — update device config
    server_on_v("/api/hal/devices", HTTP_PUT, [&server]() {
        if (!requireAuth()) return;
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, server.arg("plain"));
        if (err) { server_send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }

        uint8_t slot = doc["slot"] | 255;
        if (slot >= HAL_MAX_DEVICES) { server_send(400, "application/json", "{\"error\":\"Invalid slot\"}"); return; }

        HalDeviceManager& mgr = HalDeviceManager::instance();
        HalDevice* dev = mgr.getDevice(slot);
        if (!dev) { server_send(404, "application/json", "{\"error\":\"No device in slot\"}"); return; }

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
            newCfg.gpioA = -1; newCfg.gpioB = -1; newCfg.gpioC = -1; newCfg.gpioD = -1;
            newCfg.usbPid = 0;
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
        if (doc.containsKey("gpioA"))           newCfg.gpioA        = doc["gpioA"].as<int8_t>();
        if (doc.containsKey("gpioB"))           newCfg.gpioB        = doc["gpioB"].as<int8_t>();
        if (doc.containsKey("gpioC"))           newCfg.gpioC        = doc["gpioC"].as<int8_t>();
        if (doc.containsKey("gpioD"))           newCfg.gpioD        = doc["gpioD"].as<int8_t>();
        if (doc.containsKey("usbPid"))          newCfg.usbPid       = doc["usbPid"].as<uint16_t>();
        if (doc.containsKey("filterMode"))     newCfg.filterMode   = doc["filterMode"].as<uint8_t>();
        newCfg.valid = true;

        // Validate bounds before persisting
        HalConfigError cfgErr = hal_validate_config(newCfg);
        if (!cfgErr.valid) {
            JsonDocument errDoc;
            errDoc["error"] = "Invalid config";
            errDoc["field"] = cfgErr.field;
            errDoc["message"] = cfgErr.message;
            String errJson;
            serializeJson(errDoc, errJson);
            server_send(422, "application/json", errJson);
            return;
        }

        mgr.setConfig(slot, newCfg);
        hal_save_device_config(slot);
        hal_apply_config(slot);
        appState.markHalDeviceDirty();

        server_send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // DELETE /api/hal/devices — remove a device
    server_on_v("/api/hal/devices", HTTP_DELETE, [&server]() {
        if (!requireAuth()) return;
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, server.arg("plain"));
        if (err) { server_send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }

        uint8_t slot = doc["slot"] | 255;
        if (slot >= HAL_MAX_DEVICES) { server_send(400, "application/json", "{\"error\":\"Invalid slot\"}"); return; }

        HalDeviceManager& mgr = HalDeviceManager::instance();
        HalDevice* dev = mgr.getDevice(slot);
        if (!dev) { server_send(404, "application/json", "{\"error\":\"No device in slot\"}"); return; }

        // DAC-path devices: trigger deferred deactivation before removal
        const HalDeviceDescriptor& delDesc = dev->getDescriptor();
        if (delDesc.capabilities & HAL_CAP_DAC_PATH) {
            // Generic deferred deactivation — device-type agnostic
            if (!appState.halCoord.requestDeviceToggle(slot, -1)) {
                LOG_W("[HAL API] Toggle queue full for slot %u (device delete)", slot);
            }
            // Update HalDeviceConfig (authoritative source)
            HalDeviceConfig* cfg = mgr.getConfig(slot);
            if (cfg) cfg->enabled = false;
            appState.markHalDeviceDirty();
        }

        // Remove (removeDevice handles deinit and ownership-based delete)
        mgr.removeDevice(slot);

        // Clear config
        HalDeviceConfig emptyCfg;
        memset(&emptyCfg, 0, sizeof(emptyCfg));
        emptyCfg.valid = false;
        mgr.setConfig(slot, emptyCfg);
        hal_save_device_config(slot);

        appState.markHalDeviceDirty();
        LOG_I("[HAL:API]", "Device removed from slot %d", slot);

        server_send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // POST /api/hal/devices/reinit — re-initialize a device
    server_on_v("/api/hal/devices/reinit", HTTP_POST, [&server]() {
        if (!requireAuth()) return;
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, server.arg("plain"));
        if (err) { server_send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }

        uint8_t slot = doc["slot"] | 255;
        if (slot >= HAL_MAX_DEVICES) { server_send(400, "application/json", "{\"error\":\"Invalid slot\"}"); return; }

        HalDeviceManager& mgr = HalDeviceManager::instance();
        HalDevice* dev = mgr.getDevice(slot);
        if (!dev) { server_send(404, "application/json", "{\"error\":\"No device in slot\"}"); return; }

        // Deinit then re-init (pause audio pipeline for safe I2S teardown)
        audio_pipeline_request_pause(500);
        dev->deinit();
        dev->setReady(false);
        dev->_state = HAL_STATE_CONFIGURING;
        appState.markHalDeviceDirty();

        bool probeOk = dev->probe();
        HalInitResult reinitResult = probeOk ? dev->init() : hal_init_fail(DIAG_HAL_INIT_FAILED, "I2C probe failed (no device response)");
        bool ok = probeOk && reinitResult.success;
        if (ok) {
            dev->clearLastError();
            dev->setReady(true);
            dev->_state = HAL_STATE_AVAILABLE;
        } else {
            dev->setLastError(reinitResult);
            dev->setReady(false);
            dev->_state = HAL_STATE_ERROR;
            LOG_W("[HAL] Reinit failed for slot %d: %s", slot, reinitResult.reason);
        }
        audio_pipeline_resume();
        appState.markHalDeviceDirty();

        JsonDocument resp;
        resp["status"] = ok ? "ok" : "error";
        resp["state"] = dev->_state;
        if (!ok && dev->_lastError[0]) {
            resp["error"] = dev->getLastError();
        }
        String json;
        serializeJson(resp, json);
        server_send(200, "application/json", json);
    });

    // POST /api/hal/devices/power — set power management state for a device
    // Body: {"slot": N, "state": "active"|"standby"|"off"}
    // Returns 422 if the device does not advertise HAL_CAP_POWER_MGMT.
    server_on_v("/api/hal/devices/power", HTTP_POST, [&server]() {
        if (!requireAuth()) return;
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, server.arg("plain"));
        if (err) { server_send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }

        uint8_t slot = doc["slot"] | 255;
        if (slot >= HAL_MAX_DEVICES) {
            server_send(400, "application/json", "{\"error\":\"Invalid slot\"}");
            return;
        }

        HalDeviceManager& mgr = HalDeviceManager::instance();
        HalDevice* dev = mgr.getDevice(slot);
        if (!dev) {
            server_send(404, "application/json", "{\"error\":\"No device in slot\"}");
            return;
        }

        if (!(dev->getDescriptor().capabilities & HAL_CAP_POWER_MGMT)) {
            server_send(422, "application/json", "{\"error\":\"Device does not support power management\"}");
            return;
        }

        const char* stateStr = doc["state"] | "";
        bool ok = false;
        HalPowerState newPm = dev->getPowerState();

        if (strcmp(stateStr, "active") == 0) {
            if (dev->getPowerState() == HAL_PM_STANDBY) {
                ok = dev->wake();
            } else if (dev->getPowerState() == HAL_PM_OFF) {
                ok = dev->powerOn();
            } else {
                ok = true;  // Already active
            }
            if (ok) newPm = HAL_PM_ACTIVE;
        } else if (strcmp(stateStr, "standby") == 0) {
            ok = dev->enterStandby();
            if (ok) newPm = HAL_PM_STANDBY;
        } else if (strcmp(stateStr, "off") == 0) {
            ok = dev->powerOff();
            if (ok) newPm = HAL_PM_OFF;
        } else {
            server_send(400, "application/json", "{\"error\":\"state must be active, standby, or off\"}");
            return;
        }

        if (ok) {
            dev->_powerState = newPm;
            appState.markHalDeviceDirty();
            LOG_I("[HAL:API] Power state for slot %d set to %s", slot, stateStr);
        } else {
            LOG_W("[HAL:API] Power state transition to %s failed for slot %d", stateStr, slot);
        }

        JsonDocument resp;
        resp["status"] = ok ? "ok" : "error";
        resp["slot"] = slot;
        resp["powerState"] = (uint8_t)dev->getPowerState();
        if (!ok) resp["error"] = "Power state transition failed";
        String json;
        serializeJson(resp, json);
        server_send(ok ? 200 : 500, "application/json", json);
    });

    // GET /api/hal/settings — auto-discovery toggle
    server_on_v("/api/hal/settings", HTTP_GET, [&server]() {
        if (!requireAuth()) return;
        JsonDocument doc;
        doc["halAutoDiscovery"] = appState.halAutoDiscovery;
        String json;
        serializeJson(doc, json);
        server_send(200, "application/json", json);
    });

    // PUT /api/hal/settings — save auto-discovery toggle
    server_on_v("/api/hal/settings", HTTP_PUT, [&server]() {
        if (!requireAuth()) return;
        JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain")) == DeserializationError::Ok) {
            if (doc["halAutoDiscovery"].is<bool>()) {
                appState.halAutoDiscovery = doc["halAutoDiscovery"].as<bool>();
                saveSettings();
            }
        }
        server_send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // GET /api/hal/devices/custom — list custom device schemas stored in LittleFS
    server_on_v("/api/hal/devices/custom", HTTP_GET, [&server]() {
        if (!requireAuth()) return;
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
        server_send(200, "application/json", out);
    });

    // POST /api/hal/devices/custom — upload a JSON device schema
    server_on_v("/api/hal/devices/custom", HTTP_POST, [&server]() {
        if (!requireAuth()) return;
        String bodyBuf = server.arg("plain");

        JsonDocument doc;
        if (deserializeJson(doc, bodyBuf) != DeserializationError::Ok ||
            !doc.containsKey("compatible")) {
            server_send(400, "application/json", "{\"error\":\"Invalid schema\"}");
            return;
        }

        const char* compatible = doc["compatible"];
        // Validate compatible string: reject path traversal characters
        if (strstr(compatible, "..") || strchr(compatible, '/') || strchr(compatible, '\\')) {
            server_send(400, "application/json", "{\"error\":\"Invalid compatible string\"}");
            return;
        }
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
            server_send(200, "application/json", "{\"ok\":true}");
        } else {
            server_send(500, "application/json", "{\"error\":\"Write failed\"}");
        }
    });

    // DELETE /api/hal/devices/custom — remove a schema by name query param (?name=<compatible>)
    server_on_v("/api/hal/devices/custom", HTTP_DELETE, [&server]() {
        if (!requireAuth()) return;
        if (!server.hasArg("name")) {
            server_send(400, "application/json", "{\"error\":\"Missing name parameter\"}");
            return;
        }
        char safeName[36];
        if (!sanitize_filename(server.arg("name").c_str(), safeName, sizeof(safeName))) {
            server_send(400, "application/json", "{\"error\":\"Invalid name\"}");
            return;
        }
        String filename = "/hal/custom/" + String(safeName) + ".json";

        if (LittleFS.remove(filename)) {
            hal_load_custom_devices();
            server_send(200, "application/json", "{\"ok\":true}");
        } else {
            server_send(404, "application/json", "{\"error\":\"Not found\"}");
        }
    });

    // POST /api/hal/devices/custom/create — structured custom device creation
    // Accepts JSON body: name, type, bus, i2cAddr, i2cBus, i2sPort, channels,
    //                    capabilities[], initSequence[] (up to 32 {reg, val} pairs).
    // Auto-generates compatible: "custom," + slugified(name).
    // Returns 201 with slot, name, state.
    server_on_v("/api/hal/devices/custom/create", HTTP_POST, [&server]() {
        if (!requireAuth()) return;
        JsonDocument body;
        DeserializationError err = deserializeJson(body, server.arg("plain"));
        if (err) {
            server_send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }

        // Validate required: name
        const char* devName = body["name"] | "";
        if (!devName[0]) {
            server_send(400, "application/json", "{\"error\":\"name is required\"}");
            return;
        }

        // Validate type
        const char* typeStr = body["type"] | "dac";
        if (strcmp(typeStr, "dac") != 0 && strcmp(typeStr, "adc") != 0 &&
            strcmp(typeStr, "codec") != 0 && strcmp(typeStr, "amp") != 0) {
            server_send(400, "application/json", "{\"error\":\"Invalid type (dac/adc/codec/amp)\"}");
            return;
        }

        // Validate I2C address if provided
        uint8_t i2cAddr = 0;
        if (body.containsKey("i2cAddr")) {
            int addrVal = body["i2cAddr"].as<int>();
            if (addrVal < 0x08 || addrVal > 0x77) {
                server_send(400, "application/json", "{\"error\":\"i2cAddr must be 0x08-0x77\"}");
                return;
            }
            i2cAddr = (uint8_t)addrVal;
        }

        // Validate bus index
        uint8_t i2cBus = body["i2cBus"] | (uint8_t)2;
        if (i2cBus > 2) {
            server_send(400, "application/json", "{\"error\":\"i2cBus must be 0-2\"}");
            return;
        }

        // Validate initSequence length
        if (body["initSequence"].is<JsonArray>()) {
            if (body["initSequence"].as<JsonArray>().size() > 32) {
                server_send(400, "application/json",
                            "{\"error\":\"initSequence exceeds 32 entries\"}");
                return;
            }
        }

        // Auto-generate compatible string: "custom," + slugified(name)
        char slug[30];
        {
            const char* src = devName;
            int si = 0;
            for (int ci = 0; src[ci] && si < 29; ci++) {
                char c = src[ci];
                if (c >= 'A' && c <= 'Z') c = (char)(c + 32);  // lowercase
                if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') {
                    slug[si++] = c;
                } else if (c == ' ' || c == '_') {
                    slug[si++] = '-';
                }
                // Other chars stripped
            }
            slug[si] = '\0';
        }
        char compatible[36];
        snprintf(compatible, sizeof(compatible), "custom,%s", slug);

        // Build schema JSON
        JsonDocument schema;
        schema["compatible"] = compatible;
        schema["name"]       = devName;
        schema["type"]       = typeStr;
        schema["bus"]        = body["bus"] | "i2c";
        if (i2cAddr) schema["i2cAddr"]  = (int)i2cAddr;
        if (body.containsKey("i2cBus"))  schema["i2cBus"]  = i2cBus;
        if (body.containsKey("i2sPort")) schema["i2sPort"] = body["i2sPort"].as<uint8_t>();
        if (body.containsKey("channels")) schema["channels"] = body["channels"].as<uint8_t>();

        // Copy capabilities array
        if (body["capabilities"].is<JsonArray>()) {
            JsonArray dstCaps = schema["capabilities"].to<JsonArray>();
            for (const char* cap : body["capabilities"].as<JsonArray>()) {
                dstCaps.add(cap);
            }
        }

        // Copy initSequence array
        if (body["initSequence"].is<JsonArray>()) {
            JsonArray dstSeq = schema["initSequence"].to<JsonArray>();
            for (JsonObject entry : body["initSequence"].as<JsonArray>()) {
                JsonObject dstEntry = dstSeq.add<JsonObject>();
                dstEntry["reg"] = entry["reg"].as<uint8_t>();
                dstEntry["val"] = entry["val"].as<uint8_t>();
            }
        }

        // Copy defaults
        if (body["defaults"].is<JsonObject>()) {
            JsonObject dstDef = schema["defaults"].to<JsonObject>();
            JsonObject srcDef = body["defaults"].as<JsonObject>();
            if (srcDef["sample_rate"].is<uint32_t>())
                dstDef["sample_rate"] = srcDef["sample_rate"].as<uint32_t>();
            if (srcDef["bits_per_sample"].is<uint8_t>())
                dstDef["bits_per_sample"] = srcDef["bits_per_sample"].as<uint8_t>();
            if (srcDef["mclk_multiple"].is<uint16_t>())
                dstDef["mclk_multiple"] = srcDef["mclk_multiple"].as<uint16_t>();
        }

        // Persist schema to LittleFS
        String schemaJson;
        serializeJson(schema, schemaJson);
        if (!LittleFS.exists("/hal/custom")) {
            LittleFS.mkdir("/hal/custom");
        }
        {
            char fname[80];
            snprintf(fname, sizeof(fname), "/hal/custom/%s.json", compatible);
            for (char* p = fname; *p; p++) {
                if (*p == ',') *p = '_';
            }
            File f = LittleFS.open(fname, "w");
            if (!f) {
                server_send(500, "application/json", "{\"error\":\"Failed to write schema\"}");
                return;
            }
            f.print(schemaJson);
            f.close();
        }

        // Reload custom devices — picks up the newly written schema
        hal_load_custom_devices();

        // Find the newly registered device
        HalDeviceManager& mgr2 = HalDeviceManager::instance();
        HalDevice* newDev = mgr2.findByCompatible(compatible);
        int newSlot = newDev ? (int)newDev->getSlot() : -1;

        JsonDocument resp;
        resp["status"]     = newDev ? "ok" : "registered";
        resp["compatible"] = compatible;
        resp["name"]       = devName;
        resp["slot"]       = newSlot;
        if (newDev) {
            resp["state"] = (uint8_t)newDev->_state;
            if (newDev->_lastError[0]) resp["error"] = newDev->getLastError();
        }
        appState.markHalDeviceDirty();
        LOG_I("[HAL:API]", "Custom device created: %s (slot %d)", compatible, newSlot);

        String respJson;
        serializeJson(resp, respJson);
        server_send(201, "application/json", respJson);
    });

    // POST /api/hal/devices/faults/clear — reset all persistent fault counters (field service)
    server_on_v("/api/hal/devices/faults/clear", HTTP_POST, [&server]() {
        if (!requireAuth()) return;
        HalDeviceManager::instance().clearFaultCounters();
        LOG_I("[HAL:API] Fault counters cleared by field service request");
        json_response(server, 200);
    });

    // GET /api/hal/scan/unmatched — return unmatched I2C addresses from last scan
    server_on_v("/api/hal/scan/unmatched", HTTP_GET, [&server]() {
        if (!requireAuth()) return;
        HalUnmatchedAddr buf[HAL_UNMATCHED_MAX];
        int n = hal_get_unmatched_addresses(buf, HAL_UNMATCHED_MAX);

        JsonDocument doc;
        JsonArray arr = doc["unmatched"].to<JsonArray>();
        for (int i = 0; i < n; i++) {
            JsonObject obj = arr.add<JsonObject>();
            obj["addr"] = buf[i].addr;
            obj["bus"]  = buf[i].bus;
            char hexAddr[8];
            snprintf(hexAddr, sizeof(hexAddr), "0x%02X", buf[i].addr);
            obj["addrHex"] = hexAddr;
        }
        doc["count"] = n;
        String json;
        serializeJson(doc, json);
        server_send(200, "application/json", json);
    });

    LOG_I("[HAL:API]", "Registered HAL REST endpoints");
}

#endif // NATIVE_TEST
#endif // DAC_ENABLED
