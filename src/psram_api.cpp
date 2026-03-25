#ifndef NATIVE_TEST

#include "psram_api.h"
#include "http_security.h"
#include "psram_alloc.h"
#include "heap_budget.h"
#include "app_state.h"
#include "globals.h"
#include "auth_handler.h"
#include <ArduinoJson.h>

extern bool requireAuth();

void registerPsramApiEndpoints(WebServer &/*srv*/) {
    // GET /api/psram/status — PSRAM health and allocation stats
    server_on_versioned("/api/psram/status", HTTP_GET, []() {
        if (!requireAuth()) return;

        JsonDocument doc;

        // PSRAM total/free from ESP API
        uint32_t total = ESP.getPsramSize();
        uint32_t free_ = ESP.getFreePsram();
        doc["total"] = total;
        doc["free"]  = free_;
        if (total > 0) {
            doc["usagePercent"] = (float)(total - free_) * 100.0f / (float)total;
        } else {
            doc["usagePercent"] = 0;
        }

        // PSRAM allocation tracker stats
        PsramAllocStats ps = psram_get_stats();
        doc["fallbackCount"] = ps.fallbackCount;
        doc["failedCount"]   = ps.failedCount;
        doc["allocPsram"]    = ps.activePsramBytes;
        doc["allocSram"]     = ps.activeSramBytes;

        // Warning/critical flags
        auto& state = AppState::getInstance();
        doc["warning"]  = state.debug.psramWarning;
        doc["critical"] = state.debug.psramCritical;

        // Heap budget breakdown
        JsonArray budget = doc["budget"].to<JsonArray>();
        for (int i = 0; i < heap_budget_count(); i++) {
            const HeapBudgetEntry* e = heap_budget_entry(i);
            if (!e) continue;
            JsonObject entry = budget.add<JsonObject>();
            entry["label"] = e->label;
            entry["bytes"] = e->bytes;
            entry["psram"] = e->isPsram;
        }

        String json;
        serializeJson(doc, json);
        server_send(200, "application/json", json);
    });
}

#endif // NATIVE_TEST
