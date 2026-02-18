# Roadmap: ALX Nova — Spotify Connect Streaming

## Overview

Integrating Spotify Connect into the ALX Nova requires building from the ground up: verifying the build environment can hold the cspot library, constructing the PSRAM data bridge and AppState extensions, integrating cspot as an ESP-IDF component, wiring the audio path through the existing DSP pipeline, hardening session management, extending the control plane to all interfaces (WebSocket, MQTT, REST, HA), delivering the TFT and Web UI surfaces, and finally enabling the auto-switch and amplifier relay intelligence that makes the feature production-grade. Each phase delivers a coherent, independently verifiable capability before the next begins.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Infrastructure Gating** - Confirm flash, heap, and build flag prerequisites are met before any cspot code is written
- [ ] **Phase 2: Ring Buffer + AppState** - Build the PSRAM data bridge and all AppState/settings extensions, fully native-tested
- [ ] **Phase 3: cspot Build + ZeroConf** - Integrate cspot as an ESP-IDF component, compile successfully, and advertise the device on the network
- [ ] **Phase 4: Audio Path** - Wire feedPCMFrames through the ring buffer into the DSP pipeline and out the DAC
- [ ] **Phase 5: Session Management** - Persist credentials, enable auto-reconnect, and harden against silent session death
- [ ] **Phase 6: Control Plane** - Extend all interfaces (WebSocket, MQTT, REST, HA) with Spotify state and controls
- [ ] **Phase 7: GUI + Web UI Surfaces** - Deliver the TFT screen and Web UI browsing and now-playing displays
- [ ] **Phase 8: Auto-switch + Amp Integration** - Enable source auto-switching, conflict handling, and amplifier relay intelligence

## Phase Details

### Phase 1: Infrastructure Gating
**Goal**: The build environment is proven capable of hosting cspot — flash headroom confirmed, internal SRAM headroom confirmed, and the feature flag scaffolding is in place
**Depends on**: Nothing (first phase)
**Requirements**: BLD-02, BLD-03, BLD-05
**Success Criteria** (what must be TRUE):
  1. A custom partition table using the N16's 16MB flash is in place (or existing table confirmed sufficient), and projected post-cspot firmware fits within the OTA partition with margin
  2. `heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)` reports 80KB+ contiguous after all existing modules initialize — measured and logged at boot
  3. Building with `-D SPOTIFY_CONNECT_ENABLED` compiles cleanly as a no-op (stubs only, no functionality yet) and the flag is documented in platformio.ini
**Plans**: TBD

Plans:
- [ ] 01-01: Flash audit + custom partition table (BLD-02)
- [ ] 01-02: Internal SRAM headroom audit and PSRAM offload pass (BLD-03)
- [ ] 01-03: SPOTIFY_CONNECT_ENABLED build flag scaffolding (BLD-05)

### Phase 2: Ring Buffer + AppState
**Goal**: The PSRAM SPSC ring buffer, PCM format conversion, AppState Spotify fields, AudioSourceEnum, and all settings/persistence structures exist and are fully native-tested — no cspot dependency required
**Depends on**: Phase 1
**Requirements**: PIPE-01, PIPE-03, SRC-01, SET-01, SET-02, SET-03, SET-04
**Success Criteria** (what must be TRUE):
  1. `pio test -e native -f test_spotify_core` passes — ring buffer write/read/available, PCM16-to-int32 conversion, and AppState field initialization all verified
  2. `AudioSourceEnum` with ADC1, ADC2, USB Audio, and Spotify variants is in AppState and accessible from all modules
  3. Settings export/import includes Spotify fields; factory reset clears `/spotify_creds.bin`; `pio test -e native -f test_settings` still passes
**Plans**: TBD

Plans:
- [ ] 02-01: PSRAM SPSC ring buffer (`spotify_rb_*` API) + PCM16-to-int32 conversion (PIPE-01, PIPE-03)
- [ ] 02-02: AppState extensions — `spotifyActive/Playing/TrackName/ArtistName/Volume/Dirty` + AudioSourceEnum (SRC-01)
- [ ] 02-03: Settings persistence — `/spotify_creds.bin`, settings export/import, factory reset (SET-01, SET-02, SET-03, SET-04)

