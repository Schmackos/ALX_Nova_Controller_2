# test_harness_ringbuf

## Purpose
Tests for the ISR-safe ring buffer (`src/test_harness_ringbuf.h`).

## Functions Tested
- `test_harness_ringbuf_init` — Ring buffer initialization
- `test_harness_ringbuf_push` — Push elements into the buffer
- `test_harness_ringbuf_pop` — Pop elements from the buffer
- `test_harness_ringbuf_count` — Query buffer element count

## How to Run
pio test -e native -f test_harness_ringbuf

## Cleanup
This is a validation artifact. To remove, delete this directory and `src/test_harness_ringbuf.h`.
For a complete cleanup of all test harness artifacts, see `test/test_harness_utils/README.md`.
