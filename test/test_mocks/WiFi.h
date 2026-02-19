#ifndef WIFI_MOCK_H
#define WIFI_MOCK_H

#include "Arduino.h"
#include "IPAddress.h"
#include <cstring>
#include <string>
#include <vector>

// Forward declare IPAddress
class IPAddress;

// Scan status constants
#define WIFI_SCAN_RUNNING (-1)
#define WIFI_SCAN_FAILED  (-2)

// Mock WiFi network scan result
struct WiFiNetwork {
  std::string ssid;
  int rssi;
  uint8_t channel;
  bool encrypted;
  uint8_t bssid[6];
};

// WiFi mode constants
#define WIFI_MODE_NULL  0
#define WIFI_MODE_STA   1
#define WIFI_MODE_AP    2
#define WIFI_MODE_APSTA 3
#define WIFI_AP         WIFI_MODE_AP
#define WIFI_STA        WIFI_MODE_STA
#define WIFI_AP_STA     WIFI_MODE_APSTA

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
  static int mockScanComplete;
  static bool mockWifiBeginCalled;
  static uint8_t mockCurrentMode;

  // Core functions
  int status() { return lastStatusCode; }

  bool begin(const char *ssid, const char *password = nullptr) {
    if (!ssid)
      return false;
    connectedSSID = ssid;
    lastStatusCode = WL_CONNECTED;
    mockWifiBeginCalled = true;
    return true;
  }

  bool begin(const char *ssid, const char *password, int32_t channel,
             const uint8_t *bssid) {
    if (!ssid)
      return false;
    connectedSSID = ssid;
    lastStatusCode = WL_CONNECTED;
    mockWifiBeginCalled = true;
    return true;
  }

  bool disconnect(bool turnOffWiFi = false) {
    connectedSSID.clear();
    lastStatusCode = WL_DISCONNECTED;
    return true;
  }

  bool mode(uint8_t m) {
    mockCurrentMode = m;
    apModeActive = (m & 2) != 0;
    return true;
  }

  uint8_t getMode() { return mockCurrentMode; }

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
  int scanNetworks() { return (int)mockScanResults.size(); }

  int scanNetworks(bool async, bool show_hidden) {
    if (mockScanComplete == WIFI_SCAN_FAILED) {
      return WIFI_SCAN_FAILED;
    }
    return WIFI_SCAN_RUNNING;
  }

  int scanComplete() { return mockScanComplete; }

  void scanDelete() { /* no-op */ }

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

  int32_t channel(uint8_t networkItem) {
    if (networkItem < mockScanResults.size()) {
      return mockScanResults[networkItem].channel;
    }
    return 1;
  }

  uint8_t *BSSID(uint8_t networkItem) {
    static uint8_t mockBssid[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    if (networkItem < mockScanResults.size()) {
      return mockScanResults[networkItem].bssid;
    }
    return mockBssid;
  }

  bool encryptionType(uint8_t networkItem) {
    if (networkItem < mockScanResults.size()) {
      return mockScanResults[networkItem].encrypted;
    }
    return false;
  }

  String macAddress() { return "AA:BB:CC:DD:EE:FF"; }

  String hostname() { return "esp32-nova"; }

  int softAPgetStationNum() { return 0; }

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
    mockScanComplete = WIFI_SCAN_FAILED;
    mockWifiBeginCalled = false;
    mockCurrentMode = WIFI_STA;
  }

  static void addMockNetwork(const char *ssid, int rssi, uint8_t channel = 1,
                             bool encrypted = true) {
    WiFiNetwork net;
    net.ssid = std::string(ssid);
    net.rssi = rssi;
    net.channel = channel;
    net.encrypted = encrypted;
    std::memset(net.bssid, 0, 6);
    net.bssid[0] = 0xAA;
    net.bssid[1] = 0xBB;
    net.bssid[2] = 0xCC;
    net.bssid[3] = 0xDD;
    net.bssid[4] = 0xEE;
    net.bssid[5] = 0xFF;
    mockScanResults.push_back(net);
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
int WiFiClass::mockScanComplete = WIFI_SCAN_FAILED;
bool WiFiClass::mockWifiBeginCalled = false;
uint8_t WiFiClass::mockCurrentMode = WIFI_STA;

// Global WiFi object
static WiFiClass WiFi;

// Global-scope WiFi status aliases (matching real Arduino WiFi library)
static const int WL_IDLE_STATUS    = WiFiClass::WL_IDLE_STATUS;
static const int WL_NO_SSID_AVAIL  = WiFiClass::WL_NO_SSID_AVAIL;
static const int WL_SCAN_COMPLETED = WiFiClass::WL_SCAN_COMPLETED;
static const int WL_CONNECTED      = WiFiClass::WL_CONNECTED;
static const int WL_CONNECT_FAILED = WiFiClass::WL_CONNECT_FAILED;
static const int WL_CONNECTION_LOST = WiFiClass::WL_CONNECTION_LOST;
static const int WL_DISCONNECTED   = WiFiClass::WL_DISCONNECTED;
static const int WL_AP_LISTENING   = WiFiClass::WL_AP_LISTENING;

#endif // WIFI_MOCK_H
