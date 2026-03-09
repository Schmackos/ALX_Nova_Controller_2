---
title: OTA Updates
sidebar_position: 7
description: Over-the-air firmware update process for the ALX Nova.
---

# OTA Firmware Updates

ALX Nova can update its own firmware over the network — no USB cable, no programmer, no disassembly. Updates are fetched directly from the project's **GitHub Releases** page over HTTPS, verified with SHA256, then installed to a separate flash partition. If anything goes wrong the original firmware remains intact.

---

## How OTA Works

The update process runs in three stages:

1. **Check** — The controller queries the GitHub Releases API to find the latest published version and compares it to the currently running firmware.
2. **Download** — If a newer version is found, the binary is downloaded over HTTPS. Every byte is verified against the published SHA256 checksum before installation begins.
3. **Install** — The verified firmware is written to the standby OTA partition. On success the controller reboots into the new firmware. If the write fails or the checksum does not match, the standby partition is discarded and the running firmware is untouched.

:::info
The ESP32-P4 uses a dual-partition OTA scheme (ota_0 and ota_1, each 4 MB). The running firmware is always on one partition; updates go to the other. A failed update can never overwrite the firmware you are currently running.
:::

---

## Automatic Update Checks

When **Auto Update** is enabled, the controller checks for a new release every **5 minutes** while it has internet access (WiFi or Ethernet). A 15-second grace period after boot prevents an update from interrupting the startup sequence.

When an update is found, a 30-second countdown is displayed on the TFT screen and in the web interface's System section before the download begins. You can cancel the countdown from either place if you want to defer the update.

---

## Triggering an Update from the Web Interface

1. Open the web interface and go to the **System** section.
2. Find the **OTA Updates** panel. It shows your current firmware version and the last time a check was performed.
3. Click **Check for Updates**.
4. If a newer version is available, an **Update Now** button appears alongside the release notes link.
5. Click **Update Now** to begin the download immediately.
6. A progress bar shows download progress. The controller reboots automatically when installation is complete.

:::warning
Do not power off the controller while an update is in progress. A power cut mid-write cannot corrupt your running firmware (it is on a separate partition), but you will need to trigger the update again after power is restored.
:::

---

## Manual Firmware Upload

You can also install firmware from a local `.bin` file — useful when the controller has no internet access, when testing pre-release builds, or when a specific version is needed.

1. In the **OTA Updates** panel, locate the **Manual Upload** area.
2. Either drag and drop your `.bin` file onto the upload area, or click to open a file browser and select the file.
3. The upload progress is shown in the progress bar.
4. The controller verifies the file and reboots if successful.

:::tip
Download official firmware `.bin` files from the [GitHub Releases page](https://github.com/Schmackos/ALX_Nova_Controller_2/releases). Each release lists the SHA256 checksum alongside the binary download.
:::

:::danger
Only upload firmware built for the ESP32-P4 (Waveshare ESP32-P4-WiFi6-DEV-Kit). Uploading firmware compiled for a different board will likely cause a boot loop. If this happens, use the USB serial connection to reflash from a computer.
:::

---

## SHA256 Verification

Every firmware image — whether downloaded automatically from GitHub or uploaded manually — is verified against a SHA256 checksum before it is written to flash.

- For **GitHub releases**, the checksum is fetched from the release assets and compared to the downloaded binary.
- For **manual uploads**, the controller computes the SHA256 of the uploaded file and displays it alongside the result. You can compare this to the checksum published on the releases page.

If verification fails the update is aborted and the controller continues running the current firmware. A warning is shown in the web interface and logged to the Debug Console.

---

## What Happens on Failure

| Failure point | Result |
|---|---|
| Network unreachable during check | Check fails silently; retry in 5 minutes |
| Download interrupted | Partial data discarded; running firmware unchanged |
| SHA256 mismatch | Installation aborted; warning shown in web UI |
| Write error to flash | Installation aborted; running firmware unchanged |
| Reboot into new firmware fails | ESP32-P4 rollback mechanism restores previous partition |

---

## Checking Your Current Firmware Version

Your firmware version is shown in several places:

- **System section** of the web interface — OTA Updates panel header
- **TFT display** — Support screen (accessible from the desktop carousel)
- **MQTT** — published to `{base}/firmware_version`
- **Serial console** — printed during boot

The current version is **1.12.1**.

---

## SSL / HTTPS Certificate Validation

OTA downloads are fetched over HTTPS with full certificate chain validation using the Mozilla certificate bundle. This ensures you are downloading genuine firmware from GitHub and not an intercepted file.

No manual certificate management is needed — the certificate bundle is embedded in the firmware and covers all major public certificate authorities.
