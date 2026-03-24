#include "auth_handler.h"

// ===== SECURITY GATE: TEST_MODE must NEVER ship in production firmware =====
#ifdef TEST_MODE
  #warning "TEST_MODE is enabled — rate limiting disabled, fixed password 'test1234'. DO NOT RELEASE this firmware."
#endif

#include "app_state.h"
#include "globals.h"
#include "config.h"
#include "debug_serial.h"
#include "http_security.h"
#include "rate_limiter.h"
#include "websocket_handler.h"
#include "hal/hal_types.h"  // hal_safe_strcpy
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#ifndef NATIVE_TEST
#include <mbedtls/constant_time.h>
#endif

Session activeSessions[MAX_SESSIONS];
Preferences authPrefs;

// Rate limiting state
static int _loginFailCount = 0;
static uint64_t _lastFailTime = 0;
static const uint64_t LOGIN_COOLDOWN_US = 300000000ULL; // 5 minutes in microseconds
static unsigned long _nextLoginAllowedMs = 0; // millis() gate — non-blocking rate limit

// WebSocket one-time token pool (replaces JS cookie reading for WS auth)
#define WS_TOKEN_SLOTS 32
#define WS_TOKEN_TTL_MS 60000  // 60 seconds
struct WsToken {
  char token[37];       // UUID string (36 chars + null)
  char sessionId[37];   // session that generated this token
  uint32_t createdMs;
  bool used;
};
static WsToken _wsTokens[WS_TOKEN_SLOTS];

// PBKDF2 migration flag — set on boot if stored hash is legacy SHA256
static bool _passwordNeedsMigration = false;

// Timing-safe string comparison — constant time regardless of where strings differ
bool timingSafeCompare(const String &a, const String &b) {
  size_t lenA = a.length();
  size_t lenB = b.length();

  // Both empty
  if (lenA == 0 && lenB == 0) return true;

  // Length mismatch: still not secret for password hashes (fixed-length hex)
  if (lenA != lenB) return false;

  // Constant-time XOR comparison — no dependency on mbedTLS
  volatile uint8_t result = 0;
  const char *pA = a.c_str();
  const char *pB = b.c_str();
  for (size_t i = 0; i < lenA; i++) {
    result |= (uint8_t)pA[i] ^ (uint8_t)pB[i];
  }
  return result == 0;
}

// Hash password using SHA256, returns 64-char hex string
String hashPassword(const String &password) {
  uint8_t shaResult[32];

  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char *)password.c_str(),
                    password.length());
  mbedtls_md_finish(&ctx, shaResult);
  mbedtls_md_free(&ctx);

  // Convert to hex string
  char hexStr[65];
  for (int i = 0; i < 32; i++) {
    snprintf(hexStr + (i * 2), 3, "%02x", shaResult[i]);
  }
  hexStr[64] = '\0';

  return String(hexStr);
}

// Helper: convert bytes to hex string
static void bytesToHex(const uint8_t *bytes, size_t len, char *out) {
  for (size_t i = 0; i < len; i++) {
    snprintf(out + (i * 2), 3, "%02x", bytes[i]);
  }
  out[len * 2] = '\0';
}

// Helper: convert hex string to bytes
static bool hexToBytes(const char *hex, uint8_t *out, size_t outLen) {
  for (size_t i = 0; i < outLen; i++) {
    unsigned int val;
    if (sscanf(hex + (i * 2), "%02x", &val) != 1) return false;
    out[i] = (uint8_t)val;
  }
  return true;
}

// Internal helper: hash with explicit salt and iteration count, producing a prefixed hash
static String hashPasswordPbkdf2WithSaltAndIter(const String &password, const uint8_t *salt, int iterations, const char* prefix) {
  uint8_t derivedKey[32];

  mbedtls_pkcs5_pbkdf2_hmac_ext(
    MBEDTLS_MD_SHA256,
    (const unsigned char *)password.c_str(), password.length(),
    salt, 16, iterations, 32, derivedKey);

  char saltHex[33], keyHex[65];
  bytesToHex(salt, 16, saltHex);
  bytesToHex(derivedKey, 32, keyHex);

  return String(prefix) + saltHex + ":" + keyHex;
}

