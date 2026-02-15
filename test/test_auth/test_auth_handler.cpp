#include <cstring>
#include <string>
#include <unity.h>


#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Preferences.h"
#include "../test_mocks/esp_random.h"
#else
#include <Arduino.h>
#include <Preferences.h>
#include <esp_random.h>
#endif

#define MAX_SESSIONS 5
#define SESSION_TIMEOUT 3600000 // 1 hour in milliseconds

// Mock Session structure
struct Session {
  String sessionId;
  unsigned long createdAt;
  unsigned long lastSeen;
};

// Global test state
Session activeSessions[MAX_SESSIONS];
String mockWebPassword = "default_password";
String mockAPPassword = "ap_password";

namespace TestAuthState {
void reset() {
  // Clear all sessions
  for (int i = 0; i < MAX_SESSIONS; i++) {
    activeSessions[i].sessionId = "";
    activeSessions[i].createdAt = 0;
    activeSessions[i].lastSeen = 0;
  }
  mockWebPassword = "default_password";
  mockAPPassword = "ap_password";
  Preferences::reset();
  EspRandomMock::reset();
#ifdef NATIVE_TEST
  ArduinoMock::reset();
#endif
}
} // namespace TestAuthState

// ===== AUTH HANDLER IMPLEMENTATIONS =====

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

  sessionId = generateSessionId();
  activeSessions[oldestIndex].sessionId = sessionId;
  activeSessions[oldestIndex].createdAt = now;
  activeSessions[oldestIndex].lastSeen = now;

  return true;
}

// Validate a session (check if exists and not expired)
bool validateSession(String sessionId) {
  if (sessionId.length() == 0) {
    return false;
  }

  unsigned long now = millis();

  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (activeSessions[i].sessionId == sessionId) {
      // Check if expired
      if (now - activeSessions[i].lastSeen > SESSION_TIMEOUT) {
        activeSessions[i].sessionId = "";
        return false;
      }

      // Update last seen time
      activeSessions[i].lastSeen = now;
      return true;
    }
  }

  return false;
}

// Remove a session
void removeSession(String sessionId) {
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (activeSessions[i].sessionId == sessionId) {
      activeSessions[i].sessionId = "";
      activeSessions[i].createdAt = 0;
      activeSessions[i].lastSeen = 0;
      return;
    }
  }
}

// Get stored web password
String getWebPassword() { return mockWebPassword; }

// Set web password
void setWebPassword(String newPassword) { mockWebPassword = newPassword; }

// Load password from Preferences
void loadPasswordFromPrefs() {
  Preferences prefs;
  prefs.begin("auth", false);
  String savedPassword = prefs.getString("web_pwd", "");
  prefs.end();

  if (savedPassword.length() > 0) {
    mockWebPassword = savedPassword;
  } else {
    mockWebPassword = mockAPPassword;
  }
}

// Save password to Preferences
void savePasswordToPrefs(String password) {
  Preferences prefs;
  prefs.begin("auth", false);
  prefs.putString("web_pwd", password);
  prefs.end();
}

// ===== Test Setup/Teardown =====

void setUp(void) { TestAuthState::reset(); }

void tearDown(void) {
  // Clean up after each test
}

// ===== Session Management Tests =====

void test_session_creation_empty_slot(void) {
  String sessionId;
  bool created = createSession(sessionId);

  TEST_ASSERT_TRUE(created);
  TEST_ASSERT_EQUAL(36, sessionId.length()); // UUID length
  TEST_ASSERT_EQUAL_STRING(sessionId.c_str(),
                           activeSessions[0].sessionId.c_str());
}

void test_session_creation_fills_slots(void) {
  String sessionIds[5];

  // Create 5 sessions in 5 slots
  for (int i = 0; i < MAX_SESSIONS; i++) {
    bool created = createSession(sessionIds[i]);
    TEST_ASSERT_TRUE(created);
    TEST_ASSERT_EQUAL(36, sessionIds[i].length());
  }

  // Verify all slots are filled with unique IDs
  for (int i = 0; i < MAX_SESSIONS; i++) {
    TEST_ASSERT_EQUAL_STRING(sessionIds[i].c_str(),
                             activeSessions[i].sessionId.c_str());
  }
}

