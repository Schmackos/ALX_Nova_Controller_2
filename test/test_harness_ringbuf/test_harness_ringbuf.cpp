/*
 * test_harness_ringbuf.cpp
 *
 * Unity unit tests for the TestHarnessRingBuf SPSC ring buffer defined in
 * src/test_harness_ringbuf.h.
 *
 * Buffer capacity: TEST_HARNESS_RINGBUF_SIZE = 64 bytes, usable = 63 items
 * (one slot is sacrificed to distinguish full from empty).
 *
 * Covered:
 *   1.  Empty buffer   — pop returns false, count is 0
 *   2.  Push single    — push returns true, count becomes 1
 *   3.  Push then pop  — popped value matches pushed value
 *   4.  Fill to cap    — push 63 items all return true
 *   5.  Overflow       — 64th push returns false (buffer full)
 *   6.  Count at full  — count returns 63 (SIZE - 1)
 *   7.  Pop all        — drain 63 items, then pop returns false (empty again)
 *   8.  Wrap-around    — fill + drain + fill again; indices wrap without corruption
 *   9.  FIFO order     — values come out in push order
 *  10.  Count tracking — count reflects every push and pop
 *  11.  Boundary       — push/pop at exact capacity edge; count stays consistent
 *  12.  Re-init        — after use, init resets buffer to empty state
 */

#include <unity.h>
#include "../../src/test_harness_ringbuf.h"

/* -------------------------------------------------------------------------
 * Global state reset by setUp before every test
 * ---------------------------------------------------------------------- */
static TestHarnessRingBuf rb;

void setUp(void)
{
    test_harness_ringbuf_init(&rb);
}

void tearDown(void) {}

/* -------------------------------------------------------------------------
 * Test 1: Empty buffer — pop returns false, count is 0
 * ---------------------------------------------------------------------- */
void test_empty_pop_returns_false(void)
{
    uint8_t out = 0xAB;
    TEST_ASSERT_FALSE(test_harness_ringbuf_pop(&rb, &out));
}

void test_empty_count_is_zero(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&rb));
}

/* -------------------------------------------------------------------------
 * Test 2: Push single byte — push returns true, count becomes 1
 * ---------------------------------------------------------------------- */
void test_push_single_returns_true(void)
{
    TEST_ASSERT_TRUE(test_harness_ringbuf_push(&rb, 0x42));
}

void test_push_single_count_is_one(void)
{
    test_harness_ringbuf_push(&rb, 0x42);
    TEST_ASSERT_EQUAL_UINT16(1, test_harness_ringbuf_count(&rb));
}

/* -------------------------------------------------------------------------
 * Test 3: Push then pop — popped value matches pushed value
 * ---------------------------------------------------------------------- */
void test_push_then_pop_value_matches(void)
{
    test_harness_ringbuf_push(&rb, 0x5E);
    uint8_t out = 0;
    TEST_ASSERT_TRUE(test_harness_ringbuf_pop(&rb, &out));
    TEST_ASSERT_EQUAL_UINT8(0x5E, out);
}

/* -------------------------------------------------------------------------
 * Test 4: Fill to capacity — push (SIZE-1) = 63 items, all return true
 * ---------------------------------------------------------------------- */
void test_fill_to_capacity_all_push_true(void)
{
    const int max_items = TEST_HARNESS_RINGBUF_SIZE - 1; /* 63 */
    for (int i = 0; i < max_items; i++) {
        TEST_ASSERT_TRUE_MESSAGE(
            test_harness_ringbuf_push(&rb, (uint8_t)(i & 0xFF)),
            "push should succeed while buffer has room");
    }
}

/* -------------------------------------------------------------------------
 * Test 5: Overflow — the 64th push returns false (buffer full)
 * ---------------------------------------------------------------------- */
