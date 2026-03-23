/**
 * test_websocket_auth.cpp
 *
 * Unit tests for the WebSocket one-time token pool used by auth_handler.cpp.
 * Tests token generation, validation, expiry, single-use enforcement, and
 * pool-full behavior.
 *
 * The token pool logic is replicated inline (same pattern as test_auth_handler)
 * because generateWsToken() depends on getSessionFromCookie() / WebServer which
 * are impractical to mock. The replicated logic is identical to auth_handler.cpp
 * lines 28-231.
 */

#include <cstring>
#include <string>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/esp_random.h"
#else
#include <Arduino.h>
#include <esp_random.h>
#endif

// ===== Replicated WS token pool (mirrors auth_handler.cpp) =====

#define WS_TOKEN_SLOTS 16
#define WS_TOKEN_TTL_MS 60000  // 60 seconds

struct WsToken {
    char token[37];       // UUID string (36 chars + null)
    char sessionId[37];   // session that generated this token
    uint32_t createdMs;
    bool used;
};

static WsToken _wsTokens[WS_TOKEN_SLOTS];

// Timing-safe compare (mirrors auth_handler.cpp NATIVE_TEST path)
static bool timingSafeCompare(const String &a, const String &b) {
    size_t lenA = a.length();
    size_t lenB = b.length();
    if (lenA == 0 && lenB == 0) return true;
    if (lenA != lenB) return false;
    volatile uint8_t result = 0;
    const char *pA = a.c_str();
    const char *pB = b.c_str();
    for (size_t i = 0; i < lenA; i++) {
        result |= (uint8_t)pA[i] ^ (uint8_t)pB[i];
    }
    return result == 0;
}

// UUID generator (mirrors generateSessionId in auth_handler.cpp)
static String generateSessionId() {
    uint8_t randomBytes[16];
    esp_fill_random(randomBytes, 16);

    char uuid[37];
    snprintf(uuid, sizeof(uuid),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             randomBytes[0], randomBytes[1], randomBytes[2], randomBytes[3],
             randomBytes[4], randomBytes[5], randomBytes[6], randomBytes[7],
             randomBytes[8], randomBytes[9], randomBytes[10], randomBytes[11],
             randomBytes[12], randomBytes[13], randomBytes[14], randomBytes[15]);
    return String(uuid);
}

// Test-friendly version: accepts session as parameter instead of reading cookies
static String generateWsTokenForSession(const String &currentSession) {
    uint32_t now = millis();

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
            String uuid = generateSessionId();
            strncpy(_wsTokens[i].token, uuid.c_str(), 36);
            _wsTokens[i].token[36] = '\0';
            strncpy(_wsTokens[i].sessionId, currentSession.c_str(), 36);
            _wsTokens[i].sessionId[36] = '\0';
            _wsTokens[i].createdMs = now;
            _wsTokens[i].used = false;
            return uuid;
        }
    }

    return "";  // no free slot
}

