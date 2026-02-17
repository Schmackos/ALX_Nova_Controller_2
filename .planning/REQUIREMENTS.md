# Requirements: ALX Nova — Spotify Connect Streaming

**Defined:** 2026-02-17
**Core Value:** The device must reliably appear as a Spotify Connect target and deliver uninterrupted audio streaming through the existing DSP pipeline to the DAC output

## v1 Requirements

Requirements for initial release. Each maps to roadmap phases.

### Streaming Core

- [ ] **STRM-01**: Device advertises itself as a Spotify Connect speaker via mDNS/ZeroConf (`_spotify-connect._tcp`)
- [ ] **STRM-02**: Spotify audio streams from Spotify servers, decodes Ogg Vorbis, and outputs PCM to the DAC via the existing DSP pipeline
- [ ] **STRM-03**: Audio quality is configurable (96 / 160 / 320 kbps) via Web UI and REST API
- [ ] **STRM-04**: Spotify authentication credentials persist across reboots (no re-pairing from Spotify app needed)
- [ ] **STRM-05**: ZeroConf pairing works via Spotify mobile/desktop app on the local network
- [ ] **STRM-06**: Playback continues when the controlling Spotify app leaves the network (device streams directly from Spotify servers)
- [ ] **STRM-07**: Device name is configurable in settings (defaults to WiFi hostname)
- [ ] **STRM-08**: Optional Web UI credential entry for standalone mode (browsing without prior Spotify app pairing)

### Audio Pipeline

- [ ] **PIPE-01**: Spotify PCM audio (44.1kHz 16-bit stereo) is converted to the existing I2S format (24-bit left-justified int32) via format bridging
- [ ] **PIPE-02**: Spotify audio passes through the full DSP pipeline (EQ, gain, limiter, crossover, compressor, routing matrix)
- [ ] **PIPE-03**: SPSC ring buffer in PSRAM bridges cspot decode task (Core 0) to audio_capture_task (Core 1)
- [ ] **PIPE-04**: I2S TX sample rate switches to 44.1kHz when Spotify is the active source (using existing audioPaused flag pattern)

### Playback Controls

- [ ] **CTRL-01**: Play/pause control from Web UI, REST API, MQTT, and TFT screen
- [ ] **CTRL-02**: Skip next/previous track from Web UI, REST API, MQTT, and TFT screen
- [ ] **CTRL-03**: Bidirectional volume sync: hardware encoder knob changes Spotify volume, Spotify app volume changes reflect on device
- [ ] **CTRL-04**: Track metadata (artist, track name, album) displayed on TFT screen with Spotify attribution
- [ ] **CTRL-05**: Track metadata displayed in Web UI Spotify status card
- [ ] **CTRL-06**: Playlist and album browsing from Web UI
- [ ] **CTRL-07**: Playlist and album browsing from TFT screen (scrollable list, rotary encoder navigation)

### Input Source Management

- [ ] **SRC-01**: AudioSourceEnum added to AppState: ADC1, ADC2, USB Audio, Spotify
- [ ] **SRC-02**: Manual input source selection via Web UI, REST API, MQTT, and TFT screen
- [ ] **SRC-03**: Auto-switch toggle: when Spotify starts streaming, optionally auto-select Spotify as active input
- [ ] **SRC-04**: Conflict handling toggle: when ADC is active and Spotify connects, notify user to choose (configurable to auto-takeover)
- [ ] **SRC-05**: When switching away from Spotify, I2S TX sample rate reverts to the appropriate rate for the new source

### Home Assistant Integration

- [ ] **HA-01**: MQTT media_player entity with HA auto-discovery (track title, artist, playback state)
- [ ] **HA-02**: MQTT playback controls (play/pause/skip/volume) mapped to Spotify via SpircHandler
- [ ] **HA-03**: MQTT input_source select entity includes "Spotify" option
- [ ] **HA-04**: MQTT Spotify quality setting (number or select entity)

### Amplifier Integration

- [ ] **AMP-01**: Spotify playback state feeds into smart sensing as "signal present" (keeps amplifier relay energized)
- [ ] **AMP-02**: When Spotify is idle for the configured auto-off duration, amplifier auto-off timer triggers normally

### Web UI

- [ ] **WEB-01**: "Streaming" section/tab in Web UI with Spotify status card (connection state, track info, quality)
- [ ] **WEB-02**: Playback controls (play/pause/skip/volume) in the Streaming card
- [ ] **WEB-03**: Playlist and album browser in the Streaming section
- [ ] **WEB-04**: Spotify settings (device name, quality, auto-switch, conflict handling) in Settings or Streaming section
- [ ] **WEB-05**: WebSocket `spotifyState` message type for real-time track info updates

### TFT GUI

- [ ] **GUI-01**: Spotify status display on Home/Desktop screen (track name, artist, playback state)
- [ ] **GUI-02**: Spotify control screen accessible from menu (play/pause/skip/volume, playlist browser)
- [ ] **GUI-03**: Spotify attribution (icon or "Spotify" text) when Spotify is active source
- [ ] **GUI-04**: Input source indicator shows "Spotify" when active

