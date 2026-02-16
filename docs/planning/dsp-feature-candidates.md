# DSP Feature Candidates

## What We Already Have
- Biquad IIR (all RBJ types: LPF, HPF, BPF, notch, peaking EQ, low/high shelf, allpass)
- FIR filters (pool-allocated, REW/APO/miniDSP import, WAV IR loading)
- Per-channel gain, polarity inversion, mute
- Delay lines (PSRAM-backed, per-channel)
- Compressor (attack/release/threshold/ratio/makeup gain)
- Hard limiter (with gain reduction metering)
- Crossover presets (LR2/LR4/LR8, Butterworth, bass management)
- 4x4 routing matrix (SIMD-accelerated)
- Double-buffered config (glitch-free swap)
- Global bypass + per-stage enable
- REW / Equalizer APO / miniDSP import/export
- 1024-pt FFT spectrum (radix-4, 6 window types, SNR/SFDR)
- VU metering, peak hold, waveform display
- Signal generator (sine, square, noise, sweep)
- CPU load monitoring

---

## Candidate Features To Implement

### Category A: DSP Processing Stages (new stage types for the pipeline)

| # | Feature | What It Does | Difficulty | ESP-DSP Support | Value |
|---|---------|-------------|------------|-----------------|-------|
| A1 | **Linkwitz Transform** | Reshapes sealed-box speaker rolloff to simulate a different alignment (e.g. make a sealed sub behave like ported). Two biquad stages with specific pole/zero mapping. Essential for subwoofer tuning. | Easy | Yes (biquad coeff gen) | High - standard in miniDSP, CamillaDSP |
| A2 | **Noise Gate / Expander** | Attenuates signal below a threshold. Reduces noise floor when no audio is playing. Parameters: threshold, ratio, attack, release, hold. Complement to compressor. | Medium | No (custom logic) | Medium - pro audio standard (dbx, Behringer) |
| A3 | **Soft Limiter / Clipper** | Polynomial or tanh saturation curve instead of hard clamp. Sounds more natural at the clipping point, reduces harsh distortion artifacts. | Easy | No (math only) | Medium - CamillaDSP has it, sounds better than hard clip |
| A4 | **Loudness Compensation** | Fletcher-Munson equal-loudness contour. Boosts bass & treble at low volumes to maintain perceived frequency balance. Tied to a volume reference level. | Medium | Yes (biquad coeff gen) | High - CamillaDSP, hi-fi receivers all have this |
| A5 | **Graphic EQ** | N-band EQ with fixed center frequencies (e.g. 10-band or 31-band octave/third-octave). Each band is a peaking biquad at constant-Q. Simpler UI than parametric for quick adjustments. | Easy | Yes (biquad coeff gen) | Medium - miniDSP, every pro processor has it |
| A6 | **Dither** | Noise-shaped quantization for bit-depth reduction (24→16 bit). Improves perceived dynamic range when outputting to lower-resolution DACs. Multiple noise shaping curves. | Medium | No (custom logic) | Low - niche, only matters for 16-bit output paths |
| A7 | **Auto Gain Control (AGC)** | Slow-acting automatic level normalization. Adjusts gain to maintain consistent average output level across different sources. Like a compressor but with very long time constants. | Medium | No (custom logic) | Medium - useful for background music, podcasts |
| A8 | **Tilt EQ** | Single knob that tilts the entire frequency response around a pivot point (e.g. 1kHz). Brightens or warms the sound with one control. Cascade of shelving filters. | Easy | Yes (biquad coeff gen) | Low-Medium - CamillaDSP has it, audiophile favorite |
| A9 | **Gain Ramp / Volume Fade** | Smooth gain transitions over 5-50ms to eliminate audible clicks/pops on gain changes (web UI, MQTT, encoder). Linear or exponential per-sample interpolation between old and new gain. | Easy | Yes (`dsps_mulc_f32`) | High - currently every gain change clicks |
| A10 | **Dynamic EQ** | PEQ whose gain is modulated by signal level via envelope follower. Tame harshness only when loud, or boost bass only at low levels. More transparent than multiband compression. | Medium | Partial (biquad + envelope) | Medium-High - Behringer DEQ2496, pro standard |
| A11 | **Multiband Compressor** | Split into N bands via crossover, compress each independently, sum back. E.g. compress bass without affecting treble. Requires sub-pipeline concept or multi-channel trick. | Hard | Yes (crossover + compressor) | Medium - pro/PA feature |
| A12 | **FFT Convolution (Long FIR)** | Overlap-add FFT convolution for 4096+ tap FIR filters. Current 256-tap limit is too short for room correction. PSRAM makes memory feasible; CPU is the constraint. | Hard | Yes (radix-4 FFT) | Medium-High - enables real room correction |

