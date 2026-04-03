---
title: Test Harness Validation
description: Documentation for the test harness validation modules used to verify the native test pipeline.
---

## Overview

The test harness modules provide validation infrastructure for the native test pipeline. These modules are **not production code** — they exist solely to verify that the build system, test dispatch, and cross-module testing work correctly.

The test harness validates:
- Test module compilation and linking
- Test function discovery and execution
- ISR-safe data structure behavior (ring buffer)
- Pure utility function behavior (math, validation)

All test harness artifacts use the `test-harness-` prefix and should be cleaned up after validation is complete.

## Modules

### `test_harness_ringbuf.h`

ISR-safe ring buffer implementation for validating concurrent-safe data structures in the test pipeline.

**Functions:**
- `test_harness_ringbuf_init(ringbuf_t* buf, size_t capacity)` — Initialize the ring buffer with given capacity
- `test_harness_ringbuf_push(ringbuf_t* buf, uint32_t value)` — Push a value (returns success/overflow status)
- `test_harness_ringbuf_pop(ringbuf_t* buf, uint32_t* value)` — Pop a value (returns success/empty status)
- `test_harness_ringbuf_count(ringbuf_t* buf)` — Get current item count

**Design:**
- Volatile head/tail indices ensure visibility across CPU cores
- Modular arithmetic wrap-around for fixed-size buffers
- No locks or synchronization primitives — ISR-safe by design
- Suitable for interrupt contexts where sleeping is not allowed

**Test module:** `test/test_harness_ringbuf/`

### `test_harness_utils.h`

Pure utility functions for validating basic math and validation routines.

**Functions:**
- `test_harness_clamp(int32_t value, int32_t min, int32_t max)` — Clamp value to range
- `test_harness_is_valid_percentage(int32_t value)` — Validate value is in [0, 100]
- `test_harness_map_range(int32_t value, int32_t from_min, int32_t from_max, int32_t to_min, int32_t to_max)` — Linear range mapping

**Design:**
- Static inline implementations for zero runtime overhead
- No dependencies on external libraries
- Straightforward logic for easy verification in tests

**Test module:** `test/test_harness_utils/`

## Web Status Card

The web UI includes a validation status card rendered by:

```javascript
// web_src/js/test-harness-status.js
```

This card displays the current state of test harness validation during development builds. It is not part of the production web UI.

## Running Tests

Run the test harness modules individually:

```bash
# Test the ring buffer implementation
pio test -e native -f test_harness_ringbuf

# Test utility functions
pio test -e native -f test_harness_utils

# Run all test harness tests
pio test -e native -f test_harness
```

Tests use the native platform (no hardware required) and execute with the standard Unity test runner.

## Cleanup Procedure

After validation is complete, remove all test harness artifacts:

```bash
# Find all test harness files
grep -rl "test_harness_\|test-harness-" src/ test/ web_src/ docs-site/docs/

# Remove header files
rm src/test_harness_ringbuf.h
rm src/test_harness_utils.h

# Remove test modules
rm -rf test/test_harness_ringbuf/
rm -rf test/test_harness_utils/

# Remove web UI component
rm web_src/js/test-harness-status.js

# Remove documentation page
rm docs-site/docs/developer/test-harness-validation.md
```

All test harness files follow the `test-harness-` or `test_harness_` naming prefix for safe identification and cleanup.

## Notes

- Test harness modules are compiled only when running `pio test -e native`
- They do not impact production firmware builds
- Source files in `src/test_harness_*.h` are header-only and not linked into production binaries
- The test modules in `test/test_harness_*/` are isolated and do not depend on production code
