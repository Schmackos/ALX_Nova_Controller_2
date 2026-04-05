/*
 * test_harness_ringbuf.cpp — Unity tests for the ISR-safe ring buffer.
 *
 * Module under test: src/test_harness_ringbuf.h
 *
 * Coverage:
 *   test_harness_ringbuf_init   — initialisation, capacity stored, indices zeroed
 *   test_harness_ringbuf_push   — success, full detection, return values
 *   test_harness_ringbuf_pop    — success, empty detection, FIFO order, *out unchanged on empty
 *   test_harness_ringbuf_count  — accuracy across init / push / pop / wrap states
 *
 * Test categories: unit
 */

#include <unity.h>
#include <string.h>
#include <stdint.h>

#include "../../src/test_harness_ringbuf.h"

/* ---- shared fixtures --------------------------------------------------- */

/* Buffer sized for the largest capacity used in these tests (255 + 1 slots). */
static uint8_t g_buf[256];
static TestHarnessRingBuf g_rb;

void setUp(void)
{
    memset(g_buf, 0, sizeof(g_buf));
    memset(&g_rb, 0, sizeof(g_rb));
}

void tearDown(void) {}

/* ======================================================================= */
/* 1. Empty buffer after init                                               */
/* ======================================================================= */

void test_init_count_is_zero(void)
{
    test_harness_ringbuf_init(&g_rb, g_buf, 4);
    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&g_rb));
}

void test_init_pop_returns_zero(void)
{
    test_harness_ringbuf_init(&g_rb, g_buf, 4);
    uint8_t out = 0xAB;
    TEST_ASSERT_EQUAL_INT(0, test_harness_ringbuf_pop(&g_rb, &out));
    /* *out must remain unchanged */
    TEST_ASSERT_EQUAL_UINT8(0xAB, out);
}

/* ======================================================================= */
/* 2. Push one byte — count becomes 1                                       */
/* ======================================================================= */

void test_push_one_count_is_one(void)
{
    test_harness_ringbuf_init(&g_rb, g_buf, 4);
    TEST_ASSERT_EQUAL_INT(1, test_harness_ringbuf_push(&g_rb, 0x42));
    TEST_ASSERT_EQUAL_UINT16(1, test_harness_ringbuf_count(&g_rb));
}

/* ======================================================================= */
/* 3. Push then pop — data correct, count returns to 0                      */
/* ======================================================================= */

void test_push_pop_data_correct(void)
{
    test_harness_ringbuf_init(&g_rb, g_buf, 4);
    test_harness_ringbuf_push(&g_rb, 0x55);

    uint8_t out = 0x00;
    int ret = test_harness_ringbuf_pop(&g_rb, &out);
    TEST_ASSERT_EQUAL_INT(1, ret);
    TEST_ASSERT_EQUAL_UINT8(0x55, out);
    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&g_rb));
}

/* ======================================================================= */
/* 4. Fill to capacity — count == capacity, next push returns 0             */
/* ======================================================================= */

void test_fill_to_capacity_count_equals_capacity(void)
{
    const uint16_t cap = 4;
    test_harness_ringbuf_init(&g_rb, g_buf, cap);

    for (uint16_t i = 0; i < cap; i++) {
        TEST_ASSERT_EQUAL_INT(1, test_harness_ringbuf_push(&g_rb, (uint8_t)i));
    }
    TEST_ASSERT_EQUAL_UINT16(cap, test_harness_ringbuf_count(&g_rb));
}

void test_push_on_full_returns_zero(void)
{
    const uint16_t cap = 4;
    test_harness_ringbuf_init(&g_rb, g_buf, cap);

    for (uint16_t i = 0; i < cap; i++) {
        test_harness_ringbuf_push(&g_rb, (uint8_t)i);
    }
    /* One more push must fail */
    TEST_ASSERT_EQUAL_INT(0, test_harness_ringbuf_push(&g_rb, 0xFF));
    /* Count must not have changed */
    TEST_ASSERT_EQUAL_UINT16(cap, test_harness_ringbuf_count(&g_rb));
}

/* ======================================================================= */
/* 5. Fill then drain completely — FIFO order, all data correct             */
/* ======================================================================= */

