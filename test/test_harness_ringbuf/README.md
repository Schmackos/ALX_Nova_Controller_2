# test_harness_ringbuf

Unit tests for `src/test_harness_ringbuf.h` — ISR-safe ring buffer validation module.

## Module Purpose

This module provides an ISR-safe circular buffer (`test_harness_ringbuf`) for atomic recording of test events without memory allocation. Used by test harness infrastructure to log events during hardware testing and validation.

**Note:** This is a test/validation artifact, not production code. The ring buffer is used exclusively by the test harness for event capture and diagnostics.

## Functions Tested

- `test_harness_ringbuf_init()` — Initialize ring buffer with fixed capacity
- `test_harness_ringbuf_push()` — Atomically push event into buffer (ISR-safe)
- `test_harness_ringbuf_pop()` — Atomically pop event from buffer (ISR-safe)
- `test_harness_ringbuf_count()` — Query current event count in buffer

## Run Tests

```bash
pio test -e native -f test_harness_ringbuf
```

## Related Files

- Implementation: `src/test_harness_ringbuf.h`
- Test suite: `test/test_harness_ringbuf/test_ringbuf.cpp` (created by test writer)
