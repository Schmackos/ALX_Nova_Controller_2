# Test Harness Ring Buffer Tests

## Purpose

Unity test module for the ISR-safe ring buffer (`src/test_harness_ringbuf.h`). Part of the test harness validation concept — NOT production tests.

This module validates the ring buffer implementation used internally by the test harness for validation artifact storage and retrieval.

## Functions Tested

- `test_harness_ringbuf_init` — Initialization and capacity validation
- `test_harness_ringbuf_push` — Adding elements to the buffer with overflow handling
- `test_harness_ringbuf_pop` — Retrieving and removing elements
- `test_harness_ringbuf_count` — Accurate element counting in various states

## Run Command

```bash
pio test -e native -f test_harness_ringbuf
```

To run all tests in the native environment:

```bash
pio test -e native
```

## Cleanup

This module is a test artifact. To remove:

```bash
rm -rf test/test_harness_ringbuf/
rm -f src/test_harness_ringbuf.h
```

For a complete cleanup of all test harness artifacts across the project, see the manifest in `test/test_harness_utils/README.md`.
