---
title: Test Harness Validation
description: Documentation for the test harness validation modules used in pipeline testing.
---

# Test Harness Validation

## Purpose

The test harness validation modules are **internal artifacts used for pipeline testing and validation only**. These are not production code and should not be committed to the main codebase or shipped in firmware releases.

Test harness modules provide:

- **Ring buffer utilities** for ISR-safe data capture and transmission in test environments
- **Utility functions** for numeric operations and range mapping during validation
- **Status reporting** in the web UI for real-time test progress monitoring

All test harness components are clearly marked with `TEST ARTIFACT ONLY` comments and carry no external dependencies.

---

## Ring Buffer (`src/test_harness_ringbuf.h`)

The ring buffer is an ISR-safe circular buffer implementation using only volatile head/tail indices, with no separate count field.

### Design

The ring buffer uses a capacity+1 internal slot design to distinguish between full and empty states without additional tracking:

- **Empty**: `head == tail`
- **Full**: `(tail + 1) % (capacity + 1) == head`
- **Count**: `(tail - head + capacity + 1) % (capacity + 1)`

This design allows safe concurrent access from a producer and consumer running on different cores or in ISR context, with only volatile qualification for memory safety.

### Initialization

```c
TestHarnessRingBuf rb;
uint8_t buf[256];  // Must be at least (capacity + 1) bytes

test_harness_ringbuf_init(&rb, buf, 255);
```

### Functions

#### `test_harness_ringbuf_init`

Initialize the ring buffer.

**Parameters:**
- `rb` — Pointer to `TestHarnessRingBuf` struct
- `buf` — Backing buffer (must be at least `capacity + 1` bytes)
- `capacity` — Usable item capacity

**Returns:** None

**Notes:** Passing `capacity == 0` is valid but the buffer will always appear full.

#### `test_harness_ringbuf_push`

Write one byte to the ring buffer.

**Parameters:**
- `rb` — Pointer to the ring buffer
- `byte` — Byte to write

**Returns:**
- `1` (true) on success
- `0` (false) if the buffer is full

**Safety:** Safe to call from an ISR (producer side only).

#### `test_harness_ringbuf_pop`

Read one byte from the ring buffer.

**Parameters:**
- `rb` — Pointer to the ring buffer
- `out` — Pointer to output byte location

**Returns:**
- `1` (true) and stores the byte in `*out` on success
- `0` (false) and leaves `*out` unchanged if the buffer is empty

**Safety:** Safe to call from an ISR (consumer side only).

#### `test_harness_ringbuf_count`

Return the number of bytes currently held in the ring buffer.

**Parameters:**
- `rb` — Pointer to the ring buffer

**Returns:** Number of bytes currently in the buffer (`uint16_t`)

---

## Utilities (`src/test_harness_utils.h`)

Pure inline utility functions with no external dependencies. All functions are static inline and C99 compatible.

### Functions

#### `test_harness_clamp`

Clamp a value to a specified range.

**Parameters:**
- `value` — Value to clamp
- `min_val` — Minimum allowed value
- `max_val` — Maximum allowed value

**Returns:** Clamped value (int32_t)

**Behavior:**
- If `min_val > max_val`, returns `min_val` (defined behavior for inverted range)
- If `value < min_val`, returns `min_val`
- If `value > max_val`, returns `max_val`
- Otherwise returns `value`

#### `test_harness_is_valid_percentage`

Check if a value is a valid percentage (0–100 inclusive).

**Parameters:**
- `value` — Value to check

**Returns:**
- `true` if `0 <= value <= 100`
- `false` otherwise

#### `test_harness_map_range`

Map a value from one numeric range to another using linear interpolation.

**Parameters:**
- `value` — Value to map
- `in_min` — Input range minimum
- `in_max` — Input range maximum
- `out_min` — Output range minimum
- `out_max` — Output range maximum

**Returns:** Mapped value (int32_t)

**Behavior:**
- If `in_min == in_max`, returns `out_min` to avoid division by zero
- Otherwise, applies linear interpolation: `out_min + (value - in_min) * (out_max - out_min) / (in_max - in_min)`

---

## Web Status Card (`web_src/js/test-harness-status.js`)

Standalone JavaScript module for rendering test harness status in the web UI.

### Module

The module exports a single function `renderTestHarnessStatus` for displaying real-time test validation progress.

#### `renderTestHarnessStatus`

Render a status card showing current test harness state and metrics.

**Usage:**
```javascript
import { renderTestHarnessStatus } from './test-harness-status.js';

const container = document.getElementById('test-status');
const html = renderTestHarnessStatus(testState);
container.innerHTML = html;
```

**Parameters:**
- `testState` — Object containing current test metrics and status

**Returns:** HTML string for rendering

**Notes:** This module is a standalone artifact with no external dependencies and does not interact with the main firmware state management.

---

## Cleanup Procedure

Test harness artifacts are designed to be completely removable without impacting production code.

### Finding All Test Harness Files

```bash
# Find all test harness artifacts across the codebase
grep -r "test.harness" --include="*.h" --include="*.cpp" --include="*.js" --include="*.md" .
```

This will locate:
- Header files with test harness definitions (`src/test_harness_*.h`)
- Implementation files using test harness code
- JavaScript modules (`web_src/js/test-harness-*.js`)
- Documentation references

### Removing All Test Harness Artifacts

```bash
# Remove all test harness files
find . -name "test_harness_*" -o -name "test-harness-*" | xargs rm
```

This single command removes:
- All `test_harness_*` header and source files
- All `test-harness-*` JavaScript modules
- Any other test harness artifacts matching the naming convention

### Verification

After removal, verify no test harness references remain:

```bash
# Should return no results
grep -r "test.harness" --include="*.h" --include="*.cpp" --include="*.js" --include="*.md" .
grep -r "TEST HARNESS\|TEST ARTIFACT" --include="*.h" --include="*.cpp" --include="*.js" .
```

---

## Integration Notes

- **No production dependencies**: Test harness modules import only standard C libraries (stdint.h, stddef.h, stdbool.h)
- **Inline only**: All functions are static inline; no external .a or .so linkage required
- **Zero overhead when unused**: Unused test harness headers impose no runtime cost
- **ISR-safe**: Ring buffer is safe for ISR context; utilities are pure functions
- **Cleanup-friendly**: All test harness code is clearly marked and can be safely removed en masse

---

## See Also

- [Testing Overview](./testing.md) — Unit tests, E2E tests, and on-device testing
- [Build Setup](./build-setup.md) — Development environment and build commands
