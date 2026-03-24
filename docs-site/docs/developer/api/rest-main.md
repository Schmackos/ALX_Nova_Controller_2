---
title: REST API — Main
sidebar_position: 1
description: Core REST API endpoints for settings, auth, OTA, and system management.
---

# REST API — Main

The ALX Nova Controller exposes an HTTP REST API on port 80. All protected endpoints require a valid session cookie obtained through `POST /api/auth/login`. The web dashboard, mobile clients, and automation scripts all use this same API surface.

Real-time state updates are delivered over the WebSocket server on port 81. See the [WebSocket Protocol](../websocket.md) reference for commands and broadcast message types.

:::note Authentication requirement
Every endpoint marked **Protected** returns HTTP 302 to `/login` when no valid session cookie is present. API clients should detect the redirect and re-authenticate. The session cookie uses the `HttpOnly` flag and cannot be read by client-side JavaScript.
:::

---

## HTTP Security Headers

All REST API responses include two security headers, applied automatically via the `server_send()` wrapper in `src/http_security.h`:

- **X-Frame-Options: DENY** -- prevents clickjacking by disallowing iframe embedding
- **X-Content-Type-Options: nosniff** -- prevents MIME-type sniffing attacks

New endpoints MUST use `server_send()` instead of `server.send()` to ensure headers are applied. The `sendGzipped()` function in `web_pages.h` applies headers independently for compressed HTML responses.

---

## Endpoint Modules

REST endpoints are registered across several focused modules rather than a single monolithic file:

| Module | File | Registration Function |
|--------|------|----------------------|
| Main endpoints | `src/main.cpp` | Inline in `setup()` |
| HAL devices | `src/hal/hal_api.cpp` | `registerHalApiEndpoints()` |
| DSP configuration | `src/dsp_api.cpp` | `registerDspApiEndpoints()` |
| Audio pipeline | `src/pipeline_api.cpp` | `registerPipelineApiEndpoints()` |
| DAC state | `src/dac_api.cpp` | `registerDacApiEndpoints()` |
| PSRAM status | `src/psram_api.cpp` | `registerPsramApiEndpoints()` |
| Diagnostics | `src/diag_api.cpp` | `registerDiagApiEndpoints()` |
| Signal generator | `src/siggen_api.cpp` | `registerSignalGenApiEndpoints()` |

---

## Endpoint Reference

### Authentication

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/login` | None | Serve the login page HTML |
| POST | `/api/auth/login` | None | Authenticate with password and receive session cookie |
| POST | `/api/auth/logout` | None | Invalidate the current session |
| GET | `/api/auth/status` | None | Query whether the current session is valid |
| POST | `/api/auth/change` | Protected | Change the web UI password |
| GET | `/api/ws-token` | Protected | Issue a one-time WebSocket authentication token |

### WiFi Management

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| POST | `/api/wificonfig` | Protected | Apply temporary WiFi credentials (not persisted) |
| POST | `/api/wifisave` | Protected | Persist credentials to the multi-network list |
| GET | `/api/wifiscan` | Protected | Trigger an async 802.11 network scan |
| GET | `/api/wifilist` | Protected | Return the stored multi-network credential list |
| POST | `/api/wifiremove` | Protected | Remove a saved network by SSID |
| POST | `/api/apconfig` | Protected | Configure the soft Access Point SSID and password |
| POST | `/api/toggleap` | Protected | Enable or disable the soft Access Point |
| GET | `/api/wifistatus` | Protected | Return current connection state and IP addresses |

### OTA Updates

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/api/checkupdate` | Protected | Check GitHub for a newer firmware release |
| POST | `/api/startupdate` | Protected | Begin background OTA download from GitHub |
| GET | `/api/updatestatus` | Protected | Poll current OTA download progress |
| GET | `/api/releasenotes` | Protected | Fetch release notes for the latest GitHub release |
| GET | `/api/releases` | Protected | Return a list of all available GitHub releases |
| POST | `/api/installrelease` | Protected | Install a specific release by tag name |
| POST | `/api/firmware/upload` | Protected | Upload a local firmware binary for OTA install |

