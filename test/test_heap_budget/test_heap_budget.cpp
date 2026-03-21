// test_heap_budget.cpp
// Tests for the heap budget allocation tracker (Phase 3).
//
// Verifies record/update/reset semantics, capacity limits, totals,
// duplicate-label deduplication, and null-input rejection.

#include <unity.h>
#include <string.h>
#include <stdio.h>

// heap_budget is pure C++ with no Arduino dependencies.
// Include the implementation directly (test_build_src = no).
#include "../../src/heap_budget.h"
#include "../../src/heap_budget.cpp"

// ---------------------------------------------------------------------------
// setUp / tearDown
// ---------------------------------------------------------------------------

void setUp(void) {
    heap_budget_reset();
}

void tearDown(void) {}

// ---------------------------------------------------------------------------
// Test 1: Initial state -- empty
// ---------------------------------------------------------------------------

void test_initial_state(void) {
    TEST_ASSERT_EQUAL_UINT8(0, heap_budget_count());
    TEST_ASSERT_EQUAL_UINT32(0, heap_budget_total_psram());
    TEST_ASSERT_EQUAL_UINT32(0, heap_budget_total_sram());
}

// ---------------------------------------------------------------------------
// Test 2: Record a single entry
// ---------------------------------------------------------------------------

void test_record_single(void) {
    TEST_ASSERT_TRUE(heap_budget_record("dsp_delay", 8192, true));

    TEST_ASSERT_EQUAL_UINT8(1, heap_budget_count());

    const HeapBudgetEntry* e = heap_budget_entry(0);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_STRING("dsp_delay", e->label);
    TEST_ASSERT_EQUAL_UINT32(8192, e->bytes);
    TEST_ASSERT_TRUE(e->isPsram);
}

// ---------------------------------------------------------------------------
// Test 3: PSRAM entry counted in PSRAM total only
// ---------------------------------------------------------------------------

void test_record_psram_entry(void) {
    heap_budget_record("fir_buf", 4096, true);

    TEST_ASSERT_EQUAL_UINT32(4096, heap_budget_total_psram());
    TEST_ASSERT_EQUAL_UINT32(0,    heap_budget_total_sram());
}

// ---------------------------------------------------------------------------
// Test 4: SRAM entry counted in SRAM total only
// ---------------------------------------------------------------------------

void test_record_sram_entry(void) {
    heap_budget_record("ring_buf", 2048, false);

    TEST_ASSERT_EQUAL_UINT32(0,    heap_budget_total_psram());
    TEST_ASSERT_EQUAL_UINT32(2048, heap_budget_total_sram());
}

// ---------------------------------------------------------------------------
// Test 5: Record multiple entries
// ---------------------------------------------------------------------------

void test_record_multiple(void) {
    heap_budget_record("alpha", 100, true);
    heap_budget_record("beta",  200, false);
    heap_budget_record("gamma", 300, true);

    TEST_ASSERT_EQUAL_UINT8(3, heap_budget_count());

    const HeapBudgetEntry* e0 = heap_budget_entry(0);
    const HeapBudgetEntry* e1 = heap_budget_entry(1);
    const HeapBudgetEntry* e2 = heap_budget_entry(2);

    TEST_ASSERT_NOT_NULL(e0);
    TEST_ASSERT_NOT_NULL(e1);
    TEST_ASSERT_NOT_NULL(e2);

    TEST_ASSERT_EQUAL_STRING("alpha", e0->label);
    TEST_ASSERT_EQUAL_STRING("beta",  e1->label);
    TEST_ASSERT_EQUAL_STRING("gamma", e2->label);
}

// ---------------------------------------------------------------------------
// Test 6: Duplicate label updates existing entry (no new slot)
// ---------------------------------------------------------------------------

void test_duplicate_label_updates(void) {
    heap_budget_record("foo", 100, false);
    TEST_ASSERT_EQUAL_UINT8(1, heap_budget_count());
    TEST_ASSERT_EQUAL_UINT32(100, heap_budget_total_sram());

    // Update same label with new bytes
    heap_budget_record("foo", 200, false);
    TEST_ASSERT_EQUAL_UINT8(1, heap_budget_count());  // still 1

    const HeapBudgetEntry* e = heap_budget_entry(0);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_UINT32(200, e->bytes);
    TEST_ASSERT_EQUAL_UINT32(200, heap_budget_total_sram());
}

// ---------------------------------------------------------------------------
// Test 7: Capacity limit -- HEAP_BUDGET_MAX_ENTRIES
// ---------------------------------------------------------------------------

void test_capacity_limit(void) {
    char label[HEAP_BUDGET_LABEL_MAX];

    for (int i = 0; i < HEAP_BUDGET_MAX_ENTRIES; i++) {
        snprintf(label, sizeof(label), "entry_%d", i);
        TEST_ASSERT_TRUE(heap_budget_record(label, 64, false));
    }
    TEST_ASSERT_EQUAL_UINT8(HEAP_BUDGET_MAX_ENTRIES, heap_budget_count());

    // One more should fail
    TEST_ASSERT_FALSE(heap_budget_record("overflow_entry", 64, false));
    TEST_ASSERT_EQUAL_UINT8(HEAP_BUDGET_MAX_ENTRIES, heap_budget_count());
}

// ---------------------------------------------------------------------------
// Test 8: Entry access out of bounds returns nullptr
// ---------------------------------------------------------------------------

void test_entry_out_of_bounds(void) {
    heap_budget_record("only_one", 512, true);

    TEST_ASSERT_NULL(heap_budget_entry(-1));
    TEST_ASSERT_NULL(heap_budget_entry(1));    // only index 0 valid
    TEST_ASSERT_NULL(heap_budget_entry(999));
}