void test_fill_drain_fifo_order(void)
{
    const uint16_t cap = 8;
    test_harness_ringbuf_init(&g_rb, g_buf, cap);

    for (uint16_t i = 0; i < cap; i++) {
        test_harness_ringbuf_push(&g_rb, (uint8_t)(i + 10));
    }

    for (uint16_t i = 0; i < cap; i++) {
        uint8_t out = 0;
        int ret = test_harness_ringbuf_pop(&g_rb, &out);
        TEST_ASSERT_EQUAL_INT(1, ret);
        TEST_ASSERT_EQUAL_UINT8((uint8_t)(i + 10), out);
    }

    /* Buffer must now be empty */
    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&g_rb));
}

/* ======================================================================= */
/* 6. Wrap-around: fill, drain half, fill again past the internal boundary  */
/* ======================================================================= */

void test_wrap_around_data_integrity(void)
{
    const uint16_t cap = 8;
    test_harness_ringbuf_init(&g_rb, g_buf, cap);

    /* Phase 1: fill completely with values 0..7 */
    for (uint8_t i = 0; i < cap; i++) {
        test_harness_ringbuf_push(&g_rb, i);
    }

    /* Phase 2: drain half (4 bytes) — head advances to slot 4 */
    for (uint16_t i = 0; i < cap / 2; i++) {
        uint8_t out = 0;
        test_harness_ringbuf_pop(&g_rb, &out);
    }

    /* Phase 3: push 4 more bytes (values 8..11) — tail wraps past slot 8 */
    for (uint8_t i = 0; i < cap / 2; i++) {
        TEST_ASSERT_EQUAL_INT(1, test_harness_ringbuf_push(&g_rb, (uint8_t)(8 + i)));
    }

    /* Phase 4: count must still be cap (4 old + 4 new = 8) */
    TEST_ASSERT_EQUAL_UINT16(cap, test_harness_ringbuf_count(&g_rb));

    /* Phase 5: drain all — expect 4,5,6,7,8,9,10,11 */
    uint8_t expected[] = {4, 5, 6, 7, 8, 9, 10, 11};
    for (uint16_t i = 0; i < cap; i++) {
        uint8_t out = 0xFF;
        int ret = test_harness_ringbuf_pop(&g_rb, &out);
        TEST_ASSERT_EQUAL_INT(1, ret);
        TEST_ASSERT_EQUAL_UINT8(expected[i], out);
    }

    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&g_rb));
}

/* ======================================================================= */
/* 7. Boundary: capacity of 1 (only one byte fits)                         */
/* ======================================================================= */

void test_capacity_one_holds_one_byte(void)
{
    uint8_t small_buf[2]; /* capacity+1 = 2 */
    TestHarnessRingBuf rb;
    test_harness_ringbuf_init(&rb, small_buf, 1);

    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&rb));
    TEST_ASSERT_EQUAL_INT(1, test_harness_ringbuf_push(&rb, 0xAA));
    TEST_ASSERT_EQUAL_UINT16(1, test_harness_ringbuf_count(&rb));

    /* Second push must fail — full */
    TEST_ASSERT_EQUAL_INT(0, test_harness_ringbuf_push(&rb, 0xBB));
    TEST_ASSERT_EQUAL_UINT16(1, test_harness_ringbuf_count(&rb));

    uint8_t out = 0;
    TEST_ASSERT_EQUAL_INT(1, test_harness_ringbuf_pop(&rb, &out));
    TEST_ASSERT_EQUAL_UINT8(0xAA, out);
    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&rb));

    /* Pop on now-empty must fail */
    out = 0xCC;
    TEST_ASSERT_EQUAL_INT(0, test_harness_ringbuf_pop(&rb, &out));
    TEST_ASSERT_EQUAL_UINT8(0xCC, out);
}

/* ======================================================================= */
/* 8. Boundary: capacity of 255 (larger buffer)                            */
/* ======================================================================= */

void test_capacity_255_fill_and_drain(void)
{
    /* g_buf is 256 bytes — exactly capacity+1 for cap=255 */
    const uint16_t cap = 255;
    test_harness_ringbuf_init(&g_rb, g_buf, cap);

    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&g_rb));

    /* Fill completely */
    for (uint16_t i = 0; i < cap; i++) {
        TEST_ASSERT_EQUAL_INT(1, test_harness_ringbuf_push(&g_rb, (uint8_t)(i & 0xFF)));
    }
    TEST_ASSERT_EQUAL_UINT16(cap, test_harness_ringbuf_count(&g_rb));

    /* One more push must fail */
    TEST_ASSERT_EQUAL_INT(0, test_harness_ringbuf_push(&g_rb, 0xFF));

    /* Drain and verify FIFO order */
    for (uint16_t i = 0; i < cap; i++) {
        uint8_t out = 0;
        TEST_ASSERT_EQUAL_INT(1, test_harness_ringbuf_pop(&g_rb, &out));
        TEST_ASSERT_EQUAL_UINT8((uint8_t)(i & 0xFF), out);
    }
    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&g_rb));
}

