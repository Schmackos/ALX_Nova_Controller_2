---
title: API Versioning Policy
sidebar_position: 7
description: Backwards-compatibility guarantees, deprecation timelines, and migration guides for the ALX Nova REST and WebSocket APIs.
---

# API Versioning Policy

This page defines the backwards-compatibility guarantees, deprecation process, and migration guidance for the ALX Nova REST API and WebSocket protocol.

---

## Current State

All REST endpoints are available at both `/api/<path>` and `/api/v1/<path>`. The two paths are identical — they route to the same handler. The versioned path is the canonical form.

The web UI frontend uses `apiFetch()` in `web_src/js/01-core.js`, which automatically rewrites `/api/` to `/api/v1/` for all requests except the internal test endpoint at `/api/__test__/`. External API clients should use `/api/v1/` directly to be explicit about the version they depend on.

The WebSocket server runs on port 81. The initial broadcast includes a `protocolVersion` field. Clients that depend on specific message shapes should check this field and log a warning if the version differs from what the client was written against.

---

## API Stability Tiers

### Stable

Endpoints and message fields marked as Stable are guaranteed to exist for at least 12 months after a deprecation notice is published. Breaking changes (removing a field, changing a field's type, changing an endpoint path) are only permitted on a major version bump and will always be preceded by a deprecation notice.

The following modules are currently Stable:

- `GET /api/v1/system/info` — firmware version, uptime, memory stats
- `GET /api/v1/health` — 9-category health report
- `GET /api/v1/hal/devices` — HAL device list
- `GET /api/v1/hal/faults` — persisted HAL fault log
- `POST /api/v1/hal/rescan` — trigger async HAL discovery
- `GET /api/v1/pipeline/status` — audio pipeline state and timing metrics
- `POST /api/v1/siggen` — signal generator control
- `GET/PUT /api/v1/settings` — device settings export/import
- WebSocket initial broadcast fields: `protocolVersion`, `audio`, `hal`, `pipeline`, `dsp`, `wifi`, `mqtt`

### Experimental

Endpoints marked as Experimental may change in minor version releases. They are typically new features that are still being validated in production. Experimental endpoints are prefixed with a warning note in their API reference page.

Current Experimental endpoints:

- `POST /api/v1/hal/devices/power` — HAL power management (introduced in PR #83, stabilising)
- `GET /api/v1/pipeline/timing` — granular pipeline timing breakdown (introduced in PR #82)
- `POST /api/v1/asrc/config` — ASRC engine configuration (introduced in PR #85, tracking feature)

---

## Deprecation Process

1. **Deprecation notice** is published in the Release Notes blog and added to the relevant API reference page with a `:::caution Deprecated` admonition.
2. The deprecated endpoint or field continues to work for a **minimum 6-month notice period**.
3. Removal only occurs on a **major version bump** (e.g. v1.x → v2.0). Minor and patch releases never remove previously-supported fields.
4. During the notice period, the deprecated endpoint or field returns an additional `_deprecated` boolean field in its response (REST) or a `deprecated: true` marker in the WebSocket broadcast (where applicable).

---

## Migration Reference

The following table lists previously-supported API paths that have been removed, and their replacements.

| Removed Endpoint | Removed In | Replacement |
|---|---|---|
| `GET /api/dac` | v1.19.0 (PR #73) | `GET /api/v1/hal/devices` filtered by capability `HAL_CAP_DAC` |
| `GET /api/dac/drivers` | v1.19.0 (PR #73) | `GET /api/v1/hal/db/presets` |
| WebSocket `dacState` broadcast | v1.19.0 (PR #73) | `hal.devices[n]` in the standard HAL broadcast |
| `POST /api/hal/eeprom/read` | — | `POST /api/v1/hal/eeprom/read` (path normalised, same handler) |

---

## WebSocket Protocol Versioning

The initial WebSocket broadcast (sent immediately after a client authenticates on port 81) includes:

```json
{
  "type": "state",
  "protocolVersion": 2,
  "audio": { ... },
  "hal": { ... },
  ...
}
```

The `protocolVersion` integer increments when the shape of the broadcast changes in a non-additive way (field removed, type changed). Additive changes (new fields added) do not increment the version.

**Recommended client behaviour:** On connection, read `protocolVersion` and compare against the version your client was written for. If the version is higher than expected, log a warning and continue — new fields will be present but existing fields are unchanged. If the version is lower than expected, some fields your client depends on may be absent.

Current protocol version: **2** (as of PR #80).

---

## Future: `/api/v2/` Path

When a major breaking change is needed, a `/api/v2/` path prefix will be introduced alongside the existing `/api/v1/` paths. Both will be served simultaneously during a transition period. The `apiFetch()` auto-rewrite in the web UI will be updated to use `v2` on the same major firmware version bump.
