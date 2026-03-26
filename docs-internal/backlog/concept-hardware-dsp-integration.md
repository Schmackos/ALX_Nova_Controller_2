# Concept: Hardware DSP Integration

| Field | Value |
|---|---|
| Workflow | `raw` |
| Priority | `---` |
| Effort | `---` |
| Success KPI | `---` |
| Sources | Hardware DSP.m4a, Analog devices adc and dac proprietary tooling needed.m4a, Lower bar for Analog Devices DSP.m4a |
| Transcripts | [hardware-dsp-integration-transcripts.md](transcripts/hardware-dsp-integration-transcripts.md) |
| Audio | [`inbox/processed/`](inbox/processed/) |
| Last updated | 2026-03-26 |

## Problem / Opportunity

The ALX Nova platform currently integrates ADCs and DACs with a software DSP pipeline but lacks support for dedicated hardware DSP chips. Adding hardware DSP support (e.g., Analog Devices ADAU1467) would give users a choice between software and hardware signal processing, enabling professional-grade audio processing offloaded from the ESP32-P4. A key differentiator would be driving these DSPs natively from the platform without requiring proprietary tools like SigmaStudio, dramatically lowering the barrier to entry for both community and enterprise users.

## Sub-topics

### Dedicated DSP chip support (ADAU1467 etc.)

#### What we know
- The platform has a working software DSP pipeline integrated with the ADC/DAC audio chain
- Hardware DSP chips like the Analog Devices ADAU1467 are candidates for integration as dedicated processing units
- Integration would introduce a selection model: software DSP vs. hardware DSP per pipeline stage
- Both community/hobbyist and enterprise/professional users could benefit from offloaded hardware DSP

#### What needs research
- Which ADAU-series DSPs are best suited for the mezzanine form factor and I2S/TDM pipeline architecture?
- What is the I2S/TDM interconnect topology between ESP32-P4, ADC, hardware DSP, and DAC?
- How does the HAL framework need to evolve to model a DSP device (new device type, capability flags, pipeline bridge role)?
- What are the latency and sample rate implications of routing audio through an external DSP vs. the software pipeline?
- Can software DSP and hardware DSP coexist in the same pipeline (e.g., software pre-processing → hardware DSP → DAC)?

### Analog Devices chipset tooling requirements

#### What we know
- Analog Devices is currently unrepresented in the ALX Nova platform (ESS Sabre and Cirrus Logic are supported)
- Analog Devices DSPs require SigmaStudio, a proprietary IDE, for programming filter topologies and signal flows
- It is unclear whether Analog Devices ADCs and DACs also require SigmaStudio or can be controlled via standard I2C registers
- ESS Sabre and Cirrus Logic devices are successfully driven via native I2C commands in the current platform

#### What needs research
- Do Analog Devices ADCs (e.g., AD1938, ADAU1977) and DACs (e.g., AD1934, ADAU1962) require SigmaStudio, or are they fully I2C/SPI register-controllable?
- What is the register map availability and documentation quality for Analog Devices audio converters vs. their DSP products?
- Are there Analog Devices ADC/DAC families that would be straightforward to add as HAL drivers using the existing generic driver pattern approach?

### SigmaStudio bypass and native ADAU control

#### What we know
- SigmaStudio generates a program binary and parameter RAM image that gets loaded onto ADAU DSPs at boot
- The SigmaStudio export format (`.params`, `.hex`, register writes) is partially documented by Analog Devices
- The goal is to remove the SigmaStudio dependency for standard use cases (EQ, crossover, delay, gain) while acknowledging power users may still need it for advanced signal flow design
- Eliminating proprietary tooling requirements would be a strong value proposition for the ALX Nova platform

#### What needs research
- Has the SigmaStudio compiler output format been reverse-engineered sufficiently to generate DSP programs programmatically?
- Can the ADAU1467 (or similar) be loaded with pre-built filter topologies at boot via I2C/SPI without SigmaStudio?
- Are there open-source projects or community efforts that have already implemented SigmaStudio-free ADAU programming?
- What is the minimum viable feature set for native DSP control (PEQ, crossover, delay, volume) and how complex is the corresponding SigmaDSP program structure?
- Could a hybrid approach work — platform ships pre-compiled DSP topologies for common use cases, with SigmaStudio available as an optional advanced tool?
- What are the licensing and IP implications of embedding SigmaDSP program generation into an open-source platform?

## Action Items

- [ ] Research Analog Devices ADC/DAC product lines (AD1938, ADAU1977, AD1934, ADAU1962) to determine if they can be controlled via standard I2C/SPI registers without SigmaStudio, and assess feasibility of adding HAL drivers using existing generic driver patterns
- [ ] Investigate the ADAU1467 and ADAU1452 SigmaDSP architecture — document the boot sequence, program/parameter RAM loading process, and whether pre-compiled DSP images can be loaded via I2C/SPI from the ESP32-P4
- [ ] Search for open-source projects and community reverse-engineering efforts around SigmaStudio export formats and native ADAU DSP programming (e.g., sigmadsp Linux driver, adau1467 open-source loaders)
- [ ] Define the HAL device model for a hardware DSP: new device type, capability flags (e.g., HAL_CAP_HW_DSP), pipeline bridge integration points, and the software DSP vs. hardware DSP selection mechanism in the audio pipeline
- [ ] Evaluate mezzanine board design requirements for an ADAU1467-based DSP expansion card including I2S/TDM routing, I2C control bus assignment, power requirements, and physical pin mapping to the 16-pin mezzanine connector
- [ ] Prototype a minimal native DSP control flow: ESP32-P4 boots ADAU1467 with a pre-built PEQ + crossover topology, and the web UI adjusts filter parameters at runtime via I2C parameter writes — no SigmaStudio in the loop
