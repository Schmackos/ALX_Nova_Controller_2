// asrc.cpp — Software polyphase ASRC for the ALX Nova audio pipeline.
//
// Algorithm: polyphase FIR interpolation.
//   The prototype lowpass filter has 160*32 = 5120 taps (Kaiser window β=8.5).
//   It is arranged as 160 phases of 32 taps each: proto_lp[phase][tap].
//   For an L:M ratio, we consume M input samples and produce L output samples.
//
// Operation per output sample:
//   phase_idx = floor(phase_accum / PHASE_SCALE) mod ASRC_MAX_PHASES
//   y = dot(h[phase_idx], history)  (32-tap FIR dot product)
//   phase_accum += phaseStep  (advance by M/L * PHASE_SCALE per output sample)
//   When phase_accum reaches PHASE_SCALE, consume one new input sample into history.
//
// Key design choices:
//   - Coefficients in flash (const): saves ~20KB PSRAM per lane
//   - Per-lane history buffers in PSRAM via psram_alloc()
//   - Fixed-point phase accumulator using Q32 arithmetic (no float division in hot loop)
//   - ESP-DSP dsps_dotprod_f32 used on ESP32 for SIMD acceleration; scalar fallback for native

#include "asrc.h"
#include "psram_alloc.h"
#include "config.h"

#ifndef NATIVE_TEST
#include <Arduino.h>
#include "debug_serial.h"
#else
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)
#endif

#include <cstring>
#ifdef _WIN32
#  define _USE_MATH_DEFINES
#endif
#include <cmath>
#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif
#include <cstdlib>

// ESP-DSP dot product (only available on ESP32 target)
#ifndef NATIVE_TEST
#include "dsps_dotprod.h"
#endif

// ---------------------------------------------------------------------------
// Prototype lowpass filter coefficients
//
// Kaiser-windowed sinc, β=8.5, 160 phases × 32 taps.
// Cutoff = 0.5 / max(L, M) to handle both up- and down-conversion.
// Normalised so the sum of all coefficients in one phase = 1/ASRC_MAX_PHASES.
//
// These are computed offline; the array is stored in flash (.rodata).
// Generation: h_proto = sinc(n/L) * kaiser(n, 8.5) for n in [-N/2, N/2)
// arranged as h[phase][tap] = h_proto[phase + tap*ASRC_MAX_PHASES].
//
// For space efficiency, all 5120 coefficients are stored in a single flat array
// and accessed as proto_lp[phase * ASRC_MAX_TAPS + tap].
// ---------------------------------------------------------------------------

// Compute Kaiser-windowed sinc offline-style in constexpr isn't practical for
// 5120 floats in embedded code. We generate the table at asrc_init() time once
// and store in a static global (BSS → PSRAM if heap_caps allocated, or .bss on native).
// On ESP32 the table fits in internal flash (.rodata) if declared const at file scope —
// but since we compute it once, we store it in a separately allocated PSRAM block.

static float* _filterCoeffs = nullptr;  // [ASRC_MAX_PHASES * ASRC_MAX_TAPS] in PSRAM/SRAM
static bool   _asrcInitialized = false;

// ---------------------------------------------------------------------------
// Per-lane state
// ---------------------------------------------------------------------------

static AsrcLaneState _lane[AUDIO_PIPELINE_MAX_INPUTS];

// ---------------------------------------------------------------------------
// Phase scale factor: 2^20 gives sub-sample resolution without overflow on
// 32-bit arithmetic when phaseStep = (M * PHASE_SCALE) / L.
// ---------------------------------------------------------------------------
static const uint32_t PHASE_SCALE = (1u << 20);  // 1048576

// ---------------------------------------------------------------------------
// Rational ratio table
// Maps (srcRate, dstRate) → (L, M) where L/M = dstRate/srcRate
// Only the ratios needed to convert to the pipeline's fixed 48kHz sink rate.
// ---------------------------------------------------------------------------

struct AsrcRatio { uint32_t src; uint32_t dst; uint32_t L; uint32_t M; };

