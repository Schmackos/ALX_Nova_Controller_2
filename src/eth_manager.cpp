#include "eth_manager.h"
#include "app_state.h"
#include "app_events.h"
#include "debug_serial.h"
#include "settings_manager.h"
#include "http_security.h"
#include "globals.h"

#if CONFIG_IDF_TARGET_ESP32P4

#include <ETH.h>
#include <Network.h>
#include <ArduinoJson.h>

static bool _ethStarted = false;

// ===== Static helper: apply static IP config via ETH.config() =====
static bool eth_apply_static_config() {
    if (!appState.ethernet.useStaticIP || appState.ethernet.staticIP[0] == '\0') return false;
    IPAddress ip, gw, sn, d1, d2;
    if (!ip.fromString(appState.ethernet.staticIP)) {
        LOG_E("[ETH] Invalid static IP: %s", appState.ethernet.staticIP);
        return false;
    }
    if (!sn.fromString(appState.ethernet.staticSubnet)) sn = IPAddress(255, 255, 255, 0);
    if (appState.ethernet.staticGateway[0] != '\0') gw.fromString(appState.ethernet.staticGateway);
    if (appState.ethernet.staticDns1[0] != '\0') d1.fromString(appState.ethernet.staticDns1);
    if (appState.ethernet.staticDns2[0] != '\0') d2.fromString(appState.ethernet.staticDns2);
    if (!ETH.config(ip, gw, sn, d1, d2)) {
        LOG_E("[ETH] ETH.config() failed");
        return false;
    }
    LOG_I("[ETH] Static IP configured: %s", appState.ethernet.staticIP);
    return true;
}

// ===== Event handler =====
static void eth_event_handler(arduino_event_id_t event) {
    switch (event) {
        case ARDUINO_EVENT_ETH_START:
            LOG_I("[ETH] Started");
            ETH.setHostname(appState.ethernet.hostname);
            strlcpy(appState.ethernet.mac, ETH.macAddress().c_str(), sizeof(appState.ethernet.mac));
            _ethStarted = true;
            if (appState.ethernet.useStaticIP) {
                eth_apply_static_config();
            }
            break;
        case ARDUINO_EVENT_ETH_CONNECTED:
            LOG_I("[ETH] Link up (%dMbps %s)", ETH.linkSpeed(),
                  ETH.fullDuplex() ? "Full Duplex" : "Half Duplex");
            appState.ethernet.linkUp = true;
            appState.ethernet.speed = ETH.linkSpeed();
            appState.ethernet.fullDuplex = ETH.fullDuplex();
            appState.markEthernetDirty();
            break;
        case ARDUINO_EVENT_ETH_GOT_IP:
            LOG_I("[ETH] Got IP: %s", ETH.localIP().toString().c_str());
            appState.ethernet.connected = true;
            strlcpy(appState.ethernet.ip, ETH.localIP().toString().c_str(), sizeof(appState.ethernet.ip));
            strlcpy(appState.ethernet.gateway, ETH.gatewayIP().toString().c_str(), sizeof(appState.ethernet.gateway));
            strlcpy(appState.ethernet.subnet, ETH.subnetMask().toString().c_str(), sizeof(appState.ethernet.subnet));
            strlcpy(appState.ethernet.dns1, ETH.dnsIP(0).toString().c_str(), sizeof(appState.ethernet.dns1));
            strlcpy(appState.ethernet.dns2, ETH.dnsIP(1).toString().c_str(), sizeof(appState.ethernet.dns2));
            appState.ethernet.activeInterface = NET_ETHERNET;
            appState.markEthernetDirty();
            // Make Ethernet the default route when it gets IP
            ETH.setDefault();
            break;
        case ARDUINO_EVENT_ETH_LOST_IP:
            LOG_W("[ETH] Lost IP");
            appState.ethernet.connected = false;
            appState.ethernet.ip[0] = '\0';
            appState.ethernet.gateway[0] = '\0';
            appState.ethernet.subnet[0] = '\0';
            appState.ethernet.dns1[0] = '\0';
            appState.ethernet.dns2[0] = '\0';
            appState.ethernet.speed = 0;
            appState.ethernet.fullDuplex = false;
            if (appState.ethernet.activeInterface == NET_ETHERNET) {
                appState.ethernet.activeInterface = appState.wifi.connectSuccess ? NET_WIFI : NET_NONE;
            }
            appState.markEthernetDirty();
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            LOG_W("[ETH] Link down");
            appState.ethernet.linkUp = false;
            appState.ethernet.connected = false;
            appState.ethernet.ip[0] = '\0';
            appState.ethernet.gateway[0] = '\0';
            appState.ethernet.subnet[0] = '\0';
            appState.ethernet.dns1[0] = '\0';
            appState.ethernet.dns2[0] = '\0';
            appState.ethernet.speed = 0;
            appState.ethernet.fullDuplex = false;
            if (appState.ethernet.activeInterface == NET_ETHERNET) {
                appState.ethernet.activeInterface = appState.wifi.connectSuccess ? NET_WIFI : NET_NONE;
            }
            appState.markEthernetDirty();
            break;
        case ARDUINO_EVENT_ETH_STOP:
            LOG_I("[ETH] Stopped");
            _ethStarted = false;
            appState.ethernet.linkUp = false;
            appState.ethernet.connected = false;
            appState.ethernet.ip[0] = '\0';
            break;
        default:
            break;
    }
}

