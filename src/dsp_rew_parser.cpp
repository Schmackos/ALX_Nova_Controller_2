#ifdef DSP_ENABLED

#include "dsp_rew_parser.h"
#include "dsp_coefficients.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// ===== Helpers =====

static void skip_whitespace(const char *&p) {
    while (*p && (*p == ' ' || *p == '\t')) p++;
}

static bool starts_with_ci(const char *str, const char *prefix) {
    while (*prefix) {
        if (tolower((unsigned char)*str) != tolower((unsigned char)*prefix)) return false;
        str++;
        prefix++;
    }
    return true;
}

static float parse_float(const char *&p) {
    skip_whitespace(p);
    char *end;
    float val = strtof(p, &end);
    p = end;
    return val;
}

static void skip_to_next_line(const char *&p) {
    while (*p && *p != '\n') p++;
    if (*p == '\n') p++;
}

// ===== Equalizer APO Parser =====

// Pattern: Filter N: ON|OFF TYPE Fc FREQ Hz Gain GAIN dB Q QVAL
int dsp_parse_apo_filters(const char *text, DspChannelConfig &channel, uint32_t sampleRate) {
    if (!text) return -1;
    int added = 0;
    const char *p = text;

    while (*p) {
        skip_whitespace(p);

        // Skip empty/comment lines
        if (*p == '\0' || *p == '\n' || *p == '#' || *p == ';') {
            skip_to_next_line(p);
            continue;
        }

        // Expect "Filter N:"
        if (!starts_with_ci(p, "filter")) {
            skip_to_next_line(p);
            continue;
        }
        p += 6; // skip "filter"
        skip_whitespace(p);

        // Skip filter number
        while (*p && isdigit((unsigned char)*p)) p++;
        skip_whitespace(p);
        if (*p == ':') p++;
        skip_whitespace(p);

        // Parse ON/OFF
        bool enabled = true;
        if (starts_with_ci(p, "on")) {
            p += 2;
            enabled = true;
        } else if (starts_with_ci(p, "off")) {
            p += 3;
            enabled = false;
        }
        skip_whitespace(p);

        // Parse filter type
        DspStageType type = DSP_BIQUAD_PEQ;
        if (starts_with_ci(p, "pk")) { type = DSP_BIQUAD_PEQ; p += 2; }
        else if (starts_with_ci(p, "lpq") || starts_with_ci(p, "lp")) {
            type = DSP_BIQUAD_LPF;
            p += (p[2] == 'q' || p[2] == 'Q') ? 3 : 2;
        }
        else if (starts_with_ci(p, "hpq") || starts_with_ci(p, "hp")) {
            type = DSP_BIQUAD_HPF;
            p += (p[2] == 'q' || p[2] == 'Q') ? 3 : 2;
        }
        else if (starts_with_ci(p, "lsc") || starts_with_ci(p, "ls")) {
            type = DSP_BIQUAD_LOW_SHELF;
            p += (p[2] == 'c' || p[2] == 'C') ? 3 : 2;
        }
        else if (starts_with_ci(p, "hsc") || starts_with_ci(p, "hs")) {
            type = DSP_BIQUAD_HIGH_SHELF;
            p += (p[2] == 'c' || p[2] == 'C') ? 3 : 2;
        }
        else if (starts_with_ci(p, "no")) { type = DSP_BIQUAD_NOTCH; p += 2; }
        else if (starts_with_ci(p, "ap")) { type = DSP_BIQUAD_ALLPASS; p += 2; }
        else {
            skip_to_next_line(p);
            continue;
        }
        skip_whitespace(p);

        // Parse "Fc FREQ Hz"
        float freq = 1000.0f;
        if (starts_with_ci(p, "fc")) {
            p += 2;
            freq = parse_float(p);
            skip_whitespace(p);
            if (starts_with_ci(p, "hz")) p += 2;
        }
        skip_whitespace(p);

        // Parse optional "Gain GAIN dB"
        float gain = 0.0f;
        if (starts_with_ci(p, "gain")) {
            p += 4;
            gain = parse_float(p);
            skip_whitespace(p);
            if (starts_with_ci(p, "db")) p += 2;
        }
        skip_whitespace(p);

        // Parse optional "Q QVAL" or "BW Oct BWVAL"
        float Q = DSP_DEFAULT_Q;
        if (starts_with_ci(p, "q")) {
            p += 1;
            Q = parse_float(p);
        } else if (starts_with_ci(p, "bw oct")) {
            p += 6;
            float bw = parse_float(p);
            // Convert octave bandwidth to Q
            if (bw > 0.0f) {
                float x = powf(2.0f, bw);
                Q = sqrtf(x) / (x - 1.0f);
            }
        }

        // Check stage limit
        if (channel.stageCount >= DSP_MAX_STAGES) {
            skip_to_next_line(p);
            continue;
        }

        // Create stage
        DspStage &s = channel.stages[channel.stageCount];
        dsp_init_stage(s, type);
        s.enabled = enabled;
        s.biquad.frequency = freq;
        s.biquad.gain = gain;
        s.biquad.Q = Q;
        dsp_compute_biquad_coeffs(s.biquad, type, sampleRate);
        channel.stageCount++;
        added++;

        skip_to_next_line(p);
    }

    return added;
}