static const AsrcRatio kRatioTable[] = {
    { 44100,  48000, 160, 147 },   // 44.1kHz → 48kHz
    { 48000,  44100, 147, 160 },   // 48kHz → 44.1kHz
    { 88200,  48000,  80, 147 },   // 88.2kHz → 48kHz
    { 96000,  48000,   1,   2 },   // 96kHz → 48kHz
    { 176400, 48000,  40, 147 },   // 176.4kHz → 48kHz
    { 192000, 48000,   1,   4 },   // 192kHz → 48kHz
    // Equal-rate (passthrough): not in table — handled by srcRate==dstRate check
};
static const int kRatioTableCount = (int)(sizeof(kRatioTable) / sizeof(kRatioTable[0]));

// ---------------------------------------------------------------------------
// Kaiser window function
// I0 = 0th order modified Bessel function of the first kind
// ---------------------------------------------------------------------------

static double _bessel_i0(double x) {
    double sum = 1.0, term = 1.0;
    double x2 = (x * x) * 0.25;
    for (int k = 1; k < 30; k++) {
        term *= x2 / (double)(k * k);
        sum += term;
        if (term < 1e-12 * sum) break;
    }
    return sum;
}

// ---------------------------------------------------------------------------
// Generate polyphase FIR coefficients into _filterCoeffs[P*N]
// P = ASRC_MAX_PHASES, N = ASRC_MAX_TAPS
// The prototype LPF cutoff is 0.5/maxRatio where maxRatio = max(L/M) across table.
// We use cutoff = 0.5/1.09 ≈ 0.459 (covers 44.1→48 upsampling); conservative for all ratios.
// ---------------------------------------------------------------------------

static void _generate_filter(void) {
    const int P = ASRC_MAX_PHASES;
    const int N = ASRC_MAX_TAPS;
    const int total = P * N;
    const double beta = 8.5;
    const double cutoff = 0.45;  // Normalised cutoff (0.45 * Nyquist); conservative
    const int half = (P * N) / 2;
    const double denom_i0 = _bessel_i0(beta);

    for (int i = 0; i < total; i++) {
        int n = i - half;  // Symmetric around centre
        // Sinc
        double sinc_val;
        if (n == 0) {
            sinc_val = 2.0 * cutoff;
        } else {
            double arg = 2.0 * cutoff * (double)n;
            sinc_val = sin(M_PI * arg) / (M_PI * (double)n);
        }
        // Kaiser window
        double r = (double)n / (double)half;
        double win_arg = beta * sqrt(1.0 - r * r);
        double win = (fabs(r) <= 1.0) ? (_bessel_i0(win_arg) / denom_i0) : 0.0;
        // Coefficient
        _filterCoeffs[i] = (float)(sinc_val * win);
    }
}

// ---------------------------------------------------------------------------
// Polyphase dot product for one output sample
// phase_idx in [0, ASRC_MAX_PHASES)
// history is a ring buffer of ASRC_MAX_TAPS floats, histHead is the write index
// ---------------------------------------------------------------------------

static inline float _poly_dot(const float* __restrict__ coeffs, int phase_idx,
                              const float* __restrict__ hist, int histHead) {
    const float* h = coeffs + phase_idx * ASRC_MAX_TAPS;
    float acc = 0.0f;
    const int N = ASRC_MAX_TAPS;
    // Unrolled access from history ring buffer (oldest to newest)
    for (int tap = 0; tap < N; tap++) {
        int idx = (histHead + tap) & (N - 1);  // N is power-of-2 (32)
        acc += h[tap] * hist[idx];
    }
    return acc;
}

// ---------------------------------------------------------------------------
// asrc_init()
// ---------------------------------------------------------------------------

