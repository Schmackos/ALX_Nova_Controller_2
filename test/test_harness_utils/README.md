# Test Harness Utility Tests

## Purpose

Unity test module for the utility functions (`src/test_harness_utils.h`). Part of the test harness validation concept — NOT production tests.

This module validates helper functions used internally by the test harness for value clamping, percentage validation, and range mapping operations.

## Functions Tested

- `test_harness_clamp` — Value clamping within min/max bounds
- `test_harness_is_valid_percentage` — Validation of percentage values (0-100)
- `test_harness_map_range` — Linear mapping of values from one range to another

## Run Command

```bash
pio test -e native -f test_harness_utils
```

To run all tests in the native environment:

```bash
pio test -e native
```

## Full Cleanup Manifest

All test harness artifacts (safe to delete):

### Headers

- `src/test_harness_ringbuf.h`
- `src/test_harness_utils.h`

### Test Modules

- `test/test_harness_ringbuf/` (this entire directory)
- `test/test_harness_utils/` (this entire directory)

### Web UI

- `web_src/js/test-harness-status.js`

### Documentation

- `docs-site/docs/developer/test-harness-validation.md`

### Cleanup Commands

Verify artifacts before deletion:

```bash
grep -r "test.harness" --include="*.h" --include="*.cpp" --include="*.js" --include="*.md" .
```

Remove all test harness files:

```bash
rm -f src/test_harness_ringbuf.h src/test_harness_utils.h
rm -rf test/test_harness_ringbuf test/test_harness_utils
rm -f web_src/js/test-harness-status.js
rm -f docs-site/docs/developer/test-harness-validation.md
```
