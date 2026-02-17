# Feature Research

**Domain:** Spotify Connect streaming receiver on embedded ESP32-S3 amplifier controller
**Researched:** 2026-02-17
**Confidence:** MEDIUM-HIGH (official Spotify dev docs verified; ESP32-specific implementation details LOW confidence)

---

## Context: Two Paths to "Spotify Connect"

Before categorizing features, a critical architectural reality shapes everything:

**Path A: Official Spotify eSDK** — Requires corporate partnership. Spotify only accepts organizations (not individuals). Requires signing NDAs, distribution agreements, Certomato certification, and shipping two test units. Provides binary SDK with OGG Vorbis decode, full ZeroConf discovery, and license-compliant playback restrictions. **Not accessible to this project without a company entity.**

**Path B: Spotify Web API + librespot approach** — The practical DIY path. Two sub-options:
- **Web API as remote control only** (fully compliant, officially supported for individuals): The ESP32 acts as a controller — calls Spotify API to play/pause/skip/volume on *another* device, or on the user's active session. Does not make the ESP32 a streaming receiver.
- **librespot/raspotify on companion hardware** (e.g., Raspberry Pi Zero 2W connected to the ESP32 DAC): librespot makes the Pi a Spotify Connect receiver; ESP32 handles display, controls, and routing. Personal/private use only — Spotify's ToS prohibits commercial use but tolerated for personal hobby projects.

**Recommendation for ALX Nova:** Use the Web API + librespot companion approach. The ESP32-S3 cannot itself run librespot (requires Rust + substantial RAM/ROM). A companion Pi Zero 2W ($15) running raspotify outputs I2S or USB audio to the ESP32's existing DAC pipeline. The ESP32 handles everything visible to the user: display, controls, MQTT, HA integration, input selection, and volume.

This document categorizes features from the **user's perspective** (what they experience) and from the **implementation complexity** perspective for the ALX Nova architecture.

---

## Feature Landscape

### Table Stakes (Users Expect These)

Features users assume exist in a Spotify Connect receiver. Missing these = product feels broken.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Device appears in Spotify app | Core Spotify Connect UX — users pick device from Connect menu | MEDIUM | mDNS/ZeroConf advertisement via `_spotify-connect._tcp`. librespot handles this on companion Pi. |
| Play / Pause | Universal media control — missing this is broken | LOW | librespot handles; ESP32 triggers via Web API `PUT /me/player/pause` |
| Skip next / Previous track | Expected from any music player | LOW | Web API: `POST /me/player/next`, `POST /me/player/previous` |
| Volume control | Physical knob must sync with Spotify app volume display | MEDIUM | Bidirectional: hardware knob -> Web API `PUT /me/player/volume`; Spotify app change -> reflected on device. SpPlaybackUpdateVolume equivalent in librespot API. |
| Track name + Artist name on display | Users cast to device precisely to see what's playing | MEDIUM | Web API `GET /me/player/currently-playing` or librespot event callbacks. Must show on 160x128 TFT. |
| Playback continues when phone leaves WiFi | Core Spotify Connect promise — device streams direct from Spotify servers | HIGH | This only works if the receiver (librespot/companion Pi) does the actual streaming. Web API remote-control-only approach loses this. Critical design decision. |
| Input source identification | User must know Spotify is the active source | LOW | TFT screen + MQTT `input_source` + Web UI badge showing "Spotify Connect" |
| Spotify attribution on display | Required by Spotify display guidelines — Spotify logo or "Spotify" text must show | LOW | The round Spotify icon (min 42x42px) or wordmark "Spotify" on TFT. Required per official spec. |
| Premium account requirement communicated | librespot only works with Premium — users on free tier must be told why it fails | LOW | Error state on display + MQTT diagnostic |

### Differentiators (Competitive Advantage)