// Hash with explicit salt using legacy 10k iterations — produces "p1:" format
static String hashPasswordPbkdf2WithSalt(const String &password, const uint8_t *salt) {
  return hashPasswordPbkdf2WithSaltAndIter(password, salt, PBKDF2_ITERATIONS_V1, "p1:");
}

// Hash with explicit salt using current 50k iterations — produces "p2:" format
static String hashPasswordPbkdf2V2WithSalt(const String &password, const uint8_t *salt) {
  return hashPasswordPbkdf2WithSaltAndIter(password, salt, PBKDF2_ITERATIONS, "p2:");
}

// Hash password with PBKDF2-SHA256 + random salt (current v2 format)
String hashPasswordPbkdf2(const String &password) {
  uint8_t salt[16];
  esp_fill_random(salt, 16);
  return hashPasswordPbkdf2V2WithSalt(password, salt);
}

// Verify password against stored hash (supports p2:, p1:, and legacy SHA256)
bool verifyPassword(const String &inputPassword, const String &storedHash) {
  if (storedHash.startsWith("p2:") && storedHash.length() == 100) {
    // PBKDF2 v2 format: "p2:<32-char salt>:<64-char key>" (50k iterations)
    uint8_t salt[16];
    if (!hexToBytes(storedHash.c_str() + 3, salt, 16)) return false;

    String computed = hashPasswordPbkdf2V2WithSalt(inputPassword, salt);
    return timingSafeCompare(computed, storedHash);
  }

  if (storedHash.startsWith("p1:") && storedHash.length() == 100) {
    // PBKDF2 v1 format: "p1:<32-char salt>:<64-char key>" (10k iterations)
    uint8_t salt[16];
    if (!hexToBytes(storedHash.c_str() + 3, salt, 16)) return false;

    String computed = hashPasswordPbkdf2WithSalt(inputPassword, salt);
    return timingSafeCompare(computed, storedHash);
  }

  // Legacy SHA256 format: 64-char hex
  return timingSafeCompare(hashPassword(inputPassword), storedHash);
}

// Get progressive login delay based on fail count
static unsigned long getLoginDelay() {
  // Progressive: 1s, 2s, 5s, 10s, 30s (capped)
  static const unsigned long delays[] = {1000, 2000, 5000, 10000, 30000};
  int idx = _loginFailCount - 1;
  if (idx < 0)
    return 0;
  if (idx > 4)
    idx = 4;
  return delays[idx];
}

// Reset rate limiting (for factory reset)
void resetLoginRateLimit() {
  _loginFailCount = 0;
  _lastFailTime = 0;
  _nextLoginAllowedMs = 0;
}

// Generate a one-time WS auth token bound to the requesting session
String generateWsToken() {
  uint32_t now = millis();
  String currentSession = getSessionFromCookie();

  // Purge expired/used entries
  for (int i = 0; i < WS_TOKEN_SLOTS; i++) {
    if (_wsTokens[i].token[0] != '\0' &&
        (_wsTokens[i].used || (now - _wsTokens[i].createdMs > WS_TOKEN_TTL_MS))) {
      _wsTokens[i].token[0] = '\0';
    }
  }

  // Find free slot
  for (int i = 0; i < WS_TOKEN_SLOTS; i++) {
    if (_wsTokens[i].token[0] == '\0') {
      String uuid = generateSessionId();  // reuse UUID generator
      hal_safe_strcpy(_wsTokens[i].token, sizeof(_wsTokens[i].token), uuid.c_str());
      hal_safe_strcpy(_wsTokens[i].sessionId, sizeof(_wsTokens[i].sessionId), currentSession.c_str());
      _wsTokens[i].createdMs = now;
      _wsTokens[i].used = false;
      return uuid;
    }
  }

  return "";  // no free slot
}

