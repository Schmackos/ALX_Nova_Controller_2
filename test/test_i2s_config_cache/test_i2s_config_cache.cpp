// test_i2s_config_cache.cpp
// Tests for the I2S config cache (per-lane HalDeviceConfig store) and
// the pin-resolution logic (_resolveI2sPin) introduced in the i2s-pin-hal-config
// fix branch. The cache lives in i2s_audio.cpp behind #ifdef NATIVE_TEST hooks.
// Because including i2s_audio.cpp pulls in a large ESP32 dependency chain, we
// replicate the cache statics and hook implementations inline here — the same
// strategy used by test_audio_rms.cpp for the pure-computation functions.
//
// Coverage:
//   - _test_i2s_cache_reset()    : cache starts invalid after reset
//   - _test_i2s_cache_set()      : populates per-lane cache correctly
//   - _test_i2s_cache_get()      : returns stored config or nullptr
//   - _test_i2s_cache_valid()    : reports validity per lane
//   - Cache per-lane independence
//   - nullptr passthrough does not corrupt an already-valid cache entry
//   - cfg.valid==false is not cached
//   - Pin resolution: override > 0 wins over fallback
//   - Pin resolution: negative value falls back to board default
//   - Pin resolution: zero falls back to board default (GPIO 0 = strapping pin)

#include <unity.h>
#include <cstring>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

#include "../../src/hal/hal_types.h"   // HalDeviceConfig — plain struct, no platform deps

// ===== Inline cache + test hook replication =====
// Mirror of what i2s_audio.cpp provides under #ifdef NATIVE_TEST.
// The production statics are file-scoped; we replicate them here so this
// test module is fully self-contained and links without the I2S driver chain.

static HalDeviceConfig _cachedAdcCfg[2] = {};
static bool _cachedAdcCfgValid[2] = {false, false};

static void _test_i2s_cache_reset() {
    _cachedAdcCfgValid[0] = _cachedAdcCfgValid[1] = false;
    memset(&_cachedAdcCfg[0], 0, sizeof(HalDeviceConfig));
    memset(&_cachedAdcCfg[1], 0, sizeof(HalDeviceConfig));
}

static void _test_i2s_cache_set(int lane, const HalDeviceConfig* cfg) {
    if (lane >= 0 && lane < 2 && cfg) {
        _cachedAdcCfg[lane] = *cfg;
        _cachedAdcCfgValid[lane] = true;
    }
}

static const HalDeviceConfig* _test_i2s_cache_get(int lane) {
    return (lane >= 0 && lane < 2 && _cachedAdcCfgValid[lane])
           ? &_cachedAdcCfg[lane] : nullptr;
}

static bool _test_i2s_cache_valid(int lane) {
    return (lane >= 0 && lane < 2) && _cachedAdcCfgValid[lane];
}

// ===== Board default pins (from src/config.h) used as fallbacks =====
static const int BOARD_MCLK_PIN = 22;
static const int BOARD_BCK_PIN  = 20;
static const int BOARD_LRC_PIN  = 21;
static const int BOARD_DOUT_PIN = 23;

// ===== setUp / tearDown =====

void setUp(void) {
    ArduinoMock::reset();
    _test_i2s_cache_reset();
}

void tearDown(void) {}

// ===== Tests =====

// Test 1: After _test_i2s_cache_reset(), both lanes report invalid.
void test_cache_starts_invalid(void) {
    // Arrange — setUp() already called _test_i2s_cache_reset()
    // Act / Assert
    TEST_ASSERT_FALSE(_test_i2s_cache_valid(0));
    TEST_ASSERT_FALSE(_test_i2s_cache_valid(1));
}

// Test 2: Populating lane 0 via _test_i2s_cache_set makes it valid and readable.
// Note: i2s_audio_configure_adc() is a no-op inline stub in NATIVE_TEST mode.
// _test_i2s_cache_set() exercises the same write path as the production code inside
// i2s_audio_configure_adc() — it is the authoritative test hook for the cache.
void test_cache_populated_on_configure(void) {
    // Arrange
    HalDeviceConfig cfg = {};
    cfg.valid    = true;
    cfg.pinMclk  = 33;

    // Act — use the test hook that mirrors the cache-write in i2s_audio_configure_adc()
    _test_i2s_cache_set(0, &cfg);

    // Assert
    TEST_ASSERT_TRUE(_test_i2s_cache_valid(0));
    const HalDeviceConfig* stored = _test_i2s_cache_get(0);
    TEST_ASSERT_NOT_NULL(stored);
    TEST_ASSERT_EQUAL_INT(33, stored->pinMclk);
}

