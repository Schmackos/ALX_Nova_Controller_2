#include <unity.h>
#include "../../src/test_harness_ringbuf.h"

static TestHarnessRingbuf rb;

void setUp(void) {
    test_harness_ringbuf_init(&rb);
}

void tearDown(void) {}

/* ===== Empty buffer ===== */

void test_empty_count_is_zero(void) {
    TEST_ASSERT_EQUAL_UINT8(0, test_harness_ringbuf_count(&rb));
}

void test_empty_pop_returns_false(void) {
    uint8_t val = 0xAA;
    TEST_ASSERT_FALSE(test_harness_ringbuf_pop(&rb, &val));
    /* Output value must not be modified on failure */
    TEST_ASSERT_EQUAL_UINT8(0xAA, val);
}

/* ===== Single push/pop cycle ===== */

void test_single_push_pop(void) {
    TEST_ASSERT_TRUE(test_harness_ringbuf_push(&rb, 42u));
    TEST_ASSERT_EQUAL_UINT8(1, test_harness_ringbuf_count(&rb));

    uint8_t val = 0;
    TEST_ASSERT_TRUE(test_harness_ringbuf_pop(&rb, &val));
    TEST_ASSERT_EQUAL_UINT8(42u, val);
    TEST_ASSERT_EQUAL_UINT8(0, test_harness_ringbuf_count(&rb));
}

/* ===== Fill to capacity ===== */

/*
 * With TEST_HARNESS_RINGBUF_SIZE == 16 the usable capacity is 15 items
 * (one slot reserved to distinguish full from empty).
 */
void test_fill_to_capacity(void) {
    const uint8_t capacity = TEST_HARNESS_RINGBUF_SIZE - 1;
    for (uint8_t i = 0; i < capacity; i++) {
        TEST_ASSERT_TRUE(test_harness_ringbuf_push(&rb, i));
    }
    TEST_ASSERT_EQUAL_UINT8(capacity, test_harness_ringbuf_count(&rb));
}

/* ===== Overflow: push when full returns false ===== */

void test_overflow_push_returns_false(void) {
    const uint8_t capacity = TEST_HARNESS_RINGBUF_SIZE - 1;
    for (uint8_t i = 0; i < capacity; i++) {
        test_harness_ringbuf_push(&rb, i);
    }
    /* Buffer is now full — next push must fail */
    TEST_ASSERT_FALSE(test_harness_ringbuf_push(&rb, 0xFFu));
    /* Count must remain at capacity */
    TEST_ASSERT_EQUAL_UINT8(capacity, test_harness_ringbuf_count(&rb));
}

/* ===== Overflow does not corrupt existing data ===== */

void test_overflow_does_not_corrupt(void) {
    const uint8_t capacity = TEST_HARNESS_RINGBUF_SIZE - 1;
    for (uint8_t i = 0; i < capacity; i++) {
        test_harness_ringbuf_push(&rb, (uint8_t)(i + 1u));
    }
    /* Attempt to push one more — should fail silently */
    test_harness_ringbuf_push(&rb, 0xFFu);

    /* All original values must still pop in FIFO order */
    for (uint8_t i = 0; i < capacity; i++) {
        uint8_t val = 0;
        TEST_ASSERT_TRUE(test_harness_ringbuf_pop(&rb, &val));
        TEST_ASSERT_EQUAL_UINT8((uint8_t)(i + 1u), val);
    }
}

/* ===== FIFO ordering ===== */

void test_fifo_order(void) {
    test_harness_ringbuf_push(&rb, 10u);
    test_harness_ringbuf_push(&rb, 20u);
    test_harness_ringbuf_push(&rb, 30u);

    uint8_t v1 = 0, v2 = 0, v3 = 0;
    test_harness_ringbuf_pop(&rb, &v1);
    test_harness_ringbuf_pop(&rb, &v2);
    test_harness_ringbuf_pop(&rb, &v3);

    TEST_ASSERT_EQUAL_UINT8(10u, v1);
    TEST_ASSERT_EQUAL_UINT8(20u, v2);
    TEST_ASSERT_EQUAL_UINT8(30u, v3);
}

