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
#else
// Stubs for native/test builds where WebServer is not available
inline void http_add_security_headers() {}
inline void server_send(int /*code*/, const char* /*contentType*/, const char* /*content*/) {}
inline void server_send(int /*code*/) {}
#endif
