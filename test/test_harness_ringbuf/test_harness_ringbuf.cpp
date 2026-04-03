// test_harness_ringbuf.cpp
// Tests for the ISR-safe ring buffer module (src/test_harness_ringbuf.h).
//
// Covers: init, push/pop round-trip, FIFO ordering, overflow/full detection,
// underflow/empty detection, count accuracy, wrap-around, negative values,
// boundary values (INT32_MIN / INT32_MAX), and refill after drain.

#include <unity.h>
#include <stdint.h>
#include <limits.h>

#include "../../src/test_harness_ringbuf.h"

// Usable capacity = CAPACITY - 1 (leave-one-slot-empty strategy).
#define USABLE_CAP (TEST_HARNESS_RINGBUF_CAPACITY - 1)

// ---------------------------------------------------------------------------
// Shared fixture
// ---------------------------------------------------------------------------

static TestHarnessRingBuf rb;

void setUp(void) {
    test_harness_ringbuf_init(&rb);
}

void tearDown(void) {}

// ---------------------------------------------------------------------------
// Test 1: init sets count to 0
// ---------------------------------------------------------------------------

void test_init_count_is_zero(void) {
    TEST_ASSERT_EQUAL_UINT32(0, test_harness_ringbuf_count(&rb));
}

// ---------------------------------------------------------------------------
// Test 2: push single value, pop returns the same value
// ---------------------------------------------------------------------------

void test_push_pop_single_value(void) {
    int ret = test_harness_ringbuf_push(&rb, 42);
    TEST_ASSERT_EQUAL_INT(TEST_HARNESS_RINGBUF_OK, ret);

    int32_t out = -1;
    ret = test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_INT(TEST_HARNESS_RINGBUF_OK, ret);
    TEST_ASSERT_EQUAL_INT32(42, out);
}

// ---------------------------------------------------------------------------
// Test 3: pop from empty buffer returns ERR_EMPTY
// ---------------------------------------------------------------------------

void test_pop_empty_returns_err_empty(void) {
    int32_t out = 99;
    int ret = test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_INT(TEST_HARNESS_RINGBUF_ERR_EMPTY, ret);
    // out_value must be unchanged on empty pop
    TEST_ASSERT_EQUAL_INT32(99, out);
}

// ---------------------------------------------------------------------------
// Test 4: count increments with each push
// ---------------------------------------------------------------------------

void test_count_after_pushes(void) {
    TEST_ASSERT_EQUAL_UINT32(0, test_harness_ringbuf_count(&rb));

    test_harness_ringbuf_push(&rb, 1);
    TEST_ASSERT_EQUAL_UINT32(1, test_harness_ringbuf_count(&rb));

    test_harness_ringbuf_push(&rb, 2);
    TEST_ASSERT_EQUAL_UINT32(2, test_harness_ringbuf_count(&rb));

    test_harness_ringbuf_push(&rb, 3);
    TEST_ASSERT_EQUAL_UINT32(3, test_harness_ringbuf_count(&rb));
}

// ---------------------------------------------------------------------------
// Test 5: count decrements with each pop
// ---------------------------------------------------------------------------

void test_count_after_pops(void) {
    test_harness_ringbuf_push(&rb, 10);
    test_harness_ringbuf_push(&rb, 20);
    test_harness_ringbuf_push(&rb, 30);
    TEST_ASSERT_EQUAL_UINT32(3, test_harness_ringbuf_count(&rb));

    int32_t out;
    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_UINT32(2, test_harness_ringbuf_count(&rb));

    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_UINT32(1, test_harness_ringbuf_count(&rb));

    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_UINT32(0, test_harness_ringbuf_count(&rb));
}

// ---------------------------------------------------------------------------
// Test 6: FIFO ordering — multiple values pop in push order
// ---------------------------------------------------------------------------

void test_fifo_ordering(void) {
    test_harness_ringbuf_push(&rb, 100);
    test_harness_ringbuf_push(&rb, 200);
    test_harness_ringbuf_push(&rb, 300);

    int32_t out;
    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_INT32(100, out);
    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_INT32(200, out);
    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_INT32(300, out);
}

// ---------------------------------------------------------------------------
// Test 7: push to full buffer returns ERR_FULL (usable capacity = CAPACITY-1)
// ---------------------------------------------------------------------------

