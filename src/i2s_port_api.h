#ifndef I2S_PORT_API_H
#define I2S_PORT_API_H

#ifndef NATIVE_TEST
class WebServer;

// Register I2S port status REST API endpoints
void registerI2sPortApiEndpoints(WebServer& server);
#endif

#endif // I2S_PORT_API_H
