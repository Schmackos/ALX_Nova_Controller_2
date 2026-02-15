#ifndef AUTH_HANDLER_H
#define AUTH_HANDLER_H

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define MAX_SESSIONS 5
#define SESSION_TIMEOUT_US 3600000000ULL // 1 hour in microseconds

struct Session {
  String sessionId;
  uint64_t createdAt;
  uint64_t lastSeen;
};

extern Session activeSessions[MAX_SESSIONS];

// Initialization
void initAuth();

// Session management
String generateSessionId();
bool createSession(String &sessionId);
bool validateSession(String sessionId);
void removeSession(String sessionId);
String getSessionFromCookie();

// Middleware
bool requireAuth();

// Password management
String getWebPassword();
void setWebPassword(String newPassword);
bool isDefaultPassword();

// Security utilities
bool timingSafeCompare(const String &a, const String &b);
String hashPassword(const String &password);
void resetLoginRateLimit();

// Handlers
void handleLogin();
void handleLogout();
void handleAuthStatus();
void handlePasswordChange();

#endif
