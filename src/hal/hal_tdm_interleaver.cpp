#ifdef DAC_ENABLED
// hal_tdm_interleaver.cpp — 8-slot TDM interleaver for 8-channel DAC expansion
//
// See hal_tdm_interleaver.h for the full design rationale and thread-safety model.
// This file implements instance logic and static AudioOutputSink thunks.

#include "hal_tdm_interleaver.h"
#include "../psram_alloc.h"

#ifndef NATIVE_TEST
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../i2s_audio.h"
#include "../debug_serial.h"
#else
// ===== Native test stubs =====
#define LOG_I(fmt, ...) ((void)0)
#define LOG_W(fmt, ...) ((void)0)
#define LOG_E(fmt, ...) ((void)0)
#define LOG_D(fmt, ...) ((void)0)

#include <cstdlib>
static inline void* heap_caps_calloc(size_t n, size_t sz, uint32_t) { return calloc(n, sz); }

// TX write stub — guarded so the interleaver unit test can provide its own
// controlled version to capture what was written.
#ifndef TDM_INTERLEAVER_TEST_PROVIDES_STUBS
inline void i2s_audio_write_expansion_tdm_tx(const void*, size_t, size_t* bw, uint32_t) {
    if (bw) *bw = 0;
}
#endif // TDM_INTERLEAVER_TEST_PROVIDES_STUBS

#define MALLOC_CAP_SPIRAM 0
#endif // NATIVE_TEST

#include <cstring>

// ---------------------------------------------------------------------------
// Instance slot array — supports up to TDM_INTERLEAVER_MAX_INSTANCES concurrent
// HalTdmInterleaver objects.  Two dedicated thunk sets (_0 / _1) route
// AudioOutputSink write/isReady callbacks to the correct instance.
// ---------------------------------------------------------------------------
#define TDM_INTERLEAVER_MAX_INSTANCES 2
static HalTdmInterleaver* _gInterleaverInstances[TDM_INTERLEAVER_MAX_INSTANCES] = { nullptr, nullptr };

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

HalTdmInterleaver::HalTdmInterleaver()
    : _tdmBuf(nullptr),
      _writeIdx(0),
      _lastFrames(0),
      _i2sPort(0),
      _ready(false),
      _initialized(false),
      _instanceIdx(0xFF)
{
    for (int s = 0; s < 2; s++) {
        for (int p = 0; p < TDM_INTERLEAVER_PAIR_COUNT; p++) {
            _pairBuf[s][p] = nullptr;
        }
    }
}

HalTdmInterleaver::~HalTdmInterleaver() {
    deinit();
}

// ---------------------------------------------------------------------------
// init() — allocate ping-pong pair buffers + TDM output buffer
// ---------------------------------------------------------------------------

bool HalTdmInterleaver::init(uint8_t i2sPort) {
    if (_initialized) return true;

    _i2sPort = i2sPort;

    // Per-pair buffer: TDM_INTERLEAVER_FRAMES stereo (L+R) int32_t pairs.
    // Two ping-pong sides × 4 pairs = 8 allocations.
    const size_t pairSamples = (size_t)TDM_INTERLEAVER_FRAMES * 2;  // L+R per frame
    const size_t pairBytes   = pairSamples * sizeof(int32_t);

    // TDM output buffer: frames × 8 slots × sizeof(int32_t)
    const size_t tdmSamples  = (size_t)TDM_INTERLEAVER_FRAMES * TDM_INTERLEAVER_SLOTS;
    const size_t tdmBytes    = tdmSamples * sizeof(int32_t);

    // Prefer PSRAM — interleaver buffers are large and not DMA-critical
    for (int s = 0; s < 2; s++) {
        for (int p = 0; p < TDM_INTERLEAVER_PAIR_COUNT; p++) {
            _pairBuf[s][p] = (int32_t*)psram_alloc(pairSamples, sizeof(int32_t), "tdm_il_pair");
            if (!_pairBuf[s][p]) {
                LOG_E("[HAL:TDMIL] Pair buffer alloc failed: side=%d pair=%d (%u bytes)",
                      s, p, (unsigned)pairBytes);
                deinit();
                return false;
            }
        }
    }

    _tdmBuf = (int32_t*)psram_alloc(tdmSamples, sizeof(int32_t), "tdm_il_out");
    if (!_tdmBuf) {
        LOG_E("[HAL:TDMIL] TDM output buffer alloc failed (%u bytes)", (unsigned)tdmBytes);
        deinit();
        return false;
    }

    _writeIdx    = 0;
    _lastFrames  = 0;
    _ready       = true;   // Ready as soon as buffers are allocated (TX path, no pre-fill needed)
    _initialized = true;

    LOG_I("[HAL:TDMIL] Interleaver ready: port=%u pair=%u bytes each, tdm=%u bytes",
          _i2sPort, (unsigned)pairBytes, (unsigned)tdmBytes);
    return true;
}

// ---------------------------------------------------------------------------
// deinit() — free all allocations and release instance slot
// ---------------------------------------------------------------------------

