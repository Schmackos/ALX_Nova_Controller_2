# ALX Nova Controller — Codebase Improvement Plan

**Created:** 2026-02-06
**Context:** Production/commercial deployment
**Baseline:** 232 native tests (all passing), firmware v1.4.2

---

## Codebase Health Summary

| Area | Rating | Key Strengths | Critical Gaps |
|------|--------|--------------|---------------|
| **Frontend/Web UI** | 9.2/10 | Full SPA, dual themes, 6500-line embedded UI | Monolithic file, no PWA, no a11y |
| **Backend/APIs** | 8.5/10 | 36 REST endpoints, 35 HA entities, dirty flags | Blocking OTA, no rate limiting, no API versioning |
| **Security** | 6/10 | SHA256 OTA verification, session auth, HW RNG | Plaintext creds, no HTTPS, weak defaults, no CSRF |
| **Authentication** | 7/10 | Sliding-window sessions, WS auth reuse, LRU eviction | No lockout, weak password policy, export leaks creds |

---

## Decisions Made

- **Deployment:** Production/Commercial — security hardening is critical
- **Priority:** Phase 1 (Security) first
- **Macro migration:** All 78 `#define` aliases removed in one commit before security work
- **MQTT TLS:** Yes, add `WiFiClientSecure` support
- **Default password:** Keep as-is for now (`12345678`)
- **Tests:** Must pass at every commit (no temporary breakage)

---

## Step 0 — Macro Migration (Clean Slate)

**Goal:** Remove all 78 `#define` aliases in `app_state.h` (lines ~240-318) and replace every usage across the entire codebase with `appState.member` syntax.

**Why first:** Clean slate makes all subsequent security changes easier to review and reduces namespace pollution.

**Rules:**
- Single commit: `refactor: Remove legacy #define macros, use appState.member directly`
- All 232 native tests must pass after
- ESP32-S3 build must compile clean
- Touch every `.cpp`/`.h` file that references the old macro names

**Files likely affected:**
- `src/app_state.h` — Remove the 78 `#define` lines
- `src/main.cpp` — Heavy usage of macros
- `src/smart_sensing.cpp/.h`
- `src/wifi_manager.cpp/.h`
- `src/mqtt_handler.cpp/.h`
- `src/ota_updater.cpp/.h`
- `src/settings_manager.cpp`
- `src/websocket_handler.cpp/.h`
- `src/auth_handler.cpp`
- `src/button_handler.cpp`
- `src/buzzer_handler.cpp/.h`
- `src/web_pages.cpp`
- `src/gui/screens/*.cpp` (any that reference macros)
- `src/gui/gui_manager.cpp`
- `test/` files that include `app_state.h`

---

## Phase 1A — Cookie & Session Security

**Priority:** CRITICAL

### 1A.1 Fix Cookie Flags
- **File:** `src/auth_handler.cpp:315`
- **Current:** `"sessionId=" + sessionId + "; Path=/; Max-Age=3600"`
- **Fix:** Add `HttpOnly; SameSite=Strict`
- **Note:** This breaks WebSocket auth which currently reads session from JS cookie

### 1A.2 WebSocket Auth Mechanism
- **Problem:** If cookie is `HttpOnly`, JS can't read session ID for WS auth
- **Solution:** Implement short-lived WS token endpoint:
  1. Add `GET /api/auth/ws-token` — returns a one-time token (valid 30s)
  2. Client fetches token before WS connect, sends in WS auth message
  3. Server validates token in `webSocketEvent()`, then discards it
- **Files:** `src/auth_handler.cpp/.h`, `src/websocket_handler.cpp`, `src/web_pages.cpp` (JS)

### 1A.3 Session Invalidation on Password Change
- **File:** `src/auth_handler.cpp` — `handlePasswordChange()` (~line 370-420)
- **Fix:** After password change, call `clearAllSessions()` (new function to zero all 5 slots)
- **Keep current session active** by re-creating it after clear

### 1A.4 Enforce WebSocket Auth Timeout
- **File:** `src/websocket_handler.cpp`
- **Problem:** `wsAuthTimeout[num]` is set to `millis() + 5000` but never checked
- **Fix:** In `webSocketEvent()` WStype_TEXT handler, check if `millis() > wsAuthTimeout[num]` for unauthenticated clients and disconnect them
- **Also add:** Periodic check in `webSocket.loop()` or a helper called from main loop

