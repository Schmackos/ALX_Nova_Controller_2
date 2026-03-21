#ifdef DAC_ENABLED
// hal_tdm_deinterleaver.cpp — ES9843PRO 4-slot TDM deinterleaver
//
// See hal_tdm_deinterleaver.h for the full design rationale and thread-safety
// model.  This file implements the instance logic and the static AudioInputSource
// thunks.

#include "hal_tdm_deinterleaver.h"
#include "../psram_alloc.h"

#ifndef NATIVE_TEST
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../i2s_audio.h"
#include "../debug_serial.h"
#include "../app_state.h"
#else
// ===== Native test stubs =====
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)

// Minimal heap_caps_calloc stub: fall back to standard calloc
#include <cstdlib>
static inline void* heap_caps_calloc(size_t n, size_t sz, uint32_t) { return calloc(n, sz); }

// Port read stubs (real implementations live in i2s_audio.cpp).
// Guarded by TDM_TEST_PROVIDES_STUBS: the deinterleaver unit test defines
// its own controlled versions of i2s_audio_port2_tdm_read / _active so it
// can inject synthetic TDM frames.  Other native tests that include this
// .cpp get the no-op versions below.
#ifndef TDM_TEST_PROVIDES_STUBS
inline uint32_t i2s_audio_port0_read(int32_t*, uint32_t) { return 0; }
inline uint32_t i2s_audio_port1_read(int32_t*, uint32_t) { return 0; }
inline uint32_t i2s_audio_port2_read(int32_t*, uint32_t) { return 0; }
inline bool i2s_audio_port0_active(void) { return false; }
inline bool i2s_audio_port1_active(void) { return false; }
inline bool i2s_audio_port2_active(void) { return false; }
inline uint32_t i2s_audio_get_sample_rate(void) { return 48000; }

// TDM-specific stubs used in native tests
inline uint32_t i2s_audio_port2_tdm_read(int32_t* dst, uint32_t frames) { (void)dst; (void)frames; return 0; }
inline bool     i2s_audio_port2_tdm_active(void) { return false; }
#endif // TDM_TEST_PROVIDES_STUBS

#define MALLOC_CAP_SPIRAM 0
#endif // NATIVE_TEST

#include <cstring>

// ---------------------------------------------------------------------------
// Instance slot array — supports up to TDM_MAX_INSTANCES concurrent
// HalTdmDeinterleaver objects.  Each instance claims one slot during
// buildSources() and releases it in deinit().  Two dedicated thunk sets
// (_0 / _1) route AudioInputSource callbacks to the correct instance without
// capturing a lambda or using a single shared pointer.
// ---------------------------------------------------------------------------
#define TDM_MAX_INSTANCES 2
static HalTdmDeinterleaver* _gInstances[TDM_MAX_INSTANCES] = { nullptr, nullptr };

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

HalTdmDeinterleaver::HalTdmDeinterleaver()
    : _rawBuf(nullptr),
      _writeIdx(0),
      _lastFrameCount(0),
      _i2sPort(2),
      _ready(false),
      _initialized(false),
      _instanceIdx(0xFF)
{
    for (int s = 0; s < 2; s++) {
        for (int p = 0; p < TDM_PAIR_COUNT; p++) {
            _pairBuf[s][p] = nullptr;
        }
    }
}

HalTdmDeinterleaver::~HalTdmDeinterleaver() {
    deinit();
}

// ---------------------------------------------------------------------------
// init() — allocate ping-pong buffers
// ---------------------------------------------------------------------------