// Validate and consume a one-time WS token, returns bound session ID
bool validateWsToken(const String &token, String &outSessionId) {
  if (token.length() == 0) return false;
  uint32_t now = millis();

  for (int i = 0; i < WS_TOKEN_SLOTS; i++) {
    if (_wsTokens[i].token[0] != '\0' && !_wsTokens[i].used &&
        (now - _wsTokens[i].createdMs <= WS_TOKEN_TTL_MS) &&
        timingSafeCompare(String(_wsTokens[i].token), token)) {
      _wsTokens[i].used = true;
      outSessionId = String(_wsTokens[i].sessionId);
      return true;
    }
  }
  return false;
}

// Handler: Get WS auth token (requires valid session cookie)
void handleGetWsToken() {
  String token = generateWsToken();

  JsonDocument response;
  if (token.length() > 0) {
    response["success"] = true;
    response["token"] = token;
  } else {
    response["success"] = false;
    response["error"] = "Too many pending tokens";
  }

  String responseStr;
  serializeJson(response, responseStr);
  server_send(token.length() > 0 ? 200 : 429, "application/json", responseStr);
}

// Character set for default password (excludes ambiguous: 0/O, 1/l/I)
static const char _pwdCharset[] =
    "abcdefghijkmnopqrstuvwxyz"
    "ABCDEFGHJKLMNPQRSTUVWXYZ"
    "23456789";
static const size_t _pwdCharsetLen = sizeof(_pwdCharset) - 1;  // 57 chars

// Generate random per-device default password (10 chars with dash: AbC3x-Kf9Yz)
String generateDefaultPassword() {
  uint8_t randomBytes[10];
  esp_fill_random(randomBytes, 10);

  char pwd[12];  // 5 + dash + 5 + null
  for (int i = 0; i < 5; i++) {
    pwd[i] = _pwdCharset[randomBytes[i] % _pwdCharsetLen];
  }
  pwd[5] = '-';
  for (int i = 0; i < 5; i++) {
    pwd[6 + i] = _pwdCharset[randomBytes[5 + i] % _pwdCharsetLen];
  }
  pwd[11] = '\0';

  return String(pwd);
}

// Initialize authentication system
void initAuth() {
  // Clear all sessions
  for (int i = 0; i < MAX_SESSIONS; i++) {
    activeSessions[i].sessionId = "";
    activeSessions[i].createdAt = 0;
    activeSessions[i].lastSeen = 0;
  }

  // Load password hash from NVS (supports PBKDF2, legacy SHA256, and plaintext)
  authPrefs.begin("auth", false);

  if (authPrefs.isKey("pwd_hash")) {
    appState.wifi.webPassword = authPrefs.getString("pwd_hash", "");
    if (appState.wifi.webPassword.startsWith("p2:")) {
      LOG_I("[Auth] Loaded PBKDF2 v2 password hash from NVS");
    } else if (appState.wifi.webPassword.startsWith("p1:")) {
      // p1: hash uses 10k iterations — flag for upgrade to p2: (50k) on next login
      _passwordNeedsMigration = true;
      LOG_I("[Auth] Loaded PBKDF2 v1 hash (will migrate to v2 on next login)");
    } else {
      // Legacy bare SHA256 — flag for migration on next successful login
      _passwordNeedsMigration = true;
      LOG_I("[Auth] Loaded legacy SHA256 hash (will migrate on next login)");
    }
  } else if (authPrefs.isKey("web_pwd")) {
    // Legacy plaintext — migrate to PBKDF2 immediately
    String plaintext = authPrefs.getString("web_pwd", "");
    if (plaintext.length() > 0) {
      String hashed = hashPasswordPbkdf2(plaintext);
      authPrefs.putString("pwd_hash", hashed);
      authPrefs.remove("web_pwd");
      appState.wifi.webPassword = hashed;
      LOG_I("[Auth] Migrated plaintext password to PBKDF2 hash");
    } else {
      appState.wifi.webPassword = hashPasswordPbkdf2(appState.wifi.apPassword);
      authPrefs.putString("pwd_hash", appState.wifi.webPassword);
      LOG_I("[Auth] Using default password (AP password, PBKDF2)");
    }
  } else {
    // First boot — generate unique per-device password
#ifdef TEST_MODE
    String defaultPwd = "test1234";
    LOG_W("[Auth] TEST MODE: using fixed password 'test1234'");
#else
    String defaultPwd = generateDefaultPassword();
#endif
    appState.wifi.webPassword = hashPasswordPbkdf2(defaultPwd);
    authPrefs.putString("pwd_hash", appState.wifi.webPassword);
    authPrefs.putString("default_pwd", defaultPwd);  // plaintext for display
    LOG_I("[Auth] Generated default password: %s", defaultPwd.c_str());
  }

  authPrefs.end();

  // Log the default password for serial console access
  authPrefs.begin("auth", true);  // read-only
  if (authPrefs.isKey("default_pwd")) {
    String defPwd = authPrefs.getString("default_pwd", "");
    if (defPwd.length() > 0) {
      LOG_I("[Auth] Default password: %s", defPwd.c_str());
    }
  }
  authPrefs.end();

  LOG_I("[Auth] Authentication system initialized");
}

