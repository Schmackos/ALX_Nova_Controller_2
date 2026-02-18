# Project Research Summary

**Project:** ALX Nova Controller — Spotify Connect Integration
**Domain:** Spotify Connect streaming receiver on ESP32-S3 embedded audio controller
**Researched:** 2026-02-17
**Confidence:** MEDIUM

## Executive Summary

Adding Spotify Connect to the ALX Nova requires integrating cspot — the only open-source Spotify Connect library that targets ESP32. cspot handles the Spotify binary protocol (Shannon/SPIRC), ZeroConf pairing, and Ogg Vorbis decode internally. It outputs decoded 44.1kHz 16-bit stereo PCM via a `feedPCMFrames()` callback, which must be bridged to the existing DAC pipeline through a PSRAM-backed SPSC ring buffer. The build integration path is the dual PlatformIO framework (`framework = arduino, espidf`) because cspot's CMake build system requires ESP-IDF component infrastructure — pure Arduino cannot compile it. This is a non-trivial but well-documented integration pattern.

The recommended approach is to treat cspot as an isolated module (`spotify_connect.h/.cpp`) mirroring the existing `usb_audio.h/.cpp` pattern: a PSRAM ring buffer bridges cspot's decode context (Core 0) to `audio_capture_task` (Core 1), SpircHandler events write only to AppState dirty flags, and the main loop handles all WebSocket/MQTT broadcasts. A `SPOTIFY` source is added to an `AudioSourceEnum` so `audio_capture_task` already has a source priority switch. The existing DSP pipeline, DAC HAL, volume controls, MQTT/HA integration, and TFT GUI all extend naturally to Spotify as a new input source.

The integration carries three pre-emptive blockers that must be resolved before writing application code. First, the flash partition must be audited — firmware is at 77.6% and cspot adds ~200KB of code; a custom partition table using the N16's 16MB flash is likely required to preserve OTA capability. Second, internal SRAM headroom must be measured and extended to at least 80KB contiguous before cspot initialization; the existing 40KB WiFi reserve is insufficient because cspot's mbedTLS crypto needs an additional 30-40KB of contiguous internal SRAM. Third, mDNS ownership must be consolidated — adding Spotify service registration to an already-initialized mDNS instance, not double-initializing. Ignoring these three issues causes silent, hard-to-diagnose failures (device reboots on first Spotify session, device not visible on network) that waste significant debugging time.

---

## Key Findings

### Recommended Stack

cspot is the sole viable path for making the ESP32 itself appear as a Spotify Connect speaker. All alternatives (librespot, Web API wrappers, commercial eSDK) are either impossible on bare-metal ESP32, non-streaming, or inaccessible without a corporate partnership. cspot must be added as a git submodule pulling `bell` transitively; its CMake build system requires `framework = arduino, espidf` in platformio.ini and a root `CMakeLists.txt` declaring cspot as an `extra_component_dirs` component.

The FEATURES.md research surfaced a critical architectural fork: FEATURES.md recommends a companion Raspberry Pi running librespot, while STACK.md recommends cspot running directly on the ESP32. These are incompatible recommendations from the same research pass. **Resolution: prefer the cspot-on-ESP32 path** (STACK.md) because it keeps the system self-contained and the ALX Nova's 8MB PSRAM and existing infrastructure make cspot viable. The companion Pi path adds hardware complexity and cost for a feature the hardware can handle natively.

