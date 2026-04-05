# test_harness_utils

## Module Name
`test_harness_utils`

## Purpose
Tests for utility functions (`src/test_harness_utils.h`).

## Functions Tested
- `test_harness_clamp` — Clamp values within range
- `test_harness_is_valid_percentage` — Validate percentage values
- `test_harness_map_range` — Map values from one range to another

## How to Run
```bash
pio test -e native -f test_harness_utils
```

## Full Cleanup Manifest

This is a validation artifact. To remove all test harness artifacts across the project:

### Files to Remove
- `src/test_harness_ringbuf.h`
- `src/test_harness_utils.h`
- `web_src/js/test-harness-status.js`
- `docs-site/docs/developer/test-harness-validation.md`

### Directories to Remove
- `test/test_harness_ringbuf/`
- `test/test_harness_utils/`

### Quick Cleanup Command
```bash
rm src/test_harness_ringbuf.h src/test_harness_utils.h web_src/js/test-harness-status.js docs-site/docs/developer/test-harness-validation.md && rm -rf test/test_harness_ringbuf test/test_harness_utils
```

### Verify No Remnants
```bash
grep -r "test_harness_\|test-harness-" --include="*.h" --include="*.cpp" --include="*.js" --include="*.md" -l
```

The `grep` command should return no matches if cleanup is complete.
