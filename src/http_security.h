#pragma once

// HTTP Security Headers — applied to all responses
// Prevents clickjacking (X-Frame-Options) and MIME-sniffing (X-Content-Type-Options)
#ifndef NATIVE_TEST
#include <WebServer.h>
extern WebServer server;

inline void http_add_security_headers() {
    server.sendHeader("X-Frame-Options", "DENY");
    server.sendHeader("X-Content-Type-Options", "nosniff");
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
