---
title: Test Harness Validation
description: Documentation for test harness validation artifacts used in pipeline testing.
---

# Test Harness Validation

This page documents the test harness validation modules — a set of self-contained test artifacts used for validating the native harness pipeline in the ALX Nova Controller 2 project.

## Overview

The test harness validation concept provides a multi-domain set of lightweight, standalone modules designed to validate core infrastructure patterns without requiring hardware. These artifacts are purely for validation purposes and are not part of the production firmware.

Key characteristics:
- **Self-contained**: Each module is independent with no inter-dependencies
- **Native-testable**: All C++ modules compile and run on the native platform
- **No production impact**: Zero modifications to existing project files
- **Prefix-based organization**: All artifacts use `test_harness_` or `test-harness-` naming for easy identification and cleanup

## Modules

### C++ Modules

#### test_harness_ringbuf.h

ISR-safe ring buffer implementation with volatile-qualified head/tail indices for safe concurrent access from interrupt contexts.

**Location**: `src/test_harness_ringbuf.h`

**Functions**:
- `test_harness_ringbuf_init(ring_buf_t *rb, uint8_t *buffer, uint32_t size)` — Initialize ring buffer with provided storage
- `test_harness_ringbuf_push(ring_buf_t *rb, uint8_t byte)` — Push single byte (safe from ISR)
- `test_harness_ringbuf_pop(ring_buf_t *rb, uint8_t *byte)` — Pop single byte (safe from ISR)
- `test_harness_ringbuf_count(ring_buf_t *rb)` — Get current element count

**Key Design**:
- Volatile-qualified `head` and `tail` indices prevent compiler optimizations that could break concurrent access
- Atomic read/write patterns for ISR safety without spinlocks
- Circular wrapping for FIFO semantics in fixed-size buffer

#### test_harness_utils.h

Pure utility functions for common validation operations.

**Location**: `src/test_harness_utils.h`

**Functions**:
- `test_harness_clamp(float value, float min, float max)` — Constrain value to [min, max] range
- `test_harness_is_valid_percentage(float value)` — Validate percentage range [0.0, 100.0]
- `test_harness_map_range(float value, float in_min, float in_max, float out_min, float out_max)` — Linear range mapping

**Use cases**:
- DSP parameter validation (gains, levels, percentages)
- Numeric bounds checking for audio configuration
- Signal value transformation and scaling

### JavaScript Modules

#### test-harness-status.js

Standalone web status card module for displaying validation metrics in the web UI.

**Location**: `web_src/js/test-harness-status.js`

**Features**:
- Self-contained status card rendering
- No external dependencies beyond base DOM APIs
- Compatible with existing web UI styling
- ESLint compliant

## Testing

### Running C++ Tests

Run individual test harness modules using the native test environment:

```bash
# Test ring buffer implementation
pio test -e native -f test_harness_ringbuf

# Test utility functions
pio test -e native -f test_harness_utils

# Run all test harness modules
pio test -e native -f test_harness
```

### Running JavaScript Validation

Validate the JavaScript module with ESLint:

```bash
cd web_src
npx eslint js/test-harness-status.js --config .eslintrc.json
```

## Cleanup Procedure

All test harness artifacts use consistent `test_harness_` (C++) or `test-harness-` (JavaScript) prefixes for easy identification and cleanup.

### Find All Artifacts

Locate all test harness files in the repository:

```bash
grep -rl "test_harness_\|test-harness-" src/ test/ web_src/ docs-site/docs/
```

### Remove All Artifacts

```bash
# Remove C++ headers
rm src/test_harness_ringbuf.h
rm src/test_harness_utils.h

# Remove C++ test modules
rm -rf test/test_harness_ringbuf
rm -rf test/test_harness_utils

# Remove JavaScript modules
rm web_src/js/test-harness-status.js

# Remove this documentation
rm docs-site/docs/developer/test-harness-validation.md
```

### Verify Cleanup

After removal, confirm no references remain:

```bash
grep -r "test_harness_\|test-harness-" src/ test/ web_src/ docs-site/ || echo "All test harness artifacts removed"
```

## Purpose

This validation exercise validates core infrastructure patterns:

- **Ring buffer safety**: Volatile semantics and ISR-safe concurrent access
- **Utility robustness**: Edge cases and numeric precision in common operations
- **JavaScript integration**: UI module architecture and ESLint compliance

**Important**: These modules are validation artifacts only. No existing project files were modified during their creation, and they do not affect production firmware builds or deployment.
