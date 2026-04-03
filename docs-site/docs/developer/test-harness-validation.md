---
title: Test Harness Validation Modules
sidebar_position: 99
description: Documentation for test harness validation modules including ring buffer, utility functions, and web status monitoring.
---

# Test Harness Validation Modules

This page documents the test harness validation artifacts — standalone modules designed for validation, diagnostics, and bench testing. All test harness code is clearly marked as **NOT PRODUCTION CODE** and uses a consistent prefix for safe cleanup.

:::info Temporary Artifacts
Test harness modules are intended for temporary use during development, testing, and validation. They are not part of the production firmware and should be removed before shipping.
:::

---

## Overview

The test harness suite consists of three core modules:

1. **ISR-safe ring buffer** (`test_harness_ringbuf.h`) — single-producer/single-consumer queue for low-level diagnostics
2. **Utility functions** (`test_harness_utils.h`) — pure inline math utilities for validation calculations
3. **Web status module** (`test-harness-status.js`) — standalone DOM rendering for harness status display

All identifiers use the `test_harness_` or `test-harness-` prefix, making them safe to locate and remove via grep.

---

## Ring Buffer (`src/test_harness_ringbuf.h`)

### Purpose

An ISR-safe single-producer/single-consumer ring buffer optimized for capturing diagnostic data from interrupt service routines without blocking or complex synchronization.

### Design

- **Capacity**: Fixed at compile time via `TEST_HARNESS_RINGBUF_CAPACITY` (default: 64)
- **Data type**: `int32_t` values
- **Synchronization**: Volatile head/tail indices guarantee ISR-safety on architectures with atomic word reads/writes
- **Power-of-two constraint**: Capacity must be a power of two (enforced via compile-time assertion)
- **Overflow policy**: `push()` when full returns `TEST_HARNESS_RINGBUF_ERR_FULL` and discards the incoming value (safe for diagnostics)
- **Empty policy**: `pop()` when empty returns `TEST_HARNESS_RINGBUF_ERR_EMPTY` and leaves the output unchanged

### Data Structure

```c
typedef struct {
    int32_t          data[TEST_HARNESS_RINGBUF_CAPACITY];
    volatile uint32_t head; /* read index — advanced by pop */
    volatile uint32_t tail; /* write index — advanced by push */
} TestHarnessRingBuf;
```

### API Functions

#### `test_harness_ringbuf_init(TestHarnessRingBuf *rb)`

Zero-initializes the ring buffer. Must be called before any push/pop operations.

**When to call**: During initialization from a safe (non-ISR) context.

```c
TestHarnessRingBuf rb;
test_harness_ringbuf_init(&rb);
```

#### `test_harness_ringbuf_push(TestHarnessRingBuf *rb, int32_t value)`

Insert one value at the tail. Safe to call from an ISR (producer side).

**Returns**:
- `TEST_HARNESS_RINGBUF_OK` (0) — value was enqueued
- `TEST_HARNESS_RINGBUF_ERR_FULL` (-1) — buffer is full; value was discarded

**Overflow behavior**: When the buffer is full, the oldest data is NOT overwritten. The new value is silently dropped. This is the safer choice for ISR contexts because overwriting data silently can hide timing problems in tests.

```c
if (test_harness_ringbuf_push(&rb, sample_value) == TEST_HARNESS_RINGBUF_OK) {
    // Sample was enqueued
}
```

#### `test_harness_ringbuf_pop(TestHarnessRingBuf *rb, int32_t *out_value)`

Remove one value from the head. Safe to call from the consumer context (main loop / task).

**Returns**:
- `TEST_HARNESS_RINGBUF_OK` (0) — value was dequeued and stored in `*out_value`
- `TEST_HARNESS_RINGBUF_ERR_EMPTY` (-2) — buffer is empty; `*out_value` is unchanged

```c
int32_t sample;
if (test_harness_ringbuf_pop(&rb, &sample) == 0) {
    // sample now contains the dequeued value
    printf("Sample: %ld\n", (long)sample);
}
```

#### `test_harness_ringbuf_count(const TestHarnessRingBuf *rb)`

Return the number of items currently stored (snapshot).

**Returns**: Unsigned integer in range [0, CAPACITY-1].

**Note**: The buffer uses a "leave one slot empty" full-detection strategy, so the usable capacity is (CAPACITY - 1) items. The count may change immediately after return if the other side (producer or consumer) runs.

```c
uint32_t items = test_harness_ringbuf_count(&rb);
if (items > 0) {
    // At least one item is available
}
```

### Configuration

Override the default capacity by defining the macro before including the header:

```c
#define TEST_HARNESS_RINGBUF_CAPACITY 256
#include "test_harness_ringbuf.h"
```

The capacity MUST be a power of two (1, 2, 4, 8, 16, 32, 64, 128, ...). A compile-time assertion will reject invalid values.

### Return Codes

