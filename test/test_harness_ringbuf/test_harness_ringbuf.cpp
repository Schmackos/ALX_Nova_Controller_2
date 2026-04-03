// test_harness_ringbuf.cpp — Unity tests for the ISR-safe ring buffer.
//
// Tests the static-inline API in src/test_harness_ringbuf.h:
//   - test_harness_ringbuf_init()
//   - test_harness_ringbuf_push()
//   - test_harness_ringbuf_pop()
//   - test_harness_ringbuf_count()
//
// Capacity: 16 elements, max 15 items (one slot wasted for full detection).
// Wrap-around uses a power-of-two mask (MASK = 15).

#include <unity.h>
#include "../../src/test_harness_ringbuf.h"

// ---------------------------------------------------------------------------
// Shared fixture
// ---------------------------------------------------------------------------

static TestHarnessRingbuf buf;

void setUp(void) {
    test_harness_ringbuf_init(&buf);
}

void tearDown(void) {}

// ---------------------------------------------------------------------------
// 1. Empty buffer after init
// ---------------------------------------------------------------------------

void test_init_count_is_zero(void) {
    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&buf));
}

void test_init_pop_returns_false(void) {
    int16_t out = 42;
    bool result = test_harness_ringbuf_pop(&buf, &out);
    TEST_ASSERT_FALSE(result);
}

void test_init_pop_does_not_change_out(void) {
    int16_t out = 99;
    test_harness_ringbuf_pop(&buf, &out);
    TEST_ASSERT_EQUAL_INT16(99, out);
}

// ---------------------------------------------------------------------------
// 2. Single push / pop round-trip
// ---------------------------------------------------------------------------

void test_single_push_returns_true(void) {
    TEST_ASSERT_TRUE(test_harness_ringbuf_push(&buf, 7));
}

void test_single_push_count_is_one(void) {
    test_harness_ringbuf_push(&buf, 7);
    TEST_ASSERT_EQUAL_UINT16(1, test_harness_ringbuf_count(&buf));
}

void test_single_pop_returns_correct_value(void) {
    test_harness_ringbuf_push(&buf, 1234);
    int16_t out = 0;
    bool ok = test_harness_ringbuf_pop(&buf, &out);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT16(1234, out);
}

void test_single_pop_empties_buffer(void) {
    test_harness_ringbuf_push(&buf, 1);
    int16_t out;
    test_harness_ringbuf_pop(&buf, &out);
    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&buf));
}

// ---------------------------------------------------------------------------
// 3. FIFO ordering
// ---------------------------------------------------------------------------

void test_fifo_ordering(void) {
    test_harness_ringbuf_push(&buf, 10);
    test_harness_ringbuf_push(&buf, 20);
    test_harness_ringbuf_push(&buf, 30);

    int16_t a = 0, b = 0, c = 0;
    test_harness_ringbuf_pop(&buf, &a);
    test_harness_ringbuf_pop(&buf, &b);
    test_harness_ringbuf_pop(&buf, &c);

    TEST_ASSERT_EQUAL_INT16(10, a);
    TEST_ASSERT_EQUAL_INT16(20, b);
    TEST_ASSERT_EQUAL_INT16(30, c);
}

// ---------------------------------------------------------------------------
// 4. Full buffer — push 15 items (CAPACITY - 1)
// ---------------------------------------------------------------------------

void test_full_buffer_count_is_15(void) {
    for (int i = 0; i < 15; i++) {
        test_harness_ringbuf_push(&buf, (int16_t)i);
    }
    TEST_ASSERT_EQUAL_UINT16(15, test_harness_ringbuf_count(&buf));
}

void test_push_when_full_returns_false(void) {
    for (int i = 0; i < 15; i++) {
        test_harness_ringbuf_push(&buf, (int16_t)i);
    }
    bool overflow = test_harness_ringbuf_push(&buf, 999);
    TEST_ASSERT_FALSE(overflow);
}

