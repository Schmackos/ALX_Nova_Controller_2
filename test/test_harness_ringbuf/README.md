# test_harness_ringbuf

Unity test module for the ISR-safe ring buffer (`src/test_harness_ringbuf.h`).

## Purpose

Validates the `TestHarnessRingBuf` implementation — a fixed-size, ISR-safe ring buffer with volatile-qualified head/tail indices and modular arithmetic wrap-around. This is a test harness validation artifact, not production code.

## Functions Under Test

| Function | Description |
|---|---|
| `test_harness_ringbuf_init` | Initialize buffer with given capacity |
| `test_harness_ringbuf_push` | Push value, returns false if full |
| `test_harness_ringbuf_pop` | Pop value, returns false if empty |
| `test_harness_ringbuf_count` | Return current item count |

## Running Tests

```bash
pio test -e native -f test_harness_ringbuf
```

## Cleanup

This module is part of the test harness validation concept. To remove, delete:
- `test/test_harness_ringbuf/` (this directory)
- `src/test_harness_ringbuf.h`

See `test/test_harness_utils/README.md` for the full cleanup manifest.