Features that set ALX Nova apart from generic Spotify Connect speakers. Not expected, but valued.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Home Assistant integration for Spotify state | HA shows currently playing track, controls playback from HA dashboard — no other consumer Spotify Connect device does this | MEDIUM | MQTT `media/title`, `media/artist`, `media/album`, `media/playing` topics → HA media player entity via MQTT Discovery |
| Automatic input source switching | Playing Spotify? Switch from ADC/USB to Spotify automatically. Stop Spotify? Switch back to previous source | MEDIUM | librespot idle event → AppState `inputSource = SPOTIFY`, resumes previous source on playback stop. Conflict handling toggle needed. |
| Spotify volume follows hardware knob in both directions | Most Spotify Connect speakers either accept app volume OR physical control — ALX Nova bidirectional sync is genuinely better UX | MEDIUM | Poll or webhook from librespot API + debounced encoder writes back to Spotify API |
| Audio quality setting exposed in Web UI + MQTT | Most speakers hide bitrate; ALX Nova exposes 96/160/320 kbps selection | LOW | librespot `bitrate` config param. Expose via Web UI dropdown + MQTT `settings/spotify_bitrate`. Restart librespot on change. |
| Track info on physical TFT display | Scrolling track name + artist at 160x128 with Spotify branding — premium feel for a DIY device | MEDIUM | Scrolling LVGL label widget. Album art is impractical at 160x128 (too small, would require HTTP download). |
| Spotify source in amplifier power management | When Spotify is active, keep amp relay energized. When idle for N minutes, allow auto-off | MEDIUM | Integrate Spotify active/idle state into smart sensing state machine. Treat Spotify playback as "signal present". |
| Sleep timer visible in Web UI | Spotify's own sleep timer is app-side only; exposing it via Web UI and HA is convenient | LOW | Set via Web API `PUT /me/player` — but this is Spotify Premium feature only |
| Configurable auto-switch behavior | Toggle: auto-switch to Spotify on playback start (on/off) + conflict behavior (pause ADC vs reject Spotify) | LOW | AppState boolean flags, Web UI toggles, MQTT switches |

### Anti-Features (Commonly Requested, Often Problematic)

| Anti-Feature | Why Requested | Why Problematic | Alternative |
|--------------|---------------|-----------------|-------------|
| Album art download and display on TFT | "Show the album cover!" — looks great in demos | 160x128 at 4-bit color depth makes album art ugly. HTTP download of JPEG on ESP32 requires ~50-100KB heap for decode (not reliable). Adds significant complexity for poor visual result. | Show track title + artist in large readable font instead. Reserve album art for Web UI only (browser displays it natively from Spotify CDN URL). |
| Multi-room Spotify grouping (Spotify Group Sessions) | "Play the same song in every room" | Requires Group Sessions API (Premium only, experimental) and multiple Spotify Connect devices. ALX Nova is a single device. Out of scope. | Document that the ESP32 is a single-zone device. |
| Playlist/album browsing on TFT | "Browse my playlists on the device" | 160x128 TFT with rotary encoder is a terrible music browser. Spotify's app is the right UI for browsing. Building a browser means reimplementing the Spotify app on a tiny screen. | Web UI `Audio > Spotify` tab shows current queue. Browsing stays in the Spotify app. |
| Free-tier Spotify support | "Why do I need Premium?" | librespot explicitly and technically only works with Premium. No way around Spotify's DRM. Building a workaround violates ToS. | Document clearly: Spotify Connect requires Spotify Premium. Display helpful error message when free account detected. |
| Offline / downloaded playlist playback | "Play saved songs without internet" | librespot streams from Spotify servers — no local cache. Spotify's offline downloads use DRM tied to their app. | Not feasible. Out of scope. |
| Spotify podcast support | "Play my podcasts on this speaker" | librespot supports podcasts in principle, but metadata + chapter navigation are complex. Display is too small for podcast UI. | Spotify app controls podcast playback fine using the device as an output. Track display shows episode title — good enough. |
| Spotify lossless (FLAC 24-bit) on the companion Pi | "Play in hi-fi quality" | As of Sep 2025, Spotify lossless rolled out. However, librespot lossless support depends on the open-source implementation catching up to Spotify's protocol changes. Status as of 2026 is unclear (LOW confidence). | Stick to 320 kbps OGG Vorbis which librespot reliably supports. Document lossless as "future" if librespot adds support. |

---

## Feature Dependencies

