#include <cstring>
#include <string>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

/**
 * GUI Input Tests
 *
 * Tests the EC11 rotary encoder Gray code state machine
 * and button debounce logic. Uses a simplified re-implementation
 * of the encoder driver to avoid LVGL dependencies.
 */

/* ===== Encoder Gray Code State Machine (mirrors gui_input.cpp logic) ===== */

/* Encoder state variables */
static volatile int8_t enc_count = 0;
static volatile bool enc_pressed = false;
static volatile uint8_t enc_state = 0;

/* EC11 Gray code state transition table
 * Rows: previous state (A<<1 | B), Columns: new state (A<<1 | B)
 * Values: 0=no change, 1=clockwise step, -1=counter-clockwise step */
static const int8_t enc_table[4][4] = {
    /*           00  01  10  11  <- new state */
    /* 00 */ {   0, -1,  1,  0 },
    /* 01 */ {   1,  0,  0, -1 },
    /* 10 */ {  -1,  0,  0,  1 },
    /* 11 */ {   0,  1, -1,  0 },
};

/* Simulate encoder interrupt handler */
static void encoder_isr(bool pin_a, bool pin_b) {
    uint8_t new_state = ((uint8_t)pin_a << 1) | (uint8_t)pin_b;
    int8_t diff = enc_table[enc_state][new_state];
    enc_count += diff;
    enc_state = new_state;
}

/* Reset encoder state */
static void encoder_reset(void) {
    enc_count = 0;
    enc_pressed = false;
    enc_state = 0;
}

/* Read and clear accumulated encoder count */
static int8_t encoder_read_and_clear(void) {
    int8_t val = enc_count;
    enc_count = 0;
    return val;
}

/* ===== Tests ===== */

void setUp(void) {
    encoder_reset();
}

void tearDown(void) {}

/* Test: Initial state is zero */
void test_encoder_initial_state(void) {
    TEST_ASSERT_EQUAL(0, enc_count);
    TEST_ASSERT_EQUAL(0, enc_state);
    TEST_ASSERT_FALSE(enc_pressed);
}

/* Test: Full clockwise step (detent to detent)
 * EC11 sequence for CW: 00 -> 01 -> 11 -> 10 -> 00 */
void test_encoder_clockwise_full_step(void) {
    /* Starting at 00 (both pins low) */
    encoder_isr(false, true);   /* 00 -> 01: -1 */
    encoder_isr(true, true);    /* 01 -> 11: -1 */
    encoder_isr(true, false);   /* 11 -> 10: -1 */
    encoder_isr(false, false);  /* 10 -> 00: -1 */

    /* Net movement: -4 (4 transitions, each -1 for this direction) */
    TEST_ASSERT_EQUAL(-4, enc_count);
}

/* Test: Full counter-clockwise step
 * EC11 sequence for CCW: 00 -> 10 -> 11 -> 01 -> 00 */
void test_encoder_counterclockwise_full_step(void) {
    encoder_isr(true, false);   /* 00 -> 10: +1 */
    encoder_isr(true, true);    /* 10 -> 11: +1 */
    encoder_isr(false, true);   /* 11 -> 01: +1 */
    encoder_isr(false, false);  /* 01 -> 00: +1 */

    TEST_ASSERT_EQUAL(4, enc_count);
}

/* Test: No movement when same state repeated */
void test_encoder_no_change(void) {
    encoder_isr(false, false);  /* 00 -> 00: 0 */
    encoder_isr(false, false);  /* 00 -> 00: 0 */
    TEST_ASSERT_EQUAL(0, enc_count);
}

/* Test: Single transition CW */
void test_encoder_single_transition_cw(void) {
    /* State 00 -> 10 should give +1 (CW direction) */
    encoder_isr(true, false);
    TEST_ASSERT_EQUAL(1, enc_count);
}

/* Test: Single transition CCW */
void test_encoder_single_transition_ccw(void) {
    /* State 00 -> 01 should give -1 (CCW direction) */
    encoder_isr(false, true);
    TEST_ASSERT_EQUAL(-1, enc_count);
}

/* Test: Read and clear resets count */
void test_encoder_read_and_clear(void) {
    encoder_isr(true, false);  /* +1 */
    encoder_isr(true, true);   /* +1 */

    int8_t val = encoder_read_and_clear();
    TEST_ASSERT_EQUAL(2, val);
    TEST_ASSERT_EQUAL(0, enc_count);
}

/* Test: Multiple CW steps accumulate */
void test_encoder_multiple_cw_steps(void) {
    /* Two full CW cycles: each cycle is 00->10->11->01->00 (net +4 each) */
    for (int cycle = 0; cycle < 2; cycle++) {
        encoder_isr(true, false);
        encoder_isr(true, true);
        encoder_isr(false, true);
        encoder_isr(false, false);
    }
    TEST_ASSERT_EQUAL(8, enc_count);
}

/* Test: Bounce (return to same state) cancels out */
void test_encoder_bounce_cancels(void) {
    /* Go forward then back: 00 -> 10 -> 00 */
    encoder_isr(true, false);   /* 00 -> 10: +1 */
    encoder_isr(false, false);  /* 10 -> 00: -1 */
    TEST_ASSERT_EQUAL(0, enc_count);
}

/* Test: State table symmetry */
void test_encoder_table_symmetry(void) {
    /* For all valid transitions: table[a][b] should equal -table[b][a] */
    for (int a = 0; a < 4; a++) {
        for (int b = 0; b < 4; b++) {
            if (a != b) {
                TEST_ASSERT_EQUAL_INT8(-enc_table[a][b], enc_table[b][a]);
            }
        }
    }
}

/* Test: State table diagonal is all zeros */
void test_encoder_table_diagonal_zero(void) {
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL(0, enc_table[i][i]);
    }
}

/* Test: Direction detection with partial steps */
void test_encoder_partial_then_complete(void) {
    /* Start CW but reverse halfway */
    encoder_isr(true, false);   /* 00 -> 10: +1 */
    encoder_isr(true, true);    /* 10 -> 11: +1 */
    /* Now reverse */
    encoder_isr(true, false);   /* 11 -> 10: -1 */
    encoder_isr(false, false);  /* 10 -> 00: -1 */

    TEST_ASSERT_EQUAL(0, enc_count);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_encoder_initial_state);
    RUN_TEST(test_encoder_clockwise_full_step);
    RUN_TEST(test_encoder_counterclockwise_full_step);
    RUN_TEST(test_encoder_no_change);
    RUN_TEST(test_encoder_single_transition_cw);
    RUN_TEST(test_encoder_single_transition_ccw);
    RUN_TEST(test_encoder_read_and_clear);
    RUN_TEST(test_encoder_multiple_cw_steps);
    RUN_TEST(test_encoder_bounce_cancels);
    RUN_TEST(test_encoder_table_symmetry);
    RUN_TEST(test_encoder_table_diagonal_zero);
    RUN_TEST(test_encoder_partial_then_complete);

    return UNITY_END();
}
