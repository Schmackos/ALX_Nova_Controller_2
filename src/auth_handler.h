#ifndef AUTH_HANDLER_H
#define AUTH_HANDLER_H

#include <Arduino.h>
#include <WebServer.h>

#define MAX_SESSIONS 5
#define SESSION_TIMEOUT 3600000  // 1 hour in milliseconds

struct Session {
    String sessionId;
    unsigned long createdAt;
    unsigned long lastSeen;
};

extern Session activeSessions[MAX_SESSIONS];

// Initialization
void initAuth();

// Session management
String generateSessionId();
bool createSession(String& sessionId);
bool validateSession(String sessionId);
void removeSession(String sessionId);
String getSessionFromCookie();

// Middleware
bool requireAuth();

// Password management
String getWebPassword();
void setWebPassword(String newPassword);
bool isDefaultPassword();

// Handlers
void handleLogin();
void handleLogout();
void handleAuthStatus();
void handlePasswordChange();

#endif
