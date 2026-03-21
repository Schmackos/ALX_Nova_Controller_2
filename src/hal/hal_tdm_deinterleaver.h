#pragma once
#ifdef DAC_ENABLED
// hal_tdm_deinterleaver.h — TDM 4-slot deinterleaver for ES9843PRO
//
// The ES9843PRO in TDM mode outputs all 4 audio channels time-multiplexed
// into a single I2S data line.  Each frame consists of 4 consecutive 32-bit
// slots in the order [SLOT0=CH1][SLOT1=CH2][SLOT2=CH3][SLOT3=CH4].
//
// This module:
//   1. Receives the raw 4-slot TDM DMA buffer from i2s_audio (port 2).
//   2. Deinterleaves it into two stereo ping-pong buffers:
//        pair A — interleaved CH1/CH2 (stereo pair 0)
//        pair B — interleaved CH3/CH4 (stereo pair 1)
//   3. Exposes two read callbacks that the audio pipeline calls independently
//      via AudioInputSource (one per pipeline lane).
//
// Thread-safety model
// -------------------
// The audio pipeline task (Core 1) calls each read callback sequentially
// within the same pipeline iteration.  The "produce first, consume both"
// ordering is guaranteed by the bridge registering pair A at a lower lane
// index than pair B.  The pipeline reads sources in ascending lane order,
// so pair A's callback is always called first.
//
// Pair A's callback:
//   - Reads the full 4-slot TDM DMA buffer from I2S2.
//   - Deinterleaves CH1+CH2 into _pairBuf[_writeIdx][0] (CH1/CH2 stereo).
//   - Deinterleaves CH3+CH4 into _pairBuf[_writeIdx][1] (CH3/CH4 stereo).
//   - Toggles _writeIdx (ping-pong swap, atomic uint8_t write on RISC-V).
//   - Returns CH1/CH2 stereo frames to the pipeline.
//
// Pair B's callback:
//   - Reads from _pairBuf[!_writeIdx][1] (the buffer pair A just finished).
//   - No DMA read — returns the deinterleaved CH3/CH4 data directly.
//   - Returns the same frame count as pair A stored.
//
// The ping-pong scheme avoids any mutex: pair A always writes to _writeIdx
// and pair B always reads from the opposite side.  Because both callbacks
// run on the same core within the same pipeline task tick, there is zero
// contention.
//
// Buffer sizing
// -------------
// DMA_BUF_LEN stereo frames × 4 slots × sizeof(int32_t) = worst-case DMA
// read size.  Each ping-pong side stores DMA_BUF_LEN stereo (L+R) int32_t
// pairs per channel pair, so each side is DMA_BUF_LEN × 2 × sizeof(int32_t)
// bytes.  With DMA_BUF_LEN=128 that is 128×2×4=1024 bytes per pair, two
// pairs per side, two sides = 4096 bytes total.  Allocated from PSRAM when
// available (see hal_tdm_deinterleaver.cpp).

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../audio_input_source.h"

// Maximum TDM frame count per DMA buffer.  Must match I2S_DMA_BUF_LEN in
// config.h.  Defining it here independently lets the deinterleaver be
// compiled without dragging in the full platform config.
#ifndef TDM_MAX_FRAMES_PER_BUF
#define TDM_MAX_FRAMES_PER_BUF 128
#endif

// Number of channel pairs produced from one 4-slot TDM stream
#define TDM_PAIR_COUNT 2

// Scratch buffer for the raw 4-slot TDM DMA read
// Size = frames × 4 slots × 4 bytes
#define TDM_RAW_BUF_SAMPLES (TDM_MAX_FRAMES_PER_BUF * 4)

// ---------------------------------------------------------------------------
// HalTdmDeinterleaver — self-contained deinterleaver state machine.
// One instance lives inside HalEs9843pro.  The driver calls init() once
// during its own init(), then passes the two AudioInputSource pointers to
// the bridge via getInputSourceAt(0) and getInputSourceAt(1).
// ---------------------------------------------------------------------------
class HalTdmDeinterleaver {
public:
    HalTdmDeinterleaver();
    ~HalTdmDeinterleaver();

    // Allocate ping-pong buffers (PSRAM preferred, heap fallback).
    // port: I2S port index used for the TDM read (matches cfg->i2sPort, default 2).
    // Returns false if allocation fails — caller should abort HAL init.
    bool init(uint8_t i2sPort);

    // Release buffers.  Safe to call even if init() failed.
    void deinit();

    // Populate two AudioInputSource structs with deinterleaver callbacks.
    // name0 / name1: human-readable names ("ES9843PRO CH1/2", "ES9843PRO CH3/4").
    // Must be called after init().
    void buildSources(const char* name0, const char* name1,
                      AudioInputSource* out0, AudioInputSource* out1);

    // Returns true if a successful TDM read has been completed at least once.
    bool isReady() const { return _ready; }

private:
    // Static thunks registered in AudioInputSource — take 'this' from a
    // module-level singleton pointer set during buildSources().  See .cpp.
    static uint32_t _pairARead(int32_t* dst, uint32_t frames);
    static uint32_t _pairBRead(int32_t* dst, uint32_t frames);
    static bool     _pairAActive(void);
    static bool     _pairBActive(void);
    static uint32_t _getSampleRate(void);

    // Instance-level implementations called by the static thunks.
    uint32_t _doPairARead(int32_t* dst, uint32_t frames);
    uint32_t _doPairBRead(int32_t* dst, uint32_t frames);

    // --------------- Ping-pong buffer state ---------------
    // _pairBuf[side][pairIdx] points to a stereo interleaved buffer of
    // TDM_MAX_FRAMES_PER_BUF int32_t pairs (L+R).
    int32_t* _pairBuf[2][TDM_PAIR_COUNT];  // [ping|pong][ch12|ch34]

    // Raw TDM scratch buffer (4 slots per frame, shared across both pairs)
    int32_t* _rawBuf;

    // Ping-pong index: pair A writes to _writeIdx, pair B reads from [1-_writeIdx].
    // volatile + uint8_t: single-byte store is atomic on RISC-V (no half-word tearing).
    volatile uint8_t _writeIdx;

    // Frame count written by pair A into the current write side.
    // Read by pair B — written before _writeIdx toggles, ensuring visibility.
    volatile uint32_t _lastFrameCount;

    uint8_t _i2sPort;
    bool    _ready;
    bool    _initialized;
};

#endif // DAC_ENABLED
