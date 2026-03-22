#ifndef STATE_ETHERNET_STATE_H
#define STATE_ETHERNET_STATE_H

#include "config.h"
#include "state/enums.h"

struct EthernetState {
  // ===== Core link/IP state =====
  bool linkUp = false;
  bool connected = false;
  String ip = "";
  NetIfType activeInterface = NET_NONE;

  // ===== Runtime status (populated from ETH events, NOT persisted) =====
  String mac = "";
  String gateway = "";
  String subnet = "";
  String dns1 = "";
  String dns2 = "";
  uint16_t speed = 0;
  bool fullDuplex = false;

  // ===== Configuration (persisted to /config.json) =====
  bool useStaticIP = false;
  String staticIP = "";
  String staticSubnet = "255.255.255.0";
  String staticGateway = "";
  String staticDns1 = "";
  String staticDns2 = "";
  String hostname = "alx-nova";

  // ===== Revert timer (NOT persisted) =====
  bool pendingConfirm = false;
  unsigned long confirmDeadline = 0;
};

#endif // STATE_ETHERNET_STATE_H
