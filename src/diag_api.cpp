#ifndef NATIVE_TEST

#include "diag_api.h"
#include "http_security.h"
#include "rate_limiter.h"
#include "diag_journal.h"
#include "diag_event.h"
#include "diag_error_codes.h"
#include "heap_budget.h"
#include "app_state.h"
#include "globals.h"
#include "auth_handler.h"
#include "settings_manager.h"
#ifdef DAC_ENABLED
#include "hal/hal_device_manager.h"
#endif
#include <ArduinoJson.h>

extern bool requireAuth();

void registerDiagApiEndpoints() {
  // GET /api/diagnostics — full diagnostics export (delegates to settings_manager)
  server_on_versioned("/api/diagnostics", HTTP_GET, []() {
    if (!requireAuth()) return;
    if (!rate_limit_check((uint32_t)server.client().remoteIP())) {
        server_send(429, "application/json", "{\"error\":\"Rate limit exceeded\"}");
        return;
    }
    handleDiagnostics();
  });

  // GET /api/diagnostics/journal — diagnostic journal entries
  server_on_versioned("/api/diagnostics/journal", HTTP_GET, []() {
    if (!requireAuth()) return;
    JsonDocument doc;
    doc["type"] = "diagJournal";
    uint8_t count = diag_journal_count();
    JsonArray entries = doc["entries"].to<JsonArray>();
    for (uint8_t i = 0; i < count; i++) {
      DiagEvent ev;
      if (diag_journal_read(i, &ev)) {
        JsonObject e = entries.add<JsonObject>();
        e["seq"]   = ev.seq;
        e["boot"]  = ev.bootId;
        e["t"]     = ev.timestamp;
        e["heap"]  = ev.heapFree;
        char codeBuf[8];
        snprintf(codeBuf, sizeof(codeBuf), "0x%04X", ev.code);
        e["c"]     = codeBuf;
        e["corr"]  = ev.corrId;
        e["sub"]   = diag_subsystem_name(diag_subsystem_from_code((DiagErrorCode)ev.code));
        e["dev"]   = ev.device;
        e["slot"]  = ev.slot;
        e["msg"]   = ev.message;
        e["sev"]   = diag_severity_char((DiagSeverity)ev.severity);
        e["retry"] = ev.retryCount;
      }
    }
    doc["count"] = count;
    String json;
    serializeJson(doc, json);
    server_send(200, "application/json", json);
  });

  // DELETE /api/diagnostics/journal — clear journal
  server_on_versioned("/api/diagnostics/journal", HTTP_DELETE, []() {
    if (!requireAuth()) return;
    diag_journal_clear();
    server_send(200, "application/json", "{\"success\":true}");
  });

  // GET /api/diag/snapshot — compact diagnostic snapshot for AI debugging
  server_on_versioned("/api/diag/snapshot", HTTP_GET, []() {
    if (!requireAuth()) return;
    JsonDocument doc;
    doc["type"] = "diagSnapshot";
    doc["timestamp"] = millis();
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["freePsram"] = ESP.getFreePsram();
    doc["maxAllocHeap"] = ESP.getMaxAllocHeap();
    doc["heapBudgetPsram"] = heap_budget_total_psram();
    doc["heapBudgetSram"]  = heap_budget_total_sram();
    doc["fsmState"] = (int)appState.fsmState;
    // HAL devices
#ifdef DAC_ENABLED
    {
      JsonArray devs = doc["halDevices"].to<JsonArray>();
      HalDeviceManager::instance().forEach([](HalDevice* dev, void* ctx) {
        JsonArray* arr = static_cast<JsonArray*>(ctx);
        JsonObject d = arr->add<JsonObject>();
        d["slot"]       = dev->getSlot();
        d["name"]       = dev->getDescriptor().name;
        d["compatible"] = dev->getDescriptor().compatible;
        d["type"]       = (int)dev->getType();
        d["state"]      = (int)dev->_state;
        d["ready"]      = dev->_ready;
        const HalRetryState* rs = HalDeviceManager::instance().getRetryState(dev->getSlot());
        if (rs) {
          d["retries"]  = rs->count;
          d["lastErr"]  = rs->lastErrorCode;
        }
        d["faults"]     = HalDeviceManager::instance().getFaultCount(dev->getSlot());
      }, &devs);
    }
#endif
    // Recent diag events
    {
      JsonArray events = doc["recentEvents"].to<JsonArray>();
      uint8_t count = diag_journal_count();
      uint8_t limit = count < 10 ? count : 10;
      for (uint8_t i = 0; i < limit; i++) {
        DiagEvent ev;
        if (diag_journal_read(i, &ev)) {
          JsonObject e = events.add<JsonObject>();
          e["seq"]  = ev.seq;
          e["t"]    = ev.timestamp;
          char codeBuf[8];
          snprintf(codeBuf, sizeof(codeBuf), "0x%04X", ev.code);
          e["c"]    = codeBuf;
          e["dev"]  = ev.device;
          e["msg"]  = ev.message;
          e["sev"]  = diag_severity_char((DiagSeverity)ev.severity);
        }
      }
    }
    String json;
    serializeJson(doc, json);
    server_send(200, "application/json", json);
  });
}

#endif // NATIVE_TEST
