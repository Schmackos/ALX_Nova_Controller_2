#ifndef ETH_MANAGER_H
#define ETH_MANAGER_H

#ifndef NATIVE_TEST
#include <Arduino.h>
#endif

// Initialize Ethernet hardware (EMAC + PHY).
// Call once from setup(). Only compiles on P4 targets.
void eth_manager_init();

// Check if Ethernet has an IP address and is ready for traffic.
bool eth_manager_is_connected();

// Check if physical link is up (cable plugged in).
bool eth_manager_link_up();

// Get the Ethernet IP address as a string (empty if no IP).
String eth_manager_get_ip();

// Make Ethernet the default route (when link is up and has IP).
void eth_manager_set_default_route();

#endif // ETH_MANAGER_H