// ===== Public API =====

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
    return appState.ethernet.connected;
}

bool eth_manager_link_up() {
    return appState.ethernet.linkUp;
}

const char* eth_manager_get_ip() {
    return appState.ethernet.ip;
}

void eth_manager_set_default_route() {
    if (_ethStarted && ETH.hasIP()) {
        ETH.setDefault();
    }
}

void eth_manager_apply_config() {
    if (!_ethStarted) return;
    ETH.setHostname(appState.ethernet.hostname);
    if (appState.ethernet.useStaticIP) {
        eth_apply_static_config();
    } else {
        ETH.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
        LOG_I("[ETH] DHCP enabled");
    }
}

void eth_manager_start_confirm_timer() {
    appState.ethernet.pendingConfirm = true;
    appState.ethernet.confirmDeadline = millis();  // store START time (overflow-safe)
    appState.markEthernetDirty();
    LOG_I("[ETH] Confirm timer started (%d ms)", ETH_CONFIRM_TIMEOUT_MS);
}

void eth_manager_confirm_config() {
    appState.ethernet.pendingConfirm = false;
    appState.ethernet.confirmDeadline = 0;
    saveSettings();
    appState.markEthernetDirty();
    LOG_I("[ETH] Configuration confirmed and saved");
}

void eth_manager_check_revert() {
    if (!appState.ethernet.pendingConfirm) return;
    if ((millis() - appState.ethernet.confirmDeadline) < ETH_CONFIRM_TIMEOUT_MS) return;

    LOG_W("[ETH] Config confirmation timeout — reverting to DHCP");
    appState.ethernet.useStaticIP = false;
    appState.ethernet.staticIP[0] = '\0';
    strlcpy(appState.ethernet.staticSubnet, "255.255.255.0", sizeof(appState.ethernet.staticSubnet));
    appState.ethernet.staticGateway[0] = '\0';
    appState.ethernet.staticDns1[0] = '\0';
    appState.ethernet.staticDns2[0] = '\0';
    appState.ethernet.pendingConfirm = false;
    appState.ethernet.confirmDeadline = 0;
    eth_manager_apply_config();
    appState.markEthernetDirty();
}

// ===== REST: GET /api/ethstatus =====
void handleEthStatus() {
    JsonDocument doc;
    doc["linkUp"]        = appState.ethernet.linkUp;
    doc["connected"]     = appState.ethernet.connected;
    doc["ip"]            = appState.ethernet.ip;
    doc["mac"]           = appState.ethernet.mac;
    doc["speed"]         = appState.ethernet.speed;
    doc["fullDuplex"]    = appState.ethernet.fullDuplex;
    doc["gateway"]       = appState.ethernet.gateway;
    doc["subnet"]        = appState.ethernet.subnet;
    doc["dns1"]          = appState.ethernet.dns1;
    doc["dns2"]          = appState.ethernet.dns2;
    doc["hostname"]      = appState.ethernet.hostname;
    doc["useStaticIP"]   = appState.ethernet.useStaticIP;
    doc["staticIP"]      = appState.ethernet.staticIP;
    doc["staticSubnet"]  = appState.ethernet.staticSubnet;
    doc["staticGateway"] = appState.ethernet.staticGateway;
    doc["staticDns1"]    = appState.ethernet.staticDns1;
    doc["staticDns2"]    = appState.ethernet.staticDns2;
    doc["activeInterface"] = (int)appState.ethernet.activeInterface;
    doc["pendingConfirm"]  = appState.ethernet.pendingConfirm;

    String output;
    serializeJson(doc, output);
    server_send(200, "application/json", output);
}

