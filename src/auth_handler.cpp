#include "auth_handler.h"
#include "app_state.h"
#include "config.h"
#include "debug_serial.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_random.h>

Session activeSessions[MAX_SESSIONS];
Preferences authPrefs;

// Initialize authentication system
void initAuth() {
  // Clear all sessions
  for (int i = 0; i < MAX_SESSIONS; i++) {
    activeSessions[i].sessionId = "";
    activeSessions[i].createdAt = 0;
    activeSessions[i].lastSeen = 0;
  }

  // Load password from NVS
  authPrefs.begin("auth", false);
  String savedPassword = "";
  if (authPrefs.isKey("web_pwd")) {
    savedPassword = authPrefs.getString("web_pwd", "");
  }
  authPrefs.end();

  if (savedPassword.length() == 0) {
    // No password saved, use default (AP password)
    appState.webPassword = appState.apPassword;
    LOG_I("[Auth] Using default password (AP password)");
  } else {
    appState.webPassword = savedPassword;
    LOG_I("[Auth] Loaded password from NVS");
  }

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
  unsigned long now = millis();

  // Try to find an empty slot
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (activeSessions[i].sessionId.length() == 0) {
      sessionId = generateSessionId();
      activeSessions[i].sessionId = sessionId;
      activeSessions[i].createdAt = now;
      activeSessions[i].lastSeen = now;
      LOG_D("[Auth] Session %s created in slot %d", sessionId.c_str(), i);
      return true;
    }
  }

  // No empty slot, evict oldest session
  int oldestIndex = 0;
  unsigned long oldestTime = activeSessions[0].lastSeen;

  for (int i = 1; i < MAX_SESSIONS; i++) {
    if (activeSessions[i].lastSeen < oldestTime) {
      oldestTime = activeSessions[i].lastSeen;
      oldestIndex = i;
    }
  }

  LOG_D("[Auth] Evicting oldest session %s from slot %d",
        activeSessions[oldestIndex].sessionId.c_str(), oldestIndex);

  sessionId = generateSessionId();
  activeSessions[oldestIndex].sessionId = sessionId;
  activeSessions[oldestIndex].createdAt = now;
  activeSessions[oldestIndex].lastSeen = now;

  LOG_D("[Auth] Session %s created in slot %d (evicted)",
        sessionId.c_str(), oldestIndex);
  return true;
}

// Validate a session (check if exists and not expired)
bool validateSession(String sessionId) {
  if (sessionId.length() == 0) {
    LOG_W("[Auth] Empty session ID, validation failed");
    return false;
  }

  unsigned long now = millis();

  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (activeSessions[i].sessionId == sessionId) {
      // Check if expired
      if (now - activeSessions[i].lastSeen > SESSION_TIMEOUT) {
        LOG_D("[Auth] Session %s expired", sessionId.c_str());
        activeSessions[i].sessionId = "";
        return false;
      }

      // Update last seen time
      activeSessions[i].lastSeen = now;
      return true;
    }
  }

  LOG_D("[Auth] Session %s not found", sessionId.c_str());
  return false;
}

// Remove a session
void removeSession(String sessionId) {
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (activeSessions[i].sessionId == sessionId) {
      LOG_D("[Auth] Session %s removed from slot %d", sessionId.c_str(), i);
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

  LOG_D("[Auth] Raw Cookie header: %s", cookie.c_str());

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

// Set web password and save to NVS
void setWebPassword(String newPassword) {
  appState.webPassword = newPassword;

  authPrefs.begin("auth", false);
  authPrefs.putString("web_pwd", newPassword);
  authPrefs.end();

  LOG_I("[Auth] Password changed and saved to NVS");
}

// Check if using default password (matches initAuth() which defaults to AP password)
bool isDefaultPassword() { return appState.webPassword == appState.apPassword; }

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

  // Validate password
  if (password != appState.webPassword) {
    // Rate limiting: delay on failed attempt
    delay(1000);

    LOG_W("[Auth] Login failed - incorrect password");

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

  LOG_I("[Auth] Login successful");

  // Send response with cookie
  JsonDocument response;
  response["success"] = true;
  response["message"] = "Login successful";
  response["isDefaultPassword"] = isDefaultPassword();

  String responseStr;
  serializeJson(response, responseStr);

  // Set cookie (Path=/, Max-Age=3600)
  // Removing HttpOnly to allow JS to read it for WebSocket auth
  String cookie = "sessionId=" + sessionId + "; Path=/; Max-Age=3600";
  server.sendHeader("Set-Cookie", cookie);
  LOG_D("[Auth] Set-Cookie: %s", cookie.c_str());
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