// ---------------------------------------------------------------------------
// 5. Overflow protection — full push does not corrupt existing data
// ---------------------------------------------------------------------------

void test_overflow_does_not_corrupt_existing_data(void) {
    // Fill completely.
    for (int16_t i = 0; i < 15; i++) {
        test_harness_ringbuf_push(&buf, i);
    }

    // Attempt overflow push (must be dropped).
    test_harness_ringbuf_push(&buf, -1);

    // Count must still be 15.
    TEST_ASSERT_EQUAL_UINT16(15, test_harness_ringbuf_count(&buf));

    // All 15 original values must survive in FIFO order.
    for (int16_t i = 0; i < 15; i++) {
        int16_t out = -999;
        bool ok = test_harness_ringbuf_pop(&buf, &out);
        TEST_ASSERT_TRUE(ok);
        TEST_ASSERT_EQUAL_INT16(i, out);
    }
}

// ---------------------------------------------------------------------------
// 6. Boundary values: INT16_MIN and INT16_MAX survive round-trip
// ---------------------------------------------------------------------------

void test_boundary_int16_min(void) {
    test_harness_ringbuf_push(&buf, INT16_MIN);
    int16_t out = 0;
    test_harness_ringbuf_pop(&buf, &out);
    TEST_ASSERT_EQUAL_INT16(INT16_MIN, out);
}

void test_boundary_int16_max(void) {
    test_harness_ringbuf_push(&buf, INT16_MAX);
    int16_t out = 0;
    test_harness_ringbuf_pop(&buf, &out);
    TEST_ASSERT_EQUAL_INT16(INT16_MAX, out);
}

void test_boundary_zero_survives(void) {
    test_harness_ringbuf_push(&buf, 0);
    int16_t out = -1;
    test_harness_ringbuf_pop(&buf, &out);
    TEST_ASSERT_EQUAL_INT16(0, out);
}

void test_boundary_negative_one_survives(void) {
    test_harness_ringbuf_push(&buf, -1);
    int16_t out = 0;
    test_harness_ringbuf_pop(&buf, &out);
    TEST_ASSERT_EQUAL_INT16(-1, out);
}

// ---------------------------------------------------------------------------
// 7. Count accuracy at various fill levels
// ---------------------------------------------------------------------------

void test_count_zero_one_half_full(void) {
    // 0
    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&buf));

    // 1
    test_harness_ringbuf_push(&buf, 1);
    TEST_ASSERT_EQUAL_UINT16(1, test_harness_ringbuf_count(&buf));

    // half (8 total)
    for (int i = 0; i < 7; i++) {
        test_harness_ringbuf_push(&buf, (int16_t)i);
    }
    TEST_ASSERT_EQUAL_UINT16(8, test_harness_ringbuf_count(&buf));

    // full (15)
    for (int i = 0; i < 7; i++) {
        test_harness_ringbuf_push(&buf, (int16_t)i);
    }
    TEST_ASSERT_EQUAL_UINT16(15, test_harness_ringbuf_count(&buf));
}

// ---------------------------------------------------------------------------
// 8. Init resets a populated buffer
// ---------------------------------------------------------------------------

void test_reinit_resets_buffer(void) {
    for (int i = 0; i < 10; i++) {
        test_harness_ringbuf_push(&buf, (int16_t)i);
    }
    TEST_ASSERT_EQUAL_UINT16(10, test_harness_ringbuf_count(&buf));

    test_harness_ringbuf_init(&buf);

    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&buf));

    int16_t out = 77;
    bool ok = test_harness_ringbuf_pop(&buf, &out);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_INT16(77, out); // out must be unchanged
}

// ---------------------------------------------------------------------------
// 9. Wrap-around — fill, drain, fill again exercises index wrap past CAPACITY
// ---------------------------------------------------------------------------