### Settings

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/api/settings` | Protected | Return all persisted application settings |
| POST | `/api/settings` | Protected | Write one or more settings fields and persist |
| GET | `/api/settings/export` | Protected | Download a full settings export as JSON |
| POST | `/api/settings/import` | Protected | Upload and apply a settings export file |

### Diagnostics

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/api/diagnostics` | Protected | Return diagnostic summary (heap, reset reason, uptime) |
| GET | `/api/diagnostics/journal` | Protected | Return the diagnostic event journal |
| DELETE | `/api/diagnostics/journal` | Protected | Clear the diagnostic event journal |
| GET | `/api/diag/snapshot` | Protected | Return a full system snapshot for support |

### System

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| POST | `/api/factoryreset` | Protected | Erase all settings and reboot |
| POST | `/api/reboot` | Protected | Reboot the device cleanly |

### Smart Sensing

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/api/smartsensing` | Protected | Return smart sensing configuration and current state |
| POST | `/api/smartsensing` | Protected | Update smart sensing parameters |

### MQTT

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/api/mqtt` | Protected | Return MQTT broker configuration and connection state |
| POST | `/api/mqtt` | Protected | Update MQTT broker settings and trigger reconnect |

### Ethernet

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/api/ethstatus` | Protected | Return full Ethernet interface status and configuration |
| POST | `/api/ethconfig` | Protected | Apply Ethernet static IP and/or hostname |
| POST | `/api/ethconfig/confirm` | Protected | Confirm a pending static IP change within 60 seconds |

### Signal Generator

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/api/signalgenerator` | Protected | Return signal generator configuration |
| POST | `/api/signalgenerator` | Protected | Update signal generator parameters |

### Audio Pipeline

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/api/pipeline/matrix` | Protected | Return the full 16×16 routing matrix |
| PUT | `/api/pipeline/matrix` | Protected | Write the full matrix, a single cell, or set bypass mode |
| GET | `/api/inputnames` | Protected | Return user-defined input channel labels |
| POST | `/api/inputnames` | Protected | Update input channel labels |

### I2S Ports

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/api/i2s/ports` | No | Return status of all 3 I2S ports (or `?id=N` for a single port) |

---

## Detailed Endpoint Documentation

### Authentication Endpoints

#### POST /api/auth/login

Authenticates and sets the `session` HttpOnly cookie on success. The default password is printed to both the TFT display and the serial console on first boot and after every factory reset. Passwords are stored as `p2:<saltHex>:<keyHex>` using PBKDF2-SHA256 with a random 16-byte salt and 50,000 iterations (controlled by the `PBKDF2_ITERATIONS` constant in `src/config.h`). The previous `p1:` format (10,000 iterations) and legacy unsalted SHA256 hashes are both accepted and automatically re-hashed to the `p2:` format on the next successful login.

**Request body** (`application/x-www-form-urlencoded`):

```
password=mypassword
```

**Success response** (HTTP 200):

```json
{ "status": "ok", "session": "<token>" }
```

**Failure response** (HTTP 401):

```json
{ "status": "error", "message": "Invalid password" }
```

**Rate limited response** (HTTP 429):

```
HTTP/1.1 429 Too Many Requests
Retry-After: 300
```

The rate limit lockout check is non-blocking: failed attempts set `_nextLoginAllowedMs` and subsequent requests that arrive during the lockout window receive HTTP 429 with a `Retry-After` header immediately. The main loop is never stalled.

:::warning Rate limiting is per-device, not per-IP
All clients share the same lockout window. A brute-force attempt from any client temporarily blocks all other clients from authenticating. The lockout window is 5 minutes.
:::

---

#### GET /api/ws-token

Issues a single-use WebSocket authentication token. The token is valid for 60 seconds and is consumed on first use. Tokens are stored in a 16-slot pool — requesting more than 16 tokens without consuming them evicts the oldest entry.