### 1A.5 Login Lockout
- **File:** `src/auth_handler.cpp`
- **Implement:** Track failed attempts per IP (simple array, max 10 IPs)
- **Policy:** 5 failed attempts → 15-minute lockout for that IP
- **Response:** Return `429 Too Many Requests` with retry-after header
- **Storage:** In-memory only (resets on reboot, which is acceptable)

---

## Phase 1B — HTTP Security Headers

**Priority:** HIGH

### 1B.1 Add Security Headers
- **File:** `src/main.cpp` — Add to all HTTP responses (or use a wrapper)
- **Headers:**
  ```
  X-Frame-Options: DENY
  X-Content-Type-Options: nosniff
  X-XSS-Protection: 1; mode=block
  ```

### 1B.2 Content Security Policy
- **File:** `src/main.cpp` or `src/web_pages.cpp`
- **Policy:** `default-src 'self'; script-src 'self' 'unsafe-inline' https://cdn.jsdelivr.net; style-src 'self' 'unsafe-inline'; img-src 'self' data:`
- **Note:** Must whitelist CDN for QR code (qrcode.js) and Markdown (marked.js) libraries used in Support tab

### 1B.3 CSRF Protection
- **Files:** `src/auth_handler.cpp/.h`, `src/main.cpp`, `src/web_pages.cpp`
- **Implement:**
  1. Generate CSRF token on login, return in response body
  2. Store token in AppState per session
  3. Client sends token in `X-CSRF-Token` header on all POST requests
  4. Server validates token before processing POST endpoints
- **Exempt:** `/api/auth/login` (no session yet), `/api/firmware/upload` (multipart)

---

## Phase 1C — Credential Protection

**Priority:** HIGH

### 1C.1 Remove Passwords from Settings Export
- **File:** `src/settings_manager.cpp` — `handleSettingsExport()` (~lines 449-540)
- **Remove:** `doc["wifi"]["password"]` and `doc["accessPoint"]["password"]`
- **Add:** `doc["wifi"]["hasPassword"] = true/false` (boolean indicator only)
- **Web UI:** Update export dialog to note "Passwords are excluded for security"

### 1C.2 SHA256 for Manual Firmware Upload
- **File:** `src/ota_updater.cpp` — manual upload handler (~lines 804-937)
- **Add:** Optional SHA256 field in upload form
- **Verify:** If provided, calculate SHA256 during chunk upload and compare at end
- **Warn:** If no checksum provided, log warning but allow upload

### 1C.3 MQTT Topic Validation
- **File:** `src/mqtt_handler.cpp` — `handleMqttPost()` (~lines 1558-1635)
- **Validate:** Reject base topics containing `#`, `+`, or leading/trailing `/`
- **Max length:** 128 characters

### 1C.4 Input Length Limits
- **Files:** All API handlers
- **Add maximum lengths:**
  - SSID: 32 chars
  - WiFi password: 63 chars
  - MQTT broker: 253 chars (DNS max)
  - MQTT username: 128 chars
  - MQTT password: 128 chars
  - MQTT base topic: 128 chars
  - Web password: 64 chars

---

## Phase 2A — MQTT TLS

**Priority:** HIGH (production requirement)

### 2A.1 WiFiClientSecure for MQTT
- **File:** `src/mqtt_handler.cpp`
- **Add:** `WiFiClientSecure` alongside existing `WiFiClient`
- **Selection:** Based on `appState.mqttTLS` boolean flag
- **Default:** TLS disabled (backward compatible)

### 2A.2 TLS Configuration in AppState
- **File:** `src/app_state.h`
- **Add:** `bool mqttTLS = false;`
- **Persist:** Add to `/mqtt_config.txt` (new line 8)

### 2A.3 Web/GUI/MQTT UI Updates
- **Web UI:** Add TLS toggle in MQTT settings card, auto-switch port to 8883 when TLS enabled
- **GUI:** Add TLS toggle to `scr_mqtt.cpp` menu
- **MQTT HA:** Update discovery entities if needed

