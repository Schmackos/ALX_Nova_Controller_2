#ifndef TEST_HARNESS_RINGBUF_H
#define TEST_HARNESS_RINGBUF_H

// test_harness_ringbuf.h — ISR-safe single-producer/single-consumer ring buffer.
//
// Designed for test harness use: safely captures int16_t samples written by an
// ISR (producer at head) and consumed by a main-loop task (consumer at tail).
//
// ISR safety guarantees:
//   - head and tail indices are volatile — the compiler may not cache them
//     across function calls or memory barriers, ensuring each access re-reads
//     the current value from memory.
//   - Capacity is a power of two so wrap-around is a single AND mask, which is
//     atomic on all supported architectures (no read-modify-write hazard).
//   - The producer (ISR) writes data then advances head; the consumer reads
//     data then advances tail.  The two sides never write the same index field,
//     so no mutex is required for single-producer/single-consumer use.
//
// Constraints:
//   - Exactly one producer and one consumer at a time (SPSC).
//   - Do NOT call push() from two ISRs simultaneously.
//   - Do NOT call pop() from two tasks simultaneously.
//   - All functions are static inline — no .cpp translation unit required.
//     This lets the native test platform (test_build_src = no) include this
//     header directly without needing access to src/ object files.
//
// Standalone — only <stdint.h>, <stdbool.h>, and <string.h> are included.
// No Arduino, ESP-IDF, or FreeRTOS dependencies.

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Capacity
// ---------------------------------------------------------------------------

// Number of elements in the ring buffer.  Must be a power of two so that the
// index wrap mask (TEST_HARNESS_RINGBUF_MASK) is computed correctly.
#ifndef TEST_HARNESS_RINGBUF_CAPACITY
#define TEST_HARNESS_RINGBUF_CAPACITY 16
#endif

// Compile-time assertion: capacity must be a power of two.
// If this triggers, change TEST_HARNESS_RINGBUF_CAPACITY to 16, 32, 64, etc.
typedef char _test_harness_ringbuf_capacity_must_be_power_of_two[
    ((TEST_HARNESS_RINGBUF_CAPACITY & (TEST_HARNESS_RINGBUF_CAPACITY - 1)) == 0) ? 1 : -1
];

// Bit-mask used for modular index wrap-around without division or branch.
#define TEST_HARNESS_RINGBUF_MASK ((uint16_t)(TEST_HARNESS_RINGBUF_CAPACITY - 1))

// ---------------------------------------------------------------------------
// Struct
// ---------------------------------------------------------------------------

// TestHarnessRingbuf — the ring buffer state.
//
// head: next write index (advanced by producer/ISR after storing a sample).
// tail: next read  index (advanced by consumer/task after loading a sample).
//
// The buffer holds (head - tail) & MASK items.  It is full when the next head
// would equal tail, i.e. ((head + 1) & MASK) == tail.
typedef struct {
    volatile uint16_t head;                             // write index (producer)
    volatile uint16_t tail;                             // read  index (consumer)
    int16_t           data[TEST_HARNESS_RINGBUF_CAPACITY]; // sample storage
} TestHarnessRingbuf;

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

// test_harness_ringbuf_init — reset the buffer to empty.
//
// Call once before use.  Safe to call from a task context; not intended to be
// called while an ISR producer is active (would create a race on head/tail).
static inline void test_harness_ringbuf_init(TestHarnessRingbuf *buf) {
    buf->head = 0;
    buf->tail = 0;
    memset(buf->data, 0, sizeof(buf->data));
}

// test_harness_ringbuf_push — write one int16_t sample into the buffer.
//
// Intended for the producer side (e.g. an ISR or audio DMA callback).
// Returns true  on success.
// Returns false if the buffer is full — the sample is dropped and head is
//               left unchanged.
//
// ISR safety: reads volatile tail once to snapshot consumer progress, writes
// data before advancing head, so the consumer never sees a slot whose data
// has not yet been written.
static inline bool test_harness_ringbuf_push(TestHarnessRingbuf *buf, int16_t value) {
    uint16_t next_head = (buf->head + 1u) & TEST_HARNESS_RINGBUF_MASK;

    // Buffer is full when advancing head would collide with tail.
    if (next_head == buf->tail) {
        return false;
    }

    buf->data[buf->head] = value;

    // Commit the write — advance head after data is stored so the consumer
    // only sees slots with valid data.
    buf->head = next_head;

    return true;
}

// test_harness_ringbuf_pop — read one int16_t sample from the buffer.
//
// Intended for the consumer side (e.g. a main-loop task or test thread).
// Returns true  on success; *out is populated with the oldest sample.
// Returns false if the buffer is empty; *out is left unchanged.
//
// ISR safety: reads volatile head once to snapshot producer progress, reads
// data before advancing tail, ensuring the producer cannot reclaim the slot
// while the consumer is still reading it.
static inline bool test_harness_ringbuf_pop(TestHarnessRingbuf *buf, int16_t *out) {
    // Snapshot head to avoid repeated volatile reads inside the function.
    uint16_t current_head = buf->head;

    if (buf->tail == current_head) {
        return false; // empty
    }

    *out = buf->data[buf->tail];

    // Advance tail after reading so the producer cannot overwrite this slot
    // before we are done with it.
    buf->tail = (buf->tail + 1u) & TEST_HARNESS_RINGBUF_MASK;

    return true;
}

// test_harness_ringbuf_count — return the number of items currently in the buffer.
//
// Safe to call from either side; result is a snapshot and may be stale by the
// time the caller acts on it.  Use only for diagnostics or polling loops, not
// for synchronisation decisions in concurrent code.
static inline uint16_t test_harness_ringbuf_count(TestHarnessRingbuf *buf) {
    // Snapshot both indices.  On architectures where 16-bit reads are not
    // atomic with respect to interrupts, the worst case is a one-element
    // error in the reported count, which is acceptable for a diagnostic path.
    uint16_t h = buf->head;
    uint16_t t = buf->tail;
    return (h - t) & TEST_HARNESS_RINGBUF_MASK;
}

#endif /* TEST_HARNESS_RINGBUF_H */