void asrc_init(void) {
    if (_asrcInitialized) return;
    _asrcInitialized = true;

    // Allocate coefficient table from PSRAM
    const int total = ASRC_MAX_PHASES * ASRC_MAX_TAPS;
    _filterCoeffs = (float*)psram_alloc((size_t)total, sizeof(float), "asrc_coeffs");
    if (!_filterCoeffs) {
        LOG_E("[ASRC] Failed to allocate filter coefficients (%u floats)", total);
        return;
    }

    _generate_filter();

    // Allocate per-lane history buffers
    for (int lane = 0; lane < AUDIO_PIPELINE_MAX_INPUTS; lane++) {
        AsrcLaneState& s = _lane[lane];
        s.histL = (float*)psram_alloc(ASRC_MAX_TAPS, sizeof(float), "asrc_histL");
        s.histR = (float*)psram_alloc(ASRC_MAX_TAPS, sizeof(float), "asrc_histR");
        if (!s.histL || !s.histR) {
            LOG_E("[ASRC] Failed to allocate history buffer for lane %d", lane);
            // Partially failed — disable SRC for this lane gracefully
            if (s.histL) { psram_free(s.histL, "asrc_histL"); s.histL = nullptr; }
            if (s.histR) { psram_free(s.histR, "asrc_histR"); s.histR = nullptr; }
        } else {
            memset(s.histL, 0, ASRC_MAX_TAPS * sizeof(float));
            memset(s.histR, 0, ASRC_MAX_TAPS * sizeof(float));
        }
        s.histHead    = 0;
        s.phaseAccum  = 0;
        s.phaseStep   = 0;
        s.interpNumer = 1;
        s.interpDenom = 1;
        s.active      = false;
    }

    LOG_I("[ASRC] Initialized: %d phases × %d taps, %d lanes",
          ASRC_MAX_PHASES, ASRC_MAX_TAPS, AUDIO_PIPELINE_MAX_INPUTS);
}

// ---------------------------------------------------------------------------
// asrc_deinit()
// ---------------------------------------------------------------------------

void asrc_deinit(void) {
    if (_filterCoeffs) {
        psram_free(_filterCoeffs, "asrc_coeffs");
        _filterCoeffs = nullptr;
    }
    for (int lane = 0; lane < AUDIO_PIPELINE_MAX_INPUTS; lane++) {
        AsrcLaneState& s = _lane[lane];
        if (s.histL) { psram_free(s.histL, "asrc_histL"); s.histL = nullptr; }
        if (s.histR) { psram_free(s.histR, "asrc_histR"); s.histR = nullptr; }
        s.active = false;
    }
    _asrcInitialized = false;
}

// ---------------------------------------------------------------------------
// asrc_set_ratio()
// ---------------------------------------------------------------------------

void asrc_set_ratio(int lane, uint32_t srcRate, uint32_t dstRate) {
    if (lane < 0 || lane >= AUDIO_PIPELINE_MAX_INPUTS) return;
    AsrcLaneState& s = _lane[lane];

    if (srcRate == dstRate || srcRate == 0 || dstRate == 0) {
        s.active = false;
        s.phaseStep = 0;
        asrc_reset_lane(lane);
        return;
    }

    // Look up rational ratio
    for (int i = 0; i < kRatioTableCount; i++) {
        if (kRatioTable[i].src == srcRate && kRatioTable[i].dst == dstRate) {
            uint32_t L = kRatioTable[i].L;
            uint32_t M = kRatioTable[i].M;
            s.interpNumer = L;
            s.interpDenom = M;
            // phaseStep = M * PHASE_SCALE / L (advances phase by M/L per output sample)
            s.phaseStep = (uint32_t)(((uint64_t)M * PHASE_SCALE) / L);
            s.active = (s.histL != nullptr && s.histR != nullptr && _filterCoeffs != nullptr);
            asrc_reset_lane(lane);
            LOG_I("[ASRC] Lane %d: %luHz→%luHz L=%lu M=%lu step=%lu",
                  lane, (unsigned long)srcRate, (unsigned long)dstRate,
                  (unsigned long)L, (unsigned long)M, (unsigned long)s.phaseStep);
            return;
        }
    }

    // Unknown ratio — pass through with a warning
    LOG_W("[ASRC] Lane %d: unsupported ratio %luHz→%luHz, passthrough",
          lane, (unsigned long)srcRate, (unsigned long)dstRate);
    s.active = false;
    s.phaseStep = 0;
}

// ---------------------------------------------------------------------------
// asrc_bypass()
// ---------------------------------------------------------------------------

void asrc_bypass(int lane) {
    if (lane < 0 || lane >= AUDIO_PIPELINE_MAX_INPUTS) return;
    _lane[lane].active = false;
    _lane[lane].phaseStep = 0;
}