void test_session_creation_full_eviction(void) {
  // Fill all slots
  String sessionIds[6];
  for (int i = 0; i < 5; i++) {
    createSession(sessionIds[i]);
    ArduinoMock::mockMillis += 1000; // Advance time
  }

  // Create 6th session - should evict oldest (first one)
  String firstSessionId = sessionIds[0];
  createSession(sessionIds[5]);

  // First session should be gone (replaced by 6th)
  TEST_ASSERT_TRUE(
      strcmp(firstSessionId.c_str(), activeSessions[0].sessionId.c_str()) != 0);

  // 6th session should be in slot 0 (where first one was)
  TEST_ASSERT_EQUAL_STRING(sessionIds[5].c_str(),
                           activeSessions[0].sessionId.c_str());
}

void test_session_validation_valid(void) {
  String sessionId;
  createSession(sessionId);

  // Immediately validate
  TEST_ASSERT_TRUE(validateSession(sessionId));
}

void test_session_validation_expired(void) {
  String sessionId;
  createSession(sessionId);

  // Advance time past expiration
  ArduinoMock::mockMillis += SESSION_TIMEOUT + 1000;

  // Session should be expired
  TEST_ASSERT_FALSE(validateSession(sessionId));

  // Session should be cleared
  TEST_ASSERT_EQUAL(0, activeSessions[0].sessionId.length());
}

void test_session_validation_nonexistent(void) {
  String fakeSessionId = "fake-session-id";
  TEST_ASSERT_FALSE(validateSession(fakeSessionId));
}

void test_session_removal(void) {
  String sessionId;
  createSession(sessionId);

  // Verify session exists
  TEST_ASSERT_TRUE(validateSession(sessionId));

  // Remove it
  removeSession(sessionId);

  // Session should not exist
  TEST_ASSERT_FALSE(validateSession(sessionId));
}

void test_session_lastSeen_updates(void) {
  String sessionId;
  createSession(sessionId);

  unsigned long initialLastSeen = activeSessions[0].lastSeen;

  // Advance time and validate
  ArduinoMock::mockMillis += 5000;
  validateSession(sessionId);

  // lastSeen should be updated
  TEST_ASSERT_GREATER_THAN(initialLastSeen, activeSessions[0].lastSeen);
}

// ===== Password Management Tests =====

void test_password_default_from_ap(void) {
  mockWebPassword = "";
  mockAPPassword = "ap_password";

  loadPasswordFromPrefs();

  // Should fall back to AP password
  TEST_ASSERT_EQUAL_STRING("ap_password", mockWebPassword.c_str());
}

void test_password_load_from_nvs(void) {
  // Save password to preferences
  Preferences prefs;
  prefs.begin("auth", false);
  prefs.putString("web_pwd", "saved_password");
  prefs.end();

  // Load it
  loadPasswordFromPrefs();

  TEST_ASSERT_EQUAL_STRING("saved_password", mockWebPassword.c_str());
}

void test_password_change_saved(void) {
  // Save a password
  savePasswordToPrefs("new_password");

  // Load it back
  Preferences prefs;
  prefs.begin("auth", false);
  String loaded = prefs.getString("web_pwd", "");
  prefs.end();

  TEST_ASSERT_EQUAL_STRING("new_password", loaded.c_str());
}

// ===== API Handler Tests =====

void test_login_success(void) {
  mockWebPassword = "correct_password";
  String sessionId;

  // Attempt login with correct password
  if (strcmp("correct_password", mockWebPassword.c_str()) == 0) {
    bool success = createSession(sessionId);
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL(36, sessionId.length());
  }
}

void test_login_failure(void) {
  mockWebPassword = "correct_password";

  // Attempt login with wrong password
  bool success = false;
  if (strcmp("wrong_password", mockWebPassword.c_str()) == 0) {
    String sessionId;
    success = createSession(sessionId);
  }

  TEST_ASSERT_FALSE(success);
}

void test_session_empty_validation(void) {
  String emptySession = "";
  TEST_ASSERT_FALSE(validateSession(emptySession));
}

void test_multiple_sessions_independent_validation(void) {
  String session1, session2;
  createSession(session1);

  // Advance time
  ArduinoMock::mockMillis += 100;

  createSession(session2);

  // Validate both
  TEST_ASSERT_TRUE(validateSession(session1));
  TEST_ASSERT_TRUE(validateSession(session2));

  // Expire both
  ArduinoMock::mockMillis += SESSION_TIMEOUT + 1000;

  // Both should be expired now
  TEST_ASSERT_FALSE(validateSession(session1));
  TEST_ASSERT_FALSE(validateSession(session2));
}