void test_overflow_push_returns_false_when_full(void)
{
    const int max_items = TEST_HARNESS_RINGBUF_SIZE - 1; /* 63 */
    for (int i = 0; i < max_items; i++) {
        test_harness_ringbuf_push(&rb, (uint8_t)(i & 0xFF));
    }
    /* One more push must be rejected. */
    TEST_ASSERT_FALSE(test_harness_ringbuf_push(&rb, 0xFF));
}

/* -------------------------------------------------------------------------
 * Test 6: Count after fill — count returns SIZE-1 (63)
 * ---------------------------------------------------------------------- */
void test_count_after_fill_is_size_minus_one(void)
{
    const int max_items = TEST_HARNESS_RINGBUF_SIZE - 1;
    for (int i = 0; i < max_items; i++) {
        test_harness_ringbuf_push(&rb, (uint8_t)(i & 0xFF));
    }
    TEST_ASSERT_EQUAL_UINT16((uint16_t)(TEST_HARNESS_RINGBUF_SIZE - 1),
                             test_harness_ringbuf_count(&rb));
}

/* -------------------------------------------------------------------------
 * Test 7: Pop all — drain 63 items (all true), then pop returns false
 * ---------------------------------------------------------------------- */
void test_pop_all_then_empty(void)
{
    const int max_items = TEST_HARNESS_RINGBUF_SIZE - 1;
    for (int i = 0; i < max_items; i++) {
        test_harness_ringbuf_push(&rb, (uint8_t)(i & 0xFF));
    }
    uint8_t out;
    for (int i = 0; i < max_items; i++) {
        TEST_ASSERT_TRUE_MESSAGE(
            test_harness_ringbuf_pop(&rb, &out),
            "pop should succeed while items remain");
    }
    /* Buffer is now empty — next pop must fail. */
    TEST_ASSERT_FALSE(test_harness_ringbuf_pop(&rb, &out));
}

/* -------------------------------------------------------------------------
 * Test 8: Wrap-around — fill, drain, fill again; no index corruption
 * ---------------------------------------------------------------------- */
void test_wraparound_fill_drain_fill(void)
{
    const int max_items = TEST_HARNESS_RINGBUF_SIZE - 1; /* 63 */
    uint8_t out;

    /* First pass: fill completely. */
    for (int i = 0; i < max_items; i++) {
        test_harness_ringbuf_push(&rb, (uint8_t)(i & 0xFF));
    }

    /* Drain completely — head and tail both advance to 63. */
    for (int i = 0; i < max_items; i++) {
        test_harness_ringbuf_pop(&rb, &out);
    }

    /* Second pass: fill again — indices must wrap around cleanly. */
    for (int i = 0; i < max_items; i++) {
        TEST_ASSERT_TRUE_MESSAGE(
            test_harness_ringbuf_push(&rb, (uint8_t)((i + 100) & 0xFF)),
            "push after wrap-around should succeed");
    }

    /* Overflow still rejected after wrap. */
    TEST_ASSERT_FALSE(test_harness_ringbuf_push(&rb, 0xBB));

    /* Drain again and verify count reaches zero. */
    for (int i = 0; i < max_items; i++) {
        test_harness_ringbuf_pop(&rb, &out);
    }
    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&rb));
}

/* -------------------------------------------------------------------------
 * Test 9: FIFO order — push 0..N, pop in same order
 * ---------------------------------------------------------------------- */
void test_fifo_order_preserved(void)
{
    const uint8_t n = 10;
    for (uint8_t i = 0; i < n; i++) {
        test_harness_ringbuf_push(&rb, i);
    }
    for (uint8_t i = 0; i < n; i++) {
        uint8_t out = 0xFF;
        test_harness_ringbuf_pop(&rb, &out);
        TEST_ASSERT_EQUAL_UINT8(i, out);
    }
}

/* -------------------------------------------------------------------------
 * Test 10: Count tracking — count reflects every push and pop
 * ---------------------------------------------------------------------- */
