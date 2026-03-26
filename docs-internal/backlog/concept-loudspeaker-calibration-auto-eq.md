# Concept: Loudspeaker Calibration & Auto-EQ Platform

| Field | Value | Description |
|---|---|---|
| Workflow | `raw` | raw / draft / ready / in-progress / done / archived |
| Priority | `---` | high / medium / low |
| Effort | `---` | S / M / L / XL |
| Success KPI | `---` | Measurable outcome, e.g. "3 boards detected in <2s" |
| Sources | Loudspeaker Calibration Platform.m4a, Auto EQ Software Feature Part 2 Basic and advacned.m4a | Auto-populated by pipeline |
| Transcripts | [loudspeaker-calibration-auto-eq-transcripts.md](transcripts/loudspeaker-calibration-auto-eq-transcripts.md) | Auto-populated by pipeline |
| Audio | [`inbox/processed/`](inbox/processed/) | Auto-populated by pipeline |
| Last updated | 2026-03-26 | YYYY-MM-DD |

## Problem / Opportunity

Most loudspeaker setups have frequency response shortcomings that require manual DSP tuning knowledge to correct — a barrier for the majority of users. The ALX Nova platform can differentiate itself by combining calibrated microphone measurement, an on-device auto-EQ engine, and a cloud-hosted community calibration database, enabling users to instantly apply optimized speaker profiles without expertise. This creates a calibration ecosystem tied to ALX hardware, driving both community engagement and a monetization incentive that competitors like DIRAC cannot match due to their lack of cross-system calibration sharing.

## Sub-topics

### Measurement pipeline and cloud calibration database

#### What we know
- A calibrated USB microphone (e.g., miniDSP UMIK) connected to the ALX Nova will be used to measure loudspeaker frequency response
- Measurements must be captured in a calibrated, normalized way to ensure consistency across setups
- Frequency response graphs will be plotted on-device and/or via the web UI
- Measurement data — with speaker model and details attached — will be uploaded to the ALX platform's online database
- Other users with the same speakers can browse and download pre-made calibration presets from the community database
- Downloaded presets apply DSP corrections automatically, flattening frequency response or applying user-specific target curves
- The concept is similar to DIRAC/REW room correction but adds a shared community database across systems and setups

#### What needs research
- Whether the measurement and analysis pipeline can be done entirely on-platform or requires REW integration for the measurement/analysis stage
- USB microphone interface requirements on ESP32-P4 (USB host mode, audio class compliance, driver support)
- Calibration file format for microphone correction curves (e.g., miniDSP .cal files)
- Normalization methodology to ensure cross-setup measurement consistency (SPL reference, distance, stimulus signal type)
- Cloud platform architecture for the calibration database (API, storage, speaker model taxonomy, search/matching)
- Privacy and data quality considerations for community-uploaded calibration data

### Two-tier auto-EQ and community calibration sharing

#### What we know
- REW software provides auto-EQ functionality that should be replicated or integrated
- Two-tier approach planned: (1) a native slimmed-down auto-EQ running directly on the ALX platform for simple use cases, and (2) REW integration for power users needing fine-grained adjustments
- Calibrated auto-EQ output files are uploaded back to the ALX platform for community sharing
- Users without a calibrated microphone can download community-shared calibration files for their speaker setup
- Users with a calibrated microphone can run auto-EQ directly to correct their measured speaker deficiencies
- Long-term vision: the platform evolves into a calibration hub where users find profiles for devices they own or have built from off-the-shelf components
- The requirement for ALX platform hardware to perform calibration creates a competitive differentiator and monetization incentive

#### What needs research
- Feasibility of implementing a native auto-EQ algorithm on the ESP32-P4 (computational requirements, latency, memory footprint)
- Whether auto-EQ computation should run on-device, in the web UI (client-side), or on a cloud backend
- REW integration path — file import/export formats, API availability, or whether REW can be driven programmatically
- Target curve options (Harman, flat, user-defined) and how to expose these in the UI
- PEQ filter generation algorithm — translating a measured frequency response delta into a set of parametric EQ biquad filters compatible with the existing DSP pipeline
- Incentive design for encouraging users to share calibration data back to the community platform

## Action Items

- [ ] Research USB audio class support on ESP32-P4. Determine if the ESP32-P4 USB host stack supports USB Audio Class 1.0/2.0 devices (specifically calibrated measurement microphones like miniDSP UMIK-1/UMIK-2). Document driver requirements, sample rate/bit depth capabilities, and any firmware modifications needed. Write findings to `docs-internal/backlog/research/usb-audio-microphone-support.md`
- [ ] Research auto-EQ algorithms suitable for embedded or web-based execution. Survey open-source implementations (e.g., AutoEq project, REW's auto-EQ approach, parametric EQ optimizer algorithms) and evaluate whether a PEQ filter set can be generated from a frequency response measurement entirely on-platform (ESP32-P4 or browser-side JS). Document algorithm options, computational complexity, and memory requirements. Write findings to `docs-internal/backlog/research/auto-eq-algorithm-feasibility.md`
- [ ] Research REW integration options for power-user workflow. Investigate REW's file formats (measurement export, EQ filter import/export), whether REW exposes an API or CLI for automation, and what the handoff workflow would look like between ALX Nova and REW. Document the integration surface and user experience flow. Write findings to `docs-internal/backlog/research/rew-integration-options.md`
- [ ] Design the loudspeaker measurement stimulus and capture pipeline. Define the signal chain from test signal generation (swept sine, pink noise, MLS) through USB mic capture to FFT/frequency response computation. Specify normalization methodology (SPL reference level, measurement distance, averaging, windowing) to ensure cross-setup consistency. Include microphone calibration file (.cal) application. Write design to `docs-internal/backlog/research/measurement-pipeline-design.md`
- [ ] Design the cloud calibration database schema and API. Define the data model for speaker calibration profiles (speaker make/model, measurement conditions, frequency response data, PEQ filter coefficients, target curve used, contributor metadata). Specify the REST API for upload, search, and download. Address data quality, moderation, and versioning. Write design to `docs-internal/backlog/research/calibration-database-design.md`
- [ ] Prototype a web UI frequency response graph component. Build a standalone frequency response plot (amplitude vs. frequency, 20Hz-20kHz log scale) that can display measured response, target curve, and corrected response overlays. Use the existing ALX web UI patterns (inline SVG, vanilla JS, WebSocket data feed). Write prototype to `web_src/js/` following existing conventions and document the approach in `docs-internal/backlog/research/frequency-response-ui-prototype.md`