### Build & Infrastructure

- [ ] **BLD-01**: cspot library integrated via PlatformIO dual framework (`arduino, espidf`) or as a compatible library
- [ ] **BLD-02**: Flash partition table supports cspot-enabled firmware + OTA (custom partition if needed)
- [ ] **BLD-03**: Internal SRAM headroom verified: 80KB+ contiguous free after cspot init (crypto + WiFi reserve)
- [ ] **BLD-04**: All cspot FreeRTOS tasks registered with watchdog or yield within 15s intervals
- [ ] **BLD-05**: Feature guarded by `-D SPOTIFY_CONNECT_ENABLED` build flag
- [ ] **BLD-06**: Session health watchdog: detect and recover from silent session death (90s silence timeout)

### Settings & Persistence

- [ ] **SET-01**: Spotify settings persisted (device name, quality, auto-switch, conflict handling)
- [ ] **SET-02**: Spotify credential blob stored in LittleFS (`/spotify_creds.bin`)
- [ ] **SET-03**: Settings export/import includes Spotify configuration
- [ ] **SET-04**: Factory reset clears Spotify credentials

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### Bluetooth A2DP

- **BT-01**: Device discoverable as Bluetooth A2DP audio sink
- **BT-02**: Bluetooth audio treated as input source alongside ADC/USB/Spotify
- **BT-03**: Bluetooth audio passes through DSP pipeline

### Enhanced Spotify

- **ESPOT-01**: Album art display in Web UI Spotify card (from Spotify CDN URL)
- **ESPOT-02**: Spotify lossless/FLAC support (pending librespot/cspot upstream support)
- **ESPOT-03**: Session transfer (seamless handoff from phone mid-song)

## Out of Scope

| Feature | Reason |
|---------|--------|
| Full text search from TFT | 160x128 screen too small for keyboard-heavy search; playlists + albums only |
| Multi-room / group playback | Complex sync protocol; single-device scope |
| Offline / cached playback | ESP32 storage too limited; Spotify DRM prevents this |
| Spotify Free tier support | cspot requires Premium; technical limitation |
| AirPlay / Chromecast / DLNA | Different protocols; not in this milestone |
| Spotify podcast chapter navigation | Display too small; podcast audio plays fine as-is |
| Album art on TFT | 160x128 at limited color depth; poor visual result for high complexity |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| BLD-02 | Phase 1 | Pending |
| BLD-03 | Phase 1 | Pending |
| BLD-05 | Phase 1 | Pending |
| PIPE-01 | Phase 2 | Pending |
| PIPE-03 | Phase 2 | Pending |
| SRC-01 | Phase 2 | Pending |
| SET-01 | Phase 2 | Pending |
| SET-02 | Phase 2 | Pending |
| SET-03 | Phase 2 | Pending |
| SET-04 | Phase 2 | Pending |
| BLD-01 | Phase 3 | Pending |
| BLD-04 | Phase 3 | Pending |
| STRM-01 | Phase 3 | Pending |
| STRM-05 | Phase 3 | Pending |
| STRM-07 | Phase 3 | Pending |
| STRM-02 | Phase 4 | Pending |
| STRM-03 | Phase 4 | Pending |
| PIPE-02 | Phase 4 | Pending |
| PIPE-04 | Phase 4 | Pending |
| STRM-04 | Phase 5 | Pending |
| STRM-06 | Phase 5 | Pending |
| STRM-08 | Phase 5 | Pending |
| BLD-06 | Phase 5 | Pending |
| CTRL-01 | Phase 6 | Pending |
| CTRL-02 | Phase 6 | Pending |
| CTRL-03 | Phase 6 | Pending |
| CTRL-05 | Phase 6 | Pending |
| HA-01 | Phase 6 | Pending |
| HA-02 | Phase 6 | Pending |
| HA-03 | Phase 6 | Pending |
| HA-04 | Phase 6 | Pending |
| WEB-01 | Phase 6 | Pending |
| WEB-02 | Phase 6 | Pending |
| WEB-04 | Phase 6 | Pending |
| WEB-05 | Phase 6 | Pending |
| SRC-02 | Phase 6 | Pending |
| CTRL-04 | Phase 7 | Pending |
| CTRL-06 | Phase 7 | Pending |
| CTRL-07 | Phase 7 | Pending |
| GUI-01 | Phase 7 | Pending |
| GUI-02 | Phase 7 | Pending |
| GUI-03 | Phase 7 | Pending |
| GUI-04 | Phase 7 | Pending |
| WEB-03 | Phase 7 | Pending |
| SRC-03 | Phase 8 | Pending |
| SRC-04 | Phase 8 | Pending |
| SRC-05 | Phase 8 | Pending |
| AMP-01 | Phase 8 | Pending |
| AMP-02 | Phase 8 | Pending |

**Coverage:**
- v1 requirements: 49 total (note: header previously stated 37 — actual count from listed requirements is 49)
- Mapped to phases: 49
- Unmapped: 0

---
*Requirements defined: 2026-02-17*
*Last updated: 2026-02-17 after roadmap creation — traceability populated*