```
[Companion Pi + librespot]
    └──enables──> [Spotify Connect discovery (mDNS)]
    └──enables──> [True streaming (phone-independent playback)]
    └──enables──> [Play/Pause/Skip (via librespot)]
    └──enables──> [Volume sync bidirectional]
    └──enables──> [Track metadata events]

[Spotify Web API OAuth token]
    └──required for──> [Web API playback control]
    └──required for──> [Volume write-back to Spotify]
    └──required for──> [Currently playing poll fallback]

[Track metadata events]
    └──enables──> [TFT display: track name + artist]
    └──enables──> [MQTT: media/title, media/artist, media/playing]
    └──enables──> [HA media player entity]
    └──enables──> [Web UI: Spotify Now Playing card]

[Input source selection system (existing)]
    └──extends to──> [Spotify source]
    └──enables──> [Auto-switch to Spotify on playback]
    └──enables──> [Amp relay power management with Spotify]

[Volume sync bidirectional]
    └──requires──> [Track metadata events (librespot API)]
    └──requires──> [Spotify Web API OAuth token]

[Auto-switch to Spotify]
    └──requires──> [librespot idle/active event callbacks]
    └──requires──> [Existing input source AppState fields]
    └──conflicts with──> [ADC1/ADC2 active recording session]

[HA media player entity]
    └──requires──> [Track metadata events]
    └──requires──> [MQTT Discovery (existing)]
    └──enhances──> [Spotify source in amp power management]
```

### Dependency Notes

- **Companion Pi is the critical dependency**: Without a Linux capable device running librespot, the ESP32 cannot be a true Spotify Connect receiver. The ESP32-S3 has insufficient RAM (~8MB PSRAM but no Linux) for the Rust-based librespot runtime.
- **Web API token is independent**: OAuth PKCE flow for the Spotify Web API works from the ESP32's web server (redirect URI back to device IP). Required for volume write-back even when librespot handles streaming.
- **Auto-switch conflicts with active ADC**: If a user is recording/monitoring via ADC1 and Spotify starts, auto-switch must not interrupt. The conflict handling toggle is therefore a real UX requirement, not nice-to-have.

---

## MVP Definition

### Launch With (v1)

Minimum viable product — what's needed to validate that Spotify works as a source in the ALX Nova ecosystem.

- [ ] Companion Pi running raspotify appears as Spotify Connect device on local network — proves the streaming path works
- [ ] ESP32 detects Spotify playback state via librespot local API (HTTP events or polling) — the signal that bridges the two devices
- [ ] Track name + artist name displayed on TFT with Spotify icon — mandatory per Spotify display guidelines, also the headline user-visible feature
- [ ] MQTT publishes `media/title`, `media/artist`, `media/playing` (boolean) — enables HA media player card on day one
- [ ] Input source `SPOTIFY` added to AppState — so Web UI, MQTT, and GUI show "Spotify" as the active source
- [ ] Volume knob (encoder) writes volume to Spotify via Web API — most critical UX moment: physical control feels connected to Spotify

### Add After Validation (v1.x)

- [ ] Auto-switch to Spotify input on playback start — add after confirming volume + display work reliably
- [ ] HA `media_player` entity with full controls (play/pause/skip via MQTT → Web API) — add once state tracking is stable
- [ ] Spotify source integrates with amp relay smart sensing — add after auto-switch works; prevents amp shutting off during Spotify playback
- [ ] Bitrate selection in Web UI + MQTT — add once core path is stable; requires raspotify restart on change
- [ ] Configurable auto-switch toggle + conflict behavior — add after auto-switch is implemented

### Future Consideration (v2+)

- [ ] Spotify lossless if librespot adds FLAC support — wait for upstream
- [ ] Spotify session transfer (hand off from phone to device seamlessly mid-song) — librespot supports this but needs UX validation
- [ ] Album art in Web UI Spotify card — trivially easy (Spotify API returns URLs), add if users request it

---

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| Device appears in Spotify app | HIGH | MEDIUM (raspotify setup) | P1 |
| Track name + artist on TFT | HIGH | MEDIUM | P1 |
| Play/Pause/Skip controls | HIGH | LOW (Web API) | P1 |
| Volume knob → Spotify sync | HIGH | MEDIUM | P1 |
| MQTT `media/*` topics for HA | HIGH | LOW (existing MQTT infra) | P1 |
| `SPOTIFY` as input source | MEDIUM | LOW (AppState field) | P1 |
| Auto-switch to Spotify | HIGH | MEDIUM | P2 |
| HA media_player entity | HIGH | MEDIUM | P2 |
| Amp relay power mgmt + Spotify | MEDIUM | LOW (extend existing FSM) | P2 |
| Bitrate selection (96/160/320) | LOW | LOW | P2 |
| Configurable auto-switch toggle | MEDIUM | LOW | P2 |
| Album art in Web UI | LOW | LOW | P3 |
| Spotify lossless support | MEDIUM | HIGH (upstream dependency) | P3 |

