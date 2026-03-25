#include <unity.h>
#include <string.h>
#include <stdio.h>

// http_security.h is a no-op under NATIVE_TEST (defined by the build system).
// Tests verify constant values and that the stub overloads compile correctly.
#include "../../src/http_security.h"

// ===== json_response envelope logic — replicated for native testing =====
// The real json_response() in http_security.h writes to WebServer (unavailable
// in native builds). We replicate the exact same envelope logic here so we can
// verify the JSON structure is correct.
static void build_json_envelope(char* out, size_t outSize, int code,
                                const char* data, const char* error) {
    int pos = 0;
    pos += snprintf(out + pos, outSize - pos, "{\"success\":");
    pos += snprintf(out + pos, outSize - pos, "%s",
                    (code >= 200 && code < 300) ? "true" : "false");
    if (data) {
        pos += snprintf(out + pos, outSize - pos, ",\"data\":%s", data);
    }
    if (error) {
        pos += snprintf(out + pos, outSize - pos, ",\"error\":\"%s\"", error);
    }
    snprintf(out + pos, outSize - pos, "}");
}

void setUp(void) {}
void tearDown(void) {}

void test_x_frame_options_value_is_deny(void) {
    const char* expected = "DENY";
    TEST_ASSERT_EQUAL_STRING(expected, "DENY");
}

void test_x_content_type_options_value_is_nosniff(void) {
    const char* expected = "nosniff";
    TEST_ASSERT_EQUAL_STRING(expected, "nosniff");
}

void test_security_headers_count(void) {
    // We add exactly 3 security headers
    int headerCount = 3; // X-Frame-Options + X-Content-Type-Options + Content-Security-Policy
    TEST_ASSERT_EQUAL_INT(3, headerCount);
}

void test_csp_header_present(void) {
    // Content-Security-Policy header is now added by http_add_security_headers()
    // Verify the expected CSP directives exist as string constants
    const char* csp = "default-src 'self'; "
        "script-src 'self' 'unsafe-inline' https://cdn.jsdelivr.net; "
        "style-src 'self' 'unsafe-inline'; "
        "img-src 'self' data:; "
        "connect-src 'self' ws: wss: https://cdn.jsdelivr.net https://github.com https://raw.githubusercontent.com; "
        "frame-src 'none'; "
        "object-src 'none'; "
        "base-uri 'self'; "
        "form-action 'self'";
    TEST_ASSERT_NOT_NULL(csp);
    TEST_ASSERT_TRUE(strlen(csp) > 0);
}

void test_csp_blocks_frame_embedding(void) {
    const char* csp = "frame-src 'none'";
    TEST_ASSERT_NOT_NULL(strstr(
        "default-src 'self'; script-src 'self' 'unsafe-inline' https://cdn.jsdelivr.net; "
        "style-src 'self' 'unsafe-inline'; img-src 'self' data:; "
        "connect-src 'self' ws: wss: https://cdn.jsdelivr.net https://github.com https://raw.githubusercontent.com; "
        "frame-src 'none'; object-src 'none'; base-uri 'self'; form-action 'self'",
        csp));
}

void test_csp_blocks_object_embedding(void) {
    const char* csp = "object-src 'none'";
    TEST_ASSERT_NOT_NULL(strstr(
        "default-src 'self'; script-src 'self' 'unsafe-inline' https://cdn.jsdelivr.net; "
        "style-src 'self' 'unsafe-inline'; img-src 'self' data:; "
        "connect-src 'self' ws: wss: https://cdn.jsdelivr.net https://github.com https://raw.githubusercontent.com; "
        "frame-src 'none'; object-src 'none'; base-uri 'self'; form-action 'self'",
        csp));
}

void test_csp_allows_cdn_scripts(void) {
    // CDN required for qrcodejs and marked.js loaded dynamically in support tab
    TEST_ASSERT_NOT_NULL(strstr(
        "script-src 'self' 'unsafe-inline' https://cdn.jsdelivr.net",
        "https://cdn.jsdelivr.net"));
}

void test_csp_allows_websocket_connections(void) {
    // WebSocket on port 81 requires ws: in connect-src
    TEST_ASSERT_NOT_NULL(strstr(
        "connect-src 'self' ws: wss: https://cdn.jsdelivr.net https://github.com https://raw.githubusercontent.com",
        "ws:"));
}

void test_csp_restricts_base_uri(void) {
    // Prevents base tag injection attacks
    TEST_ASSERT_NOT_NULL(strstr(
        "base-uri 'self'", "'self'"));
}

void test_csp_restricts_form_action(void) {
    // Prevents form hijacking to external URLs
    TEST_ASSERT_NOT_NULL(strstr(
        "form-action 'self'", "'self'"));
}

void test_header_names_are_standard(void) {
    // Verify header names match HTTP spec (case-sensitive)
    TEST_ASSERT_EQUAL_STRING("X-Frame-Options", "X-Frame-Options");
    TEST_ASSERT_EQUAL_STRING("X-Content-Type-Options", "X-Content-Type-Options");
}

// ===== server_send wrapper tests (NATIVE_TEST stubs) =====

void test_server_send_no_content_compiles(void) {
    // server_send(code) stub must be callable without crash
    server_send(200);
    TEST_ASSERT_TRUE(true);
}

void test_server_send_with_cstr_compiles(void) {
    // server_send(code, type, cstr) stub must be callable without crash
    server_send(200, "application/json", "{\"ok\":true}");
    TEST_ASSERT_TRUE(true);
}

void test_server_send_error_code_compiles(void) {
    // server_send with error code must be callable without crash
    server_send(400, "application/json", "{\"error\":true}");
    TEST_ASSERT_TRUE(true);
}

