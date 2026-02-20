# ALX Nova Audio Pipeline

```mermaid
graph LR
    subgraph CORE1["⚙ Core 1 — audio_cap task  ·  priority 3  ·  ~5.3 ms/buffer"]
        subgraph INPUTS["Input Acquisition  (NUM_AUDIO_INPUTS = 3)"]
            A1["PCM1808 ADC1\nI2S_NUM_0  ·  i2s_read()\nBCK=16 WS=18 DOUT=17"]
            A2["PCM1808 ADC2\nI2S_NUM_1  ·  i2s_read()\nDOUT2=9  (shared clocks)"]
            UA["USB Audio\nTinyUSB UAC2  ·  GPIO 19/20\nSPSC ring 1024 frames → int32"]
        end

        SG["Signal Generator\n(conditional · targeted)\nsiggen_is_active() && software_mode\nSine / Square / Noise / Sweep"]

        subgraph PERINPUT["Per-Input Processing  ×3  —  process_adc_buffer()"]
            PB["Diagnostics + DC-block IIR\nSilence fast-path gate\nRMS · VU ballistics · Waveform · FFT\nHealth status derivation"]
            DP["dsp_process_buffer()\nDeinterleave  int32 → float  [-1, +1]\n24 DSP stage types  ·  Emergency limiter\n→  _postDspChannels[6][256]  (PSRAM)"]
        end

        subgraph ROUTE["Routing & DAC Output"]
            RT["dsp_routing_execute()\n6×6 SIMD routing matrix\ndsps_mulc / dsps_add  ·  ~18 µs/buffer"]
            DW["dac_output_write()\nI2S TX  ·  GPIO 40\nPCM5102A"]
        end
    end

    subgraph CORE0["⚙ Core 0 — loop task  ·  5 ms poll"]
        SS["detectSignal()\nSmart Sensing FSM\nIDLE → SIGNAL_DETECTED → AUTO_OFF_TIMER"]
        AR["Amplifier Relay\nGPIO 4"]
        WB["WebSocket Broadcast  :81\naudioLevels JSON\n+ waveform 258 B binary  (type 0x01)\n+ spectrum  70 B binary  (type 0x02)"]
    end

    A1 -->|"int32 stereo · 256 frames · 2 KB\nleft-justified 24-bit  (>> 8)"| PB
    A2 -->|"int32 stereo · 256 frames · 2 KB"| PB
    UA -->|"PCM16/24 → int32 · 256 frames · 2 KB\nhost volume applied"| PB
    SG -.->|"optional buffer override\nsiggen_fill_buffer()"| PB
    PB --> DP
    DP -->|"float[6][256]  PSRAM\nch0-1 = ADC1  ·  ch2-3 = ADC2  ·  ch4-5 = USB"| RT
    RT -->|"float[2][256] → int32 stereo · 2 KB\nre-interleaved  (<< 8)"| DW
    PB -->|"_analysisReady flag\n(dirty flag, cross-core)"| SS
    PB -.->|"VU / waveform / spectrum\ndirty flags"| WB
    SS --> AR
```

## Data Flow Summary

| Stage | Format | Size |
|-------|--------|------|
| I2S read (ADC1/2) | `int32` stereo interleaved, 24-bit left-justified | 256 frames · 2 KB |
| USB read | `int32` stereo, converted from PCM16/24 | 256 frames · 2 KB |
| DSP input | `float` deinterleaved, normalised `[-1, +1]` | per-channel `float[256]` |
| Post-DSP / routing | `float[6][256]` — 6 channels in PSRAM | 6 KB |
| DAC buffer | `int32` stereo re-interleaved | 256 frames · 2 KB |
| WebSocket waveform | `uint8[256]` + 2-byte header | 258 B binary |
| WebSocket spectrum | `uint8[16×2]` bands + 2-byte header + freq | 70 B binary |

## Key Constants (`src/config.h`)

| Constant | Value | Scope |
|----------|-------|-------|
| `NUM_AUDIO_ADCS` | 2 | I2S hardware driver, DMA, clock config **only** |
| `NUM_AUDIO_INPUTS` | 3 | Metering arrays, WS/MQTT/REST, AppState, routing |
| `DSP_MAX_CHANNELS` | 6 | L1 R1 L2 R2 USB_L USB_R |
| `I2S_DMA_BUF_LEN` | 256 | frames per DMA buffer |
| `I2S_DMA_BUF_COUNT` | 8 | DMA buffers (~42 ms total runway) |
