#ifdef DAC_ENABLED

#include "io_registry.h"
#include "app_state.h"
#include "dac_hal.h"
#include "dac_eeprom.h"
#include "debug_serial.h"
#include <string.h>

#ifndef NATIVE_TEST
#include <ArduinoJson.h>
#include <LittleFS.h>
#endif

static IoOutputEntry _outputs[IO_MAX_OUTPUTS];
static IoInputEntry  _inputs[IO_MAX_INPUTS];

// ===== Helper: find first free output slot =====
static int find_free_output_slot() {
    for (int i = 0; i < IO_MAX_OUTPUTS; i++) {
        if (!_outputs[i].active) return i;
    }
    return -1;
}

// ===== Register a builtin output =====
static void register_builtin_output(uint8_t slot, uint16_t deviceId, const char* name,
                                     uint8_t deviceType, uint8_t i2sPort, uint8_t channelCount) {
    if (slot >= IO_MAX_OUTPUTS) return;
    IoOutputEntry& e = _outputs[slot];
    e.active = true;
    e.index = slot;
    e.deviceId = deviceId;
    strncpy(e.name, name, 32);
    e.name[32] = '\0';
    e.deviceType = deviceType;
    e.discovery = IO_DISC_BUILTIN;
    e.i2sPort = i2sPort;
    e.channelCount = channelCount;
    e.firstOutputChannel = slot * 2;
    e.ready = false;  // Updated later when driver reports ready
}

// ===== Register a builtin input =====
static void register_builtin_input(uint8_t slot, const char* name,
                                    uint8_t i2sPort, uint8_t channelCount) {
    if (slot >= IO_MAX_INPUTS) return;
    IoInputEntry& e = _inputs[slot];
    e.active = true;
    e.index = slot;
    strncpy(e.name, name, 32);
    e.name[32] = '\0';
    e.discovery = IO_DISC_BUILTIN;
    e.i2sPort = i2sPort;
    e.channelCount = channelCount;
    e.firstInputChannel = slot * 2;
}

// ===== Check EEPROM cache and update registry =====
static void check_eeprom_cache() {
    AppState& as = AppState::getInstance();
    if (!as.eepromDiag.scanned || !as.eepromDiag.found) return;

    // If EEPROM device matches slot 0's device ID, upgrade discovery to EEPROM
    if (as.eepromDiag.deviceId == _outputs[0].deviceId) {
        _outputs[0].discovery = IO_DISC_EEPROM;
        // Update name from EEPROM if available
        if (as.eepromDiag.deviceName[0] != '\0') {
            strncpy(_outputs[0].name, as.eepromDiag.deviceName, 32);
            _outputs[0].name[32] = '\0';
        }
        LOG_I("[IO] Slot 0 upgraded to EEPROM discovery: %s", _outputs[0].name);
        return;
    }

    // Different device — register in next free slot
    int slot = find_free_output_slot();
    if (slot < 0) {
        LOG_W("[IO] No free output slot for EEPROM device (ID=0x%04X)", as.eepromDiag.deviceId);
        return;
    }

    IoOutputEntry& e = _outputs[slot];
    e.active = true;
    e.index = (uint8_t)slot;
    e.deviceId = as.eepromDiag.deviceId;
    strncpy(e.name, as.eepromDiag.deviceName, 32);
    e.name[32] = '\0';
    e.deviceType = 0;  // DAC default from EEPROM v1
    e.discovery = IO_DISC_EEPROM;
    e.i2sPort = 0;
    e.channelCount = as.eepromDiag.maxChannels > 0 ? as.eepromDiag.maxChannels : 2;
    e.firstOutputChannel = (uint8_t)(slot * 2);
    e.ready = false;
    LOG_I("[IO] EEPROM device registered in slot %d: %s (ID=0x%04X)", slot, e.name, e.deviceId);
}

void io_registry_init() {
    memset(_outputs, 0, sizeof(_outputs));
    memset(_inputs, 0, sizeof(_inputs));

    // ===== Builtin outputs (P4) =====
    AppState& as = AppState::getInstance();

    // Slot 0: Primary DAC — use dacModelName if available, else "PCM5102A"
    register_builtin_output(0, as.dacDeviceId, as.dacModelName, 0 /*DAC*/, 0 /*I2S0*/, 2);
    _outputs[0].ready = as.dacReady;

    // Slot 1: ES8311 (onboard codec + NS4150B speaker amp)
    register_builtin_output(1, 0x0004 /*DAC_ID_ES8311*/, "ES8311", 2 /*CODEC*/, 2 /*I2S2*/, 2);
    _outputs[1].ready = as.es8311Ready;

    // ===== Builtin inputs (P4) =====
    register_builtin_input(0, "ADC1 (PCM1808)", 0 /*I2S0*/, 2);
    register_builtin_input(1, "ADC2 (PCM1808)", 1 /*I2S1*/, 2);

    // ===== Check EEPROM cache =====
    check_eeprom_cache();

    // ===== Load manual entries from LittleFS =====
    io_registry_load();

    int oc = io_registry_output_count();
    int ic = io_registry_input_count();
    LOG_I("[IO] Registry initialized: %d output%s, %d input%s", oc, oc == 1 ? "" : "s", ic, ic == 1 ? "" : "s");
}

