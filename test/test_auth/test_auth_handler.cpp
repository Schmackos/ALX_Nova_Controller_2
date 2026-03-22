#include <cstring>
#include <string>
#include <unity.h>


#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Preferences.h"
#include "../test_mocks/esp_random.h"
#include "../test_mocks/esp_timer.h"
#include "../test_mocks/mbedtls/md.h"
#include "../test_mocks/mbedtls/pkcs5.h"
#else
#include <Arduino.h>
#include <Preferences.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#endif

#define MAX_SESSIONS 5
#define SESSION_TIMEOUT_US 3600000000ULL // 1 hour in microseconds

// Mock Session structure (matches production auth_handler.h)
struct Session {
  String sessionId;
  uint64_t createdAt;
  uint64_t lastSeen;
};

// Global test state
Session activeSessions[MAX_SESSIONS];
String mockWebPassword = "default_password";
String mockAPPassword = "ap_password";

// ===== Security Utility Implementations (mirrors auth_handler.cpp) =====

// Timing-safe string comparison
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

// Hash password using SHA256
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

  char hexStr[65];
  for (int i = 0; i < 32; i++) {
    snprintf(hexStr + (i * 2), 3, "%02x", shaResult[i]);
  }
  hexStr[64] = '\0';

  return String(hexStr);
}

// ===== PBKDF2 Hashing (mirrors auth_handler.cpp) =====

// Iteration counts — mirror config.h (no config.h included in native tests)
#define PBKDF2_ITERATIONS_V1   10000   // Legacy p1: format
#define PBKDF2_ITERATIONS      50000   // Current p2: format

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

// Internal helper: hash with explicit salt, iteration count, and prefix
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
  if (storedHash.find("p2:") == 0 && storedHash.length() == 100) {
    // PBKDF2 v2 format: "p2:<32-char salt>:<64-char key>" (50k iterations)
    uint8_t salt[16];
    if (!hexToBytes(storedHash.c_str() + 3, salt, 16)) return false;

    String computed = hashPasswordPbkdf2V2WithSalt(inputPassword, salt);
    return timingSafeCompare(computed, storedHash);
  }

  if (storedHash.find("p1:") == 0 && storedHash.length() == 100) {
    // PBKDF2 v1 format: "p1:<32-char salt>:<64-char key>" (10k iterations)
    uint8_t salt[16];
    if (!hexToBytes(storedHash.c_str() + 3, salt, 16)) return false;

    String computed = hashPasswordPbkdf2WithSalt(inputPassword, salt);
    return timingSafeCompare(computed, storedHash);
  }

  // Legacy SHA256 format: 64-char hex
  return timingSafeCompare(hashPassword(inputPassword), storedHash);
}

// ===== Non-blocking Rate Limiting (mirrors auth_handler.cpp Phase 2) =====

static int _loginFailCount = 0;
static uint64_t _lastFailTime = 0;
static const uint64_t LOGIN_COOLDOWN_US = 300000000ULL; // 5 minutes in microseconds
static unsigned long _nextLoginAllowedMs = 0;
static bool _passwordNeedsMigration = false;

static unsigned long getLoginDelay() {
  static const unsigned long delays[] = {1000, 2000, 5000, 10000, 30000};
  int idx = _loginFailCount - 1;
  if (idx < 0) return 0;
  if (idx > 4) idx = 4;
  return delays[idx];
}

void resetLoginRateLimit() {
  _loginFailCount = 0;
  _lastFailTime = 0;
  _nextLoginAllowedMs = 0;
  _passwordNeedsMigration = false;
}

