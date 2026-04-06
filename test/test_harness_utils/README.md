# test_harness_utils

Unit tests for the utility functions declared in `src/test_harness_utils.h`.

## Functions under test

- `test_harness_clamp` — clamp a value to [min, max]
- `test_harness_is_valid_percentage` — return true when a float is in [0.0, 100.0]
- `test_harness_map_range` — linearly map a value from one range to another

## Running

```bash
pio test -e native -f test_harness_utils
```

## Notes

This is a **pipeline validation artifact** — see the cleanup section below.

---

## Cleanup

All test harness artifacts use the `test_harness_` or `test-harness-` prefix.
To remove all artifacts after validation:

### Files to delete
- src/test_harness_ringbuf.h
- src/test_harness_utils.h
- test/test_harness_ringbuf/test_harness_ringbuf.cpp
- test/test_harness_ringbuf/README.md
- test/test_harness_utils/test_harness_utils.cpp
- test/test_harness_utils/README.md
- web_src/js/test-harness-status.js
- docs-site/docs/developer/test-harness-validation.md

### Directories to remove
- test/test_harness_ringbuf/
- test/test_harness_utils/

### Find command
```bash
grep -r "test_harness_\|test-harness-" --include="*.h" --include="*.cpp" --include="*.js" --include="*.md"
```