const IoOutputEntry* io_registry_get_outputs() { return _outputs; }
const IoInputEntry*  io_registry_get_inputs()  { return _inputs; }

int io_registry_output_count() {
    int count = 0;
    for (int i = 0; i < IO_MAX_OUTPUTS; i++) {
        if (_outputs[i].active) count++;
    }
    return count;
}

int io_registry_input_count() {
    int count = 0;
    for (int i = 0; i < IO_MAX_INPUTS; i++) {
        if (_inputs[i].active) count++;
    }
    return count;
}

int io_registry_add_output(const char* name, uint8_t i2sPort, uint16_t deviceId, uint8_t channelCount) {
    int slot = find_free_output_slot();
    if (slot < 0) return -1;

    IoOutputEntry& e = _outputs[slot];
    e.active = true;
    e.index = (uint8_t)slot;
    e.deviceId = deviceId;
    strncpy(e.name, name ? name : "Manual", 32);
    e.name[32] = '\0';
    e.deviceType = 0;  // DAC
    e.discovery = IO_DISC_MANUAL;
    e.i2sPort = i2sPort;
    e.channelCount = channelCount > 0 ? channelCount : 2;
    e.firstOutputChannel = (uint8_t)(slot * 2);
    e.ready = false;

    AppState::getInstance().markIoRegistryDirty();
    LOG_I("[IO] Manual output added in slot %d: %s", slot, e.name);
    return slot;
}

bool io_registry_remove_output(uint8_t index) {
    if (index >= IO_MAX_OUTPUTS) return false;
    if (!_outputs[index].active) return false;
    if (_outputs[index].discovery != IO_DISC_MANUAL) {
        LOG_W("[IO] Cannot remove non-manual output in slot %d (discovery=%d)", index, _outputs[index].discovery);
        return false;
    }

    memset(&_outputs[index], 0, sizeof(IoOutputEntry));
    AppState::getInstance().markIoRegistryDirty();
    LOG_I("[IO] Output removed from slot %d", index);
    return true;
}

// ===== Persistence (LittleFS JSON) =====

void io_registry_save() {
#ifndef NATIVE_TEST
    JsonDocument doc;
    JsonArray arr = doc["manualOutputs"].to<JsonArray>();

    for (int i = 0; i < IO_MAX_OUTPUTS; i++) {
        if (_outputs[i].active && _outputs[i].discovery == IO_DISC_MANUAL) {
            JsonObject obj = arr.add<JsonObject>();
            obj["slot"] = i;
            obj["name"] = _outputs[i].name;
            obj["i2sPort"] = _outputs[i].i2sPort;
            obj["deviceId"] = _outputs[i].deviceId;
            obj["channelCount"] = _outputs[i].channelCount;
        }
    }

    File f = LittleFS.open("/io_registry.json", "w");
    if (!f) {
        LOG_W("[IO] Failed to open /io_registry.json for writing");
        return;
    }
    serializeJson(doc, f);
    f.close();
    LOG_I("[IO] Registry saved to /io_registry.json");
#endif
}

void io_registry_load() {
#ifndef NATIVE_TEST
    if (!LittleFS.exists("/io_registry.json")) return;

    File f = LittleFS.open("/io_registry.json", "r");
    if (!f) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        LOG_W("[IO] Failed to parse /io_registry.json: %s", err.c_str());
        return;
    }

    JsonArray arr = doc["manualOutputs"].as<JsonArray>();
    for (JsonObject obj : arr) {
        int slot = obj["slot"] | -1;
        if (slot < 0 || slot >= IO_MAX_OUTPUTS) continue;
        if (_outputs[slot].active) continue;  // Don't overwrite existing

        IoOutputEntry& e = _outputs[slot];
        e.active = true;
        e.index = (uint8_t)slot;
        const char* name = obj["name"] | "Manual";
        strncpy(e.name, name, 32);
        e.name[32] = '\0';
        e.i2sPort = obj["i2sPort"] | 0;
        e.deviceId = obj["deviceId"] | 0;
        e.channelCount = obj["channelCount"] | 2;
        e.deviceType = 0;  // DAC
        e.discovery = IO_DISC_MANUAL;
        e.firstOutputChannel = (uint8_t)(slot * 2);
        e.ready = false;
        LOG_I("[IO] Loaded manual output in slot %d: %s", slot, e.name);
    }
#endif
}

#endif // DAC_ENABLED
