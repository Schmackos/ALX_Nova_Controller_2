// heap_budget.cpp — Heap allocation budget tracker implementation.
// Pure C++ — no Arduino/FreeRTOS dependencies.

#include "heap_budget.h"
#include <string.h>

static HeapBudgetEntry _entries[HEAP_BUDGET_MAX_ENTRIES];
static uint8_t  _count       = 0;
static uint32_t _totalPsram  = 0;
static uint32_t _totalSram   = 0;

// Find existing entry by label, returns index or -1.
static int _findByLabel(const char* label) {
    for (int i = 0; i < _count; i++) {
        if (strncmp(_entries[i].label, label, HEAP_BUDGET_LABEL_MAX) == 0) {
            return i;
        }
    }
    return -1;
}

// Recompute totals from scratch (after update or reset).
static void _recomputeTotals(void) {
    _totalPsram = 0;
    _totalSram  = 0;
    for (int i = 0; i < _count; i++) {
        if (_entries[i].isPsram) {
            _totalPsram += _entries[i].bytes;
        } else {
            _totalSram += _entries[i].bytes;
        }
    }
}

bool heap_budget_record(const char* label, uint32_t bytes, bool isPsram) {
    if (!label) return false;

    int idx = _findByLabel(label);
    if (idx >= 0) {
        // Update existing entry
        _entries[idx].bytes  = bytes;
        _entries[idx].isPsram = isPsram;
        _recomputeTotals();
        return true;
    }

    // New entry — check capacity
    if (_count >= HEAP_BUDGET_MAX_ENTRIES) return false;

    strncpy(_entries[_count].label, label, HEAP_BUDGET_LABEL_MAX - 1);
    _entries[_count].label[HEAP_BUDGET_LABEL_MAX - 1] = '\0';
    _entries[_count].bytes  = bytes;
    _entries[_count].isPsram = isPsram;
    _count++;
    _recomputeTotals();
    return true;
}

void heap_budget_reset(void) {
    _count      = 0;
    _totalPsram = 0;
    _totalSram  = 0;
    memset(_entries, 0, sizeof(_entries));
}

uint8_t heap_budget_count(void) {
    return _count;
}

const HeapBudgetEntry* heap_budget_entry(int index) {
    if (index < 0 || index >= _count) return nullptr;
    return &_entries[index];
}

uint32_t heap_budget_total_psram(void) {
    return _totalPsram;
}

uint32_t heap_budget_total_sram(void) {
    return _totalSram;
}