void test_count_tracks_pushes_and_pops(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&rb));

    test_harness_ringbuf_push(&rb, 0xAA);
    TEST_ASSERT_EQUAL_UINT16(1, test_harness_ringbuf_count(&rb));

    test_harness_ringbuf_push(&rb, 0xBB);
    TEST_ASSERT_EQUAL_UINT16(2, test_harness_ringbuf_count(&rb));

    uint8_t out;
    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_UINT16(1, test_harness_ringbuf_count(&rb));

    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&rb));
}

/* -------------------------------------------------------------------------
 * Test 11: Boundary — push/pop at exact capacity edge; count stays correct
 * ---------------------------------------------------------------------- */
void test_boundary_push_pop_at_capacity_edge(void)
{
    const int max_items = TEST_HARNESS_RINGBUF_SIZE - 1; /* 63 */

    /* Fill to max and verify count. */
    for (int i = 0; i < max_items; i++) {
        test_harness_ringbuf_push(&rb, (uint8_t)(i & 0xFF));
    }
    TEST_ASSERT_EQUAL_UINT16((uint16_t)max_items, test_harness_ringbuf_count(&rb));

    /* Pop one — should now accept exactly one more push. */
    uint8_t out;
    test_harness_ringbuf_pop(&rb, &out);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)(max_items - 1), test_harness_ringbuf_count(&rb));

    TEST_ASSERT_TRUE(test_harness_ringbuf_push(&rb, 0xCC));
    TEST_ASSERT_EQUAL_UINT16((uint16_t)max_items, test_harness_ringbuf_count(&rb));

    /* Buffer is full again — next push must fail. */
    TEST_ASSERT_FALSE(test_harness_ringbuf_push(&rb, 0xDD));
}

/* -------------------------------------------------------------------------
 * Test 12: Re-init — after use, init resets buffer to empty state
 * ---------------------------------------------------------------------- */
void test_reinit_resets_to_empty(void)
{
    /* Partially fill the buffer. */
    for (int i = 0; i < 20; i++) {
        test_harness_ringbuf_push(&rb, (uint8_t)(i & 0xFF));
    }
    TEST_ASSERT_EQUAL_UINT16(20, test_harness_ringbuf_count(&rb));

    /* Re-initialize. */
    test_harness_ringbuf_init(&rb);

    /* Must appear empty. */
    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&rb));

    uint8_t out = 0xAB;
    TEST_ASSERT_FALSE(test_harness_ringbuf_pop(&rb, &out));

    /* And push must work again from scratch. */
    TEST_ASSERT_TRUE(test_harness_ringbuf_push(&rb, 0x77));
    TEST_ASSERT_EQUAL_UINT16(1, test_harness_ringbuf_count(&rb));
}

/* -------------------------------------------------------------------------
 * Entry point
 * ---------------------------------------------------------------------- */
int main(int argc, char **argv)
{
    UNITY_BEGIN();

    /* 1. Empty buffer */
    RUN_TEST(test_empty_pop_returns_false);
    RUN_TEST(test_empty_count_is_zero);

    /* 2. Push single */
    RUN_TEST(test_push_single_returns_true);
    RUN_TEST(test_push_single_count_is_one);

    /* 3. Push then pop */
    RUN_TEST(test_push_then_pop_value_matches);

    /* 4. Fill to capacity */
    RUN_TEST(test_fill_to_capacity_all_push_true);

    /* 5. Overflow */
    RUN_TEST(test_overflow_push_returns_false_when_full);

    /* 6. Count after fill */
    RUN_TEST(test_count_after_fill_is_size_minus_one);

    /* 7. Pop all */
    RUN_TEST(test_pop_all_then_empty);

    /* 8. Wrap-around */
    RUN_TEST(test_wraparound_fill_drain_fill);

    /* 9. FIFO order */
    RUN_TEST(test_fifo_order_preserved);

    /* 10. Count tracking */
    RUN_TEST(test_count_tracks_pushes_and_pops);

    /* 11. Boundary */
    RUN_TEST(test_boundary_push_pop_at_capacity_edge);

    /* 12. Re-init */
    RUN_TEST(test_reinit_resets_to_empty);

    return UNITY_END();
}
