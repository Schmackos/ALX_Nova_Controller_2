#ifndef STATE_ETHERNET_STATE_H
#define STATE_ETHERNET_STATE_H

#include "config.h"
#include "state/enums.h"

struct EthernetState {
  bool linkUp = false;
  bool connected = false;
  String ip = "";
  NetIfType activeInterface = NET_NONE;
};

#endif // STATE_ETHERNET_STATE_H
