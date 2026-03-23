#pragma once
// health_check_api.h — REST API for firmware health check
//
// Unlike /api/diag/snapshot (raw data dump), /api/health returns pass/fail
// verdicts against defined thresholds for automated CI validation.
//
// GET /api/health — run (or return cached) full health check, JSON response
//
// Registered via registerHealthCheckApiEndpoints() from main.cpp.

#ifdef HEALTH_CHECK_ENABLED
void registerHealthCheckApiEndpoints();
#endif