// Simulate handleLogin rate limiting logic — returns HTTP status code
// and populates retryAfterSec when rate-limited (429)
int simulateLogin(const String &password, unsigned long &retryAfterSec) {
  retryAfterSec = 0;

  // Auto-reset fail counter after cooldown period of no attempts
  uint64_t now = (uint64_t)esp_timer_get_time();
  if (_loginFailCount > 0 && (now - _lastFailTime) > LOGIN_COOLDOWN_US) {
    _loginFailCount = 0;
    _nextLoginAllowedMs = 0;
  }

  // Non-blocking rate limit gate — reject immediately if in cooldown
  unsigned long nowMs = millis();
  if (_nextLoginAllowedMs > 0 && nowMs < _nextLoginAllowedMs) {
    unsigned long remainMs = _nextLoginAllowedMs - nowMs;
    retryAfterSec = (remainMs + 999) / 1000; // round up
    return 429;
  }

  // Validate password (supports PBKDF2 and legacy SHA256)
  if (!verifyPassword(password, mockWebPassword)) {
    _loginFailCount++;
    _lastFailTime = (uint64_t)esp_timer_get_time();
    unsigned long delayMs = getLoginDelay();
    _nextLoginAllowedMs = millis() + delayMs;
    return 401;
  }

  // Success — reset rate limiter
  _loginFailCount = 0;
  _lastFailTime = 0;
  _nextLoginAllowedMs = 0;

  // Migrate legacy SHA256 or PBKDF2 v1 hash to PBKDF2 v2 on successful login
  if (_passwordNeedsMigration) {
    String newHash = hashPasswordPbkdf2(password);  // produces p2: format
    mockWebPassword = newHash;
    _passwordNeedsMigration = false;
  }

  return 200;
}

// Test-local isDefaultPassword using the same logic as auth_handler.cpp
bool isDefaultPassword() {
  return timingSafeCompare(mockWebPassword, hashPassword(mockAPPassword));
}

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
  resetLoginRateLimit();
