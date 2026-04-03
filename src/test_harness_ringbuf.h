#ifndef TEST_HARNESS_RINGBUF_H
#define TEST_HARNESS_RINGBUF_H

// test_harness_ringbuf.h
// ISR-safe ring buffer for ALX Nova test harness validation.
//
// Design constraints:
//   - Fixed-size internal storage (32 elements, int32_t)
//   - volatile head/tail for ISR-safe access without atomics on single-core reads
//   - All functions static inline -- no src/ compilation needed (test_build_src = no)
//   - No dynamic allocation, no external dependencies beyond stdint/stdbool

#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Capacity constant
// ---------------------------------------------------------------------------

#define TEST_HARNESS_RINGBUF_MAX_CAPACITY 32

// ---------------------------------------------------------------------------
// Struct definition
// ---------------------------------------------------------------------------

typedef struct {
    int32_t          buf[TEST_HARNESS_RINGBUF_MAX_CAPACITY];
    volatile uint16_t head;      // next write position (producer advances this)
    volatile uint16_t tail;      // next read position  (consumer advances this)
    uint16_t          capacity;  // active capacity (<= TEST_HARNESS_RINGBUF_MAX_CAPACITY)
} TestHarnessRingBuf;

// ---------------------------------------------------------------------------
// API -- all static inline so native tests can include this header directly
// ---------------------------------------------------------------------------

// Initialize the ring buffer. capacity must be <= TEST_HARNESS_RINGBUF_MAX_CAPACITY.
// Excess capacity is clamped silently.
static inline void test_harness_ringbuf_init(TestHarnessRingBuf* rb, uint16_t capacity) {
    if (!rb) return;
    rb->head     = 0;
    rb->tail     = 0;
    rb->capacity = (capacity == 0 || capacity > TEST_HARNESS_RINGBUF_MAX_CAPACITY)
                       ? TEST_HARNESS_RINGBUF_MAX_CAPACITY
                       : capacity;
}

// Returns the number of items currently stored in the buffer.
static inline uint16_t test_harness_ringbuf_count(const TestHarnessRingBuf* rb) {
    if (!rb || rb->capacity == 0) return 0;
    // head and tail are in absolute (unwrapped) space; difference gives count.
    return (uint16_t)(rb->head - rb->tail);
}

// Push a value onto the buffer. Returns false if the buffer is full.
static inline bool test_harness_ringbuf_push(TestHarnessRingBuf* rb, int32_t value) {
    if (!rb || rb->capacity == 0) return false;
    if (test_harness_ringbuf_count(rb) >= rb->capacity) return false;
    rb->buf[rb->head % rb->capacity] = value;
    rb->head = (uint16_t)(rb->head + 1);
    return true;
}

// Pop a value from the buffer into *out. Returns false if the buffer is empty.
static inline bool test_harness_ringbuf_pop(TestHarnessRingBuf* rb, int32_t* out) {
    if (!rb || !out || rb->capacity == 0) return false;
    if (test_harness_ringbuf_count(rb) == 0) return false;
    *out = rb->buf[rb->tail % rb->capacity];
    rb->tail = (uint16_t)(rb->tail + 1);
    return true;
}

#endif // TEST_HARNESS_RINGBUF_H