void test_push_full_returns_err_full(void) {
    int ret;
    // Fill to the maximum usable capacity
    for (uint32_t i = 0; i < (uint32_t)USABLE_CAP; i++) {
        ret = test_harness_ringbuf_push(&rb, (int32_t)i);
        TEST_ASSERT_EQUAL_INT(TEST_HARNESS_RINGBUF_OK, ret);
    }

    // Buffer is now full; one more push must be rejected
    ret = test_harness_ringbuf_push(&rb, 9999);
    TEST_ASSERT_EQUAL_INT(TEST_HARNESS_RINGBUF_ERR_FULL, ret);
}

// ---------------------------------------------------------------------------
// Test 8: count equals USABLE_CAP when buffer is full
// ---------------------------------------------------------------------------

void test_count_equals_usable_cap_when_full(void) {
    for (uint32_t i = 0; i < (uint32_t)USABLE_CAP; i++) {
        test_harness_ringbuf_push(&rb, (int32_t)i);
    }
    TEST_ASSERT_EQUAL_UINT32((uint32_t)USABLE_CAP, test_harness_ringbuf_count(&rb));
}

// ---------------------------------------------------------------------------
// Test 9: fill to capacity then drain completely, count returns to 0
// ---------------------------------------------------------------------------

void test_fill_then_drain_completely(void) {
    for (uint32_t i = 0; i < (uint32_t)USABLE_CAP; i++) {
        test_harness_ringbuf_push(&rb, (int32_t)i);
    }
    TEST_ASSERT_EQUAL_UINT32((uint32_t)USABLE_CAP, test_harness_ringbuf_count(&rb));

    int32_t out;
    for (uint32_t i = 0; i < (uint32_t)USABLE_CAP; i++) {
        int ret = test_harness_ringbuf_pop(&rb, &out);
        TEST_ASSERT_EQUAL_INT(TEST_HARNESS_RINGBUF_OK, ret);
        TEST_ASSERT_EQUAL_INT32((int32_t)i, out);
    }

    TEST_ASSERT_EQUAL_UINT32(0, test_harness_ringbuf_count(&rb));

    // Buffer is empty again; next pop returns ERR_EMPTY
    int ret = test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_INT(TEST_HARNESS_RINGBUF_ERR_EMPTY, ret);
}

// ---------------------------------------------------------------------------
// Test 10: push after pop frees space (buffer is not permanently full)
// ---------------------------------------------------------------------------

void test_push_after_pop_succeeds(void) {
    // Fill the buffer
    for (uint32_t i = 0; i < (uint32_t)USABLE_CAP; i++) {
        test_harness_ringbuf_push(&rb, (int32_t)i);
    }

    // Confirm full
    int ret = test_harness_ringbuf_push(&rb, -1);
    TEST_ASSERT_EQUAL_INT(TEST_HARNESS_RINGBUF_ERR_FULL, ret);

    // Pop one item to free a slot
    int32_t out;
    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_INT32(0, out);

    // New push must now succeed
    ret = test_harness_ringbuf_push(&rb, 555);
    TEST_ASSERT_EQUAL_INT(TEST_HARNESS_RINGBUF_OK, ret);
}

// ---------------------------------------------------------------------------
// Test 11: wrap-around — push enough items to force index past CAPACITY
// ---------------------------------------------------------------------------

void test_wrap_around(void) {
    // Strategy: push USABLE_CAP items, drain them all (head and tail both advance
    // to USABLE_CAP position), then push one more batch and verify FIFO integrity.
    // Because MASK = CAPACITY-1, indices wrap correctly via bitwise AND.

    int32_t out;

    // First batch: fill and drain to advance indices
    for (uint32_t i = 0; i < (uint32_t)USABLE_CAP; i++) {
        test_harness_ringbuf_push(&rb, (int32_t)i);
    }
    for (uint32_t i = 0; i < (uint32_t)USABLE_CAP; i++) {
        test_harness_ringbuf_pop(&rb, &out);
    }
    // head == tail == USABLE_CAP (positions inside the array, wrapped)

    // Second batch: push 5 items spanning the wrap boundary
    for (int32_t v = 1000; v <= 1004; v++) {
        test_harness_ringbuf_push(&rb, v);
    }
    TEST_ASSERT_EQUAL_UINT32(5, test_harness_ringbuf_count(&rb));

    // Pop and verify FIFO order is preserved across the wrap
    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_INT32(1000, out);
    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_INT32(1001, out);
    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_INT32(1002, out);
    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_INT32(1003, out);
    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_INT32(1004, out);

    TEST_ASSERT_EQUAL_UINT32(0, test_harness_ringbuf_count(&rb));
}

