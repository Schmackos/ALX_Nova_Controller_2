---
title: Production Deployment
sidebar_position: 3
description: Firmware versioning, OTA at scale, MQTT fleet provisioning, and factory test procedures for volume production.
---

# Production Deployment

This page covers the operational procedures for manufacturing and deploying ALX Nova-based products at volume — from factory flashing to over-the-air updates in the field.

---

## Firmware Versioning Strategy

Every production build must have a distinct `FIRMWARE_VERSION` string in `src/config.h`. The version is embedded in the firmware binary and reported by `GET /api/system/info`, MQTT birth messages, and the OTA update mechanism.

Recommended scheme for OEM products:

```
<product-line>-<major>.<minor>.<patch>
```

Example: `nova-amp-1.4.2`. Keep the version string under 32 characters (the field is null-terminated in a 32-byte buffer).

Update the version in `src/config.h` before every production build, even for patch releases. The OTA update checker performs an exact string comparison — identical version strings always report "up to date" regardless of binary content.

---

## Factory Flashing

### Direct esptool Flash (Recommended for Volume)

For production lines, flash directly with `esptool.py` rather than PlatformIO. This bypasses the build step and flashes a pre-built binary:

```bash
# Windows — esptool Unicode fix for progress bar
PYTHONIOENCODING=utf-8 esptool.py \
  --chip esp32p4 \
  --port COM8 \
  --baud 921600 \
  write_flash \
  --flash_size 16MB \
  --flash_mode qio \
  0x0    build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 .pio/build/esp32-p4/firmware.bin
```

Pre-built binaries are available as GitHub Release assets. Download them as part of your production tooling setup, or host them on an internal artifact server.

### PlatformIO Upload (Development and Low-Volume)

For development and low-volume flashing:

```bash
PYTHONIOENCODING=utf-8 pio run --target upload
```

Upload port is configured as `COM8` in `platformio.ini`. Override with `-e esp32-p4 --upload-port COMx` for a different port.

---

## OTA at Scale

### Self-Hosted Release Endpoint

Override the default OTA check URL by setting `OTA_RELEASE_URL` in `platformio.ini`:

```ini
build_flags =
  -D OTA_RELEASE_URL='"https://releases.yourdomain.com/nova/latest.json"'
```

The endpoint must return:

```json
{
  "latest_version": "nova-amp-1.4.2",
  "download_url": "https://releases.yourdomain.com/nova/nova-amp-1.4.2.bin",
  "sha256": "a1b2c3d4..."
}
```

The firmware verifies the SHA-256 hash of the downloaded binary before applying. A mismatched hash aborts the update and rolls back automatically — the running firmware is never overwritten by a corrupt binary.

### Device Group MQTT Topics

For fleet-wide update pushes without polling, use MQTT to trigger OTA checks across device groups. The controller subscribes to `alxnova/<mac>/cmd` for direct device commands and to `alxnova/broadcast/cmd` for fleet-wide commands.

To trigger an OTA check on all devices in a product group:

```bash
mosquitto_pub -t "alxnova/broadcast/cmd" \
  -m '{"cmd": "ota_check"}' \
  -h your-broker.local
```

Each device will independently call its configured OTA endpoint and apply the update if a newer version is available.

### ESP-IDF Dual-Partition Rollback

The ESP32-P4 uses a dual-partition OTA scheme. If the new firmware fails to boot (crashes before calling `esp_ota_mark_app_valid_cancel_rollback()`), the bootloader rolls back to the previous partition on the next reset. This makes OTA updates safe to push at scale without physical recovery procedures for most failure modes.

If a device enters a persistent boot loop after an update:
1. The bootloader detects multiple consecutive failures and reverts to the last known-good partition.
2. The device boots into the previous firmware version and reports the rollback via `GET /api/system/info` (`"otaRollback": true`).

---

## MQTT Fleet Provisioning

### Boot Announcement

Every device publishes a birth message at boot on `alxnova/<mac>/status`:

```json
{
  "status": "online",
  "version": "nova-amp-1.4.2",
  "ip": "192.168.1.50",
  "mac": "AA:BB:CC:DD:EE:FF"
}
```

Fleet management systems can subscribe to `alxnova/+/status` to maintain a real-time device registry.

### Settings Backup and Restore

Export a device's full configuration:
```bash
GET /api/settings/export
```

Restore to another device (or the same device after a factory reset):
```bash
PUT /api/settings/import
Content-Type: application/json
<settings JSON>
```

For fleet provisioning, push a golden settings JSON to new devices immediately after the first boot OTA update to apply a standard configuration baseline.

---

## Factory Test Checklist

Run this checklist on every unit before shipping. All checks use the REST API and can be automated with a simple Python or bash test script.

:::note Factory Test Script
The on-device test harness in `device_tests/` provides a ready-made pytest framework for automated hardware validation. Run `pytest tests/ --device-ip <IP> -v` with the device connected to the test bench network.
:::

**1. Health Check — All Categories Green**
```bash
GET /api/health
# Expected: all 9 categories in "ok" state, no "error" or "warn"
```

**2. HAL Device Count — Correct Onboard Devices**
```bash
GET /api/hal/devices
# Expected: 14 onboard devices (ES8311, PCM1808×2, onboard amplifier, etc.)
# If count is wrong: I2C bus wiring or power issue
```

**3. Audio Signal Path — Signal Generator Smoke Test**
```bash
# Start 1 kHz sine on output 0
POST /api/siggen
Content-Type: application/json
{"enabled": true, "frequency": 1000, "amplitude": 0.1, "outputMask": 1}

# Measure output with audio analyser or oscilloscope
# Stop signal generator
POST /api/siggen
{"enabled": false}
```

**4. WiFi AP Mode Connectivity**
- Confirm the `ALX-XXXXXXXXXXXX` AP appears in WiFi scan
- Connect and verify `http://192.168.4.1` loads the web dashboard

**5. WiFi STA Mode Provisioning**
- Configure STA credentials via `PUT /api/wifi`
- Verify `GET /api/wifi/status` returns `connected` and a valid IP address

**6. OTA Endpoint Reachability**
```bash
POST /api/ota/check
# Expected: JSON response with version info (does not need a newer version,
# just confirm the endpoint is reachable and returns a 200 response)
```

**7. WebSocket Authentication**
- Connect to ws://`<device-ip>`:81 with valid credentials
- Confirm initial state broadcast received within 2 seconds

**8. Factory Reset and Re-provision (Spot Check — 1 in 20 units)**
- Trigger factory reset via `POST /api/system/factory-reset`
- Confirm device boots into AP mode with default password
- Re-provision with standard configuration backup
