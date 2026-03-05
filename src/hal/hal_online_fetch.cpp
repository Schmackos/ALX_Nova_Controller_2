#ifdef DAC_ENABLED

#include "hal_online_fetch.h"
#include <string.h>
#include <stdlib.h>

#ifndef NATIVE_TEST
#include "../debug_serial.h"
#include "../app_state.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#else
#define LOG_I(tag, ...) ((void)0)
#define LOG_W(tag, ...) ((void)0)
#define LOG_E(tag, ...) ((void)0)
#endif

// Base URL for the HAL device YAML database
// Placeholder — fill in with actual repo during deployment
#define HAL_DEVICE_DB_BASE_URL "https://raw.githubusercontent.com/alx-audio/hal-devices/main/devices/"

static volatile bool _fetchCancelled = false;

HalFetchResult hal_online_fetch(const char* compatible, HalDeviceDescriptor* out) {
    if (!compatible || !out) return HAL_FETCH_ERROR;

#ifndef NATIVE_TEST
    // Check WiFi
    if (!appState.wifiConnectSuccess) return HAL_FETCH_NO_WIFI;

    // Check heap — TLS needs ~44KB internal SRAM
    size_t maxBlock = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    if (maxBlock < 35000) {
        LOG_W("[HAL Fetch]", "Heap too low for TLS: %u bytes", maxBlock);
        return HAL_FETCH_LOW_HEAP;
    }

    // Build URL: base + compatible + ".yaml"
    // e.g. "evergrande,es8311" → "es8311.yaml" (strip vendor prefix)
    const char* model = strchr(compatible, ',');
    if (model) model++; else model = compatible;

    char url[128];
    snprintf(url, sizeof(url), "%s%s.yaml", HAL_DEVICE_DB_BASE_URL, model);

    // Check NVS for cached ETag
    Preferences prefs;
    char etagKey[40];
    snprintf(etagKey, sizeof(etagKey), "hal_etag_%.20s", model);

    char cachedEtag[64] = "";
    prefs.begin("hal_fetch", true);
    prefs.getString(etagKey, cachedEtag, sizeof(cachedEtag));
    prefs.end();

    // HTTPS request
    _fetchCancelled = false;
    WiFiClientSecure client;
    if (maxBlock < 50000 || !appState.enableCertValidation) {
        client.setInsecure();  // Heap-conserving fallback
    }

    HTTPClient http;
    http.begin(client, url);
    if (cachedEtag[0]) {
        http.addHeader("If-None-Match", cachedEtag);
    }

    int httpCode = http.GET();

    if (_fetchCancelled) {
        http.end();
        return HAL_FETCH_ERROR;
    }

    if (httpCode == 304) {
        http.end();
        LOG_I("[HAL Fetch]", "%s: 304 Not Modified (ETag cached)", model);
        return HAL_FETCH_NOT_MODIFIED;
    }

    if (httpCode == 404) {
        http.end();
        LOG_I("[HAL Fetch]", "%s: 404 Not Found", model);
        return HAL_FETCH_NOT_FOUND;
    }

    if (httpCode == 429) {
        http.end();
        LOG_W("[HAL Fetch]", "%s: 429 Rate Limited", model);
        return HAL_FETCH_RATE_LIMITED;
    }

    if (httpCode != 200) {
        http.end();
        LOG_E("[HAL Fetch]", "%s: HTTP %d", model, httpCode);
        return HAL_FETCH_ERROR;
    }

    // 200 OK — parse YAML
    String body = http.getString();
    http.end();

    if (!hal_parse_device_yaml(body.c_str(), body.length(), out)) {
        LOG_E("[HAL Fetch]", "%s: YAML parse failed", model);
        return HAL_FETCH_ERROR;
    }

    // Cache ETag in NVS
    const char* newEtag = http.header("ETag").c_str();
    if (newEtag && newEtag[0]) {
        prefs.begin("hal_fetch", false);
        prefs.putString(etagKey, newEtag);
        prefs.end();
    }

    LOG_I("[HAL Fetch]", "%s: OK, parsed '%s'", model, out->name);
    return HAL_FETCH_OK;
#else
    // Native test — network calls are not available
    return HAL_FETCH_NO_WIFI;
#endif
}

void hal_online_fetch_cancel() {
    _fetchCancelled = true;
}