bool HalTdmDeinterleaver::init(uint8_t i2sPort) {
    if (_initialized) return true;

    _i2sPort = i2sPort;

    // Each ping-pong side holds TDM_MAX_FRAMES_PER_BUF stereo (L+R) int32_t pairs
    // per channel pair.  Memory layout per side:
    //   _pairBuf[side][0] : CH1/CH2 — TDM_MAX_FRAMES_PER_BUF × 2 × sizeof(int32_t)
    //   _pairBuf[side][1] : CH3/CH4 — same size
    //   _rawBuf            : TDM_RAW_BUF_SAMPLES × sizeof(int32_t)  (shared scratch)
    const size_t stBufSamples = (size_t)TDM_MAX_FRAMES_PER_BUF * 2;  // L+R per frame
    const size_t stBufBytes   = stBufSamples * sizeof(int32_t);
    const size_t rawBufBytes  = (size_t)TDM_RAW_BUF_SAMPLES * sizeof(int32_t);

    // Prefer PSRAM — TDM buffers are large and not latency-critical
    for (int s = 0; s < 2; s++) {
        for (int p = 0; p < TDM_PAIR_COUNT; p++) {
            _pairBuf[s][p] = (int32_t*)psram_alloc(stBufSamples, sizeof(int32_t), "tdm_pair");
            if (!_pairBuf[s][p]) {
                LOG_E("[HAL:TDM] Buffer alloc failed: side=%d pair=%d (%u bytes)",
                      s, p, (unsigned)stBufBytes);
                deinit();
                return false;
            }
        }
    }

    _rawBuf = (int32_t*)psram_alloc(TDM_RAW_BUF_SAMPLES, sizeof(int32_t), "tdm_raw");
    if (!_rawBuf) {
        LOG_E("[HAL:TDM] Raw scratch buffer alloc failed (%u bytes)",
              (unsigned)rawBufBytes);
        deinit();
        return false;
    }

    _writeIdx      = 0;
    _lastFrameCount = 0;
    _ready         = false;
    _initialized   = true;

    LOG_I("[HAL:TDM] Deinterleaver ready: port=%u bufs=%u bytes each, raw=%u bytes",
          _i2sPort, (unsigned)stBufBytes, (unsigned)rawBufBytes);
    return true;
}

// ---------------------------------------------------------------------------
// deinit() — free all allocations
// ---------------------------------------------------------------------------

void HalTdmDeinterleaver::deinit() {
    for (int s = 0; s < 2; s++) {
        for (int p = 0; p < TDM_PAIR_COUNT; p++) {
            psram_free(_pairBuf[s][p], "tdm_pair");
            _pairBuf[s][p] = nullptr;
        }
    }
    psram_free(_rawBuf, "tdm_raw");
    _rawBuf      = nullptr;
    _ready       = false;
    _initialized = false;

    // Release the instance slot so another device may claim it
    if (_instanceIdx < TDM_MAX_INSTANCES) {
        _gInstances[_instanceIdx] = nullptr;
        _instanceIdx = 0xFF;
    }
}

// ---------------------------------------------------------------------------
// buildSources() — populate two AudioInputSource structs
// ---------------------------------------------------------------------------

void HalTdmDeinterleaver::buildSources(const char* name0, const char* name1,
                                        AudioInputSource* out0, AudioInputSource* out1) {
    // Find a free instance slot
    uint8_t idx = 0xFF;
    for (uint8_t i = 0; i < TDM_MAX_INSTANCES; i++) {
        if (_gInstances[i] == nullptr) { idx = i; break; }
    }
    if (idx == 0xFF) {
        LOG_E("[HAL:TDM] All %d instance slots full — cannot build sources", TDM_MAX_INSTANCES);
        // Leave out0/out1 callbacks null so caller can detect the failure
        return;
    }
    _instanceIdx      = idx;
    _gInstances[idx]  = this;

    // Select thunk set for this slot
    typedef uint32_t (*ReadFn)(int32_t*, uint32_t);
    typedef bool     (*ActiveFn)(void);
    ReadFn   fnARead,   fnBRead;
    ActiveFn fnAActive, fnBActive;
    if (idx == 0) {
        fnARead   = _pairARead_0;   fnBRead   = _pairBRead_0;
        fnAActive = _pairAActive_0; fnBActive = _pairBActive_0;
    } else {
        fnARead   = _pairARead_1;   fnBRead   = _pairBRead_1;
        fnAActive = _pairAActive_1; fnBActive = _pairBActive_1;
    }

    // -- Pair A: CH1/CH2 (pipeline reads this first, triggers TDM DMA read) --
    memset(out0, 0, sizeof(AudioInputSource));
    out0->name          = name0;
    out0->lane          = 0;       // Overwritten by bridge during registration
    out0->halSlot       = 0xFF;    // Overwritten by bridge
    out0->gainLinear    = 1.0f;
    out0->vuL           = -90.0f;
    out0->vuR           = -90.0f;
    out0->isHardwareAdc = true;
    out0->read          = fnARead;
    out0->isActive      = fnAActive;
    out0->getSampleRate = _getSampleRate;

    // -- Pair B: CH3/CH4 (reads from the buffer pair A just filled) --
    memset(out1, 0, sizeof(AudioInputSource));
    out1->name          = name1;
    out1->lane          = 0;       // Overwritten by bridge during registration
    out1->halSlot       = 0xFF;    // Overwritten by bridge
    out1->gainLinear    = 1.0f;
    out1->vuL           = -90.0f;
    out1->vuR           = -90.0f;
    out1->isHardwareAdc = true;
    out1->read          = fnBRead;
    out1->isActive      = fnBActive;
    out1->getSampleRate = _getSampleRate;

    LOG_I("[HAL:TDM] Sources built (slot %u): '%s' (pair A), '%s' (pair B)",
          idx, name0, name1);
}