/* ===== Wrap-around: fill, drain, refill ===== */

void test_wrap_around(void) {
    const uint8_t capacity = TEST_HARNESS_RINGBUF_SIZE - 1;

    /* First fill */
    for (uint8_t i = 0; i < capacity; i++) {
        TEST_ASSERT_TRUE(test_harness_ringbuf_push(&rb, i));
    }
    /* Drain completely */
    for (uint8_t i = 0; i < capacity; i++) {
        uint8_t val = 0;
        TEST_ASSERT_TRUE(test_harness_ringbuf_pop(&rb, &val));
        TEST_ASSERT_EQUAL_UINT8(i, val);
    }
    TEST_ASSERT_EQUAL_UINT8(0, test_harness_ringbuf_count(&rb));

    /* Refill — indices have wrapped; verify correctness */
    for (uint8_t i = 0; i < capacity; i++) {
        TEST_ASSERT_TRUE(test_harness_ringbuf_push(&rb, (uint8_t)(i + 100u)));
    }
    TEST_ASSERT_EQUAL_UINT8(capacity, test_harness_ringbuf_count(&rb));

    for (uint8_t i = 0; i < capacity; i++) {
        uint8_t val = 0;
        TEST_ASSERT_TRUE(test_harness_ringbuf_pop(&rb, &val));
        TEST_ASSERT_EQUAL_UINT8((uint8_t)(i + 100u), val);
    }
    TEST_ASSERT_EQUAL_UINT8(0, test_harness_ringbuf_count(&rb));
}

/* ===== Partial wrap-around ===== */

void test_partial_wrap_around(void) {
    /* Push half capacity, drain all, then push again to force index wrap */
    const uint8_t half = (TEST_HARNESS_RINGBUF_SIZE - 1) / 2;

    for (uint8_t i = 0; i < half; i++) {
        test_harness_ringbuf_push(&rb, i);
    }
    uint8_t dummy = 0;
    for (uint8_t i = 0; i < half; i++) {
        test_harness_ringbuf_pop(&rb, &dummy);
    }

    /* Now push again — head and tail have advanced past zero */
    for (uint8_t i = 0; i < half; i++) {
        TEST_ASSERT_TRUE(test_harness_ringbuf_push(&rb, (uint8_t)(i + 50u)));
    }
    TEST_ASSERT_EQUAL_UINT8(half, test_harness_ringbuf_count(&rb));

    for (uint8_t i = 0; i < half; i++) {
        uint8_t val = 0;
        TEST_ASSERT_TRUE(test_harness_ringbuf_pop(&rb, &val));
        TEST_ASSERT_EQUAL_UINT8((uint8_t)(i + 50u), val);
    }
}

/* ===== Boundary: push exactly to capacity, pop exactly to empty ===== */

void test_boundary_push_to_capacity_pop_to_empty(void) {
    const uint8_t capacity = TEST_HARNESS_RINGBUF_SIZE - 1;

    /* Push exactly capacity items */
    for (uint8_t i = 0; i < capacity; i++) {
        TEST_ASSERT_TRUE(test_harness_ringbuf_push(&rb, (uint8_t)(i + 1u)));
    }
    TEST_ASSERT_EQUAL_UINT8(capacity, test_harness_ringbuf_count(&rb));

    /* Pop exactly capacity items */
    for (uint8_t i = 0; i < capacity; i++) {
        uint8_t val = 0;
        TEST_ASSERT_TRUE(test_harness_ringbuf_pop(&rb, &val));
        TEST_ASSERT_EQUAL_UINT8((uint8_t)(i + 1u), val);
    }
    TEST_ASSERT_EQUAL_UINT8(0, test_harness_ringbuf_count(&rb));

    /* One more pop must fail */
    uint8_t val = 0xBBu;
    TEST_ASSERT_FALSE(test_harness_ringbuf_pop(&rb, &val));
    TEST_ASSERT_EQUAL_UINT8(0xBBu, val);
}

