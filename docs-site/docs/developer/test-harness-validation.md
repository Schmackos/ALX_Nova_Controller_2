---
title: Test Harness Validation
description: Documentation for test harness validation modules used in pipeline testing.
sidebar_label: Test Harness Validation
---

# Test Harness Validation

The test harness validation modules are lightweight, header-only utilities used during pipeline testing. They provide a ring buffer for streaming byte data between test stages, a small set of numeric helpers for clamping and range mapping, and a standalone JavaScript status card for rendering test results in the browser without any server connection.

---

## Modules

### Ring Buffer (`src/test_harness_ringbuf.h`)

A fixed-capacity, power-of-two ring buffer operating on `uint8_t` values. The implementation is entirely `static inline` — there is no corresponding `.cpp` file. The buffer uses a one-slot-reserved scheme, so the usable capacity is `TEST_HARNESS_RINGBUF_SIZE - 1` bytes. The default size constant is 16, giving 15 usable slots.

#### Struct

```c
typedef struct {
    uint8_t          buf[TEST_HARNESS_RINGBUF_SIZE];
    volatile uint8_t head;
    volatile uint8_t tail;
} TestHarnessRingbuf;
```

`head` and `tail` are `volatile` to allow safe use in simple single-producer / single-consumer scenarios across interrupt and task contexts, though no explicit memory barrier is provided. For multi-core firmware paths, callers are responsible for any additional synchronisation.

#### API

```c
void test_harness_ringbuf_init(TestHarnessRingbuf *buf);
```

Zeroes `head` and `tail`, putting the buffer into an empty state. Does not touch `buf` contents.

---

```c
bool test_harness_ringbuf_push(TestHarnessRingbuf *buf, uint8_t value);
```

Writes `value` at the current `head` position and advances `head`. Returns `true` on success. Returns `false` without modifying the buffer when the buffer is full (i.e., when adding one more byte would make `head == tail`).

---

```c
bool test_harness_ringbuf_pop(TestHarnessRingbuf *buf, uint8_t *value);
```

Reads the byte at `tail` into `*value` and advances `tail`. Returns `true` on success. Returns `false` without modifying `*value` or `tail` when the buffer is empty.

---

```c
uint8_t test_harness_ringbuf_count(const TestHarnessRingbuf *buf);
```

Returns the number of bytes currently held in the buffer. The result is always in the range `[0, TEST_HARNESS_RINGBUF_SIZE - 1]`.

#### Usage Example

```c
TestHarnessRingbuf rb;
test_harness_ringbuf_init(&rb);

test_harness_ringbuf_push(&rb, 0xAB);
test_harness_ringbuf_push(&rb, 0xCD);

uint8_t val;
while (test_harness_ringbuf_pop(&rb, &val)) {
    // process val
}
```

---

### Numeric Utilities (`src/test_harness_utils.h`)

Three `static inline` helpers for float arithmetic commonly needed when constructing or evaluating pipeline test vectors. No state, no side effects.

#### API

```c
float test_harness_clamp(float value, float min, float max);
```

Returns `value` clamped to the closed interval `[min, max]`. If `value < min`, returns `min`. If `value > max`, returns `max`. Otherwise returns `value` unchanged.

---

```c
bool test_harness_is_valid_percentage(float value);
```

Returns `true` when `value` is in the closed interval `[0.0f, 100.0f]`, `false` otherwise. Intended for validating level and threshold parameters that are expressed as percentages.

---

```c
float test_harness_map_range(float value, float in_min, float in_max,
                             float out_min, float out_max);
```

Linearly maps `value` from the input range `[in_min, in_max]` to the output range `[out_min, out_max]`. When `in_min == in_max` (zero-width input range), the function returns `out_min` rather than performing a division by zero.

The mapping formula for the normal case is:

```
out_min + (value - in_min) * (out_max - out_min) / (in_max - in_min)
```

No clamping is applied — `value` outside `[in_min, in_max]` produces a result outside `[out_min, out_max]`.

#### Usage Example

```c
// Map a raw ADC reading (0–4095) to a dB range (-60 to 0).
float db = test_harness_map_range((float)adc_raw, 0.0f, 4095.0f, -60.0f, 0.0f);
float safe_db = test_harness_clamp(db, -60.0f, 0.0f);
```

---

### Status Card (`test-harness-status.js`)

A standalone JavaScript module delivered as an IIFE (immediately-invoked function expression). It has no dependency on the web server, no WebSocket connection, and no integration with the firmware's REST API. It is intended to be loaded directly in a browser and called from a test runner page.

The single public function it exposes is:

```js
renderTestHarnessStatus(containerId, moduleStatuses?)
```

| Parameter | Type | Description |
|---|---|---|
| `containerId` | `string` | The `id` of a DOM element that the status card will be rendered into. |
| `moduleStatuses` | `object` (optional) | A map of module names to status strings. When omitted, the card renders in an empty / default state. |

The function does not return a value. It constructs DOM nodes via `createElement` / `createElementNS` / `appendChild` and replaces the container's children (no `innerHTML`). Because the module is a self-contained IIFE, `renderTestHarnessStatus` is available as a global after the script tag loads — no `import` or `require` call is needed.

```html
<div id="harness-status"></div>
<script src="test-harness-status.js"></script>
<script>
  renderTestHarnessStatus('harness-status', {
    'ring-buffer': 'pass',
    utils:         'pass',
  });
</script>
```

---

## Test Coverage

The C++ modules are covered by native Unity tests under `test/test_harness_ringbuf/` and `test/test_harness_utils/`. These compile with `-D UNIT_TEST -D NATIVE_TEST` and require no hardware.

```bash
# Run both harness validation modules
pio test -e native -f test_harness_ringbuf
pio test -e native -f test_harness_utils

# Run all native tests (includes the above)
pio test -e native
```

Key assertions verified by the ring buffer tests:

- `init` produces an empty buffer (`count == 0`)
- `push` to a full buffer returns `false` and does not modify state
- `pop` from an empty buffer returns `false` and does not write to the output pointer
- `count` tracks the correct occupancy through interleaved push/pop sequences
- Wrap-around at `TEST_HARNESS_RINGBUF_SIZE` boundary is handled correctly

Key assertions verified by the utils tests:

- `clamp` returns the lower bound when value is below range, upper bound when above, and value unchanged when within range
- `is_valid_percentage` returns `true` at exactly 0.0 and 100.0 (closed interval boundary check)
- `map_range` with equal `in_min` and `in_max` returns `out_min` (division-by-zero guard)
- `map_range` with a value at `in_min` returns `out_min`; value at `in_max` returns `out_max`

---

## Cleanup Procedure

The test harness validation artifacts are temporary. To remove them from the working tree after the workflow completes:

```bash
# Confirm files are present before removal
grep -r "test_harness" src/ --include="*.h" -l
grep -r "test_harness" test/ --include="*.cpp" -l

# Remove source headers
rm src/test_harness_ringbuf.h
rm src/test_harness_utils.h

# Remove JS status card
rm web_src/js/test-harness-status.js

# Remove C++ test modules
rm -rf test/test_harness_ringbuf/
rm -rf test/test_harness_utils/

# Remove documentation page
rm docs-site/docs/developer/test-harness-validation.md

# Verify nothing remains
grep -r "test_harness" src/ test/ web_src/ docs-site/ 2>/dev/null
```

If the final `grep` produces no output, the cleanup is complete.
