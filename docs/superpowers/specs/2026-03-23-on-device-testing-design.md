# On-Device Testing Framework — Design Spec

**Date:** 2026-03-23
**Status:** Draft
**Approach:** pytest-embedded + Firmware Health Check Module

## Problem

The ALX Nova Controller 2 has 3050 native C++ unit tests and 302 Playwright E2E tests — excellent coverage for logic and UI. But there is **zero on-device testing**. The gap between "tests pass on host" and "firmware works on ESP32-P4 hardware" is entirely manual. Regressions in boot sequences, peripheral initialization, I2C/I2S timing, network connectivity, and memory health are only caught when a developer manually uploads firmware and observes serial output.

## Solution Overview

A two-layer on-device testing framework:

1. **Firmware-side:** A `health_check` module that runs structured self-diagnostics and outputs parseable JSON results over serial and REST API
2. **Host-side:** A Python test harness using `pytest-embedded` (Espressif's official framework) that flashes firmware, captures serial output, runs network-level smoke tests, and auto-creates GitHub issues on failure

## Architecture

```
┌─────────────────────────────────────────────────────┐
│  Host Machine (Windows PC / CI Runner)              │
│                                                     │
│  pytest-embedded                                    │
│  ├── Flash firmware binary via esptool              │
│  ├── Open serial (COM8, 115200)                     │
│  ├── Parse [HEALTH] JSON lines                      │
│  ├── Run HTTP/WS/MQTT smoke tests                   │
│  ├── Save serial logs to device_tests/logs/         │
│  └── Auto-create GitHub issues on failure           │
│                                                     │
│         USB Serial (COM8)    HTTP/WS (:80/:81)      │
│              │                      │               │
└──────────────┼──────────────────────┼───────────────┘
               │                      │
┌──────────────┼──────────────────────┼───────────────┐
│  ESP32-P4 (Waveshare Dev Kit)       │               │
│              │                      │               │
│  health_check module                │               │
│  ├── System checks (heap, PSRAM)    │               │
│  ├── I2C bus scans                  │               │
│  ├── HAL device state verification  │               │
│  ├── I2S port status                │               │
│  ├── Network status                 │               │
│  ├── FreeRTOS task health           │               │
│  ├── Storage verification           │               │
│  └── Audio pipeline status          │               │
│                                     │               │
│  GET /api/health ─────────────────────              │
│  Serial [HEALTH] JSON ──────────────                │
└─────────────────────────────────────────────────────┘
```

---

## Layer 1: Firmware Health Check Module

### Files
- `src/health_check.h` — Public API
- `src/health_check.cpp` — Implementation
- Build flag: `-D HEALTH_CHECK_ENABLED` (enabled in firmware, disabled in native tests)

### Trigger Modes

| Mode | Trigger | Use Case |
|------|---------|----------|
| Boot | Runs after `setup()` completes | Automated CI: flash → wait for results |
| Serial command | Send `HEALTH_CHECK\n` over serial | Manual re-run without reboot |
| REST endpoint | `GET /api/health` | Host-side network tests |

### Check Categories

#### 1. System Health
| Check | Pass Condition | Source |
|-------|---------------|--------|
| `heap_free` | Free internal heap >= 40KB | `heap_caps_get_largest_free_block()` |
| `psram_free` | Free PSRAM >= 1MB | `heap_caps_get_free_size(MALLOC_CAP_SPIRAM)` |
| `psram_available` | PSRAM detected and usable | `heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0` |
| `boot_loop` | Boot count < 3 (no crash loop) | NVS boot counter |
| `dma_buffers` | All DMA buffers allocated | `appState.audio.dmaAllocFailed == false` |

#### 2. I2C Bus Health
| Check | Pass Condition | Source |
|-------|---------------|--------|
| `bus1_scan` | Bus 1 (onboard) responds, ES8311 at 0x18 | `Wire1.beginTransmission()` scan |
| `bus2_scan` | Bus 2 (expansion) responds (devices optional) | `Wire2.beginTransmission()` scan |
| `bus0_status` | Reports skipped (WiFi active) or scan result | `hal_wifi_sdio_active()` |

#### 3. HAL Devices
| Check | Pass Condition | Source |
|-------|---------------|--------|
| `<compatible>` | Device state == AVAILABLE (or MANUAL for optional) | `HalDeviceManager::getDevice()` |
| Expected onboard: PCM5102A, ES8311, PCM1808 x2, NS4150B, TempSensor, SigGen, LED, Relay, Buzzer, Button, Encoder | All reach AVAILABLE within 10s of boot | State callback or poll |
| Expansion devices | Reported as discovered (not required to pass) | Discovery results |

#### 4. I2S Ports
| Check | Pass Condition | Source |
|-------|---------------|--------|
| `port0_status` | Enabled, mode/direction correct | `i2s_port_get_info(0)` |
| `port1_status` | Enabled, mode/direction correct | `i2s_port_get_info(1)` |
| `port2_status` | Reports configured state (may be unused) | `i2s_port_get_info(2)` |

#### 5. Network
| Check | Pass Condition | Source |
|-------|---------------|--------|
| `wifi_status` | Connected to AP or AP mode active | `WiFi.status()` |
| `http_server` | Listening on port 80 | Internal: bound check |
| `ws_server` | Listening on port 81 | Internal: bound check |
| `ip_address` | Valid IP assigned | `WiFi.localIP()` |

#### 6. MQTT (conditional — only if broker configured)
| Check | Pass Condition | Source |
|-------|---------------|--------|
| `mqtt_connected` | Connected to configured broker | `mqttClient.connected()` |
| `mqtt_broker` | Broker address configured | `appState.mqtt.broker` non-empty |

#### 7. FreeRTOS Tasks
| Check | Pass Condition | Source |
|-------|---------------|--------|
| `task_<name>` | Task exists, stack watermark > 512 bytes | `uxTaskGetSystemState()` |
| Expected tasks: loopTask, audio_pipeline_task, gui_task, mqtt_task | All present and healthy | Task enumeration |

#### 8. Storage
| Check | Pass Condition | Source |
|-------|---------------|--------|
| `littlefs_mounted` | LittleFS mounted | `LittleFS.begin()` or exists check |
| `config_readable` | `/config.json` opens for read | File open |
| `nvs_accessible` | Preferences namespace opens | `Preferences.begin()` |

#### 9. Audio Pipeline
| Check | Pass Condition | Source |
|-------|---------------|--------|
| `pipeline_task` | Audio pipeline task running | Task state check |
| `audio_paused` | `audioPaused == false` (not stuck) | `appState.audio.paused` |
| `adc_health` | At least one ADC reports OK/NO_DATA (not ERROR) | `appState.audio.adcHealth[]` |

### Serial Output Format

Health check output follows the established `LOG_*` macro convention with `[Health]` module prefix — no separate format. This ensures compatibility with Debug Console module filtering, WebSocket log forwarding, and log level control.

- **PASS** checks use `LOG_I` (info level)
- **FAIL** checks use `LOG_W` (warning level) — also emit `diag_emit()` with appropriate `DiagErrorCode` and `DiagSeverity`
- **Summary** uses `LOG_I`

```
[I][Health] heap_free: PASS (85432 >= 40000)
[I][Health] psram_free: PASS (3145728 >= 1048576)
[I][Health] bus1_scan: PASS (devices: 0x18)
[W][Health] es8311: FAIL (state=ERROR, error=I2C NAK, slot=1)
[I][Health] wifi_status: PASS (mode=STA, ip=192.168.1.42)
[I][Health] audio_pipeline_task: PASS (watermark=2048)
[I][Health] Summary: 21/22 PASS, 1 FAIL (3200ms)
```

Each FAIL also creates a `DiagEvent` via `diag_emit()` — automatically appearing in the diagnostic journal, WebSocket broadcasts, REST `/api/diagnostics`, and LittleFS persistence. Severity mapping: recoverable failure = `DIAG_SEV_WARN`, hardware fault = `DIAG_SEV_ERROR`.

### REST Endpoint

`GET /api/health` returns the same data as a JSON object:
```json
{
  "firmware": "1.12.3",
  "uptime_ms": 15000,
  "checks": [
    {"cat": "system", "check": "heap_free", "result": "PASS", "value": 85432, "threshold": 40000},
    ...
  ],
  "summary": {"total": 22, "pass": 21, "fail": 1, "skip": 0, "duration_ms": 3200}
}
```

### Integration with Existing Systems
- Serial output uses `LOG_I`/`LOG_W`/`LOG_E` macros with `[Health]` module prefix — follows the same convention as all other modules
- Each FAIL check calls `diag_emit()` with the appropriate `DiagErrorCode` from the new `0x08xx` health check subsystem in `diag_error_codes.h`
- Health check results appear in the diagnostic journal, WebSocket broadcasts, and Debug Console (filterable by `[Health]` module chip)
- REST `/api/health` provides structured JSON for automated test harness parsing (distinct from serial output and `/api/diag/snapshot`)
- Boot health check runs in two phases: immediate (system/storage/tasks) at end of `setup()`, deferred (network/MQTT/HAL) 15s later via one-shot timer

---

## Layer 2: Host-Side Python Test Harness

### Directory Structure
```
device_tests/
├── conftest.py              # pytest-embedded config, fixtures
├── pytest.ini               # markers, timeouts, serial config
├── requirements.txt         # pytest, pytest-embedded, requests, websocket-client
├── utils/
│   ├── health_parser.py     # Parses [HEALTH] JSON lines from serial
│   ├── issue_creator.py     # Creates GitHub issues via `gh` CLI
│   └── serial_monitor.py    # Extended serial capture with log saving
├── tests/
│   ├── test_boot_health.py  # Flash → boot → parse health checks
│   ├── test_network.py      # HTTP smoke, WebSocket, API endpoints
│   ├── test_mqtt.py         # MQTT connectivity (if broker configured)
│   ├── test_settings.py     # Config write → reboot → verify persistence
│   ├── test_hal_devices.py  # HAL device states via REST + serial
│   └── test_audio.py        # I2S ports, pipeline, DMA via REST
└── logs/                    # Serial captures (gitignored)
```

### Dependencies (`requirements.txt`)
```
pytest>=8.0
pytest-embedded>=1.12
pytest-embedded-serial-esp>=1.12
pytest-embedded-idf>=1.12
requests>=2.31
websocket-client>=1.7
```

### Configuration (`conftest.py`)

```python
import pytest

def pytest_addoption(parser):
    parser.addoption("--port", default="COM8", help="Serial port for ESP32-P4")
    parser.addoption("--skip-flash", action="store_true", help="Skip firmware flashing")
    parser.addoption("--firmware-bin", default=None, help="Path to firmware binary")
    parser.addoption("--create-issues", action="store_true", help="Auto-create GitHub issues on failure")
    parser.addoption("--device-ip", default=None, help="Device IP (auto-detected from serial if omitted)")

@pytest.fixture
def dut(request):
    """Device Under Test — serial connection to ESP32-P4"""
    # pytest-embedded provides this; configured via pytest.ini
    ...

@pytest.fixture
def device_ip(dut):
    """Extract device IP from health check output"""
    # Parse [HEALTH] network check for IP address
    ...
```

### Test Modules

#### `test_boot_health.py` — Core Health Verification
- Flash firmware (unless `--skip-flash`)
- Wait for `[HEALTH] SUMMARY` line (timeout: 60s)
- Parse all `[HEALTH]` JSON lines
- Assert each check result == "PASS" (or "SKIP" for conditional checks)
- On failure: capture full serial log, emit pytest failure with details

#### `test_network.py` — Network Smoke Tests
- `GET /api/health` returns 200 with valid JSON
- `GET /api/hal/devices` returns device array
- `GET /api/diag/snapshot` returns diagnostics
- WebSocket connects to `:81`, receives initial state broadcast within 5s
- `GET /api/i2s/ports` returns port configuration

#### `test_mqtt.py` — MQTT Connectivity (conditional)
- Skip if MQTT broker not configured in device settings
- Verify `mqttConnected == true` in health check output
- Verify MQTT state in WebSocket broadcast

#### `test_settings.py` — Persistence Across Reboot
- Read current settings via `GET /api/settings`
- Write a known test value via REST API
- Trigger device reboot (via serial command or REST)
- Wait for boot health check
- Read settings back, verify test value persisted
- Restore original settings

#### `test_hal_devices.py` — HAL Device Validation
- Query `GET /api/hal/devices`
- Verify expected onboard devices present and AVAILABLE
- Check no devices stuck in ERROR state
- Verify device capabilities match expected flags
- Check `_lastError` is empty for all healthy devices

#### `test_audio.py` — Audio Pipeline Health
- Query `GET /api/i2s/ports` — verify port 0 and 1 enabled as RX
- Query `GET /api/health` — verify audio pipeline checks pass
- Verify DMA buffers allocated (no `dmaAllocFailed`)
- Verify `audioPaused == false`

### Failure Handling & GitHub Issue Creation

**`issue_creator.py`:**
```python
def create_issue(test_name, failure_details, serial_log_excerpt, device_state):
    """Create GitHub issue via gh CLI on test failure"""
    title = f"[Device Test] {test_name} FAIL — {failure_details['summary']}"
    body = f"""## Device Test Failure

**Test:** `{test_name}`
**Firmware:** {device_state.get('firmware', 'unknown')}
**Timestamp:** {datetime.now().isoformat()}

### Failed Checks
{format_failed_checks(failure_details['checks'])}

### Device State
- Heap free: {device_state.get('heap_free', 'N/A')} bytes
- PSRAM free: {device_state.get('psram_free', 'N/A')} bytes
- Uptime: {device_state.get('uptime_ms', 'N/A')} ms

### Serial Log (last 50 lines)
```
{serial_log_excerpt}
```

### Reproduction
1. Flash firmware version {device_state.get('firmware', 'unknown')}
2. Run: `cd device_tests && pytest tests/{test_file} --port COM8`

---
*Auto-created by device test harness*
"""
    # Deduplicate: search for open issue with same title prefix
    # Only create if no matching open issue exists
    subprocess.run(["gh", "issue", "create",
                     "--title", title,
                     "--body", body,
                     "--label", "device-test,bug"])
```

**Deduplication:** Before creating an issue, searches for open issues with `[Device Test] <test_name>` in the title. Skips creation if a matching open issue exists (adds a comment instead).

**Log capture:** All serial output is saved to `device_tests/logs/<timestamp>_<test_name>.log` regardless of pass/fail.

### Running Tests

```bash
# Install dependencies (first time)
cd device_tests && pip install -r requirements.txt

# Run all device tests (flash + test)
pytest --port COM8

# Skip flash (firmware already uploaded)
pytest --port COM8 --skip-flash

# Run specific test
pytest tests/test_boot_health.py --port COM8

# Enable GitHub issue creation
pytest --port COM8 --create-issues

# CI mode (firmware binary from build artifact)
pytest --port %CI_ESP32_PORT% --firmware-bin ../firmware.bin --create-issues
```

---

## Layer 3: CI Integration

### Self-Hosted Runner Setup (One-Time)

1. Install GitHub Actions runner on Windows PC
2. Add label `has-esp32p4`
3. Set environment variable `CI_ESP32_PORT=COM8`
4. Install Python + pip, run `pip install -r device_tests/requirements.txt`

### Workflow: `.github/workflows/device-tests.yml`

```yaml
name: Device Tests

on:
  workflow_dispatch:           # Manual trigger from GitHub UI
  push:
    branches: [main, develop]
    # Only when commit message contains [device-test]
  pull_request:
    types: [labeled]           # Trigger when 'device-test' label added

jobs:
  device-tests:
    runs-on: [self-hosted, has-esp32p4]
    needs: [build]             # From tests.yml reusable workflow
    if: >
      github.event_name == 'workflow_dispatch' ||
      contains(github.event.head_commit.message, '[device-test]') ||
      contains(github.event.pull_request.labels.*.name, 'device-test')
    timeout-minutes: 10

    steps:
      - uses: actions/checkout@v4

      - name: Download firmware
        uses: actions/download-artifact@v4
        with:
          name: firmware
          path: .pio/build/esp32-p4/

      - name: Install test dependencies
        run: pip install -r device_tests/requirements.txt

      - name: Run device tests
        run: |
          cd device_tests
          pytest --port $CI_ESP32_PORT \
                 --firmware-bin ../.pio/build/esp32-p4/firmware.bin \
                 --create-issues \
                 --junitxml=results.xml \
                 -v

      - name: Upload test results
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: device-test-results
          path: |
            device_tests/results.xml
            device_tests/logs/
          retention-days: 14

      - name: Upload serial logs
        if: failure()
        uses: actions/upload-artifact@v4
        with:
          name: device-serial-logs
          path: device_tests/logs/
          retention-days: 30
```

### Trigger Matrix

| Trigger | When | Use Case |
|---------|------|----------|
| `workflow_dispatch` | Manual button in GitHub Actions UI | On-demand validation |
| `[device-test]` in commit | Push to main/develop | Developer opts in per commit |
| `device-test` PR label | Label added to PR | Reviewer requests hardware validation |

### Non-Blocking Design
- Device tests are a **separate workflow**, not a required check
- The 5 existing quality gates (cpp-tests, cpp-lint, js-lint, e2e-tests, doc-coverage) remain the merge requirement
- Device test results appear as a status check but don't block merges
- When the runner PC is off, jobs queue and eventually timeout — no impact

---

## Verification Plan

### Manual Verification
1. Build firmware: `pio run`
2. Upload to device: `pio run --target upload`
3. Monitor serial: verify `[HEALTH]` lines appear after boot
4. Run pytest: `cd device_tests && pytest --port COM8 --skip-flash -v`
5. Verify all checks pass
6. Simulate failure: disconnect I2C bus → verify FAIL reported
7. Verify `GET /api/health` returns JSON with same results
8. Test `--create-issues` flag: verify GitHub issue created with correct format

### CI Verification
1. Install self-hosted runner, verify it appears in GitHub Settings > Actions > Runners
2. Trigger `workflow_dispatch` manually
3. Verify firmware download, flash, serial capture, pytest results
4. Verify artifacts uploaded (results.xml, serial logs)
5. Verify GitHub issue created on simulated failure

---

## Success Criteria
- Boot health check completes in < 5 seconds
- All 9 check categories produce structured output
- pytest harness can flash, boot, and validate in < 3 minutes total
- GitHub issues auto-created with actionable content on failure
- Serial logs captured and preserved for debugging
- CI workflow runs end-to-end on self-hosted runner
- Zero impact on existing 5 quality gates

## Out of Scope (Future)
- Expansion mezzanine module testing (base board only for now)
- Audio signal injection / loopback measurement
- Power consumption profiling
- QEMU emulation (ESP32-P4 not yet supported)
- Dedicated Raspberry Pi test runner (design supports easy migration)
