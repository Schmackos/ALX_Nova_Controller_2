#ifdef DAC_ENABLED

#include "hal_device_db.h"
#include "hal_device_manager.h"
#include "hal_driver_registry.h"
#include "hal_pipeline_bridge.h"
#include <string.h>

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#include "../diag_journal.h"
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
    // ES8311 (canonical compatible string: everest-semi,es8311)
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "everest-semi,es8311", 31);
        strncpy(d.name, "ES8311", 32);
        strncpy(d.manufacturer, "Everest Semiconductor", 32);
        d.type = HAL_DEV_CODEC;
        d.legacyId = 0x0004;
        d.channelCount = 2;
        d.i2cAddr = 0x18;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_ONBOARD;
        d.sampleRatesMask = HAL_RATE_8K | HAL_RATE_16K | HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K;
        d.capabilities = HAL_CAP_CODEC | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE |
                         HAL_CAP_ADC_PATH | HAL_CAP_DAC_PATH;
        hal_db_add(&d);
    }
    // ES8311 legacy alias (evergrande) — kept for backward compatibility
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "evergrande,es8311", 31);
        strncpy(d.name, "ES8311", 32);
        strncpy(d.manufacturer, "Everest Semiconductor", 32);
        d.type = HAL_DEV_CODEC;
        d.legacyId = 0x0004;
        d.channelCount = 2;
        d.i2cAddr = 0x18;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_ONBOARD;
        d.sampleRatesMask = HAL_RATE_8K | HAL_RATE_16K | HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K;
        d.capabilities = HAL_CAP_CODEC | HAL_CAP_HW_VOLUME | HAL_CAP_MUTE |
                         HAL_CAP_ADC_PATH | HAL_CAP_DAC_PATH;
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
        d.capabilities = HAL_CAP_ADC_PATH;
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
        strncpy(d.compatible, "alx,signal-gen", 31);
        strncpy(d.name, "Signal Generator", 32);
        strncpy(d.manufacturer, "ALX", 32);
        d.type = HAL_DEV_ADC;
        d.channelCount = 2;
        d.bus.type = HAL_BUS_INTERNAL;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K;
        d.capabilities = HAL_CAP_ADC_PATH;
        hal_db_add(&d);
    }
    // USB Audio
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "alx,usb-audio", 31);
        strncpy(d.name, "USB Audio", 32);
        strncpy(d.manufacturer, "ALX", 32);
        d.type = HAL_DEV_ADC;
        d.channelCount = 2;
        d.bus.type = HAL_BUS_INTERNAL;
        d.sampleRatesMask = HAL_RATE_44K1 | HAL_RATE_48K | HAL_RATE_96K;
        d.capabilities = HAL_CAP_ADC_PATH;
        hal_db_add(&d);
    }
    // MCP4725 — 12-bit I2C voltage output DAC (add-on module)
    {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        strncpy(d.compatible, "microchip,mcp4725", 31);
        strncpy(d.name, "MCP4725", 32);
        strncpy(d.manufacturer, "Microchip Technology", 32);
        d.type = HAL_DEV_DAC;
        d.channelCount = 1;
        d.i2cAddr = 0x60;
        d.bus.type = HAL_BUS_I2C;
        d.bus.index = HAL_I2C_BUS_EXP;  // GPIO 28/29 expansion bus
        d.sampleRatesMask = 0;
        d.capabilities = HAL_CAP_HW_VOLUME;
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
        LOG_W("[HAL:DB]", "Failed to parse %s: %s", HAL_DB_FILE_PATH, err.c_str());
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
    LOG_I("[HAL:DB]", "Loaded %d entries (including builtins)", _dbCount);
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

    if (_dbCount >= HAL_DB_MAX_ENTRIES) {
        LOG_W("[HAL DB] Device DB full (%d/%d): %s", _dbCount, HAL_DB_MAX_ENTRIES, desc->compatible);
#ifndef NATIVE_TEST
        diag_emit(DIAG_HAL_DB_FULL, DIAG_SEV_ERROR, 0, desc->compatible, "DB full");
#endif
        return false;
    }
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
        LOG_E("[HAL:DB]", "Failed to open %s for writing", HAL_DB_FILE_PATH);
        return false;
    }
    serializeJson(doc, f);
    f.close();
    LOG_I("[HAL:DB]", "Saved %d entries to %s", _dbCount, HAL_DB_FILE_PATH);
    return true;