/* ======================================================================= */
/* 9. Consecutive push/pop interleaving                                     */
/* ======================================================================= */

void test_interleaved_push_pop(void)
{
    const uint16_t cap = 4;
    test_harness_ringbuf_init(&g_rb, g_buf, cap);

    /* push A, push B, pop A, push C, pop B, pop C */
    test_harness_ringbuf_push(&g_rb, 0xA0);
    test_harness_ringbuf_push(&g_rb, 0xB0);

    uint8_t out = 0;
    test_harness_ringbuf_pop(&g_rb, &out);
    TEST_ASSERT_EQUAL_UINT8(0xA0, out);
    TEST_ASSERT_EQUAL_UINT16(1, test_harness_ringbuf_count(&g_rb));

    test_harness_ringbuf_push(&g_rb, 0xC0);
    TEST_ASSERT_EQUAL_UINT16(2, test_harness_ringbuf_count(&g_rb));

    test_harness_ringbuf_pop(&g_rb, &out);
    TEST_ASSERT_EQUAL_UINT8(0xB0, out);

    test_harness_ringbuf_pop(&g_rb, &out);
    TEST_ASSERT_EQUAL_UINT8(0xC0, out);

    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&g_rb));
}

/* ======================================================================= */
/* 10. Count accuracy throughout various states                             */
/* ======================================================================= */

void test_count_accuracy_through_states(void)
{
    const uint16_t cap = 6;
    test_harness_ringbuf_init(&g_rb, g_buf, cap);

    for (uint16_t i = 1; i <= cap; i++) {
        test_harness_ringbuf_push(&g_rb, (uint8_t)i);
        TEST_ASSERT_EQUAL_UINT16(i, test_harness_ringbuf_count(&g_rb));
    }

    uint8_t out = 0;
    for (uint16_t removed = 1; removed <= cap; removed++) {
        test_harness_ringbuf_pop(&g_rb, &out);
        TEST_ASSERT_EQUAL_UINT16((uint16_t)(cap - removed),
                                 test_harness_ringbuf_count(&g_rb));
    }
}

/* ======================================================================= */
/* 11. Pop on empty returns 0, does not modify *out                         */
/* ======================================================================= */

void test_pop_on_empty_does_not_modify_out(void)
{
    test_harness_ringbuf_init(&g_rb, g_buf, 4);
    uint8_t sentinel = 0xDE;
    TEST_ASSERT_EQUAL_INT(0, test_harness_ringbuf_pop(&g_rb, &sentinel));
    TEST_ASSERT_EQUAL_UINT8(0xDE, sentinel);
}

/* ======================================================================= */
/* 12. Push on full returns 0                                               */
/* ======================================================================= */

void test_push_on_full_returns_zero_and_preserves_data(void)
{
    const uint16_t cap = 3;
    test_harness_ringbuf_init(&g_rb, g_buf, cap);

    test_harness_ringbuf_push(&g_rb, 0x11);
    test_harness_ringbuf_push(&g_rb, 0x22);
    test_harness_ringbuf_push(&g_rb, 0x33);

    /* Buffer is now full */
    TEST_ASSERT_EQUAL_INT(0, test_harness_ringbuf_push(&g_rb, 0x44));

    /* Existing data must be intact */
    uint8_t out = 0;
    test_harness_ringbuf_pop(&g_rb, &out);
    TEST_ASSERT_EQUAL_UINT8(0x11, out);
    test_harness_ringbuf_pop(&g_rb, &out);
    TEST_ASSERT_EQUAL_UINT8(0x22, out);
    test_harness_ringbuf_pop(&g_rb, &out);
    TEST_ASSERT_EQUAL_UINT8(0x33, out);
    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&g_rb));
}

/* ======================================================================= */
/* 13. Capacity 0 — buffer is always full                                   */
/* ======================================================================= */

