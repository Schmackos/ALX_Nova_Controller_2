#ifdef DAC_ENABLED
#include "hal_custom_device.h"
#include "hal_device_manager.h"
#include "hal_types.h"

#ifndef NATIVE_TEST
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "../debug_serial.h"
#else
#define LOG_I(t, ...) ((void)0)
#define LOG_W(t, ...) ((void)0)
#define LOG_E(t, ...) ((void)0)
#endif

// ===== HalCustomDevice =====

HalCustomDevice::HalCustomDevice(const char* compatible, const char* name,
                                  uint8_t caps, uint8_t busType) {
    strncpy(_descriptor.compatible, compatible, sizeof(_descriptor.compatible) - 1);
    _descriptor.compatible[sizeof(_descriptor.compatible) - 1] = '\0';
    strncpy(_descriptor.name, name, sizeof(_descriptor.name) - 1);
    _descriptor.name[sizeof(_descriptor.name) - 1] = '\0';
    _descriptor.type = HAL_DEV_DAC;  // Default; overridden by loader if ADC/CODEC
    _descriptor.capabilities = caps;
    _descriptor.bus.type = (HalBusType)busType;
    _initPriority = HAL_PRIORITY_HARDWARE;
}

bool HalCustomDevice::probe() {
    // No hardware probe for custom devices — always return true
    return true;
}

bool HalCustomDevice::init() {
    _state = HAL_STATE_AVAILABLE;
    _ready = true;
    LOG_I("[HAL Custom]", "Custom device init: %s", _descriptor.name);
    return true;
}

void HalCustomDevice::deinit() {
    _ready = false;
    _state = HAL_STATE_REMOVED;
}

void HalCustomDevice::dumpConfig() {
    LOG_I("[HAL Custom]", "Custom: %s (%s)", _descriptor.name, _descriptor.compatible);
}

bool HalCustomDevice::healthCheck() {
    return _ready;
}

bool HalCustomDevice::configure(uint32_t sampleRate, uint8_t bitDepth) {
    // Store to config — actual hardware reconfiguration deferred to platform class
    HalDeviceConfig* cfg = HalDeviceManager::instance().getConfig(_slot);
    if (cfg) {
        cfg->sampleRate = sampleRate;
        cfg->bitDepth = bitDepth;
    }
    return true;
}

bool HalCustomDevice::setVolume(uint8_t percent) {
    (void)percent;
    return true;
}

bool HalCustomDevice::setMute(bool mute) {
    (void)mute;
    return true;
}

// ===== hal_load_custom_devices =====

void hal_load_custom_devices() {
    HalDeviceManager& mgr = HalDeviceManager::instance();

    // Remove previously-loaded custom devices (HAL_DISC_MANUAL) to avoid duplicates
    for (uint8_t i = 0; i < HAL_MAX_DEVICES; i++) {
        HalDevice* dev = mgr.getDevice(i);
        if (dev && dev->getDiscovery() == HAL_DISC_MANUAL) {
            dev->deinit();
            mgr.removeDevice(i);
        }
    }

#ifndef NATIVE_TEST
    if (!LittleFS.exists("/hal/custom")) {
        LittleFS.mkdir("/hal/custom");
        return;
    }

    File dir = LittleFS.open("/hal/custom");
    if (!dir || !dir.isDirectory()) return;

    File f = dir.openNextFile();
    while (f) {
        if (!f.isDirectory()) {
            String content = f.readString();
            JsonDocument doc;
            if (deserializeJson(doc, content) != DeserializationError::Ok) {
                LOG_W("[HAL Custom]", "Bad JSON in %s — skipping", f.name());
                f = dir.openNextFile();
                continue;
            }

            const char* compatible = doc["compatible"] | "";
            const char* name = doc["name"] | compatible;

            if (strlen(compatible) == 0) {
                LOG_W("[HAL Custom]", "Schema missing 'compatible' field — skipping");
                f = dir.openNextFile();
                continue;
            }

            // Skip if already registered as a builtin
            if (mgr.findByCompatible(compatible)) {
                LOG_I("[HAL Custom]", "Already registered: %s — skipping", compatible);
                f = dir.openNextFile();
                continue;
            }

            // Determine capabilities from JSON array
            uint8_t caps = 0;
            JsonArray capArr = doc["capabilities"].as<JsonArray>();
            for (const char* cap : capArr) {
                if (strcmp(cap, "volume_control") == 0) caps |= HAL_CAP_HW_VOLUME;
                if (strcmp(cap, "mute") == 0)           caps |= HAL_CAP_MUTE;
                if (strcmp(cap, "adc_path") == 0)       caps |= HAL_CAP_ADC_PATH;
                if (strcmp(cap, "dac_path") == 0)       caps |= HAL_CAP_DAC_PATH;
            }

            // Determine bus type
            const char* busType = doc["bus"] | "i2s";
            uint8_t busIdx = (strcmp(busType, "i2c") == 0) ? HAL_BUS_I2C : HAL_BUS_I2S;

            HalCustomDevice* dev = new HalCustomDevice(compatible, name, caps, busIdx);
            if (!dev) {
                LOG_E("[HAL Custom]", "Out of memory allocating device: %s", compatible);
                f = dir.openNextFile();
                continue;
            }

            int slot = mgr.registerDevice(dev, HAL_DISC_MANUAL);
            if (slot < 0) {
                LOG_E("[HAL Custom]", "No free slots for device: %s", compatible);
                delete dev;
                f = dir.openNextFile();
                continue;
            }

            // Apply defaults from schema to config
            HalDeviceConfig* cfg = mgr.getConfig((uint8_t)slot);
            if (cfg) {
                cfg->valid        = true;
                cfg->sampleRate   = doc["defaults"]["sample_rate"] | 48000U;
                cfg->bitDepth     = doc["defaults"]["bits_per_sample"] | (uint8_t)16;
                cfg->mclkMultiple = doc["defaults"]["mclk_multiple"] | (uint16_t)256;
                cfg->enabled      = true;
                cfg->paControlPin = -1;
            }

            dev->init();
            LOG_I("[HAL Custom]", "Loaded custom device: %s (slot %d)", name, slot);
        }
        f = dir.openNextFile();
    }
#else
    LOG_I("[HAL Custom]", "NATIVE_TEST: skipping LittleFS scan");
#endif // NATIVE_TEST
}

#endif // DAC_ENABLED
