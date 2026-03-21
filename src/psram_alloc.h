#pragma once
// psram_alloc.h — Unified PSRAM allocation wrapper with SRAM fallback.
// Centralizes the PSRAM→SRAM fallback pattern, automatic heap_budget recording,
// and diagnostic emission (DIAG_SYS_PSRAM_ALLOC_FAIL).
// Pure C++ — no Arduino/FreeRTOS dependencies (testable natively).

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct PsramAllocStats {
    uint32_t fallbackCount;    // lifetime PSRAM->SRAM fallback count
    uint32_t failedCount;      // lifetime total-failure count (both PSRAM+SRAM failed)
    uint32_t activePsramBytes; // bytes currently on PSRAM
    uint32_t activeSramBytes;  // bytes currently on SRAM via fallback
};

// Allocate `count * size` bytes, preferring PSRAM with SRAM fallback.
//
// On PSRAM success:  records to heap_budget as PSRAM, returns ptr.
// On PSRAM fail + SRAM success: records as SRAM, increments fallback counter,
//   emits DIAG_SYS_PSRAM_ALLOC_FAIL (WARN), returns ptr.
// On total failure: increments failed counter,
//   emits DIAG_SYS_PSRAM_ALLOC_FAIL (ERROR), returns NULL.
//
// `label` must be a string literal or static-lifetime pointer (stored in heap_budget).
void* psram_alloc(size_t count, size_t size, const char* label);

// Free memory previously allocated via psram_alloc().
// Removes the heap_budget entry for `label` and adjusts stats.
void psram_free(void* ptr, const char* label);

// Query allocation statistics.
PsramAllocStats psram_get_stats(void);

// Get lifetime PSRAM->SRAM fallback count (convenience).
uint32_t psram_get_fallback_count(void);

// Reset all stats (test-only).
void psram_stats_reset(void);

#ifdef UNIT_TEST
// Test hook: set to true to force PSRAM allocation to fail,
// exercising the SRAM fallback path in native tests.
extern bool _psram_test_force_fail;
#endif