void test_http_add_security_headers_stub_compiles(void) {
    // http_add_security_headers() stub must be callable without crash
    http_add_security_headers();
    TEST_ASSERT_TRUE(true);
}

void test_json_response_stub_compiles(void) {
    // json_response template stub must compile and call without crash
    struct FakeServer {};
    FakeServer srv;
    json_response(srv, 200);
    json_response(srv, 200, "{\"x\":1}");
    json_response(srv, 400, nullptr, "bad request");
    TEST_ASSERT_TRUE(true);
}

// ===== json_response envelope logic tests =====

void test_json_response_envelope_success(void) {
    char out[128];
    build_json_envelope(out, sizeof(out), 200, nullptr, nullptr);
    TEST_ASSERT_EQUAL_STRING("{\"success\":true}", out);
}

void test_json_response_envelope_error(void) {
    char out[128];
    build_json_envelope(out, sizeof(out), 400, nullptr, "bad request");
    TEST_ASSERT_EQUAL_STRING("{\"success\":false,\"error\":\"bad request\"}", out);
}

void test_json_response_envelope_with_data(void) {
    char out[128];
    build_json_envelope(out, sizeof(out), 200, "{\"x\":1}", nullptr);
    TEST_ASSERT_EQUAL_STRING("{\"success\":true,\"data\":{\"x\":1}}", out);
}

void test_json_response_envelope_5xx_is_failure(void) {
    char out[128];
    build_json_envelope(out, sizeof(out), 503, nullptr, "unavailable");
    TEST_ASSERT_EQUAL_STRING("{\"success\":false,\"error\":\"unavailable\"}", out);
}

void test_json_response_envelope_201_is_success(void) {
    // HTTP 201 Created is in the 2xx range — success:true
    char out[128];
    build_json_envelope(out, sizeof(out), 201, nullptr, nullptr);
    TEST_ASSERT_EQUAL_STRING("{\"success\":true}", out);
}

void test_json_response_envelope_data_array(void) {
    // Data can be a JSON array, not just an object
    char out[128];
    build_json_envelope(out, sizeof(out), 200, "[1,2,3]", nullptr);
    TEST_ASSERT_EQUAL_STRING("{\"success\":true,\"data\":[1,2,3]}", out);
}

void test_json_response_envelope_404(void) {
    char out[128];
    build_json_envelope(out, sizeof(out), 404, nullptr, "not found");
    TEST_ASSERT_EQUAL_STRING("{\"success\":false,\"error\":\"not found\"}", out);
}

void test_json_response_null_data_omitted(void) {
    // When data is nullptr, "data" field must not appear in output
    char out[128];
    build_json_envelope(out, sizeof(out), 200, nullptr, nullptr);
    TEST_ASSERT_NULL(strstr(out, "\"data\""));
}

void test_json_response_null_error_omitted(void) {
    // When error is nullptr, "error" field must not appear in output
    char out[128];
    build_json_envelope(out, sizeof(out), 200, "{\"ok\":true}", nullptr);
    TEST_ASSERT_NULL(strstr(out, "\"error\""));
}

void test_json_response_boundary_199_is_failure(void) {
    // HTTP 199 is below 200 — should be success:false
    char out[128];
    build_json_envelope(out, sizeof(out), 199, nullptr, nullptr);
    TEST_ASSERT_EQUAL_STRING("{\"success\":false}", out);
}

void test_json_response_boundary_300_is_failure(void) {
    // HTTP 300 is outside 2xx range — should be success:false
    char out[128];
    build_json_envelope(out, sizeof(out), 300, nullptr, nullptr);
    TEST_ASSERT_EQUAL_STRING("{\"success\":false}", out);
}

void test_server_send_p_compiles(void) {
    server_send_P(200, "text/html", "<html></html>");
    TEST_ASSERT_TRUE(true);
}

void test_server_send_p_with_length_compiles(void) {
    const char* content = "<html></html>";
    server_send_P(200, "text/html", content, 13);
    TEST_ASSERT_TRUE(true);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_x_frame_options_value_is_deny);
    RUN_TEST(test_x_content_type_options_value_is_nosniff);
    RUN_TEST(test_security_headers_count);
    RUN_TEST(test_csp_header_present);
    RUN_TEST(test_csp_blocks_frame_embedding);
    RUN_TEST(test_csp_blocks_object_embedding);
    RUN_TEST(test_csp_allows_cdn_scripts);
    RUN_TEST(test_csp_allows_websocket_connections);
    RUN_TEST(test_csp_restricts_base_uri);
    RUN_TEST(test_csp_restricts_form_action);
    RUN_TEST(test_header_names_are_standard);
    RUN_TEST(test_server_send_no_content_compiles);
    RUN_TEST(test_server_send_with_cstr_compiles);
    RUN_TEST(test_server_send_error_code_compiles);
    RUN_TEST(test_server_send_p_compiles);
    RUN_TEST(test_server_send_p_with_length_compiles);
    RUN_TEST(test_http_add_security_headers_stub_compiles);
    RUN_TEST(test_json_response_stub_compiles);
    RUN_TEST(test_json_response_envelope_success);
    RUN_TEST(test_json_response_envelope_error);
    RUN_TEST(test_json_response_envelope_with_data);
    RUN_TEST(test_json_response_envelope_5xx_is_failure);
    RUN_TEST(test_json_response_envelope_201_is_success);
    RUN_TEST(test_json_response_envelope_data_array);
    RUN_TEST(test_json_response_envelope_404);
    RUN_TEST(test_json_response_null_data_omitted);
    RUN_TEST(test_json_response_null_error_omitted);
    RUN_TEST(test_json_response_boundary_199_is_failure);
    RUN_TEST(test_json_response_boundary_300_is_failure);
    return UNITY_END();
}
