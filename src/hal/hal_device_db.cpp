#ifdef DAC_ENABLED

#include "hal_device_db.h"
#include <string.h>

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#else
#define LOG_I(tag, ...) ((void)0)
#define LOG_W(tag, ...) ((void)0)
#define LOG_E(tag, ...) ((void)0)
#endif

// ===== In-memory database =====
static HalDeviceDescriptor _db[HAL_DB_MAX_ENTRIES];
static int _dbCount = 0;

// ===== Builtin entries (always available, no LittleFS needed) =====
static void hal_db_add_builtins() {
    // PCM5102A
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "ti,pcm5102a", 31);
        strncpy(d.name, "PCM5102A", 32);
        strncpy(d.manufacturer, "Texas Instruments", 32);
        d.type = HAL_DEV_DAC;
        d.legacyId = 0x0001;
        d.channelCount = 2;
        d.bus.type = HAL_BUS_I2S;
        d.bus.index = 0;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K;
        d.capabilities = 0;
        hal_db_add(&d);
    }
    // ES8311
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "evergrande,es8311", 31);
        strncpy(d.name, "ES8311", 32);
        strncpy(d.manufacturer, "Evergrande", 32);
        d.type = HAL_DEV_CODEC;
        d.legacyId = 0x0004;
        d.channelCount = 1;
        d.i2cAddr = 0x18;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_ONBOARD;
        d.sampleRatesMask = HAL_RATE_8K | HAL_RATE_16K | HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K;
        d.capabilities = HAL_CAP_HW_VOLUME | HAL_CAP_MUTE | HAL_CAP_ADC_PATH | HAL_CAP_DAC_PATH;
        hal_db_add(&d);
    }
    // PCM1808
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "ti,pcm1808", 31);
        strncpy(d.name, "PCM1808", 32);
        strncpy(d.manufacturer, "Texas Instruments", 32);
        d.type = HAL_DEV_ADC;
        d.legacyId = 0;
        d.channelCount = 2;
        d.bus.type = HAL_BUS_I2S;
        d.bus.index = 0;
        d.sampleRatesMask = HAL_RATE_48K | HAL_RATE_96K;
        d.capabilities = 0;
        hal_db_add(&d);
    }
}

void hal_db_init() {
    _dbCount = 0;
    memset(_db, 0, sizeof(_db));
    hal_db_add_builtins();

#ifndef NATIVE_TEST
    // Load additional entries from LittleFS
    if (!LittleFS.exists(HAL_DB_FILE_PATH)) return;

    File f = LittleFS.open(HAL_DB_FILE_PATH, "r");
    if (!f) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        LOG_W("[HAL DB]", "Failed to parse %s: %s", HAL_DB_FILE_PATH, err.c_str());
        return;
    }

    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
        const char* compat = obj["compatible"] | "";
        if (hal_db_lookup(compat, nullptr)) continue;  // Skip if already a builtin

        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, compat, 31);
        strncpy(d.name, obj["name"] | "", 32);
        strncpy(d.manufacturer, obj["manufacturer"] | "", 32);
        d.type = static_cast<HalDeviceType>(obj["type"] | 0);
        d.legacyId = obj["legacyId"] | 0;
        d.i2cAddr = obj["i2cAddr"] | 0;
        d.channelCount = obj["channels"] | 2;
        d.sampleRatesMask = obj["ratesMask"] | 0;
        d.capabilities = obj["capabilities"] | 0;
        hal_db_add(&d);
    }
    LOG_I("[HAL DB]", "Loaded %d entries (including builtins)", _dbCount);
#endif
}

bool hal_db_lookup(const char* compatible, HalDeviceDescriptor* out) {
    if (!compatible) return false;
    for (int i = 0; i < _dbCount; i++) {
        if (strcmp(_db[i].compatible, compatible) == 0) {
            if (out) *out = _db[i];
            return true;
        }
    }
    return false;
}

bool hal_db_add(const HalDeviceDescriptor* desc) {
    if (!desc || desc->compatible[0] == '\0') return false;

    // Update existing entry
    for (int i = 0; i < _dbCount; i++) {
        if (strcmp(_db[i].compatible, desc->compatible) == 0) {
            _db[i] = *desc;
            return true;
        }
    }

    if (_dbCount >= HAL_DB_MAX_ENTRIES) return false;
    _db[_dbCount] = *desc;
    _dbCount++;
    return true;
}

bool hal_db_remove(const char* compatible) {
    if (!compatible) return false;
    for (int i = 0; i < _dbCount; i++) {
        if (strcmp(_db[i].compatible, compatible) == 0) {
            // Shift remaining entries
            for (int j = i; j < _dbCount - 1; j++) {
                _db[j] = _db[j + 1];
            }
            _dbCount--;
            return true;
        }
    }
    return false;
}

bool hal_db_save() {
#ifndef NATIVE_TEST
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (int i = 0; i < _dbCount; i++) {
        JsonObject obj = arr.add<JsonObject>();
        obj["compatible"] = _db[i].compatible;
        obj["name"] = _db[i].name;
        obj["manufacturer"] = _db[i].manufacturer;
        obj["type"] = _db[i].type;
        obj["legacyId"] = _db[i].legacyId;
        obj["i2cAddr"] = _db[i].i2cAddr;
        obj["channels"] = _db[i].channelCount;
        obj["ratesMask"] = _db[i].sampleRatesMask;
        obj["capabilities"] = _db[i].capabilities;
    }

    File f = LittleFS.open(HAL_DB_FILE_PATH, "w");
    if (!f) {
        LOG_E("[HAL DB]", "Failed to open %s for writing", HAL_DB_FILE_PATH);
        return false;
    }
    serializeJson(doc, f);
    f.close();
    LOG_I("[HAL DB]", "Saved %d entries to %s", _dbCount, HAL_DB_FILE_PATH);
    return true;
#else
    return true;
#endif
}

int hal_db_count() { return _dbCount; }

const HalDeviceDescriptor* hal_db_get(int index) {
    if (index < 0 || index >= _dbCount) return nullptr;
    return &_db[index];
}

void hal_load_device_configs() {
#ifndef NATIVE_TEST
    if (!LittleFS.exists(HAL_CONFIG_FILE_PATH)) return;
    // Config loading happens in Phase 4 — placeholder
    LOG_I("[HAL DB]", "Device configs loaded from %s", HAL_CONFIG_FILE_PATH);
#endif
}

bool hal_save_device_config(uint8_t slot) {
#ifndef NATIVE_TEST
    (void)slot;
    // Config saving happens in Phase 4 — placeholder
    return true;
#else
    (void)slot;
    return true;
#endif
}

void hal_db_reset() {
    _dbCount = 0;
    memset(_db, 0, sizeof(_db));
}

#endif // DAC_ENABLED
