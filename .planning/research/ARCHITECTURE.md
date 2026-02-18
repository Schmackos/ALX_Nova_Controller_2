# Architecture Research

**Domain:** Spotify Connect streaming integration on existing ESP32-S3 embedded audio controller
**Researched:** 2026-02-17
**Confidence:** MEDIUM — cspot interfaces verified via source inspection; ESP32-S3-specific behavior is MEDIUM (some issues documented, fixes not all confirmed in official sources)

---

## Standard Architecture

### System Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          CONTROL PLANE (Core 0)                          │
│                                                                          │
│  ┌──────────────┐  ┌─────────────┐  ┌──────────────┐  ┌─────────────┐  │
│  │  Web UI      │  │  MQTT/HA    │  │  REST API    │  │  GUI (TFT)  │  │
│  │  HTTP:80     │  │  Port 1883  │  │  /api/*      │  │  Core 1     │  │
│  │  WS:81       │  │             │  │              │  │             │  │
│  └──────┬───────┘  └──────┬──────┘  └──────┬───────┘  └──────┬──────┘  │
│         │                 │                 │                 │          │
│  ┌──────┴─────────────────┴─────────────────┴─────────────────┘          │
│  │                    AppState Singleton (dirty flags)                    │
│  └──────┬──────────────────────────────────────────────────────────────  │
│         │                                                                 │
│  ┌──────▼───────────────────────────────────────────────────────────┐    │
│  │           NEW: Spotify Connect Module (spotify_connect.h/.cpp)   │    │
│  │  • cspot library integration                                      │    │
│  │  • ZeroConf HTTP server (port 8080, separate from port 80)       │    │
│  │  • mDNS advertisement (_spotify-connect._tcp)                    │    │
│  │  • SpotifyAudioSink (feedPCMFrames → SPSC ring buffer)          │    │
│  │  • Credentials cache (LittleFS /spotify_creds.bin)              │    │
│  │  • SpircHandler event bridge → AppState dirty flags             │    │
│  └──────┬───────────────────────────────────────────────────────────┘    │
└─────────┼───────────────────────────────────────────────────────────────┘
          │ SPSC ring buffer (PSRAM, ~50ms / ~8820 frames)
          │ [cspot decode task fills] [audio_capture_task drains]
          ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                           AUDIO PLANE (Core 1)                           │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐     │
│  │                    audio_capture_task (priority 3)              │     │
│  │                                                                 │     │
│  │  Source selection (per-iteration):                             │     │
│  │    if spotify_is_playing():  read from Spotify ring buffer     │     │
│  │    elif usb_audio_streaming: read from USB ring buffer         │     │
│  │    else:                     i2s_read() from ADC1/ADC2         │     │
│  │                                                                 │     │
│  │  ┌─────────────────────────────────────────────────────────┐  │     │
│  │  │           Unified int32 stereo PCM buffer               │  │     │
│  │  │           (left-justified, matching ADC format)         │  │     │
│  │  └───────────────────────────┬─────────────────────────────┘  │     │
│  │                              │                                  │     │
│  │  ┌───────────────────────────▼─────────────────────────────┐  │     │
│  │  │              DSP Pipeline (dsp_pipeline.cpp)            │  │     │
│  │  │    EQ / crossover / compressor / limiter / delay        │  │     │
│  │  └───────────────────────────┬─────────────────────────────┘  │     │
│  │                              │                                  │     │
│  │  ┌───────────────────────────▼─────────────────────────────┐  │     │
│  │  │    RMS / dBFS / VU metering / peak hold / FFT           │  │     │
│  │  │    (skipped/reduced when Spotify is active source)      │  │     │
│  │  └───────────────────────────┬─────────────────────────────┘  │     │
│  │                              │                                  │     │
│  │  ┌───────────────────────────▼─────────────────────────────┐  │     │
│  │  │             DAC Output (I2S_NUM_0 TX)                   │  │     │
│  │  │             dac_output_write(buffer, frames)            │  │     │
│  │  └─────────────────────────────────────────────────────────┘  │     │
│  └─────────────────────────────────────────────────────────────────┘     │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐     │
│  │              gui_task (priority 1, Core 1)                      │     │
│  │              LVGL / ST7735S TFT display                         │     │
│  └─────────────────────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────┐
│               SPOTIFY CONNECT TASKS (cspot library)                      │
│                                                                          │
│  ┌─────────────────────────────┐   ┌──────────────────────────────┐      │
│  │  cspot_task (CSpotTask)     │   │  cspot_player (CSpotPlayer)  │      │
│  │  Core 0, priority 1         │   │  Core 0, priority 1          │      │
│  │  Stack: 32KB                │   │  Stack: 8KB                  │      │
│  │  Handles:                   │   │  Reads decoded PCM from      │      │
│  │  • Spotify AP connection    │   │  cspot internal buffer       │      │
│  │  • Shannon/TLS session      │   │  → calls feedPCMFrames()     │      │
│  │  • SpircHandler (SPIRC)     │   │  → writes to Spotify SPSC    │      │
│  │  • Track queue management   │   │  ring buffer (PSRAM)         │      │
│  └─────────────────────────────┘   └──────────────────────────────┘      │
└─────────────────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component | Responsibility | Communicates With |
|-----------|----------------|-------------------|
| `spotify_connect.h/.cpp` | cspot init, ZeroConf HTTP, mDNS, credentials storage, AppState bridge | AppState, cspot library, LittleFS, mDNS |
| `SpotifyAudioSink` (inner class) | Implements cspot `feedPCMFrames()` — converts PCM16→int32, writes to SPSC ring buffer | Spotify SPSC ring buffer (PSRAM) |
| `cspot_task` (bell::Task) | Spotify AP connection, session management, SpircHandler, SPIRC control | Spotify servers (TCP), cspot library |
| `cspot_player` (bell::Task) | Drains cspot internal audio buffer, calls AudioSink | SpotifyAudioSink, cspot library |
| Spotify SPSC ring buffer | Lock-free bridge between cspot decode (Core 0) and audio_capture_task (Core 1) | SpotifyAudioSink (write), audio_capture_task (read) |
| `audio_capture_task` (existing, extended) | Source selection: Spotify / USB / ADC. Unified buffer → DSP → DAC | Spotify ring buffer, USB ring buffer, I2S ADC, DSP pipeline, DAC |
| `AppState` (existing, extended) | `spotifyActive`, `spotifyPlaying`, `spotifyTrackName`, `spotifyVolume`, dirty flags | All modules |
| ZeroConf HTTP server | Handles Spotify mobile app pairing — `getInfo` + `addUser` endpoints on port 8080 | Spotify mobile app, cspot LoginBlob |
| mDNS (existing ESPmDNS) | Advertises `_spotify-connect._tcp` alongside existing `_http._tcp` service | LAN |

---

## Recommended Project Structure

```
src/
├── spotify_connect.h           # Public API: init, is_playing, get_state, track info
├── spotify_connect.cpp         # cspot integration, ZeroConf, mDNS, AudioSink impl
├── i2s_audio.cpp               # MODIFIED: source selection logic added to audio_capture_task
├── app_state.h                 # MODIFIED: spotify* fields, dirty flags, source enum
├── websocket_handler.cpp       # MODIFIED: spotifyState broadcast, setSpotify* commands
├── mqtt_handler.cpp            # MODIFIED: spotify HA entities (media_player)
├── web_pages.cpp               # MODIFIED: Spotify status card, track info display
└── gui/screens/                # MODIFIED: Spotify status on home/desktop screens
    └── scr_spotify.h/.cpp      # NEW: Spotify control screen (play/pause/skip/volume)

lib/
└── cspot/                      # cspot library (as PlatformIO lib_deps or git submodule)
    ├── cspot/                  # Core cspot source
    └── bell/                   # Bell utilities (Task, Buffer, etc.)

data/
└── (LittleFS)
    └── spotify_creds.bin       # Persistent auth blob (written after successful login)
```

### Structure Rationale

- **`spotify_connect.h/.cpp`:** Single integration point. Keeps cspot dependency isolated — rest of codebase uses clean `spotify_*` API. Mirrors the `usb_audio.h/.cpp` pattern already established.
- **SPSC ring buffer:** Reuses the exact pattern from USB audio (`UsbAudioRingBuffer`). The cspot decode callback runs in cspot's FreeRTOS context (Core 0); audio_capture_task consumes on Core 1. Lock-free SPSC is correct here.
- **Separate ZeroConf HTTP on port 8080:** The existing AsyncWebServer/WebServer occupies port 80. Spotify's ZeroConf pairing requires its own HTTP endpoint. Port 8080 is the convention used by cspot's ESP32 reference.
- **LittleFS for credentials:** Credentials blob must survive reboots. Already used for DSP presets and other configs — `/spotify_creds.bin` fits the pattern.

---

## Architectural Patterns

### Pattern 1: SPSC Ring Buffer as Audio Source Bridge

**What:** A lock-free single-producer / single-consumer ring buffer (mirroring `UsbAudioRingBuffer`) bridges the cspot decode context (Core 0) and `audio_capture_task` (Core 1). The ring buffer lives in PSRAM.

**When to use:** Any time audio data crosses core/task boundaries without a mutex (real-time constraint on consumer side).

**Trade-offs:** No locking overhead; requires power-of-2 capacity; overruns silently drop frames (acceptable — Spotify re-buffers continuously).

**Sizing:** cspot's reference uses a 1MB circular buffer. For embedded integration, a PSRAM allocation of ~176KB (2 seconds at 44.1kHz × 2ch × 2B = 176,400 bytes, rounded to 131,072 or 196,608 frames as power-of-2) is sufficient. Keep it in PSRAM — not internal SRAM.

```cpp
// In spotify_connect.cpp — SpotifyAudioSink::feedPCMFrames()
void SpotifyAudioSink::feedPCMFrames(std::vector<uint8_t> &data) {
    uint32_t frames = data.size() / 4;  // 16-bit stereo = 4 bytes/frame
    // Convert PCM16 stereo to left-justified int32 (matches ADC format)
    // Then write to SPSC ring buffer
    spotify_pcm16_to_int32(
        reinterpret_cast<const int16_t*>(data.data()),
        _convBuf, frames);
    spotify_rb_write(&_rb, _convBuf, frames);
}
```

### Pattern 2: Source Priority Enum in audio_capture_task

**What:** Add a `AudioSourceEnum` to AppState (`AUDIO_SOURCE_ADC`, `AUDIO_SOURCE_USB`, `AUDIO_SOURCE_SPOTIFY`). Each iteration of `audio_capture_task` checks the active source and reads from the corresponding buffer. Priority: Spotify > USB > ADC (most user-intentional source wins).

**When to use:** Whenever a new digital audio input is added.

**Trade-offs:** Simple, explicit, testable. Does not support mixing (mono-source model). If source switching is needed while audio is playing, a short crossfade or mute avoids pops.

```cpp
// Extended audio_capture_task source selection
if (appState.spotifyPlaying && spotify_rb_available(&_spotifyRb) >= stereo_frames) {
    spotify_rb_read(&_spotifyRb, buf, stereo_frames);
    source = AUDIO_SOURCE_SPOTIFY;
} else if (usb_audio_is_streaming()) {
    usb_audio_read(buf, stereo_frames);
    source = AUDIO_SOURCE_USB;
} else {
    i2s_read(I2S_PORT_ADC1, buf, ...);
    source = AUDIO_SOURCE_ADC;
}
```

### Pattern 3: Credentials Persistence via LittleFS

**What:** After a successful Spotify ZeroConf authentication, serialize the credentials blob to `/spotify_creds.bin` on LittleFS. On boot, attempt to load and reconnect automatically without requiring the user to re-pair.

**When to use:** Required — without this, users must re-pair from the Spotify app after every reboot.

**Trade-offs:** Small file (~200 bytes). LittleFS already mounted. Must not save credentials until login is confirmed (cspot's requirement — the encrypted blob is invalid until verified by Spotify servers).

### Pattern 4: SpircHandler Events → AppState Dirty Flags

**What:** cspot's `SpircHandler` delivers events (`PLAY_PAUSE`, `VOLUME`, `TRACK_INFO`, `FLUSH`, `NEXT`, `PREV`, etc.) via a callback. The callback runs in cspot's task context (Core 0). It should only write to AppState fields and set dirty flags — not call WebSocket or MQTT directly (those are not thread-safe from arbitrary task contexts).

**When to use:** Always — matches the existing dirty-flag pattern used by OTA tasks and GUI task.

```cpp
spircHandler->setEventHandler([](std::unique_ptr<SpircHandler::Event> event) {
    auto &as = AppState::getInstance();
    switch (event->eventType) {
        case SpircHandler::EventType::TRACK_INFO:
            as.spotifyTrackName = std::get<TrackInfo>(event->data).name;
            as.spotifyArtistName = std::get<TrackInfo>(event->data).artist;
            as.markSpotifyDirty();
            break;
        case SpircHandler::EventType::PLAY_PAUSE:
            as.spotifyPlaying = std::get<bool>(event->data);
            as.markSpotifyDirty();
            break;
        case SpircHandler::EventType::VOLUME:
            as.spotifyVolume = std::get<int>(event->data);
            as.markSpotifyDirty();
            break;
        // ...
    }
});
```

---

## Data Flow

### Spotify Pairing (one-time or session start)

```
Spotify Mobile App
    ↓ (mDNS discovery: _spotify-connect._tcp)
ZeroConf HTTP GET /spotify_info?action=getInfo
    ↓ returns device name, deviceID, publicKey
Spotify Mobile App
    ↓ HTTP POST /spotify_info?action=addUser (encrypted credentials blob)
spotify_connect.cpp → cspot LoginBlob::populate()
    ↓
cspot Session → Spotify AP servers (apresolve.spotify.com → TCP)
    ↓ Shannon-encrypted session established
SpircHandler initialized
    ↓
/spotify_creds.bin written to LittleFS
AppState::spotifyActive = true, markSpotifyDirty()
```

### Audio Streaming (steady state)

```
Spotify Servers (CDN)
    ↓ (Ogg Vorbis, downloaded by cspot_task)
cspot TrackPlayer (Vorbis decoder, Core 0)
    ↓ decoded 44.1kHz 16-bit stereo PCM chunks (~1024 bytes each)
SpotifyAudioSink::feedPCMFrames()
    ↓ PCM16→int32 conversion + SPSC ring buffer write
Spotify SPSC ring buffer (PSRAM, ~131K frames / ~3 seconds)
    ↓ (Core 0 → Core 1 boundary)
audio_capture_task reads ring buffer (Core 1, priority 3)
    ↓ unified int32 stereo buffer
DSP Pipeline (EQ, compressor, crossover)
    ↓
dac_output_write() → I2S_NUM_0 TX → PCM5102A DAC
    ↓
Analog audio output → amplifier
```

### Control Flow (Spotify → AppState → UI)

```
SpircHandler event (Core 0 task context)
    ↓ write to AppState fields + markSpotifyDirty()
Main loop (Core 0, 1ms cycle)
    ↓ detects spotifyDirty flag
sendSpotifyState() → WebSocket broadcast → Web UI
publishSpotifyMQTT() → MQTT → Home Assistant (media_player entity)
GUI dirty flag → gui_task → TFT screen update
```

### Volume Control (bidirectional)

```
[Spotify App] → SpircHandler VOLUME event → appState.spotifyVolume → DAC software volume
[Web UI setVolume] → appState.spotifyVolume → spircHandler->setRemoteVolume() → Spotify App sync
[MQTT volume] → same path as Web UI
```

---

## FreeRTOS Task Assignment

| Task | Core | Priority | Stack | Notes |
|------|------|----------|-------|-------|
| `loopTask` (main loop) | 0 | 1 | default | HTTP, WS, MQTT, AppState polling |
| `audio_capture_task` | 1 | 3 | 12KB | Extended with source selection |
| `gui_task` | 1 | 1 | 16KB | Unchanged |
| `usb_audio` | 0 | 1 | 4KB | Unchanged |
| `cspot_task` (CSpotTask) | 0 | 1 | 32KB | Spotify session + protocol |
| `cspot_player` (CSpotPlayer) | 0 | 1 | 8KB | Drains cspot buffer → AudioSink |

**Core 0 impact:** Adding `cspot_task` (32KB) and `cspot_player` (8KB) both to Core 0. These compete with `loopTask` (main loop) and `usb_audio`. cspot tasks are priority 1, same as main loop — acceptable since they block on network I/O most of the time. **Watch for main loop starvation** during active Spotify track downloads; test `sendHardwareStats()` timing.

**Core 1 stays clean:** `audio_capture_task` (priority 3) still dominates Core 1. `gui_task` (priority 1) unchanged. No Spotify tasks on Core 1.

**Watchdog:** Register `cspot_task` with `esp_task_wdt_add()` OR ensure it calls `esp_task_wdt_reset()` via its parent bell::Task machinery. Verify — cspot's bell::Task does not guarantee WDT integration.

---

## Memory Budget Considerations

### SRAM (Internal, ~327KB available)

cspot requires TLS/Shannon encryption using mbedTLS — crypto operations use internal SRAM (MALLOC_CAP_INTERNAL). This is the primary concern on ESP32-S3.

| Allocation | Size | Location | Notes |
|------------|------|----------|-------|
| `cspot_task` stack | 32KB | SRAM | FreeRTOS task stacks cannot go in PSRAM |
| `cspot_player` stack | 8KB | SRAM | Same constraint |
| mbedTLS crypto buffers | ~20-40KB | SRAM | TLS handshake + SHA/AES ops |
| Vorbis decoder working memory | ~20-30KB | SRAM or PSRAM | Allocate to PSRAM if cspot supports |
| WiFi RX reserve | 40KB | SRAM | **Existing constraint — must maintain** |

**Critical:** The existing 40KB WiFi RX heap reserve must be maintained. cspot's mbedTLS crypto adds ~20-40KB of SRAM pressure during session establishment. Monitor with `ESP.getMaxAllocHeap()` before and after cspot init.

**Risk:** On devices without PSRAM, cspot crashes immediately (confirmed via issue #52). The ALX Nova N16R8 has 8MB OPI PSRAM — this risk is mitigated, but internal SRAM pressure from crypto is still real.

### PSRAM (8MB OPI, ~PSRAM available)

| Allocation | Size | Location | Notes |
|------------|------|----------|-------|
| Spotify SPSC ring buffer | ~350KB | PSRAM | 2s audio @ 44.1kHz×2ch×2B×2 (headroom) |
| cspot internal audio buffer | ~100-200KB | PSRAM | cspot's own pre-decode buffer |
| PCM16→int32 conversion buffer | ~8KB | PSRAM or SRAM | Per DMA buffer size |

Use `heap_caps_calloc(frames, sizeof(int32_t) * 2, MALLOC_CAP_SPIRAM)` for the Spotify ring buffer. Never use `ps_calloc()` for PSRAM on S3 — use `heap_caps_calloc(MALLOC_CAP_SPIRAM)` directly (documented project gotcha).

### Sample Rate Mismatch

cspot outputs 44.1kHz. The existing I2S ADC runs at whatever `appState.audioSampleRate` is configured (likely 44100 or 48000Hz). The DAC output (PCM5102A via I2S_NUM_0 TX) must be re-initialized to 44.1kHz when Spotify is active if the current rate differs. Use the existing `appState.audioPaused` flag pattern: pause the audio task, reinit I2S, resume.

USB Audio runs at 48kHz — there is already a sample rate mismatch between USB and ADC paths. Spotify at 44.1kHz adds a third potential rate. **Source switching may require I2S rate changes** — this is a significant implementation complexity to plan for.

---

## Integration Points

### External Services

| Service | Integration Pattern | Notes |
|---------|---------------------|-------|
| Spotify AP servers | TCP via cspot SessionManager | cspot handles DNS resolution (apresolve.spotify.com) |
| Spotify CDN | HTTPS via cspot TrackPlayer | Audio track download |
| Spotify mobile app | ZeroConf HTTP (port 8080) + mDNS | Pairing only — not ongoing |
| LittleFS | Credentials blob serialization | `/spotify_creds.bin`, ~200 bytes |

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| cspot → audio pipeline | SPSC ring buffer (PSRAM) | Lock-free, power-of-2 capacity |
| SpircHandler → AppState | Direct write + dirty flag | Must only set flags, not call WS/MQTT |
| AppState → Web/MQTT/GUI | Dirty flag polling in main loop | Existing pattern — unchanged |
| Spotify source ↔ ADC/USB | `AudioSourceEnum` in `audio_capture_task` | Per-iteration, no mutex |
| ZeroConf ↔ cspot | `LoginBlob` struct | cspot-internal; just call `LoginBlob::populate()` |
| Spotify volume ↔ DAC | Software gain in DSP or DAC HAL | Hardware volume not available on PCM5102A |

---

## Anti-Patterns

### Anti-Pattern 1: Calling WebSocket or MQTT from SpircHandler Callback

**What people do:** In SpircHandler event callback, directly call `webSocket.sendTXT()` or `mqttClient.publish()` to immediately push track metadata.

**Why it's wrong:** SpircHandler callback runs in `cspot_task` context (Core 0). `webSocket` and `mqttClient` are not thread-safe — concurrent access from `loopTask` and `cspot_task` causes crashes or corrupted packets.

**Do this instead:** Write to AppState fields and call `markSpotifyDirty()`. Main loop detects the flag and calls WebSocket/MQTT from the correct context.

### Anti-Pattern 2: Allocating the Spotify Ring Buffer in Internal SRAM

**What people do:** Use `calloc()` or `malloc()` for the ring buffer because it's simpler.

**Why it's wrong:** ~350KB ring buffer in SRAM would consume >100% of available free internal SRAM, immediately crashing WiFi (drops below 40KB reserve) and starving mbedTLS crypto.

**Do this instead:** Allocate with `heap_caps_calloc(capacity * 2, sizeof(int32_t), MALLOC_CAP_SPIRAM)`. Verify success and abort init if NULL.

### Anti-Pattern 3: Running cspot Tasks on Core 1

**What people do:** Pin `cspot_task` to Core 1 to "keep network on Core 0."

**Why it's wrong:** Core 1 already runs `audio_capture_task` (priority 3) and `gui_task` (priority 1). cspot does significant CPU work during Vorbis decoding and TLS sessions. Running it on Core 1 risks preempting `audio_capture_task` or causing DMA starvation.

**Do this instead:** Keep both cspot tasks on Core 0 at priority 1. They yield on network I/O, so their impact on the main loop is bounded.

### Anti-Pattern 4: Sharing the ZeroConf Port with the Existing Web Server

**What people do:** Register Spotify's `getInfo`/`addUser` endpoints on the existing AsyncWebServer at port 80.

**Why it's wrong:** The Spotify mobile app mDNS TXT record (`CPath`) must point to a specific path on a specific port. Mixing with the existing web server is possible but couples the Spotify auth flow into the web server's request handling, making it harder to enable/disable at runtime and harder to test. Also the existing web server's auth middleware may intercept Spotify requests.

**Do this instead:** Use a dedicated lightweight HTTP server on port 8080 for ZeroConf only. cspot's reference uses this pattern. Register a separate mDNS SRV record pointing to port 8080.

### Anti-Pattern 5: Sample Rate Mismatch Without I2S Reinit

**What people do:** Start feeding 44.1kHz Spotify PCM into an I2S TX configured for 48kHz (USB audio rate).

**Why it's wrong:** The DAC will play at the wrong pitch and speed. PCM5102A does not auto-detect sample rate from I2S — it uses the BCK/LRCK ratio as-is.

**Do this instead:** On source switch to Spotify, check current I2S TX rate. If it differs from 44100Hz, pause audio task (`appState.audioPaused = true`), call `dac_enable_i2s_tx(44100)`, then clear the pause flag. On switch away from Spotify, reinit to the appropriate rate.

---

## Build Order Implications

The following phase sequence is implied by component dependencies:

1. **Spotify SPSC Ring Buffer + Format Conversion** — No dependencies. Fully testable in `native` env. Mirrors `usb_audio` ring buffer implementation. Establish `spotify_rb_*` API and `spotify_pcm16_to_int32()`. Add native tests.

2. **AppState Extension + Source Enum** — Depends on ring buffer API. Add `AudioSourceEnum`, `spotifyActive`, `spotifyPlaying`, `spotifyTrackName`, `spotifyArtistName`, `spotifyVolume`, `spotifyDirty` flag. Extend native tests.

3. **audio_capture_task Source Selection** — Depends on AppState fields and ring buffer. Modify existing task to poll Spotify ring buffer first. Can be developed without cspot linked — just needs the ring buffer readable.

4. **cspot Library Integration** — Independent of phases 1-3 from a build perspective, but depends on WiFi being stable. Add cspot as a PlatformIO `lib_deps` entry (git submodule recommended for version pinning). Verify it compiles with Arduino framework on ESP32-S3. **This step has the highest risk** — cspot uses CMake, while the project uses PlatformIO Arduino framework. A PlatformIO-compatible wrapper or the `framework = arduino, espidf` dual-framework approach may be required.

5. **SpotifyAudioSink + spotify_connect Module** — Depends on cspot building (phase 4) and ring buffer (phase 1). Implement `feedPCMFrames()`, ZeroConf HTTP, mDNS advertisement, credential persistence.

6. **AppState Bridge + Main Loop Integration** — Depends on phases 2, 3, 5. Wire SpircHandler events to dirty flags. Add `sendSpotifyState()` to WebSocket handler. Add MQTT entities.

7. **Web UI + GUI** — Depends on WebSocket messages from phase 6. Spotify status card, track display, playback controls.

---

## Scaling Considerations

This is a single-device embedded system. "Scaling" means operational robustness over time, not multi-user load.

| Concern | Approach |
|---------|----------|
| Extended playback (hours) | Ring buffer sized for 2-3s audio headroom prevents underruns during brief network hiccups |
| WiFi reconnect during playback | cspot handles reconnect internally; ring buffer provides ~3s of audio for gap filling |
| Memory fragmentation over time | Allocate ring buffer at boot (not on Spotify connect) to prevent PSRAM fragmentation |
| I2S rate switching stability | Use `appState.audioPaused` + 50ms settle time (same as DAC reinit pattern) |
| Spotify session expiry | cspot handles token refresh; cached credentials survive reboot |
| Multiple control interfaces (app + web + MQTT) | AppState is single source of truth; all interfaces read/write only AppState |

---

## Sources

- [cspot main.cpp ESP32 target — task architecture, buffer sizes, AudioSink usage](https://github.com/feelfreelinux/cspot/blob/master/targets/esp32/main/main.cpp)
- [cspot README — AudioSink interface, feedPCMFrames() spec, credentials caching requirement](https://github.com/feelfreelinux/cspot/blob/master/README.md)
- [cspot SpircHandler.h — EventType enum, event callbacks, setPause/nextSong/setRemoteVolume API](https://github.com/feelfreelinux/cspot/blob/master/cspot/include/SpircHandler.h)
- [cspot issue #52 — PSRAM dependency confirmed, heap exhaustion root cause](https://github.com/feelfreelinux/cspot/issues/52)
- [squeezelite-esp32 issue #419 — ESP32-S3 cspot SHA/AES allocation failures, crypto SRAM pressure](https://github.com/sle118/squeezelite-esp32/issues/419)
- [Spotify ZeroConf API — getInfo/addUser endpoints, mDNS SRV/TXT record format](https://developer.spotify.com/documentation/commercial-hardware/implementation/guides/zeroconf)
- [ESP-IDF External RAM guide — MALLOC_CAP_SPIRAM allocation, DMA constraint](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/external-ram.html)

---

*Architecture research for: Spotify Connect on ESP32-S3 ALX Nova Controller*
*Researched: 2026-02-17*
