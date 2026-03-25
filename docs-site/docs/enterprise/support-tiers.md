---
title: Support Tiers
sidebar_position: 5
description: Community versus commercial support options, partner program, and where to get help for enterprise deployments.
---

# Support Tiers

ALX Nova is an open-source project with structured support options for commercial deployments. This page explains what each tier includes and how to access it.

---

## Tier Comparison

| | Community | Works With Partner | Certified Partner |
|---|---|---|---|
| **GitHub Issues** | Yes | Yes | Yes (priority label) |
| **GitHub Discussions** | Yes | Yes | Yes |
| **Response time SLA** | Best effort | Best effort | P1: 5 business days |
| **Regression SLA** | None | 2 minor releases | 2 minor releases |
| **Pre-release firmware** | No | No | 2 weeks ahead |
| **Named contact** | No | No | Yes |
| **ALX Store listing** | No | No | Eligible |
| **Device DB listing** | No | Community | Certified |
| **Commercial licence required** | No | No | Contact us |

---

## Community Support

All users and developers have access to the full community support channels:

- **GitHub Issues** — Bug reports and feature requests. Use the issue templates and include firmware version, hardware configuration, and reproduction steps.
- **GitHub Discussions** — General questions, integration help, and sharing projects. Use the `[Enterprise]` tag in the title if your question is specific to production deployment or OEM integration.
- **Documentation** — This documentation site covers the full API reference, HAL framework, testing procedures, and developer guides.

Response time on community issues is best effort. The maintainer team prioritises issues with clear reproduction steps and attached diagnostic output (`GET /api/health`, serial log excerpt with `LOG_E` and `LOG_W` lines).

---

## Works With Partner Support

Organisations with a "Works With ALX" badge have their device in the public driver registry. The regression SLA applies to that specific device:

**Regression SLA:** If a firmware update breaks a certified "Works With" device (device no longer initialises, audio path fails, or HAL state machine enters a permanent Error state), the maintainer team commits to shipping a fix within 2 minor releases (approximately 4–8 weeks depending on release cadence).

To report a regression for your device:
1. Open a GitHub Issue with the `[regression]` label and your device's compatible string in the title (e.g. `[regression] ess,es9038q2m — volume control broken in v1.21.0`).
2. Include the firmware version where the device last worked correctly.
3. Attach the serial log with `LOG_E` and `LOG_W` output from the HAL module.

---

## Certified Partner Support

Certified commercial partners receive structured support with defined response times.

### Response Time Commitments

| Priority | Definition | Response Time |
|---|---|---|
| P1 | Device not initialising or audio pipeline not functioning on released firmware | 5 business days |
| P2 | Non-critical regression, UI issue, or API mismatch | 10 business days |
| P3 | Feature request or documentation gap | Next planning cycle |

Response time means a maintainer has acknowledged the issue and provided an initial assessment or workaround. Resolution time depends on issue complexity.

### Pre-Release Firmware Access

Certified partners receive access to release-candidate builds 2 weeks before public release. This allows production-critical regression testing before your customer base updates.

Pre-release builds are shared via a private GitHub repository or release channel agreed during the partner onboarding process.

### Named Contact

Each certified partner is assigned a named technical contact within the maintainer team. Escalate P1 and P2 issues directly to your contact via the agreed channel (email or private GitHub repository) in addition to opening a public issue.

---

## Consulting Availability

For requirements that go beyond the structured support tiers — private firmware forks, custom HAL drivers, carrier board design review, or custom certification support — consulting engagements are available.

To enquire, open a GitHub Discussion with the `[Enterprise]` tag and a brief description of your project scope, timeline, and the specific area where you need assistance. A maintainer will respond to assess fit and availability.

Areas commonly covered by consulting engagements:

- **Custom HAL driver development** — Tier 3 drivers for silicon not in the public registry
- **Carrier board design review** — Schematic and layout review against the mezzanine connector specification and power supply guidelines
- **OTA infrastructure setup** — Private release hosting, MQTT fleet provisioning, rollback strategy
- **Regulatory pre-scan preparation** — EMC issue identification and mitigation guidance before formal submission
- **Production test harness** — Extending the `device_tests/` pytest harness for your specific hardware configuration

---

## Filing a Support Request

Regardless of tier, include the following in every support request to minimise back-and-forth:

1. **Firmware version** — from `GET /api/system/info` (`version` field) or the web dashboard title bar
2. **Hardware configuration** — carrier board revision, mezzanine modules populated, ESP32-P4 board variant
3. **Symptom** — what you expected vs what actually happened
4. **Health report** — output of `GET /api/health` (copy the JSON)
5. **Serial log excerpt** — `LOG_E` and `LOG_W` lines from the relevant module, captured via `pio device monitor`
6. **Reproduction steps** — minimal sequence of API calls or UI actions that triggers the issue

For audio-specific issues, also include the output of `GET /api/hal/devices` and `GET /api/pipeline/status`.