#else
    return true;
#endif
}

int hal_db_count() { return _dbCount; }

int hal_db_max() { return HAL_DB_MAX_ENTRIES; }

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
        LOG_W("[HAL:DB]", "Failed to parse %s: %s", HAL_CONFIG_FILE_PATH, err.c_str());
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
        cfg.gpioA = obj["gpioA"] | (int)-1;
        cfg.gpioB = obj["gpioB"] | (int)-1;
        cfg.gpioC = obj["gpioC"] | (int)-1;
        cfg.gpioD = obj["gpioD"] | (int)-1;
        cfg.usbPid = obj["usbPid"] | (int)0;
        cfg.filterMode = obj["filterMode"] | 0;
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
    LOG_I("[HAL:DB]", "Device configs loaded from %s", HAL_CONFIG_FILE_PATH);
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
        if (cfg->gpioA >= 0) obj["gpioA"] = cfg->gpioA;
        if (cfg->gpioB >= 0) obj["gpioB"] = cfg->gpioB;
        if (cfg->gpioC >= 0) obj["gpioC"] = cfg->gpioC;
        if (cfg->gpioD >= 0) obj["gpioD"] = cfg->gpioD;
        if (cfg->usbPid != 0) obj["usbPid"] = cfg->usbPid;
        if (cfg->filterMode != 0) obj["filterMode"] = cfg->filterMode;
    }

    File f = LittleFS.open(HAL_CONFIG_FILE_PATH, "w");
    if (!f) {
        LOG_E("[HAL:DB]", "Failed to open %s for writing", HAL_CONFIG_FILE_PATH);
        return false;
    }
    serializeJson(doc, f);
    f.close();
    LOG_I("[HAL:DB]", "Saved device configs to %s", HAL_CONFIG_FILE_PATH);
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

// ===== Auto-provisioning =====

void hal_provision_defaults() {
#ifndef NATIVE_TEST
    if (LittleFS.exists("/hal_auto_devices.json")) return;

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    // PCM5102A DAC — I2S TX (DOUT=24, shared BCK/LRC/MCLK with ADCs)
    {
        JsonObject o = arr.add<JsonObject>();
        o["compatible"] = "ti,pcm5102a";
        o["label"]      = "PCM5102A DAC";
        o["i2sPort"]    = 0;
        o["sampleRate"] = 48000;
        o["bitDepth"]   = 32;
        o["pinData"]    = 24;    // I2S_TX_DATA_PIN
        o["pinBck"]     = 20;    // I2S_BCK_PIN (shared)
        o["pinLrc"]     = 21;    // I2S_LRC_PIN (shared)
        o["pinMclk"]    = 22;    // I2S_MCLK_PIN (shared)
        o["pinFmt"]     = -1;
        o["probeOnly"]  = true;  // HAL state holder; audio pipeline sink via dac_output_init()
    }

    // PCM1808 ADC1 — I2S RX channel 0 (DIN=23), clock master
    {
        JsonObject o = arr.add<JsonObject>();
        o["compatible"]       = "ti,pcm1808";
        o["label"]            = "PCM1808 ADC1";
        o["i2sPort"]          = 0;
        o["sampleRate"]       = 48000;
        o["bitDepth"]         = 32;
        o["pinData"]          = 23;    // I2S_DOUT_PIN
        o["pinBck"]           = 20;
        o["pinLrc"]           = 21;
        o["pinMclk"]          = 22;
        o["pinFmt"]           = -1;
        o["isI2sClockMaster"] = true;  // ADC1 outputs MCLK/BCK/WS
        o["probeOnly"]        = false;
    }

    // PCM1808 ADC2 — I2S RX channel 1 (DIN=25, shared clocks, data-only)
    {
        JsonObject o = arr.add<JsonObject>();
        o["compatible"]       = "ti,pcm1808";
        o["label"]            = "PCM1808 ADC2";
        o["i2sPort"]          = 1;
        o["sampleRate"]       = 48000;
        o["bitDepth"]         = 32;
        o["pinData"]          = 25;    // I2S_DOUT2_PIN
        o["pinBck"]           = 20;
        o["pinLrc"]           = 21;
        o["pinMclk"]          = 22;
        o["pinFmt"]           = -1;
        o["isI2sClockMaster"] = false; // ADC2 receives clocks only (data-only)
        o["probeOnly"]        = false;
    }

    File f = LittleFS.open("/hal_auto_devices.json", "w");
    if (!f) {
        LOG_E("[HAL:DB] Cannot create /hal_auto_devices.json");
        return;
    }
    serializeJson(doc, f);
    f.close();
    LOG_I("[HAL:DB] Default auto-devices written to /hal_auto_devices.json");
#endif
}