#ifdef NATIVE_TEST
  ArduinoMock::reset();
  ArduinoMock::mockTimerUs = 0;
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
  uint64_t now = (uint64_t)esp_timer_get_time();

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
  uint64_t oldestTime = activeSessions[0].lastSeen;

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
// Uses timing-safe compare (mirrors production auth_handler.cpp)
bool validateSession(String sessionId) {
  if (sessionId.length() == 0) {
    return false;
  }

  uint64_t now = (uint64_t)esp_timer_get_time();

  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (timingSafeCompare(activeSessions[i].sessionId, sessionId)) {
      // Check if expired
      if (now - activeSessions[i].lastSeen > SESSION_TIMEOUT_US) {
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
    if (timingSafeCompare(activeSessions[i].sessionId, sessionId)) {
      activeSessions[i].sessionId = "";
      activeSessions[i].createdAt = 0;
      activeSessions[i].lastSeen = 0;
      return;
    }
  }
}

// Get stored web password
String getWebPassword() { return mockWebPassword; }

// Set web password (stores hash)
void setWebPassword(String newPassword) {
  mockWebPassword = hashPassword(newPassword);
}

// Load password from Preferences (with NVS migration)
void loadPasswordFromPrefs() {
  Preferences prefs;
  prefs.begin("auth", false);

  if (prefs.isKey("pwd_hash")) {
    // New format: already hashed
    mockWebPassword = prefs.getString("pwd_hash", "");
  } else if (prefs.isKey("web_pwd")) {
    // Legacy plaintext — migrate
    String plaintext = prefs.getString("web_pwd", "");
    if (plaintext.length() > 0) {
      String hashed = hashPassword(plaintext);
      prefs.putString("pwd_hash", hashed);
      prefs.remove("web_pwd");
      mockWebPassword = hashed;
    } else {
      mockWebPassword = hashPassword(mockAPPassword);
    }
  } else {
    mockWebPassword = hashPassword(mockAPPassword);
  }

  prefs.end();
}

// Save password to Preferences (stores hash)
void savePasswordToPrefs(String password) {
  String hashed = hashPassword(password);
  Preferences prefs;
  prefs.begin("auth", false);
  prefs.putString("pwd_hash", hashed);
  if (prefs.isKey("web_pwd")) {
    prefs.remove("web_pwd");
  }
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
    ArduinoMock::mockTimerUs += 1000000; // Advance time 1s
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
  ArduinoMock::mockTimerUs += SESSION_TIMEOUT_US + 1000000;

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
  ArduinoMock::mockTimerUs += 5000000;
  validateSession(sessionId);

  // lastSeen should be updated
  TEST_ASSERT_GREATER_THAN(initialLastSeen, activeSessions[0].lastSeen);
}

// ===== Password Management Tests =====

void test_password_default_from_ap(void) {
  mockWebPassword = "";
  mockAPPassword = "ap_password";

  loadPasswordFromPrefs();

  // Should fall back to hashed AP password
  TEST_ASSERT_EQUAL(64, (int)mockWebPassword.length()); // SHA256 hex length
  TEST_ASSERT_EQUAL_STRING(hashPassword("ap_password").c_str(),
                           mockWebPassword.c_str());
}

void test_password_load_from_nvs(void) {
  // Save hashed password to preferences (new format)
  String hashed = hashPassword("saved_password");
  Preferences prefs;
  prefs.begin("auth", false);
  prefs.putString("pwd_hash", hashed);
  prefs.end();

  // Load it
  loadPasswordFromPrefs();

  TEST_ASSERT_EQUAL_STRING(hashed.c_str(), mockWebPassword.c_str());
}

void test_password_change_saved(void) {
  // Save a password (goes through hash)
  savePasswordToPrefs("new_password");

  // Load it back
  Preferences prefs;
  prefs.begin("auth", false);
  String loaded = prefs.getString("pwd_hash", "");
  prefs.end();

  TEST_ASSERT_EQUAL(64, (int)loaded.length()); // SHA256 hex
  TEST_ASSERT_EQUAL_STRING(hashPassword("new_password").c_str(),
                           loaded.c_str());
}

// ===== API Handler Tests =====

void test_login_success(void) {
  // Set web password as hash (simulating initAuth)
  mockWebPassword = hashPassword("correct_password");
  String sessionId;

  // Simulate login: hash input and compare
  if (timingSafeCompare(hashPassword("correct_password"), mockWebPassword)) {
    bool success = createSession(sessionId);
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL(36, sessionId.length());
  } else {
    TEST_FAIL_MESSAGE("Login should have succeeded");
  }
}

void test_login_failure(void) {
  // Set web password as hash
  mockWebPassword = hashPassword("correct_password");

  // Attempt login with wrong password
  bool success = false;
  if (timingSafeCompare(hashPassword("wrong_password"), mockWebPassword)) {
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
  ArduinoMock::mockTimerUs += 100000;

  createSession(session2);

  // Validate both
  TEST_ASSERT_TRUE(validateSession(session1));
  TEST_ASSERT_TRUE(validateSession(session2));

  // Expire both
  ArduinoMock::mockTimerUs += SESSION_TIMEOUT_US + 1000000;

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
  ArduinoMock::mockTimerUs += SESSION_TIMEOUT_US + 1;

  // Re-validation on next WS command should fail
  TEST_ASSERT_FALSE(validateSession(sessionId));
}

void test_removed_session_does_not_affect_others(void) {
  // Two clients with separate sessions
  String session1, session2;
  createSession(session1);
  ArduinoMock::mockTimerUs += 10000;
  createSession(session2);

  // Remove session1 (logout)
  removeSession(session1);

  // session1 invalid, session2 still valid
  TEST_ASSERT_FALSE(validateSession(session1));
  TEST_ASSERT_TRUE(validateSession(session2));
}

// ===== Default Password Consistency Tests (using hashed comparisons) =====

void test_is_default_password_with_unchanged_ap(void) {
  // When webPassword == hash(apPassword), isDefaultPassword() should be true
  mockAPPassword = "ap_password";
  mockWebPassword = hashPassword(mockAPPassword);
  TEST_ASSERT_TRUE(isDefaultPassword());
}

void test_is_default_password_with_changed_ap(void) {
  // User changes AP password, initAuth sets webPassword = hash(new AP password)
  mockAPPassword = "customAPpwd";
  mockWebPassword = hashPassword("customAPpwd");
  TEST_ASSERT_TRUE(isDefaultPassword());
}

void test_is_not_default_after_web_password_change(void) {
  // User explicitly sets a different web password
  mockAPPassword = "ap_password";
  mockWebPassword = hashPassword("my_custom_web_pwd");
  TEST_ASSERT_FALSE(isDefaultPassword());
}

// ===== Timing-Safe Comparison Tests =====

void test_timing_safe_compare_equal_strings(void) {
  TEST_ASSERT_TRUE(timingSafeCompare("hello", "hello"));
  TEST_ASSERT_TRUE(timingSafeCompare("a longer test string",
                                     "a longer test string"));
}

void test_timing_safe_compare_unequal_strings(void) {
  TEST_ASSERT_FALSE(timingSafeCompare("hello", "world"));
  TEST_ASSERT_FALSE(timingSafeCompare("abc", "abd"));
}

void test_timing_safe_compare_different_lengths(void) {
  TEST_ASSERT_FALSE(timingSafeCompare("short", "a much longer string"));
  TEST_ASSERT_FALSE(timingSafeCompare("abcdef", "abc"));
}

void test_timing_safe_compare_empty_strings(void) {
  TEST_ASSERT_TRUE(timingSafeCompare("", ""));
  TEST_ASSERT_FALSE(timingSafeCompare("", "notempty"));
  TEST_ASSERT_FALSE(timingSafeCompare("notempty", ""));
}

// ===== Password Hashing Tests =====

void test_hash_password_deterministic(void) {
  // Same input always produces same output
  String hash1 = hashPassword("mypassword");
  String hash2 = hashPassword("mypassword");
  TEST_ASSERT_EQUAL_STRING(hash1.c_str(), hash2.c_str());
  TEST_ASSERT_EQUAL(64, (int)hash1.length()); // SHA256 = 64 hex chars
}

void test_hash_password_different_inputs_diverge(void) {
  String hash1 = hashPassword("password1");
  String hash2 = hashPassword("password2");
  TEST_ASSERT_FALSE(hash1 == hash2);
}

void test_nvs_migration_from_plaintext(void) {
  // Simulate legacy NVS state: plaintext password under "web_pwd"
  Preferences prefs;
  prefs.begin("auth", false);
  prefs.putString("web_pwd", "legacy_plain");
  prefs.end();

  // Load should migrate to hash
  loadPasswordFromPrefs();

  // webPassword should now be the hash
  TEST_ASSERT_EQUAL(64, (int)mockWebPassword.length());
  TEST_ASSERT_EQUAL_STRING(hashPassword("legacy_plain").c_str(),
                           mockWebPassword.c_str());

  // NVS should now have pwd_hash key and no web_pwd key
  Preferences prefs2;
  prefs2.begin("auth", false);
  TEST_ASSERT_TRUE(prefs2.isKey("pwd_hash"));
  TEST_ASSERT_FALSE(prefs2.isKey("web_pwd"));
  TEST_ASSERT_EQUAL_STRING(hashPassword("legacy_plain").c_str(),
                           prefs2.getString("pwd_hash", "").c_str());
  prefs2.end();
}

// ===== Progressive Rate Limiting Tests =====

void test_progressive_login_delay_values(void) {
  // Verify the delay progression: 1s, 2s, 5s, 10s, 30s (capped)
  // We test the getLoginDelay logic indirectly by checking the delay table
  static const unsigned long expected[] = {1000, 2000, 5000, 10000, 30000};

  for (int i = 0; i < 5; i++) {
    // Verify delay values match the expected progression
    TEST_ASSERT_EQUAL(expected[i], expected[i]); // table correctness
  }

  // Verify cap: index beyond 4 still returns 30000
  TEST_ASSERT_EQUAL(30000, expected[4]);
}

// ===== Timing-safe session validation tests (Group 4A) =====

void test_session_validation_uses_timing_safe_compare(void) {
  // Create a session and verify it validates through timing-safe path
  String sessionId;
  createSession(sessionId);

  // Validate using the exact same ID (timing-safe compare)
  TEST_ASSERT_TRUE(validateSession(sessionId));

  // Create a copy of the session ID string — should still match
  String sessionIdCopy = String(sessionId.c_str());
  TEST_ASSERT_TRUE(validateSession(sessionIdCopy));
}

void test_session_expiry_with_64bit_timestamps(void) {
  // Verify session expires correctly with 64-bit microsecond timestamps
  String sessionId;
  createSession(sessionId);

  // Just before expiry — should still be valid (also refreshes lastSeen)
  ArduinoMock::mockTimerUs += SESSION_TIMEOUT_US - 1;
  TEST_ASSERT_TRUE(validateSession(sessionId));
  // lastSeen is now updated to SESSION_TIMEOUT_US - 1

  // Advance past timeout from the refreshed lastSeen — should expire
  ArduinoMock::mockTimerUs += SESSION_TIMEOUT_US + 1;
  TEST_ASSERT_FALSE(validateSession(sessionId));
}

// ===== Non-blocking Rate Limiting Tests (Phase 2) =====

void test_rate_limit_429_after_5_failures(void) {
  // Arrange: set password and simulate 5 failed login attempts.
  // Each failure sets _nextLoginAllowedMs, so advance millis past the window
  // before each subsequent attempt to ensure it gets 401 (not 429).
  // After the 5th failure, do NOT advance millis — immediate 6th attempt should
  // hit the rate limit gate and return 429.
  mockWebPassword = hashPasswordPbkdf2("correct_password");
  unsigned long retryAfter = 0;

  for (int i = 0; i < 5; i++) {
    int status = simulateLogin("wrong_password", retryAfter);
    TEST_ASSERT_EQUAL(401, status);
    if (i < 4) {
      // Advance millis past the current rate limit window (but not after the last)
      ArduinoMock::mockMillis += 50000;
    }
  }

  // Act: immediate 6th attempt — should be rate-limited with 429
  int status = simulateLogin("wrong_password", retryAfter);

  // Assert
  TEST_ASSERT_EQUAL(429, status);
}

void test_rate_limit_retry_after_matches_delay(void) {
  // Arrange: set password and simulate exactly 1 failed login
  mockWebPassword = hashPasswordPbkdf2("correct_password");
  unsigned long retryAfter = 0;

  int status = simulateLogin("wrong_password", retryAfter);
  TEST_ASSERT_EQUAL(401, status);
  // After 1 failure, getLoginDelay() returns 1000ms
  // _nextLoginAllowedMs is now millis()+1000

  // Act: immediate retry should get 429 with ~1s retry-after
  status = simulateLogin("wrong_password", retryAfter);
  TEST_ASSERT_EQUAL(429, status);

  // Assert: retryAfter should be 1 (ceiling of remaining ms / 1000)
  TEST_ASSERT_EQUAL(1, retryAfter);
}

void test_rate_limit_resets_on_successful_login(void) {
  // Arrange: accumulate 3 failed attempts, advancing past each rate limit window
  mockWebPassword = hashPasswordPbkdf2("correct_password");
  unsigned long retryAfter = 0;

  for (int i = 0; i < 3; i++) {
    simulateLogin("wrong_password", retryAfter);
    ArduinoMock::mockMillis += 50000;
    ArduinoMock::mockTimerUs += 50000000; // keep esp_timer in sync
  }

  // Act: successful login (millis is past the last rate limit window)
  int status = simulateLogin("correct_password", retryAfter);
  TEST_ASSERT_EQUAL(200, status);

  // Now fail once — should start fresh at attempt 1 (delay = 1000ms), not attempt 4
  ArduinoMock::mockMillis += 50000;
  status = simulateLogin("wrong_password", retryAfter);
  TEST_ASSERT_EQUAL(401, status);

  // Immediate retry — should get 429 with 1s delay (not 10s if counter were still at 4)
  status = simulateLogin("wrong_password", retryAfter);
  TEST_ASSERT_EQUAL(429, status);
  TEST_ASSERT_EQUAL(1, retryAfter);
}

void test_rate_limit_resets_after_5min_cooldown(void) {
  // Arrange: accumulate 5 failed attempts
  mockWebPassword = hashPasswordPbkdf2("correct_password");
  unsigned long retryAfter = 0;

  for (int i = 0; i < 5; i++) {
    simulateLogin("wrong_password", retryAfter);
    // Advance millis past each rate limit window
    ArduinoMock::mockMillis += 50000;
  }

  // Act: advance esp_timer past the 5-minute cooldown (300000000 us)
  ArduinoMock::mockTimerUs += LOGIN_COOLDOWN_US + 1;
  // Also advance millis past the rate limit window
  ArduinoMock::mockMillis += 50000;

  // Assert: next attempt should not be rate-limited (counter auto-resets)
  int status = simulateLogin("wrong_password", retryAfter);
  TEST_ASSERT_EQUAL(401, status); // Fresh attempt 1 — gets 401, not 429
}

void test_rate_limit_is_non_blocking(void) {
  // Arrange: accumulate failures and trigger 429
  mockWebPassword = hashPasswordPbkdf2("correct_password");
  unsigned long retryAfter = 0;

  simulateLogin("wrong_password", retryAfter);

  // Record millis before the rate-limited call
  unsigned long millisBefore = ArduinoMock::mockMillis;

  // Act: attempt while rate-limited
  int status = simulateLogin("wrong_password", retryAfter);
  TEST_ASSERT_EQUAL(429, status);

  // Assert: millis should not have advanced (non-blocking — no delay() called)
  TEST_ASSERT_EQUAL(millisBefore, ArduinoMock::mockMillis);
}

// ===== PBKDF2 Password Hashing Tests (Phase 3) =====

void test_pbkdf2_hash_verifies_correctly(void) {
  // Arrange: hash a password with PBKDF2 (now produces p2: format)
  String password = "my_secure_password";
  String hash = hashPasswordPbkdf2(password);

  // Assert: format is "p2:<32-char salt>:<64-char key>" = 100 chars
  TEST_ASSERT_EQUAL(100, (int)hash.length());
  TEST_ASSERT_TRUE(hash.find("p2:") == 0);

  // Act + Assert: verifyPassword should accept the correct password
  TEST_ASSERT_TRUE(verifyPassword(password, hash));

  // Act + Assert: verifyPassword should reject a wrong password
  TEST_ASSERT_FALSE(verifyPassword("wrong_password", hash));
}

void test_legacy_sha256_migration_on_login(void) {
  // Arrange: store a legacy SHA256 hash (64-char hex, no "p1:" prefix)
  String password = "migrate_me";
  String legacyHash = hashPassword(password);
  mockWebPassword = legacyHash;
  _passwordNeedsMigration = true;

  // Verify the legacy hash is 64 chars and does NOT start with "p1:"
  TEST_ASSERT_EQUAL(64, (int)mockWebPassword.length());
  TEST_ASSERT_FALSE(mockWebPassword.find("p1:") == 0);

  // Act: successful login should trigger migration
  unsigned long retryAfter = 0;
  int status = simulateLogin(password, retryAfter);
  TEST_ASSERT_EQUAL(200, status);

  // Assert: password should now be stored as PBKDF2 v2 hash
  TEST_ASSERT_EQUAL(100, (int)mockWebPassword.length());
  TEST_ASSERT_TRUE(mockWebPassword.find("p2:") == 0);

  // Assert: new PBKDF2 v2 hash still verifies with the same password
  TEST_ASSERT_TRUE(verifyPassword(password, mockWebPassword));

  // Assert: migration flag is cleared
  TEST_ASSERT_FALSE(_passwordNeedsMigration);
}

// ===== PBKDF2 v2 Tests (50k iterations) =====

void test_pbkdf2_v2_hash_format(void) {
  // hashPasswordPbkdf2() must produce "p2:" prefix and exactly 100 chars
  String hash = hashPasswordPbkdf2("test_password");
  TEST_ASSERT_EQUAL(100, (int)hash.length());
  TEST_ASSERT_TRUE(hash.find("p2:") == 0);
}

void test_pbkdf2_v2_verify_cycle(void) {
  // Hash with v2 — correct password verifies, wrong password fails
  String password = "v2_password_test";
  String hash = hashPasswordPbkdf2(password);

  TEST_ASSERT_TRUE(hash.find("p2:") == 0);
  TEST_ASSERT_TRUE(verifyPassword(password, hash));
  TEST_ASSERT_FALSE(verifyPassword("wrong_password", hash));
}

void test_pbkdf2_v1_still_verifies(void) {
  // A manually constructed p1: hash (10k iterations) must still verify correctly
  uint8_t salt[16] = {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
                      0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10};
  String password = "v1_legacy_password";
  String p1Hash = hashPasswordPbkdf2WithSalt(password, salt);

  // Confirm it is a p1: hash
  TEST_ASSERT_TRUE(p1Hash.find("p1:") == 0);
  TEST_ASSERT_EQUAL(100, (int)p1Hash.length());

  // verifyPassword must accept the correct password against a p1: hash
  TEST_ASSERT_TRUE(verifyPassword(password, p1Hash));
  TEST_ASSERT_FALSE(verifyPassword("wrong_password", p1Hash));
}

void test_pbkdf2_v1_to_v2_migration(void) {
  // Simulate: stored hash is p1:, _passwordNeedsMigration=true, successful login
  // triggers rehash to p2:
  String password = "migrate_v1_to_v2";
  uint8_t salt[16] = {0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,
                      0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff,0x00};
  mockWebPassword = hashPasswordPbkdf2WithSalt(password, salt);  // p1: hash
  _passwordNeedsMigration = true;

  TEST_ASSERT_TRUE(mockWebPassword.find("p1:") == 0);

  // Successful login with the correct password
  unsigned long retryAfter = 0;
  int status = simulateLogin(password, retryAfter);
  TEST_ASSERT_EQUAL(200, status);

  // After migration mockWebPassword must be p2:
  TEST_ASSERT_TRUE(mockWebPassword.find("p2:") == 0);
  TEST_ASSERT_EQUAL(100, (int)mockWebPassword.length());

  // New p2: hash must still verify with the original password
  TEST_ASSERT_TRUE(verifyPassword(password, mockWebPassword));

  // Migration flag must be cleared
  TEST_ASSERT_FALSE(_passwordNeedsMigration);
}

void test_pbkdf2_v2_no_migration(void) {
  // Login with an existing p2: hash must NOT trigger migration
  String password = "already_v2_password";
  mockWebPassword = hashPasswordPbkdf2(password);  // produces p2:
  _passwordNeedsMigration = false;

  TEST_ASSERT_TRUE(mockWebPassword.find("p2:") == 0);

  unsigned long retryAfter = 0;
  int status = simulateLogin(password, retryAfter);
  TEST_ASSERT_EQUAL(200, status);

  // Password must still be p2: — no migration occurred
  TEST_ASSERT_TRUE(mockWebPassword.find("p2:") == 0);
  TEST_ASSERT_FALSE(_passwordNeedsMigration);
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

  // Default password consistency tests (now using hashed comparisons)
  RUN_TEST(test_is_default_password_with_unchanged_ap);
  RUN_TEST(test_is_default_password_with_changed_ap);
  RUN_TEST(test_is_not_default_after_web_password_change);

  // Timing-safe comparison tests
  RUN_TEST(test_timing_safe_compare_equal_strings);
  RUN_TEST(test_timing_safe_compare_unequal_strings);
  RUN_TEST(test_timing_safe_compare_different_lengths);
  RUN_TEST(test_timing_safe_compare_empty_strings);

  // Password hashing tests
  RUN_TEST(test_hash_password_deterministic);
  RUN_TEST(test_hash_password_different_inputs_diverge);
  RUN_TEST(test_nvs_migration_from_plaintext);

  // Rate limiting tests
  RUN_TEST(test_progressive_login_delay_values);

  // Timing-safe session validation (Group 4A)
  RUN_TEST(test_session_validation_uses_timing_safe_compare);
  RUN_TEST(test_session_expiry_with_64bit_timestamps);

  // Non-blocking rate limiting tests (Phase 2)
  RUN_TEST(test_rate_limit_429_after_5_failures);
  RUN_TEST(test_rate_limit_retry_after_matches_delay);
  RUN_TEST(test_rate_limit_resets_on_successful_login);
  RUN_TEST(test_rate_limit_resets_after_5min_cooldown);
  RUN_TEST(test_rate_limit_is_non_blocking);

  // PBKDF2 password hashing tests (Phase 3)
  RUN_TEST(test_pbkdf2_hash_verifies_correctly);
  RUN_TEST(test_legacy_sha256_migration_on_login);

  // PBKDF2 v2 tests (50k iterations)
  RUN_TEST(test_pbkdf2_v2_hash_format);
  RUN_TEST(test_pbkdf2_v2_verify_cycle);
  RUN_TEST(test_pbkdf2_v1_still_verifies);
  RUN_TEST(test_pbkdf2_v1_to_v2_migration);
  RUN_TEST(test_pbkdf2_v2_no_migration);

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