### Phase 3: cspot Build + ZeroConf
**Goal**: cspot compiles as an ESP-IDF component inside the PlatformIO dual-framework build, the device advertises itself on the network, and all cspot FreeRTOS tasks are WDT-registered
**Depends on**: Phase 2
**Requirements**: BLD-01, BLD-04, STRM-01, STRM-05, STRM-07
**Success Criteria** (what must be TRUE):
  1. `pio run` with `framework = arduino, espidf` succeeds — cspot and all bell transitive dependencies compile without errors or conflicts with existing pre-built libraries (libespressif__esp-dsp.a, libarduino_tinyusb.a)
  2. The device appears in `dns-sd -B _spotify-connect._tcp` output on the local network within 10 seconds of WiFi connection
  3. The Spotify mobile or desktop app lists the device by its configured name (defaulting to WiFi hostname) in the Connect menu
  4. All cspot FreeRTOS tasks are registered with the hardware watchdog and yield within 15-second intervals — confirmed via task monitor
**Plans**: TBD

Plans:
- [ ] 03-01: PlatformIO dual-framework build + cspot as ESP-IDF component (BLD-01)
- [ ] 03-02: `spotify_connect.h/.cpp` skeleton — ZeroConf HTTP (port 8080), mDNS `_spotify-connect._tcp` registration on existing mDNS instance, device name config (STRM-01, STRM-05, STRM-07)
- [ ] 03-03: cspot task sizing + WDT registration (BLD-04)

### Phase 4: Audio Path
**Goal**: Spotify audio streams from the app, decodes through cspot, passes through the PSRAM ring buffer into the DSP pipeline, and plays through the PCM5102A DAC
**Depends on**: Phase 3
**Requirements**: STRM-02, STRM-03, PIPE-02, PIPE-04
**Success Criteria** (what must be TRUE):
  1. Playing a track on Spotify and selecting the device produces audible audio output through the amplifier with no glitching or dropout within the first 30 seconds
  2. Audio passes visibly through the DSP pipeline — DSP gain, EQ, and limiter stages affect the Spotify output identically to ADC input
  3. I2S TX sample rate switches to 44.1kHz when Spotify is active (confirmed via audio diagnostics or serial log) and reverts correctly when switching away
  4. Audio quality setting (96 / 160 / 320 kbps) changes the stream quality and is reflected in the Web UI
**Plans**: TBD

Plans:
- [ ] 04-01: SpotifyAudioSink — `feedPCMFrames()` → PCM16-to-int32 → ring buffer write (STRM-02, PIPE-02)
- [ ] 04-02: `audio_capture_task` source priority switch — Spotify > USB > ADC; I2S TX sample rate reinit via `audioPaused` flag (PIPE-04)
- [ ] 04-03: Audio quality (bitrate) selection — AppState field, cspot config, REST API (STRM-03)

### Phase 5: Session Management
**Goal**: The device reconnects automatically after reboot without user re-pairing, supports optional Web UI credential entry for standalone mode, and recovers autonomously from silent session death
**Depends on**: Phase 4
**Requirements**: STRM-04, STRM-06, STRM-08, BLD-06
**Success Criteria** (what must be TRUE):
  1. After a successful first pairing and reboot, the device reconnects to Spotify and resumes playback without any action required from the user or Spotify app
  2. Playback continues for at least 90 minutes without user interaction — the session health watchdog detects and recovers from any silent death within 90 seconds
  3. When the controlling Spotify app leaves the network mid-session, playback continues uninterrupted (device streams directly from Spotify servers)
  4. Web UI credential entry (standalone mode) allows browsing without a prior Spotify app pairing
**Plans**: TBD

Plans:
- [ ] 05-01: Credential persistence — `/spotify_creds.bin` write on auth success, load on boot + auto-reconnect (STRM-04)
- [ ] 05-02: Session health watchdog — 90-second silence timeout triggers full cspot restart; "Spotify disconnected" state visible in AppState (BLD-06, STRM-06)
- [ ] 05-03: Web UI credential entry for standalone mode (STRM-08)

### Phase 6: Control Plane
**Goal**: All interfaces expose Spotify state and controls — WebSocket broadcasts track info, MQTT exposes a Home Assistant media_player entity with discovery, REST API covers all Spotify settings, and input source selection is visible everywhere
**Depends on**: Phase 5
**Requirements**: CTRL-01, CTRL-02, CTRL-03, CTRL-05, HA-01, HA-02, HA-03, HA-04, WEB-01, WEB-02, WEB-04, WEB-05, SRC-02
**Success Criteria** (what must be TRUE):
  1. Play/pause and skip next/previous commands sent from the Web UI, REST API, or MQTT take effect within 2 seconds and the new state reflects back on all interfaces
  2. Volume change via the rotary encoder knob updates the Spotify volume and the change is visible in the Spotify app within 3 seconds (bidirectional volume sync)
  3. Home Assistant shows a `media_player` entity for the device with track title, artist, playback state, and working play/pause/skip/volume controls
  4. The Web UI "Streaming" card shows connection state, current track info, and quality setting; all Spotify settings (device name, quality, auto-switch, conflict handling) are configurable from the Settings or Streaming section
  5. `SPOTIFY` appears as a selectable input source in the Web UI, REST API, and MQTT — switching to it or away from it works