// ===== Pure YAML parser (flat key-value only) =====
// Handles the ALX Nova HAL YAML schema v1:
//   key: value
//   key: "quoted value"
// No nesting, no arrays (space-separated values are stored as strings)

static bool yaml_get_value(const char* line, const char* key, char* out, int maxLen) {
    int keyLen = strlen(key);
    if (strncmp(line, key, keyLen) != 0) return false;
    const char* p = line + keyLen;
    // Skip ':'
    if (*p != ':') return false;
    p++;
    // Skip whitespace
    while (*p == ' ' || *p == '\t') p++;
    // Strip quotes
    if (*p == '"') {
        p++;
        const char* end = strchr(p, '"');
        if (end) {
            int len = end - p;
            if (len >= maxLen) len = maxLen - 1;
            memcpy(out, p, len);
            out[len] = '\0';
            return true;
        }
    }
    // Unquoted value — copy to end of line
    int len = strlen(p);
    // Strip trailing whitespace/newline
    while (len > 0 && (p[len-1] == '\n' || p[len-1] == '\r' || p[len-1] == ' ')) len--;
    if (len >= maxLen) len = maxLen - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    return true;
}

bool hal_parse_device_yaml(const char* yamlText, int len, HalDeviceDescriptor* out) {
    if (!yamlText || !out || len <= 0) return false;

    memset(out, 0, sizeof(HalDeviceDescriptor));

    char buf[64];
    bool hasCompatible = false;

    // Parse line by line
    const char* p = yamlText;
    const char* end = yamlText + len;

    while (p < end) {
        // Find end of line
        const char* eol = p;
        while (eol < end && *eol != '\n') eol++;

        int lineLen = eol - p;
        if (lineLen > 0 && lineLen < 256) {
            // Copy line to temp buffer for safe parsing
            char line[256];
            memcpy(line, p, lineLen);
            line[lineLen] = '\0';

            // Skip comments and blank lines
            if (line[0] != '#' && line[0] != '\0') {
                if (yaml_get_value(line, "compatible", buf, sizeof(buf))) {
                    strncpy(out->compatible, buf, 31);
                    hasCompatible = true;
                }
                else if (yaml_get_value(line, "name", buf, sizeof(buf)))
                    strncpy(out->name, buf, 32);
                else if (yaml_get_value(line, "manufacturer", buf, sizeof(buf)))
                    strncpy(out->manufacturer, buf, 32);
                else if (yaml_get_value(line, "device_type", buf, sizeof(buf))) {
                    if (strcmp(buf, "DAC") == 0) out->type = HAL_DEV_DAC;
                    else if (strcmp(buf, "ADC") == 0) out->type = HAL_DEV_ADC;
                    else if (strcmp(buf, "CODEC") == 0) out->type = HAL_DEV_CODEC;
                    else if (strcmp(buf, "AMP") == 0) out->type = HAL_DEV_AMP;
                    else if (strcmp(buf, "DSP") == 0) out->type = HAL_DEV_DSP;
                    else if (strcmp(buf, "SENSOR") == 0) out->type = HAL_DEV_SENSOR;
                }
                else if (yaml_get_value(line, "i2c_default_address", buf, sizeof(buf)))
                    out->i2cAddr = (uint8_t)strtol(buf, nullptr, 0);
                else if (yaml_get_value(line, "channel_count", buf, sizeof(buf)))
                    out->channelCount = (uint8_t)atoi(buf);
                else if (yaml_get_value(line, "cap_hw_volume", buf, sizeof(buf))) {
                    if (strcmp(buf, "true") == 0) out->capabilities |= HAL_CAP_HW_VOLUME;
                }
                else if (yaml_get_value(line, "cap_hw_mute", buf, sizeof(buf))) {
                    if (strcmp(buf, "true") == 0) out->capabilities |= HAL_CAP_MUTE;
                }
                else if (yaml_get_value(line, "cap_adc_path", buf, sizeof(buf))) {
                    if (strcmp(buf, "true") == 0) out->capabilities |= HAL_CAP_ADC_PATH;
                }
                else if (yaml_get_value(line, "cap_dac_path", buf, sizeof(buf))) {
                    if (strcmp(buf, "true") == 0) out->capabilities |= HAL_CAP_DAC_PATH;
                }
            }
        }

        p = eol + 1;
    }

    return hasCompatible;
}

#endif // DAC_ENABLED
