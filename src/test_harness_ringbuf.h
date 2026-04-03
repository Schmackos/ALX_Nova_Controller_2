/*
 * test_harness_ringbuf.h
 *
 * TEST/VALIDATION ARTIFACT — not production code.
 * All identifiers carry the test_harness_ prefix for safe grep-and-delete cleanup.
 *
 * ISR-safe single-producer / single-consumer ring buffer for int32_t values.
 *
 * Design notes
 * ------------
 * - Capacity is fixed at compile time via TEST_HARNESS_RINGBUF_CAPACITY.
 *   It MUST be a power of two (enforced by a compile-time assertion).
 * - head and tail are volatile so the compiler cannot cache them across an
 *   ISR boundary (the classic SPSC ISR-safe guarantee).
 * - Overflow policy: push when full returns TEST_HARNESS_RINGBUF_ERR_FULL
 *   and discards the incoming value.  The caller decides whether to drop the
 *   sample or handle the error.  This is the safer choice for an ISR context
 *   because overwriting old data silently can hide timing problems in tests.
 * - Empty pop returns TEST_HARNESS_RINGBUF_ERR_EMPTY and leaves *out_value
 *   unchanged.
 * - All functions are pure inline so they compile cleanly under the native
 *   test environment where test_build_src = no prevents src/ from being
 *   compiled as a separate translation unit.
 *
 * Usage
 * -----
 *   #define TEST_HARNESS_RINGBUF_CAPACITY 64   // optional, default 64
 *   #include "test_harness_ringbuf.h"
 *
 *   TestHarnessRingBuf rb;
 *   test_harness_ringbuf_init(&rb);
 *   test_harness_ringbuf_push(&rb, 42);
 *   int32_t v;
 *   if (test_harness_ringbuf_pop(&rb, &v) == 0) { ... }
 */

#ifndef TEST_HARNESS_RINGBUF_H
#define TEST_HARNESS_RINGBUF_H

#include <stdint.h>
#include <stddef.h>

/* -------------------------------------------------------------------------
 * Capacity configuration
 * ---------------------------------------------------------------------- */

#ifndef TEST_HARNESS_RINGBUF_CAPACITY
#  define TEST_HARNESS_RINGBUF_CAPACITY 64
#endif

/* Compile-time power-of-two check.  A power of two N satisfies (N & (N-1)) == 0. */
typedef char test_harness_ringbuf_capacity_must_be_power_of_two[
    ((TEST_HARNESS_RINGBUF_CAPACITY) > 0 &&
     (((TEST_HARNESS_RINGBUF_CAPACITY) & ((TEST_HARNESS_RINGBUF_CAPACITY) - 1)) == 0))
    ? 1 : -1];

/* Mask used for index wrapping: index & mask == index % capacity. */
#define TEST_HARNESS_RINGBUF_MASK  ((TEST_HARNESS_RINGBUF_CAPACITY) - 1)

/* -------------------------------------------------------------------------
 * Return codes
 * ---------------------------------------------------------------------- */

/** Success. */
#define TEST_HARNESS_RINGBUF_OK         (0)

/** push() was called on a full buffer; value was discarded. */
#define TEST_HARNESS_RINGBUF_ERR_FULL   (-1)

/** pop() was called on an empty buffer; *out_value is unchanged. */
#define TEST_HARNESS_RINGBUF_ERR_EMPTY  (-2)

/* -------------------------------------------------------------------------
 * Data structure
 * ---------------------------------------------------------------------- */

/**
 * TestHarnessRingBuf — fixed-size ISR-safe ring buffer for int32_t values.
 *
 * head is owned by the consumer (pop side).
 * tail is owned by the producer (push side / ISR).
 * Both are volatile so the compiler re-reads them on every access, which is
 * required for correct SPSC behaviour when one side runs in an ISR.
 */
typedef struct {
    int32_t          data[TEST_HARNESS_RINGBUF_CAPACITY];
    volatile uint32_t head; /* read index  — advanced by pop  */
    volatile uint32_t tail; /* write index — advanced by push */
} TestHarnessRingBuf;

/* -------------------------------------------------------------------------
 * API — all functions are static inline
 * ---------------------------------------------------------------------- */

/**
 * test_harness_ringbuf_init — zero-initialise the ring buffer.
 *
 * Must be called before any push/pop.  Not ISR-safe during initialisation;
 * call from a safe context before enabling the ISR.
 *
 * @param rb  Pointer to the ring buffer instance.
 */
