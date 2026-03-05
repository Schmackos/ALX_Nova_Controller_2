#pragma once
// HAL Online Fetch — fetches device YAML from GitHub raw CDN
// Phase 2: ETag caching in NVS, rate limit handling

#ifdef DAC_ENABLED

#include "hal_types.h"

// Fetch result codes
enum HalFetchResult : uint8_t {
    HAL_FETCH_OK            = 0,   // 200 OK — descriptor populated
    HAL_FETCH_NOT_MODIFIED  = 1,   // 304 — ETag matched, use cached
    HAL_FETCH_NOT_FOUND     = 2,   // 404 — unknown device
    HAL_FETCH_RATE_LIMITED  = 3,   // 429 — try again later
    HAL_FETCH_NO_WIFI       = 4,   // No network connection
    HAL_FETCH_LOW_HEAP      = 5,   // Not enough SRAM for TLS (~44KB)
    HAL_FETCH_ERROR         = 6,   // Network or parse error
};

// Fetch a device descriptor from the online YAML database
// compatible: "vendor,model" string (e.g. "evergrande,es8311")
// out: populated on HAL_FETCH_OK
// Returns HalFetchResult
HalFetchResult hal_online_fetch(const char* compatible, HalDeviceDescriptor* out);

// Cancel any in-progress fetch (for clean shutdown)
void hal_online_fetch_cancel();

// Parse a single YAML key-value string into a descriptor
// (Pure function — no network, testable on native)
bool hal_parse_device_yaml(const char* yamlText, int len, HalDeviceDescriptor* out);

#endif // DAC_ENABLED
