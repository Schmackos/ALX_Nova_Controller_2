#ifndef STATE_WIFI_STATE_H
#define STATE_WIFI_STATE_H

#include "config.h"

// WiFi + AP mode state
struct WifiState {
  String ssid;
  String password;

  // AP mode
  bool isAPMode = false;
  bool apEnabled = false;
  bool autoAPEnabled = true;
  String apSSID;
  String apPassword = DEFAULT_AP_PASSWORD;

  // Web authentication
  String webPassword = DEFAULT_AP_PASSWORD;

  // Security
  uint8_t minSecurity = 0;  // 0=any, 1=WPA2+, 2=WPA3 only

  // Async connection state
  bool connecting = false;
  bool connectSuccess = false;
  String newIP;
  String connectError;
};

#endif // STATE_WIFI_STATE_H