**Success response** (HTTP 200):

```json
{ "token": "a3f7c9d2-e1b0-4f8c-9c32-7f1d2e3a4b5c" }
```

The client passes this token in the first WebSocket text frame after connecting to port 81:

```json
{ "type": "auth", "token": "a3f7c9d2-e1b0-4f8c-9c32-7f1d2e3a4b5c" }
```

See the [WebSocket Protocol](../websocket.md) page for the full authentication handshake sequence.

---

#### POST /api/auth/change

Changes the web UI password. The new password is hashed with PBKDF2-SHA256 at 50,000 iterations (`p2:` format) and persisted to NVS. All existing sessions remain valid after a password change — clients are not forcibly logged out.

**Request body** (`application/json`):

```json
{
  "current": "oldpassword",
  "new": "newpassword123"
}
```

**Success response** (HTTP 200):

```json
{ "status": "ok" }
```

**Failure response** (HTTP 401):

```json
{ "status": "error", "message": "Current password incorrect" }
```

---

### Settings Endpoints

#### GET /api/settings

Returns all persisted application settings. Fields are organised by subsystem.

**Success response** (HTTP 200):

```json
{
  "audioUpdateRate": 50,
  "vuMeterEnabled": true,
  "waveformEnabled": true,
  "spectrumEnabled": false,
  "fftWindowType": 0,
  "screenTimeout": 300,
  "backlightBrightness": 200,
  "dimEnabled": true,
  "dimTimeout": 30,
  "dimBrightness": 26,
  "buzzerEnabled": true,
  "buzzerVolume": 1,
  "debugMode": false,
  "serialLevel": 1
}
```

---

#### POST /api/settings

Accepts a JSON object with one or more settings fields to update. Only provided fields are written — unrecognised keys are silently ignored.

**Request body** (`application/json`):

```json
{
  "audioUpdateRate": 33,
  "vuMeterEnabled": false,
  "buzzerVolume": 2
}
```

**Success response** (HTTP 200):

```json
{ "status": "ok" }
```

Settings are written to `/config.json` via an atomic write: the payload is first written to `/config.json.tmp`, then renamed to `/config.json`. The previous file is not deleted until the rename succeeds, preventing corruption on power loss during write.

---

#### GET /api/settings/export

Downloads the full settings file as a JSON attachment. The response is suitable for saving as a backup and re-importing with `POST /api/settings/import`.

The current export format is **version 2.0**, which includes HAL device configs, custom device schemas, DSP configuration, output DSP, and the audio routing matrix in addition to the base settings from v1.

**Success response** (HTTP 200, `application/json`):

```json
{
  "version": "2.0",
  "settings": {
    "audioUpdateRate": 50,
    "vuMeterEnabled": true,
    "buzzerEnabled": true
  },
  "mqtt": {
    "broker": "192.168.1.100",
    "port": 1883,
    "user": "ha_user",
    "prefix": "alxnova"
  },
  "halDevices": [
    {
      "slot": 0,
      "compatible": "ti,pcm5102a",
      "enabled": true,
      "volume": 80,
      "mute": false,
      "i2sPort": 0,
      "label": "Main DAC"
    }
  ],
  "halCustomSchemas": [
    {
      "compatible": "vendor,my-dac",
      "name": "My Custom DAC",
      "tier": 1,
      "i2sPort": 2,
      "sampleRatesMask": 14
    }
  ],
  "dspGlobal": {
    "enabled": true,
    "bypass": false,
    "sampleRate": 48000
  },
  "dspChannels": [
    {
      "channel": 0,
      "bypass": false,
      "stages": [
        {
          "type": 0,
          "enabled": true,
          "freq": 80.0,
          "gain": -3.0,
          "Q": 0.707
        }
      ]
    }
  ],
  "outputDsp": [
    {
      "slot": 0,
      "stages": []
    }
  ],
  "pipelineMatrix": {
    "bypass": false,
    "matrix": [
      [1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
      [0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
    ]
  }
}
```

