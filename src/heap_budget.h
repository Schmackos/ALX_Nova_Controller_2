#pragma once
// heap_budget.h — Lightweight heap allocation budget tracker.
// Records labeled allocations with PSRAM/SRAM classification.
// Pure C++ — no Arduino/FreeRTOS dependencies (testable natively).

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef HEAP_BUDGET_MAX_ENTRIES
#define HEAP_BUDGET_MAX_ENTRIES 32
#endif

#define HEAP_BUDGET_LABEL_MAX 24

struct HeapBudgetEntry {
    char     label[HEAP_BUDGET_LABEL_MAX];
    uint32_t bytes;
    bool     isPsram;
};

// Record an allocation.  If `label` already exists, update its bytes/isPsram.
// Returns false if label is null or capacity exceeded (and label is new).
bool     heap_budget_record(const char* label, uint32_t bytes, bool isPsram);

// Reset all entries and totals.
void     heap_budget_reset(void);

// Number of tracked entries.
uint8_t  heap_budget_count(void);

// Access entry by index (0-based).  Returns nullptr if out of bounds.
const HeapBudgetEntry* heap_budget_entry(int index);

// Running totals.
uint32_t heap_budget_total_psram(void);
uint32_t heap_budget_total_sram(void);
