#include "eth_manager.h"
#include "app_state.h"
#include "app_events.h"
#include "debug_serial.h"

#if CONFIG_IDF_TARGET_ESP32P4

#include <ETH.h>
#include <Network.h>

static bool _ethStarted = false;

static void eth_event_handler(arduino_event_id_t event) {
    switch (event) {
        case ARDUINO_EVENT_ETH_START:
            LOG_I("[ETH] Started");
            ETH.setHostname("alx-nova");
            _ethStarted = true;
            break;
        case ARDUINO_EVENT_ETH_CONNECTED:
            LOG_I("[ETH] Link up (%dMbps %s)", ETH.linkSpeed(),
                  ETH.fullDuplex() ? "Full Duplex" : "Half Duplex");
            appState.ethLinkUp = true;
            appState.markEthernetDirty();
            app_events_signal(EVT_ETHERNET);
            break;
        case ARDUINO_EVENT_ETH_GOT_IP:
            LOG_I("[ETH] Got IP: %s", ETH.localIP().toString().c_str());
            appState.ethConnected = true;
            appState.ethIP = ETH.localIP().toString();
            appState.activeInterface = NET_ETHERNET;
            appState.markEthernetDirty();
            app_events_signal(EVT_ETHERNET);
            // Make Ethernet the default route when it gets IP
            ETH.setDefault();
            break;
        case ARDUINO_EVENT_ETH_LOST_IP:
            LOG_W("[ETH] Lost IP");
            appState.ethConnected = false;
            appState.ethIP = "";
            if (appState.activeInterface == NET_ETHERNET) {
                appState.activeInterface = appState.wifiConnectSuccess ? NET_WIFI : NET_NONE;
            }
            appState.markEthernetDirty();
            app_events_signal(EVT_ETHERNET);
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            LOG_W("[ETH] Link down");
            appState.ethLinkUp = false;
            appState.ethConnected = false;
            appState.ethIP = "";
            if (appState.activeInterface == NET_ETHERNET) {
                appState.activeInterface = appState.wifiConnectSuccess ? NET_WIFI : NET_NONE;
            }
            appState.markEthernetDirty();
            app_events_signal(EVT_ETHERNET);
            break;
        case ARDUINO_EVENT_ETH_STOP:
            LOG_I("[ETH] Stopped");
            _ethStarted = false;
            appState.ethLinkUp = false;
            appState.ethConnected = false;
            appState.ethIP = "";
            break;
        default:
            break;
    }
}

void eth_manager_init() {
    Network.onEvent(eth_event_handler);
    if (!ETH.begin()) {
        LOG_E("[ETH] Failed to start Ethernet");
        return;
    }
    LOG_I("[ETH] Initializing (PHY addr=%d, MDC=%d, MDIO=%d)",
          ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO);
}

bool eth_manager_is_connected() {
    return appState.ethConnected;
}

bool eth_manager_link_up() {
    return appState.ethLinkUp;
}

String eth_manager_get_ip() {
    return appState.ethIP;
}

void eth_manager_set_default_route() {
    if (_ethStarted && ETH.hasIP()) {
        ETH.setDefault();
    }
}

#else // !CONFIG_IDF_TARGET_ESP32P4

// Stubs for non-P4 targets (S3, native tests)
void eth_manager_init() {}
bool eth_manager_is_connected() { return false; }
bool eth_manager_link_up() { return false; }
String eth_manager_get_ip() { return ""; }
void eth_manager_set_default_route() {}

#endif // CONFIG_IDF_TARGET_ESP32P4
