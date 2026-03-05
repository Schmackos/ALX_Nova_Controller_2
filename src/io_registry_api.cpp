#ifdef DAC_ENABLED

#include "io_registry_api.h"
#include "io_registry.h"
#include "app_state.h"
#include "auth_handler.h"
#include "debug_serial.h"
#include <ArduinoJson.h>
#include <WebServer.h>

extern WebServer server;
extern bool requireAuth();

void registerIoRegistryApiEndpoints() {
    // GET /api/io/registry — List all I/O devices
    server.on("/api/io/registry", HTTP_GET, []() {
        if (!requireAuth()) return;

        JsonDocument doc;
        doc["success"] = true;

        JsonArray outArr = doc["outputs"].to<JsonArray>();
        const IoOutputEntry* outputs = io_registry_get_outputs();
        for (int i = 0; i < IO_MAX_OUTPUTS; i++) {
            if (!outputs[i].active) continue;
            JsonObject o = outArr.add<JsonObject>();
            o["index"] = outputs[i].index;
            o["name"] = outputs[i].name;
            o["deviceId"] = outputs[i].deviceId;
            o["deviceType"] = outputs[i].deviceType;
            o["discovery"] = (uint8_t)outputs[i].discovery;
            o["i2sPort"] = outputs[i].i2sPort;
            o["channelCount"] = outputs[i].channelCount;
            o["firstChannel"] = outputs[i].firstOutputChannel;
            o["ready"] = outputs[i].ready;
        }

        JsonArray inArr = doc["inputs"].to<JsonArray>();
        const IoInputEntry* inputs = io_registry_get_inputs();
        for (int i = 0; i < IO_MAX_INPUTS; i++) {
            if (!inputs[i].active) continue;
            JsonObject o = inArr.add<JsonObject>();
            o["index"] = inputs[i].index;
            o["name"] = inputs[i].name;
            o["discovery"] = (uint8_t)inputs[i].discovery;
            o["i2sPort"] = inputs[i].i2sPort;
            o["channelCount"] = inputs[i].channelCount;
            o["firstChannel"] = inputs[i].firstInputChannel;
        }

        String json;
        serializeJson(doc, json);
        server.send(200, "application/json", json);
    });

    // POST /api/io/output — Add manual output
    server.on("/api/io/output", HTTP_POST, []() {
        if (!requireAuth()) return;

        if (!server.hasArg("plain")) {
            server.send(400, "application/json", "{\"success\":false,\"message\":\"No data\"}");
            return;
        }

        JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain"))) {
            server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
            return;
        }

        const char* name = doc["name"] | "Manual";
        uint8_t i2sPort = doc["i2sPort"] | 0;
        uint16_t deviceId = doc["deviceId"] | 0;
        uint8_t channelCount = doc["channelCount"] | 2;

        int slot = io_registry_add_output(name, i2sPort, deviceId, channelCount);
        if (slot < 0) {
            server.send(400, "application/json", "{\"success\":false,\"message\":\"No free output slot\"}");
            return;
        }

        io_registry_save();

        JsonDocument resp;
        resp["success"] = true;
        resp["slot"] = slot;
        String json;
        serializeJson(resp, json);
        server.send(200, "application/json", json);
    });

    // POST /api/io/output/remove — Remove manual output
    server.on("/api/io/output/remove", HTTP_POST, []() {
        if (!requireAuth()) return;

        if (!server.hasArg("plain")) {
            server.send(400, "application/json", "{\"success\":false,\"message\":\"No data\"}");
            return;
        }

        JsonDocument doc;
        if (deserializeJson(doc, server.arg("plain"))) {
            server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
            return;
        }

        int index = doc["index"] | -1;
        if (index < 0 || index >= IO_MAX_OUTPUTS) {
            server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid index\"}");
            return;
        }

        if (!io_registry_remove_output((uint8_t)index)) {
            server.send(400, "application/json", "{\"success\":false,\"message\":\"Cannot remove (builtin or inactive)\"}");
            return;
        }

        io_registry_save();
        server.send(200, "application/json", "{\"success\":true}");
    });

    // POST /api/io/scan — Return cached EEPROM data (no runtime re-scan)
    server.on("/api/io/scan", HTTP_POST, []() {
        if (!requireAuth()) return;

        JsonDocument doc;
        doc["success"] = true;
        doc["note"] = "Cached data from boot scan (no runtime re-scan due to GPIO 54 SDIO conflict)";

        AppState& as = AppState::getInstance();
        doc["scanned"] = as.eepromDiag.scanned;
        doc["found"] = as.eepromDiag.found;
        if (as.eepromDiag.found) {
            doc["deviceName"] = as.eepromDiag.deviceName;
            doc["deviceId"] = as.eepromDiag.deviceId;
            doc["manufacturer"] = as.eepromDiag.manufacturer;
            doc["eepromAddr"] = as.eepromDiag.eepromAddr;
        }

        String json;
        serializeJson(doc, json);
        server.send(200, "application/json", json);
    });

    LOG_I("[IO] API endpoints registered");
}

#endif // DAC_ENABLED
