#ifdef DAC_ENABLED

#include "hal_settings.h"
#include "hal_device_manager.h"
#include "hal_device_db.h"
#include "hal_types.h"

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#else
#define LOG_I(...)
#define LOG_W(...)
#define LOG_E(...)
#endif

static bool _savePending = false;
static unsigned long _lastSaveRequest = 0;
static const unsigned long HAL_SAVE_DEBOUNCE_MS = 2000;

#ifndef NATIVE_TEST

void hal_save_all_configs() {
    JsonDocument doc;
    JsonArray arr = doc["devices"].to<JsonArray>();

    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.forEach([](HalDevice* dev, void* ctx) {
        JsonArray* a = static_cast<JsonArray*>(ctx);
        const HalDeviceDescriptor& desc = dev->getDescriptor();
        JsonObject obj = a->add<JsonObject>();
        obj["slot"] = dev->getSlot();
        obj["compatible"] = desc.compatible;
        obj["name"] = desc.name;
        obj["type"] = static_cast<uint8_t>(desc.type);
        obj["discovery"] = static_cast<uint8_t>(dev->getDiscovery());
        obj["state"] = static_cast<uint8_t>(dev->_state);
        obj["channelCount"] = desc.channelCount;
        obj["i2cAddr"] = desc.i2cAddr;
        obj["capabilities"] = desc.capabilities;
        obj["legacyId"] = desc.legacyId;
    }, &arr);

    File f = LittleFS.open(HAL_CONFIG_FILE_PATH, "w");
    if (!f) {
        LOG_E("[HAL] Failed to open %s for writing", HAL_CONFIG_FILE_PATH);
        return;
    }
    serializeJson(doc, f);
    f.close();
    LOG_I("[HAL] Configs saved to %s", HAL_CONFIG_FILE_PATH);
}

void hal_save_device_config_deferred(uint8_t slot) {
    (void)slot;  // Save all devices in single file
    _savePending = true;
    _lastSaveRequest = millis();
}

void hal_check_deferred_save() {
    if (_savePending && (millis() - _lastSaveRequest >= HAL_SAVE_DEBOUNCE_MS)) {
        hal_save_all_configs();
        _savePending = false;
    }
}

bool hal_export_configs(char* buf, size_t bufSize) {
    if (!buf || bufSize == 0) return false;

    JsonDocument doc;
    JsonArray arr = doc["devices"].to<JsonArray>();

    HalDeviceManager& mgr = HalDeviceManager::instance();
    mgr.forEach([](HalDevice* dev, void* ctx) {
        JsonArray* a = static_cast<JsonArray*>(ctx);
        const HalDeviceDescriptor& desc = dev->getDescriptor();
        JsonObject obj = a->add<JsonObject>();
        obj["slot"] = dev->getSlot();
        obj["compatible"] = desc.compatible;
        obj["name"] = desc.name;
        obj["type"] = static_cast<uint8_t>(desc.type);
        obj["channelCount"] = desc.channelCount;
    }, &arr);

    size_t written = serializeJson(doc, buf, bufSize);
    return written > 0 && written < bufSize;
}

bool hal_import_configs(const char* json, size_t len) {
    if (!json || len == 0) return false;

    JsonDocument doc;
    if (deserializeJson(doc, json, len)) {
        LOG_E("[HAL] Import parse error");
        return false;
    }

    // Write to file
    File f = LittleFS.open(HAL_CONFIG_FILE_PATH, "w");
    if (!f) {
        LOG_E("[HAL] Failed to open %s for import", HAL_CONFIG_FILE_PATH);
        return false;
    }
    serializeJson(doc, f);
    f.close();
    LOG_I("[HAL] Config imported successfully");
    return true;
}

#else
// Native test stubs
void hal_save_all_configs() {}
void hal_save_device_config_deferred(uint8_t) {}
void hal_check_deferred_save() {}
bool hal_export_configs(char*, size_t) { return false; }
bool hal_import_configs(const char*, size_t) { return false; }
#endif

#endif // DAC_ENABLED