void test_wrap_around(void) {
    // First cycle: fill 15, drain 15.  head lands at index 15, tail at 15.
    for (int16_t i = 0; i < 15; i++) {
        TEST_ASSERT_TRUE(test_harness_ringbuf_push(&buf, i));
    }
    for (int i = 0; i < 15; i++) {
        int16_t out;
        TEST_ASSERT_TRUE(test_harness_ringbuf_pop(&buf, &out));
    }
    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&buf));

    // Second cycle: push wraps head from 15 → 0 → ... → 14.
    // Pop must still return values in FIFO order after the wrap.
    for (int16_t i = 100; i < 115; i++) {
        TEST_ASSERT_TRUE(test_harness_ringbuf_push(&buf, i));
    }
    TEST_ASSERT_EQUAL_UINT16(15, test_harness_ringbuf_count(&buf));

    for (int16_t i = 100; i < 115; i++) {
        int16_t out = 0;
        bool ok = test_harness_ringbuf_pop(&buf, &out);
        TEST_ASSERT_TRUE(ok);
        TEST_ASSERT_EQUAL_INT16(i, out);
    }

    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&buf));
}

// ---------------------------------------------------------------------------
// 10. Pop on empty after draining returns false, out unchanged
// ---------------------------------------------------------------------------

void test_pop_after_drain_returns_false(void) {
    test_harness_ringbuf_push(&buf, 55);
    int16_t out = 0;
    test_harness_ringbuf_pop(&buf, &out);    // consume the one item
    TEST_ASSERT_EQUAL_INT16(55, out);

    int16_t out2 = 123;
    bool ok = test_harness_ringbuf_pop(&buf, &out2);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_INT16(123, out2);      // must be unchanged
}

// ---------------------------------------------------------------------------
// 11. Partial drain then refill — count remains accurate
// ---------------------------------------------------------------------------

void test_partial_drain_then_refill_count(void) {
    for (int16_t i = 0; i < 10; i++) {
        test_harness_ringbuf_push(&buf, i);
    }
    TEST_ASSERT_EQUAL_UINT16(10, test_harness_ringbuf_count(&buf));

    // Pop 5.
    for (int i = 0; i < 5; i++) {
        int16_t out;
        test_harness_ringbuf_pop(&buf, &out);
    }
    TEST_ASSERT_EQUAL_UINT16(5, test_harness_ringbuf_count(&buf));

    // Push 5 more.
    for (int16_t i = 50; i < 55; i++) {
        test_harness_ringbuf_push(&buf, i);
    }
    TEST_ASSERT_EQUAL_UINT16(10, test_harness_ringbuf_count(&buf));
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // 1. Empty buffer
    RUN_TEST(test_init_count_is_zero);
    RUN_TEST(test_init_pop_returns_false);
    RUN_TEST(test_init_pop_does_not_change_out);

    // 2. Single push / pop
    RUN_TEST(test_single_push_returns_true);
    RUN_TEST(test_single_push_count_is_one);
    RUN_TEST(test_single_pop_returns_correct_value);
    RUN_TEST(test_single_pop_empties_buffer);

    // 3. FIFO ordering
    RUN_TEST(test_fifo_ordering);

    // 4. Full buffer
    RUN_TEST(test_full_buffer_count_is_15);
    RUN_TEST(test_push_when_full_returns_false);

    // 5. Overflow protection
    RUN_TEST(test_overflow_does_not_corrupt_existing_data);

    // 6. Boundary values
    RUN_TEST(test_boundary_int16_min);
    RUN_TEST(test_boundary_int16_max);
    RUN_TEST(test_boundary_zero_survives);
    RUN_TEST(test_boundary_negative_one_survives);

    // 7. Count accuracy
    RUN_TEST(test_count_zero_one_half_full);

    // 8. Init resets
    RUN_TEST(test_reinit_resets_buffer);

    // 9. Wrap-around
    RUN_TEST(test_wrap_around);

    // 10. Pop after drain
    RUN_TEST(test_pop_after_drain_returns_false);

    // 11. Partial drain and refill
    RUN_TEST(test_partial_drain_then_refill_count);

    return UNITY_END();
}