// ---------------------------------------------------------------------------
// _doPairARead() — TDM DMA read + deinterleave; provides CH1/CH2 to pipeline
//
// Frame layout in TDM buffer (4 slots × 32-bit per frame):
//   sample[frame*4 + 0] = SLOT0 = CH1 left-justified 24-bit PCM
//   sample[frame*4 + 1] = SLOT1 = CH2 left-justified 24-bit PCM
//   sample[frame*4 + 2] = SLOT2 = CH3 left-justified 24-bit PCM
//   sample[frame*4 + 3] = SLOT3 = CH4 left-justified 24-bit PCM
//
// dst layout expected by AudioInputSource.read():
//   dst[frame*2 + 0] = L sample (CH1 for pair A)
//   dst[frame*2 + 1] = R sample (CH2 for pair A)
// ---------------------------------------------------------------------------

uint32_t HalTdmDeinterleaver::_doPairARead(int32_t* dst, uint32_t frames) {
    if (!_initialized || !_rawBuf) return 0;

    // --- Step 1: Read full 4-slot TDM frame from I2S ---
    // The TDM channel delivers 4 slots × 32-bit per "stereo frame" as seen by
    // the IDF5 i2s_channel_read() API (which measures in slot-pairs, but with
    // TDM enabled the driver packs all 4 slots contiguously into the DMA buffer).
    //
    // We request 'frames' TDM frames.  The raw buffer must hold frames × 4 slots.
    // Cap to TDM_MAX_FRAMES_PER_BUF to protect the scratch buffer.
    if (frames > TDM_MAX_FRAMES_PER_BUF) frames = TDM_MAX_FRAMES_PER_BUF;

    // Use the port2 TDM variant that reads 4-slot frames.
    // i2s_audio_port2_tdm_read() reads frames × 4 × sizeof(int32_t) bytes from I2S2.
    // In native tests, the test translation unit provides its own stub via
    // TDM_TEST_PROVIDES_STUBS; see the NATIVE_TEST block at the top of this file.
    uint32_t framesRead = i2s_audio_port2_tdm_read(_rawBuf, frames);

    if (framesRead == 0) {
        _lastFrameCount = 0;
        return 0;
    }

    // --- Step 2: Deinterleave into ping-pong write side ---
    uint8_t wIdx = _writeIdx;   // Capture local copy (both pairs use same side this tick)

    int32_t* pA = _pairBuf[wIdx][0];  // CH1/CH2 destination
    int32_t* pB = _pairBuf[wIdx][1];  // CH3/CH4 destination

    for (uint32_t f = 0; f < framesRead; f++) {
        const int32_t* slot = _rawBuf + (f * 4);
        // CH1/CH2 stereo pair A
        pA[f * 2 + 0] = slot[0];  // CH1 → L
        pA[f * 2 + 1] = slot[1];  // CH2 → R
        // CH3/CH4 stereo pair B
        pB[f * 2 + 0] = slot[2];  // CH3 → L
        pB[f * 2 + 1] = slot[3];  // CH4 → R
    }

    // --- Step 3: Publish frame count and toggle write index ---
    // Write _lastFrameCount before toggling _writeIdx so that pair B reads a
    // consistent value.  On RISC-V, normal stores are not reordered past each
    // other by the core (in-order execution), but the compiler may reorder.
    // A volatile store on _lastFrameCount prevents the reorder.
    _lastFrameCount = framesRead;

    // Atomic ping-pong swap: 0→1 or 1→0.  uint8_t write is single-byte on all
    // RISC-V implementations — no need for atomic CAS here.
    _writeIdx = (uint8_t)(wIdx ^ 1u);

    _ready = true;

    // --- Step 4: Copy CH1/CH2 into caller's dst ---
    // The pipeline supplies its own dst buffer; copy from the write-side pair A
    // before the index toggle (we captured wIdx above so we're still pointing at
    // the correct side).
    memcpy(dst, pA, framesRead * 2 * sizeof(int32_t));

    return framesRead;
}

