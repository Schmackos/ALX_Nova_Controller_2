// psram_alloc.cpp — Unified PSRAM allocation wrapper implementation.
// See psram_alloc.h for API documentation.

#include "psram_alloc.h"
#include "heap_budget.h"
#include <stdlib.h>
#include <string.h>

#ifndef NATIVE_TEST
#include "esp_heap_caps.h"
#include "diag_journal.h"
#include "debug_serial.h"
#endif

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------

static uint32_t _fallbackCount  = 0;
static uint32_t _failedCount    = 0;
static uint32_t _activePsram    = 0;
static uint32_t _activeSram     = 0;

#ifdef UNIT_TEST
bool _psram_test_force_fail = false;
#endif

// ---------------------------------------------------------------------------
// Allocation
// ---------------------------------------------------------------------------

void* psram_alloc(size_t count, size_t size, const char* label) {
    if (!label || count == 0 || size == 0) return nullptr;

    const uint32_t bytes = (uint32_t)(count * size);
    void* ptr = nullptr;
    bool isPsram = false;

#ifdef NATIVE_TEST
    // Native tests: simulate PSRAM with calloc, allow forced failure
    if (!_psram_test_force_fail) {
        ptr = calloc(count, size);
        isPsram = true;  // Pretend it's PSRAM in native tests
    }
    if (!ptr) {
        // SRAM fallback (or first attempt if force_fail)
        ptr = calloc(count, size);
        if (ptr) {
            isPsram = false;
            _fallbackCount++;
        }
    }
#else
    // ESP32: try PSRAM first
    ptr = heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM);
    if (ptr) {
        isPsram = true;
    } else {
        // SRAM fallback
        ptr = calloc(count, size);
        if (ptr) {
            isPsram = false;
            _fallbackCount++;
            LOG_W("[PsramAlloc] %s (%lu bytes) fell back to SRAM", label, (unsigned long)bytes);
            diag_emit(DIAG_SYS_PSRAM_ALLOC_FAIL, DIAG_SEV_WARN,
                      0xFF, "System", label);
        }
    }
#endif

    if (ptr) {
        // Record in heap budget
        heap_budget_record(label, bytes, isPsram);
        if (isPsram) {
            _activePsram += bytes;
        } else {
            _activeSram += bytes;
        }
    } else {
        // Total failure
        _failedCount++;
#ifndef NATIVE_TEST
        LOG_E("[PsramAlloc] %s (%lu bytes) allocation FAILED", label, (unsigned long)bytes);
        diag_emit(DIAG_SYS_PSRAM_ALLOC_FAIL, DIAG_SEV_ERROR,
                  0xFF, "System", label);
#endif
    }

    return ptr;
}

// ---------------------------------------------------------------------------
// Free
// ---------------------------------------------------------------------------

void psram_free(void* ptr, const char* label) {
    if (!ptr) return;

    // Look up the budget entry to adjust stats before removing
    if (label) {
        const int count = heap_budget_count();
        for (int i = 0; i < count; i++) {
            const HeapBudgetEntry* e = heap_budget_entry(i);
            if (e && strncmp(e->label, label, HEAP_BUDGET_LABEL_MAX) == 0) {
                if (e->isPsram) {
                    _activePsram = (_activePsram >= e->bytes) ? _activePsram - e->bytes : 0;
                } else {
                    _activeSram = (_activeSram >= e->bytes) ? _activeSram - e->bytes : 0;
                }
                break;
            }
        }
        heap_budget_remove(label);
    }

    free(ptr);
}

// ---------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------

PsramAllocStats psram_get_stats(void) {
    PsramAllocStats s;
    s.fallbackCount    = _fallbackCount;
    s.failedCount      = _failedCount;
    s.activePsramBytes = _activePsram;
    s.activeSramBytes  = _activeSram;
    return s;
}

uint32_t psram_get_fallback_count(void) {
    return _fallbackCount;
}

void psram_stats_reset(void) {
    _fallbackCount = 0;
    _failedCount   = 0;
    _activePsram   = 0;
    _activeSram    = 0;
}