**New sections in v2.0:**

| Section | Description |
|---------|-------------|
| `halDevices` | Per-device runtime config (volume, mute, I2S port, user label) for all registered HAL devices with saved configs |
| `halCustomSchemas` | Full JSON schemas for all custom devices stored in `/hal/custom/` |
| `dspGlobal` | Global DSP enable/bypass state and sample rate |
| `dspChannels` | Per-input-channel DSP stage list (filter type, frequency, gain, Q) |
| `outputDsp` | Per-output-sink DSP stage list |
| `pipelineMatrix` | Full 16×16 audio routing matrix and bypass state |

:::tip v1 files are still accepted
`POST /api/settings/import` accepts both v1 and v2.0 files. A v1 file restores base settings and MQTT only; a v2.0 file additionally restores HAL configs, custom schemas, DSP, and the routing matrix. The import reads whatever sections are present — missing sections are silently skipped.
:::

---

#### POST /api/settings/import

Uploads and applies a settings export file previously downloaded from `GET /api/settings/export`. The controller parses the file and restores whatever sections are present. If the `version` field is absent or `"1"`, only `settings` and `mqtt` are restored. If `version` is `"2.0"`, all available sections are restored.

**Request**: `multipart/form-data` with a field named `file` containing the JSON export file.

**Import preview response** (HTTP 200, before applying):

Before applying changes, the endpoint returns a summary of what sections it found in the uploaded file:

```json
{
  "status": "preview",
  "version": "2.0",
  "sectionsFound": [
    "settings",
    "mqtt",
    "halDevices",
    "halCustomSchemas",
    "dspGlobal",
    "dspChannels",
    "outputDsp",
    "pipelineMatrix"
  ]
}
```

The web UI displays this preview and asks the user to confirm before applying. To apply after preview, send the same file again with an `apply=1` query parameter:

```
POST /api/settings/import?apply=1
```

**Apply response** (HTTP 200):

```json
{
  "status": "ok",
  "sectionsApplied": [
    "settings",
    "mqtt",
    "halDevices",
    "dspGlobal",
    "dspChannels",
    "pipelineMatrix"
  ],
  "sectionsSkipped": [
    "halCustomSchemas",
    "outputDsp"
  ]
}
```

`sectionsSkipped` lists sections that were present in the file but could not be applied — for example, a `halCustomSchemas` section whose devices could not be instantiated due to a missing I2S port.

**Version compatibility:**

| File `version` | Sections restored |
|----------------|-----------------|
| absent or `"1"` | `settings`, `mqtt` |
| `"2.0"` | All sections present in the file |

**Error codes**

| Status | Meaning |
|--------|---------|
| 200 | Preview or apply successful |
| 400 | Not a valid JSON file, or `version` field unrecognised |
| 413 | File too large (max 64 KB) |
| 500 | LittleFS write failed during apply |

:::warning Partial imports
Only sections present in the uploaded file are touched. Sections absent from the file are left unchanged. This means you can safely import a v1 backup onto a device that has custom schemas and DSP presets — only the base settings and MQTT will be overwritten.
:::

---

### OTA Update Endpoints

#### GET /api/checkupdate

Checks the GitHub Releases API for a firmware version newer than the running `FIRMWARE_VERSION`. Requires WiFi or Ethernet connectivity. The first OTA check is delayed 30 seconds after boot to allow Ethernet DHCP and DNS to stabilise.

**Success response** (HTTP 200):

```json
{
  "current": "1.15.0",
  "latest": "1.16.0",
  "updateAvailable": true,
  "releaseUrl": "https://github.com/Schmackos/ALX_Nova_Controller_2/releases/tag/v1.16.0"
}
```

**No update available** (HTTP 200):

```json
{
  "current": "1.16.0",
  "latest": "1.16.0",
  "updateAvailable": false
}
```

