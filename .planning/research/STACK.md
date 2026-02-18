# Stack Research

**Domain:** Spotify Connect streaming integration on ESP32-S3 (PlatformIO + Arduino framework)
**Researched:** 2026-02-17
**Confidence:** MEDIUM — cspot is the only viable path; framework integration is non-trivial and sparsely documented

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| cspot | master (no releases) | Spotify Connect protocol + Ogg Vorbis decode | Only open-source Spotify Connect implementation targeting ESP32. Implements ZeroConf discovery, Shannon/Spirc protocol, and internally bundles tremor (fixed-point Vorbis decoder). Outputs 44.1kHz 16-bit stereo PCM via `feedPCMFrames()`. No alternative exists that provides actual audio streaming (not just Web API control). |
| bell (submodule of cspot) | develop branch | Audio utilities, codec dependencies, HTTP server | Automatically pulled with cspot via `--recursive` clone. Contains tremor (Vorbis), civetweb (embedded HTTP), nanopb (protobuf), mdnssvc. Do not vendor separately. |
| ESP-IDF dual framework | via `framework = arduino, espidf` in platformio.ini | Enables ESP-IDF CMake components alongside Arduino code | cspot's build system is CMake/ESP-IDF only. The dual-framework mode in PlatformIO (`framework = arduino, espidf`) allows cspot to be added as an `extra_component_dirs` component while keeping all existing Arduino code intact. Pure Arduino-only framework cannot build cspot. |
| ESPmDNS (Arduino built-in) | bundled with arduino-esp32 | Advertise `_spotify-connect._tcp` service | Already in the project via Arduino core. Used to broadcast the ZeroConf endpoint that Spotify apps use to discover the device. cspot's bell library includes `mdnssvc` but the Arduino `ESPmDNS` class is simpler to control from the application layer. MEDIUM confidence — cspot may handle mDNS internally; verify whether to use cspot's mdnssvc or Arduino ESPmDNS. |

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| witnessmenow/spotify-api-arduino | latest (main branch, no versioned release) | Spotify Web API: now-playing metadata, track info, playback control | When building the "now playing" display on TFT/GUI. Provides GET `/me/player/currently-playing`, track metadata, artwork URL, playback state. Uses WiFiClientSecure with PKCE auth flow. Does NOT stream audio — complementary to cspot. |
| ArduinoJson | ^7.4.2 (already in project) | Parse cspot config, ZeroConf HTTP responses, Web API JSON | Already a dependency. Use it to build the ZeroConf HTTP responses (`/zeroconf` GET/POST endpoints) and parse Spotify Web API JSON. |
| WebSockets | ^2.7.2 (already in project) | Broadcast now-playing state to web UI | Already present. Extend existing `sendAudioData()` pattern with a `spotifyState` WS message type. |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| PlatformIO dual framework | Build cspot as ESP-IDF component alongside Arduino | Add `framework = arduino, espidf` to platformio.ini env. Add `extra_component_dirs = path/to/cspot/cspot, path/to/cspot/cspot/bell` to a `CMakeLists.txt` in project root. This is the critical build integration step. |
| git submodules | Pull cspot + bell transitively | Clone cspot with `git submodule add https://github.com/feelfreelinux/cspot lib/cspot` then `git submodule update --init --recursive` to get bell and its externals (tremor, nanopb, civetweb, etc.). |
| Spotify Developer Dashboard | Register app for Web API access | Required for `witnessmenow/spotify-api-arduino`. Create app, set redirect URI, copy Client ID + Secret. Not needed for cspot itself (cspot uses ZeroConf + Spotify's access point protocol, not the Web API). |

## Installation

```bash
# Add cspot as a git submodule (pulls bell + all externals recursively)
cd /path/to/ALX_Nova_Controller
git submodule add https://github.com/feelfreelinux/cspot lib/cspot
git submodule update --init --recursive

# platformio.ini change: add espidf to framework
# framework = arduino, espidf

# Add to platformio.ini build_flags:
# -D SPOTIFY_CONNECT_ENABLED

# Spotify Web API library via lib_deps
# witnessmenow/spotify-api-arduino @ https://github.com/witnessmenow/spotify-api-arduino.git
```

### platformio.ini Dual Framework Pattern

```ini
[env:esp32-s3-devkitm-1]
platform = espressif32
board = esp32-s3-devkitm-1
framework = arduino, espidf          ; <-- critical change
; ... rest of existing config unchanged
```

### Root CMakeLists.txt (new file required for ESP-IDF component discovery)

```cmake
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS
    lib/cspot/cspot
    lib/cspot/cspot/bell
)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ALX_Nova_Controller)
```

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| cspot (C++ library, submodule) | squeezelite-esp32 (full firmware) | Never — squeezelite-esp32 replaces your entire firmware. You cannot use it as a library within an existing project. |
| cspot | librespot (Rust) | Never for ESP32 — librespot requires a full Linux environment. Not embeddable. |
| cspot | SpotifyEsp32 / spotify-api-arduino for streaming | Never — these are Web API wrappers. They can control playback on other devices but cannot make the ESP32 itself appear as a Spotify Connect speaker. |
| dual framework (arduino + espidf) | pure ESP-IDF (drop Arduino) | Only if you are willing to rewrite ALL existing Arduino code. High cost for an existing project of this size. Avoid. |
| tremor (bundled in bell) | stb_vorbis | No action needed — tremor is already bundled in cspot's bell submodule. Do not add a second Vorbis decoder. stb_vorbis requires floating point; tremor is fixed-point (faster on ESP32-S3 without FPU for this use case). |
| witnessmenow/spotify-api-arduino | SpotifyEsp32 (FinianLandes) | Both work for Web API metadata. witnessmenow is more mature (112 commits) and explicitly supports ESP32. Either is acceptable; witnessmenow has more documented playback control features. |

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| SpotifyEsp32 / spotify-esp for audio streaming | These are Spotify Web API wrappers — they control Spotify on other devices. They cannot make the ESP32 a Connect speaker or stream audio. Misleadingly named. | cspot |
| librespot | Rust-only, requires Linux. Not usable on bare-metal ESP32. | cspot |
| Pure Arduino framework for cspot | cspot's CMake build system requires ESP-IDF component infrastructure. It cannot be compiled as a standard Arduino lib_deps entry. Issue #139 on cspot's tracker confirms no Arduino-only solution exists. | arduino + espidf dual framework |
| Arduino ESPmDNS for low-level ZeroConf negotiation | The ZeroConf protocol requires a full HTTP server to handle GET (device info) and POST (credential blob) at the `/zeroconf` path. ESPmDNS handles service advertisement only; civetweb (bundled in bell) handles the HTTP server. Do not replace civetweb with AsyncWebServer or the existing port-80 server — port conflicts will occur. | cspot's bundled civetweb on a separate port (default 8080) |
| Removing the existing mbedTLS PSRAM linker wraps | The project already has `-Wl,--wrap=esp_mbedtls_mem_calloc` and `-Wl,--wrap=esp_mbedtls_mem_free` redirecting TLS allocations to PSRAM. cspot's Shannon encryption and SHA operations will crash with internal heap OOM if these wraps are removed. This was confirmed on ESP32-S3 in squeezelite-esp32 issue #419 (crypto abort with "Failed to allocate buf memory"). | Keep existing wraps |
| Feeding cspot PCM directly to `i2s_write()` without format conversion | cspot outputs int16 stereo (2 bytes per channel). The existing DAC pipeline uses 24-bit left-justified I2S frames (int32, data in bits [31:8]). Writing raw int16 buffers directly will produce wrong sample alignment and distorted audio. | Convert: `int32_t sample = ((int32_t)pcm16_sample) << 8` per channel before feeding to i2s_write |

## Audio Format Bridging

cspot's `feedPCMFrames(std::vector<uint8_t>& data)` delivers interleaved int16 stereo at 44.1kHz:

```
[L_low][L_high][R_low][R_high][L_low][L_high]...
```

The existing PCM5102A DAC output uses the existing `i2s_write()` path with 32-bit frames (24-bit data left-justified, bits [31:8]):

```cpp
// Conversion in the custom AudioSink::feedPCMFrames() implementation:
for (size_t i = 0; i < data.size(); i += 4) {
    int16_t left  = (int16_t)(data[i]   | (data[i+1] << 8));
    int16_t right = (int16_t)(data[i+2] | (data[i+3] << 8));
    int32_t left32  = (int32_t)left  << 8;   // 16-bit -> 24-bit in 32-bit frame
    int32_t right32 = (int32_t)right << 8;
    // Write to ring buffer, read by I2S TX task
}
```

The existing DAC `configure(44100, 16)` method must support 44.1kHz (most PCM5102A setups already handle this rate). The existing I2S TX on GPIO40 already runs in full-duplex mode on I2S_NUM_0.

## ZeroConf Endpoint Requirements

The Spotify app discovers the device via mDNS (`_spotify-connect._tcp`) and then hits two HTTP endpoints:

```
GET  /zeroconf  -> JSON: {status:101, deviceID, publicKey, remoteName, version:"1.0", ...}
POST /zeroconf  -> Receives encrypted credentials blob, calls cspot login, returns {status:101}
```

cspot handles this automatically via its bundled civetweb HTTP server on port 8080. The TXT record must be `CPath=/zeroconf`. The ESPmDNS advertisement:

```cpp
MDNS.addService("spotify-connect", "tcp", 8080);
MDNS.addServiceTxt("spotify-connect", "tcp", "CPath", "/zeroconf");
MDNS.addServiceTxt("spotify-connect", "tcp", "VERSION", "1.0");
```

## Stack Patterns by Variant

**If Spotify Web API metadata display is needed (now-playing on TFT):**
- Add `witnessmenow/spotify-api-arduino` to lib_deps
- Implement PKCE authorization flow via the existing web server (redirect URI on port 80)
- Cache refresh token in NVS/Preferences

**If Spotify Web API metadata display is NOT needed (audio only):**
- Skip `witnessmenow/spotify-api-arduino`
- cspot provides basic track title/artist via its SpircHandler callbacks
- Sufficient for basic display

**If flash space is tight (currently 77.6% used):**
- cspot + bell add substantial code: tremor (~60KB), civetweb (~40KB), nanopb (~20KB), cspot core (~100KB) — rough estimates, LOW confidence
- May need to switch from `huge_app.csv` partition or reduce other features
- Consider enabling partition table with larger app slot: `board_build.partitions = huge_app.csv`

## Version Compatibility

| Component | Compatible With | Notes |
|-----------|-----------------|-------|
| cspot master | ESP-IDF 4.4.x — 5.x | Issue #156 reports build errors with IDF 5.1.1; issue #176 (Dec 2024) questions which IDF is tested. The existing Arduino ESP32 core in this project bundles IDF 5.1.x. Compatibility with cspot master is MEDIUM confidence — may need patching. |
| cspot master | arduino-esp32 3.x (IDF 5.1) | Dual framework pulls arduino-esp32 as an IDF component. The existing project uses arduino-esp32 with IDF 5.1. Crypto crash on S3 is mitigated by existing mbedTLS PSRAM wraps. HIGH confidence the wraps solve the known crash. |
| witnessmenow/spotify-api-arduino | Arduino core 2.x / 3.x | Uses `WiFiClientSecure` which is a compat alias in 3.x. Compatible. |
| tremor (bundled) | No conflicts | Fixed-point Vorbis. No symbol conflicts with arduinoFFT or ESP-DSP since Vorbis symbols are namespaced within bell. MEDIUM confidence — verify no duplicate symbol issues with `libespressif__esp-dsp.a`. |
| civetweb (bundled in bell) | Port 8080 | Will conflict if anything else uses port 8080. Existing servers use port 80 (HTTP) and 81 (WebSocket). Safe. |
| nanopb (bundled in bell) | No protobuf in existing project | No conflicts expected. |
| mbedTLS wraps (existing) | cspot crypto | CRITICAL — keep existing `-Wl,--wrap=esp_mbedtls_mem_calloc` and `-Wl,--wrap=esp_mbedtls_mem_free` linker flags. cspot's Shannon + SHA crypto WILL crash without PSRAM redirect on ESP32-S3. |

## Sources

- https://github.com/feelfreelinux/cspot — Main repository, README, issue tracker, targets/esp32 structure (HIGH confidence)
- https://github.com/feelfreelinux/cspot/blob/master/.gitmodules — Confirmed bell is only submodule (HIGH confidence)
- https://github.com/feelfreelinux/bell/tree/develop/external — Confirmed tremor, nanopb, civetweb, opus, libhelix-mp3 bundled (HIGH confidence)
- https://github.com/feelfreelinux/cspot/issues/139 — Confirmed no Arduino-only framework support (HIGH confidence)
- https://github.com/feelfreelinux/cspot/issues/176 — IDF version uncertainty as of Dec 2024 (LOW confidence on IDF compat)
- https://github.com/sle118/squeezelite-esp32/issues/419 — ESP32-S3 crypto crash confirmed, PSRAM fix identified (MEDIUM confidence — fix inferred, not explicitly confirmed resolved)
- https://developer.spotify.com/documentation/commercial-hardware/implementation/guides/zeroconf — Official ZeroConf API spec (HIGH confidence)
- https://github.com/witnessmenow/spotify-api-arduino — Web API library for metadata (MEDIUM confidence — "work in progress" caveat in README)
- feelfreelinux/cspot targets/esp32/main.cpp — ZeroConf flow (mDNS init, HTTP server port 8080, LoginBlob) confirmed (HIGH confidence)
- feelfreelinux/cspot cspot/CMakeLists.txt — Confirmed C++20 required, bell + nanopb as build deps (HIGH confidence)

---
*Stack research for: Spotify Connect integration on ESP32-S3 ALX Nova Controller*
*Researched: 2026-02-17*
