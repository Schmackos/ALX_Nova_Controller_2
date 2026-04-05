/*
 * test_harness_ringbuf.cpp
 *
 * Unity tests for the ISR-safe ring buffer declared in
 * src/test_harness_ringbuf.h.
 *
 * Coverage checklist
 * ------------------
 *  1.  Empty buffer — pop returns false, count == 0
 *  2.  Single push then pop — round-trip, value preserved, count back to 0
 *  3.  Fill to capacity — push returns true for every valid slot, count correct
 *  4.  Overflow rejection — push beyond capacity returns false
 *  5.  Overflow non-corruption — existing data intact after rejected push
 *  6.  FIFO ordering — items come out in insertion order
 *  7.  Wrap-around — indices wrap correctly across the SIZE boundary
 *  8.  Partial wrap-around — mixed push/pop sequence spanning the boundary
 *  9.  Boundary values — 0x00 and 0xFF survive a round-trip
 * 10.  Count accuracy after mixed operations
 * 11.  Init reset — re-initialising a dirty buffer gives a clean slate
 * 12.  Full-cycle wrap stress — two complete cycles of fill + drain
 */

#include <unity.h>
#include "../../src/test_harness_ringbuf.h"

/* A single buffer instance reused across all tests. */
static TestHarnessRingbuf rb;

/* -------------------------------------------------------------------------
 * setUp / tearDown
 * ---------------------------------------------------------------------- */

void setUp(void)
{
    test_harness_ringbuf_init(&rb);
}

void tearDown(void) {}

/* =========================================================================
 * Test 1 — Empty buffer: pop returns false; count is 0
 * ====================================================================== */
void test_empty_pop_returns_false(void)
{
    uint8_t val = 0xAB; /* sentinel — must not be overwritten */

    TEST_ASSERT_EQUAL_UINT8(0, test_harness_ringbuf_count(&rb));
    TEST_ASSERT_FALSE(test_harness_ringbuf_pop(&rb, &val));
    /* Sentinel unchanged */
    TEST_ASSERT_EQUAL_UINT8(0xAB, val);
}

/* =========================================================================
 * Test 2 — Single push then pop: value preserved, count returns to 0
 * ====================================================================== */
void test_single_push_pop_roundtrip(void)
{
    uint8_t out = 0;

    TEST_ASSERT_TRUE(test_harness_ringbuf_push(&rb, 0x42u));
    TEST_ASSERT_EQUAL_UINT8(1, test_harness_ringbuf_count(&rb));

    TEST_ASSERT_TRUE(test_harness_ringbuf_pop(&rb, &out));
    TEST_ASSERT_EQUAL_UINT8(0x42u, out);
    TEST_ASSERT_EQUAL_UINT8(0, test_harness_ringbuf_count(&rb));
}

/* =========================================================================
 * Test 3 — Fill to capacity: all pushes succeed; count == CAPACITY
 * ====================================================================== */
void test_fill_to_capacity(void)
{
    for (uint8_t i = 0; i < TEST_HARNESS_RINGBUF_CAPACITY; i++) {
        TEST_ASSERT_TRUE_MESSAGE(
            test_harness_ringbuf_push(&rb, i),
            "push should succeed while below capacity");
    }
    TEST_ASSERT_EQUAL_UINT8(TEST_HARNESS_RINGBUF_CAPACITY,
                             test_harness_ringbuf_count(&rb));
}

/* =========================================================================
 * Test 4 — Overflow rejection: push beyond capacity returns false
 * ====================================================================== */
void test_overflow_push_returns_false(void)
{
    /* Fill to capacity */
    for (uint8_t i = 0; i < TEST_HARNESS_RINGBUF_CAPACITY; i++) {
        test_harness_ringbuf_push(&rb, i);
    }
    /* One more push must fail */
    TEST_ASSERT_FALSE(test_harness_ringbuf_push(&rb, 0xFFu));
    /* Count must remain at capacity */
    TEST_ASSERT_EQUAL_UINT8(TEST_HARNESS_RINGBUF_CAPACITY,
                             test_harness_ringbuf_count(&rb));
}

/* =========================================================================
 * Test 5 — Overflow non-corruption: existing data intact after rejection
 * ====================================================================== */
void test_overflow_does_not_corrupt_data(void)
{
    /* Push known values to fill */
    for (uint8_t i = 0; i < TEST_HARNESS_RINGBUF_CAPACITY; i++) {
        test_harness_ringbuf_push(&rb, (uint8_t)(i + 1u));
    }
    /* Attempt overflow with a canary value */
    test_harness_ringbuf_push(&rb, 0xCCu);

    /* Pop everything and verify original sequence is intact */
    for (uint8_t i = 0; i < TEST_HARNESS_RINGBUF_CAPACITY; i++) {
        uint8_t out = 0;
        TEST_ASSERT_TRUE(test_harness_ringbuf_pop(&rb, &out));
        TEST_ASSERT_EQUAL_UINT8((uint8_t)(i + 1u), out);
    }
    /* Buffer now empty — canary never entered */
    TEST_ASSERT_EQUAL_UINT8(0, test_harness_ringbuf_count(&rb));
}

