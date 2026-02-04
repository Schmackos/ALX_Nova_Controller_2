#ifndef WIFI_MOCK_H
#define WIFI_MOCK_H

#include "Arduino.h"
#include "IPAddress.h"
#include <cstring>
#include <string>
#include <vector>

// Forward declare IPAddress
class IPAddress;

// Mock WiFi network scan result
struct WiFiNetwork {
  std::string ssid;
  int rssi;
  uint8_t channel;
  bool encrypted;
};

// Mock WiFi class
class WiFiClass {
public:
  enum Status {
    WL_IDLE_STATUS = 0,
    WL_NO_SSID_AVAIL = 1,
    WL_SCAN_COMPLETED = 2,
    WL_CONNECTED = 3,
    WL_CONNECT_FAILED = 4,
    WL_CONNECTION_LOST = 5,
    WL_DISCONNECTED = 6,
    WL_AP_LISTENING = 7
  };

  // Scan results
  static std::vector<WiFiNetwork> mockScanResults;
  static int lastStatusCode;
  static std::string connectedSSID;
  static bool apModeActive;
  static IPAddress mockLocalIP;
  static IPAddress mockGateway;
  static IPAddress mockSubnet;
  static IPAddress mockAPIP;
  static int mockRSSI;

  // Core functions
  int status() { return lastStatusCode; }

  bool begin(const char *ssid, const char *password = nullptr) {
    if (!ssid)
      return false;
    connectedSSID = ssid;
    lastStatusCode = WL_CONNECTED;
    return true;
  }

  bool disconnect(bool turnOffWiFi = false) {
    connectedSSID.clear();
    lastStatusCode = WL_DISCONNECTED;
    return true;
  }

  bool mode(uint8_t m) {
    // m = 1 for STA, 2 for AP, 3 for STA+AP
    apModeActive = (m & 2) != 0;
    return true;
  }

  // AP mode functions
  bool softAP(const char *ssid, const char *password = nullptr) {
    apModeActive = true;
    lastStatusCode = WL_AP_LISTENING;
    return true;
  }

  bool softAPdisconnect(bool turnOffWiFi = false) {
    apModeActive = false;
    return true;
  }

  // IP configuration
  bool config(IPAddress local_ip, IPAddress gateway, IPAddress subnet,
              IPAddress dns1 = IPADDR_NONE, IPAddress dns2 = IPADDR_NONE) {
    mockLocalIP = local_ip;
    mockGateway = gateway;
    mockSubnet = subnet;
    return true;
  }

  IPAddress localIP() { return mockLocalIP; }

  IPAddress gatewayIP() { return mockGateway; }

  IPAddress subnetMask() { return mockSubnet; }

  IPAddress softAPIP() { return mockAPIP; }

  String SSID() { return String(connectedSSID.c_str()); }

  int RSSI() { return mockRSSI; }

  // Scanning
  int scanNetworks() { return mockScanResults.size(); }

  String SSID(uint8_t networkItem) {
    if (networkItem < mockScanResults.size()) {
      return String(mockScanResults[networkItem].ssid.c_str());
    }
    return "";
  }

  int RSSI(uint8_t networkItem) {
    if (networkItem < mockScanResults.size()) {
      return mockScanResults[networkItem].rssi;
    }
    return 0;
  }

  uint8_t channel(uint8_t networkItem) {
    if (networkItem < mockScanResults.size()) {
      return mockScanResults[networkItem].channel;
    }
    return 0;
  }

  bool encryptionType(uint8_t networkItem) {
    if (networkItem < mockScanResults.size()) {
      return mockScanResults[networkItem].encrypted;
    }
    return false;
  }

  String macAddress() { return "AA:BB:CC:DD:EE:FF"; }

  String hostname() { return "esp32-nova"; }

  // Static methods to reset for tests
  static void reset() {
    mockScanResults.clear();
    lastStatusCode = WL_IDLE_STATUS;
    connectedSSID.clear();
    apModeActive = false;
    mockLocalIP = IPAddress(192, 168, 1, 100);
    mockGateway = IPAddress(192, 168, 1, 1);
    mockSubnet = IPAddress(255, 255, 255, 0);
    mockAPIP = IPAddress(192, 168, 4, 1);
    mockRSSI = -50;
  }

  static void addMockNetwork(const char *ssid, int rssi, uint8_t channel = 1,
                             bool encrypted = true) {
    mockScanResults.push_back({std::string(ssid), rssi, channel, encrypted});
  }

  static void clearMockNetworks() { mockScanResults.clear(); }
};

// Static member initialization
std::vector<WiFiNetwork> WiFiClass::mockScanResults;
int WiFiClass::lastStatusCode = WiFiClass::WL_IDLE_STATUS;
std::string WiFiClass::connectedSSID;
bool WiFiClass::apModeActive = false;
IPAddress WiFiClass::mockLocalIP(192, 168, 1, 100);
IPAddress WiFiClass::mockGateway(192, 168, 1, 1);
IPAddress WiFiClass::mockSubnet(255, 255, 255, 0);
IPAddress WiFiClass::mockAPIP(192, 168, 4, 1);
int WiFiClass::mockRSSI = -50;

// Global WiFi object
static WiFiClass WiFi;

#endif // WIFI_MOCK_H
