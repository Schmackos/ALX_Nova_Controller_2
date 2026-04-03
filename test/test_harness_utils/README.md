# test_harness_utils

## Module
`test_harness_utils`

## Purpose
Unity tests for utility functions (`src/test_harness_utils.h`).

## Functions Tested
- `test_harness_clamp` — Constrain a value within min/max bounds
- `test_harness_is_valid_percentage` — Validate percentage value (0–100)
- `test_harness_map_range` — Linear map from input range to output range

## Run Tests
```bash
pio test -e native -f test_harness_utils
```

## Full Cleanup Manifest

All test harness artifacts use `test_harness_` or `test-harness-` prefix.

### Files to Remove
- `src/test_harness_ringbuf.h`
- `src/test_harness_utils.h`
- `test/test_harness_ringbuf/` (entire directory)
- `test/test_harness_utils/` (entire directory including this README)
- `web_src/js/test-harness-status.js`
- `docs-site/docs/developer/test-harness-validation.md`

### Cleanup Commands
```bash
rm src/test_harness_ringbuf.h src/test_harness_utils.h
rm -rf test/test_harness_ringbuf test/test_harness_utils
rm web_src/js/test-harness-status.js
rm docs-site/docs/developer/test-harness-validation.md
```

### Verify No Leftovers
```bash
grep -rl "test_harness_\|test-harness-" src/ test/ web_src/ docs-site/docs/
```
