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

Returns the latest health check results for all categories. If the immediate phase has run but the deferred phase has not yet completed, deferred-phase fields are omitted from the response.

## Authentication

Requires a valid session cookie (obtained via `POST /api/auth/login`) or a WebSocket token header. Same authentication as all other `/api/` endpoints.

## Request

No request body or query parameters.

```bash
curl -b "session=<cookie>" http://<device-ip>/api/health
```

## Response Schema

HTTP 200 on success. The response is a JSON object with a top-level `phase` field indicating which phases have completed, and nested objects for each check category.

```json
{
  "phase": "deferred",
  "timestamp": 42318,
  "system": {
    "heap": {
      "pass": true,
      "detail": "maxBlock=82KB freeHeap=210KB"
    },
    "psram": {
      "pass": true,
      "detail": "free=3842KB total=4096KB"
    },
    "dma": {
      "pass": true,
      "detail": "16 buffers x 2KB allocated from internal SRAM"
    }
  },
  "storage": {
    "littlefs": {
      "pass": true,
      "detail": "mounted OK, used=124KB total=1024KB"
    },
    "config": {
      "pass": true,
      "detail": "/config.json present, 3842 bytes"
    }
  },
  "tasks": {
    "created": {
      "pass": true,
      "detail": "audio=OK gui=OK mqtt=OK"
    },
    "stackLow": {
      "pass": true,
      "detail": "min watermark 1024B (audio_pipeline_task)"
    }
  },
  "network": {
    "wifi": {
      "pass": true,
      "detail": "connected SSID=MyNetwork IP=192.168.1.100"
    },
    "http": {
      "pass": true,
      "detail": "self-probe port 80 OK 12ms"
    },
    "websocket": {
      "pass": true,
      "detail": "port 81 bound OK"
    }
  },
  "mqtt": {
    "broker": {
      "pass": false,
      "detail": "connection refused 192.168.1.50:1883 (rc=-2)"
    }
  },
  "hal": {
    "available": {
      "pass": true,
      "detail": "14 devices AVAILABLE, 0 ERROR"
    },
    "expansion": {
      "pass": true,
      "detail": "Bus2 scan found 1 EEPROM (ES9038Q2M)"
    }
  },
  "i2s": {
    "port0": {
      "pass": true,
      "detail": "STD RX 48kHz master BCK=20 WS=21 DIN=23 MCLK=22"
    },
    "port1": {
      "pass": true,
      "detail": "STD RX 48kHz follower DIN=25"
    },
    "port2": {
      "pass": false,
      "detail": "not configured (no expansion DAC present)"
    }
  },
  "audio": {
    "pipeline": {
      "pass": true,
      "detail": "8 lanes active, matrix 32x32 OK"
    },
    "adcHealth": {
      "pass": true,
      "detail": "ADC1=OK ADC2=OK (NO_DATA)"
    }
  }
}
```

### Field Reference

| Field | Type | Description |
|---|---|---|
| `phase` | `string` | `"immediate"` if only the first phase has run; `"deferred"` when both phases are complete |
| `timestamp` | `number` | Milliseconds since boot when the last phase completed |
| `*.*.pass` | `boolean` | `true` if the check passed |
| `*.*.detail` | `string` | Human-readable description of the result, up to 64 characters |

### Category Presence

The `mqtt` category is only present in the response when MQTT is enabled in the device configuration. The `i2s.port2` sub-field is present regardless of whether an expansion device is connected — a missing expansion DAC is reported as `pass: false` with an explanatory detail string, not as an absent field.

## Status Codes

| Code | Meaning |
|---|---|
| 200 | Success — response body contains health results |
| 401 | Not authenticated — no valid session cookie |
| 503 | Health check module not yet initialised (immediate phase has not run) |

A 503 response will only occur in the first few hundred milliseconds after boot. The pytest harness retries with exponential backoff for up to 30 seconds before failing the test.

## Example: Checking a Specific Category

The harness typically fetches the full response once and asserts individual fields:

```python
import requests

def test_hal_all_available(device_ip, session_cookie):
    resp = requests.get(
        f"http://{device_ip}/api/health",
        cookies={"session": session_cookie},
        timeout=10,
    )
    assert resp.status_code == 200
    data = resp.json()
    assert data["phase"] == "deferred", "deferred phase not yet complete"
    assert data["hal"]["available"]["pass"] is True, data["hal"]["available"]["detail"]
```

## WebSocket Broadcast

The same data is also pushed over the WebSocket connection as a `healthCheck` message whenever the health state changes (after each phase completes, and after any `diag_emit()` that changes a check result):

```json
{
  "type": "healthCheck",
  "phase": "deferred",
  "timestamp": 42318,
  "system": { ... },
  "network": { ... },
  "hal": { ... }
}
```

The web UI Health Dashboard subscribes to this message type and updates in real time without polling.
