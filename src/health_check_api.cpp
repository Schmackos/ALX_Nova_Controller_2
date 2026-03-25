#ifndef NATIVE_TEST
// health_check_api.cpp — REST endpoint for firmware health check
//
// Unlike /api/diag/snapshot (raw data dump), /api/health returns pass/fail
// verdicts against defined thresholds for automated CI validation.
//
// GET /api/health:
//   Returns the most recent HealthCheckReport as structured JSON.
//   Uses a 5-second staleness cache; re-runs if stale or never run.
//   Auth-guarded, rate-limited, heapCritical-aware.

#ifdef HEALTH_CHECK_ENABLED

#include "health_check_api.h"
#include "health_check.h"
#include "http_security.h"
#include "rate_limiter.h"
#include "app_state.h"
#include "globals.h"
#include "auth_handler.h"
#include "debug_serial.h"
#include <ArduinoJson.h>

extern bool requireAuth();

// Map HealthCheckResult to string label
static const char* _result_str(HealthCheckResult r) {
    switch (r) {
        case HC_PASS: return "pass";
        case HC_WARN: return "warn";
        case HC_FAIL: return "fail";
        case HC_SKIP: return "skip";
        default:      return "unknown";
    }
}

void registerHealthCheckApiEndpoints() {
    // GET /api/health — run (or return cached) full health check
    server_on_versioned("/api/health", HTTP_GET, []() {
        if (!requireAuth()) return;
        if (!rate_limit_check((uint32_t)server.client().remoteIP())) {
            server_send(429, "application/json", "{\"error\":\"Rate limit exceeded\"}");
            return;
        }

        // Under heap pressure, return a minimal error to avoid OOM
        if (appState.debug.heapCritical) {
            server_send(503, "application/json",
                "{\"type\":\"healthCheck\",\"error\":\"heap critical\"}");
            return;
        }

        // Trigger a run (uses internal 5-second staleness guard)
        HealthCheckReport report;
        health_check_run_full(&report);

        JsonDocument doc;
        doc["type"]          = "healthCheck";
        doc["timestamp"]     = report.timestamp;
        doc["durationMs"]    = report.durationMs;
        doc["deferred"]      = report.deferredPhase;
        doc["total"]         = report.count;
        doc["passCount"]     = report.passCount;
        doc["warnCount"]     = report.warnCount;
        doc["failCount"]     = report.failCount;
        doc["skipCount"]     = report.skipCount;

        // Overall verdict: "pass" if no failures, "warn" if any warns, "fail" if any fails
        if (report.failCount > 0) {
            doc["verdict"] = "fail";
        } else if (report.warnCount > 0) {
            doc["verdict"] = "warn";
        } else {
            doc["verdict"] = "pass";
        }

        JsonArray checks = doc["checks"].to<JsonArray>();
        for (uint8_t i = 0; i < report.count; i++) {
            const HealthCheckItem& item = report.items[i];
            JsonObject obj = checks.add<JsonObject>();
            obj["id"]     = i;
            obj["name"]   = item.name;
            obj["status"] = _result_str(item.result);
            obj["detail"] = item.detail;
        }

        String json;
        serializeJson(doc, json);
        server_send(200, "application/json", json);
    });
}

#endif // HEALTH_CHECK_ENABLED
#endif // NATIVE_TEST