**Plans**: TBD

Plans:
- [ ] 06-01: SpircHandler → AppState bridge + `spotifyDirty` flag + `sendSpotifyState()` WebSocket broadcast (CTRL-05, WEB-05)
- [ ] 06-02: Playback controls — CTRL-01, CTRL-02 across Web UI, REST API, MQTT; bidirectional volume sync CTRL-03
- [ ] 06-03: HA MQTT Discovery — media_player entity, input_source select, quality setting (HA-01, HA-02, HA-03, HA-04)
- [ ] 06-04: Web UI Streaming card + Settings integration + SRC-02 input source selector (WEB-01, WEB-02, WEB-04, SRC-02)

### Phase 7: GUI + Web UI Surfaces
**Goal**: The TFT screen displays track metadata with Spotify attribution and provides a full control screen with playlist/album browsing; the Web UI playlist and album browser works end-to-end
**Depends on**: Phase 6
**Requirements**: CTRL-04, CTRL-06, CTRL-07, GUI-01, GUI-02, GUI-03, GUI-04, WEB-03
**Success Criteria** (what must be TRUE):
  1. When Spotify is the active source, the TFT Home/Desktop screen shows the current track name and artist with a "Spotify" attribution text or icon visible at all times (Spotify display spec compliance)
  2. Navigating to the Spotify control screen via the rotary encoder shows play/pause/skip/volume controls and a scrollable playlist/album list that can be navigated and selected
  3. The active input indicator on the TFT shows "Spotify" when Spotify is the active source
  4. The Web UI Streaming section includes a working playlist and album browser — user can browse and select a playlist/album to play
**Plans**: TBD

Plans:
- [ ] 07-01: TFT now-playing display on Home screen + Spotify attribution + input source indicator (GUI-01, GUI-03, GUI-04, CTRL-04)
- [ ] 07-02: `scr_spotify.h/.cpp` — LVGL control screen with play/pause/skip/volume + scrollable playlist/album browser (GUI-02, CTRL-07)
- [ ] 07-03: Web UI playlist and album browser in Streaming section (CTRL-06, WEB-03)

### Phase 8: Auto-switch + Amp Integration
**Goal**: The device intelligently manages source conflicts, auto-switches to Spotify when playback starts, and keeps the amplifier relay energized during Spotify sessions — the feature behaves correctly in all real-world usage scenarios
**Depends on**: Phase 7
**Requirements**: SRC-03, SRC-04, SRC-05, AMP-01, AMP-02
**Success Criteria** (what must be TRUE):
  1. With auto-switch enabled, starting Spotify playback on the app automatically selects Spotify as the active input and the user hears audio without any manual source change
  2. With conflict handling configured for notification mode, starting Spotify while ADC is active shows a conflict indicator in the Web UI and TFT; with auto-takeover mode, Spotify takes over automatically
  3. The amplifier relay remains energized for the duration of a Spotify session and does not trigger the auto-off timer while a track is playing
  4. When switching away from Spotify to ADC1, ADC2, or USB Audio, the I2S TX sample rate reverts correctly and audio plays from the new source within 3 seconds
**Plans**: TBD

Plans:
- [ ] 08-01: Auto-switch toggle — AppState field, enable/disable logic, Web UI control (SRC-03)
- [ ] 08-02: Conflict handling toggle — notification vs auto-takeover modes; revert sample rate on source switch (SRC-04, SRC-05)
- [ ] 08-03: Amplifier relay integration — Spotify playback state feeds smart sensing FSM as "signal present"; auto-off timer behavior (AMP-01, AMP-02)

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Infrastructure Gating | 0/3 | Not started | - |
| 2. Ring Buffer + AppState | 0/3 | Not started | - |
| 3. cspot Build + ZeroConf | 0/3 | Not started | - |
| 4. Audio Path | 0/3 | Not started | - |
| 5. Session Management | 0/3 | Not started | - |
| 6. Control Plane | 0/4 | Not started | - |
| 7. GUI + Web UI Surfaces | 0/3 | Not started | - |
| 8. Auto-switch + Amp Integration | 0/3 | Not started | - |