**Core technologies:**
- cspot (master, git submodule): Spotify Connect protocol + Ogg Vorbis decode — only open-source implementation targeting ESP32; outputs PCM via `feedPCMFrames()`
- bell (cspot's submodule, develop branch): Bundled audio utilities, tremor (Vorbis), civetweb (ZeroConf HTTP), nanopb (protobuf), mdnssvc — pulled automatically via `--recursive`
- PlatformIO dual framework (`arduino, espidf`): Enables cspot's CMake build alongside all existing Arduino code — no Arduino-only alternative exists
- witnessmenow/spotify-api-arduino (optional): Spotify Web API for metadata enrichment and write-back volume control — not needed if cspot's SpircHandler callbacks are sufficient

**Critical version notes:**
- cspot + IDF 5.1 compatibility is MEDIUM confidence (issue #176 Dec 2024 unresolved); may require patching
- Existing mbedTLS PSRAM linker wraps (`-Wl,--wrap=esp_mbedtls_mem_calloc`) are CRITICAL — keep them; cspot crypto crashes without PSRAM redirect on ESP32-S3
- cspot's bundled civetweb runs on port 8080 — no conflict with existing port 80 (HTTP) and 81 (WebSocket)

### Expected Features

cspot's SpircHandler delivers `TRACK_INFO`, `PLAY_PAUSE`, and `VOLUME` events that map directly to AppState fields. The minimal integration already satisfies all P1 table-stakes features. Differentiators (HA media_player entity, auto-switch, amp relay integration) build on top using existing infrastructure.

**Must have for launch (P1):**
- Device appears in Spotify app as a Connect speaker (mDNS `_spotify-connect._tcp` + ZeroConf HTTP on port 8080)
- Track name + artist name on TFT with Spotify attribution (required per official Spotify display spec — round icon min 42x42px)
- Play/Pause/Skip controls (SpircHandler or Spotify Web API)
- Volume knob (rotary encoder) writes volume to Spotify via Web API — the critical physical UX moment
- MQTT `media/title`, `media/artist`, `media/playing` topics for HA
- `SPOTIFY` as a named input source in AppState, Web UI, and GUI

**Should have after validation (P2):**
- Auto-switch to Spotify input on playback start (requires conflict handling toggle for active ADC sessions)
- Full HA `media_player` entity with play/pause/skip via MQTT commands
- Spotify active state integrated with amp relay smart sensing (treat Spotify as "signal present")
- Bitrate selection (96/160/320 kbps) in Web UI + MQTT

**Defer to v2+:**
- Spotify lossless (FLAC) — upstream librespot/cspot dependency, timeline unknown
- Album art on TFT — impractical at 160x128; deliver in Web UI only (trivial via Spotify CDN URLs)
- Session transfer mid-song — librespot supports it but needs UX validation

**Anti-features to avoid:** Multi-room Spotify grouping, playlist browsing on TFT, free-tier support (librespot/cspot is Premium-only by design).

### Architecture Approach

The integration follows the established `usb_audio.h/.cpp` pattern: a new `spotify_connect.h/.cpp` module isolates all cspot dependency; a PSRAM SPSC ring buffer (~350KB, sized for 2-3 seconds headroom) bridges cspot's decode task (Core 0) to `audio_capture_task` (Core 1); SpircHandler events write only to AppState dirty flags; the main loop handles all WebSocket/MQTT broadcasts. The existing `audio_capture_task` gains a source priority switch (`AUDIO_SOURCE_SPOTIFY > AUDIO_SOURCE_USB > AUDIO_SOURCE_ADC`) checked per iteration.

**Major components:**
1. `spotify_connect.h/.cpp` — cspot init, ZeroConf HTTP (port 8080), mDNS service registration, credential persistence (`/spotify_creds.bin` in LittleFS), SpotifyAudioSink (PCM16→int32 conversion + ring buffer write), SpircHandler→AppState dirty flag bridge
2. Spotify SPSC ring buffer (PSRAM, ~350KB) — lock-free Core 0→Core 1 PCM data path, allocated at boot (not on connect) to prevent fragmentation
3. `audio_capture_task` source selection — per-iteration priority switch; Spotify source feeds unified int32 stereo buffer → existing DSP pipeline → DAC HAL; sample rate reinit via `audioPaused` flag if switching from 48kHz USB
4. AppState extensions — `spotifyActive`, `spotifyPlaying`, `spotifyTrackName`, `spotifyArtistName`, `spotifyVolume`, `AudioSourceEnum`, `spotifyDirty` flag
5. `scr_spotify.h/.cpp` — new LVGL screen for Spotify control (play/pause/skip/volume) + track display on Home screen

**Data flow (steady state):** Spotify CDN → cspot Vorbis decoder (Core 0) → SpotifyAudioSink → PSRAM ring buffer → `audio_capture_task` (Core 1) → DSP pipeline → DAC HAL → PCM5102A → analog output.

### Critical Pitfalls

1. **Cryptographic heap exhaustion on first session** — cspot's Shannon+TLS crypto requires ~30-40KB of contiguous internal SRAM on top of the existing 40KB WiFi reserve. Must audit `heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)` and target >80KB free before cspot init; move LVGL/DSP buffers to PSRAM if needed. This is a crash-on-first-use failure with no graceful degradation.

2. **Flash partition overflow breaks OTA** — At 77.6% flash usage, cspot's ~200KB code addition will overflow the OTA partition. The 16MB flash on the N16 variant allows a custom partition table with 4MB per OTA partition. This must be addressed before integration begins — it cannot be fixed via OTA after the fact.

3. **mDNS double-init makes device invisible** — ESP-IDF mDNS is a singleton; calling `mdns_init()` twice produces undefined behavior. The Spotify `_spotify-connect._tcp` service must be added to the existing mDNS instance (likely in `wifi_manager.cpp`), not initialized separately inside the cspot module. Verify with `dns-sd -B _spotify-connect._tcp` and confirm it survives WiFi reconnect.

4. **I2S teardown race condition during OTA + Spotify concurrent** — The existing `audioPaused` flag covers ADC capture but not cspot's DAC write path. cspot output must route through the existing DAC HAL (never bypass with direct `i2s_write()`) so the pause protocol applies. Test: trigger OTA check from web UI during active Spotify playback — must not crash.

5. **cspot task stack overflow on track switch** — The default 8192-byte stack in cspot's Task.h is insufficient for Vorbis decode call depth during track switching (confirmed in cspot issue #68). Allocate `cspot_task` at 32KB and `cspot_player` at minimum 16KB from the start. Monitor with `uxTaskGetStackHighWaterMark()` after 20 rapid track switches.

6. **Access point token expiry (silent session death at ~60 minutes)** — cspot sessions can silently die when Spotify rotates access points or tokens expire. A software watchdog is required: if no audio data for >90 seconds while session is marked active, trigger a full cspot restart. The 15s hardware WDT will not catch this because cspot is doing network I/O.

---

## Implications for Roadmap

Based on research, the following phase structure is recommended. Phases 0 and 1 are pre-integration infrastructure that must complete before any cspot code is written.

### Phase 0: Pre-Integration Gating
**Rationale:** Three blockers (flash, heap, mDNS) will cause silent failures if not resolved first. Establishing measurable go/no-go gates prevents wasted integration effort.
**Delivers:** Confirmed flash budget headroom, confirmed internal SRAM headroom, consolidated mDNS ownership, custom partition table if needed
**Addresses:** Flash overflow pitfall (P7), cryptographic heap exhaustion pitfall (P1), mDNS double-init pitfall (P4)
**Actions:** Run `xtensa-esp32s3-elf-size` on a cspot-only test build; measure `heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)`; audit all `mdns_init()` / `MDNS.begin()` calls; create custom partition table using 16MB flash if projected firmware > 85% of current app partition

### Phase 1: SPSC Ring Buffer + AppState Extension
**Rationale:** The ring buffer and AppState fields are fully testable in the `native` environment without cspot linked. Establishing these first lets native tests validate the data path before any real ESP32 build complexity is introduced. Mirrors the proven `usb_audio` pattern.
**Delivers:** `spotify_rb_*` API (write/read/available), `spotify_pcm16_to_int32()` conversion, AppState `spotifyActive/Playing/TrackName/ArtistName/Volume/Dirty` fields, `AudioSourceEnum` with SPOTIFY variant
**Uses:** PSRAM `heap_caps_calloc(MALLOC_CAP_SPIRAM)`, existing SPSC ring buffer pattern from `usb_audio`
**Implements:** Spotify SPSC ring buffer component
**Test target:** `pio test -e native -f test_spotify_core` — ring buffer, PCM conversion, AppState fields

### Phase 2: cspot Library Integration + Build System
**Rationale:** The dual-framework build change (`framework = arduino, espidf`) is the highest-risk single step. Isolating it here means if cspot doesn't compile with the current IDF version, it's caught before any application code depends on it.
**Delivers:** cspot compiling as an ESP-IDF component inside PlatformIO dual-framework build; `spotify_connect.h/.cpp` skeleton with `spotify_connect_init()` / `spotify_connect_task_start()` stubs; ZeroConf HTTP server on port 8080; mDNS `_spotify-connect._tcp` service registration added to existing mDNS instance
**Avoids:** mDNS double-init, cspot task stack overflow (size generously: cspot_task 32KB, cspot_player 16KB)
**Research flag:** NEEDS RESEARCH — cspot IDF 5.1 compatibility is MEDIUM confidence; may require source patching. Verify build before proceeding.

### Phase 3: Audio Path Integration
**Rationale:** Audio output is the feature; everything else is plumbing. Once the ring buffer (Phase 1) and cspot build (Phase 2) are in place, wiring `feedPCMFrames()` to the ring buffer and extending `audio_capture_task` with source selection completes the critical path.
**Delivers:** Working Spotify audio output through existing DSP pipeline and PCM5102A DAC; `audio_capture_task` source priority switch (Spotify > USB > ADC); sample rate reinit via `audioPaused` flag when switching from 48kHz USB source; cspot tasks registered with FreeRTOS WDT
**Addresses:** Audio capture task starvation pitfall (P6) — pin all cspot tasks to Core 0; I2S teardown race (P3) — route through DAC HAL only
**Implements:** SpotifyAudioSink (feedPCMFrames → PCM16→int32 → ring buffer), audio_capture_task source selection

### Phase 4: Credential Persistence + Session Management
**Rationale:** Without credential persistence, users re-pair after every reboot. Without the session health watchdog, the device silently dies after ~60 minutes. Both are required for the feature to be usable in practice, not just in a demo.
**Delivers:** `/spotify_creds.bin` written to LittleFS after successful auth; auto-reconnect on boot; 90-second silence watchdog that triggers full cspot restart; `"Spotify disconnected"` state visible in AppState and GUI
**Addresses:** Access point token expiry pitfall (P5)

### Phase 5: AppState Bridge + Control Plane Integration
**Rationale:** With audio working, the control plane (WebSocket, MQTT, REST) can be wired in using the established dirty-flag pattern. SpircHandler callbacks populate AppState; the main loop broadcasts state to all interfaces.
**Delivers:** `sendSpotifyState()` WebSocket broadcast; MQTT `media/title`, `media/artist`, `media/playing` topics; HA MQTT Discovery for media_player entity; `SPOTIFY` as named input source visible in Web UI and MQTT
**Uses:** Existing WebSocket pattern (`sendBIN`/sendTXT), existing MQTT discovery pattern
**Implements:** SpircHandler→AppState bridge, dirty-flag polling in main loop

### Phase 6: GUI + Web UI
**Rationale:** UI surfaces the feature to the end user. TFT and Web UI are independent deliverables that can be built in parallel once WebSocket messages exist.
**Delivers:** New `scr_spotify.h/.cpp` LVGL screen (play/pause/skip/volume, scrolling track name + artist, Spotify attribution icon); Home screen shows "Spotify Active" badge; Web UI "Spotify" card on Audio tab (now-playing, controls, bitrate selection); Spotify display requirements satisfied (42x42px icon OR "Spotify" text, scrolling metadata)
**Uses:** Existing LVGL screen pattern (`scr_*.h/.cpp`), existing WebSocket `spotifyState` message

### Phase 7: P2 Features (Auto-switch, Amp Relay, Volume Sync)
**Rationale:** These features extend the P1 integration. Deferred until the P1 path is stable and tested.
**Delivers:** Auto-switch to Spotify source on playback start (with conflict handling toggle for active ADC sessions); Spotify active state integrated with smart sensing FSM (amp relay stays energized during Spotify playback); bidirectional volume sync (encoder → Spotify Web API write-back via `witnessmenow/spotify-api-arduino`); bitrate selection exposed in Web UI and MQTT
**Addresses:** Auto-switch conflict with ADC recording sessions (explicit conflict handling toggle required)

---

### Phase Ordering Rationale

- Phase 0 must precede everything because flash overflow is unrecoverable via OTA and heap exhaustion causes silent boot crashes — both waste all subsequent effort if unaddressed.
- Phases 1 and 2 are decoupled from each other (ring buffer is native-testable, cspot build is ESP32-only) but both must precede Phase 3.
- Phase 3 (audio) precedes Phase 5 (control plane) because there is no point wiring WebSocket broadcasts for a track name if the audio path does not work.
- Phase 4 (session management) follows Phase 3 because it requires a working session to validate credential persistence and the watchdog.
- Phases 6 and 7 are delivery polish on top of a working core; they can proceed once Phase 5 is stable.

### Research Flags

Phases needing `/gsd:research-phase` during planning:
- **Phase 2:** cspot + IDF 5.1 compatibility is MEDIUM confidence. Check cspot master commit history for IDF 5.x fixes before starting. Issue #176 (Dec 2024) is unresolved. Plan for possible source-level patching or pinning to a specific commit.
- **Phase 2:** PlatformIO dual-framework (`arduino, espidf`) interaction with existing `build_unflags`, `build_flags`, and pre-built library paths (`libespressif__esp-dsp.a`) needs verification. The CMakeLists.txt root file approach may conflict with existing platformio.ini section overrides.
- **Phase 3:** Sample rate switching (44.1kHz Spotify vs 48kHz USB) while audio is running needs testing with the `audioPaused` flag. The reinit sequence is known to work for DAC toggle, but adding a third sample rate path has not been validated.

Phases with standard patterns (skip research-phase):
- **Phase 1:** Ring buffer + AppState extension follows established `usb_audio` pattern exactly. Well-documented, native-testable.
- **Phase 5:** WebSocket/MQTT integration follows established dirty-flag pattern used by OTA, GUI, and USB audio. No novel patterns.
- **Phase 6:** LVGL screen follows established `scr_*.h/.cpp` pattern. Spotify display requirements are officially documented.

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | MEDIUM | cspot is the only path — HIGH confidence. Dual-framework build integration is MEDIUM (sparsely documented for existing-project injection). IDF 5.1 compat is LOW from single unresolved issue. |
| Features | MEDIUM-HIGH | Official Spotify display and ZeroConf specs are HIGH confidence. cspot-specific feature delivery (SpircHandler events, volume callback) is MEDIUM (verified via source inspection, not live testing). FEATURES.md's Pi companion path is ruled out — decision made. |
| Architecture | MEDIUM | SPSC ring buffer pattern and dirty-flag bridge are HIGH confidence (proven in project via usb_audio). cspot task stack/core assignment is MEDIUM (squeezelite-esp32 issues referenced but not directly replicated). Sample rate reinit path is MEDIUM. |
| Pitfalls | MEDIUM | Cryptographic heap exhaustion and task stack overflow are HIGH confidence (confirmed in cspot issue tracker). Flash overflow is HIGH (calculable). mDNS double-init is HIGH (singleton behavior is documented). Session death at 60min is MEDIUM (librespot issue referenced, cspot behavior may differ). |

**Overall confidence:** MEDIUM

### Gaps to Address

- **IDF 5.1 / cspot compatibility:** Resolve by building a minimal cspot smoke test (`cspot_init()` + WiFi connect) before committing to Phase 2. If it fails, check cspot commit log for IDF 5.x fixes and pin to a working commit.
- **FEATURES.md vs STACK.md architectural conflict (Pi companion vs cspot on ESP32):** Resolved in this summary in favor of cspot-on-ESP32. Confirm this decision is correct before roadmap creation — if the user intends a Pi companion architecture, Phase 0-7 above need significant rework.
- **cspot code size:** The ~200KB estimate for cspot+bell is LOW confidence (rough estimate from STACK.md). Measure actual compiled size in Phase 0 gate before assuming the 16MB flash partition table is sufficient.
- **witnessmenow/spotify-api-arduino for volume write-back:** Marked as "work in progress" in its README. Volume write-back may require alternative: direct HTTP to Spotify Web API or polling cspot's local state. Validate in Phase 7.
- **Long-session stability:** The 4-hour continuous playback requirement from the pitfall checklist has no documented pass/fail case for cspot + ESP32-S3 specifically. Plan explicit soak testing in Phase 4.

---

## Sources

### Primary (HIGH confidence)
- https://github.com/feelfreelinux/cspot — cspot repository, README, ESP32 target, CMakeLists.txt, SpircHandler.h
- https://developer.spotify.com/documentation/commercial-hardware/implementation/guides/zeroconf — Official ZeroConf API spec (getInfo/addUser endpoints, mDNS TXT record)
- https://developer.spotify.com/documentation/commercial-hardware/implementation/requirements/speaker-display — Official display requirements (attribution, metadata, 42x42px minimum icon)
- https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/mem_alloc.html — MALLOC_CAP_SPIRAM allocation, PSRAM constraints

### Secondary (MEDIUM confidence)
- https://github.com/sle118/squeezelite-esp32/issues/419 — ESP32-S3 cspot SHA/AES allocation failure, crypto SRAM pressure, PSRAM wrap fix
- https://github.com/feelfreelinux/cspot/issues/52 — PSRAM dependency confirmed; crash without PSRAM
- https://github.com/feelfreelinux/cspot/issues/68 — Task stack overflow on track switch; 8192 bytes insufficient
- https://github.com/witnessmenow/spotify-api-arduino — Web API library for metadata and playback control
- https://github.com/devgianlu/go-librespot — go-librespot reference for protocol behavior

### Tertiary (LOW confidence)
- https://github.com/feelfreelinux/cspot/issues/176 — IDF version compatibility uncertainty (Dec 2024, unresolved)
- https://github.com/librespot-org/librespot/issues/1377 — Access point token expiry / session death at ~60min (librespot; cspot behavior may differ)
- squeezelite-esp32 issue #366 — Long session degradation (unresolved root cause, single source)

---
*Research completed: 2026-02-17*
*Ready for roadmap: yes*