// ---------------------------------------------------------------------------
// _doPairBRead() — returns CH3/CH4 from the buffer pair A already filled
// ---------------------------------------------------------------------------

uint32_t HalTdmDeinterleaver::_doPairBRead(int32_t* dst, uint32_t frames) {
    if (!_initialized || !_ready) return 0;

    // After pair A toggled _writeIdx, the "just written" side is [1 - _writeIdx].
    // pair B reads from that side.
    uint8_t rIdx = (uint8_t)(_writeIdx ^ 1u);

    uint32_t count = _lastFrameCount;
    if (count == 0) return 0;
    if (count > frames) count = frames;

    memcpy(dst, _pairBuf[rIdx][1], count * 2 * sizeof(int32_t));
    return count;
}

// ---------------------------------------------------------------------------
// Static thunks — two sets, one per instance slot.
// Each set routes to _gInstances[N] independently so that two concurrent
// HalTdmDeinterleaver objects never share a callback pointer.
// ---------------------------------------------------------------------------

// ---- Slot 0 thunks ----
uint32_t HalTdmDeinterleaver::_pairARead_0(int32_t* dst, uint32_t frames) {
    if (!_gInstances[0]) return 0;
    return _gInstances[0]->_doPairARead(dst, frames);
}
uint32_t HalTdmDeinterleaver::_pairBRead_0(int32_t* dst, uint32_t frames) {
    if (!_gInstances[0]) return 0;
    return _gInstances[0]->_doPairBRead(dst, frames);
}
bool HalTdmDeinterleaver::_pairAActive_0(void) {
    if (!_gInstances[0]) return false;
    return i2s_audio_port2_tdm_active();
}
bool HalTdmDeinterleaver::_pairBActive_0(void) {
    if (!_gInstances[0]) return false;
    return _gInstances[0]->_ready;
}

// ---- Slot 1 thunks ----
uint32_t HalTdmDeinterleaver::_pairARead_1(int32_t* dst, uint32_t frames) {
    if (!_gInstances[1]) return 0;
    return _gInstances[1]->_doPairARead(dst, frames);
}
uint32_t HalTdmDeinterleaver::_pairBRead_1(int32_t* dst, uint32_t frames) {
    if (!_gInstances[1]) return 0;
    return _gInstances[1]->_doPairBRead(dst, frames);
}
bool HalTdmDeinterleaver::_pairAActive_1(void) {
    if (!_gInstances[1]) return false;
    return i2s_audio_port2_tdm_active();
}
bool HalTdmDeinterleaver::_pairBActive_1(void) {
    if (!_gInstances[1]) return false;
    return _gInstances[1]->_ready;
}

uint32_t HalTdmDeinterleaver::_getSampleRate(void) {
#ifndef NATIVE_TEST
    return i2s_audio_get_sample_rate();
#else
    return 48000;
#endif
}

#endif // DAC_ENABLED
