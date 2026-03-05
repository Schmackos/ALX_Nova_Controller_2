#ifdef DAC_ENABLED

#include "hal_device_db.h"
#include "hal_device_manager.h"
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
    // NS4150B
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "ns,ns4150b-amp", 31);
        strncpy(d.name, "NS4150B Amp", 32);
        strncpy(d.manufacturer, "Nsiway", 32);
        d.type = HAL_DEV_AMP;
        d.channelCount = 1;
        d.bus.type = HAL_BUS_GPIO;
        d.capabilities = 0;
        hal_db_add(&d);
    }
    // Chip Temperature Sensor
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "espressif,esp32p4-temp", 31);
        strncpy(d.name, "Chip Temperature", 32);
        strncpy(d.manufacturer, "Espressif", 32);
        d.type = HAL_DEV_SENSOR;
        d.channelCount = 1;
        d.bus.type = HAL_BUS_INTERNAL;
        d.capabilities = 0;
        hal_db_add(&d);
    }
    // ST7735S TFT Display
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "sitronix,st7735s", 31);
        strncpy(d.name, "ST7735S TFT", 32);
        strncpy(d.manufacturer, "Sitronix", 32);
        d.type = HAL_DEV_DISPLAY;
        d.channelCount = 1;
        d.bus.type = HAL_BUS_SPI;
        d.bus.pinA = 2;   // MOSI
        d.bus.pinB = 3;   // SCLK
        d.sampleRatesMask = 0;
        d.capabilities = 0;
        hal_db_add(&d);
    }
    // Rotary Encoder
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "alps,ec11", 31);
        strncpy(d.name, "Rotary Encoder", 32);
        strncpy(d.manufacturer, "Alps", 32);
        d.type = HAL_DEV_INPUT;
        d.channelCount = 3;
        d.bus.type = HAL_BUS_GPIO;
        d.sampleRatesMask = 0;
        d.capabilities = 0;
        hal_db_add(&d);
    }
    // Piezo Buzzer
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "generic,piezo-buzzer", 31);
        strncpy(d.name, "Piezo Buzzer", 32);
        strncpy(d.manufacturer, "Generic", 32);
        d.type = HAL_DEV_GPIO;
        d.channelCount = 1;
        d.bus.type = HAL_BUS_GPIO;
        d.sampleRatesMask = 0;
        d.capabilities = 0;
        hal_db_add(&d);
    }
    // Status LED
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "generic,status-led", 31);
        strncpy(d.name, "Status LED", 32);
        strncpy(d.manufacturer, "Generic", 32);
        d.type = HAL_DEV_GPIO;
        d.channelCount = 1;
        d.bus.type = HAL_BUS_GPIO;
        d.sampleRatesMask = 0;
        d.capabilities = 0;
        hal_db_add(&d);
    }
    // Amplifier Relay
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "generic,relay-amp", 31);
        strncpy(d.name, "Amplifier Relay", 32);
        strncpy(d.manufacturer, "Generic", 32);
        d.type = HAL_DEV_AMP;
        d.channelCount = 1;
        d.bus.type = HAL_BUS_GPIO;
        d.sampleRatesMask = 0;
        d.capabilities = 0;
        hal_db_add(&d);
    }
    // Reset Button
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "generic,tact-switch", 31);
        strncpy(d.name, "Reset Button", 32);
        strncpy(d.manufacturer, "Generic", 32);
        d.type = HAL_DEV_INPUT;
        d.channelCount = 1;
        d.bus.type = HAL_BUS_GPIO;
        d.sampleRatesMask = 0;
        d.capabilities = 0;
        hal_db_add(&d);
    }
    // Signal Generator
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "generic,signal-gen", 31);
        strncpy(d.name, "Signal Generator", 32);
        strncpy(d.manufacturer, "Generic", 32);
        d.type = HAL_DEV_GPIO;
        d.channelCount = 1;
        d.bus.type = HAL_BUS_GPIO;
        d.sampleRatesMask = 0;
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

    File f = LittleFS.open(HAL_CONFIG_FILE_PATH, "r");
    if (!f) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        LOG_W("[HAL DB]", "Failed to parse %s: %s", HAL_CONFIG_FILE_PATH, err.c_str());
        return;
    }

    JsonArray arr = doc.as<JsonArray>();
    HalDeviceManager& mgr = HalDeviceManager::instance();
    for (JsonObject obj : arr) {
        uint8_t slot = obj["slot"] | 255;
        if (slot >= HAL_MAX_DEVICES) continue;

        HalDeviceConfig cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.valid = true;
        cfg.i2cAddr = obj["i2cAddr"] | 0;
        cfg.i2cBusIndex = obj["i2cBus"] | 0;
        cfg.i2cSpeedHz = obj["i2cSpeed"] | 0;
        cfg.pinSda = obj["pinSda"] | -1;
        cfg.pinScl = obj["pinScl"] | -1;
        cfg.pinMclk = obj["pinMclk"] | -1;
        cfg.pinData = obj["pinData"] | -1;
        cfg.i2sPort = obj["i2sPort"] | 255;
        cfg.sampleRate = obj["sampleRate"] | 0;
        cfg.bitDepth = obj["bitDepth"] | 0;
        cfg.volume = obj["volume"] | 100;
        cfg.mute = obj["mute"] | false;
        cfg.enabled = obj["enabled"] | true;
        const char* label = obj["label"] | "";
        strncpy(cfg.userLabel, label, 32);
        cfg.userLabel[32] = '\0';

        mgr.setConfig(slot, cfg);
    }
    LOG_I("[HAL DB]", "Device configs loaded from %s", HAL_CONFIG_FILE_PATH);
#endif
}

bool hal_save_device_config(uint8_t slot) {
#ifndef NATIVE_TEST
    // Save ALL configs (simpler than surgical single-slot update)
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    HalDeviceManager& mgr = HalDeviceManager::instance();

    for (int i = 0; i < HAL_MAX_DEVICES; i++) {
        HalDeviceConfig* cfg = mgr.getConfig(i);
        if (!cfg || !cfg->valid) continue;

        JsonObject obj = arr.add<JsonObject>();
        obj["slot"] = i;
        obj["i2cAddr"] = cfg->i2cAddr;
        obj["i2cBus"] = cfg->i2cBusIndex;
        obj["i2cSpeed"] = cfg->i2cSpeedHz;
        obj["pinSda"] = cfg->pinSda;
        obj["pinScl"] = cfg->pinScl;
        obj["pinMclk"] = cfg->pinMclk;
        obj["pinData"] = cfg->pinData;
        obj["i2sPort"] = cfg->i2sPort;
        obj["sampleRate"] = cfg->sampleRate;
        obj["bitDepth"] = cfg->bitDepth;
        obj["volume"] = cfg->volume;
        obj["mute"] = cfg->mute;
        obj["enabled"] = cfg->enabled;
        if (cfg->userLabel[0]) obj["label"] = cfg->userLabel;
    }

    File f = LittleFS.open(HAL_CONFIG_FILE_PATH, "w");
    if (!f) {
        LOG_E("[HAL DB]", "Failed to open %s for writing", HAL_CONFIG_FILE_PATH);
        return false;
    }
    serializeJson(doc, f);
    f.close();
    LOG_I("[HAL DB]", "Saved device configs to %s", HAL_CONFIG_FILE_PATH);
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
