# ALX Nova Controller — Spotify Connect & Streaming Input

## What This Is

A Spotify Connect streaming integration for the ALX Nova ESP32-S3 amplifier controller. The device becomes discoverable as a Spotify Connect speaker on the local network, allowing users to stream music from the Spotify app directly to the amplifier. The streaming input is treated as a first-class audio input alongside the existing I2S ADC inputs, feeding through the full DSP pipeline to the DAC output. Bluetooth A2DP will follow as a separate future phase.

## Core Value

The device must reliably appear as a Spotify Connect target and deliver uninterrupted audio streaming through the existing DSP pipeline to the DAC output — the core listening experience cannot drop, glitch, or stall.

## Requirements

### Validated

(None yet — ship to validate)

### Active

- [ ] Device is discoverable as a Spotify Connect speaker via mDNS/ZeroConf
- [ ] Spotify audio streams to the device and plays through the DAC output
- [ ] Streaming input integrates into the existing audio pipeline as an input source (like ADC1/ADC2/USB)
- [ ] Audio passes through the full DSP pipeline (EQ, gain, limiter, crossover, etc.)
- [ ] Playback controls: play, pause, skip next/previous from all interfaces
- [ ] Volume control from the device (independent of Spotify app volume)
- [ ] Track metadata display (artist, track name, album) on Web UI and TFT
- [ ] Playlist and album browsing from Web UI and TFT screen
- [ ] Input source selection: manual switching between ADC1, ADC2, USB Audio, Spotify
- [ ] Auto-switch toggle: when Spotify starts streaming, optionally auto-select as active input
- [ ] Conflict handling toggle: when ADC is active and Spotify connects, notify user or auto-takeover
- [ ] Device name configurable in settings (defaults to WiFi hostname)
- [ ] Authentication: ZeroConf discovery as default, optional Web UI credential entry for standalone mode
- [ ] Audio quality configurable (96 / 160 / 320 kbps)
- [ ] Full configuration via Web UI and REST API
- [ ] Basic playback controls via MQTT (play/pause/skip/volume) with Home Assistant discovery
- [ ] Basic playback controls and status display on TFT screen
- [ ] Streaming section in Web UI as a dedicated tab/card

### Out of Scope

- Bluetooth A2DP streaming — separate future milestone, not part of this work
- AirPlay / Chromecast / DLNA support — different protocols, not planned
- Spotify Free tier support — cspot requires Premium, by design
- Full text search from TFT screen — too constrained at 160x128; playlists + albums only
- Multi-room / group playback — complex sync protocol, defer to future
- Offline playback / caching — ESP32 storage too limited
- Spotify podcast support — audio-only music streaming focus

## Context

### Existing Architecture
- ESP32-S3 WROOM (N16R8) with 8MB PSRAM, running Arduino framework via PlatformIO
- Dual I2S ADC inputs (PCM1808, 24-bit) + USB Audio input (TinyUSB UAC2) already implemented
- Full DSP pipeline: biquad IIR, FIR, gain, limiter, compressor, crossover, routing matrix
- DAC output via PCM5102A on I2S_NUM_0 (full-duplex TX)
- Web UI served from ESP32 (gzipped HTML/JS), WebSocket on port 81, REST API on port 80
- MQTT with Home Assistant auto-discovery
- LVGL v9.4 TFT GUI on ST7735S 128x160 with rotary encoder
- AppState singleton with dirty flags for cross-module communication

### Key Integration Points
- cspot library outputs 44.1kHz 16-bit stereo PCM — needs conversion to match existing 24-bit left-justified I2S format
- Audio sample rate may differ from ADC rate (44.1kHz vs whatever ADC is configured at) — may need resampling or rate-matching
- cspot expects an `AudioSink` interface to be provided — natural integration point
- PSRAM available for audio buffers (ring buffer, decode buffer)
- WiFi must remain stable during streaming — cspot needs persistent TCP connection to Spotify servers

### cspot Library
- C++ Spotify Connect library targeting embedded devices
- GitHub: feelfreelinux/cspot, 607 stars, actively developed
- Uses mDNS/ZeroConf for device advertisement
- Outputs raw PCM (44100 Hz, 16-bit stereo)
- ESP-IDF build support with menuconfig
- Integrated successfully in squeezelite-esp32 project (ESP32-S3 confirmed working)

## Constraints

- **Memory**: ESP32-S3 RAM at ~44% usage, PSRAM available. Spotify decode + network buffers must fit alongside existing audio + DSP + GUI tasks
- **CPU**: Core 0 runs main loop + WiFi + USB audio; Core 1 runs audio capture + GUI. Spotify decode task needs careful core assignment to avoid starving existing tasks
- **Network**: Spotify streaming requires stable WiFi. Competing with MQTT heartbeats, WebSocket broadcasts, OTA checks
- **Library**: cspot is the only viable ESP32 Spotify Connect library. If it doesn't work on S3, there's no fallback
- **Spotify Premium**: Required — this is a hard dependency, not a limitation we can work around
- **Sample Rate**: cspot outputs 44.1kHz; existing ADC pipeline may run at different rate. Format bridging needed
- **TFT Display**: 160x128 pixels limits browsing UI to simple list navigation (playlists, albums) — no album art, no rich layout

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Use cspot library | Only viable ESP32 Spotify Connect implementation; proven in squeezelite-esp32 | — Pending |
| Treat Spotify as input source | Consistent with existing ADC/USB input architecture; gets DSP for free | — Pending |
| ZeroConf + optional Web UI auth | ZeroConf is standard Spotify Connect UX; Web UI login enables standalone browsing | — Pending |
| Bluetooth A2DP in separate milestone | Reduces scope; Spotify Connect is complex enough on its own | — Pending |
| Playlists + albums browsing (no full search) | TFT screen too small for keyboard-heavy search; Web UI can do more | — Pending |
| Configurable audio quality | Users may prefer bandwidth savings on slow networks; 320kbps default for audiophile use | — Pending |

---
*Last updated: 2026-02-17 after initialization*