/* =========================================================================
 * Test 6 — FIFO ordering: items come out in the order they went in
 * ====================================================================== */
void test_fifo_ordering(void)
{
    const uint8_t seq[] = {10, 20, 30, 40, 50};
    const uint8_t n     = (uint8_t)(sizeof(seq) / sizeof(seq[0]));

    for (uint8_t i = 0; i < n; i++) {
        test_harness_ringbuf_push(&rb, seq[i]);
    }
    for (uint8_t i = 0; i < n; i++) {
        uint8_t out = 0;
        TEST_ASSERT_TRUE(test_harness_ringbuf_pop(&rb, &out));
        TEST_ASSERT_EQUAL_UINT8(seq[i], out);
    }
}

/* =========================================================================
 * Test 7 — Wrap-around: head and tail indices roll past SIZE boundary
 *
 * Strategy: advance head/tail close to SIZE by filling+draining, then do
 * another push/pop cycle to force the modulo wrap.
 * ====================================================================== */
void test_wrap_around(void)
{
    /* Advance the internal indices most of the way around the ring. */
    for (uint8_t i = 0; i < TEST_HARNESS_RINGBUF_SIZE - 2u; i++) {
        TEST_ASSERT_TRUE(test_harness_ringbuf_push(&rb, (uint8_t)(i + 1u)));
        uint8_t discard = 0;
        TEST_ASSERT_TRUE(test_harness_ringbuf_pop(&rb, &discard));
    }

    /* Now push/pop a sentinel that exercises the wrap boundary. */
    TEST_ASSERT_TRUE(test_harness_ringbuf_push(&rb, 0xA5u));
    uint8_t out = 0;
    TEST_ASSERT_TRUE(test_harness_ringbuf_pop(&rb, &out));
    TEST_ASSERT_EQUAL_UINT8(0xA5u, out);
    TEST_ASSERT_EQUAL_UINT8(0, test_harness_ringbuf_count(&rb));
}

/* =========================================================================
 * Test 8 — Partial wrap-around: mixed push/pop spanning the ring boundary
 * ====================================================================== */
void test_partial_wrap_around(void)
{
    /* Push half capacity's worth to move indices partway round. */
    const uint8_t half = TEST_HARNESS_RINGBUF_CAPACITY / 2u;

    for (uint8_t i = 0; i < half; i++) {
        test_harness_ringbuf_push(&rb, (uint8_t)i);
    }
    /* Drain those, advancing tail */
    for (uint8_t i = 0; i < half; i++) {
        uint8_t discard = 0;
        test_harness_ringbuf_pop(&rb, &discard);
    }

    /* Push a fresh batch that will span the array boundary */
    const uint8_t batch = TEST_HARNESS_RINGBUF_CAPACITY;
    for (uint8_t i = 0; i < batch; i++) {
        TEST_ASSERT_TRUE_MESSAGE(
            test_harness_ringbuf_push(&rb, (uint8_t)(0x80u + i)),
            "push should succeed in partially-wrapped state");
    }
    TEST_ASSERT_EQUAL_UINT8(batch, test_harness_ringbuf_count(&rb));

    /* Pop all and confirm values */
    for (uint8_t i = 0; i < batch; i++) {
        uint8_t out = 0;
        TEST_ASSERT_TRUE(test_harness_ringbuf_pop(&rb, &out));
        TEST_ASSERT_EQUAL_UINT8((uint8_t)(0x80u + i), out);
    }
    TEST_ASSERT_EQUAL_UINT8(0, test_harness_ringbuf_count(&rb));
}

/* =========================================================================
 * Test 9 — Boundary values: 0x00 and 0xFF survive a round-trip
 * ====================================================================== */
void test_boundary_value_zero_and_max(void)
{
    uint8_t out = 0xAA;

    /* Zero */
    TEST_ASSERT_TRUE(test_harness_ringbuf_push(&rb, 0x00u));
    TEST_ASSERT_TRUE(test_harness_ringbuf_pop(&rb, &out));
    TEST_ASSERT_EQUAL_UINT8(0x00u, out);

    /* Max */
    out = 0x00;
    TEST_ASSERT_TRUE(test_harness_ringbuf_push(&rb, 0xFFu));
    TEST_ASSERT_TRUE(test_harness_ringbuf_pop(&rb, &out));
    TEST_ASSERT_EQUAL_UINT8(0xFFu, out);
}

/* =========================================================================
 * Test 10 — Count accuracy after mixed push/pop operations
 * ====================================================================== */