void HalTdmInterleaver::deinit() {
    for (int s = 0; s < 2; s++) {
        for (int p = 0; p < TDM_INTERLEAVER_PAIR_COUNT; p++) {
            psram_free(_pairBuf[s][p], "tdm_il_pair");
            _pairBuf[s][p] = nullptr;
        }
    }
    psram_free(_tdmBuf, "tdm_il_out");
    _tdmBuf      = nullptr;
    _ready       = false;
    _initialized = false;

    // Release instance slot so another device may claim it
    if (_instanceIdx < TDM_INTERLEAVER_MAX_INSTANCES) {
        _gInterleaverInstances[_instanceIdx] = nullptr;
        _instanceIdx = 0xFF;
    }
}

// ---------------------------------------------------------------------------
// buildSinks() — populate four AudioOutputSink structs
// ---------------------------------------------------------------------------

void HalTdmInterleaver::buildSinks(const char* nameA, const char* nameB,
                                    const char* nameC, const char* nameD,
                                    AudioOutputSink* outA, AudioOutputSink* outB,
                                    AudioOutputSink* outC, AudioOutputSink* outD,
                                    uint8_t halSlot) {
    // Find a free instance slot
    uint8_t idx = 0xFF;
    for (uint8_t i = 0; i < TDM_INTERLEAVER_MAX_INSTANCES; i++) {
        if (_gInterleaverInstances[i] == nullptr) { idx = i; break; }
    }
    if (idx == 0xFF) {
        LOG_E("[HAL:TDMIL] All %d instance slots full — cannot build sinks",
              TDM_INTERLEAVER_MAX_INSTANCES);
        // Leave callbacks null so the caller can detect the failure
        return;
    }
    _instanceIdx                  = idx;
    _gInterleaverInstances[idx]   = this;

    // Select thunk set for this slot
    typedef void (*WriteFn)(const int32_t*, int);
    typedef bool (*ReadyFn)(void);

    WriteFn fnW0, fnW1, fnW2, fnW3;
    ReadyFn fnReady;
    if (idx == 0) {
        fnW0    = _writePair0_0;  fnW1    = _writePair1_0;
        fnW2    = _writePair2_0;  fnW3    = _writePair3_0;
        fnReady = _isReady_0;
    } else {
        fnW0    = _writePair0_1;  fnW1    = _writePair1_1;
        fnW2    = _writePair2_1;  fnW3    = _writePair3_1;
        fnReady = _isReady_1;
    }

    // Helper lambda-style macro to fill one sink struct
    // (C++11, no captures needed since all data is in arguments)
    const char* names[4]  = { nameA, nameB, nameC, nameD };
    AudioOutputSink* outs[4] = { outA, outB, outC, outD };
    WriteFn writers[4]   = { fnW0, fnW1, fnW2, fnW3 };

    for (int p = 0; p < TDM_INTERLEAVER_PAIR_COUNT; p++) {
        AudioOutputSink* s = outs[p];
        memset(s, 0, sizeof(AudioOutputSink));
        s->name         = names[p];
        s->firstChannel = (uint8_t)(p * 2);  // 0, 2, 4, 6
        s->channelCount = 2;
        s->gainLinear   = 1.0f;
        s->volumeGain   = 1.0f;
        s->muted        = false;
        s->vuL          = -90.0f;
        s->vuR          = -90.0f;
        s->halSlot      = halSlot;
        s->write        = writers[p];
        s->isReady      = fnReady;
    }

    LOG_I("[HAL:TDMIL] Sinks built (slot %u): '%s','%s','%s','%s' halSlot=%u",
          idx, nameA, nameB, nameC, nameD, halSlot);
}

// ---------------------------------------------------------------------------
// _writePair() — copy stereo data into the current ping-pong write side.
// If pairIdx == 3, trigger TDM assembly + I2S TX write.
// ---------------------------------------------------------------------------

void HalTdmInterleaver::_writePair(uint8_t pairIdx, const int32_t* buf, int frames) {
    if (!_initialized || !buf || frames <= 0) return;

    // Cap to allocated buffer size
    if (frames > TDM_INTERLEAVER_FRAMES) frames = TDM_INTERLEAVER_FRAMES;

    uint8_t wIdx = _writeIdx;
    memcpy(_pairBuf[wIdx][pairIdx], buf, (size_t)frames * 2 * sizeof(int32_t));

    // Record the frame count on the first pair write of this tick.
    // All four pairs should receive the same frame count from the pipeline,
    // but we track it on pair 0 as the authoritative value for _flushTdm.
    if (pairIdx == 0) {
        _lastFrames = frames;
    }

    // Pair 3 is the last writer — trigger flush
    if (pairIdx == 3) {
        _flushTdm(_lastFrames);
    }
}

