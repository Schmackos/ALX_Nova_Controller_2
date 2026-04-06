/*
 * test_harness_ringbuf.h
 *
 * Test harness artifact for ALX Nova pipeline validation.
 * This is a textbook single-producer / single-consumer (SPSC) ring buffer
 * intended for use in test harness scaffolding only — not production firmware.
 *
 * ISR safety is achieved by declaring the head and tail indices volatile,
 * preventing the compiler from caching them across read/write boundaries.
 * The full condition wastes one slot so that head == tail always means empty
 * and (head + 1) % SIZE == tail always means full, with no ambiguity.
 *
 * All functions are static inline so that native unit tests (test_build_src = no)
 * can include this header without needing to link a .cpp translation unit.
 */

#ifndef TEST_HARNESS_RINGBUF_H
#define TEST_HARNESS_RINGBUF_H

#include <stdint.h>
#include <stdbool.h>

/* Buffer capacity. Must be a power of 2 for mask-based wrap-around.
 * The buffer holds at most (TEST_HARNESS_RINGBUF_SIZE - 1) elements;
 * one slot is sacrificed to distinguish full from empty. */
#define TEST_HARNESS_RINGBUF_SIZE 64

/* Compile-time assertion: SIZE must be a power of 2. */
typedef char _test_harness_ringbuf_size_must_be_power_of_2[
    ((TEST_HARNESS_RINGBUF_SIZE & (TEST_HARNESS_RINGBUF_SIZE - 1)) == 0) ? 1 : -1
];

typedef struct {
    volatile uint16_t head;                  /* write index — advanced by producer */
    volatile uint16_t tail;                  /* read index  — advanced by consumer */
    uint8_t buffer[TEST_HARNESS_RINGBUF_SIZE]; /* data storage */
} TestHarnessRingBuf;

/* Initialize the ring buffer to an empty state. */
static inline void test_harness_ringbuf_init(TestHarnessRingBuf *rb)
{
    rb->head = 0;
    rb->tail = 0;
}

/*
 * Push one byte into the ring buffer.
 * Returns true on success, false if the buffer is full.
 * Safe to call from an ISR (producer side of an SPSC pair).
 */
static inline bool test_harness_ringbuf_push(TestHarnessRingBuf *rb, uint8_t value)
{
    uint16_t next_head = (uint16_t)((rb->head + 1u) & (TEST_HARNESS_RINGBUF_SIZE - 1u));

    /* Full: advancing head would collide with tail. */
    if (next_head == rb->tail) {
        return false;
    }

    rb->buffer[rb->head] = value;
    rb->head = next_head;
    return true;
}

/*
 * Pop one byte from the ring buffer into *out.
 * Returns true on success, false if the buffer is empty.
 * Safe to call from an ISR (consumer side of an SPSC pair).
 */
static inline bool test_harness_ringbuf_pop(TestHarnessRingBuf *rb, uint8_t *out)
{
    /* Empty: head and tail coincide. */
    if (rb->head == rb->tail) {
        return false;
    }

    *out = rb->buffer[rb->tail];
    rb->tail = (uint16_t)((rb->tail + 1u) & (TEST_HARNESS_RINGBUF_SIZE - 1u));
    return true;
}

/*
 * Return the number of bytes currently stored in the ring buffer.
 * The result is a snapshot — it may change immediately if called
 * concurrently with push/pop from another context.
 */
static inline uint16_t test_harness_ringbuf_count(const TestHarnessRingBuf *rb)
{
    return (uint16_t)((rb->head - rb->tail) & (TEST_HARNESS_RINGBUF_SIZE - 1u));
}

#endif /* TEST_HARNESS_RINGBUF_H */
