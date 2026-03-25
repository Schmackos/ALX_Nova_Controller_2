#pragma once

// HTTP Security Headers — applied to all responses
// Prevents clickjacking (X-Frame-Options) and MIME-sniffing (X-Content-Type-Options)
#ifndef NATIVE_TEST
#include <WebServer.h>
extern WebServer server;

inline void http_add_security_headers() {
    server.sendHeader("X-Frame-Options", "DENY");
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.sendHeader("Content-Security-Policy",
        "default-src 'self'; "
        "script-src 'self' 'unsafe-inline' https://cdn.jsdelivr.net; "
        "style-src 'self' 'unsafe-inline'; "
        "img-src 'self' data:; "
        "connect-src 'self' ws: wss: https://cdn.jsdelivr.net https://github.com https://raw.githubusercontent.com; "
        "frame-src 'none'; "
        "object-src 'none'; "
        "base-uri 'self'; "
        "form-action 'self'");
}

// Convenience wrappers that inject security headers then call server.send().
// Use server_send() instead of the http_add_security_headers() + server.send() pair.
inline void server_send(int code, const char* contentType, const String& content) {
    http_add_security_headers();
    server.send(code, contentType, content);
}
inline void server_send(int code, const char* contentType, const char* content) {
    http_add_security_headers();
    server.send(code, contentType, content);
}
inline void server_send(int code) {
    http_add_security_headers();
    server.send(code);
}
inline void server_send_P(int code, const char* contentType, const char* content) {
    http_add_security_headers();
    server.send_P(code, contentType, content);
}
inline void server_send_P(int code, const char* contentType, const char* content, size_t len) {
    http_add_security_headers();
    server.send_P(code, contentType, content, len);
}
// Structured JSON response envelope: {"success":true/false[,"data":...][,"error":"..."]}.
// Use for new REST endpoints to ensure consistent response shape.
// Do NOT retrofit existing endpoints — changes would break existing clients.
// `data` must be pre-serialized JSON (object/array). `error` is escaped for safety.
inline void json_response(WebServer& server, int code, const char* data = nullptr, const char* error = nullptr) {
    String json = "{\"success\":";
    json += (code >= 200 && code < 300) ? "true" : "false";
    if (data)  { json += ",\"data\":";    json += data;  }
    if (error) {
        json += ",\"error\":\"";
        // Escape special JSON characters to prevent malformed output
        for (const char* p = error; *p; ++p) {
            switch (*p) {
                case '"':  json += "\\\""; break;
                case '\\': json += "\\\\"; break;
                case '\n': json += "\\n";  break;
                case '\r': json += "\\r";  break;
                case '\t': json += "\\t";  break;
                default:   json += *p;     break;
            }
        }
        json += "\"";
    }
    json += "}";
    server_send(code, "application/json", json);
}

// Sanitize a user-provided filename to prevent path traversal.
// Returns true if the name is safe and was copied to `out`; false otherwise.
inline bool sanitize_filename(const char* name, char* out, size_t outSize) {
    if (!name || !out || outSize == 0) return false;
    size_t len = strlen(name);
    if (len == 0 || len > 32) return false;
    // Reject path traversal sequences
    if (strstr(name, "..") || strchr(name, '/') || strchr(name, '\\')) return false;
    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_' || c == ' ' || c == '.')) {
            return false;
        }
    }
    // Safe copy with guaranteed null termination
    size_t copyLen = len < (outSize - 1) ? len : (outSize - 1);
    memcpy(out, name, copyLen);
    out[copyLen] = '\0';
    return true;
}

#else
// Stubs for native/test builds where WebServer is not available
inline void http_add_security_headers() {}
inline void server_send(int /*code*/, const char* /*contentType*/, const char* /*content*/) {}
inline void server_send(int /*code*/) {}
inline void server_send_P(int /*code*/, const char* /*contentType*/, const char* /*content*/) {}
inline void server_send_P(int /*code*/, const char* /*contentType*/, const char* /*content*/, size_t /*len*/) {}
// json_response stub — server type omitted since WebServer is not available in native builds
template<typename T>
inline void json_response(T& /*server*/, int /*code*/, const char* /*data*/ = nullptr, const char* /*error*/ = nullptr) {}

// sanitize_filename is available in test builds too
inline bool sanitize_filename(const char* name, char* out, size_t outSize) {
    if (!name || !out || outSize == 0) return false;
    size_t len = strlen(name);
    if (len == 0 || len > 32) return false;
    if (strstr(name, "..") || strchr(name, '/') || strchr(name, '\\')) return false;
    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_' || c == ' ' || c == '.')) {
            return false;
        }
    }
    size_t copyLen = len < (outSize - 1) ? len : (outSize - 1);
    memcpy(out, name, copyLen);
    out[copyLen] = '\0';
    return true;
}
#endif