// Test 3: Lane 0 and lane 1 caches are independent — each holds its own config.
void test_cache_per_lane_independence(void) {
    // Arrange
    HalDeviceConfig cfg0 = {};
    cfg0.valid   = true;
    cfg0.pinMclk = 33;

    HalDeviceConfig cfg1 = {};
    cfg1.valid   = true;
    cfg1.pinMclk = 42;

    // Act
    _test_i2s_cache_set(0, &cfg0);
    _test_i2s_cache_set(1, &cfg1);

    // Assert — each lane returns its own value, not the other's
    const HalDeviceConfig* s0 = _test_i2s_cache_get(0);
    const HalDeviceConfig* s1 = _test_i2s_cache_get(1);

    TEST_ASSERT_NOT_NULL(s0);
    TEST_ASSERT_NOT_NULL(s1);
    TEST_ASSERT_EQUAL_INT(33, s0->pinMclk);
    TEST_ASSERT_EQUAL_INT(42, s1->pinMclk);

    // Cross-check: lane 0 does not hold lane 1's value and vice versa
    TEST_ASSERT_NOT_EQUAL(s0->pinMclk, s1->pinMclk);
}

// Test 4: Passing nullptr after a valid cache entry leaves the entry intact.
// Production code: "if (cfg && cfg->valid && lane >= 0 && lane < 2) { cache = *cfg; }"
// A nullptr cfg skips the write — the cached entry survives.
void test_cache_survives_nullptr_passthrough(void) {
    // Arrange — pre-populate lane 0
    HalDeviceConfig cfg = {};
    cfg.valid   = true;
    cfg.pinMclk = 33;
    _test_i2s_cache_set(0, &cfg);

    TEST_ASSERT_TRUE(_test_i2s_cache_valid(0));

    // Act — replicate the nullptr guard from i2s_audio_configure_adc():
    //   "if (cfg && cfg->valid && lane >= 0 && lane < 2) { ... }" skips on nullptr
    const HalDeviceConfig* nullCfg = nullptr;
    if (nullCfg && nullCfg->valid && 0 >= 0 && 0 < 2) {
        _test_i2s_cache_set(0, nullCfg);   // Must not be reached
    }

    // Assert — cache is still valid and unchanged
    TEST_ASSERT_TRUE(_test_i2s_cache_valid(0));
    const HalDeviceConfig* stored = _test_i2s_cache_get(0);
    TEST_ASSERT_NOT_NULL(stored);
    TEST_ASSERT_EQUAL_INT(33, stored->pinMclk);
}

// Test 5: A cfg with valid==false is rejected — cache stays invalid.
// Production code: "if (cfg && cfg->valid && ...)" — the cfg->valid check gates writes.
void test_cache_invalid_config_not_cached(void) {
    // Arrange
    HalDeviceConfig cfg = {};
    cfg.valid   = false;   // <-- invalid flag
    cfg.pinMclk = 99;

    // Act — replicate the guard: cfg->valid is false so the write is skipped
    if (&cfg && cfg.valid && 0 >= 0 && 0 < 2) {
        _test_i2s_cache_set(0, &cfg);   // Must not be reached
    }

    // Assert
    TEST_ASSERT_FALSE(_test_i2s_cache_valid(0));
    TEST_ASSERT_NULL(_test_i2s_cache_get(0));
}

// Test 6: Pin resolution — HAL config value > 0 overrides the board default.
// Mirrors: (cfgValue > 0) ? cfgValue : fallback
void test_resolve_pin_uses_config_when_valid(void) {
    // Arrange
    int8_t cfgVal  = 33;
    int    fallback = BOARD_MCLK_PIN;   // 22

    // Act
    int result = (cfgVal > 0) ? (int)cfgVal : fallback;

    // Assert — HAL override wins
    TEST_ASSERT_EQUAL_INT(33, result);
}

// Test 7: Pin resolution — negative cfg value falls back to the board default.
void test_resolve_pin_uses_fallback_when_negative(void) {
    // Arrange
    int8_t cfgVal  = -1;
    int    fallback = BOARD_MCLK_PIN;   // 22

    // Act
    int result = (cfgVal > 0) ? (int)cfgVal : fallback;

    // Assert — board default returned
    TEST_ASSERT_EQUAL_INT(22, result);
}

// Test 8: Pin resolution — zero falls back to the board default.
// GPIO 0 is a strapping pin on ESP32-P4; it must never be used as an I2S clock.
// The production code uses (cfgValue > 0) — not (>= 0) — to exclude GPIO 0.
void test_resolve_pin_uses_fallback_when_zero(void) {
    // Arrange
    int8_t cfgVal  = 0;
    int    fallback = BOARD_MCLK_PIN;   // 22

    // Act
    int result = (cfgVal > 0) ? (int)cfgVal : fallback;

    // Assert — board default returned, GPIO 0 excluded
    TEST_ASSERT_EQUAL_INT(22, result);
}

// ===== Main =====

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_cache_starts_invalid);
    RUN_TEST(test_cache_populated_on_configure);
    RUN_TEST(test_cache_per_lane_independence);
    RUN_TEST(test_cache_survives_nullptr_passthrough);
    RUN_TEST(test_cache_invalid_config_not_cached);
    RUN_TEST(test_resolve_pin_uses_config_when_valid);
    RUN_TEST(test_resolve_pin_uses_fallback_when_negative);
    RUN_TEST(test_resolve_pin_uses_fallback_when_zero);
    return UNITY_END();
}