// ===== REST: POST /api/ethconfig =====
void handleEthConfig() {
    String body = server.arg("plain");
    JsonDocument doc;
    if (deserializeJson(doc, body) != DeserializationError::Ok) {
        server_send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    bool configChanged = false;
    bool staticIpChanged = false;

    // Hostname validation: 1-63 chars, [a-zA-Z0-9-], no leading/trailing hyphen
    if (doc["hostname"].is<const char*>()) {
        String newHostname = doc["hostname"].as<const char*>();
        bool valid = newHostname.length() >= 1 && newHostname.length() <= 63;
        if (valid && (newHostname[0] == '-' || newHostname[newHostname.length() - 1] == '-')) {
            valid = false;
        }
        if (valid) {
            for (unsigned int i = 0; i < newHostname.length(); i++) {
                char c = newHostname[i];
                if (!isalnum(c) && c != '-') { valid = false; break; }
            }
        }
        if (!valid) {
            server_send(400, "application/json", "{\"error\":\"Invalid hostname\"}");
            return;
        }
        strlcpy(appState.ethernet.hostname, newHostname.c_str(), sizeof(appState.ethernet.hostname));
        configChanged = true;
    }

    // Static IP config
    if (doc["useStaticIP"].is<bool>()) {
        bool wantStatic = doc["useStaticIP"].as<bool>();
        if (wantStatic) {
            // Validate required fields
            if (!doc["staticIP"].is<const char*>() ||
                !doc["staticSubnet"].is<const char*>() ||
                !doc["staticGateway"].is<const char*>()) {
                server_send(400, "application/json",
                            "{\"error\":\"staticIP, staticSubnet and staticGateway required\"}");
                return;
            }
            IPAddress testIP, testSN, testGW;
            if (!testIP.fromString(doc["staticIP"].as<const char*>())) {
                server_send(400, "application/json", "{\"error\":\"Invalid staticIP\"}");
                return;
            }
            if (!testSN.fromString(doc["staticSubnet"].as<const char*>())) {
                server_send(400, "application/json", "{\"error\":\"Invalid staticSubnet\"}");
                return;
            }
            if (!testGW.fromString(doc["staticGateway"].as<const char*>())) {
                server_send(400, "application/json", "{\"error\":\"Invalid staticGateway\"}");
                return;
            }
            // Optional DNS fields
            if (doc["staticDns1"].is<const char*>()) {
                IPAddress testD1;
                if (!testD1.fromString(doc["staticDns1"].as<const char*>())) {
                    server_send(400, "application/json", "{\"error\":\"Invalid staticDns1\"}");
                    return;
                }
                strlcpy(appState.ethernet.staticDns1, doc["staticDns1"].as<const char*>(), sizeof(appState.ethernet.staticDns1));
            }
            if (doc["staticDns2"].is<const char*>()) {
                IPAddress testD2;
                if (!testD2.fromString(doc["staticDns2"].as<const char*>())) {
                    server_send(400, "application/json", "{\"error\":\"Invalid staticDns2\"}");
                    return;
                }
                strlcpy(appState.ethernet.staticDns2, doc["staticDns2"].as<const char*>(), sizeof(appState.ethernet.staticDns2));
            }
            appState.ethernet.useStaticIP = true;
            strlcpy(appState.ethernet.staticIP,      doc["staticIP"].as<const char*>(),      sizeof(appState.ethernet.staticIP));
            strlcpy(appState.ethernet.staticSubnet,  doc["staticSubnet"].as<const char*>(),  sizeof(appState.ethernet.staticSubnet));
            strlcpy(appState.ethernet.staticGateway, doc["staticGateway"].as<const char*>(), sizeof(appState.ethernet.staticGateway));
            staticIpChanged = true;
        } else {
            appState.ethernet.useStaticIP = false;
        }
        configChanged = true;
    }

    eth_manager_apply_config();
    appState.markEthernetDirty();

    if (staticIpChanged) {
        eth_manager_start_confirm_timer();
        server_send(200, "application/json", "{\"success\":true,\"pendingConfirm\":true}");
    } else {
        if (configChanged) saveSettings();
        server_send(200, "application/json", "{\"success\":true}");
    }
}

// ===== REST: POST /api/ethconfig/confirm =====
void handleEthConfigConfirm() {
    eth_manager_confirm_config();
    server_send(200, "application/json", "{\"success\":true}");
}

#else // !CONFIG_IDF_TARGET_ESP32P4

// Stubs for non-P4 targets (S3, native tests)
void eth_manager_init() {}
bool eth_manager_is_connected() { return false; }
bool eth_manager_link_up() { return false; }
const char* eth_manager_get_ip() { return ""; }
void eth_manager_set_default_route() {}
void eth_manager_apply_config() {}
void eth_manager_start_confirm_timer() {}
void eth_manager_confirm_config() {}
void eth_manager_check_revert() {}
void handleEthStatus() {}
void handleEthConfig() {}
void handleEthConfigConfirm() {}

#endif // CONFIG_IDF_TARGET_ESP32P4
