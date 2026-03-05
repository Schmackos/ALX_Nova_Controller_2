#ifdef DAC_ENABLED
#ifndef NATIVE_TEST

#include "hal_api.h"
#include "hal_device_manager.h"
#include "hal_device_db.h"
#include "hal_discovery.h"
#include "../debug_serial.h"
#include "../app_state.h"
#include <ArduinoJson.h>

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
        int found = hal_rescan();
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

    LOG_I("[HAL API]", "Registered HAL REST endpoints");
}

#endif // NATIVE_TEST
#endif // DAC_ENABLED