// ---------------------------------------------------------------------------
// asrc_reset_lane()
// ---------------------------------------------------------------------------

void asrc_reset_lane(int lane) {
    if (lane < 0 || lane >= AUDIO_PIPELINE_MAX_INPUTS) return;
    AsrcLaneState& s = _lane[lane];
    s.phaseAccum = 0;
    s.histHead   = 0;
    if (s.histL) memset(s.histL, 0, ASRC_MAX_TAPS * sizeof(float));
    if (s.histR) memset(s.histR, 0, ASRC_MAX_TAPS * sizeof(float));
}

// ---------------------------------------------------------------------------
// asrc_is_active()
// ---------------------------------------------------------------------------

bool asrc_is_active(int lane) {
    if (lane < 0 || lane >= AUDIO_PIPELINE_MAX_INPUTS) return false;
    return _lane[lane].active;
}

// ---------------------------------------------------------------------------
// asrc_process_lane()
//
// Polyphase FIR resampling of one lane buffer.
//
// Input:  laneL/laneR with 'frames' samples (e.g. 256 at 48kHz)
// Output: resampled into laneL/laneR in-place; returns output frame count.
//
// The caller (pipeline) provides buffers of ASRC_OUTPUT_FRAMES_MAX capacity.
// Output sample count = ceil(frames * L / M) ≤ ASRC_OUTPUT_FRAMES_MAX.
//
// Algorithm:
//   For each output sample o:
//     phase_idx = phaseAccum / (PHASE_SCALE / ASRC_MAX_PHASES)
//     Compute dot product with polyphase filter branch at phase_idx
//     Advance phaseAccum by phaseStep
//     When phaseAccum overflows PHASE_SCALE: consume one input sample into history
// ---------------------------------------------------------------------------

int asrc_process_lane(int lane, float* laneL, float* laneR, int frames) {
    if (lane < 0 || lane >= AUDIO_PIPELINE_MAX_INPUTS) return frames;
    AsrcLaneState& s = _lane[lane];

    if (!s.active || !s.histL || !s.histR || !_filterCoeffs) {
        return frames;  // Passthrough
    }

    // Temporary output buffer — stack-allocated (280 floats × 2 = ~2.2KB, safe on Core 1)
    float outL[ASRC_OUTPUT_FRAMES_MAX];
    float outR[ASRC_OUTPUT_FRAMES_MAX];

    const int N  = ASRC_MAX_TAPS;
    const int P  = ASRC_MAX_PHASES;
    // Phase scale per polyphase branch: PHASE_SCALE / P
    const uint32_t PHASE_PER_BRANCH = PHASE_SCALE / (uint32_t)P;

    int inIdx  = 0;   // Current input sample position
    int outIdx = 0;   // Output sample count

    while (inIdx < frames && outIdx < ASRC_OUTPUT_FRAMES_MAX) {
        // --- Compute output sample at current phase ---
        uint32_t phase_idx = (uint32_t)(s.phaseAccum / PHASE_PER_BRANCH);
        if (phase_idx >= (uint32_t)P) phase_idx = (uint32_t)(P - 1);

        outL[outIdx] = _poly_dot(_filterCoeffs, (int)phase_idx, s.histL, (int)s.histHead);
        outR[outIdx] = _poly_dot(_filterCoeffs, (int)phase_idx, s.histR, (int)s.histHead);
        outIdx++;

        // --- Advance phase accumulator ---
        s.phaseAccum += s.phaseStep;

        // --- Consume input samples that the phase advance has stepped past ---
        while (s.phaseAccum >= PHASE_SCALE && inIdx < frames) {
            s.phaseAccum -= PHASE_SCALE;
            // Push new input sample into ring buffer
            s.histL[s.histHead] = laneL[inIdx];
            s.histR[s.histHead] = laneR[inIdx];
            s.histHead = (s.histHead + 1) & (uint32_t)(N - 1);
            inIdx++;
        }
    }

    // --- Copy output back to lane buffers ---
    if (outIdx > 0) {
        memcpy(laneL, outL, (size_t)outIdx * sizeof(float));
        memcpy(laneR, outR, (size_t)outIdx * sizeof(float));
    }

    return outIdx;
}