// Generate a cryptographically random session ID (UUID format)
String generateSessionId() {
  uint8_t randomBytes[16];
  esp_fill_random(randomBytes, 16);

  // Format as UUID (8-4-4-4-12)
  char uuid[37];
  snprintf(
      uuid, sizeof(uuid),
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      randomBytes[0], randomBytes[1], randomBytes[2], randomBytes[3],
      randomBytes[4], randomBytes[5], randomBytes[6], randomBytes[7],
      randomBytes[8], randomBytes[9], randomBytes[10], randomBytes[11],
      randomBytes[12], randomBytes[13], randomBytes[14], randomBytes[15]);

  return String(uuid);
}

// Create a new session
bool createSession(String &sessionId) {
  uint64_t now = (uint64_t)esp_timer_get_time();

  // Try to find an empty slot
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (activeSessions[i].sessionId.length() == 0) {
      sessionId = generateSessionId();
      activeSessions[i].sessionId = sessionId;
      activeSessions[i].createdAt = now;
      activeSessions[i].lastSeen = now;
      LOG_D("[Auth] Session %s... created in slot %d",
            sessionId.substring(0, 8).c_str(), i);
      return true;
    }
  }

  // No empty slot, evict oldest session
  int oldestIndex = 0;
  uint64_t oldestTime = activeSessions[0].lastSeen;

  for (int i = 1; i < MAX_SESSIONS; i++) {
    if (activeSessions[i].lastSeen < oldestTime) {
      oldestTime = activeSessions[i].lastSeen;
      oldestIndex = i;
    }
  }

  LOG_D("[Auth] Evicting oldest session %s... from slot %d",
        activeSessions[oldestIndex].sessionId.substring(0, 8).c_str(),
        oldestIndex);

  sessionId = generateSessionId();
  activeSessions[oldestIndex].sessionId = sessionId;
  activeSessions[oldestIndex].createdAt = now;
  activeSessions[oldestIndex].lastSeen = now;

  LOG_D("[Auth] Session %s... created in slot %d (evicted)",
        sessionId.substring(0, 8).c_str(), oldestIndex);
  return true;
}

// Validate a session (check if exists and not expired)
bool validateSession(String sessionId) {
  if (sessionId.length() == 0) {
    LOG_W("[Auth] Empty session ID, validation failed");
    return false;
  }

  uint64_t now = (uint64_t)esp_timer_get_time();

  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (timingSafeCompare(activeSessions[i].sessionId, sessionId)) {
      // Check if expired
      if (now - activeSessions[i].lastSeen > SESSION_TIMEOUT_US) {
        LOG_D("[Auth] Session %s... expired",
              sessionId.substring(0, 8).c_str());
        activeSessions[i].sessionId = "";
        return false;
      }

      // Update last seen time
      activeSessions[i].lastSeen = now;
      return true;
    }
  }

  LOG_D("[Auth] Session %s... not found", sessionId.substring(0, 8).c_str());
  return false;
}