**Priority key:**
- P1: Must have for launch
- P2: Should have, add when possible
- P3: Nice to have, future consideration

---

## Competitor Feature Analysis

Comparing to other DIY / semi-commercial Spotify Connect implementations:

| Feature | ThingPulse ESP32 Spotify Remote | Squeezelite-ESP32 | ALX Nova Target |
|---------|--------------------------------|-------------------|-----------------|
| Appears as Connect device | No (Web API remote only) | Yes (via LMS plugin) | Yes (raspotify) |
| Phone-independent streaming | No | Yes | Yes |
| Track info on display | Yes (album art + title) | Yes | Yes (title + artist, no album art) |
| Volume sync bidirectional | Partial | Yes | Yes |
| MQTT / HA integration | No | Partial | Yes (full HA discovery) |
| Physical controls (encoder) | Touchscreen only | Buttons | Rotary encoder |
| Input source switching | No | No | Yes (unique to ALX Nova) |
| Amp relay power management | No | No | Yes (unique to ALX Nova) |
| DSP pipeline integration | No | No | Yes (Spotify → DAC via existing DSP) |
| Audio quality selection | No | Yes | Yes |

ALX Nova's differentiators are the existing infrastructure: MQTT/HA, amp relay management, DSP pipeline, and the input switching framework. These are genuinely unique — no off-the-shelf Spotify Connect speaker has them.

---

## Spotify Display Requirements (Official — Binding)

Per Spotify's official Speaker with Display requirements (verified from `developer.spotify.com`):

- **Mandatory at all times**: Spotify attribution (logo or "Spotify" text) + track metadata (song title, artist, album)
- **For displays too small for full logo**: Round Spotify icon minimum 42x42px is acceptable; below that, use text "Spotify" only
- **Album art**: Optional for small displays — may be omitted. The 160x128 TFT qualifies as "small" by these standards
- **Scrolling text**: Acceptable for metadata that doesn't fit in one row
- **Separator**: Use "•" to separate metadata fields in a single row

ALX Nova at 160x128 (landscape): Show Spotify icon (42x42) + scrolling "Title • Artist" row. This meets the mandatory requirements.

---

## Sources

- [Spotify Technical Requirements for Commercial Hardware](https://developer.spotify.com/documentation/commercial-hardware/implementation/requirements/technical) — HIGH confidence (official)
- [Spotify Speaker with Display Requirements](https://developer.spotify.com/documentation/commercial-hardware/implementation/requirements/speaker-display) — HIGH confidence (official)
- [Spotify ZeroConf API Guide](https://developer.spotify.com/documentation/commercial-hardware/implementation/guides/zeroconf) — HIGH confidence (official)
- [Spotify Connect Basics Guide](https://developer.spotify.com/documentation/commercial-hardware/implementation/guides/connect-basics) — HIGH confidence (official)
- [Spotify Commercial Hardware Overview](https://developer.spotify.com/documentation/commercial-hardware) — HIGH confidence (official)
- [go-librespot on GitHub](https://github.com/devgianlu/go-librespot) — MEDIUM confidence (community project, actively maintained as of 2025)
- [librespot on GitHub](https://github.com/librespot-org/librespot) — MEDIUM confidence (community project)
- [ThingPulse ESP32 Spotify Remote](https://github.com/ThingPulse/esp32-spotify-remote) — MEDIUM confidence (community reference)
- [SpotifyEsp32 library](https://github.com/FinianLandes/SpotifyEsp32) — MEDIUM confidence (community, updated for new Spotify OAuth)
- [Spotify Lossless Launch (September 2025)](https://newsroom.spotify.com/2025-09-10/lossless-listening-arrives-on-spotify-premium-with-a-richer-more-detailed-listening-experience/) — HIGH confidence (official Spotify newsroom)
- [What Hi-Fi: Spotify Connect + Lossless](https://www.whathifi.com/advice/spotify-connect-what-it-how-can-you-get-it) — MEDIUM confidence (editorial)
- [Spotify Web API February 2026 Changelog](https://developer.spotify.com/documentation/web-api/references/changes/february-2026) — HIGH confidence (official)

---
*Feature research for: Spotify Connect streaming on ALX Nova ESP32-S3 amplifier controller*
*Researched: 2026-02-17*
