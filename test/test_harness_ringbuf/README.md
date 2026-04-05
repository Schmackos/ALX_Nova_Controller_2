# test_harness_ringbuf

## Module Name
`test_harness_ringbuf`

## Purpose
Tests for the ISR-safe ring buffer implementation (`src/test_harness_ringbuf.h`).

## Functions Tested
- `test_harness_ringbuf_init` — Ring buffer initialization
- `test_harness_ringbuf_push` — Push elements into the buffer
- `test_harness_ringbuf_pop` — Pop elements from the buffer
- `test_harness_ringbuf_count` — Query buffer element count

## How to Run
```bash
pio test -e native -f test_harness_ringbuf
```

## Cleanup
This is a validation artifact. To remove:

1. Delete this directory:
   ```bash
   rm -rf test/test_harness_ringbuf
   ```

2. Delete the header file:
   ```bash
   rm src/test_harness_ringbuf.h
   ```

For a complete cleanup of all test harness artifacts, see `test/test_harness_utils/README.md`.