void test_capacity_zero_always_full(void)
{
    uint8_t one_byte_buf[1]; /* capacity+1 = 1 slot */
    TestHarnessRingBuf rb;
    test_harness_ringbuf_init(&rb, one_byte_buf, 0);

    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&rb));
    TEST_ASSERT_EQUAL_INT(0, test_harness_ringbuf_push(&rb, 0xFF));
    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&rb));
}

/* ======================================================================= */
/* 14. Wrap-around: multiple full cycles to stress modulo arithmetic        */
/* ======================================================================= */

void test_multiple_full_cycles(void)
{
    const uint16_t cap = 4;
    test_harness_ringbuf_init(&g_rb, g_buf, cap);

    /* Run 3 full fill/drain cycles */
    for (int cycle = 0; cycle < 3; cycle++) {
        for (uint8_t i = 0; i < cap; i++) {
            TEST_ASSERT_EQUAL_INT(1,
                test_harness_ringbuf_push(&g_rb, (uint8_t)(cycle * 10 + i)));
        }
        for (uint8_t i = 0; i < cap; i++) {
            uint8_t out = 0xFF;
            TEST_ASSERT_EQUAL_INT(1, test_harness_ringbuf_pop(&g_rb, &out));
            TEST_ASSERT_EQUAL_UINT8((uint8_t)(cycle * 10 + i), out);
        }
        TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&g_rb));
    }
}

/* ======================================================================= */
/* 15. Single-byte push/pop cycle at head == tail boundary after wrap       */
/* ======================================================================= */

void test_single_byte_after_full_wrap(void)
{
    const uint16_t cap = 2;
    uint8_t buf[3];
    TestHarnessRingBuf rb;
    test_harness_ringbuf_init(&rb, buf, cap);

    /* Fill then drain to advance head and tail past slot 0 */
    test_harness_ringbuf_push(&rb, 0x01);
    test_harness_ringbuf_push(&rb, 0x02);
    uint8_t out = 0;
    test_harness_ringbuf_pop(&rb, &out);
    test_harness_ringbuf_pop(&rb, &out);

    /* Push/pop a fresh byte — head and tail both at slot 2 */
    TEST_ASSERT_EQUAL_INT(1, test_harness_ringbuf_push(&rb, 0xAB));
    TEST_ASSERT_EQUAL_UINT16(1, test_harness_ringbuf_count(&rb));
    out = 0;
    TEST_ASSERT_EQUAL_INT(1, test_harness_ringbuf_pop(&rb, &out));
    TEST_ASSERT_EQUAL_UINT8(0xAB, out);
    TEST_ASSERT_EQUAL_UINT16(0, test_harness_ringbuf_count(&rb));
}

/* ======================================================================= */
/* main                                                                     */
/* ======================================================================= */

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    /* 1. Empty buffer after init */
    RUN_TEST(test_init_count_is_zero);
    RUN_TEST(test_init_pop_returns_zero);

    /* 2. Push one byte */
    RUN_TEST(test_push_one_count_is_one);

    /* 3. Push then pop */
    RUN_TEST(test_push_pop_data_correct);

    /* 4. Fill to capacity */
    RUN_TEST(test_fill_to_capacity_count_equals_capacity);
    RUN_TEST(test_push_on_full_returns_zero);

    /* 5. Fill then drain */
    RUN_TEST(test_fill_drain_fifo_order);

    /* 6. Wrap-around */
    RUN_TEST(test_wrap_around_data_integrity);

    /* 7. Boundary: capacity 1 */
    RUN_TEST(test_capacity_one_holds_one_byte);

    /* 8. Boundary: capacity 255 */
    RUN_TEST(test_capacity_255_fill_and_drain);

    /* 9. Interleaving */
    RUN_TEST(test_interleaved_push_pop);

    /* 10. Count accuracy */
    RUN_TEST(test_count_accuracy_through_states);

    /* 11. Pop on empty */
    RUN_TEST(test_pop_on_empty_does_not_modify_out);

    /* 12. Push on full */
    RUN_TEST(test_push_on_full_returns_zero_and_preserves_data);

    /* 13. Capacity 0 */
    RUN_TEST(test_capacity_zero_always_full);

    /* 14. Multiple cycles */
    RUN_TEST(test_multiple_full_cycles);

    /* 15. Single byte after full wrap */
    RUN_TEST(test_single_byte_after_full_wrap);

    return UNITY_END();
}