// ===== miniDSP Biquad Parser =====

// Pattern: biquadN, b0=VAL, b1=VAL, b2=VAL, a1=VAL, a2=VAL
int dsp_parse_minidsp_biquads(const char *text, DspChannelConfig &channel) {
    if (!text) return -1;
    int added = 0;
    const char *p = text;

    while (*p) {
        skip_whitespace(p);
        if (*p == '\0' || *p == '\n' || *p == '#' || *p == ';') {
            skip_to_next_line(p);
            continue;
        }

        if (!starts_with_ci(p, "biquad")) {
            skip_to_next_line(p);
            continue;
        }

        // Skip "biquadN,"
        while (*p && *p != ',') p++;
        if (*p == ',') p++;

        float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        bool gotAll = false;

        // Parse comma-separated key=value pairs
        for (int i = 0; i < 5 && *p; i++) {
            skip_whitespace(p);
            // Find '='
            while (*p && *p != '=' && *p != '\n') p++;
            if (*p != '=') break;
            p++; // skip '='
            float val = parse_float(p);

            switch (i) {
                case 0: b0 = val; break;
                case 1: b1 = val; break;
                case 2: b2 = val; break;
                case 3: a1 = val; break;
                case 4: a2 = val; gotAll = true; break;
            }

            // Skip to comma or end of line
            while (*p && *p != ',' && *p != '\n') p++;
            if (*p == ',') p++;
        }

        if (gotAll && channel.stageCount < DSP_MAX_STAGES) {
            DspStage &s = channel.stages[channel.stageCount];
            dsp_init_stage(s, DSP_BIQUAD_CUSTOM);
            // miniDSP negates a1/a2 relative to standard form
            dsp_load_custom_coeffs(s.biquad, b0, b1, b2, -a1, -a2);
            channel.stageCount++;
            added++;
        }

        skip_to_next_line(p);
    }

    return added;
}

// ===== FIR Text Parser =====

int dsp_parse_fir_text(const char *text, float *taps_buf, int maxTaps) {
    if (!text || !taps_buf || maxTaps <= 0) return -1;
    const char *p = text;
    int taps = 0;

    while (*p && taps < maxTaps) {
        skip_whitespace(p);
        if (*p == '\0') break;
        if (*p == '\n' || *p == '#' || *p == ';') {
            skip_to_next_line(p);
            continue;
        }

        char *end;
        float val = strtof(p, &end);
        if (end == p) {
            // Not a valid float — skip line
            skip_to_next_line(p);
            continue;
        }

        taps_buf[taps] = val;
        taps++;
        p = end;
        skip_to_next_line(p);
    }

    return taps;
}

// ===== WAV IR Parser =====