// ---------------------------------------------------------------------------
// Test 9: Reset clears everything
// ---------------------------------------------------------------------------

void test_reset(void) {
    heap_budget_record("a", 100, true);
    heap_budget_record("b", 200, false);
    TEST_ASSERT_EQUAL_UINT8(2, heap_budget_count());

    heap_budget_reset();

    TEST_ASSERT_EQUAL_UINT8(0, heap_budget_count());
    TEST_ASSERT_EQUAL_UINT32(0, heap_budget_total_psram());
    TEST_ASSERT_EQUAL_UINT32(0, heap_budget_total_sram());
    TEST_ASSERT_NULL(heap_budget_entry(0));
}

// ---------------------------------------------------------------------------
// Test 10: Null label rejected
// ---------------------------------------------------------------------------

void test_null_label_rejected(void) {
    TEST_ASSERT_FALSE(heap_budget_record(nullptr, 100, true));
    TEST_ASSERT_EQUAL_UINT8(0, heap_budget_count());
}

// ---------------------------------------------------------------------------
// Test 11: Mixed PSRAM and SRAM totals
// ---------------------------------------------------------------------------

void test_mixed_psram_sram_totals(void) {
    heap_budget_record("psram_a", 1000, true);
    heap_budget_record("sram_a",  2000, false);
    heap_budget_record("psram_b", 3000, true);
    heap_budget_record("sram_b",  4000, false);

    TEST_ASSERT_EQUAL_UINT32(4000, heap_budget_total_psram());   // 1000 + 3000
    TEST_ASSERT_EQUAL_UINT32(6000, heap_budget_total_sram());    // 2000 + 4000
}

// ---------------------------------------------------------------------------
// Test 12: Duplicate label can change isPsram classification
// ---------------------------------------------------------------------------

void test_duplicate_label_changes_classification(void) {
    heap_budget_record("shared_buf", 500, false);  // SRAM
    TEST_ASSERT_EQUAL_UINT32(500, heap_budget_total_sram());
    TEST_ASSERT_EQUAL_UINT32(0,   heap_budget_total_psram());

    // Same label, now PSRAM
    heap_budget_record("shared_buf", 500, true);
    TEST_ASSERT_EQUAL_UINT32(0,   heap_budget_total_sram());
    TEST_ASSERT_EQUAL_UINT32(500, heap_budget_total_psram());
}

// ---------------------------------------------------------------------------
// Test 13: Label truncation at HEAP_BUDGET_LABEL_MAX
// ---------------------------------------------------------------------------

void test_label_truncation(void) {
    // Create a label longer than HEAP_BUDGET_LABEL_MAX
    char long_label[64];
    memset(long_label, 'X', sizeof(long_label));
    long_label[63] = '\0';

    TEST_ASSERT_TRUE(heap_budget_record(long_label, 100, false));

    const HeapBudgetEntry* e = heap_budget_entry(0);
    TEST_ASSERT_NOT_NULL(e);
    // Label should be truncated and null-terminated
    TEST_ASSERT_EQUAL_UINT32(HEAP_BUDGET_LABEL_MAX - 1, strlen(e->label));
}

// ---------------------------------------------------------------------------
// Test 14: Reset then re-record works
// ---------------------------------------------------------------------------

void test_reset_then_rerecord(void) {
    heap_budget_record("first", 100, true);
    heap_budget_reset();

    heap_budget_record("second", 200, false);
    TEST_ASSERT_EQUAL_UINT8(1, heap_budget_count());

    const HeapBudgetEntry* e = heap_budget_entry(0);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_STRING("second", e->label);
    TEST_ASSERT_EQUAL_UINT32(200, e->bytes);
    TEST_ASSERT_FALSE(e->isPsram);
}

// ---------------------------------------------------------------------------
// Test 15: Capacity refill after reset
// ---------------------------------------------------------------------------

void test_capacity_refill_after_reset(void) {
    char label[HEAP_BUDGET_LABEL_MAX];

    // Fill to capacity
    for (int i = 0; i < HEAP_BUDGET_MAX_ENTRIES; i++) {
        snprintf(label, sizeof(label), "fill_%d", i);
        heap_budget_record(label, 10, false);
    }
    TEST_ASSERT_EQUAL_UINT8(HEAP_BUDGET_MAX_ENTRIES, heap_budget_count());

    // Reset
    heap_budget_reset();
    TEST_ASSERT_EQUAL_UINT8(0, heap_budget_count());

    // Refill with different labels
    for (int i = 0; i < HEAP_BUDGET_MAX_ENTRIES; i++) {
        snprintf(label, sizeof(label), "refill_%d", i);
        TEST_ASSERT_TRUE(heap_budget_record(label, 20, true));
    }
    TEST_ASSERT_EQUAL_UINT8(HEAP_BUDGET_MAX_ENTRIES, heap_budget_count());
    TEST_ASSERT_EQUAL_UINT32(HEAP_BUDGET_MAX_ENTRIES * 20, heap_budget_total_psram());
}

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    RUN_TEST(test_initial_state);
    RUN_TEST(test_record_single);
    RUN_TEST(test_record_psram_entry);
    RUN_TEST(test_record_sram_entry);
    RUN_TEST(test_record_multiple);
    RUN_TEST(test_duplicate_label_updates);
    RUN_TEST(test_capacity_limit);
    RUN_TEST(test_entry_out_of_bounds);
    RUN_TEST(test_reset);
    RUN_TEST(test_null_label_rejected);
    RUN_TEST(test_mixed_psram_sram_totals);
    RUN_TEST(test_duplicate_label_changes_classification);
    RUN_TEST(test_label_truncation);
    RUN_TEST(test_reset_then_rerecord);
    RUN_TEST(test_capacity_refill_after_reset);

    return UNITY_END();
}