:::info OTA check is skipped when heap is critical
When `ESP.getMaxAllocHeap()` drops below 40 KB, the automatic background OTA check is suppressed. TLS handshake buffers require approximately 55 KB of contiguous heap. The check can still be triggered manually via this endpoint regardless of heap state.
:::

---

#### POST /api/startupdate

Starts a background OTA download and install from the latest GitHub release. The download runs in a separate FreeRTOS task on Core 0. Poll `GET /api/updatestatus` for progress.

**Success response** (HTTP 200):

```json
{ "status": "ok", "message": "OTA started" }
```

**Already in progress** (HTTP 409):

```json
{ "status": "error", "message": "OTA already in progress" }
```

---

#### GET /api/updatestatus

Returns the current OTA download progress. The `percent` field is 0–100. The device reboots automatically after a successful install.

**Success response** (HTTP 200):

```json
{
  "inProgress": true,
  "percent": 47,
  "error": null
}
```

**Error state** (HTTP 200):

```json
{
  "inProgress": false,
  "percent": 0,
  "error": "SHA256 verification failed"
}
```

---

#### POST /api/installrelease

Installs a specific release tag rather than the latest. Useful for rollback.

**Request body** (`application/json`):

```json
{ "tag": "v1.14.2" }
```

**Success response** (HTTP 200):

```json
{ "status": "ok", "message": "OTA started for v1.14.2" }
```

---

#### POST /api/firmware/upload

Accepts a firmware binary uploaded as `multipart/form-data`. The binary is written directly to the OTA partition. On completion, the SHA256 digest is verified against the partition content before the device reboots.

**Request**: `multipart/form-data` with a field named `update` containing the `.bin` file.

**Success response** (HTTP 200):

```json
{ "status": "ok", "message": "Update complete, rebooting" }
```

**Failure responses**:

```json
{ "status": "error", "message": "SHA256 verification failed" }
{ "status": "error", "message": "Not enough space" }
{ "status": "error", "message": "Upload failed" }
```

:::warning OTA partition size limit
Each OTA partition is 4 MB. Binaries exceeding this size are rejected with HTTP 400. Confirm you are uploading a release build (`pio run`) rather than a debug build — debug builds with full symbols can exceed the partition limit.
:::

---

### Smart Sensing Endpoints

#### GET /api/smartsensing

Returns smart sensing configuration and current FSM state.

**Success response** (HTTP 200):

```json
{
  "enabled": true,
  "threshold": 0.05,
  "autoOffTimeout": 300,
  "amplifierActive": false,
  "fsmState": "IDLE",
  "signalDetected": false
}
```

---

#### POST /api/smartsensing

Updates smart sensing parameters. Partial updates are supported — only provided fields are written.

**Request body** (`application/json`):

```json
{
  "enabled": true,
  "threshold": 0.08,
  "autoOffTimeout": 600
}
```

**Success response** (HTTP 200):

```json
{ "status": "ok" }
```

---

### MQTT Endpoints

#### GET /api/mqtt

Returns MQTT broker configuration and current connection state.

**Success response** (HTTP 200):

```json
{
  "enabled": true,
  "broker": "192.168.1.100",
  "port": 1883,
  "user": "ha_user",
  "prefix": "alxnova",
  "connected": true,
  "haDiscovery": true
}
```

---

#### POST /api/mqtt

Saves MQTT broker configuration and signals the dedicated MQTT task on Core 0 to reconnect by setting `appState._mqttReconfigPending`.

**Request body** (`application/json`):

```json
{
  "broker": "192.168.1.100",
  "port": 1883,
  "user": "ha_user",
  "pass": "secret",
  "prefix": "alxnova",
  "enabled": true
}
```

**Success response** (HTTP 200):

```json
{ "status": "ok" }
```

The MQTT task polls `_mqttReconfigPending` at 20 Hz and reconnects within approximately 50 ms of this flag being set. The HTTP server on the main loop is never blocked during MQTT reconnect — the 1–3 second blocking TCP connect runs entirely on Core 0 inside `mqtt_task`.

