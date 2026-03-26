#include "routes.h"
#include "app_state.h"
#include "globals.h"
#include "auth_handler.h"
#include "debug_serial.h"
#include "login_page.h"
#include "ota_updater.h"
#include "settings_manager.h"
#include "smart_sensing.h"
#include "psram_api.h"
#include "diag_api.h"
#include "siggen_api.h"
#include "i2s_port_api.h"
#include "health_check_api.h"
#include "web_pages.h"
#include "http_security.h"
#include "mqtt_handler.h"
#include "wifi_manager.h"
#include "eth_manager.h"
#include "websocket_handler.h"
#include "audio_pipeline.h"
#ifdef DSP_ENABLED
#include "dsp_api.h"
#include "dsp_pipeline.h"
#endif
#ifdef DAC_ENABLED
#include "hal/hal_eeprom_api.h"
#include "hal/hal_api.h"
#include "pipeline_api.h"
#endif
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>

extern WebServer server;

void registerMainRoutes() {
  // Favicon (don't redirect/auth for this)
  server.on("/favicon.ico", HTTP_GET,
            []() { server_send(404, "text/plain", "Not Found"); });

  // Common browser auto-requests (reduce console noise)
  server.on("/manifest.json", HTTP_GET,
            []() { server_send(404, "text/plain", "Not Found"); });
  server.on("/robots.txt", HTTP_GET,
            []() { server_send(404, "text/plain", "Not Found"); });
  server.on("/sitemap.xml", HTTP_GET,
            []() { server_send(404, "text/plain", "Not Found"); });
  server.on("/apple-touch-icon.png", HTTP_GET,
            []() { server_send(404, "text/plain", "Not Found"); });
  server.on("/apple-touch-icon-precomposed.png", HTTP_GET,
            []() { server_send(404, "text/plain", "Not Found"); });

  // Android/Chrome captive portal check
  server.on("/generate_204", HTTP_GET, []() {
    server.sendHeader("Location", "/", true);
    server_send(302, "text/plain", "Redirecting...");
  });

  // Apple captive portal check
  server.on("/hotspot-detect.html", HTTP_GET, []() {
    server.sendHeader("Location", "/", true);
    server_send(302, "text/plain", "Redirecting...");
  });

  // Redirect all unknown routes to root in AP mode (Captive Portal)
  server.onNotFound([]() {
    // Log the request for debugging
    LOG_W("[Main] 404 Not Found: %s %s",
          server.method() == HTTP_GET ? "GET" :
          server.method() == HTTP_POST ? "POST" : "OTHER",
          server.uri().c_str());

    if (appState.wifi.isAPMode) {
      server.sendHeader("Location",
                        String("http://") + WiFi.softAPIP().toString() + "/",
                        true);
      server_send(302, "text/plain", "Redirecting to Captive Portal");
    } else {
      server_send(404, "application/json", "{\"error\":\"Not found\"}");
    }
  });

  // Authentication routes (unprotected)
  server.on("/login", HTTP_GET, []() {
    httpServingPage = true;
    if (!sendGzipped(server, loginPage_gz, loginPage_gz_len)) {
      server_send_P(200, "text/html", loginPage);
    }
    httpServingPage = false;
  });
  server_on_versioned("/api/auth/login", HTTP_POST, handleLogin);
  server_on_versioned("/api/auth/logout", HTTP_POST, handleLogout);
  server_on_versioned("/api/auth/status", HTTP_GET, handleAuthStatus);
  server_on_versioned("/api/auth/change", HTTP_POST, handlePasswordChange);
  server_on_versioned("/api/ws-token", HTTP_GET, []() {
    if (!requireAuth()) return;
    handleGetWsToken();
  });

  // Protected routes
  server.on("/", HTTP_GET, []() {
    if (!requireAuth())
      return;

    // Serve gzipped dashboard if client supports it (~85% smaller)
    httpServingPage = true;
    if (!sendGzipped(server, htmlPage_gz, htmlPage_gz_len)) {
      server_send_P(200, "text/html", htmlPage);
    }
    httpServingPage = false;
  });

  server_on_versioned("/api/ethstatus", HTTP_GET, []() {
    if (!requireAuth()) return;
    handleEthStatus();
  });
  server_on_versioned("/api/ethconfig", HTTP_POST, []() {
    if (!requireAuth()) return;
    handleEthConfig();
  });
  server_on_versioned("/api/ethconfig/confirm", HTTP_POST, []() {
    if (!requireAuth()) return;
    handleEthConfigConfirm();
  });
  server_on_versioned("/api/wificonfig", HTTP_POST, []() {
    if (!requireAuth()) return;
    handleWiFiConfig();
  });
  server_on_versioned("/api/wifisave", HTTP_POST, []() {
    if (!requireAuth()) return;
    handleWiFiSave();
  });
  server_on_versioned("/api/wifiscan", HTTP_GET, []() {
    if (!requireAuth()) return;
    handleWiFiScan();
  });
  server_on_versioned("/api/wifilist", HTTP_GET, []() {
    if (!requireAuth()) return;
    handleWiFiList();
  });
  server_on_versioned("/api/wifiremove", HTTP_POST, []() {
    if (!requireAuth()) return;
    handleWiFiRemove();
  });
  server_on_versioned("/api/apconfig", HTTP_POST, []() {
    if (!requireAuth()) return;
    handleAPConfigUpdate();
  });
  server_on_versioned("/api/toggleap", HTTP_POST, []() {
    if (!requireAuth()) return;
    handleAPToggle();
  });
  server_on_versioned("/api/wifistatus", HTTP_GET, []() {
    if (!requireAuth()) return;
    handleWiFiStatus();
  });
  server_on_versioned("/api/checkupdate", HTTP_GET, []() {
    if (!requireAuth()) return;
    handleCheckUpdate();
  });
  server_on_versioned("/api/startupdate", HTTP_POST, []() {
    if (!requireAuth()) return;
    handleStartUpdate();
  });
  server_on_versioned("/api/updatestatus", HTTP_GET, []() {
    if (!requireAuth()) return;
    handleUpdateStatus();
  });
  server_on_versioned("/api/releasenotes", HTTP_GET, []() {
    if (!requireAuth()) return;
    handleGetReleaseNotes();
  });
  server_on_versioned("/api/releases", HTTP_GET, []() {
    if (!requireAuth()) return;
    handleGetReleaseList();
  });
  server_on_versioned("/api/installrelease", HTTP_POST, []() {
    if (!requireAuth()) return;
    handleInstallRelease();
  });
  server_on_versioned("/api/settings", HTTP_GET, []() {
    if (!requireAuth()) return;
    handleSettingsGet();
  });
  server_on_versioned("/api/settings", HTTP_POST, []() {
    if (!requireAuth()) return;
    handleSettingsUpdate();
  });
  server_on_versioned("/api/settings/export", HTTP_GET, []() {
    if (!requireAuth()) return;
    handleSettingsExport();
  });
  server_on_versioned("/api/settings/import", HTTP_POST, []() {
    if (!requireAuth()) return;
    handleSettingsImport();
  });
  // Diagnostic endpoints registered in diag_api.cpp
  server_on_versioned("/api/factoryreset", HTTP_POST, []() {
    if (!requireAuth()) return;
    handleFactoryReset();
  });
  server_on_versioned("/api/reboot", HTTP_POST, []() {
    if (!requireAuth()) return;
    handleReboot();
  });
  server_on_versioned("/api/smartsensing", HTTP_GET, []() {
    if (!requireAuth()) return;
    handleSmartSensingGet();
  });
  server_on_versioned("/api/smartsensing", HTTP_POST, []() {
    if (!requireAuth()) return;
    handleSmartSensingUpdate();
  });
  server_on_versioned("/api/mqtt", HTTP_GET, []() {
    if (!requireAuth()) return;
    handleMqttGet();
  });
  server_on_versioned("/api/mqtt", HTTP_POST, []() {
    if (!requireAuth()) return;
    handleMqttUpdate();
  });
  // Note: /api/firmware/upload uses a two-handler overload — manually aliased
  server.on(
      "/api/firmware/upload", HTTP_POST,
      []() {
        if (!requireAuth()) return;
        handleFirmwareUploadComplete();
      },
      []() {
        if (!requireAuth()) return;
        handleFirmwareUploadChunk();
      });
  server.on(
      "/api/v1/firmware/upload", HTTP_POST,
      []() {
        if (!requireAuth()) return;
        handleFirmwareUploadComplete();
      },
      []() {
        if (!requireAuth()) return;
        handleFirmwareUploadChunk();
      });
  // Signal generator endpoints registered in siggen_api.cpp
  // Audio Pipeline Matrix API
  server_on_versioned("/api/pipeline/matrix", HTTP_GET, []() {
    if (!requireAuth())
      return;
    JsonDocument doc;
    doc["success"] = true;
    doc["bypass"]  = audio_pipeline_is_matrix_bypass();
    doc["size"]    = AUDIO_PIPELINE_MATRIX_SIZE;
    JsonArray matrix = doc["matrix"].to<JsonArray>();
    for (int o = 0; o < AUDIO_PIPELINE_MATRIX_SIZE; o++) {
      JsonArray row = matrix.add<JsonArray>();
      for (int i = 0; i < AUDIO_PIPELINE_MATRIX_SIZE; i++) {
        row.add(audio_pipeline_get_matrix_gain(o, i));
      }
    }
    String json;
    serializeJson(doc, json);
    server_send(200, "application/json", json);
  });
  server_on_versioned("/api/pipeline/matrix", HTTP_PUT, []() {
    if (!requireAuth())
      return;
    if (!server.hasArg("plain")) {
      server_send(400, "application/json",
                  "{\"success\":false,\"message\":\"No data\"}");
      return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, server.arg("plain"))) {
      server_send(400, "application/json",
                  "{\"success\":false,\"message\":\"Invalid JSON\"}");
      return;
    }
    // Set bypass mode
    if (doc["bypass"].is<bool>()) {
      audio_pipeline_bypass_matrix(doc["bypass"].as<bool>());
    }
    // Set single cell (linear gain)
    if (doc["cell"].is<JsonObject>()) {
      int out_ch   = doc["cell"]["out"].as<int>();
      int in_ch    = doc["cell"]["in"].as<int>();
      float gain   = doc["cell"]["gain"].as<float>();
      audio_pipeline_set_matrix_gain(out_ch, in_ch, gain);
    }
    // Set single cell (dB gain)
    if (doc["cell_db"].is<JsonObject>()) {
      int out_ch     = doc["cell_db"]["out"].as<int>();
      int in_ch      = doc["cell_db"]["in"].as<int>();
      float gain_db  = doc["cell_db"]["gain_db"].as<float>();
      audio_pipeline_set_matrix_gain_db(out_ch, in_ch, gain_db);
    }
    // Set full matrix (array of 8 rows, each with 8 linear gain values)
    if (doc["matrix"].is<JsonArray>()) {
      JsonArray rows = doc["matrix"].as<JsonArray>();
      int o = 0;
      for (JsonArray row : rows) {
        if (o >= AUDIO_PIPELINE_MATRIX_SIZE) break;
        int i = 0;
        for (float gain : row) {
          if (i >= AUDIO_PIPELINE_MATRIX_SIZE) break;
          audio_pipeline_set_matrix_gain(o, i, gain);
          i++;
        }
        o++;
      }
    }
    server_send(200, "application/json", "{\"success\":true}");
  });
  // Input Names API
  server_on_versioned("/api/inputnames", HTTP_GET, []() {
    if (!requireAuth())
      return;
    JsonDocument doc;
    doc["success"] = true;
    JsonArray names = doc["names"].to<JsonArray>();
    for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS * 2; i++) {
      names.add(appState.audio.inputNames[i]);
    }
    doc["numAdcsDetected"] = appState.audio.numAdcsDetected;
    String json;
    serializeJson(doc, json);
    server_send(200, "application/json", json);
  });
  server_on_versioned("/api/inputnames", HTTP_POST, []() {
    if (!requireAuth())
      return;
    if (!server.hasArg("plain")) {
      server_send(400, "application/json",
                  "{\"success\":false,\"message\":\"No data\"}");
      return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, server.arg("plain"))) {
      server_send(400, "application/json",
                  "{\"success\":false,\"message\":\"Invalid JSON\"}");
      return;
    }
    if (doc["names"].is<JsonArray>()) {
      JsonArray names = doc["names"].as<JsonArray>();
      for (int i = 0; i < AUDIO_PIPELINE_MAX_INPUTS * 2 && i < (int)names.size(); i++) {
        String name = names[i].as<String>();
        if (name.length() > 0) appState.audio.inputNames[i] = name;
      }
      saveInputNames();
    }
    server_send(200, "application/json", "{\"success\":true}");
  });

  // Pipeline format status — new v1 endpoint (also available on /api/v1/ prefix)
  // Returns per-lane sample rates, mismatch flag, DSD detection, and sink format info.
  server_on_versioned("/api/pipeline/status", HTTP_GET, []() {
    if (!requireAuth()) return;
    JsonDocument doc;
    doc["success"] = true;
    doc["rateMismatch"] = appState.audio.rateMismatch;
    JsonArray laneRates = doc["laneSampleRates"].to<JsonArray>();
    JsonArray laneDsd = doc["laneDsd"].to<JsonArray>();
    for (int lane = 0; lane < AUDIO_PIPELINE_MAX_INPUTS; lane++) {
      laneRates.add(appState.audio.laneSampleRates[lane]);
      laneDsd.add(appState.audio.laneDsd[lane]);
    }
    // Sink format summary
    JsonArray sinks = doc["sinks"].to<JsonArray>();
    int sinkCount = audio_pipeline_get_sink_count();
    for (int s = 0; s < sinkCount; s++) {
      const AudioOutputSink* sink = audio_pipeline_get_sink(s);
      if (!sink) continue;
      JsonObject obj = sinks.add<JsonObject>();
      obj["index"] = s;
      obj["name"] = sink->name ? sink->name : "";
      obj["sampleRate"] = sink->sampleRate;
      obj["sampleRatesMask"] = sink->sampleRatesMask;
      obj["bitDepth"] = sink->bitDepth;
      obj["maxBitDepth"] = sink->maxBitDepth;
      obj["supportsDsd"] = sink->supportsDsd;
    }
    String json;
    serializeJson(doc, json);
    server_send(200, "application/json", json);
  });

  // Note: Certificate API routes removed - now using Mozilla certificate bundle

#ifdef DSP_ENABLED
  // Register DSP API endpoints
  registerDspApiEndpoints();
#endif

#ifdef DAC_ENABLED
  // Register DAC REST API endpoints
  registerHalEepromApiEndpoints();
  registerHalApiEndpoints(server);
  registerPipelineApiEndpoints(server);
#endif

  // Register PSRAM health API endpoint
  registerPsramApiEndpoints(server);

  // Register extracted API modules
  registerDiagApiEndpoints();
  registerSignalGenApiEndpoints();

  // Register I2S port status API endpoint
  registerI2sPortApiEndpoints(server);

#ifdef HEALTH_CHECK_ENABLED
  // Register health check REST API endpoint
  registerHealthCheckApiEndpoints();
#endif
}
