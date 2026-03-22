# Code Review Guidelines — ALX Nova Controller 2

> Reviewed by Gemini 2.5 Flash (free tier)

## Always check

### Memory & Heap Safety
- PSRAM allocations use `psram_alloc()` wrapper, never raw `ps_calloc()`
- DMA buffers use `MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA`, never PSRAM
- New allocations are guarded by heap pressure checks (40KB internal reserve)
- `psram_free()` is called with matching label on cleanup paths

### Concurrency & Real-Time
- No `Serial.print`, `LOG_*`, or blocking calls in ISR or `audio_pipeline_task` (Core 1)
- `volatile` on cross-core shared variables
- Dirty-flag pattern for cross-core communication (task sets flag, main loop acts)
- `appState.audio.paused` + semaphore handshake before `i2s_driver_uninstall()`
- No new FreeRTOS tasks pinned to Core 1 (reserved for audio)

### HAL Device Lifecycle
- Slot-indexed `remove_sink(slot)` only — never `clear_sinks()` in deinit paths
- `set_source()` / `set_sink()` return values checked (bool)
- `buildSink()` guards `firstChannel + channelCount <= MATRIX_SIZE`
- HAL state changes fire through `HalStateChangeCb`, not direct pipeline calls
- Device toggles via `requestDeviceToggle(halSlot, action)` only

### I2C Bus Safety
- Bus 0 (GPIO 48/54) operations check `hal_wifi_sdio_active()` first
- I2C transactions on Bus 0 while WiFi active cause MCU reset

### Web UI (`web_src/`)
- No duplicate `let`/`const` declarations (concatenated scope)
- Icons use inline SVG from MDI, no external CDN
- New JS globals added to `web_src/.eslintrc.json`
- Changes to `web_src/` require `node tools/build_web_assets.js` before build

### Security
- Auth endpoints use PBKDF2-SHA256 (10k iterations)
- WebSocket auth via short-lived token (60s TTL)
- HTTP responses include security headers via `http_add_security_headers()`
- No secrets or internal details in user-facing error messages
- Rate limiting on auth endpoints (HTTP 429)

### DSP Pipeline
- Double-buffered config with atomic swap for glitch-free changes
- `dsp_add_stage()` rolls back on pool exhaustion
- Delay lines use PSRAM with heap pre-flight check (40KB reserve)

## Style (nit-level)
- Prefer `appState.domain.field` over legacy `#define` macros
- Use `LOG_I` for state transitions, `LOG_D` for high-frequency details
- Log transitions with static `prev` variables, not repetitive state
- Use `(nowMs >= W) ? (nowMs - W) : 0` pattern for uint32_t time differences

## Skip
- Generated files: `src/web_pages.cpp`, `src/web_pages_gz.cpp`
- GUI code under `src/gui/` (separate review concern)
- Formatting-only changes
- Test mock implementations in `test/test_mocks/`