```c
#define TEST_HARNESS_RINGBUF_OK         (0)   // Success
#define TEST_HARNESS_RINGBUF_ERR_FULL   (-1)  // push() on full buffer
#define TEST_HARNESS_RINGBUF_ERR_EMPTY  (-2)  // pop() on empty buffer
```

### Implementation Notes

- All functions are **static inline** so they compile cleanly under the native test environment where `test_build_src = no` prevents `src/` from being compiled as a separate translation unit.
- Volatile indices ensure the compiler cannot cache head/tail values across ISR boundaries.
- No dynamic allocation — the entire buffer is stack-allocated at compile time.
- Memory barrier considerations: On ARM architectures without explicit barriers, volatile writes provide sufficient synchronization. On the native x86 test host, the strong memory model makes this safe without additional barriers.

---

## Utility Functions (`src/test_harness_utils.h`)

### Purpose

Pure inline utility functions for validation calculations. Fully self-contained with no Arduino, FreeRTOS, or ESP-IDF dependencies. All functions operate on IEEE 754 double-precision floats.

### API Functions

#### `test_harness_clamp(double value, double lo, double hi)`

Clamp a value to a closed interval.

**Returns**:
- `lo` when `value < lo`
- `hi` when `value > hi`
- `value` otherwise

**Special case**: NaN input propagates as-is (comparison with NaN is always false, so the function returns the unmodified value rather than silently clamping to a boundary).

```c
double level = 0.85;
double clamped = test_harness_clamp(level, 0.0, 1.0);  // Returns 0.85
```

#### `test_harness_is_valid_percentage(double value)`

Validate that a value is a percentage in the range [0, 100].

**Returns**:
- Non-zero (true) if `0 <= value <= 100`
- Zero (false) otherwise, including for NaN and infinity

```c
if (test_harness_is_valid_percentage(50.5)) {
    // 50.5 is a valid percentage
}
if (!test_harness_is_valid_percentage(NAN)) {
    // NaN is not a valid percentage
}
```

#### `test_harness_map_range(double value, double in_min, double in_max, double out_min, double out_max)`

Linear map from an input range to an output range.

Equivalent to Arduino's `map()` but operates on doubles and is numerically correct for floating-point ranges.

```c
// Map 50 from [0, 100] to [0.0, 1.0]
double normalized = test_harness_map_range(50.0, 0.0, 100.0, 0.0, 1.0);
// normalized == 0.5
```

**Edge cases**:
- **Zero-width input range** (`in_min == in_max`): Returns `out_min` to avoid division by zero
- **NaN or infinity**: Result is unspecified but will not trap or invoke undefined behavior — standard IEEE 754 propagation applies

**Usage in validation**:
```c
// Scale a 0–1 DSP level to a 20–20000 Hz frequency range
double hz = test_harness_map_range(param, 0.0, 1.0, 20.0, 20000.0);
```

### Configuration

No configuration needed — all functions are compile-time constants suitable for any environment that has `<math.h>` and `isnan()`/`isinf()`.

---

## Web Status Module (`web_src/js/test-harness-status.js`)

### Purpose

A standalone web module that renders a test harness status card into the DOM. Designed to be included in HTML without depending on the main web server build pipeline or other JavaScript modules.

### Exports

#### `renderTestHarnessStatus(info, container)`

Render a test harness status card into the given container element.

**Parameters**:
- `info` (object, optional) — Module information with optional fields:
  - `name` (string) — Human-readable module name, defaults to "Test Harness"
  - `version` (string) — Semantic version string (e.g., "1.2.3"), defaults to "—"
  - `status` (string) — One of "idle", "running", "pass", "fail", defaults to "idle"
  - `testCount` (number) — Total number of tests in the harness, defaults to "—"
  - `lastRun` (string) — ISO 8601 timestamp of the most recent run, defaults to "—"
- `container` (Element, optional) — DOM element to render into. Falls back to `document.getElementById("tharStatusContainer")` when omitted.

**Returns**: void

### Usage Example

```html
<!-- In your HTML -->
<div id="tharStatusContainer"></div>

<!-- Call from JavaScript -->
<script src="test-harness-status.js"></script>
<script>
  renderTestHarnessStatus({
    name: 'Audio DSP Validation',
    version: '1.0.0',
    status: 'running',
    testCount: 2847,
    lastRun: '2026-04-03T12:34:56Z'
  });
</script>
```

### Status Card Layout

The rendered card displays:
- A test harness icon (Material Design Icons / mdi-test-tube)
- Module name
- Status badge with color coding:
  - Gray for "idle"
  - Animated pulse for "running"
  - Green for "pass"
  - Red for "fail"
- Three info rows: Version, Tests, Last Run

### HTML Structure

```html
<div class="thar-card">
  <div class="thar-header">
    <!-- Icon, name, status badge -->
  </div>
  <div class="thar-body">
    <!-- Info rows -->
  </div>
</div>
```