// ---------------------------------------------------------------------------
// _flushTdm() — interleave 4 stereo pairs into 8-slot TDM and write to I2S TX
//
// Output TDM frame layout (8 slots × 32-bit per frame):
//   tdm[f*8 + 0] = pair0[f*2 + 0]  CH1 L
//   tdm[f*8 + 1] = pair0[f*2 + 1]  CH1 R  (CH2)
//   tdm[f*8 + 2] = pair1[f*2 + 0]  CH3 L
//   tdm[f*8 + 3] = pair1[f*2 + 1]  CH3 R  (CH4)
//   tdm[f*8 + 4] = pair2[f*2 + 0]  CH5 L
//   tdm[f*8 + 5] = pair2[f*2 + 1]  CH5 R  (CH6)
//   tdm[f*8 + 6] = pair3[f*2 + 0]  CH7 L
//   tdm[f*8 + 7] = pair3[f*2 + 1]  CH7 R  (CH8)
// ---------------------------------------------------------------------------

void HalTdmInterleaver::_flushTdm(int frames) {
    if (!_tdmBuf || frames <= 0) return;

    uint8_t wIdx = _writeIdx;

    const int32_t* p0 = _pairBuf[wIdx][0];
    const int32_t* p1 = _pairBuf[wIdx][1];
    const int32_t* p2 = _pairBuf[wIdx][2];
    const int32_t* p3 = _pairBuf[wIdx][3];

    for (int f = 0; f < frames; f++) {
        int32_t* slot = _tdmBuf + (f * TDM_INTERLEAVER_SLOTS);
        slot[0] = p0[f * 2 + 0];  // CH1 L
        slot[1] = p0[f * 2 + 1];  // CH2 R
        slot[2] = p1[f * 2 + 0];  // CH3 L
        slot[3] = p1[f * 2 + 1];  // CH4 R
        slot[4] = p2[f * 2 + 0];  // CH5 L
        slot[5] = p2[f * 2 + 1];  // CH6 R
        slot[6] = p3[f * 2 + 0];  // CH7 L
        slot[7] = p3[f * 2 + 1];  // CH8 R
    }

    // Write assembled TDM buffer to I2S TX DMA.
    // In native builds the call is compiled out unless the test provides its own
    // controlled stub via TDM_INTERLEAVER_TEST_PROVIDES_STUBS, which overrides the
    // default no-op and lets tests capture what was transmitted.
    const size_t txBytes = (size_t)frames * TDM_INTERLEAVER_SLOTS * sizeof(int32_t);
#if !defined(NATIVE_TEST) || defined(TDM_INTERLEAVER_TEST_PROVIDES_STUBS)
    size_t written = 0;
    i2s_audio_write_expansion_tdm_tx(_tdmBuf, txBytes, &written, 5);
#endif

    // Ping-pong swap: next tick writes into the other side
    _writeIdx = (uint8_t)(wIdx ^ 1u);
}

// ---------------------------------------------------------------------------
// Static thunks — two sets, one per instance slot.
// Each set routes to _gInterleaverInstances[N] independently so that two
// concurrent HalTdmInterleaver objects never share a callback pointer.
// ---------------------------------------------------------------------------

// ---- Slot 0 thunks ----
void HalTdmInterleaver::_writePair0_0(const int32_t* buf, int frames) {
    if (_gInterleaverInstances[0]) _gInterleaverInstances[0]->_writePair(0, buf, frames);
}
void HalTdmInterleaver::_writePair1_0(const int32_t* buf, int frames) {
    if (_gInterleaverInstances[0]) _gInterleaverInstances[0]->_writePair(1, buf, frames);
}
void HalTdmInterleaver::_writePair2_0(const int32_t* buf, int frames) {
    if (_gInterleaverInstances[0]) _gInterleaverInstances[0]->_writePair(2, buf, frames);
}
void HalTdmInterleaver::_writePair3_0(const int32_t* buf, int frames) {
    if (_gInterleaverInstances[0]) _gInterleaverInstances[0]->_writePair(3, buf, frames);
}
bool HalTdmInterleaver::_isReady_0(void) {
    if (!_gInterleaverInstances[0]) return false;
    return _gInterleaverInstances[0]->_ready;
}

// ---- Slot 1 thunks ----
void HalTdmInterleaver::_writePair0_1(const int32_t* buf, int frames) {
    if (_gInterleaverInstances[1]) _gInterleaverInstances[1]->_writePair(0, buf, frames);
}
void HalTdmInterleaver::_writePair1_1(const int32_t* buf, int frames) {
    if (_gInterleaverInstances[1]) _gInterleaverInstances[1]->_writePair(1, buf, frames);
}
void HalTdmInterleaver::_writePair2_1(const int32_t* buf, int frames) {
    if (_gInterleaverInstances[1]) _gInterleaverInstances[1]->_writePair(2, buf, frames);
}
void HalTdmInterleaver::_writePair3_1(const int32_t* buf, int frames) {
    if (_gInterleaverInstances[1]) _gInterleaverInstances[1]->_writePair(3, buf, frames);
}
bool HalTdmInterleaver::_isReady_1(void) {
    if (!_gInterleaverInstances[1]) return false;
    return _gInterleaverInstances[1]->_ready;
}

#endif // DAC_ENABLED
