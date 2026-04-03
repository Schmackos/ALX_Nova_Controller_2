# test_harness_utils

Unit tests for `src/test_harness_utils.h` — test harness utility functions validation module.

## Module Purpose

This module provides utility functions for test harness infrastructure, including value clamping, percentage validation, and range mapping. These helpers support test event capture and data normalization.

**Note:** This is a test/validation artifact, not production code. These utilities are used exclusively by the test harness for data processing and validation.

## Functions Tested

- `test_harness_clamp()` — Clamp value to min/max bounds
- `test_harness_is_valid_percentage()` — Validate percentage value (0-100)
- `test_harness_map_range()` — Map value from source range to target range

## Run Tests

```bash
pio test -e native -f test_harness_utils
```

## Cleanup Manifest

The test harness is a temporary validation framework. To remove all test harness artifacts from the repository, delete the following files and directories:

### Source Files

```
src/test_harness_ringbuf.h
src/test_harness_utils.h
```

### Test Modules (entire directories)

```
test/test_harness_ringbuf/
test/test_harness_utils/
```

### Web UI Integration

```
web_src/js/test-harness-status.js
```

### Documentation

```
docs-site/docs/developer/test-harness-validation.md
```

## Cleanup Commands

Find all test harness artifacts across the repository:

```bash
grep -r "test_harness\|test-harness" --include="*.h" --include="*.cpp" --include="*.js" --include="*.md" -l
```

Remove all test harness artifacts:

```bash
rm src/test_harness_ringbuf.h src/test_harness_utils.h
rm -rf test/test_harness_ringbuf/ test/test_harness_utils/
rm web_src/js/test-harness-status.js
rm docs-site/docs/developer/test-harness-validation.md
```

Verify cleanup (should return empty):

```bash
grep -r "test_harness\|test-harness" --include="*.h" --include="*.cpp" --include="*.js" --include="*.md" | grep -v node_modules | wc -l
```

## Related Files

- Ring buffer implementation: `src/test_harness_ringbuf.h`
- Utilities implementation: `src/test_harness_utils.h`
- Test suite: `test/test_harness_utils/test_utils.cpp` (created by test writer)
- Status dashboard: `web_src/js/test-harness-status.js`
- Validation guide: `docs-site/docs/developer/test-harness-validation.md`
