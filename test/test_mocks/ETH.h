#ifndef ETH_MOCK_H
#define ETH_MOCK_H

#include "Arduino.h"

// PHY type constants
#define ETH_PHY_TLK110 0
#define ETH_PHY_IP101  ETH_PHY_TLK110
typedef int eth_phy_type_t;
typedef int eth_clock_mode_t;
#define EMAC_CLK_EXT_IN 0

// Default pin defines (match P4 variant)
#ifndef ETH_PHY_TYPE
#define ETH_PHY_TYPE ETH_PHY_TLK110
#endif
#ifndef ETH_PHY_ADDR
#define ETH_PHY_ADDR 1
#endif
#ifndef ETH_PHY_MDC
#define ETH_PHY_MDC 31
#endif
#ifndef ETH_PHY_MDIO
#define ETH_PHY_MDIO 52
#endif
#ifndef ETH_PHY_POWER
#define ETH_PHY_POWER 51
#endif
#ifndef ETH_CLK_MODE
#define ETH_CLK_MODE EMAC_CLK_EXT_IN
#endif

// Minimal IP address mock
class IPAddressMock {
public:
    String toString() const { return "192.168.1.100"; }
};

// Mock Ethernet class
class ETHClass {
public:
    bool begin() { return true; }
    bool linkUp() const { return _linkUp; }
    bool connected() const { return _connected; }
    bool hasIP() const { return _hasIP; }
    IPAddressMock localIP() const { return IPAddressMock(); }
    uint16_t linkSpeed() const { return 100; }
    bool fullDuplex() const { return true; }
    bool setHostname(const char*) { return true; }
    bool setDefault() { return true; }

    // Test helpers — set these directly in tests
    bool _linkUp = false;
    bool _connected = false;
    bool _hasIP = false;

    static void reset() {
        // Instance fields reset via individual test helper assignments
    }
};

// Global ETH instance
static ETHClass ETH;

// Arduino event IDs for Ethernet
#ifndef ARDUINO_EVENT_ETH_START
#define ARDUINO_EVENT_ETH_START       20
#define ARDUINO_EVENT_ETH_CONNECTED   21
#define ARDUINO_EVENT_ETH_GOT_IP      22
#define ARDUINO_EVENT_ETH_LOST_IP     23
#define ARDUINO_EVENT_ETH_DISCONNECTED 24
#define ARDUINO_EVENT_ETH_STOP        25
#endif

#endif // ETH_MOCK_H
