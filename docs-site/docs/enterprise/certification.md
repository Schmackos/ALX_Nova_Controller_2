---
title: Certification & Compliance
sidebar_position: 4
description: CE/FCC regulatory roadmap, OSHWA open-source hardware certification, and the ALX Works With program.
---

# Certification & Compliance

This page covers the certification and compliance landscape for products built on the ALX Nova platform — from the community "Works With ALX" badge through radio certification to open-source hardware recognition.

---

## Works With ALX (Community Programme)

Any mezzanine module or carrier board that meets the interface standard can display the "Works with ALX" badge. This is a community programme with no commercial fee.

### Requirements

1. **EEPROM v3 compliance** — The module carries an AT24C02 (or compatible 2 kbit) EEPROM programmed with the v3 layout. The `compatible` string at offset 0x5E must be in `"category,chipname"` format (e.g. `"ess,es9038q2m"`). The CRC-16/CCITT at offset 0x7E must be valid.

2. **Connector pinout compliance** — The module uses the ALX Nova [16-pin mezzanine connector](../developer/hal/mezzanine-connector) pinout. Revision 1 14-pin modules are grandfathered but must document that pins 15–16 (±15V) are absent.

3. **HAL driver available** — Either a pull request to the public driver registry is merged, or the module works with an existing driver (confirmed with the `compatible` string).

4. **Hardware design files published** — Schematic in PDF or KiCad/EasyEDA native format; BOM; Gerbers. These do not need to be under a copyleft licence but must be openly accessible.

### Application Process

Open a GitHub Discussion with the `[Works With]` label. Include:
- Module name and description
- Link to published design files
- EEPROM binary image (or link to `tools/eeprom/` image)
- Photo of the assembled module

A maintainer will review the connector pinout and EEPROM image. Once approved, you will receive a SVG badge to display in your project documentation.

---

## Certified Partner Programme

Commercial partners gain access to pre-release firmware, a named contact for priority bug escalation, and listing in the ALX device database as a certified module.

### Benefits

| Benefit | Works With | Certified Partner |
|---|---|---|
| Community badge | Yes | Yes |
| Device DB listing | Community | Certified |
| Pre-release firmware access | No | 2 weeks ahead of public |
| Priority bug escalation | No | Yes |
| Regression SLA | Best effort | Fixed within 2 minor releases |
| Named contact | No | Yes |
| ALX Store listing | No | Eligible |

### How to Apply

Open a GitHub Discussion with the `[Enterprise]` label, or contact the maintainers via the address listed in `SECURITY.md`. Include your organisation name, product description, and estimated shipping volume.

---

## CE / FCC Regulatory Roadmap

### What the ESP32-P4 Module Covers

The ESP32-P4 module (as shipped by Espressif or by board partners like Waveshare) carries its own FCC ID and CE marking for the radio subsystem (WiFi 6 via the companion C6 co-processor). When your product uses a certified module and does not modify the antenna, the module-level certification covers the radio portion of your CE/FCC submission.

### What the Carrier Board Must Cover

The carrier board as an end product (combining the module, power supply, audio circuits, and enclosure) requires a full device certification filing:

- **CE (EU):** RED (Radio Equipment Directive) for the radio; EMC Directive for the carrier board electronics; Low Voltage Directive if mains-powered. SELV (Safety Extra-Low Voltage) classification for the 12V DC input simplifies the safety assessment.
- **FCC (USA):** Part 15B for the carrier board; Part 15C for intentional radiators if you modify the antenna path.
- **UKCA:** Post-Brexit UK equivalent of CE; process and technical requirements are essentially identical.

### Pre-Scan Recommendation

Before submitting for full certification, conduct a pre-compliance EMC scan at a local EMC pre-scan facility. Common failure modes for ESP32-P4 carrier boards:

- Switching regulator harmonics at 1 MHz and harmonics (buck converter for 5V rail)
- I2S clock edges at 48 kHz harmonics radiating from unshielded cables
- MCLK (22.5 MHz / 24.576 MHz) radiating from the MCLK trace on the mezzanine

Address these before formal submission to avoid costly re-tests.

### SELV Classification

The 12V DC input meets the SELV (Safety Extra-Low Voltage) definition — voltage is below 60V DC and the source is transformer-isolated from mains. SELV circuits do not require creepage/clearance distances appropriate for mains-level circuits, which simplifies the enclosure design and PCB layout for safety compliance.

---

## OSHWA Open-Source Hardware Certification

The ALX Nova hardware design files (schematic, PCB layout, BOM) are published in the repository under an open-source hardware licence. An OSHWA (Open Source Hardware Association) certification application is planned.

OSHWA UID: **[TBD — application pending]**

Once certified, the OSHWA gear logo and UID will be added to the PCB silkscreen of the reference carrier board.

---

## Compliance Checklist Template

Use this checklist for your own product compliance tracking.

- [ ] ESP32-P4 module carries valid FCC ID and CE mark (visible on module label)
- [ ] SELV confirmed: input voltage ≤ 60V DC, transformer-isolated from mains
- [ ] EMC pre-scan completed; identified issues resolved before formal submission
- [ ] Radio antenna not modified from certified module reference design
- [ ] Software Bill of Materials (SBOM) generated listing open-source components and licences
- [ ] GDPR / PII review: confirm device does not persist personally identifiable information (web UI uses session-only cookies; no analytics; MQTT payloads contain only device state)
- [ ] EEPROM v3 format validated (CRC passes) on production sample
- [ ] CE DoC (Declaration of Conformity) drafted with applicable directives and standards
- [ ] FCC SDoC (Supplier's Declaration of Conformity) or FCC ID referenced in user manual