### Category B: Analysis & Measurement (new analysis capabilities)

| # | Feature | What It Does | Difficulty | ESP-DSP Support | Value |
|---|---------|-------------|------------|-----------------|-------|
| B1 | **Cross-Correlation (Dual ADC Delay)** | Measures time delay between ADC1 and ADC2 using cross-correlation. Detects phase alignment issues, measures propagation delay between two mic positions or driver outputs. | Medium | Yes (`dsps_ccorr_f32`) | High - unique dual-ADC capability |
| B2 | **Impulse Response Measurement** | Inject delta/step signal through DSP chain and capture output. Measures the actual filter response including all biquad/FIR stages. Verify DSP config matches intent. | Easy | Yes (`dsps_d_gen_f32`, `dsps_h_gen_f32`) | Medium - great for diagnostics |
| B3 | **Transfer Function / Frequency Response Plot** | Compute and display the theoretical frequency response of the entire DSP chain (magnitude + phase). Sweep biquad coefficients through frequency range. No audio needed. | Medium | Yes (biquad + FFT) | High - miniDSP and CamillaDSP both show this |
| B4 | **RTA (Real-Time Analyzer)** | Continuous 1/3-octave or 1/6-octave bar display of live spectrum. Different from FFT spectrum — uses standardized octave bands with proper weighting. | Easy | Yes (FFT + windowing) | Medium - pro audio standard tool |
| B5 | **Crest Factor / LUFS Metering** | Broadcast-standard loudness measurement (EBU R128 / ITU-R BS.1770). K-weighted, gated. Shows integrated, short-term, momentary loudness + loudness range. | Hard | Partial (needs custom weighting) | Low-Medium - broadcast/podcast use case |

### Category C: Infrastructure & UX (pipeline enhancements)

| # | Feature | What It Does | Difficulty | ESP-DSP Support | Value |
|---|---------|-------------|------------|-----------------|-------|
| C1 | **DSP Config Presets** | Save/recall multiple complete DSP configurations (e.g. "Music", "Movie", "Night Mode", "Room Correction"). Quick-switch between profiles. Store on LittleFS. | Medium | N/A | High - miniDSP has 4 presets, everyone expects this |
| C2 | **Per-Channel Level Meters (post-DSP)** | Show signal level after each DSP stage or at least at output of each channel. Currently we only meter the input. Helps diagnose gain staging issues. | Medium | Yes (mulc for RMS) | High - every pro DSP shows I/O levels |
| C3 | **Decimation FIR for Waveform** | Replace manual waveform downsampling with ESP-DSP's `dsps_fird_f32`. Proper anti-aliasing filter before decimation. Better waveform display quality. | Easy | Yes (`dsps_fird_f32`) | Low - current display works fine |
| C4 | **Subsample Delay Precision** | Fractional sample delay using allpass interpolation. Currently delay is integer samples only. Enables sub-sample time alignment (fractions of ~20μs at 48kHz). | Medium | Yes (allpass biquad) | Low-Medium - CamillaDSP has it, matters for precise alignment |
| C5 | **DSP Chain Reordering via Drag-Drop** | Web UI drag-and-drop to reorder DSP stages within a channel. Currently uses API-based reorder but UI could be more intuitive. | Medium | N/A | Medium - UX improvement |
| C6 | **A/B Comparison Toggle** | Quick toggle between two DSP configs to audition changes. Stores "before" snapshot, lets you flip back and forth. Level-matched comparison. | Medium | N/A | Medium - useful for tuning |

