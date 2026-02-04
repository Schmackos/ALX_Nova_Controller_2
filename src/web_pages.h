// Web page HTML content declarations
#ifndef WEB_PAGES_H
#define WEB_PAGES_H

#include <Arduino.h>
#include <pgmspace.h>
#include <WebServer.h>

// Raw PROGMEM pages (for development/debugging)
extern const char htmlPage[] PROGMEM;
extern const char apHtmlPage[] PROGMEM;

// Gzipped versions for efficient serving (~85% smaller transfer)
extern const uint8_t htmlPage_gz[] PROGMEM;
extern const size_t htmlPage_gz_len;
extern const uint8_t apHtmlPage_gz[] PROGMEM;
extern const size_t apHtmlPage_gz_len;
extern const uint8_t loginPage_gz[] PROGMEM;
extern const size_t loginPage_gz_len;

// Helper function to serve gzipped content when supported
// Returns true if gzipped response was sent, false otherwise
inline bool sendGzipped(WebServer& server, const uint8_t* gzData, size_t gzLen,
                        const char* contentType = "text/html") {
    if (server.hasHeader("Accept-Encoding")) {
        String encoding = server.header("Accept-Encoding");
        if (encoding.indexOf("gzip") >= 0) {
            server.sendHeader("Content-Encoding", "gzip");
            server.sendHeader("Cache-Control", "no-cache");
            server.send_P(200, contentType, reinterpret_cast<const char*>(gzData), gzLen);
            return true;
        }
    }
    return false;
}

#endif
