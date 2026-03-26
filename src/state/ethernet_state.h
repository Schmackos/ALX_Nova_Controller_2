#ifndef STATE_ETHERNET_STATE_H
#define STATE_ETHERNET_STATE_H

#include "config.h"
#include "state/enums.h"

struct EthernetState {
  // ===== Core link/IP state =====
  bool linkUp = false;
  bool connected = false;
  char ip[40] = "";
  NetIfType activeInterface = NET_NONE;

  // ===== Runtime status (populated from ETH events, NOT persisted) =====
  char mac[40] = "";
  char gateway[40] = "";
  char subnet[40] = "";
  char dns1[40] = "";
  char dns2[40] = "";
  uint16_t speed = 0;
  bool fullDuplex = false;

  // ===== Configuration (persisted to /config.json) =====
  bool useStaticIP = false;
  char staticIP[40] = "";
  char staticSubnet[40] = "255.255.255.0";
  char staticGateway[40] = "";
  char staticDns1[40] = "";
  char staticDns2[40] = "";
  char hostname[64] = "alx-nova";

  // ===== Revert timer (NOT persisted) =====
  bool pendingConfirm = false;
  unsigned long confirmDeadline = 0;
};

#endif // STATE_ETHERNET_STATE_H
