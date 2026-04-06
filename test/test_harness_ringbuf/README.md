# test_harness_ringbuf

Unit tests for the ISR-safe ring buffer declared in `src/test_harness_ringbuf.h`.

## Functions under test

- `test_harness_ringbuf_init` — initialise the buffer to a known empty state
- `test_harness_ringbuf_push` — enqueue a value (returns false when full)
- `test_harness_ringbuf_pop` — dequeue the oldest value (returns false when empty)
- `test_harness_ringbuf_count` — return the number of items currently held

## Running

```bash
pio test -e native -f test_harness_ringbuf
```

## Notes

This is a **pipeline validation artifact**. It exists solely to verify that the
test-harness kitchen-sink workflow can build, register, and run a new test
module end-to-end. It is safe to delete once validation passes — see the
cleanup manifest in `test/test_harness_utils/README.md`.
