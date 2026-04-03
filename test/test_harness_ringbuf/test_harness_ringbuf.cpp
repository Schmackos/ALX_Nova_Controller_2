// test_harness_ringbuf.cpp
// Unity tests for the ISR-safe ring buffer (test_harness_ringbuf.h).
//
// Coverage:
//   - Init sets head/tail to 0, count to 0
//   - Push single element, verify count = 1
//   - Push and pop, verify correct value returned
//   - Push until full, verify push returns false on overflow
//   - Pop from empty returns false
//   - Wrap-around: fill completely, pop all, refill -- verify modular indexing
//   - Boundary: push exactly capacity items succeeds
//   - Count accuracy at various fill levels
//   - Multiple push/pop cycles (FIFO ordering)
//   - Null pointer safety

#include <unity.h>
#include <cstring>
#include <cstdint>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

#include "../../src/test_harness_ringbuf.h"

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

static TestHarnessRingBuf rb;

// ---------------------------------------------------------------------------
// setUp / tearDown
// ---------------------------------------------------------------------------

void setUp(void) {
    test_harness_ringbuf_init(&rb, 8);
}

void tearDown(void) {
    // nothing
}

// ---------------------------------------------------------------------------
// Test 1: Init produces head=0, tail=0, count=0
// ---------------------------------------------------------------------------

void test_init_state(void) {
    TestHarnessRingBuf local;
    test_harness_ringbuf_init(&local, 8);
    TEST_ASSERT_EQUAL_UINT16(0, local.head);
    TEST_ASSERT_EQUAL_UINT16(0, local.tail);
    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&local));
}

// ---------------------------------------------------------------------------
// Test 2: Init sets capacity correctly
// ---------------------------------------------------------------------------

void test_init_capacity(void) {
    TestHarnessRingBuf local;
    test_harness_ringbuf_init(&local, 16);
    TEST_ASSERT_EQUAL_UINT16(16, local.capacity);
}

// ---------------------------------------------------------------------------
// Test 3: Push single element increases count to 1
// ---------------------------------------------------------------------------

void test_push_single_increments_count(void) {
    bool ok = test_harness_ringbuf_push(&rb, 42);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT16(1, test_harness_ringbuf_count(&rb));
}

// ---------------------------------------------------------------------------
// Test 4: Push then pop returns the correct value
// ---------------------------------------------------------------------------

void test_push_pop_correct_value(void) {
    test_harness_ringbuf_push(&rb, 1234);
    int32_t out = 0;
    bool ok = test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT32(1234, out);
}

// ---------------------------------------------------------------------------
// Test 5: Pop from empty returns false and does not modify *out
// ---------------------------------------------------------------------------

void test_pop_empty_returns_false(void) {
    int32_t out = 0xDEAD;
    bool ok = test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_INT32(0xDEAD, out);  // unchanged
}

// ---------------------------------------------------------------------------
// Test 6: Push exactly capacity items all succeed
// ---------------------------------------------------------------------------

void test_push_exactly_capacity(void) {
    uint16_t cap = rb.capacity;
    for (uint16_t i = 0; i < cap; i++) {
        TEST_ASSERT_TRUE(test_harness_ringbuf_push(&rb, (int32_t)i));
    }
    TEST_ASSERT_EQUAL_UINT16(cap, test_harness_ringbuf_count(&rb));
}

// ---------------------------------------------------------------------------
// Test 7: Push beyond capacity returns false (buffer full)
// ---------------------------------------------------------------------------

void test_push_overflow_returns_false(void) {
    uint16_t cap = rb.capacity;
    for (uint16_t i = 0; i < cap; i++) {
        test_harness_ringbuf_push(&rb, (int32_t)i);
    }
    bool overflow = test_harness_ringbuf_push(&rb, 999);
    TEST_ASSERT_FALSE(overflow);
    TEST_ASSERT_EQUAL_UINT16(cap, test_harness_ringbuf_count(&rb));
}

// ---------------------------------------------------------------------------
// Test 8: Pop after full reduces count correctly
// ---------------------------------------------------------------------------

void test_count_after_pop(void) {
    uint16_t cap = rb.capacity;
    for (uint16_t i = 0; i < cap; i++) {
        test_harness_ringbuf_push(&rb, (int32_t)i);
    }
    int32_t out;
    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_UINT16(cap - 1, test_harness_ringbuf_count(&rb));
    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_UINT16(cap - 2, test_harness_ringbuf_count(&rb));
}

// ---------------------------------------------------------------------------
// Test 9: FIFO ordering -- values come out in insertion order
// ---------------------------------------------------------------------------

void test_fifo_ordering(void) {
    test_harness_ringbuf_push(&rb, 10);
    test_harness_ringbuf_push(&rb, 20);
    test_harness_ringbuf_push(&rb, 30);

    int32_t a = 0, b = 0, c = 0;
    test_harness_ringbuf_pop(&rb, &a);
    test_harness_ringbuf_pop(&rb, &b);
    test_harness_ringbuf_pop(&rb, &c);

    TEST_ASSERT_EQUAL_INT32(10, a);
    TEST_ASSERT_EQUAL_INT32(20, b);
    TEST_ASSERT_EQUAL_INT32(30, c);
}

// ---------------------------------------------------------------------------
// Test 10: Wrap-around -- fill, drain, refill uses modular indexing correctly
// ---------------------------------------------------------------------------

