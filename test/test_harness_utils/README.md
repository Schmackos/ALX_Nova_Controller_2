# test_harness_utils

Unity test module for pure utility functions (`src/test_harness_utils.h`).

## Purpose

Validates three inline utility functions: clamp, percentage validation, and range mapping. This is a test harness validation artifact, not production code.

## Functions Under Test

| Function | Description |
|---|---|
| `test_harness_clamp` | Clamp int32 value to [min, max] range |
| `test_harness_is_valid_percentage` | Validate float is in [0.0, 100.0] |
| `test_harness_map_range` | Linear map from one range to another |

## Running Tests

```bash
pio test -e native -f test_harness_utils
```

## Cleanup — Full Test Harness Manifest

All test harness artifacts use `test_harness_` or `test-harness-` prefix for safe identification. To remove ALL test harness validation artifacts:

### Files to Delete

| File | Type |
|---|---|
| `src/test_harness_ringbuf.h` | C++ header — ISR-safe ring buffer |
| `src/test_harness_utils.h` | C++ header — utility functions |
| `test/test_harness_ringbuf/test_harness_ringbuf.cpp` | Unity tests — ring buffer |
| `test/test_harness_ringbuf/README.md` | Documentation |
| `test/test_harness_utils/test_harness_utils.cpp` | Unity tests — utilities |
| `test/test_harness_utils/README.md` | Documentation (this file) |
| `web_src/js/test-harness-status.js` | JS module — status card |
| `docs-site/docs/developer/test-harness-validation.md` | Docusaurus page |

### Cleanup Commands

```bash
# Verify all artifacts (should list only the files above)
grep -rl "test_harness_\|test-harness-" src/ test/ web_src/ docs-site/docs/

# Remove all test harness artifacts
rm src/test_harness_ringbuf.h
rm src/test_harness_utils.h
rm -rf test/test_harness_ringbuf/
rm -rf test/test_harness_utils/
rm web_src/js/test-harness-status.js
rm docs-site/docs/developer/test-harness-validation.md
```

### Safety Notes

- No existing files were modified by this concept
- No changes to `platformio.ini`, `sidebars.js`, or `.eslintrc.json`
- No production code references these modules
- Safe to delete without any rollback needed