### 2A.4 Certificate Handling
- **Option 1:** Use built-in root CAs (like OTA updater does)
- **Option 2:** `setInsecure()` for self-signed brokers with a toggle
- **Ship with:** Common MQTT broker CAs (Let's Encrypt, etc.)

---

## Phase 2B — Reliability

**Priority:** HIGH

### 2B.1 Non-Blocking OTA Update
- **File:** `src/ota_updater.cpp`
- **Change:** Move `performOTAUpdate()` into a FreeRTOS task
- **Progress:** Report via WebSocket `updateStatus` messages (already exists)
- **Block:** Prevent other operations during OTA (set FSM to `STATE_OTA_UPDATE`)

### 2B.2 Watchdog Timer
- **File:** `src/main.cpp` or `src/tasks.cpp`
- **Add:** ESP32 Task Watchdog Timer (TWDT) on main loop task
- **Timeout:** 30 seconds
- **Feed:** In main `loop()` at each iteration

### 2B.3 Async WiFi Scan
- **File:** `src/wifi_manager.cpp`, `src/gui/screens/scr_wifi.cpp`
- **Change:** Use `WiFi.scanNetworks(true)` (async mode) + poll `WiFi.scanComplete()`
- **GUI:** Show "Scanning..." with spinner while scan runs
- **Web API:** Already non-blocking from web perspective (separate HTTP request)

### 2B.4 Voltage Hysteresis
- **File:** `src/smart_sensing.cpp`
- **Add:** Configurable deadband (e.g., 0.05V above threshold to turn on, threshold to turn off)
- **Prevent:** Rapid relay cycling when voltage hovers near threshold
- **AppState:** Add `voltageHysteresis` float (default 0.05V)

### 2B.5 Settings Format Migration
- **File:** `src/settings_manager.cpp`
- **Change:** Migrate `/settings.txt` from line-based to JSON format
- **Add:** CRC32 or SHA256 checksum field
- **Backward compat:** If JSON parse fails, try line-based format (migration path)
- **This is a larger change** — consider deferring if Phase 1 takes long

---

## Phase 3 — Polish & Features

**Priority:** MEDIUM

### 3.1 API Rate Limiting
- Add per-IP request counter with sliding window
- Limit expensive operations: WiFi scan (1/10s), OTA check (1/60s), export (1/30s)
- Return `429 Too Many Requests` when exceeded

### 3.2 Centralized Error Logging
- Ring buffer (configurable size, default 100 entries)
- Severity levels: DEBUG, INFO, WARN, ERROR
- Accessible via `/api/diagnostics/logs` and WebSocket debug stream
- Replace scattered `Serial.println()` with `DebugLog.error("message")`

### 3.3 OTA Rollback
- Use ESP32 dual OTA partitions (`ota_0` / `ota_1`)
- Mark new firmware as "pending verification"
- If device doesn't confirm health within 60s, revert to previous partition
- Requires partition table update

### 3.4 Password Complexity
- Add requirements: 1 uppercase, 1 lowercase, 1 digit, min 10 chars
- Add strength meter to web UI (client-side zxcvbn-lite or simple rules)
- Reject common passwords (short blocklist)

### 3.5 Web Pages Build Pipeline
- Split `web_pages.cpp` monolith into separate `index.html`, `style.css`, `app.js`
- Add build script: minify + gzip + embed as C arrays
- Enables linting, formatting, and testing of web code separately

---

## Appendix A — Security Findings Detail

### Critical (3)
1. **Cookie missing HttpOnly/SameSite** — `auth_handler.cpp:315` — Session hijack via XSS
2. **Plaintext credential storage** — WiFi/MQTT passwords in NVS/LittleFS unencrypted
3. **Hardcoded default password** — `config.h:99` — `"12345678"` on all devices

### High (6)
1. **No login rate limiting** — Only 1s delay, no lockout (`auth_handler.cpp:275`)
2. **No CORS/security headers** — No X-Frame-Options, CSP, etc.
3. **WebSocket auth timeout not enforced** — `wsAuthTimeout` set but never checked
4. **No MQTT TLS** — Credentials sent plaintext over TCP (`mqtt_handler.cpp:430-436`)
5. **WiFi passwords in NVS plaintext** — Extractable with physical access
6. **Settings export includes WiFi/AP passwords** — `settings_manager.cpp:465,470`

### Medium (9)
1. No CSRF protection on POST endpoints
2. Weak password policy (8-char minimum only)
3. No session invalidation on password change
4. MQTT topic not validated for wildcards/injection
5. No input length limits on many string fields
6. MQTT QoS hardcoded to 0 (fire-and-forget)
7. No manual firmware upload checksum verification
8. Insecure TLS mode available for OTA (`enableCertValidation` toggle)
9. Integer overflow potential in `millis()` comparisons (~49-day wraparound)

### Low (2)
1. WiFi scan info disclosure (expected functionality)
2. `sprintf` usage in OTA (safe — fixed-size buffer correctly sized)

---

## Appendix B — Current API Inventory (36 Endpoints)

### Authentication (4)
| Method | Endpoint | Auth Required |
|--------|----------|:---:|
| POST | `/api/auth/login` | No |
| POST | `/api/auth/logout` | No |
| GET | `/api/auth/status` | No |
| POST | `/api/auth/change` | Yes |

### WiFi (8)
| Method | Endpoint | Auth Required |
|--------|----------|:---:|
| POST | `/api/wificonfig` | Yes |
| POST | `/api/wifisave` | Yes |
| GET | `/api/wifiscan` | Yes |
| GET | `/api/wifilist` | Yes |
| POST | `/api/wifiremove` | Yes |
| POST | `/api/apconfig` | Yes |
| POST | `/api/toggleap` | Yes |
| GET | `/api/wifistatus` | Yes |

### Smart Sensing (2)
| Method | Endpoint | Auth Required |
|--------|----------|:---:|
| GET | `/api/smartsensing` | Yes |
| POST | `/api/smartsensing` | Yes |

### OTA Updates (5)
| Method | Endpoint | Auth Required |
|--------|----------|:---:|
| GET | `/api/checkupdate` | Yes |
| POST | `/api/startupdate` | Yes |
| GET | `/api/updatestatus` | Yes |
| GET | `/api/releasenotes` | Yes |
| POST | `/api/firmware/upload` | Yes |

### MQTT (2)
| Method | Endpoint | Auth Required |
|--------|----------|:---:|
| GET | `/api/mqtt` | Yes |
| POST | `/api/mqtt` | Yes |

### Settings & System (7)
| Method | Endpoint | Auth Required |
|--------|----------|:---:|
| GET | `/api/settings` | Yes |
| POST | `/api/settings` | Yes |
| GET | `/api/settings/export` | Yes |
| POST | `/api/settings/import` | Yes |
| GET | `/api/diagnostics` | Yes |
| POST | `/api/factoryreset` | Yes |
| POST | `/api/reboot` | Yes |

---

## Appendix C — MQTT Home Assistant Entities (35)

| Type | Count | Entities |
|------|-------|----------|
| Switch | 9 | LED Blinking, Amplifier, Access Point, Auto Update, Night Mode, Cert Validation, Backlight, Buzzer |
| Number | 4 | Timer Duration, Voltage Threshold, Screen Timeout, Buzzer Volume |
| Sensor | 11 | Voltage, Timer Remaining, WiFi RSSI, FW Version, Latest Version, IP, CPU Temp, CPU Usage, Free Heap, Uptime, LittleFS Used, WiFi Channel |
| Binary Sensor | 4 | WiFi Connected, Voltage Detected, Update Available |
| Button | 3 | Reboot, Check Updates, Factory Reset |
| Select | 1 | Smart Sensing Mode |
| Update | 1 | Firmware Update |

---

## Appendix D — WebSocket Protocol

### Auth Flow
1. Server → `{"type":"authRequired"}`
2. Client → `{"type":"auth","sessionId":"..."}` (within 5s)
3. Server → `{"type":"authSuccess"}` + initial state broadcasts

### Commands (Client → Server)
`toggle`, `toggleAP`, `getHardwareStats`, `setBacklight`, `setScreenTimeout`, `setBuzzerEnabled`, `setBuzzerVolume`

### Broadcasts (Server → Client)
`ledState`, `blinkingEnabled`, `displayState`, `buzzerState`, `hardware_stats`, `wifiStatus`, `smartSensingState`, `mqttSettings`, `updateStatus`