/* ===== Count accuracy at various fill levels ===== */

void test_count_accuracy(void) {
    TEST_ASSERT_EQUAL_UINT8(0, test_harness_ringbuf_count(&rb));

    test_harness_ringbuf_push(&rb, 1u);
    TEST_ASSERT_EQUAL_UINT8(1, test_harness_ringbuf_count(&rb));

    test_harness_ringbuf_push(&rb, 2u);
    TEST_ASSERT_EQUAL_UINT8(2, test_harness_ringbuf_count(&rb));

    test_harness_ringbuf_push(&rb, 3u);
    TEST_ASSERT_EQUAL_UINT8(3, test_harness_ringbuf_count(&rb));

    uint8_t dummy = 0;
    test_harness_ringbuf_pop(&rb, &dummy);
    TEST_ASSERT_EQUAL_UINT8(2, test_harness_ringbuf_count(&rb));

    test_harness_ringbuf_pop(&rb, &dummy);
    TEST_ASSERT_EQUAL_UINT8(1, test_harness_ringbuf_count(&rb));

    test_harness_ringbuf_pop(&rb, &dummy);
    TEST_ASSERT_EQUAL_UINT8(0, test_harness_ringbuf_count(&rb));
}

/* ===== Init resets a used buffer ===== */

void test_init_resets_used_buffer(void) {
    /* Fill the buffer partially */
    test_harness_ringbuf_push(&rb, 0xAAu);
    test_harness_ringbuf_push(&rb, 0xBBu);
    TEST_ASSERT_EQUAL_UINT8(2, test_harness_ringbuf_count(&rb));

    /* Reinitialize — must appear empty */
    test_harness_ringbuf_init(&rb);
    TEST_ASSERT_EQUAL_UINT8(0, test_harness_ringbuf_count(&rb));

    /* Pop must fail after re-init */
    uint8_t val = 0xCCu;
    TEST_ASSERT_FALSE(test_harness_ringbuf_pop(&rb, &val));
    TEST_ASSERT_EQUAL_UINT8(0xCCu, val);

    /* Push must succeed after re-init */
    TEST_ASSERT_TRUE(test_harness_ringbuf_push(&rb, 0x55u));
    TEST_ASSERT_EQUAL_UINT8(1, test_harness_ringbuf_count(&rb));
}

/* ===== Zero value round-trip ===== */

void test_zero_value_round_trip(void) {
    TEST_ASSERT_TRUE(test_harness_ringbuf_push(&rb, 0u));
    uint8_t val = 0xFFu;
    TEST_ASSERT_TRUE(test_harness_ringbuf_pop(&rb, &val));
    TEST_ASSERT_EQUAL_UINT8(0u, val);
}

/* ===== Max value round-trip ===== */

void test_max_value_round_trip(void) {
    TEST_ASSERT_TRUE(test_harness_ringbuf_push(&rb, 0xFFu));
    uint8_t val = 0u;
    TEST_ASSERT_TRUE(test_harness_ringbuf_pop(&rb, &val));
    TEST_ASSERT_EQUAL_UINT8(0xFFu, val);
}

/* ===== Test runner ===== */

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_empty_count_is_zero);
    RUN_TEST(test_empty_pop_returns_false);
    RUN_TEST(test_single_push_pop);
    RUN_TEST(test_fill_to_capacity);
    RUN_TEST(test_overflow_push_returns_false);
    RUN_TEST(test_overflow_does_not_corrupt);
    RUN_TEST(test_fifo_order);
    RUN_TEST(test_wrap_around);
    RUN_TEST(test_partial_wrap_around);
    RUN_TEST(test_boundary_push_to_capacity_pop_to_empty);
    RUN_TEST(test_count_accuracy);
    RUN_TEST(test_init_resets_used_buffer);
    RUN_TEST(test_zero_value_round_trip);
    RUN_TEST(test_max_value_round_trip);

    return UNITY_END();
}