void test_count_accuracy_mixed_ops(void)
{
    /* Push 5 */
    for (uint8_t i = 0; i < 5u; i++) {
        test_harness_ringbuf_push(&rb, i);
    }
    TEST_ASSERT_EQUAL_UINT8(5, test_harness_ringbuf_count(&rb));

    /* Pop 3 */
    uint8_t discard = 0;
    test_harness_ringbuf_pop(&rb, &discard);
    test_harness_ringbuf_pop(&rb, &discard);
    test_harness_ringbuf_pop(&rb, &discard);
    TEST_ASSERT_EQUAL_UINT8(2, test_harness_ringbuf_count(&rb));

    /* Push 4 more */
    for (uint8_t i = 0; i < 4u; i++) {
        test_harness_ringbuf_push(&rb, (uint8_t)(10u + i));
    }
    TEST_ASSERT_EQUAL_UINT8(6, test_harness_ringbuf_count(&rb));

    /* Drain completely */
    for (uint8_t i = 0; i < 6u; i++) {
        test_harness_ringbuf_pop(&rb, &discard);
    }
    TEST_ASSERT_EQUAL_UINT8(0, test_harness_ringbuf_count(&rb));
}

/* =========================================================================
 * Test 11 — Init reset: re-initialising a used buffer clears it
 * ====================================================================== */
void test_init_resets_dirty_buffer(void)
{
    /* Dirty the buffer */
    test_harness_ringbuf_push(&rb, 0x01u);
    test_harness_ringbuf_push(&rb, 0x02u);
    test_harness_ringbuf_push(&rb, 0x03u);
    TEST_ASSERT_EQUAL_UINT8(3, test_harness_ringbuf_count(&rb));

    /* Re-initialise */
    test_harness_ringbuf_init(&rb);

    /* Must appear empty */
    TEST_ASSERT_EQUAL_UINT8(0, test_harness_ringbuf_count(&rb));

    uint8_t val = 0xBB;
    TEST_ASSERT_FALSE(test_harness_ringbuf_pop(&rb, &val));
    TEST_ASSERT_EQUAL_UINT8(0xBB, val); /* sentinel unchanged */

    /* And accept new data correctly */
    TEST_ASSERT_TRUE(test_harness_ringbuf_push(&rb, 0x55u));
    TEST_ASSERT_EQUAL_UINT8(1, test_harness_ringbuf_count(&rb));
    uint8_t out = 0;
    TEST_ASSERT_TRUE(test_harness_ringbuf_pop(&rb, &out));
    TEST_ASSERT_EQUAL_UINT8(0x55u, out);
}

/* =========================================================================
 * Test 12 — Full-cycle wrap stress: two complete fill+drain cycles
 *
 * Ensures that internal indices remain consistent after a complete cycle
 * (head == tail, both having wrapped at least once).
 * ====================================================================== */
void test_full_cycle_wrap_stress(void)
{
    for (uint8_t cycle = 0; cycle < 2u; cycle++) {
        /* Fill */
        for (uint8_t i = 0; i < TEST_HARNESS_RINGBUF_CAPACITY; i++) {
            TEST_ASSERT_TRUE(
                test_harness_ringbuf_push(&rb, (uint8_t)(cycle * 100u + i)));
        }
        TEST_ASSERT_EQUAL_UINT8(TEST_HARNESS_RINGBUF_CAPACITY,
                                 test_harness_ringbuf_count(&rb));

        /* Drain and verify sequence */
        for (uint8_t i = 0; i < TEST_HARNESS_RINGBUF_CAPACITY; i++) {
            uint8_t out = 0;
            TEST_ASSERT_TRUE(test_harness_ringbuf_pop(&rb, &out));
            TEST_ASSERT_EQUAL_UINT8((uint8_t)(cycle * 100u + i), out);
        }
        TEST_ASSERT_EQUAL_UINT8(0, test_harness_ringbuf_count(&rb));
    }
}

/* =========================================================================
 * main
 * ====================================================================== */

int main(int argc, char **argv)
{
    UNITY_BEGIN();

    RUN_TEST(test_empty_pop_returns_false);
    RUN_TEST(test_single_push_pop_roundtrip);
    RUN_TEST(test_fill_to_capacity);
    RUN_TEST(test_overflow_push_returns_false);
    RUN_TEST(test_overflow_does_not_corrupt_data);
    RUN_TEST(test_fifo_ordering);
    RUN_TEST(test_wrap_around);
    RUN_TEST(test_partial_wrap_around);
    RUN_TEST(test_boundary_value_zero_and_max);
    RUN_TEST(test_count_accuracy_mixed_ops);
    RUN_TEST(test_init_resets_dirty_buffer);
    RUN_TEST(test_full_cycle_wrap_stress);

    return UNITY_END();
}
