---
title: Health Check API
sidebar_position: 6
description: REST API for on-device health check results.
---

The health check API exposes the results of the two-phase boot health check in JSON format. It is the primary interface used by the pytest device test harness and the web UI Health Dashboard.

## Endpoint

```
GET /api/health
```

Returns the latest health check results as a flat array of per-check items. If the immediate phase has run but the deferred phase has not yet completed, `deferred` is `false` and only the immediate-phase checks are present.

## Authentication

Requires a valid session cookie (obtained via `POST /api/auth/login`) or a WebSocket token header. Same authentication as all other `/api/` endpoints.

## Request

No request body or query parameters.

```bash
curl -b "session=<cookie>" http://<device-ip>/api/health
```

## Response Schema

HTTP 200 on success. The response is a JSON object with summary counters and a flat `checks[]` array — one entry per health check.

```json
{
  "type": "healthCheck",
  "timestamp": 45000,
  "durationMs": 84,
  "deferred": true,
  "total": 16,
  "passCount": 6,
  "warnCount": 2,
  "failCount": 0,
  "skipCount": 8,
  "verdict": "pass",
  "checks": [
    {"id": 0, "name": "heap_free",      "status": "pass", "detail": "131KB"},
    {"id": 1, "name": "psram",          "status": "pass", "detail": "32530KB free"},
    {"id": 2, "name": "dma_alloc",      "status": "pass", "detail": ""},
    {"id": 3, "name": "storage",        "status": "pass", "detail": "68KB/8064KB used"},
    {"id": 4, "name": "i2c_bus0_ext",   "status": "skip", "detail": "no devices"},
    {"id": 5, "name": "hal_summary",    "status": "pass", "detail": "8/11 AVAILABLE"},
    {"id": 6, "name": "i2s_ports",      "status": "warn", "detail": "no ports active"},
    {"id": 7, "name": "network",        "status": "warn", "detail": "AP mode"},
    {"id": 8, "name": "mqtt",           "status": "skip", "detail": "not configured"},
    {"id": 9, "name": "audio_pipeline", "status": "pass", "detail": "1 in, 0 out"}
  ]
}
```

### Field Reference

| Field | Type | Description |
|-------|------|-------------|
| `type` | string | Always `"healthCheck"` |
| `timestamp` | number | `millis()` when the check ran |
| `durationMs` | number | Check execution time in ms |
| `deferred` | boolean | `true` after the 30 s deferred phase completes |
| `total` | number | Total check count |
| `passCount` | number | Checks that passed |
| `warnCount` | number | Checks with warnings |
| `failCount` | number | Checks that failed |
| `skipCount` | number | Checks skipped (feature not configured or not applicable) |
| `verdict` | string | `"pass"`, `"warn"`, or `"fail"` — worst status across all non-skip checks |
| `checks` | array | Per-check results, one entry per check |
| `checks[].id` | number | Check index (0-based, stable across boots) |
| `checks[].name` | string | Check identifier (e.g., `"heap_free"`, `"hal_summary"`) |
| `checks[].status` | string | `"pass"`, `"warn"`, `"fail"`, or `"skip"` |
| `checks[].detail` | string | Human-readable detail string, up to 64 characters |

### Verdict Rules

- `"fail"` — at least one check has `status: "fail"`
- `"warn"` — no failures but at least one check has `status: "warn"`
- `"pass"` — all checks are `"pass"` or `"skip"`

`"skip"` entries never influence the verdict. The MQTT check, for example, is always `"skip"` when MQTT is not configured, and does not degrade the verdict.

## Status Codes

| Code | Meaning |
|------|---------|
| 200 | Success — response body contains health results |
| 401 | Not authenticated — no valid session cookie |
| 503 | Health check module not yet initialised (immediate phase has not run) |

A 503 response will only occur in the first few hundred milliseconds after boot. The pytest harness retries with exponential backoff for up to 30 seconds before failing the test.

## Example: Checking a Specific Check by Name

The harness typically fetches the full response once and looks up individual items by `name`:

```python
import requests

def test_hal_summary_pass(device_ip, session_cookie):
    resp = requests.get(
        f"http://{device_ip}/api/health",
        cookies={"session": session_cookie},
        timeout=10,
    )
    assert resp.status_code == 200
    data = resp.json()
    assert data["deferred"] is True, "deferred phase not yet complete"

    check = next(c for c in data["checks"] if c["name"] == "hal_summary")
    assert check["status"] == "pass", check["detail"]
```

## WebSocket Broadcast

The same data is pushed over the WebSocket connection as a `healthCheck` message whenever the health state changes (after each phase completes, and after any `diag_emit()` that changes a check result). The message has the same structure as the REST response:

```json
{
  "type": "healthCheck",
  "timestamp": 45000,
  "durationMs": 84,
  "deferred": true,
  "total": 16,
  "passCount": 6,
  "warnCount": 2,
  "failCount": 0,
  "skipCount": 8,
  "verdict": "pass",
  "checks": [ ... ]
}
```

The web UI Health Dashboard subscribes to this message type and updates in real time without polling.