void hal_load_auto_devices() {
#ifndef NATIVE_TEST
    if (!LittleFS.exists("/hal_auto_devices.json")) return;

    File f = LittleFS.open("/hal_auto_devices.json", "r");
    if (!f) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        LOG_E("[HAL:DB] Parse error in /hal_auto_devices.json: %s", err.c_str());
        return;
    }

    HalDeviceManager& mgr = HalDeviceManager::instance();

    for (JsonObject obj : doc.as<JsonArray>()) {
        const char* compatible = obj["compatible"] | "";
        if (!compatible[0]) continue;

        // Skip if already registered (e.g. by dac_hal.cpp or a previous call)
        if (mgr.findByCompatible(compatible) != nullptr) {
            LOG_I("[HAL:DB] '%s' already registered — skip", compatible);
            continue;
        }

        // Look up driver factory
        const HalDriverEntry* entry = hal_registry_find(compatible);
        if (!entry || !entry->factory) {
            LOG_W("[HAL:DB]No factory for '%s' — skip", compatible);
            continue;
        }

        HalDevice* dev = entry->factory();
        int slot = mgr.registerDevice(dev, HAL_DISC_MANUAL);
        if (slot < 0) {
            delete dev;
            LOG_W("[HAL:DB]No free slot for '%s' — skip", compatible);
            continue;
        }

        // Apply config from JSON (pin overrides, sample rate, label)
        HalDeviceConfig cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.valid        = true;
        cfg.enabled      = obj["enabled"]    | true;
        cfg.i2sPort      = obj["i2sPort"]    | (uint8_t)255;
        cfg.sampleRate   = obj["sampleRate"] | (uint32_t)0;
        cfg.bitDepth     = obj["bitDepth"]   | (uint8_t)0;
        cfg.pinData      = obj["pinData"]    | (int8_t)-1;
        cfg.pinBck       = obj["pinBck"]     | (int8_t)-1;
        cfg.pinLrc       = obj["pinLrc"]     | (int8_t)-1;
        cfg.pinMclk      = obj["pinMclk"]    | (int8_t)-1;
        cfg.pinFmt       = obj["pinFmt"]     | (int8_t)-1;
        cfg.pinSda       = -1;
        cfg.pinScl       = -1;
        cfg.paControlPin = -1;
        cfg.volume       = 100;
        const char* label = obj["label"] | "";
        strncpy(cfg.userLabel, label, 32);
        cfg.userLabel[32] = '\0';
        mgr.setConfig(slot, cfg);

        // probeOnly=true  — device is a config/state holder; full init done elsewhere
        //   (e.g. PCM5102A: audio pipeline sink registered by dac_output_init())
        // probeOnly=false — probe + init now
        //   (e.g. PCM1808: I2S RX managed by i2s_audio.cpp; HAL just tracks state)
        bool probeOnly = obj["probeOnly"] | false;
        dev->probe();
        if (!probeOnly) {
            dev->init();
            hal_pipeline_on_device_available(slot);
        }

        LOG_I("[HAL:DB]Auto-device '%s' (%s) slot %d%s",
              compatible, label[0] ? label : "?", slot,
              probeOnly ? " [probe-only]" : " [ready]");
    }
#endif
}

#endif // DAC_ENABLED
