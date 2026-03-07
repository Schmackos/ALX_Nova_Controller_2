#ifdef DAC_ENABLED

#include "hal_settings.h"
#include "hal_device_manager.h"
#include "hal_device_db.h"
#include "hal_pipeline_bridge.h"
#include "hal_types.h"

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#include "../app_state.h"
#include "../i2s_audio.h"
#include "../audio_pipeline.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
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
        LOG_E("[HAL:Settings] Failed to open %s for writing", HAL_CONFIG_FILE_PATH);
        return;
    }
    serializeJson(doc, f);
    f.close();
    LOG_I("[HAL:Settings] Configs saved to %s", HAL_CONFIG_FILE_PATH);
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
        LOG_E("[HAL:Settings] Import parse error");
        return false;
    }

    // Write to file
    File f = LittleFS.open(HAL_CONFIG_FILE_PATH, "w");
    if (!f) {
        LOG_E("[HAL:Settings] Failed to open %s for import", HAL_CONFIG_FILE_PATH);
        return false;
    }
    serializeJson(doc, f);
    f.close();
    LOG_I("[HAL:Settings] Config imported successfully");
    return true;
}

HalDeviceConfig* hal_get_config_for_type(HalDeviceType type) {
    HalDeviceManager& mgr = HalDeviceManager::instance();
    for (uint8_t i = 0; i < HAL_MAX_DEVICES; i++) {
        HalDevice* dev = mgr.getDevice(i);
        if (!dev) continue;
        if (dev->getDescriptor().type != type) continue;
        HalDeviceConfig* cfg = mgr.getConfig(i);
        if (cfg && cfg->valid) return cfg;
    }
    return nullptr;
}

void hal_apply_config(uint8_t slot) {
    HalDeviceManager& mgr = HalDeviceManager::instance();
    HalDevice* dev = mgr.getDevice(slot);
    HalDeviceConfig* cfg = mgr.getConfig(slot);
    if (!dev || !cfg || !cfg->valid) return;

    const HalDeviceDescriptor& desc = dev->getDescriptor();

    // DISABLE: deinit device and mark MANUAL (persisted disabled state)
    if (!cfg->enabled) {
        // DAC-path devices: deferred legacy teardown via main loop
        // (I2S driver lifecycle is too heavy for direct call — must pause audio pipeline first)
        if (desc.capabilities & HAL_CAP_DAC_PATH) {
            HalDeviceState oldState = dev->_state;
            dev->_ready = false;
            dev->_state = HAL_STATE_MANUAL;
            // Fire state change callback — bridge removes the sink immediately,
            // then the deferred deinit tears down the I2S driver safely.
            hal_pipeline_state_change(slot, oldState, HAL_STATE_MANUAL);
            if (desc.type == HAL_DEV_DAC) {
                appState.dacEnabled = false;
                appState._pendingDacToggle = -1;
            } else if (desc.type == HAL_DEV_CODEC) {
                appState._pendingEs8311Toggle = -1;
            }
            LOG_I("[HAL:Settings] DAC-path device slot %u disable deferred", slot);
            appState.markHalDeviceDirty();
            return;
        }
        if (dev->_state == HAL_STATE_AVAILABLE || dev->_state == HAL_STATE_CONFIGURING) {
            dev->_ready = false;
            dev->deinit();
        }
        {
            HalDeviceState oldState = dev->_state;
            dev->_state = HAL_STATE_MANUAL;
            dev->_ready = false;
            hal_pipeline_state_change(slot, oldState, HAL_STATE_MANUAL);
        }
        LOG_I("[HAL:Settings] Device slot %u disabled", slot);
        appState.markHalDeviceDirty();
        return;
    }

    // RE-ENABLE from MANUAL state: probe + init
    if (dev->_state == HAL_STATE_MANUAL) {
        // DAC-path devices: deferred legacy re-init via main loop
        if (desc.capabilities & HAL_CAP_DAC_PATH) {
            if (desc.type == HAL_DEV_DAC) {
                appState.dacEnabled = true;
                appState._pendingDacToggle = 1;
            } else if (desc.type == HAL_DEV_CODEC) {
                appState._pendingEs8311Toggle = 1;
            }
            LOG_I("[HAL:Settings] DAC-path device slot %u re-enable deferred", slot);
            appState.markHalDeviceDirty();
            return;
        }
        dev->_state = HAL_STATE_CONFIGURING;
        hal_pipeline_state_change(slot, HAL_STATE_MANUAL, HAL_STATE_CONFIGURING);
        bool ok = dev->probe() && dev->init();
        dev->_state = ok ? HAL_STATE_AVAILABLE : HAL_STATE_ERROR;
        hal_pipeline_state_change(slot, HAL_STATE_CONFIGURING,
                                  ok ? HAL_STATE_AVAILABLE : HAL_STATE_ERROR);
        // Re-enable I2S devices: reinit I2S with potentially changed pin config
        if (ok && desc.bus.type == HAL_BUS_I2S) {
            appState.audioPaused = true;
            vTaskDelay(pdMS_TO_TICKS(30));
            i2s_audio_set_sample_rate(appState.audioSampleRate);
            appState.audioPaused = false;
        }
        LOG_I("[HAL:Settings] Device slot %u %s", slot, ok ? "re-enabled" : "re-enable failed");
        appState.markHalDeviceDirty();
        return;
    }

    // RECONFIGURE (already enabled — update pins/rate/etc.)

    if (desc.bus.type == HAL_BUS_I2S) {
        // Pause audio pipeline, reconfigure I2S with new pins, resume
        appState.audioPaused = true;
        vTaskDelay(pdMS_TO_TICKS(30));
        i2s_audio_set_sample_rate(appState.audioSampleRate);
        appState.audioPaused = false;
        LOG_I("[HAL:Settings] I2S device slot %u reconfigured (hot)", slot);
    } else {
        HalDeviceState prevState = dev->_state;
        dev->_state = HAL_STATE_CONFIGURING;
        hal_pipeline_state_change(slot, prevState, HAL_STATE_CONFIGURING);
        dev->deinit();
        if (dev->init()) {
            dev->_state = HAL_STATE_AVAILABLE;
            hal_pipeline_state_change(slot, HAL_STATE_CONFIGURING, HAL_STATE_AVAILABLE);
            LOG_I("[HAL:Settings] Device slot %u reinitialised", slot);
        } else {
            dev->_state = HAL_STATE_ERROR;
            hal_pipeline_state_change(slot, HAL_STATE_CONFIGURING, HAL_STATE_ERROR);
            LOG_E("[HAL:Settings] Device slot %u reinit failed", slot);
        }
    }
    appState.markHalDeviceDirty();
}

#else
// Native test stubs
void hal_save_all_configs() {}
void hal_save_device_config_deferred(uint8_t) {}
void hal_check_deferred_save() {}
bool hal_export_configs(char*, size_t) { return false; }
bool hal_import_configs(const char*, size_t) { return false; }
HalDeviceConfig* hal_get_config_for_type(HalDeviceType) { return nullptr; }
void hal_apply_config(uint8_t) {}
#endif

#endif // DAC_ENABLED
