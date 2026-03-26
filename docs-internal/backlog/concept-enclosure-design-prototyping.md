# Concept: Enclosure Design & Prototyping

| Field | Value | Description |
|---|---|---|
| Workflow | `raw` | raw / draft / ready / in-progress / done / archived |
| Priority | `---` | high / medium / low |
| Effort | `---` | S / M / L / XL |
| Success KPI | `---` | Measurable outcome, e.g. "3 boards detected in <2s" |
| Sources | Enclosure creation.m4a | Auto-populated by pipeline |
| Transcripts | [enclosure-design-prototyping-transcripts.md](transcripts/enclosure-design-prototyping-transcripts.md) | Auto-populated by pipeline |
| Audio | [`inbox/processed/`](inbox/processed/) | Auto-populated by pipeline |
| Last updated | 2026-03-26 | YYYY-MM-DD |

## Problem / Opportunity

The ALX Nova Controller 2 needs a physical enclosure to be a complete product. A half-rack form factor would allow the unit to function as a standalone device or be rack-mounted in professional audio environments. Rapid prototyping is essential to iterate on fit, thermal management, and connector placement before committing to production tooling.

## Sub-topics

### Half-rack enclosure with 3D printing for rapid iteration

#### What we know
- Target form factor is half-rack size (typically ~9.5" / 241mm wide, 1U height)
- Must support both standalone desktop use and rack-mounting (with optional rack ears or dual-unit bracket)
- 3D printing is the leading candidate for rapid prototyping
- Once the design is finalized, production will be outsourced to manufacturing partners for volume runs
- The enclosure must accommodate the ESP32-P4 dev kit, carrier board, and mezzanine expansion slot

#### What needs research
- Exact internal dimensions required to fit the carrier PCB, mezzanine daughter board, and all connectors
- Which 3D printing materials are suitable for an audio enclosure (PLA vs PETG vs ABS vs nylon for thermal, EMI, and structural requirements)
- Whether SLA resin printing offers better surface finish for front panel aesthetics vs FDM
- Alternative rapid prototyping methods beyond 3D printing (laser-cut acrylic/aluminum, CNC-milled aluminum, sheet metal bending services)
- Thermal management requirements — does the ESP32-P4 or any DAC/ADC need ventilation slots or heatsinking
- Front panel layout: display cutout (ST7735S 128x160), rotary encoder, status LEDs, power button
- Rear panel layout: I2S/I2C expansion connectors, USB-C, Ethernet, power input, audio I/O jacks
- EMI/RFI shielding considerations for audio-grade enclosure (conductive paint, metal liner, or full metal enclosure)
- Cost comparison: 3D printed prototypes vs low-volume CNC aluminum vs sheet metal for first 10-50 units
- Rack-mount compatibility: standard half-rack bracket dimensions and mounting hole patterns

## Action Items

- [ ] Research half-rack enclosure dimensions and standards for pro audio equipment. Document standard 1U half-rack width/height/depth specifications, common mounting bracket patterns, and ventilation best practices. Include at least 3 reference products (e.g., Grace Design, RME, Audient half-rack units) with their dimensions. Write findings to docs-internal/backlog/research/half-rack-enclosure-standards.md
- [ ] Measure and document the physical dimensions of the ALX Nova Controller 2 PCB assembly including the ESP32-P4 dev kit, carrier board, mezzanine connector height clearance, and all external connectors (USB-C, Ethernet, audio jacks, I2C headers). Create a dimensioned sketch or table of minimum internal enclosure clearances. Write to docs-internal/backlog/research/enclosure-internal-dimensions.md
- [ ] Research 3D printing materials and methods suitable for electronic enclosures. Compare FDM (PLA, PETG, ABS, ASA) vs SLA resin vs SLS nylon on dimensions: structural rigidity, thermal tolerance, surface finish, EMI shielding capability, cost per unit, and print time. Include recommendations for prototype vs small-batch production. Write findings to docs-internal/backlog/research/3d-printing-materials-comparison.md
- [ ] Research alternative rapid prototyping methods beyond 3D printing for enclosure production. Evaluate laser-cut acrylic with fasteners, CNC-milled aluminum (e.g., PCBWay, Xometry), sheet metal bending services (e.g., SendCutSend, OSH Cut), and injection molding minimum order quantities. Compare lead time, cost at 1/10/50/100 units, and finish quality. Write findings to docs-internal/backlog/research/enclosure-manufacturing-methods.md
- [ ] Design a preliminary front and rear panel layout for the ALX Nova enclosure. Front panel must include cutouts for: ST7735S 128x160 TFT display, rotary encoder with push button, status LED, and power switch. Rear panel must include: USB-C port, Ethernet jack, DC power input, and mezzanine expansion connector access. Produce a dimensioned layout diagram or CAD-ready specification. Write to docs-internal/backlog/research/enclosure-panel-layout.md