---

### Signal Generator Endpoints

#### GET /api/signalgenerator

Returns the current signal generator configuration.

**Success response** (HTTP 200):

```json
{
  "success": true,
  "enabled": false,
  "waveform": "sine",
  "frequency": 1000.0,
  "amplitude": -12.0,
  "channel": "both",
  "outputMode": "software",
  "sweepSpeed": 100.0
}
```

**Field descriptions**:

| Field | Type | Values | Description |
|-------|------|--------|-------------|
| `waveform` | string | `sine`, `square`, `white_noise`, `sweep` | Output waveform shape |
| `frequency` | float | 1.0 – 22000.0 Hz | Tone frequency (not used for `white_noise`) |
| `amplitude` | float | -96.0 – 0.0 dBFS | Output level in dBFS |
| `channel` | string | `left`, `right`, `both` | Which audio channel receives the signal |
| `outputMode` | string | `software`, `pwm` | `software` injects into the audio pipeline; `pwm` outputs on GPIO 47 |
| `sweepSpeed` | float | 1.0 – 22000.0 | Frequency sweep rate in Hz/s (only used when `waveform` is `sweep`) |

---

#### POST /api/signalgenerator

Updates signal generator parameters. Only provided fields are applied. Settings are persisted automatically.

**Request body** (`application/json`):

```json
{
  "enabled": true,
  "waveform": "sweep",
  "frequency": 20.0,
  "amplitude": -20.0,
  "channel": "both",
  "outputMode": "software",
  "sweepSpeed": 500.0
}
```

**Success response** (HTTP 200):

```json
{ "success": true }
```

:::tip Disabling the signal generator
Send `{ "enabled": false }` to stop output without changing any other parameters. The generator always boots disabled regardless of the persisted state.
:::

---

### Audio Pipeline Endpoints

#### GET /api/pipeline/matrix

Returns the full 16×16 routing matrix. Index convention: `matrix[output][input]`, where each cell is a linear gain value (1.0 = unity gain, 0.0 = silence). Also returns the current bypass state.

**Success response** (HTTP 200):

```json
{
  "success": true,
  "bypass": false,
  "size": 16,
  "matrix": [
    [1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    "..."
  ]
}
```

:::tip Backward compatibility for legacy 8×8 matrices
When loading a matrix saved by firmware older than v1.12.0, the 8×8 values are placed in the top-left quadrant of the 16×16 matrix. The remaining cells are zero-filled. This migration is applied automatically when the settings file is loaded.
:::

---

#### PUT /api/pipeline/matrix

Writes the routing matrix. Supports three mutually exclusive operations in a single request: set bypass mode, update a single cell (linear or dB gain), or replace the entire matrix. The matrix is saved to LittleFS asynchronously 2 seconds after the last change.

**Set bypass mode**:

```json
{ "bypass": true }
```

**Update a single cell (linear gain)**:

```json
{
  "cell": { "out": 0, "in": 1, "gain": 0.707 }
}
```

**Update a single cell (dB gain)**:

```json
{
  "cell_db": { "out": 0, "in": 1, "gain_db": -3.0 }
}
```

**Replace full matrix (16 rows, each with 16 linear gain values)**:

```json
{
  "matrix": [
    [1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    "..."
  ]
}
```

**Success response** (HTTP 200):

```json
{ "success": true }
```

---

#### GET /api/inputnames

Returns user-defined labels for all input channels.

**Success response** (HTTP 200):

```json
{
  "success": true,
  "names": ["ADC In 1", "ADC In 2", "ADC 2 L", "ADC 2 R", "SigGen", "SigGen R", "USB L", "USB R", "..."],
  "numAdcsDetected": 2
}
```

The `names` array has `AUDIO_PIPELINE_MAX_INPUTS * 2` entries (16 entries for the current build).

---

#### POST /api/inputnames

Updates user-defined input channel labels. The request must include a `names` array; entries beyond the array length are left unchanged.

