#ifndef TEST_HARNESS_RINGBUF_H
#define TEST_HARNESS_RINGBUF_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * test_harness_ringbuf.h
 *
 * ISR-safe fixed-size ring buffer for uint8_t values.
 *
 * Design notes:
 * - head and tail are declared volatile to model ISR-safe access patterns.
 *   A real ISR-safe implementation would also require atomic load/store or
 *   critical-section guards around multi-statement sequences; the volatile
 *   qualifier prevents the compiler from caching the indices in registers,
 *   matching the minimal requirement for a single-producer/single-consumer
 *   lock-free ring buffer where the producer runs in an ISR and the consumer
 *   runs in a task (or vice versa).
 * - Buffer capacity is (TEST_HARNESS_RINGBUF_SIZE - 1) items because one
 *   slot is reserved to distinguish full from empty without a separate count
 *   field: empty => head == tail, full => (head + 1) % size == tail.
 * - All functions are static inline so they can be included from any
 *   translation unit without generating duplicate symbol errors.  This is
 *   required because the project's native test environment sets
 *   test_build_src = no, meaning src/ is not compiled as a library.
 */

#ifndef TEST_HARNESS_RINGBUF_SIZE
#define TEST_HARNESS_RINGBUF_SIZE 16
#endif

typedef struct {
    uint8_t          buf[TEST_HARNESS_RINGBUF_SIZE];
    volatile uint8_t head; /* write index (producer advances) */
    volatile uint8_t tail; /* read  index (consumer advances) */
} TestHarnessRingbuf;

/* Initialize (or reset) a ring buffer to the empty state. */
static inline void test_harness_ringbuf_init(TestHarnessRingbuf *rb) {
    rb->head = 0;
    rb->tail = 0;
}

/*
 * Push a value onto the ring buffer.
 * Returns true on success, false if the buffer is full.
 */
static inline bool test_harness_ringbuf_push(TestHarnessRingbuf *rb, uint8_t value) {
    uint8_t next_head = (uint8_t)((rb->head + 1u) % TEST_HARNESS_RINGBUF_SIZE);
    if (next_head == rb->tail) {
        /* Buffer full — one slot is always kept empty to distinguish
         * full from empty without a separate flag. */
        return false;
    }
    rb->buf[rb->head] = value;
    rb->head = next_head;
    return true;
}

/*
 * Pop a value from the ring buffer into *value.
 * Returns true on success, false if the buffer is empty.
 */
static inline bool test_harness_ringbuf_pop(TestHarnessRingbuf *rb, uint8_t *value) {
    if (rb->tail == rb->head) {
        /* Buffer empty */
        return false;
    }
    *value = rb->buf[rb->tail];
    rb->tail = (uint8_t)((rb->tail + 1u) % TEST_HARNESS_RINGBUF_SIZE);
    return true;
}

/*
 * Return the number of items currently stored in the ring buffer.
 */
static inline uint8_t test_harness_ringbuf_count(TestHarnessRingbuf *rb) {
    return (uint8_t)((rb->head - rb->tail + TEST_HARNESS_RINGBUF_SIZE) % TEST_HARNESS_RINGBUF_SIZE);
}

#endif /* TEST_HARNESS_RINGBUF_H */
