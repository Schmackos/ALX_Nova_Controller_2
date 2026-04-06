---
title: Test Harness Validation
sidebar_position: 99
description: Documentation for the test harness validation artifacts used in pipeline testing.
---

These are **pipeline validation artifacts** — temporary files injected by the CI workflow to verify that the test harness itself can discover, compile, and execute new modules end-to-end. They are not production firmware code and carry no runtime presence in a release build.

The naming convention (`test_harness_` / `test-harness-`) makes the artifacts trivially identifiable and removable once validation is complete.

## Artifact Inventory

| File | Description |
|---|---|
| `src/test_harness_ringbuf.h` | Minimal lock-free ring buffer for byte-oriented FIFO testing |
| `src/test_harness_utils.h` | Floating-point utility functions (clamp, range map, percentage validation) |
| `web_src/js/test-harness-status.js` | Web UI status card renderer for harness state |
| `test/test_harness_ringbuf/test_harness_ringbuf.cpp` | Unity tests for the ring buffer |
| `test/test_harness_utils/test_harness_utils.cpp` | Unity tests for the utility functions |

## API Reference

### Ring Buffer (`src/test_harness_ringbuf.h`)

A simple power-of-2 ring buffer for single-byte values. Uses the one-slot-wasted convention to distinguish full from empty without an extra flag.

```cpp
struct TestHarnessRingBuf {
    volatile uint16_t head;
    volatile uint16_t tail;
    uint8_t buffer[64];
};
```

Wrap-around is handled by bitwise masking against the power-of-2 capacity, so no modulo is required.

```cpp
// Initialise the ring buffer to an empty state.
void test_harness_ringbuf_init(TestHarnessRingBuf *rb);

// Push a byte onto the buffer.
// Returns true on success, false when the buffer is full.
bool test_harness_ringbuf_push(TestHarnessRingBuf *rb, uint8_t value);

// Pop a byte from the buffer into *out.
// Returns true on success, false when the buffer is empty.
bool test_harness_ringbuf_pop(TestHarnessRingBuf *rb, uint8_t *out);

// Return the number of bytes currently held in the buffer.
uint16_t test_harness_ringbuf_count(const TestHarnessRingBuf *rb);
```

### Utility Functions (`src/test_harness_utils.h`)

Stateless floating-point helpers with no firmware dependencies — safe to link into native tests.

```cpp
// Clamp value to [min_val, max_val].
float test_harness_clamp(float value, float min_val, float max_val);

// Return true when value is in the closed interval [0.0, 100.0].
bool test_harness_is_valid_percentage(float value);

// Linearly map value from [in_min, in_max] to [out_min, out_max].
// Returns out_min when in_min == in_max (degenerate range guard).
float test_harness_map_range(float value,
                             float in_min, float in_max,
                             float out_min, float out_max);
```

### Web Status Card (`web_src/js/test-harness-status.js`)

Client-side helper that renders a status card into an existing DOM container.

```js
// Render a status card for the given statusData into the element
// identified by containerId.
renderTestHarnessStatus(containerId, statusData);
```

`containerId` is a CSS selector string (e.g. `"#harness-status"`). `statusData` is an object whose shape mirrors the WebSocket state broadcast for harness diagnostics.

## Running the Tests

The test modules compile and run on the **native** PlatformIO environment — no hardware required.

```bash
# Run both harness test modules together
pio test -e native -f test_harness_ringbuf -f test_harness_utils

# Run each module individually
pio test -e native -f test_harness_ringbuf
pio test -e native -f test_harness_utils

# Run with verbose output to see individual assertion results
pio test -e native -f test_harness_ringbuf -v
pio test -e native -f test_harness_utils -v
```

These modules integrate with the standard `pio test -e native` run, so they appear in the full test count alongside the production modules.

## Cleanup Procedure

When validation is complete, all artifacts can be identified by their shared prefix and removed in one pass.

**Find all harness artifacts:**

```bash
grep -r "test_harness_\|test-harness-" \
  --include="*.h" \
  --include="*.cpp" \
  --include="*.js" \
  --include="*.md" \
  -l
```

**Files to delete:**

- `src/test_harness_ringbuf.h`
- `src/test_harness_utils.h`
- `web_src/js/test-harness-status.js`
- `test/test_harness_ringbuf/` (entire directory)
- `test/test_harness_utils/` (entire directory)
- `docs-site/docs/developer/test-harness-validation.md` (this file)

After deletion, run `pio test -e native` and the full Playwright suite to confirm no production tests were inadvertently coupled to the harness artifacts.