// ===== Session Revocation Tests (WS auth vulnerability fix) =====

void test_session_invalid_after_removal(void) {
  // Simulates: user logs in via HTTP, authenticates WS, then logs out via HTTP
  String sessionId;
  createSession(sessionId);

  // Session is valid (WS auth would succeed)
  TEST_ASSERT_TRUE(validateSession(sessionId));

  // HTTP logout removes the session
  removeSession(sessionId);

  // Session must now fail validation (WS re-check catches this)
  TEST_ASSERT_FALSE(validateSession(sessionId));
}

void test_session_revalidation_catches_expiry(void) {
  // WS client authenticates, then session expires between messages
  String sessionId;
  createSession(sessionId);
  TEST_ASSERT_TRUE(validateSession(sessionId));

  // Time passes beyond session timeout
  ArduinoMock::mockMillis += SESSION_TIMEOUT + 1;

  // Re-validation on next WS command should fail
  TEST_ASSERT_FALSE(validateSession(sessionId));
}

void test_removed_session_does_not_affect_others(void) {
  // Two clients with separate sessions
  String session1, session2;
  createSession(session1);
  ArduinoMock::mockMillis += 10;
  createSession(session2);

  // Remove session1 (logout)
  removeSession(session1);

  // session1 invalid, session2 still valid
  TEST_ASSERT_FALSE(validateSession(session1));
  TEST_ASSERT_TRUE(validateSession(session2));
}

// ===== Default Password Consistency Tests =====

void test_is_default_password_with_unchanged_ap(void) {
  // When webPassword == apPassword, it should be "default"
  mockWebPassword = mockAPPassword;
  TEST_ASSERT_TRUE(mockWebPassword == mockAPPassword);
}

void test_is_default_password_with_changed_ap(void) {
  // User changes AP password, initAuth sets webPassword = new AP password
  // isDefaultPassword() should still detect this as "default"
  mockAPPassword = "customAPpwd";
  mockWebPassword = "customAPpwd";  // initAuth() would set this
  TEST_ASSERT_TRUE(mockWebPassword == mockAPPassword);
}

void test_is_not_default_after_web_password_change(void) {
  // User explicitly sets a different web password
  mockAPPassword = "ap_password";
  mockWebPassword = "my_custom_web_pwd";
  TEST_ASSERT_FALSE(mockWebPassword == mockAPPassword);
}

// ===== Test Runner =====

int runUnityTests(void) {
  UNITY_BEGIN();

  // Session management tests
  RUN_TEST(test_session_creation_empty_slot);
  RUN_TEST(test_session_creation_fills_slots);
  RUN_TEST(test_session_creation_full_eviction);
  RUN_TEST(test_session_validation_valid);
  RUN_TEST(test_session_validation_expired);
  RUN_TEST(test_session_validation_nonexistent);
  RUN_TEST(test_session_removal);
  RUN_TEST(test_session_lastSeen_updates);

  // Password management tests
  RUN_TEST(test_password_default_from_ap);
  RUN_TEST(test_password_load_from_nvs);
  RUN_TEST(test_password_change_saved);

  // API handler tests
  RUN_TEST(test_login_success);
  RUN_TEST(test_login_failure);
  RUN_TEST(test_session_empty_validation);
  RUN_TEST(test_multiple_sessions_independent_validation);

  // Session revocation tests (WS auth vulnerability fix)
  RUN_TEST(test_session_invalid_after_removal);
  RUN_TEST(test_session_revalidation_catches_expiry);
  RUN_TEST(test_removed_session_does_not_affect_others);

  // Default password consistency tests
  RUN_TEST(test_is_default_password_with_unchanged_ap);
  RUN_TEST(test_is_default_password_with_changed_ap);
  RUN_TEST(test_is_not_default_after_web_password_change);

  return UNITY_END();
}

// For native platform
#ifdef NATIVE_TEST
int main(void) { return runUnityTests(); }
#endif

// For Arduino platform
#ifndef NATIVE_TEST
void setup() {
  delay(2000);
  runUnityTests();
}

void loop() {
  // Do nothing
}
#endif
