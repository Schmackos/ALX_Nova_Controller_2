#ifndef DSP_REW_PARSER_H
#define DSP_REW_PARSER_H

#ifdef DSP_ENABLED

#include "dsp_pipeline.h"

// ===== Import Functions =====

// Parse Equalizer APO filter text and append stages to channel config.
// Returns number of stages added, or -1 on error.
int dsp_parse_apo_filters(const char *text, DspChannelConfig &channel, uint32_t sampleRate);

// Parse miniDSP biquad coefficient text and append as BIQUAD_CUSTOM stages.
// Returns number of stages added, or -1 on error.
int dsp_parse_minidsp_biquads(const char *text, DspChannelConfig &channel);

// Parse FIR coefficients from text (one float per line) into caller-provided buffer.
// Returns number of taps loaded, or -1 on error.
int dsp_parse_fir_text(const char *text, float *taps, int maxTaps);

// Parse WAV impulse response into FIR taps buffer.
// data: raw WAV file bytes, len: byte count.
// Returns number of taps loaded, or -1 on error.
int dsp_parse_wav_ir(const uint8_t *data, int len, float *taps, int maxTaps, uint32_t expectedSampleRate);

// ===== Export Functions =====

// Export channel's biquad stages as Equalizer APO text.
// buf: output buffer, bufSize: max bytes. Returns chars written.
int dsp_export_apo(const DspChannelConfig &channel, uint32_t sampleRate, char *buf, int bufSize);

// Export channel's biquad stages as miniDSP biquad text.
int dsp_export_minidsp(const DspChannelConfig &channel, char *buf, int bufSize);

#endif // DSP_ENABLED
#endif // DSP_REW_PARSER_H