void test_wrap_around(void) {
    uint16_t cap = rb.capacity;  // 8

    // Fill completely
    for (uint16_t i = 0; i < cap; i++) {
        test_harness_ringbuf_push(&rb, (int32_t)(i + 100));
    }
    TEST_ASSERT_EQUAL_UINT16(cap, test_harness_ringbuf_count(&rb));

    // Drain completely
    for (uint16_t i = 0; i < cap; i++) {
        int32_t out = 0;
        TEST_ASSERT_TRUE(test_harness_ringbuf_pop(&rb, &out));
        TEST_ASSERT_EQUAL_INT32((int32_t)(i + 100), out);
    }
    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&rb));

    // Refill -- head and tail have both advanced past capacity boundary;
    // modular indexing must still place items at buf[0..cap-1]
    for (uint16_t i = 0; i < cap; i++) {
        TEST_ASSERT_TRUE(test_harness_ringbuf_push(&rb, (int32_t)(i + 200)));
    }
    TEST_ASSERT_EQUAL_UINT16(cap, test_harness_ringbuf_count(&rb));

    // Verify second batch reads back correctly
    for (uint16_t i = 0; i < cap; i++) {
        int32_t out = 0;
        TEST_ASSERT_TRUE(test_harness_ringbuf_pop(&rb, &out));
        TEST_ASSERT_EQUAL_INT32((int32_t)(i + 200), out);
    }
    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&rb));
}

// ---------------------------------------------------------------------------
// Test 11: Partial fill and drain cycle, count stays accurate
// ---------------------------------------------------------------------------

void test_count_at_various_fill_levels(void) {
    // Start empty
    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&rb));

    test_harness_ringbuf_push(&rb, 1);
    TEST_ASSERT_EQUAL_UINT16(1, test_harness_ringbuf_count(&rb));

    test_harness_ringbuf_push(&rb, 2);
    test_harness_ringbuf_push(&rb, 3);
    TEST_ASSERT_EQUAL_UINT16(3, test_harness_ringbuf_count(&rb));

    int32_t out;
    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_UINT16(2, test_harness_ringbuf_count(&rb));

    test_harness_ringbuf_pop(&rb, &out);
    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&rb));
}

// ---------------------------------------------------------------------------
// Test 12: Multiple push/pop cycles without re-init
// ---------------------------------------------------------------------------

void test_multiple_cycles(void) {
    // Cycle 1
    test_harness_ringbuf_push(&rb, 0xAA);
    int32_t out = 0;
    TEST_ASSERT_TRUE(test_harness_ringbuf_pop(&rb, &out));
    TEST_ASSERT_EQUAL_INT32(0xAA, out);

    // Cycle 2
    test_harness_ringbuf_push(&rb, 0xBB);
    test_harness_ringbuf_push(&rb, 0xCC);
    TEST_ASSERT_TRUE(test_harness_ringbuf_pop(&rb, &out));
    TEST_ASSERT_EQUAL_INT32(0xBB, out);
    TEST_ASSERT_TRUE(test_harness_ringbuf_pop(&rb, &out));
    TEST_ASSERT_EQUAL_INT32(0xCC, out);

    // Buffer should be empty again
    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&rb));
    TEST_ASSERT_FALSE(test_harness_ringbuf_pop(&rb, &out));
}

// ---------------------------------------------------------------------------
// Test 13: Negative values stored and retrieved correctly
// ---------------------------------------------------------------------------

void test_negative_values(void) {
    test_harness_ringbuf_push(&rb, -1);
    test_harness_ringbuf_push(&rb, -32768);
    test_harness_ringbuf_push(&rb, INT32_MIN);

    int32_t a = 0, b = 0, c = 0;
    test_harness_ringbuf_pop(&rb, &a);
    test_harness_ringbuf_pop(&rb, &b);
    test_harness_ringbuf_pop(&rb, &c);

    TEST_ASSERT_EQUAL_INT32(-1, a);
    TEST_ASSERT_EQUAL_INT32(-32768, b);
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, c);
}

// ---------------------------------------------------------------------------
// Test 14: Capacity clamped to max when initialised with oversized value
// ---------------------------------------------------------------------------

void test_capacity_clamped_to_max(void) {
    TestHarnessRingBuf local;
    test_harness_ringbuf_init(&local, 0xFFFF);
    TEST_ASSERT_EQUAL_UINT16(TEST_HARNESS_RINGBUF_MAX_CAPACITY, local.capacity);
}

// ---------------------------------------------------------------------------
// Test 15: Pop decrements count to 0 after one push
// ---------------------------------------------------------------------------

void test_count_returns_to_zero_after_pop(void) {
    test_harness_ringbuf_push(&rb, 77);
    TEST_ASSERT_EQUAL_UINT16(1, test_harness_ringbuf_count(&rb));
    int32_t out;
    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&rb));
}

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    RUN_TEST(test_init_state);
    RUN_TEST(test_init_capacity);
    RUN_TEST(test_push_single_increments_count);
    RUN_TEST(test_push_pop_correct_value);
    RUN_TEST(test_pop_empty_returns_false);
    RUN_TEST(test_push_exactly_capacity);
    RUN_TEST(test_push_overflow_returns_false);
    RUN_TEST(test_count_after_pop);
    RUN_TEST(test_fifo_ordering);
    RUN_TEST(test_wrap_around);
    RUN_TEST(test_count_at_various_fill_levels);
    RUN_TEST(test_multiple_cycles);
    RUN_TEST(test_negative_values);
    RUN_TEST(test_capacity_clamped_to_max);
    RUN_TEST(test_count_returns_to_zero_after_pop);

    return UNITY_END();
}