// Validate and consume a one-time WS token (mirrors auth_handler.cpp)
static bool validateWsToken(const String &token, String &outSessionId) {
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

// ===== Helper =====

static void clearTokenPool() {
    memset(_wsTokens, 0, sizeof(_wsTokens));
}

static const char *TEST_SESSION = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";

// ===== Test Setup / Teardown =====

void setUp() {
    ArduinoMock::reset();
    EspRandomMock::reset();
    clearTokenPool();
    // Start at a non-zero time to avoid edge cases with millis() == 0
    ArduinoMock::mockMillis = 1000;
}

void tearDown() {
    // Nothing to clean up beyond setUp reset
}

// ===== Test: Generate 16 tokens, all succeed =====

void test_generate_16_tokens_all_succeed() {
    String tokens[WS_TOKEN_SLOTS];

    for (int i = 0; i < WS_TOKEN_SLOTS; i++) {
        tokens[i] = generateWsTokenForSession(TEST_SESSION);
        TEST_ASSERT_TRUE_MESSAGE(tokens[i].length() > 0,
                                 "Token generation should succeed for all 16 slots");
    }

    // Verify all tokens are distinct
    for (int i = 0; i < WS_TOKEN_SLOTS; i++) {
        for (int j = i + 1; j < WS_TOKEN_SLOTS; j++) {
            TEST_ASSERT_FALSE_MESSAGE(
                tokens[i] == tokens[j],
                "Each generated token must be unique");
        }
    }
}

// ===== Test: 17th token fails when pool is full =====

void test_17th_token_fails_pool_full() {
    // Fill all 16 slots
    for (int i = 0; i < WS_TOKEN_SLOTS; i++) {
        String tok = generateWsTokenForSession(TEST_SESSION);
        TEST_ASSERT_TRUE(tok.length() > 0);
    }

    // 17th should fail
    String overflow = generateWsTokenForSession(TEST_SESSION);
    TEST_ASSERT_EQUAL_STRING("", overflow.c_str());
}

// ===== Test: Token expires after TTL =====

void test_token_expires_after_ttl() {
    String token = generateWsTokenForSession(TEST_SESSION);
    TEST_ASSERT_TRUE(token.length() > 0);

    // Advance time past TTL (60001ms past creation)
    ArduinoMock::mockMillis += WS_TOKEN_TTL_MS + 1;

    String outSession;
    bool valid = validateWsToken(token, outSession);
    TEST_ASSERT_FALSE_MESSAGE(valid,
                              "Token should be invalid after TTL expires");
}

// ===== Test: Token valid just before TTL =====

void test_token_valid_at_ttl_boundary() {
    String token = generateWsTokenForSession(TEST_SESSION);
    TEST_ASSERT_TRUE(token.length() > 0);

    // Advance time to exactly TTL (should still be valid)
    ArduinoMock::mockMillis += WS_TOKEN_TTL_MS;

    String outSession;
    bool valid = validateWsToken(token, outSession);
    TEST_ASSERT_TRUE_MESSAGE(valid,
                             "Token should still be valid at exactly TTL boundary");
}

// ===== Test: Expired slot is reusable =====

void test_expired_slot_reusable() {
    // Fill all 16 slots
    for (int i = 0; i < WS_TOKEN_SLOTS; i++) {
        generateWsTokenForSession(TEST_SESSION);
    }

    // Confirm pool is full
    String shouldFail = generateWsTokenForSession(TEST_SESSION);
    TEST_ASSERT_EQUAL_STRING("", shouldFail.c_str());

    // Expire all tokens
    ArduinoMock::mockMillis += WS_TOKEN_TTL_MS + 1;

    // Now generation should succeed (expired slots purged on next generate call)
    String newToken = generateWsTokenForSession(TEST_SESSION);
    TEST_ASSERT_TRUE_MESSAGE(newToken.length() > 0,
                             "Expired slot should be reclaimed for new token");
}

// ===== Test: Validate token with correct session =====

void test_token_validation_correct_session() {
    String token = generateWsTokenForSession(TEST_SESSION);
    TEST_ASSERT_TRUE(token.length() > 0);

    String outSession;
    bool valid = validateWsToken(token, outSession);
    TEST_ASSERT_TRUE_MESSAGE(valid, "Token should validate successfully");
    TEST_ASSERT_EQUAL_STRING(TEST_SESSION, outSession.c_str());
}

// ===== Test: Token is single-use =====

void test_token_single_use() {
    String token = generateWsTokenForSession(TEST_SESSION);
    TEST_ASSERT_TRUE(token.length() > 0);

    // First validation succeeds
    String outSession;
    bool firstUse = validateWsToken(token, outSession);
    TEST_ASSERT_TRUE_MESSAGE(firstUse, "First validation should succeed");

    // Second validation fails
    String outSession2;
    bool secondUse = validateWsToken(token, outSession2);
    TEST_ASSERT_FALSE_MESSAGE(secondUse,
                              "Token must not be usable a second time");
}

// ===== Test: Used slot is reusable =====

void test_used_slot_reusable() {
    // Fill all 16 slots
    String tokens[WS_TOKEN_SLOTS];
    for (int i = 0; i < WS_TOKEN_SLOTS; i++) {
        tokens[i] = generateWsTokenForSession(TEST_SESSION);
    }

    // Consume one token
    String outSession;
    validateWsToken(tokens[0], outSession);

    // Generate should succeed by reclaiming the used slot
    String newToken = generateWsTokenForSession(TEST_SESSION);
    TEST_ASSERT_TRUE_MESSAGE(newToken.length() > 0,
                             "Used slot should be reclaimed for new token");
}

// ===== Test: Empty token validation fails =====

void test_empty_token_validation_fails() {
    String outSession;
    bool valid = validateWsToken(String(""), outSession);
    TEST_ASSERT_FALSE_MESSAGE(valid,
                              "Empty token must be rejected");
}

// ===== Test: Wrong token string fails validation =====

void test_wrong_token_fails_validation() {
    String token = generateWsTokenForSession(TEST_SESSION);
    TEST_ASSERT_TRUE(token.length() > 0);

    String outSession;
    bool valid = validateWsToken(String("00000000-0000-0000-0000-000000000000"), outSession);
    TEST_ASSERT_FALSE_MESSAGE(valid,
                              "Non-matching token must be rejected");
}

// ===== Test: Different sessions get independent tokens =====

void test_different_sessions_independent() {
    const char *sessionA = "11111111-1111-1111-1111-111111111111";
    const char *sessionB = "22222222-2222-2222-2222-222222222222";

    String tokenA = generateWsTokenForSession(sessionA);
    String tokenB = generateWsTokenForSession(sessionB);
    TEST_ASSERT_TRUE(tokenA.length() > 0);
    TEST_ASSERT_TRUE(tokenB.length() > 0);

    // Validate tokenA returns sessionA
    String outA;
    TEST_ASSERT_TRUE(validateWsToken(tokenA, outA));
    TEST_ASSERT_EQUAL_STRING(sessionA, outA.c_str());

    // Validate tokenB returns sessionB
    String outB;
    TEST_ASSERT_TRUE(validateWsToken(tokenB, outB));
    TEST_ASSERT_EQUAL_STRING(sessionB, outB.c_str());
}

// ===== Test: Token format is UUID-like (36 chars with dashes) =====

void test_token_format_uuid() {
    String token = generateWsTokenForSession(TEST_SESSION);
    TEST_ASSERT_EQUAL_UINT32(36, token.length());

    // Check dash positions: 8-4-4-4-12
    TEST_ASSERT_EQUAL_CHAR('-', token.c_str()[8]);
    TEST_ASSERT_EQUAL_CHAR('-', token.c_str()[13]);
    TEST_ASSERT_EQUAL_CHAR('-', token.c_str()[18]);
    TEST_ASSERT_EQUAL_CHAR('-', token.c_str()[23]);
}

// ===== Test: Partial expiry reclaims only expired slots =====

void test_partial_expiry_reclaims_only_expired() {
    // Generate 16 tokens at time 1000
    String earlyTokens[WS_TOKEN_SLOTS];
    for (int i = 0; i < WS_TOKEN_SLOTS; i++) {
        earlyTokens[i] = generateWsTokenForSession(TEST_SESSION);
    }

    // Consume 4 tokens (slots 0-3 become used)
    String dummy;
    for (int i = 0; i < 4; i++) {
        validateWsToken(earlyTokens[i], dummy);
    }

    // Pool is still full (12 active + 4 used)
    // Attempt generate: used slots get purged, so 4 slots free up
    String newToken = generateWsTokenForSession(TEST_SESSION);
    TEST_ASSERT_TRUE_MESSAGE(newToken.length() > 0,
                             "Used slots should be purged on generate, freeing space");

    // The 12 unconsumed tokens should still validate
    for (int i = 4; i < WS_TOKEN_SLOTS; i++) {
        String out;
        TEST_ASSERT_TRUE_MESSAGE(
            validateWsToken(earlyTokens[i], out),
            "Unconsumed non-expired token should still be valid");
    }
}

// ===== Test: millis() wraparound (uint32_t overflow) =====

void test_millis_wraparound_expires_token() {
    // Create token at near-max millis
    ArduinoMock::mockMillis = 0xFFFFFFFF - 1000;
    String token = generateWsTokenForSession(TEST_SESSION);
    TEST_ASSERT_TRUE(token.length() > 0);

    // Advance past wraparound (unsigned subtraction handles this)
    // At millis = 0xFFFFFFFF - 1000 + WS_TOKEN_TTL_MS + 1
    // With uint32_t wraparound, now - createdMs > WS_TOKEN_TTL_MS
    ArduinoMock::mockMillis = (uint32_t)(0xFFFFFFFF - 1000 + WS_TOKEN_TTL_MS + 2);

    String outSession;
    bool valid = validateWsToken(token, outSession);
    TEST_ASSERT_FALSE_MESSAGE(valid,
                              "Token should expire even across millis() wraparound");
}

// ===== Test: Generate after partial use fills used slots first =====

void test_generate_reuses_earliest_free_slot() {
    // Fill pool
    String tokens[WS_TOKEN_SLOTS];
    for (int i = 0; i < WS_TOKEN_SLOTS; i++) {
        tokens[i] = generateWsTokenForSession(TEST_SESSION);
    }

    // Mark slot 5 as used
    String outSession;
    validateWsToken(tokens[5], outSession);

    // Generate new token - should succeed (slot 5 was used, gets purged)
    String newToken = generateWsTokenForSession(TEST_SESSION);
    TEST_ASSERT_TRUE(newToken.length() > 0);

    // Try to generate another - pool should be full again
    String overflow = generateWsTokenForSession(TEST_SESSION);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("", overflow.c_str(),
                                     "Pool should be full after reclaiming only 1 used slot");
}

// ===== Main =====

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_generate_16_tokens_all_succeed);
    RUN_TEST(test_17th_token_fails_pool_full);
    RUN_TEST(test_token_expires_after_ttl);
    RUN_TEST(test_token_valid_at_ttl_boundary);
    RUN_TEST(test_expired_slot_reusable);
    RUN_TEST(test_token_validation_correct_session);
    RUN_TEST(test_token_single_use);
    RUN_TEST(test_used_slot_reusable);
    RUN_TEST(test_empty_token_validation_fails);
    RUN_TEST(test_wrong_token_fails_validation);
    RUN_TEST(test_different_sessions_independent);
    RUN_TEST(test_token_format_uuid);
    RUN_TEST(test_partial_expiry_reclaims_only_expired);
    RUN_TEST(test_millis_wraparound_expires_token);
    RUN_TEST(test_generate_reuses_earliest_free_slot);

    return UNITY_END();
}
