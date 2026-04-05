---
title: Test Harness Validation
description: Documentation for test harness validation modules used in pipeline testing.
sidebar_label: Test Harness Validation
---

## Overview

The test harness validation modules provide a set of utilities and supporting code for validating the ALX Nova Controller's pipeline under various conditions. These modules enable multi-domain testing of the audio pipeline, DSP processing, HAL device management, and other subsystems. The test harness is designed to be temporary—all artifacts can be cleanly removed when testing is complete.

The test harness consists of:
- **C++ header libraries** for low-level utilities and ISR-safe data structures
- **Unity-based C++ unit tests** covering boundary conditions, edge cases, and overflow scenarios
- **Web UI status components** for displaying test harness state

## Modules

### test_harness_ringbuf.h

An ISR-safe ring buffer implementation using volatile head/tail pointers for lock-free operation in interrupt contexts.

**Purpose:** Provide thread-safe data buffering between ISR contexts and task-level code without requiring mutexes.

**Key Features:**
- Volatile head and tail pointers for atomic updates
- Handles wrap-around automatically
- Safe for push/pop operations from both ISR and task context
- Configurable buffer size at compile time

**API:**

```c
// Initialize (or reset) a ring buffer to the empty state
void test_harness_ringbuf_init(TestHarnessRingbuf *rb);

// Push a uint8_t value onto the buffer (ISR-safe)
// Returns true if successful, false if buffer is full
bool test_harness_ringbuf_push(TestHarnessRingbuf *rb, uint8_t value);

// Pop a value from the buffer into *value (ISR-safe)
// Returns true if successful, false if buffer is empty
bool test_harness_ringbuf_pop(TestHarnessRingbuf *rb, uint8_t *value);

// Get current count of items in buffer
uint8_t test_harness_ringbuf_count(TestHarnessRingbuf *rb);
```

### test_harness_utils.h

Pure utility functions for common validation operations and data transformations.

**Purpose:** Provide reusable, stateless utility functions for test harness operations and pipeline validation.

**API:**

```c
// Clamp a value between min and max
float test_harness_clamp(float value, float min, float max);

// Check if value is a valid percentage (0.0 to 100.0)
bool test_harness_is_valid_percentage(float value);

// Map a value from one range to another (linear interpolation)
// Returns out_min when in_min == in_max (division-by-zero guard)
float test_harness_map_range(float value, float in_min, float in_max, float out_min, float out_max);
```

### test-harness-status.js

Web UI component for displaying test harness validation status and metrics.

**Purpose:** Provide real-time visibility into test harness operation through the web dashboard.

**Features:**
- Self-contained IIFE pattern (no external dependencies)
- Status card display with per-module pass/fail/pending indicators
- MDI inline SVG icons following project conventions
- Standalone — not wired into web server or build pipeline

## Test Coverage

The test harness modules include comprehensive Unity-based tests covering:

### Ring Buffer Tests
- **Initialization** — Buffer starts empty with correct size
- **Push operations** — Single and multiple pushes work correctly
- **Pop operations** — Values retrieved in FIFO order
- **Wrap-around** — Head and tail pointer wrapping at buffer boundaries
- **Overflow detection** — Push to full buffer returns false, doesn't corrupt data
- **Empty detection** — Pop from empty buffer returns false
- **Full detection** — Buffer correctly reports full state
- **Count accuracy** — Item count matches number of pushed items
- **Boundary conditions** — Single-item buffer, power-of-two sizes
- **Edge cases** — Alternating push/pop, multiple wraps around buffer

### Utility Function Tests
- **Clamp function** — Values above max clamped, below min clamped, within range unchanged
- **Percentage validation** — Valid range 0.0-100.0, rejects negative and >100.0
- **Range mapping** — Linear interpolation across ranges, boundary values, inverted ranges
- **Division-by-zero guard** — map_range with equal input bounds returns out_min
- **Extreme values** — Very small/large numbers, zero, negative values

## Cleanup Procedure

When test harness validation is complete, all artifacts can be cleanly removed using the following procedure:

### 1. Find All Test Harness Files

```bash
grep -r "test_harness_\|test-harness-" --include="*.h" --include="*.cpp" --include="*.js" --include="*.md" -l
```

This command will locate all files containing test harness references:
- C++ headers: `src/test_harness_*.h`
- C++ tests: `test/test_harness_*/*.cpp`
- JavaScript modules: `web_src/js/test-harness-*.js`
- Documentation: `docs-site/docs/developer/test-harness-validation.md`

### 2. Remove Test Harness Implementation

```bash
# Remove C++ header libraries
rm src/test_harness_ringbuf.h
rm src/test_harness_utils.h

# Remove C++ unit test directories
rm -rf test/test_harness_ringbuf/
rm -rf test/test_harness_utils/
```

### 3. Remove Web UI Components

```bash
# Remove web UI status component
rm web_src/js/test-harness-status.js
```

### 4. Remove Documentation

```bash
# Remove this documentation file
rm docs-site/docs/developer/test-harness-validation.md
```

### 5. Verify Cleanup

```bash
# Confirm no test harness references remain
grep -r "test_harness_\|test-harness-" --include="*.h" --include="*.cpp" --include="*.js" --include="*.md" .

# This should return no results
```

### 6. Commit Cleanup

After removing all test harness files:

```bash
git add -A
git commit -m "Remove test harness validation artifacts"
```

## Integration Notes

The test harness modules integrate minimally with the existing codebase:

- **No modifications to platformio.ini** — test harness files are optional headers
- **No changes to main() or boot sequence** — test harness is opt-in via includes
- **Isolated test directories** — Under `test/test_harness_*/` following project conventions
- **No impact on production firmware** — Guards ensure test code doesn't ship

When integrated into the build process, the test harness operates under the `-D UNIT_TEST` and `-D NATIVE_TEST` compilation flags, ensuring it only executes in native test environments and never impacts ESP32-P4 firmware builds.