**Request body** (`application/json`):

```json
{
  "names": ["Phono", "CD Player", "Tape", "Tuner"]
}
```

**Success response** (HTTP 200):

```json
{ "success": true }
```

---

### Diagnostics Endpoints

#### GET /api/diagnostics/journal

Returns the diagnostic event journal — a persistent ring buffer stored in LittleFS. Events are emitted by all firmware subsystems with a severity level, a structured 16-bit error code, and an optional HAL device slot reference.

**Success response** (HTTP 200):

```json
{
  "type": "diagJournal",
  "count": 3,
  "entries": [
    {
      "seq": 42,
      "boot": 5,
      "t": 12340,
      "heap": 180420,
      "c": "0x1001",
      "sev": "W",
      "sub": "AUDIO",
      "dev": "PCM1808",
      "slot": 2,
      "msg": "ADC lane 1 health check failed",
      "retry": 1,
      "corr": 7
    }
  ]
}
```

**Severity codes**: `I` = Info, `W` = Warning, `E` = Error, `C` = Critical.

Error codes are structured as 16-bit values where the high byte encodes the subsystem and the low byte encodes the specific fault. See `src/diag_error_codes.h` for the full enumeration.

---

#### GET /api/diag/snapshot

Returns a comprehensive system snapshot in a single JSON response. Intended for attaching to support requests or feeding to AI-assisted debugging tools.

**Success response** (HTTP 200):

```json
{
  "type": "diagSnapshot",
  "timestamp": 84321,
  "freeHeap": 182400,
  "freePsram": 4194304,
  "maxAllocHeap": 102400,
  "fsmState": 0,
  "halDevices": [
    {
      "slot": 0,
      "name": "PCM5102A",
      "compatible": "ti,pcm5102a",
      "type": 1,
      "state": 4,
      "ready": true,
      "retries": 0,
      "faults": 0
    }
  ],
  "recentEvents": [
    {
      "seq": 42,
      "t": 12340,
      "c": "0x1001",
      "dev": "PCM1808",
      "msg": "ADC health check failed",
      "sev": "W"
    }
  ]
}
```

---

### System Endpoints

#### POST /api/factoryreset

Erases all LittleFS settings files and reboots the device. A new random password (10 characters, approximately 57 bits of entropy) is generated on the next boot and displayed on the TFT display and serial console.

**Success response** (HTTP 200):

```json
{ "status": "ok", "message": "Factory reset initiated" }
```

The device reboots approximately 2 seconds after this response is sent.

:::danger Irreversible operation
Factory reset erases all LittleFS data: settings, DSP presets, input names, MQTT configuration, HAL device configs, and the diagnostic journal. WiFi credentials stored in ESP32 NVS are preserved. There is no confirmation step or undo.
:::

---

#### POST /api/reboot

Reboots the device cleanly. All pending settings writes are flushed before reboot.

**Success response** (HTTP 200):

```json
{ "status": "ok", "message": "Rebooting" }
```

---

### Ethernet Endpoints

#### GET /api/ethstatus

Returns the full Ethernet interface status and configuration. Available regardless of link state.

**Success response** (HTTP 200):

```json
{
  "linkUp": true,
  "connected": true,
  "ip": "192.168.1.100",
  "mac": "AA:BB:CC:DD:EE:FF",
  "speed": 100,
  "fullDuplex": true,
  "gateway": "192.168.1.1",
  "subnet": "255.255.255.0",
  "dns1": "8.8.8.8",
  "dns2": "8.8.4.4",
  "hostname": "alx-nova",
  "useStaticIP": false,
  "staticIP": "",
  "staticSubnet": "255.255.255.0",
  "staticGateway": "",
  "staticDns1": "",
  "staticDns2": "",
  "activeInterface": "ethernet",
  "pendingConfirm": false
}
```