// Remove a session
void removeSession(String sessionId) {
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (timingSafeCompare(activeSessions[i].sessionId, sessionId)) {
      LOG_D("[Auth] Session %s... removed from slot %d",
            sessionId.substring(0, 8).c_str(), i);
      activeSessions[i].sessionId = "";
      activeSessions[i].createdAt = 0;
      activeSessions[i].lastSeen = 0;
      return;
    }
  }
}

// Invalidate all active sessions (force re-login after password change)
void clearAllSessions() {
  for (int i = 0; i < MAX_SESSIONS; i++) {
    activeSessions[i].sessionId = "";
    activeSessions[i].createdAt = 0;
    activeSessions[i].lastSeen = 0;
  }
  LOG_I("[Auth] All sessions cleared");
}

// Helper: Get session ID from Cookie header (HttpOnly — no JS access)
String getSessionFromCookie() {
  if (!server.hasHeader("Cookie")) {
    return "";
  }

  String cookie = server.header("Cookie");
  if (cookie.length() == 0) {
    return "";
  }

  LOG_D("[Auth] Cookie header received [len=%d]", cookie.length());

  // Parse cookie header for sessionId
  int start = cookie.indexOf("sessionId=");
  if (start == -1) {
    return "";
  }

  start += 10; // Length of "sessionId="
  int end = cookie.indexOf(";", start);

  if (end == -1) {
    end = cookie.length();
  }

  return cookie.substring(start, end);
}

// Middleware: Require authentication
bool requireAuth() {
  String sessionId = getSessionFromCookie();

  if (validateSession(sessionId)) {
#ifndef TEST_MODE
    // Per-IP rate limiting for all authenticated endpoints
    if (!rate_limit_check((uint32_t)server.client().remoteIP())) {
      server.sendHeader("Retry-After", "1");
      server_send(429, "application/json", "{\"error\":\"Rate limit exceeded\"}");
      return false;
    }
#endif
    return true;
  }

  // Not authenticated
  LOG_W("[Auth] Unauthorized access attempt to %s", server.uri().c_str());

  // Determine if this is a page request or API call
  // Check both URL path and Accept header for better detection
  String acceptHeader = server.header("Accept");
  bool isApiCall = server.uri().startsWith("/api/") ||
                   acceptHeader.indexOf("application/json") >= 0;
  bool isPageRequest = !isApiCall &&
                       (acceptHeader.indexOf("text/html") >= 0 ||
                        acceptHeader.length() == 0);

  // For page requests, send HTML redirect page
  if (isPageRequest) {
    String html = F("<!DOCTYPE html><html><head>"
                    "<meta charset='UTF-8'>"
                    "<meta http-equiv='refresh' content='0;url=/login'>"
                    "<title>Redirecting...</title></head>"
                    "<body><p>Redirecting to login...</p>"
                    "<script>window.location.href='/login';</script>"
                    "</body></html>");
    server.sendHeader("Location", "/login");
    server_send(302, "text/html", html);
    return false;
  }

  // For API calls, return 401 Unauthorized with JSON
  JsonDocument doc;
  doc["success"] = false;
  doc["error"] = "Unauthorized";
  doc["redirect"] = "/login";

  String response;
  serializeJson(doc, response);
  server_send(401, "application/json", response);

  return false;
}

// Get web password
String getWebPassword() { return appState.wifi.webPassword; }

// Get stored default password (for TFT display on boot screen)
String getDefaultPassword() {
  Preferences prefs;
  prefs.begin("auth", true);
  String pwd = prefs.getString("default_pwd", "");
  prefs.end();
  return pwd;
}

// Set web password and save to NVS (PBKDF2 hash, deletes legacy key)
void setWebPassword(String newPassword) {
  String hashed = hashPasswordPbkdf2(newPassword);
  appState.wifi.webPassword = hashed;

  authPrefs.begin("auth", false);
  authPrefs.putString("pwd_hash", hashed);
  if (authPrefs.isKey("web_pwd")) {
    authPrefs.remove("web_pwd");
  }
  authPrefs.end();
  _passwordNeedsMigration = false;

  LOG_I("[Auth] Password changed and saved to NVS (PBKDF2)");
}

