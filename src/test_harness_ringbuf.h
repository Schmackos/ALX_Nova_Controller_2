#ifndef TEST_HARNESS_RINGBUF_H
#define TEST_HARNESS_RINGBUF_H

/*
 * test_harness_ringbuf.h
 *
 * ISR-safe single-producer / single-consumer ring buffer for the ALX Nova
 * test harness.  All functions are static inline so the header can be
 * included in native-platform unit tests compiled with test_build_src=no
 * without pulling in any translation unit.
 *
 * Design notes
 * ------------
 *  - head / tail are declared volatile; the compiler may not cache them
 *    in a register across ISR boundaries on bare-metal targets.
 *  - One slot is always kept empty so that full vs. empty can be
 *    distinguished without a separate counter or flag (classic FIFO idiom).
 *    Effective capacity = TEST_HARNESS_RINGBUF_SIZE - 1.
 *  - push() returns false (and leaves the buffer unchanged) when full.
 *  - pop()  returns false (and leaves *value unchanged) when empty.
 *  - On a single-core host (native tests) these invariants are sufficient;
 *    on a multi-core embedded target the caller is responsible for any
 *    additional memory-barrier needs beyond volatile.
 */

#include <stdbool.h>
#include <stdint.h>

#ifndef TEST_HARNESS_RINGBUF_SIZE
#define TEST_HARNESS_RINGBUF_SIZE 16
#endif

/* One slot is reserved; effective capacity is SIZE - 1. */
#define TEST_HARNESS_RINGBUF_CAPACITY (TEST_HARNESS_RINGBUF_SIZE - 1)

typedef struct {
    uint8_t          buf[TEST_HARNESS_RINGBUF_SIZE];
    volatile uint8_t head; /* producer writes here, then advances */
    volatile uint8_t tail; /* consumer reads here, then advances  */
} TestHarnessRingbuf;

/* --------------------------------------------------------------------------
 * test_harness_ringbuf_init
 *
 * Reset head and tail to 0.  Must be called before any other operation.
 * -------------------------------------------------------------------------- */
static inline void test_harness_ringbuf_init(TestHarnessRingbuf *buf)
{
    buf->head = 0;
    buf->tail = 0;
}

/* --------------------------------------------------------------------------
 * test_harness_ringbuf_push
 *
 * Append value to the buffer.
 * Returns true on success, false if the buffer is full (value discarded).
 * -------------------------------------------------------------------------- */
static inline bool test_harness_ringbuf_push(TestHarnessRingbuf *buf,
                                              uint8_t             value)
{
    uint8_t next_head = (uint8_t)((buf->head + 1u) % TEST_HARNESS_RINGBUF_SIZE);
    if (next_head == buf->tail) {
        /* Buffer full — reject to preserve existing data. */
        return false;
    }
    buf->buf[buf->head] = value;
    buf->head           = next_head;
    return true;
}

/* --------------------------------------------------------------------------
 * test_harness_ringbuf_pop
 *
 * Remove the oldest byte from the buffer and store it in *value.
 * Returns true on success, false if the buffer is empty (*value unchanged).
 * -------------------------------------------------------------------------- */
static inline bool test_harness_ringbuf_pop(TestHarnessRingbuf *buf,
                                             uint8_t            *value)
{
    if (buf->head == buf->tail) {
        /* Buffer empty. */
        return false;
    }
    *value     = buf->buf[buf->tail];
    buf->tail  = (uint8_t)((buf->tail + 1u) % TEST_HARNESS_RINGBUF_SIZE);
    return true;
}

/* --------------------------------------------------------------------------
 * test_harness_ringbuf_count
 *
 * Return the number of bytes currently stored in the buffer.
 * -------------------------------------------------------------------------- */
static inline uint8_t test_harness_ringbuf_count(const TestHarnessRingbuf *buf)
{
    /* Cast away volatile for the arithmetic; values are read once each. */
    uint8_t h = buf->head;
    uint8_t t = buf->tail;
    if (h >= t) {
        return (uint8_t)(h - t);
    }
    return (uint8_t)(TEST_HARNESS_RINGBUF_SIZE - t + h);
}

#endif /* TEST_HARNESS_RINGBUF_H */
