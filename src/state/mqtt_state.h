#ifndef STATE_MQTT_STATE_H
#define STATE_MQTT_STATE_H

#include "config.h"

// MQTT broker config + connection state
struct MqttState {
  bool enabled = false;
  String broker;
  int port = DEFAULT_MQTT_PORT;
  String username;
  String password;
  String baseTopic;
  bool haDiscovery = false;
  bool useTls = false;       // Enable TLS encryption for MQTT broker connection
  bool verifyCert = false;   // Verify broker certificate (requires public CA; disable for self-signed)
  unsigned long lastReconnect = 0;
  bool connected = false;
  unsigned long lastPublish = 0;
};

#endif // STATE_MQTT_STATE_H