// Check if using default password (generated or legacy AP password)
bool isDefaultPassword() {
  // Check against stored generated default password
  authPrefs.begin("auth", true);
  if (authPrefs.isKey("default_pwd")) {
    String defPwd = authPrefs.getString("default_pwd", "");
    authPrefs.end();
    if (defPwd.length() > 0) {
      return verifyPassword(defPwd, appState.wifi.webPassword);
    }
    return false;
  }
  authPrefs.end();
  // Legacy fallback: AP password was the old default
  return verifyPassword(appState.wifi.apPassword, appState.wifi.webPassword);
}

// Handler: Login
void handleLogin() {
  if (server.method() != HTTP_POST) {
    server_send(405, "application/json", "{\"error\":\"Method not allowed\"}");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    JsonDocument response;
    response["success"] = false;
    response["error"] = "Invalid JSON";

    String responseStr;
    serializeJson(response, responseStr);
    server_send(400, "application/json", responseStr);
    return;
  }

  String password = doc["password"].as<String>();

  // Auto-reset fail counter after cooldown period of no attempts
  uint64_t now = (uint64_t)esp_timer_get_time();
  if (_loginFailCount > 0 && (now - _lastFailTime) > LOGIN_COOLDOWN_US) {
    _loginFailCount = 0;
    _nextLoginAllowedMs = 0;
  }

#ifndef TEST_MODE
  // Non-blocking rate limit gate — reject immediately if in cooldown
  unsigned long nowMs = millis();
  if (_nextLoginAllowedMs > 0 && nowMs < _nextLoginAllowedMs) {
    unsigned long remainMs = _nextLoginAllowedMs - nowMs;
    unsigned long retryAfterSec = (remainMs + 999) / 1000; // round up

    LOG_W("[Auth] Rate limited — retry after %lus", retryAfterSec);

    server.sendHeader("Retry-After", String(retryAfterSec));
    JsonDocument response;
    response["success"] = false;
    response["error"] = "Too many attempts. Try again later.";
    response["retryAfter"] = retryAfterSec;

    String responseStr;
    serializeJson(response, responseStr);
    server_send(429, "application/json", responseStr);
    return;
  }
#endif // TEST_MODE

  // Validate password (supports PBKDF2 and legacy SHA256)
  if (!verifyPassword(password, appState.wifi.webPassword)) {
#ifndef TEST_MODE
    // Progressive rate limiting (non-blocking)
    _loginFailCount++;
    _lastFailTime = (uint64_t)esp_timer_get_time();
    unsigned long delayMs = getLoginDelay();
    _nextLoginAllowedMs = millis() + delayMs;

    LOG_W("[Auth] Login failed - incorrect password (attempt %d, next allowed in %lums)",
          _loginFailCount, delayMs);
#else
    LOG_W("[Auth] Login failed - incorrect password (TEST_MODE: no rate limiting)");
#endif

    JsonDocument response;
    response["success"] = false;
    response["error"] = "Incorrect password";

    String responseStr;
    serializeJson(response, responseStr);
    server_send(401, "application/json", responseStr);
    return;
  }

  // Create session
  String sessionId;
  if (!createSession(sessionId)) {
    JsonDocument response;
    response["success"] = false;
    response["error"] = "Failed to create session";

    String responseStr;
    serializeJson(response, responseStr);
    server_send(500, "application/json", responseStr);
    return;
  }

  // Reset rate limiter on success
  _loginFailCount = 0;
  _lastFailTime = 0;
  _nextLoginAllowedMs = 0;

  // Migrate legacy SHA256 or PBKDF2 v1 hash to PBKDF2 v2 on successful login
  if (_passwordNeedsMigration) {
    String newHash = hashPasswordPbkdf2(password);  // produces p2: format
    appState.wifi.webPassword = newHash;
    authPrefs.begin("auth", false);
    authPrefs.putString("pwd_hash", newHash);
    authPrefs.end();
    _passwordNeedsMigration = false;
    LOG_I("[Auth] Migrated password hash to PBKDF2 v2 (50k iterations)");
  }

  LOG_I("[Auth] Login successful");

  // Send response with cookie
  JsonDocument response;
  response["success"] = true;
  response["message"] = "Login successful";
  response["isDefaultPassword"] = isDefaultPassword();

  String responseStr;
  serializeJson(response, responseStr);

  // Set cookie with HttpOnly — JS uses /api/ws-token for WS auth instead
  String cookie =
      "sessionId=" + sessionId + "; Path=/; Max-Age=3600; SameSite=Strict; HttpOnly";
  server.sendHeader("Set-Cookie", cookie);
  LOG_D("[Auth] Set-Cookie for session %s...",
        sessionId.substring(0, 8).c_str());
  server_send(200, "application/json", responseStr);
}

