#ifndef GLOBALS_H
#define GLOBALS_H

#ifdef NATIVE_TEST
#include "../test/test_mocks/Arduino.h"
// Minimal stubs for extern type declarations in native tests
class WiFiClient {
public:
    bool connected() { return false; }
    void stop() {}
    bool connect(const char*, uint16_t, int) { return false; }
    void setTimeout(uint32_t) {}
};
class WiFiClientSecure : public WiFiClient {
public:
    void setInsecure() {}
    void setCACert(const char*) {}
};
class PubSubClient {
public:
    bool connected() { return false; }
    bool connect(const char*) { return false; }
    bool publish(const char*, const char*) { return false; }
    void loop() {}
    void setClient(WiFiClient&) {}
};
class WebServer {
public:
    void handleClient() {}
};
class WebSocketsServer {
public:
    void loop() {}
    void broadcastTXT(const String&) {}
};
class WiFiClass {
public:
    bool isConnected() { return false; }
};
#else
#include <Arduino.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#endif

// ===== Global Object extern declarations =====
// These are actual global objects, not AppState members
extern WebServer server;
extern WebSocketsServer webSocket;
extern WiFiClient mqttWifiClient;
extern WiFiClientSecure mqttWifiClientSecure;
extern PubSubClient mqttClient;

// Firmware info (const, not state)
extern const char *firmwareVer;
extern const char *githubRepoOwner;
extern const char *githubRepoName;

// ===== API Version Helper =====
// Registers a handler on both /api/<path> AND /api/v1/<path> for backward compatibility.
// The /api/ path is retained for existing clients; /api/v1/ is the new canonical path.
// path must begin with "/api/" (e.g. "/api/settings").
#ifndef NATIVE_TEST
#include <functional>
template<typename THandler>
inline void server_on_versioned(const char* path, HTTPMethod method, THandler handler) {
    server.on(path, method, handler);
    // Build /api/v1/ path: skip the first 4 chars ("/api") and prepend "/api/v1"
    String v1path = "/api/v1";
    v1path += (path + 4);  // path+4 skips "/api" → appends "/settings", "/hal/devices", etc.
    server.on(v1path.c_str(), method, handler);
}
#endif

#endif // GLOBALS_H
