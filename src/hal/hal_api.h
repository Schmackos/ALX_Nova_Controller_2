#pragma once
// HAL REST API — HTTP endpoints for device management
// Phase 2: CRUD operations, scan trigger, DB management

#ifdef DAC_ENABLED
#ifndef NATIVE_TEST

#include <WebServer.h>

// Register all HAL API endpoints with the web server
void registerHalApiEndpoints(WebServer& server);

#endif // NATIVE_TEST
#endif // DAC_ENABLED