static inline void test_harness_ringbuf_init(TestHarnessRingBuf *rb)
{
    uint32_t i;
    for (i = 0; i < (uint32_t)TEST_HARNESS_RINGBUF_CAPACITY; i++) {
        rb->data[i] = 0;
    }
    rb->head = 0;
    rb->tail = 0;
}

/**
 * test_harness_ringbuf_count — return the number of items currently stored.
 *
 * Safe to call from either producer or consumer context.  The result is a
 * snapshot; it may change immediately after return if the other side runs.
 *
 * Implementation note: this buffer uses a "leave one slot empty" full
 * detection strategy, so the usable capacity is (CAPACITY - 1) items.
 * count returns a value in the range [0, CAPACITY - 1].
 *
 * @param rb  Pointer to the ring buffer instance.
 * @return    Number of items available for pop (0 .. CAPACITY-1).
 */
static inline uint32_t test_harness_ringbuf_count(const TestHarnessRingBuf *rb)
{
    /*
     * Read tail before head so we get a conservative (never over-) count from
     * the consumer's perspective.  Both reads are through volatile pointers so
     * the compiler cannot reorder or cache them.
     *
     * Because indices are unsigned and capacity is a power of two, unsigned
     * subtraction wraps correctly: (tail - head) & MASK gives the number of
     * occupied slots regardless of wrap-around.
     */
    uint32_t tail = rb->tail;
    uint32_t head = rb->head;
    return (tail - head) & (uint32_t)TEST_HARNESS_RINGBUF_MASK;
}

/**
 * test_harness_ringbuf_push — insert one value at the tail.
 *
 * Overflow policy: returns TEST_HARNESS_RINGBUF_ERR_FULL and discards the
 * value when the buffer is full.  The oldest data is NOT overwritten.
 *
 * Safe to call from an ISR (producer side of SPSC).
 *
 * @param rb     Pointer to the ring buffer instance.
 * @param value  Value to enqueue.
 * @return       TEST_HARNESS_RINGBUF_OK or TEST_HARNESS_RINGBUF_ERR_FULL.
 */
static inline int test_harness_ringbuf_push(TestHarnessRingBuf *rb, int32_t value)
{
    uint32_t next_tail;

    /* Snapshot tail; this is our index to write into. */
    uint32_t tail = rb->tail;
    next_tail = (tail + 1u) & (uint32_t)TEST_HARNESS_RINGBUF_MASK;

    /*
     * If next_tail == head the buffer is full.
     * Read head through the volatile pointer so the compiler fetches the
     * current value, not a stale cached one.
     */
    if (next_tail == rb->head) {
        return TEST_HARNESS_RINGBUF_ERR_FULL;
    }

    rb->data[tail] = value;

    /*
     * Write tail AFTER the data store.  On architectures without a store
     * barrier (e.g. ARM Cortex-M without __DMB) this relies on the
     * compiler not reordering the volatile write — which volatile guarantees
     * at the C level.  On the native x86 test host this is also safe because
     * x86 has a strong memory model.  For real ISR use on ARM, pair this with
     * a compiler barrier (__asm__ volatile("" ::: "memory")) if needed.
     */
    rb->tail = next_tail;

    return TEST_HARNESS_RINGBUF_OK;
}

/**
 * test_harness_ringbuf_pop — remove one value from the head.
 *
 * Returns TEST_HARNESS_RINGBUF_ERR_EMPTY if the buffer is empty and leaves
 * *out_value unchanged.
 *
 * Safe to call from the consumer context (main loop / task side of SPSC).
 *
 * @param rb         Pointer to the ring buffer instance.
 * @param out_value  Output pointer; receives the dequeued value on success.
 * @return           TEST_HARNESS_RINGBUF_OK or TEST_HARNESS_RINGBUF_ERR_EMPTY.
 */
static inline int test_harness_ringbuf_pop(TestHarnessRingBuf *rb, int32_t *out_value)
{
    uint32_t head;

    /*
     * Read tail through the volatile pointer first so we see the most recent
     * value written by the producer / ISR before checking emptiness.
     */
    if (rb->tail == rb->head) {
        return TEST_HARNESS_RINGBUF_ERR_EMPTY;
    }

    head = rb->head;
    *out_value = rb->data[head];

    /* Advance head AFTER the data read. */
    rb->head = (head + 1u) & (uint32_t)TEST_HARNESS_RINGBUF_MASK;

    return TEST_HARNESS_RINGBUF_OK;
}

#endif /* TEST_HARNESS_RINGBUF_H */