| Field | Type | Description |
|-------|------|-------------|
| `linkUp` | boolean | Physical link is detected on the Ethernet port |
| `connected` | boolean | Interface has an assigned IP address |
| `ip` | string | Currently assigned IP address (DHCP or static) |
| `mac` | string | Hardware MAC address |
| `speed` | number | Negotiated link speed in Mbps (0, 10, or 100) |
| `fullDuplex` | boolean | Full duplex negotiated |
| `gateway` | string | Default gateway IP |
| `subnet` | string | Subnet mask |
| `dns1` / `dns2` | string | Primary and secondary DNS server addresses |
| `hostname` | string | Device hostname (shared between Ethernet and WiFi) |
| `useStaticIP` | boolean | Static IP configuration is active |
| `staticIP` | string | Configured static IP (empty string if not set) |
| `activeInterface` | string | Active network interface: `"ethernet"`, `"wifi"`, or `"none"` |
| `pendingConfirm` | boolean | A static IP change is awaiting confirmation within the 60-second window |

---

#### POST /api/ethconfig

Apply Ethernet configuration. Supports updating the static IP settings, the hostname, or both in a single request.

**Request body** (`application/json`):

```json
{
  "useStaticIP": true,
  "staticIP": "192.168.1.100",
  "subnet": "255.255.255.0",
  "gateway": "192.168.1.1",
  "dns1": "8.8.8.8",
  "dns2": "8.8.4.4",
  "hostname": "alx-nova"
}
```

**Response when static IP is applied** (HTTP 200):

```json
{ "success": true, "pendingConfirm": true }
```

**Response for hostname-only or DHCP changes** (HTTP 200):

```json
{ "success": true }
```

When `pendingConfirm` is `true`, the new configuration is applied immediately but a 60-second safety timer starts. If `/api/ethconfig/confirm` is not called before the timer expires, the configuration automatically reverts to DHCP. This prevents a misconfigured static IP from making the device permanently unreachable.

**Validation rules:**

- IP addresses (staticIP, subnet, gateway, dns1, dns2) are validated with `IPAddress.fromString()`
- Hostname: 1–63 characters, characters must match `[a-zA-Z0-9-]`, no leading or trailing hyphen (RFC 1123)
- When `useStaticIP` is `true`: the `staticIP`, `subnet`, and `gateway` fields are all required

**Error responses** (HTTP 400):

```json
{ "success": false, "message": "Invalid IP address" }
{ "success": false, "message": "Invalid hostname" }
{ "success": false, "message": "Static IP, subnet, and gateway required" }
```

:::warning Static IP confirmation window
After applying a static IP, you must call `POST /api/ethconfig/confirm` within 60 seconds from a client that can reach the device at the new IP. If you cannot reach the device at the new address, wait 60 seconds — the configuration reverts to DHCP automatically.
:::

---

#### POST /api/ethconfig/confirm

Confirms a pending static IP configuration. Must be called within 60 seconds of a `POST /api/ethconfig` response that returned `pendingConfirm: true`.

**Success response** (HTTP 200):

```json
{ "success": true }
```

**No pending confirmation** (HTTP 400):

```json
{ "success": false, "message": "No pending confirmation" }
```

Once confirmed, the static IP configuration is persisted to `/config.json` and the revert timer is cancelled.

---

## Standard Error Responses

All endpoints use a consistent error envelope:

| HTTP Status | Meaning |
|-------------|---------|
| 302 | Session expired or not authenticated — redirect to `/login` |
| 400 | Bad request — missing required fields or invalid values |
| 409 | Conflict — the requested operation is already in progress |
| 429 | Login rate limited — check `Retry-After` header for wait duration |
| 500 | Internal firmware error — check serial log for details |
| 503 | Service temporarily unavailable — DSP swap busy or heap critical |

**Error body** (`application/json`):

```json
{ "status": "error", "message": "Descriptive error text" }
```

DSP endpoints additionally return HTTP 503 when a double-buffer swap cannot complete within the allowed window. The client should retry the request after a short delay — the DSP task yields every 5.33 ms and swap failures are transient.