// ---------------------------------------------------------------------------
// Test 12: negative values round-trip correctly
// ---------------------------------------------------------------------------

void test_negative_values_round_trip(void) {
    test_harness_ringbuf_push(&rb, -1);
    test_harness_ringbuf_push(&rb, -100);
    test_harness_ringbuf_push(&rb, -32768);

    int32_t out;
    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_INT32(-1, out);
    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_INT32(-100, out);
    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_INT32(-32768, out);
}

// ---------------------------------------------------------------------------
// Test 13: INT32_MIN and INT32_MAX round-trip correctly
// ---------------------------------------------------------------------------

void test_int32_extremes_round_trip(void) {
    test_harness_ringbuf_push(&rb, INT32_MIN);
    test_harness_ringbuf_push(&rb, INT32_MAX);

    int32_t out;
    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, out);
    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, out);
}

// ---------------------------------------------------------------------------
// Test 14: discarded value on overflow — original data is preserved
// ---------------------------------------------------------------------------

void test_overflow_does_not_overwrite_existing_data(void) {
    // Push one known value, fill to full, then try to overflow
    test_harness_ringbuf_push(&rb, 7777);
    for (uint32_t i = 1; i < (uint32_t)USABLE_CAP; i++) {
        test_harness_ringbuf_push(&rb, 0);
    }

    // Buffer is full; overflow attempt
    int ret = test_harness_ringbuf_push(&rb, 9999);
    TEST_ASSERT_EQUAL_INT(TEST_HARNESS_RINGBUF_ERR_FULL, ret);

    // First item must still be 7777 (not overwritten by 9999)
    int32_t out = -1;
    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_INT32(7777, out);
}

// ---------------------------------------------------------------------------
// Test 15: zero value round-trips correctly and is distinguishable from error
// ---------------------------------------------------------------------------

void test_zero_value_round_trip(void) {
    int ret = test_harness_ringbuf_push(&rb, 0);
    TEST_ASSERT_EQUAL_INT(TEST_HARNESS_RINGBUF_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1, test_harness_ringbuf_count(&rb));

    int32_t out = -1;
    ret = test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_INT(TEST_HARNESS_RINGBUF_OK, ret);
    TEST_ASSERT_EQUAL_INT32(0, out);
}

// ---------------------------------------------------------------------------
// Test 16: interleaved push/pop maintains correct FIFO ordering
// ---------------------------------------------------------------------------

void test_interleaved_push_pop_fifo(void) {
    int32_t out;

    test_harness_ringbuf_push(&rb, 10);
    test_harness_ringbuf_push(&rb, 20);

    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_INT32(10, out);

    test_harness_ringbuf_push(&rb, 30);

    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_INT32(20, out);
    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_INT32(30, out);

    TEST_ASSERT_EQUAL_UINT32(0, test_harness_ringbuf_count(&rb));
}

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    RUN_TEST(test_init_count_is_zero);
    RUN_TEST(test_push_pop_single_value);
    RUN_TEST(test_pop_empty_returns_err_empty);
    RUN_TEST(test_count_after_pushes);
    RUN_TEST(test_count_after_pops);
    RUN_TEST(test_fifo_ordering);
    RUN_TEST(test_push_full_returns_err_full);
    RUN_TEST(test_count_equals_usable_cap_when_full);
    RUN_TEST(test_fill_then_drain_completely);
    RUN_TEST(test_push_after_pop_succeeds);
    RUN_TEST(test_wrap_around);
    RUN_TEST(test_negative_values_round_trip);
    RUN_TEST(test_int32_extremes_round_trip);
    RUN_TEST(test_overflow_does_not_overwrite_existing_data);
    RUN_TEST(test_zero_value_round_trip);
    RUN_TEST(test_interleaved_push_pop_fifo);

    return UNITY_END();
}