### Category D: Unused ESP-DSP Functions Worth Leveraging

| # | Feature | What It Does | Difficulty | ESP-DSP Function | Value |
|---|---------|-------------|------------|------------------|-------|
| D1 | **SIMD Vector Subtract** | Use `dsps_sub_f32` for differential signal processing (L-R extraction, sidechain comparison between ADC1/ADC2). Currently no subtraction in pipeline. | Easy | `dsps_sub_f32` | Low - niche use case |
| D2 | **Vectorized Sqrt for RMS** | Replace per-sample `sqrtf()` in RMS computation with SIMD `dsps_sqrt_f32`. Minor CPU savings. | Easy | `dsps_sqrt_f32` | Low - micro-optimization |
| D3 | **DC Offset Removal** | Use `dsps_addc_f32` to subtract measured DC component from audio buffer. Some ADCs have inherent DC offset. | Easy | `dsps_addc_f32` | Low-Medium - correctness improvement |
| D4 | **Convolution (Block IR)** | Use `dsps_conv_f32` for offline/one-shot impulse response analysis. Not real-time streaming (we already use `dsps_fir_f32` for that), but useful for filter verification. | Easy | `dsps_conv_f32` | Low - diagnostic tool |
| D5 | **Console Spectrum View** | Use `dsps_view_spectrum` for serial-only debugging. ASCII art spectrum on serial monitor without needing web UI. | Easy | `dsps_view`, `dsps_view_spectrum` | Low - debug convenience |

---

## Recommendation Priority

### Tier 1 — High impact, reasonable effort
- **A9** Gain Ramp / Volume Fade (Easy — eliminates audible clicks on every gain change)
- **A1** Linkwitz Transform (Easy — single most-requested DIY audio feature)
- **C1** DSP Config Presets (Medium — everyone expects save/recall profiles)
- **A4** Loudness Compensation (Medium — Fletcher-Munson, high perceived value)
- **B3** Transfer Function Plot (Medium — visualize DSP chain response without audio)
- **C2** Per-Channel Output Meters (Medium — diagnose gain staging issues)

### Tier 2 — Good value, moderate effort
- **A3** Soft Limiter (Easy — sounds better than hard clip)
- **A5** Graphic EQ (Easy — simpler UI than PEQ for quick adjustments)
- **A2** Noise Gate / Expander (Medium — complements smart sensing)
- **A10** Dynamic EQ (Medium — more transparent than multiband compression)
- **B1** Cross-Correlation (Medium — unique dual-ADC delay measurement)
- **B4** RTA 1/3-octave (Easy — pro audio standard analysis)

### Tier 3 — Nice to have
- **A8** Tilt EQ (Easy)
- **A7** AGC (Medium)
- **A12** FFT Convolution / Long FIR (Hard — enables real room correction)
- **B2** Impulse Response Measurement (Easy)
- **C4** Subsample Delay (Medium)
- **C5** Drag-Drop Reorder (Medium)
- **C6** A/B Comparison (Medium)
- **D3** DC Offset Removal (Easy)

### Tier 4 — Marginal / niche
- **A6** Dither (only matters for 16-bit output)
- **A11** Multiband Compressor (Hard, pro/PA only)
- **B5** LUFS Metering (Hard, broadcast niche)
- **C3** Decimation FIR (current waveform is fine)
- **D1-D2, D4-D5** ESP-DSP micro-optimizations
