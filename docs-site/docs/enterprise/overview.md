---
title: Enterprise Overview
sidebar_position: 1
description: How ALX Nova serves OEMs building professional audio products — platform overview, certification tiers, and commercial model.
---

# Enterprise Overview

ALX Nova is an open-source audio platform designed to scale from maker prototype to commercial product without rewriting firmware. This section is for organisations building professional audio devices — carrier board manufacturers, OEM integrators, and audio equipment companies who want to ship on the ESP32-P4 and benefit from an actively maintained HAL, DSP pipeline, and web UI.

## Why ALX Nova for OEM Products?

### No Silicon Re-work on DAC/ADC Substitutions

The Hardware Abstraction Layer (HAL) insulates application code and web UI from the specifics of audio codec silicon. The 35 built-in drivers cover 26 expansion devices across 4 generic driver patterns (Pattern A–D). Swapping an ES9038Q2M for an ES9039Q2M requires changing one EEPROM image — no firmware modification, no web UI update, no re-certification of the control stack.

### ESP32-P4 Long-Term Availability

Espressif offers a 5-year supply commitment for the ESP32-P4. Combined with the board-agnostic HAL layer, a future migration to a successor chip requires only updating the I2S and I2C peripheral drivers — the HAL driver library and REST API surface remain unchanged.

### Production-Ready Control Stack Included

Every product built on ALX Nova ships with a complete web dashboard (WebSocket real-time updates), REST API (dual-path `/api/` and `/api/v1/`), MQTT integration with Home Assistant auto-discovery, OTA firmware update, and secure WebSocket authentication — all maintainable without a dedicated UI engineering team.

### Whitelabelable Interface

The web UI source is in `web_src/` and is compiled into gzip-compressed static assets at build time. OEMs can customise branding, hide irrelevant tabs, and extend the UI by editing JavaScript source files before the asset build step (`node tools/build_web_assets.js`).

---

## Commercial Paths

ALX Nova offers three structured engagement tiers for organisations building on the platform.

### Community ("Works With ALX")

The no-cost entry point. Any mezzanine module or carrier board that implements the EEPROM v3 compatible string format and the connector pinout standard can apply for a "Works with ALX" badge. Requirements:

- EEPROM v3 layout with a valid `compatible` string (e.g. `"ess,es9038q2m"`)
- 16-pin connector pinout compliance (see [Mezzanine Connector](../developer/hal/mezzanine-connector))
- Driver passes the standard HAL unit test suite or a pull request is submitted for a new driver
- Hardware design files published (OSHWA certification encouraged)

### Certified Partner

Commercial partners who have formally joined the partner programme. Benefits include:

- Pre-release firmware access (2 weeks ahead of public release)
- Named contact for priority bug escalation
- Regression SLA: issues specific to your device fixed within 2 minor releases
- Listing in the ALX device database as a certified module

### Fully Custom OEM

For products that require a private fork, custom HAL drivers not intended for the public driver registry, or modifications to the REST API surface. Available as a consulting engagement — see [Support Tiers](./support-tiers) for details.

---

## Quick Navigation

| Topic | Description |
|---|---|
| [OEM Integration](./oem-integration) | HAL driver tiers, mezzanine connector, BOM guidance, build flags |
| [Production Deployment](./production-deployment) | Factory flash, OTA at scale, MQTT fleet provisioning, factory test checklist |
| [Certification & Compliance](./certification) | CE/FCC roadmap, OSHWA, Works With programme, partner programme |
| [Support Tiers](./support-tiers) | Community vs commercial support comparison, consulting availability |

---

## Platform Snapshot

| Attribute | Value |
|---|---|
| Target MCU | ESP32-P4 (360 MHz, Xtensa LX7 dual-core, 32 MB PSRAM) |
| Framework | PlatformIO + Arduino (ESP-IDF 5.x based) |
| HAL drivers | 35 registered (4 generic patterns, 26 expansion devices) |
| Audio pipeline | 8 input lanes → 32×32 routing matrix → 16 output sinks, float32 |
| API surface | REST (port 80) + WebSocket (port 81), dual-path `/api/` and `/api/v1/` |
| OTA | SHA-256 verified, dual-partition automatic rollback |
| HAL device slots | 32 total (14 onboard at boot, up to 18 expansion) |
| I2S ports | 3 fully configurable (STD/TDM, TX/RX, any pins) |
