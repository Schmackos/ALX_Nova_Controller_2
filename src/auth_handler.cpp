#include "auth_handler.h"
#include "app_state.h"
#include "config.h"
#include "debug_serial.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <mbedtls/md.h>

Session activeSessions[MAX_SESSIONS];
Preferences authPrefs;

// Rate limiting state
static int _loginFailCount = 0;
static uint64_t _lastFailTime = 0;
static const uint64_t LOGIN_COOLDOWN_US = 300000000ULL; // 5 minutes in microseconds

// Timing-safe string comparison — constant time regardless of where strings differ
bool timingSafeCompare(const String &a, const String &b) {
  size_t lenA = a.length();
  size_t lenB = b.length();
  size_t maxLen = (lenA > lenB) ? lenA : lenB;

  if (maxLen == 0) {
    return (lenA == 0 && lenB == 0);
  }

  volatile uint8_t result = (lenA != lenB) ? 1 : 0;
  const char *pA = a.c_str();
  const char *pB = b.c_str();

  for (size_t i = 0; i < maxLen; i++) {
    uint8_t byteA = (i < lenA) ? (uint8_t)pA[i] : 0;
    uint8_t byteB = (i < lenB) ? (uint8_t)pB[i] : 0;
    result |= byteA ^ byteB;
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
}

// Initialize authentication system
void initAuth() {
  // Clear all sessions
  for (int i = 0; i < MAX_SESSIONS; i++) {
    activeSessions[i].sessionId = "";
    activeSessions[i].createdAt = 0;
    activeSessions[i].lastSeen = 0;
  }

  // Load password hash from NVS (with migration from plaintext)
  authPrefs.begin("auth", false);

  if (authPrefs.isKey("pwd_hash")) {
    // New format: already hashed
    appState.webPassword = authPrefs.getString("pwd_hash", "");
    LOG_I("[Auth] Loaded password hash from NVS");
  } else if (authPrefs.isKey("web_pwd")) {
    // Legacy plaintext — migrate to hashed
    String plaintext = authPrefs.getString("web_pwd", "");
    if (plaintext.length() > 0) {
      String hashed = hashPassword(plaintext);
      authPrefs.putString("pwd_hash", hashed);
      authPrefs.remove("web_pwd");
      appState.webPassword = hashed;
      LOG_I("[Auth] Migrated plaintext password to hash");
    } else {
      appState.webPassword = hashPassword(appState.apPassword);
      LOG_I("[Auth] Using default password (AP password)");
    }
  } else {
    // No password saved, use hashed default (AP password)
    appState.webPassword = hashPassword(appState.apPassword);
    LOG_I("[Auth] Using default password (AP password)");
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

// Helper: Get session ID from Cookie or Header
String getSessionFromCookie() {
  // First try the custom header (more reliable for API calls)
  if (server.hasHeader("X-Session-ID")) {
    String headerId = server.header("X-Session-ID");
    if (headerId.length() > 0) {
      return headerId;
    }
  }

  // Fallback to Cookie header
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
    server.send(302, "text/html", html);
    return false;
  }

  // For API calls, return 401 Unauthorized with JSON
  JsonDocument doc;
  doc["success"] = false;
  doc["error"] = "Unauthorized";
  doc["redirect"] = "/login";

  String response;
  serializeJson(doc, response);
  server.send(401, "application/json", response);

  return false;
}

// Get web password
String getWebPassword() { return appState.webPassword; }

// Set web password and save to NVS (stores hash, deletes legacy key)
void setWebPassword(String newPassword) {
  String hashed = hashPassword(newPassword);
  appState.webPassword = hashed;

  authPrefs.begin("auth", false);
  authPrefs.putString("pwd_hash", hashed);
  if (authPrefs.isKey("web_pwd")) {
    authPrefs.remove("web_pwd");
  }
  authPrefs.end();

  LOG_I("[Auth] Password changed and saved to NVS");
}

// Check if using default password (hash of AP password)
bool isDefaultPassword() {
  return timingSafeCompare(appState.webPassword, hashPassword(appState.apPassword));
}

// Handler: Login
void handleLogin() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
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
    server.send(400, "application/json", responseStr);
    return;
  }

  String password = doc["password"].as<String>();

  // Auto-reset fail counter after cooldown period of no attempts
  uint64_t now = (uint64_t)esp_timer_get_time();
  if (_loginFailCount > 0 && (now - _lastFailTime) > LOGIN_COOLDOWN_US) {
    _loginFailCount = 0;
  }

  // Validate password — compare hash of input against stored hash
  if (!timingSafeCompare(hashPassword(password), appState.webPassword)) {
    // Progressive rate limiting
    _loginFailCount++;
    _lastFailTime = (uint64_t)esp_timer_get_time();
    unsigned long delayMs = getLoginDelay();
    delay(delayMs);

    LOG_W("[Auth] Login failed - incorrect password (attempt %d, delay %lums)",
          _loginFailCount, delayMs);

    JsonDocument response;
    response["success"] = false;
    response["error"] = "Incorrect password";

    String responseStr;
    serializeJson(response, responseStr);
    server.send(401, "application/json", responseStr);
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
    server.send(500, "application/json", responseStr);
    return;
  }

  // Reset rate limiter on success
  _loginFailCount = 0;
  _lastFailTime = 0;

  LOG_I("[Auth] Login successful");

  // Send response with cookie
  JsonDocument response;
  response["success"] = true;
  response["message"] = "Login successful";
  response["isDefaultPassword"] = isDefaultPassword();

  String responseStr;
  serializeJson(response, responseStr);

  // Set cookie (Path=/, Max-Age=3600, SameSite=Strict)
  // Removing HttpOnly to allow JS to read it for WebSocket auth
  String cookie =
      "sessionId=" + sessionId + "; Path=/; Max-Age=3600; SameSite=Strict";
  server.sendHeader("Set-Cookie", cookie);
  LOG_D("[Auth] Set-Cookie for session %s...",
        sessionId.substring(0, 8).c_str());
  server.send(200, "application/json", responseStr);
}

// Handler: Logout
void handleLogout() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
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
  server.send(200, "application/json", responseStr);
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
  server.send(200, "application/json", responseStr);
}

// Handler: Password change
void handlePasswordChange() {
  if (!requireAuth())
    return;

  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
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
    server.send(400, "application/json", responseStr);
    return;
  }

  String newPassword = doc["newPassword"].as<String>();

  // Validate new password
  if (newPassword.length() < 8) {
    JsonDocument response;
    response["success"] = false;
    response["error"] = "Password must be at least 8 characters";

    String responseStr;
    serializeJson(response, responseStr);
    server.send(400, "application/json", responseStr);
    return;
  }

  // Change password
  setWebPassword(newPassword);

  LOG_I("[Auth] Password changed successfully");

  JsonDocument response;
  response["success"] = true;
  response["message"] = "Password changed successfully";
  response["isDefaultPassword"] = isDefaultPassword();

  String responseStr;
  serializeJson(response, responseStr);
  server.send(200, "application/json", responseStr);
}