int dsp_parse_wav_ir(const uint8_t *data, int len, float *taps_buf, int maxTaps, uint32_t expectedSampleRate) {
    if (!data || len < 44 || !taps_buf || maxTaps <= 0) return -1;

    // RIFF header
    if (data[0] != 'R' || data[1] != 'I' || data[2] != 'F' || data[3] != 'F') return -1;
    if (data[8] != 'W' || data[9] != 'A' || data[10] != 'V' || data[11] != 'E') return -1;

    // Find 'fmt ' chunk
    int pos = 12;
    uint16_t audioFormat = 0;
    uint16_t numChannels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;

    while (pos + 8 <= len) {
        uint32_t chunkId = *(uint32_t *)(data + pos);
        uint32_t chunkSize = *(uint32_t *)(data + pos + 4);

        if (chunkId == 0x20746D66) { // "fmt "
            if (pos + 8 + 16 > len) return -1;
            audioFormat = *(uint16_t *)(data + pos + 8);
            numChannels = *(uint16_t *)(data + pos + 10);
            sampleRate = *(uint32_t *)(data + pos + 12);
            bitsPerSample = *(uint16_t *)(data + pos + 22);
        }

        if (chunkId == 0x61746164) { // "data"
            // Validate format
            if (numChannels != 1) return -1; // Mono only
            if (sampleRate != expectedSampleRate) return -1;
            if (audioFormat != 1 && audioFormat != 3) return -1; // PCM or IEEE float

            int dataStart = pos + 8;
            int bytesPerSample = bitsPerSample / 8;
            int totalSamples = chunkSize / bytesPerSample;
            if (totalSamples > maxTaps) totalSamples = maxTaps;

            for (int i = 0; i < totalSamples && dataStart + (i + 1) * bytesPerSample <= len; i++) {
                const uint8_t *sp = data + dataStart + i * bytesPerSample;
                if (audioFormat == 3 && bitsPerSample == 32) {
                    // 32-bit float
                    float val;
                    memcpy(&val, sp, 4);
                    taps_buf[i] = val;
                } else if (audioFormat == 1 && bitsPerSample == 16) {
                    // 16-bit PCM
                    int16_t s16 = (int16_t)(sp[0] | (sp[1] << 8));
                    taps_buf[i] = (float)s16 / 32768.0f;
                } else if (audioFormat == 1 && bitsPerSample == 32) {
                    // 32-bit PCM
                    int32_t s32 = (int32_t)(sp[0] | (sp[1] << 8) | (sp[2] << 16) | (sp[3] << 24));
                    taps_buf[i] = (float)s32 / 2147483648.0f;
                } else {
                    return -1; // Unsupported format
                }
            }

            return totalSamples;
        }

        pos += 8 + chunkSize;
        if (chunkSize & 1) pos++; // Pad to even boundary
    }

    return -1; // No data chunk found
}

// ===== Equalizer APO Export =====

static const char *stage_type_to_apo(DspStageType type) {
    switch (type) {
        case DSP_BIQUAD_PEQ:        return "PK";
        case DSP_BIQUAD_LPF:        return "LPQ";
        case DSP_BIQUAD_HPF:        return "HPQ";
        case DSP_BIQUAD_LOW_SHELF:  return "LSC";
        case DSP_BIQUAD_HIGH_SHELF: return "HSC";
        case DSP_BIQUAD_NOTCH:      return "NO";
        case DSP_BIQUAD_ALLPASS:    return "AP";
        case DSP_BIQUAD_BPF:        return "PK"; // BPF exported as PK approximation
        default: return NULL;
    }
}

int dsp_export_apo(const DspChannelConfig &channel, uint32_t sampleRate, char *buf, int bufSize) {
    if (!buf || bufSize <= 0) return -1;
    int written = 0;
    int filterNum = 1;

    for (int i = 0; i < channel.stageCount; i++) {
        const DspStage &s = channel.stages[i];
        if (s.type > DSP_BIQUAD_CUSTOM) continue;

        const char *typeName = stage_type_to_apo(s.type);
        if (!typeName) continue;

        int n;
        if (s.type == DSP_BIQUAD_PEQ || s.type == DSP_BIQUAD_LOW_SHELF ||
            s.type == DSP_BIQUAD_HIGH_SHELF) {
            n = snprintf(buf + written, bufSize - written,
                         "Filter %d: %s %s Fc %.2f Hz Gain %.1f dB Q %.4f\n",
                         filterNum, s.enabled ? "ON" : "OFF", typeName,
                         s.biquad.frequency, s.biquad.gain, s.biquad.Q);
        } else {
            n = snprintf(buf + written, bufSize - written,
                         "Filter %d: %s %s Fc %.2f Hz Q %.4f\n",
                         filterNum, s.enabled ? "ON" : "OFF", typeName,
                         s.biquad.frequency, s.biquad.Q);
        }
        if (n > 0 && written + n < bufSize) {
            written += n;
        }
        filterNum++;
    }

    if (written < bufSize) buf[written] = '\0';
    return written;
}

// ===== miniDSP Export =====

int dsp_export_minidsp(const DspChannelConfig &channel, char *buf, int bufSize) {
    if (!buf || bufSize <= 0) return -1;
    int written = 0;
    int biquadNum = 1;

    for (int i = 0; i < channel.stageCount; i++) {
        const DspStage &s = channel.stages[i];
        if (s.type > DSP_BIQUAD_CUSTOM) continue;

        // miniDSP negates a1/a2
        int n = snprintf(buf + written, bufSize - written,
                         "biquad%d, b0=%.10f, b1=%.10f, b2=%.10f, a1=%.10f, a2=%.10f\n",
                         biquadNum,
                         s.biquad.coeffs[0], s.biquad.coeffs[1], s.biquad.coeffs[2],
                         -s.biquad.coeffs[3], -s.biquad.coeffs[4]);
        if (n > 0 && written + n < bufSize) {
            written += n;
        }
        biquadNum++;
    }

    if (written < bufSize) buf[written] = '\0';
    return written;
}

#endif // DSP_ENABLED
