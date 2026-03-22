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

// Apply current config (static or DHCP) — call after modifying ethernet state.
void eth_manager_apply_config();

// Start 60s confirm countdown after applying a static IP config.
void eth_manager_start_confirm_timer();

// Confirm the pending config (clears timer and persists to settings).
void eth_manager_confirm_config();

// Main loop: check if confirm timer has expired and revert to DHCP if so.
void eth_manager_check_revert();

// REST handler: GET /api/ethstatus
void handleEthStatus();

// REST handler: POST /api/ethconfig
void handleEthConfig();

// REST handler: POST /api/ethconfig/confirm
void handleEthConfigConfirm();

#endif // ETH_MANAGER_H