// Handler: Logout
void handleLogout() {
  if (server.method() != HTTP_POST) {
    server_send(405, "application/json", "{\"error\":\"Method not allowed\"}");
    return;
  }

  String sessionId = getSessionFromCookie();

  if (sessionId.length() > 0) {
    removeSession(sessionId);
  }

  LOG_I("[Auth] Logout successful");

  // Clear cookie
  String cookie = "sessionId=; Path=/; Max-Age=0; SameSite=Strict; HttpOnly";
  server.sendHeader("Set-Cookie", cookie);

  JsonDocument response;
  response["success"] = true;
  response["message"] = "Logged out successfully";

  String responseStr;
  serializeJson(response, responseStr);
  server_send(200, "application/json", responseStr);
}

// Handler: Auth status
void handleAuthStatus() {
  String sessionId = getSessionFromCookie();

  JsonDocument response;

  if (validateSession(sessionId)) {
    response["success"] = true;
    response["authenticated"] = true;
    response["isDefaultPassword"] = isDefaultPassword();
  } else {
    response["success"] = true;
    response["authenticated"] = false;
  }

  String responseStr;
  serializeJson(response, responseStr);
  server_send(200, "application/json", responseStr);
}

// Handler: Password change
void handlePasswordChange() {
  if (!requireAuth())
    return;

  if (server.method() != HTTP_POST) {
    server_send(405, "application/json", "{\"error\":\"Method not allowed\"}");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    JsonDocument response;
    response["success"] = false;
    response["error"] = "Invalid JSON";

    String responseStr;
    serializeJson(response, responseStr);
    server_send(400, "application/json", responseStr);
    return;
  }

  // Require current password unless device is still on its generated default (first-boot exemption)
  if (!isDefaultPassword()) {
    if (!doc["currentPassword"].is<String>() || doc["currentPassword"].as<String>().length() == 0) {
      JsonDocument response;
      response["success"] = false;
      response["error"] = "Current password is required";
      String responseStr;
      serializeJson(response, responseStr);
      server_send(400, "application/json", responseStr);
      return;
    }

    String currentPassword = doc["currentPassword"].as<String>();
    if (!verifyPassword(currentPassword, appState.wifi.webPassword)) {
      LOG_W("[Auth] Password change failed: incorrect current password");
      JsonDocument response;
      response["success"] = false;
      response["error"] = "Current password is incorrect";
      String responseStr;
      serializeJson(response, responseStr);
      server_send(403, "application/json", responseStr);
      return;
    }
  }

  String newPassword = doc["newPassword"].as<String>();

  // Validate new password
  if (newPassword.length() < 8) {
    JsonDocument response;
    response["success"] = false;
    response["error"] = "Password must be at least 8 characters";

    String responseStr;
    serializeJson(response, responseStr);
    server_send(400, "application/json", responseStr);
    return;
  }

  // Change password
  setWebPassword(newPassword);

  // Invalidate all existing sessions and WebSocket connections (force re-login)
  clearAllSessions();
  ws_disconnect_all_clients();

  LOG_I("[Auth] Password changed successfully");

  JsonDocument response;
  response["success"] = true;
  response["message"] = "Password changed successfully";
  response["isDefaultPassword"] = isDefaultPassword();

  String responseStr;
  serializeJson(response, responseStr);
  server_send(200, "application/json", responseStr);
}
