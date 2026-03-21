// test_psram_alloc.cpp
// Tests for the unified PSRAM allocation wrapper (Phase 7).
//
// Verifies alloc/free semantics, heap_budget integration, fallback tracking,
// stat queries, null/zero input rejection, and multi-allocation tracking.

#include <unity.h>
#include <string.h>
#include <stdio.h>

// psram_alloc depends on heap_budget — include budget implementation first.
// Both are pure C++ with no Arduino dependencies.
#include "../../src/heap_budget.h"
#include "../../src/heap_budget.cpp"

#include "../../src/psram_alloc.h"
#include "../../src/psram_alloc.cpp"

// ---------------------------------------------------------------------------
// setUp / tearDown
// ---------------------------------------------------------------------------

void setUp(void) {
    _psram_test_force_fail = false;
    psram_stats_reset();
    heap_budget_reset();
}

void tearDown(void) {}

// ---------------------------------------------------------------------------
// Test 1: Basic allocation succeeds and returns non-null pointer
// ---------------------------------------------------------------------------

void test_alloc_returns_non_null(void) {
    void* ptr = psram_alloc(10, sizeof(float), "test_buf");
    TEST_ASSERT_NOT_NULL(ptr);
    psram_free(ptr, "test_buf");
}

// ---------------------------------------------------------------------------
// Test 2: After psram_alloc(), heap_budget_count() increases and entry has
//         correct label/bytes
// ---------------------------------------------------------------------------

void test_alloc_records_budget(void) {
    TEST_ASSERT_EQUAL_UINT8(0, heap_budget_count());

    void* ptr = psram_alloc(8, 1024, "dsp_delay");
    TEST_ASSERT_NOT_NULL(ptr);

    TEST_ASSERT_EQUAL_UINT8(1, heap_budget_count());

    const HeapBudgetEntry* e = heap_budget_entry(0);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_STRING("dsp_delay", e->label);
    TEST_ASSERT_EQUAL_UINT32(8 * 1024, e->bytes);

    psram_free(ptr, "dsp_delay");
}

// ---------------------------------------------------------------------------
// Test 3: After psram_free(), the heap_budget entry is removed
// ---------------------------------------------------------------------------

void test_free_removes_budget(void) {
    void* ptr = psram_alloc(4, 256, "ring_buf");
    TEST_ASSERT_EQUAL_UINT8(1, heap_budget_count());

    psram_free(ptr, "ring_buf");
    TEST_ASSERT_EQUAL_UINT8(0, heap_budget_count());
}

// ---------------------------------------------------------------------------
// Test 4: psram_get_stats() returns all zeros before any allocation
// ---------------------------------------------------------------------------

void test_stats_initial_zero(void) {
    PsramAllocStats s = psram_get_stats();
    TEST_ASSERT_EQUAL_UINT32(0, s.fallbackCount);
    TEST_ASSERT_EQUAL_UINT32(0, s.failedCount);
    TEST_ASSERT_EQUAL_UINT32(0, s.activePsramBytes);
    TEST_ASSERT_EQUAL_UINT32(0, s.activeSramBytes);
}

// ---------------------------------------------------------------------------
// Test 5: Force PSRAM fail -> fallback counter incremented, activeSramBytes
//         updated (native test: force_fail skips first calloc, second succeeds
//         as SRAM)
// ---------------------------------------------------------------------------

void test_fallback_increments_counter(void) {
    _psram_test_force_fail = true;

    void* ptr = psram_alloc(16, 64, "fallback_test");
    TEST_ASSERT_NOT_NULL(ptr);

    PsramAllocStats s = psram_get_stats();
    TEST_ASSERT_EQUAL_UINT32(1, s.fallbackCount);
    TEST_ASSERT_EQUAL_UINT32(16 * 64, s.activeSramBytes);
    TEST_ASSERT_EQUAL_UINT32(0, s.activePsramBytes);

    // Convenience function should agree
    TEST_ASSERT_EQUAL_UINT32(1, psram_get_fallback_count());

    // Budget entry should be marked as SRAM
    const HeapBudgetEntry* e = heap_budget_entry(0);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_FALSE(e->isPsram);

    psram_free(ptr, "fallback_test");
    _psram_test_force_fail = false;
}

// ---------------------------------------------------------------------------
// Test 6: psram_alloc with nullptr label returns NULL
// ---------------------------------------------------------------------------

void test_alloc_null_label_returns_null(void) {
    void* ptr = psram_alloc(10, 4, nullptr);
    TEST_ASSERT_NULL(ptr);

    // No budget entry should be recorded
    TEST_ASSERT_EQUAL_UINT8(0, heap_budget_count());

    // Stats unchanged
    PsramAllocStats s = psram_get_stats();
    TEST_ASSERT_EQUAL_UINT32(0, s.failedCount);
}

// ---------------------------------------------------------------------------
// Test 7: psram_alloc with zero size returns NULL
// ---------------------------------------------------------------------------

void test_alloc_zero_size_returns_null(void) {
    void* ptr = psram_alloc(0, 4, "zero_count");
    TEST_ASSERT_NULL(ptr);

    ptr = psram_alloc(4, 0, "zero_size");
    TEST_ASSERT_NULL(ptr);

    TEST_ASSERT_EQUAL_UINT8(0, heap_budget_count());
}

