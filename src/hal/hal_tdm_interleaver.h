#pragma once
#ifdef DAC_ENABLED
// hal_tdm_interleaver.h — TDM 8-slot interleaver for 8-channel DAC expansion
//
// This is the reverse of HalTdmDeinterleaver.  Where the deinterleaver splits
// a single 4-slot TDM RX frame into two stereo pipeline input sources, the
// interleaver COMBINES four stereo pipeline sink pairs into one 8-slot TDM TX
// frame for delivery to an 8-channel DAC (e.g. ES9038PRO).
//
// Each "frame" in the TDM output buffer has 8 consecutive 32-bit slots:
//   SLOT0 = pair0 L (CH1)    SLOT1 = pair0 R (CH2)
//   SLOT2 = pair1 L (CH3)    SLOT3 = pair1 R (CH4)
//   SLOT4 = pair2 L (CH5)    SLOT5 = pair2 R (CH6)
//   SLOT6 = pair3 L (CH7)    SLOT7 = pair3 R (CH8)
//
// Thread-safety model ("last writer flushes")
// -------------------------------------------
// The audio pipeline task (Core 1) calls sink write callbacks in ascending
// slot order.  The bridge registers pair 0 at the lowest slot index, pair 3
// at the highest.  Therefore the pipeline always calls pairs 0, 1, 2, 3 in
// that order within the same task tick.
//
// Pair 0 write callback:
//   - Copies stereo frame data into _pairBuf[_writeIdx][0].
//   - No flush yet.
//
// Pair 1 and 2 write callbacks:
//   - Copy stereo frame data into _pairBuf[_writeIdx][1] / [2].
//   - No flush yet.
//
// Pair 3 write callback:
//   - Copies stereo frame data into _pairBuf[_writeIdx][3].
//   - Interleaves all four pair buffers into _tdmBuf (8 slots per frame).
//   - Calls i2s_audio_write_expansion_tdm_tx() to push to I2S DMA.
//   - Swaps _writeIdx (ping-pong) so the next tick uses the other side.
//
// Ping-pong scheme
// ----------------
// Two sides of _pairBuf[0..3] let the next pipeline tick start buffering into
// the idle side while the current tick is still flushing the TDM write.  Because
// all four pairs run sequentially on Core 1 and the I2S write is synchronous
// (blocking until DMA accepts the buffer), no mutex is required.
//
// Buffer sizing
// -------------
// TDM_INTERLEAVER_FRAMES stereo frames per pair per side:
//   Per-pair bytes per side = TDM_INTERLEAVER_FRAMES × 2 × sizeof(int32_t)
//   TDM output buffer      = TDM_INTERLEAVER_FRAMES × 8 × sizeof(int32_t)
// With 256 frames: 256×2×4 = 2 048 bytes per pair-side, 256×8×4 = 8 192 bytes TDM.
// Allocated from PSRAM when available (not latency-critical; DMA read is internal SRAM).

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../audio_output_sink.h"

// Maximum stereo frames per TDM output buffer.  Should match the pipeline's
// DMA buffer length (I2S_DMA_BUF_LEN).  Defining it here keeps the interleaver
// compilable without the full platform config.
#ifndef TDM_INTERLEAVER_FRAMES
#define TDM_INTERLEAVER_FRAMES 256
#endif

// Number of stereo sink pairs combined into one TDM frame
#define TDM_INTERLEAVER_PAIR_COUNT 4

// Slots per TDM output frame (2 per pair × 4 pairs)
#define TDM_INTERLEAVER_SLOTS 8

// ---------------------------------------------------------------------------
// HalTdmInterleaver — self-contained interleaver state machine.
// One instance lives inside a multi-channel DAC driver (e.g. HalEssDac8ch).
// The driver calls init() once during its own init(), then passes the four
// AudioOutputSink pointers to the pipeline bridge via buildSinks().
// ---------------------------------------------------------------------------
class HalTdmInterleaver {
public:
    HalTdmInterleaver();
    ~HalTdmInterleaver();

    // Allocate ping-pong pair buffers + TDM output buffer (PSRAM preferred).
    // port: I2S port index used for TDM TX output.
    // Returns false if any allocation fails — caller should abort HAL init.
    bool init(uint8_t i2sPort);

    // Release all buffers.  Safe to call even if init() failed or was never called.
    void deinit();

    // Populate four AudioOutputSink structs, one per stereo pair.
    // nameA/B/C/D: human-readable names ("ES9038PRO CH1/2" ... "ES9038PRO CH7/8").
    // halSlot: HAL device slot index set on all four sinks (bridge overwrites firstChannel).
    // Must be called after init().
    void buildSinks(const char* nameA, const char* nameB,
                    const char* nameC, const char* nameD,
                    AudioOutputSink* outA, AudioOutputSink* outB,
                    AudioOutputSink* outC, AudioOutputSink* outD,
                    uint8_t halSlot);

    // Returns true after init() succeeds.  Stays true until deinit().
    bool isReady() const { return _ready; }

private:
    // Instance-level write implementation called by static thunks.
    // pairIdx 0-3; buf is stereo interleaved (frames × 2 int32_t); frames = stereo frame count.
    void _writePair(uint8_t pairIdx, const int32_t* buf, int frames);

    // Called by pair 3 write: interleave all four pairs into _tdmBuf and TX.
    void _flushTdm(int frames);

    // ---- Static thunks — slot 0 (instance 0) ----
    static void _writePair0_0(const int32_t* buf, int frames);
    static void _writePair1_0(const int32_t* buf, int frames);
    static void _writePair2_0(const int32_t* buf, int frames);
    static void _writePair3_0(const int32_t* buf, int frames);
    static bool _isReady_0(void);

    // ---- Static thunks — slot 1 (instance 1) ----
    static void _writePair0_1(const int32_t* buf, int frames);
    static void _writePair1_1(const int32_t* buf, int frames);
    static void _writePair2_1(const int32_t* buf, int frames);
    static void _writePair3_1(const int32_t* buf, int frames);
    static bool _isReady_1(void);

    // --------------- Ping-pong buffer state ---------------
    // _pairBuf[side][pairIdx] points to a stereo interleaved buffer of
    // TDM_INTERLEAVER_FRAMES int32_t pairs (L+R) per stereo pair.
    int32_t* _pairBuf[2][TDM_INTERLEAVER_PAIR_COUNT];  // [ping|pong][pair0..3]

    // Assembled TDM output buffer: TDM_INTERLEAVER_FRAMES × 8 slots × int32_t
    int32_t* _tdmBuf;

    // Ping-pong index: write callbacks use _writeIdx, flush toggles it after TX.
    // volatile + uint8_t: single-byte store is atomic on RISC-V.
    volatile uint8_t _writeIdx;

    // Last frame count passed by the pipeline (set by pair 0, used by flush).
    int _lastFrames;

    uint8_t _i2sPort;
    bool    _ready;
    bool    _initialized;
    uint8_t _instanceIdx;  // Index into _gInterleaverInstances[]; 0xFF = not registered
};

#endif // DAC_ENABLED
