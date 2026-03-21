#ifndef PSRAM_API_H
#define PSRAM_API_H

class WebServer;

// Register PSRAM health REST API endpoints on the global web server
void registerPsramApiEndpoints(WebServer &server);

#endif // PSRAM_API_H