### Security Model

All user-provided input is HTML-escaped before insertion into the DOM:

1. The `_tharBuildRow()` helper escapes `&`, `<`, and `>` in both label and value
2. The `_tharBuildCardHtml()` builder escapes the module name in the header
3. Timestamps are converted to locale-specific strings using `Date.toLocaleString()`
4. The final HTML is inserted via `innerHTML` (with ESLint exception `no-restricted-syntax` for the static template pattern)

**Static template pattern**: All HTML is built from static templates with values escaped before insertion. No external data sources are allowed in `innerHTML`. The ESLint comment documents this intent.

### Styling

The module defines the following CSS classes (you must provide the styles):

- `.thar-card` — Card container
- `.thar-header` — Header with icon, name, and status badge
- `.thar-title` — Module name
- `.thar-status-badge` — Status display
- `.thar-status-idle` — Status class for "idle"
- `.thar-status-running` — Status class for "running" (recommend animated pulse)
- `.thar-status-pass` — Status class for "pass" (green)
- `.thar-status-fail` — Status class for "fail" (red)
- `.thar-body` — Info rows container
- `.thar-row` — Single info row
- `.thar-label` — Row label text
- `.thar-value` — Row value text

Example CSS:

```css
.thar-card {
  border: 1px solid #ccc;
  border-radius: 8px;
  padding: 16px;
  font-family: monospace;
  background-color: #fafafa;
}

.thar-header {
  display: flex;
  align-items: center;
  gap: 8px;
  margin-bottom: 12px;
}

.thar-status-running {
  animation: pulse 2s infinite;
}

@keyframes pulse {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.6; }
}

.thar-status-pass {
  color: green;
}

.thar-status-fail {
  color: red;
}
```

---

## Test Modules

### C++ Tests for Ring Buffer (`test/test_harness_ringbuf/`)

Unity-based tests covering:
- Buffer initialization
- Push/pop happy paths
- Overflow (push on full)
- Underflow (pop on empty)
- Count tracking
- Circular wrap-around
- ISR-safety (volatile access patterns)

Run with:
```bash
pio test -e native -f test_harness_ringbuf
```

### C++ Tests for Utilities (`test/test_harness_utils/`)

Unity-based tests covering:
- `clamp()` with normal, boundary, and NaN inputs
- `is_valid_percentage()` with percentages, out-of-range, NaN, and infinity
- `map_range()` with normal ranges, zero-width ranges, and IEEE 754 edge cases

Run with:
```bash
pio test -e native -f test_harness_utils
```

---

## Cleanup Procedure

All test harness artifacts use a consistent naming convention: `test_harness_` (C/C++) or `test-harness-` (JavaScript). This makes removal safe and non-destructive.

### Finding All Test Harness Code

Use this grep command to locate all test harness artifacts across the codebase:

```bash
grep -r "test_harness\|test-harness" \
  --include="*.h" \
  --include="*.cpp" \
  --include="*.js" \
  --include="*.md"
```

This will find:
- Headers: `src/test_harness_*.h`
- Tests: `test/test_harness_*/`
- Web modules: `web_src/js/test-harness-*.js`
- Documentations: References in `.md` files

### Removal Checklist

1. **Remove header files**:
   ```bash
   rm src/test_harness_ringbuf.h
   rm src/test_harness_utils.h
   ```

2. **Remove test directories**:
   ```bash
   rm -rf test/test_harness_ringbuf
   rm -rf test/test_harness_utils
   ```

3. **Remove web modules**:
   ```bash
   rm web_src/js/test-harness-status.js
   ```

4. **Remove from `web_src/.eslintrc.json`** (if any globals were added):
   - Search for and remove any `renderTestHarnessStatus` or other test harness exports

5. **Remove from `src/.eslintrc.json`** (if any ESLint globals were added)

6. **Rebuild web assets** (after removing JS):
   ```bash
   node tools/build_web_assets.js
   ```

7. **Run tests to confirm no breakage**:
   ```bash
   pio test -e native -v
   cd e2e && npx playwright test
   ```

8. **Update documentation** (this file and any references):
   - Optionally remove this page from Docusaurus

### Verification

After cleanup, verify no orphaned references remain:

```bash
grep -r "test_harness\|test-harness" \
  --include="*.h" \
  --include="*.cpp" \
  --include="*.js" \
  --include="*.json" \
  src/ web_src/ test/
```

Should produce zero results.

---

## References

- **Ring Buffer Design**: ISR-safe SPSC queue using volatile indices. See `test_harness_ringbuf.h` for detailed implementation notes.
- **Math Utilities**: IEEE 754-compatible functions in `test_harness_utils.h` (no external dependencies).
- **Web Status Module**: Standalone DOM renderer in `web_src/js/test-harness-status.js` (no build pipeline dependency).
- **Testing**: C++ tests via Unity framework; run via `pio test -e native`.
