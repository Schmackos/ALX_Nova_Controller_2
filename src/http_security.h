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
#else
inline void http_add_security_headers() {} // no-op in tests
#endif
