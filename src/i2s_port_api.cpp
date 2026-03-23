#ifndef NATIVE_TEST

#include "i2s_port_api.h"
#include <WebServer.h>
#include <ArduinoJson.h>
#include "i2s_audio.h"
#include "debug_serial.h"
#include "http_security.h"
#include "rate_limiter.h"

// Helper: build JSON object for a single I2S port
static void _buildPortJson(JsonObject& obj, uint8_t port) {
    I2sPortInfo info = i2s_port_get_info(port);
    obj["id"] = info.port;

    JsonObject tx = obj["tx"].to<JsonObject>();
    tx["active"] = info.txActive;
    tx["mode"] = (info.txMode == I2S_MODE_STD) ? "std" :
                 (info.txMode == I2S_MODE_TDM) ? "tdm" : "off";
    if (info.txMode == I2S_MODE_TDM) tx["tdmSlots"] = info.txTdmSlots;
    tx["doutPin"] = info.txDoutPin;
    tx["format"] = (info.txFormat == 1) ? "msb" :
                   (info.txFormat == 2) ? "pcm" : "philips";
    tx["bitDepth"] = info.txBitDepth;

    JsonObject rx = obj["rx"].to<JsonObject>();
    rx["active"] = info.rxActive;
    rx["mode"] = (info.rxMode == I2S_MODE_STD) ? "std" :
                 (info.rxMode == I2S_MODE_TDM) ? "tdm" : "off";
    if (info.rxMode == I2S_MODE_TDM) rx["tdmSlots"] = info.rxTdmSlots;
    rx["dinPin"] = info.rxDinPin;
    rx["format"] = (info.rxFormat == 1) ? "msb" :
                   (info.rxFormat == 2) ? "pcm" : "philips";
    rx["bitDepth"] = info.rxBitDepth;

    JsonObject clocks = obj["clocks"].to<JsonObject>();
    clocks["master"] = info.isClockMaster;
    clocks["mclk"] = info.mclkPin;
    clocks["bck"] = info.bckPin;
    clocks["lrc"] = info.lrcPin;
    clocks["mclkMultiple"] = info.mclkMultiple;
}

void registerI2sPortApiEndpoints(WebServer& server) {
    // GET /api/i2s/ports — list all I2S port status (or single port with ?id=N)
    server.on("/api/i2s/ports", HTTP_GET, [&server]() {
        if (!rate_limit_check((uint32_t)server.client().remoteIP())) {
            server_send(429, "application/json", "{\"error\":\"Rate limit exceeded\"}");
            return;
        }
        // Single-port query
        if (server.hasArg("id")) {
            int id = server.arg("id").toInt();
            if (id < 0 || id >= I2S_PORT_COUNT) {
                server_send(400, "application/json", "{\"error\":\"Invalid port id\"}");
                return;
            }
            JsonDocument doc;
            JsonObject port = doc.to<JsonObject>();
            _buildPortJson(port, (uint8_t)id);
            String json;
            serializeJson(doc, json);
            server_send(200, "application/json", json);
            return;
        }

        // All ports
        JsonDocument doc;
        JsonArray ports = doc["ports"].to<JsonArray>();
        for (uint8_t i = 0; i < I2S_PORT_COUNT; i++) {
            JsonObject port = ports.add<JsonObject>();
            _buildPortJson(port, i);
        }
        doc["sampleRate"] = i2s_audio_get_sample_rate();
        String json;
        serializeJson(doc, json);
        server_send(200, "application/json", json);
    });

    LOG_I("[I2S API] Endpoints registered");
}

#endif // NATIVE_TEST
