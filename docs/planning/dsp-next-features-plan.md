# DSP Engine: Next Feature Selection Plan

## Status
- M1 (Stereo Link), M2 (Two-Pass Compressor), N1 (Decimation FIR), N2 (Auto Delay Alignment), N3 (Partitioned Convolution) — ALL COMPLETE
- 682/682 tests passing, firmware builds (RAM 46.6%, Flash 74.7%)
- CPU load: 0.5-3% — significant headroom for new features
- User needs to SELECT which features to implement next

## Tier 1 — Quick Wins (Biggest competitive gaps)

### D1: Noise Gate / Expander
- **Effort**: Low (extend existing compressor code)
- **Impact**: High — essential for PA/live use, eliminates background noise
- **Who has it**: ADAU1467, DBX DriveRack, all pro DSPs
- **Implementation**: Threshold + attack/hold/release + ratio below threshold. Reuse compressor envelope detector.

### B2: Loudness Compensation (Fletcher-Munson / ISO 226)
- **Effort**: Low
- **Impact**: High — boost bass/treble at low volumes per equal-loudness contours
- **Who has it**: Hi-Fi amps, streaming apps
- **Implementation**: Volume-dependent shelving EQ curves from ISO 226 lookup table.

### U1: Tone Controls (Bass / Mid / Treble)
- **Effort**: Very Low
- **Impact**: High — simplest UX win, maps to existing shelving EQ
- **Who has it**: Every consumer amp
- **Implementation**: 3 shelving/peaking filters with fixed frequencies, user-adjustable gain. Maps directly to existing biquad infrastructure.

### C3: Bessel Crossover Filter
- **Effort**: Low (just add coefficient generation)
- **Impact**: Medium — maximally-flat group delay, completes crossover portfolio (alongside existing LR/BW)
- **Who has it**: miniDSP, DBX, Dayton DSP-408
- **Implementation**: Add `dsp_gen_bessel_*` to `dsp_biquad_gen.c`, add to crossover preset list.

### D4: Speaker Protection Limiter
- **Effort**: Medium
- **Impact**: Very High — thermal/excursion modeling prevents driver damage
- **Who has it**: Hypex, Purifi, pro amplifier modules
- **Implementation**: RMS power tracking + peak excursion estimation + thermal model. Auto-limits output when approaching driver limits. Major selling point for active speaker market.

## Tier 2 — Strong Differentiators

### B1: Bass Enhancement (Missing Fundamental / Psychoacoustic Bass)
- **Effort**: Medium
- **Impact**: Very High — illusion of deeper bass on small speakers
- **Who has it**: ADAU1467 (MaxxBass algorithm), Waves
- **Implementation**: Detect fundamental, generate harmonics (2nd/3rd), mix back. Highpass the sub-bass, replace with harmonics. Huge wow factor on small speakers.

### D2: Multi-Band Compressor
- **Effort**: Medium
- **Impact**: High — per-band dynamics for mastering-quality control
- **Who has it**: ADAU1467, DBX DriveRack
- **Implementation**: Crossover split → per-band compressor (reuse existing) → sum. 3-4 bands typical.

### B3: Stereo Width / Mid-Side Processing
- **Effort**: Low
- **Impact**: Medium — spatial control (widen or narrow stereo image)
- **Who has it**: Pro DAWs, high-end processors
- **Implementation**: M = (L+R)/2, S = (L-R)/2, adjust S gain, decode back. Simple matrix math.

### C4: Baffle Step Correction
- **Effort**: Low
- **Impact**: Medium — automatic shelf filter for speaker boundary compensation
- **Who has it**: Active speaker DSPs
- **Implementation**: First-order shelving filter, frequency determined by baffle width. Valued by DIY speaker builders.

### C5: THD+N Measurement
- **Effort**: Medium
- **Impact**: Medium — total harmonic distortion analysis
- **Who has it**: Lab-grade analyzers, ADAU1467
- **Implementation**: Use existing signal generator (sine) + FFT. Measure fundamental power vs harmonic + noise power. Display as percentage and dB.

## Tier 3 — Advanced / Niche

### D3: AGC (Auto Gain Control)
- **Effort**: Low
- **Impact**: Medium — auto-level for varying input sources
- **Who has it**: ADAU1467, broadcast chains
- **Implementation**: Slow-acting compressor with very long attack/release. Keeps output level consistent across different sources.

### C2: AutoEQ Wizard (Standalone RTA Mic)
- **Effort**: High
- **Impact**: Very High — automated room measurement + correction, #1 requested feature
- **Who has it**: DBX DriveRack, Dirac Live, Audyssey
- **How it works on ALX Nova**:
  - Connect any analog measurement mic (e.g., Dayton EMM-6) to PCM1808 ADC input
  - ALX Nova plays pink noise via DAC → amp → speakers
  - ADC captures room response, FFT analyzes frequency curve
  - Algorithm computes inverse EQ, auto-loads into PEQ bands
  - No laptop or USB mic needed — fully standalone
- **Already have**: Signal gen, dual ADC, FFT, PEQ bands, REW APO import
- **New work**: RTA averaging, target curve comparison, auto-EQ fitting algorithm
- **Note**: REW import via APO parser already works today for users who prefer laptop-based measurement

### A1: Feedback Suppression (AFS)
- **Effort**: High
- **Impact**: High — auto-detect and notch howling frequencies, critical for PA
- **Who has it**: ADAU1467 (640-tap), DBX DriveRack
- **Implementation**: Continuous FFT monitoring, detect sustained narrow peaks, auto-deploy notch filters.

### U2: Interpolation (Upsampling)
- **Effort**: Medium
- **Impact**: Low — complement to existing decimation (N1)
- **Implementation**: Insert zeros + lowpass FIR. Reverse of decimation path.

### U3: Algorithmic Reverb
- **Effort**: Medium
- **Impact**: Low — Freeverb/Schroeder reverb, niche use for amplifier
- **Implementation**: Comb filters + allpass filters. Fun but niche.

## Decision Needed

Select features to implement. Options:
1. **All of Tier 1** — 5 features, mostly low effort, fills biggest competitive gaps
2. **Tier 1 + selected Tier 2** — e.g., add Bass Enhancement and/or Multi-Band Compressor
3. **Cherry-pick from any tier** — mix and match based on priorities
4. **Focus on AutoEQ** — high effort but highest impact single feature

Resume this conversation by saying: "Let's continue with the DSP feature selection from plans/dsp-next-features-plan.md"
