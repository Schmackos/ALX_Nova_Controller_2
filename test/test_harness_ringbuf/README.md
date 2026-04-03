# test_harness_ringbuf

## Module
`test_harness_ringbuf`

## Purpose
Unity tests for the ISR-safe ring buffer (`src/test_harness_ringbuf.h`).

## Functions Tested
- `test_harness_ringbuf_init` — Initialize ring buffer with capacity
- `test_harness_ringbuf_push` — Push data into ring buffer
- `test_harness_ringbuf_pop` — Pop data from ring buffer
- `test_harness_ringbuf_count` — Get current count of items in buffer

## Run Tests
```bash
pio test -e native -f test_harness_ringbuf
```

## Cleanup
This is a test harness artifact. To remove:
1. Delete this directory: `rm -rf test/test_harness_ringbuf/`
2. Delete the header: `rm src/test_harness_ringbuf.h`