// ---------------------------------------------------------------------------
// Test 8: After allocations, psram_stats_reset() clears all counters
// ---------------------------------------------------------------------------

void test_stats_reset(void) {
    // Create some activity
    _psram_test_force_fail = true;
    void* ptr1 = psram_alloc(4, 128, "reset_a");
    _psram_test_force_fail = false;
    void* ptr2 = psram_alloc(4, 256, "reset_b");

    // Verify non-zero stats
    PsramAllocStats s = psram_get_stats();
    TEST_ASSERT_GREATER_THAN(0, s.fallbackCount);
    TEST_ASSERT_GREATER_THAN(0, s.activeSramBytes + s.activePsramBytes);

    // Reset
    psram_stats_reset();

    s = psram_get_stats();
    TEST_ASSERT_EQUAL_UINT32(0, s.fallbackCount);
    TEST_ASSERT_EQUAL_UINT32(0, s.failedCount);
    TEST_ASSERT_EQUAL_UINT32(0, s.activePsramBytes);
    TEST_ASSERT_EQUAL_UINT32(0, s.activeSramBytes);

    // Clean up memory (budget entries still exist, just stats are reset)
    psram_free(ptr1, "reset_a");
    psram_free(ptr2, "reset_b");
}

// ---------------------------------------------------------------------------
// Test 9: Multiple allocations with different labels are tracked independently
//         in the budget
// ---------------------------------------------------------------------------

void test_multiple_allocs_tracked(void) {
    void* ptr1 = psram_alloc(8, 128, "buf_alpha");
    void* ptr2 = psram_alloc(4, 256, "buf_beta");
    void* ptr3 = psram_alloc(2, 512, "buf_gamma");

    TEST_ASSERT_NOT_NULL(ptr1);
    TEST_ASSERT_NOT_NULL(ptr2);
    TEST_ASSERT_NOT_NULL(ptr3);

    TEST_ASSERT_EQUAL_UINT8(3, heap_budget_count());

    // Verify each entry has the correct label and size
    const HeapBudgetEntry* e0 = heap_budget_entry(0);
    const HeapBudgetEntry* e1 = heap_budget_entry(1);
    const HeapBudgetEntry* e2 = heap_budget_entry(2);

    TEST_ASSERT_NOT_NULL(e0);
    TEST_ASSERT_NOT_NULL(e1);
    TEST_ASSERT_NOT_NULL(e2);

    TEST_ASSERT_EQUAL_STRING("buf_alpha", e0->label);
    TEST_ASSERT_EQUAL_UINT32(8 * 128, e0->bytes);

    TEST_ASSERT_EQUAL_STRING("buf_beta", e1->label);
    TEST_ASSERT_EQUAL_UINT32(4 * 256, e1->bytes);

    TEST_ASSERT_EQUAL_STRING("buf_gamma", e2->label);
    TEST_ASSERT_EQUAL_UINT32(2 * 512, e2->bytes);

    // Stats should reflect total PSRAM bytes (native test: all "PSRAM" when not force_fail)
    PsramAllocStats s = psram_get_stats();
    uint32_t total = (8 * 128) + (4 * 256) + (2 * 512);
    TEST_ASSERT_EQUAL_UINT32(total, s.activePsramBytes);
    TEST_ASSERT_EQUAL_UINT32(0, s.activeSramBytes);

    psram_free(ptr1, "buf_alpha");
    psram_free(ptr2, "buf_beta");
    psram_free(ptr3, "buf_gamma");
}

// ---------------------------------------------------------------------------
// Test 10: psram_free with a label that doesn't match any budget entry
//          doesn't crash (ptr was allocated with a different label)
// ---------------------------------------------------------------------------

void test_free_unknown_label_safe(void) {
    void* ptr = psram_alloc(4, 64, "real_label");
    TEST_ASSERT_NOT_NULL(ptr);
    TEST_ASSERT_EQUAL_UINT8(1, heap_budget_count());

    // Free with wrong label — should not crash, budget entry remains
    // (heap_budget_remove returns false for unknown label)
    psram_free(ptr, "nonexistent");

    // The "real_label" entry should still be in budget since we freed with wrong label
    TEST_ASSERT_EQUAL_UINT8(1, heap_budget_count());
    const HeapBudgetEntry* e = heap_budget_entry(0);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_STRING("real_label", e->label);

    // Clean up properly — the memory was already freed by psram_free,
    // but budget entry remains. Remove it manually.
    heap_budget_remove("real_label");
}

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    RUN_TEST(test_alloc_returns_non_null);
    RUN_TEST(test_alloc_records_budget);
    RUN_TEST(test_free_removes_budget);
    RUN_TEST(test_stats_initial_zero);
    RUN_TEST(test_fallback_increments_counter);
    RUN_TEST(test_alloc_null_label_returns_null);
    RUN_TEST(test_alloc_zero_size_returns_null);
    RUN_TEST(test_stats_reset);
    RUN_TEST(test_multiple_allocs_tracked);
    RUN_TEST(test_free_unknown_label_safe);

    return UNITY_END();
}
