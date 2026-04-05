#ifndef TEST_HARNESS_RINGBUF_H
#define TEST_HARNESS_RINGBUF_H

/*
 * test_harness_ringbuf.h — ISR-safe ring buffer (pure inline, no dependencies)
 *
 * TEST ARTIFACT ONLY — not production code.
 *
 * Design: capacity+1 internal slots so a full buffer and an empty buffer
 * can be distinguished without a separate count field, using only volatile
 * head/tail indices and modulo arithmetic.
 *
 *   empty : head == tail
 *   full  : (tail + 1) % (capacity + 1) == head
 *   count : (tail - head + capacity + 1) % (capacity + 1)
 *
 * The backing buffer supplied by the caller must be at least (capacity+1)
 * bytes.  test_harness_ringbuf_init() records this internal slot count as
 * (capacity + 1) in the struct field so that all arithmetic uses a single
 * stored value.
 *
 * ISR safety: head is advanced only by the consumer; tail is advanced only
 * by the producer.  On single-core or when only one side runs in an ISR the
 * volatile qualification on both indices is sufficient.  On multi-core
 * hardware pair this with appropriate memory barriers if needed.
 */

#include <stdint.h>
#include <stddef.h>

typedef struct {
    volatile uint16_t head;  /* consumer index — advanced by pop */
    volatile uint16_t tail;  /* producer index — advanced by push */
    uint8_t          *buffer;
    uint16_t          capacity; /* usable item capacity; internal slots = capacity+1 */
} TestHarnessRingBuf;

/*
 * test_harness_ringbuf_init — initialise the ring buffer.
 *
 * buf must point to at least (capacity + 1) bytes of storage.
 * Passing capacity == 0 is valid but the buffer will always appear full.
 */
static inline void test_harness_ringbuf_init(TestHarnessRingBuf *rb,
                                              uint8_t            *buf,
                                              uint16_t            capacity)
{
    rb->head     = 0;
    rb->tail     = 0;
    rb->buffer   = buf;
    rb->capacity = capacity;
}

/*
 * test_harness_ringbuf_push — write one byte to the ring buffer.
 *
 * Returns 1 (true) on success, 0 (false) if the buffer is full.
 * Safe to call from an ISR (producer side).
 */
static inline int test_harness_ringbuf_push(TestHarnessRingBuf *rb, uint8_t byte)
{
    uint16_t slots    = (uint16_t)(rb->capacity + 1u);
    uint16_t cur_tail = rb->tail;
    uint16_t next     = (uint16_t)((cur_tail + 1u) % slots);

    if (next == rb->head) {
        return 0; /* full */
    }

    rb->buffer[cur_tail] = byte;
    rb->tail             = next;
    return 1;
}

/*
 * test_harness_ringbuf_pop — read one byte from the ring buffer.
 *
 * Returns 1 (true) and stores the byte in *out on success.
 * Returns 0 (false) and leaves *out unchanged if the buffer is empty.
 * Safe to call from an ISR (consumer side).
 */
static inline int test_harness_ringbuf_pop(TestHarnessRingBuf *rb, uint8_t *out)
{
    uint16_t slots    = (uint16_t)(rb->capacity + 1u);
    uint16_t cur_head = rb->head;

    if (cur_head == rb->tail) {
        return 0; /* empty */
    }

    *out      = rb->buffer[cur_head];
    rb->head  = (uint16_t)((cur_head + 1u) % slots);
    return 1;
}

/*
 * test_harness_ringbuf_count — return the number of bytes currently held.
 */
static inline uint16_t test_harness_ringbuf_count(const TestHarnessRingBuf *rb)
{
    uint16_t slots = (uint16_t)(rb->capacity + 1u);
    return (uint16_t)((rb->tail - rb->head + slots) % slots);
}

#endif /* TEST_HARNESS_RINGBUF_H */
