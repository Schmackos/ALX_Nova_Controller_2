# Test Mocks - Complete Guide

## Overview

This directory contains mock implementations of external dependencies used by ALX Nova Controller. These mocks enable comprehensive unit testing without requiring ESP32 hardware.

## Mock Files

### Arduino.h
**Purpose:** Mock Arduino core functions and types

**Available Functions:**
- `millis()` - Current system time (milliseconds)
- `delay(ms)` - Advance mock time
- `digitalWrite(pin, value)` - Set digital pin state
- `digitalRead(pin)` - Read digital pin state
- `analogRead(pin)` - Read analog value
- `pinMode(pin, mode)` - Configure pin mode (no-op in mock)

**Global Variables:**
- `ArduinoMock::mockMillis` - Current time for testing
- `ArduinoMock::mockAnalogValue` - Analog input value
- `ArduinoMock::mockDigitalPins[50]` - Pin states array

**Usage:**
```cpp
#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#endif

void test_timing() {
    ArduinoMock::mockMillis = 0;
    ArduinoMock::mockAnalogValue = 2000;

    unsigned long now = millis();
    TEST_ASSERT_EQUAL(0, now);

    delay(100);
    now = millis();
    TEST_ASSERT_EQUAL(100, now);
}
```

### Preferences.h
**Purpose:** Mock ESP32 NVS (Non-Volatile Storage)

**Features:**
- Namespace-based storage
- Multiple data types
- Read-only mode support
- Static storage across tests

**API:**
```cpp
Preferences prefs;

// Begin namespace
prefs.begin("namespace_name", false);  // false = read/write

// String operations
prefs.putString("key", "value");
String value = prefs.getString("key", "default");

// Boolean operations
prefs.putBool("key", true);
bool value = prefs.getBool("key", false);

// Integer operations
prefs.putInt("key", 42);
int value = prefs.getInt("key", 0);

// Double/Float operations
prefs.putDouble("key", 3.14);
double value = prefs.getDouble("key", 0.0);

// Key operations
bool exists = prefs.isKey("key");
prefs.remove("key");
prefs.clear();

// End namespace
prefs.end();

// Test utilities
Preferences::reset();     // Clear all storage
Preferences::clearAll();  // Same as reset
```

**Usage:**
```cpp
void test_save_settings() {
    Preferences prefs;
    prefs.begin("settings", false);
    prefs.putString("name", "Device1");
    prefs.putInt("count", 5);
    prefs.end();

    // Later in test
    prefs.begin("settings", true);
    TEST_ASSERT_EQUAL_STRING("Device1",
        prefs.getString("name", "").c_str());
    TEST_ASSERT_EQUAL(5, prefs.getInt("count", 0));
    prefs.end();
}
```

### WiFi.h
**Purpose:** Mock ESP32 WiFi library

**Features:**
- WiFi scanning and connection
- AP mode support
- IP configuration
- Mock network database

**API:**
```cpp
WiFiClass WiFi;  // Global object

// Connection
WiFi.begin("SSID", "password");
WiFi.disconnect();
WiFi.status();  // Returns WL_CONNECTED, WL_DISCONNECTED, etc.

// AP Mode
WiFi.mode(WIFI_STA);           // STA only
WiFi.mode(WIFI_AP);            // AP only
WiFi.mode(WIFI_AP_STA);        // Both
WiFi.softAP("SSID", "password");
WiFi.softAPdisconnect();

// IP Configuration
WiFi.config(local_ip, gateway, subnet, dns1, dns2);
IPAddress ip = WiFi.localIP();
IPAddress gw = WiFi.gatewayIP();
IPAddress subnet = WiFi.subnetMask();

// Scanning
int count = WiFi.scanNetworks();
String ssid = WiFi.SSID(index);
int rssi = WiFi.RSSI(index);

// Identification
String mac = WiFi.macAddress();
String hostname = WiFi.hostname();

// Testing utilities
WiFiClass::reset();           // Reset to defaults
WiFiClass::addMockNetwork("SSID", -50);  // Add scan result
WiFiClass::clearMockNetworks();  // Clear all results
```

**Usage:**
```cpp
void test_wifi_scan() {
    WiFiClass::addMockNetwork("Network1", -50);
    WiFiClass::addMockNetwork("Network2", -75);

    int count = WiFi.scanNetworks();
    TEST_ASSERT_EQUAL(2, count);
    TEST_ASSERT_EQUAL_STRING("Network1", WiFi.SSID(0).c_str());
    TEST_ASSERT_EQUAL(-50, WiFi.RSSI(0));
}
```

### IPAddress.h
**Purpose:** Mock IP address class for network configuration

**Features:**
- Quad notation (192.168.1.1)
- Comparison operators
- String conversion

**API:**
```cpp
// Constructors
IPAddress ip1(192, 168, 1, 1);
IPAddress ip2(192, 168, 1, 100);

// Comparison
bool equal = (ip1 == ip2);
bool different = (ip1 != ip2);

// Access
uint8_t firstOctet = ip1[0];
ip1[0] = 10;

// String representation
std::string str = ip1.toString();  // "192.168.1.1"

// Validation
bool valid = ip1.isValid();
```

**Usage:**
```cpp
void test_ip_config() {
    IPAddress local(192, 168, 1, 100);
    IPAddress gateway(192, 168, 1, 1);

    TEST_ASSERT_EQUAL(192, local[0]);
    TEST_ASSERT_EQUAL(100, local[3]);

    WiFi.config(local, gateway, IPAddress(255, 255, 255, 0));
    TEST_ASSERT_EQUAL_STRING("192.168.1.100",
        WiFi.localIP().toString().c_str());
}
```

### PubSubClient.h
**Purpose:** Mock MQTT client library

**Features:**
- Connection management
- Message publishing and subscription
- Message recording for test verification

**API:**
```cpp
PubSubClient client(wifiClient);  // or WiFiClient mock

// Configuration
client.setServer("mqtt.example.com", 1883);
client.setCallback(callback_function);

// Connection
bool connected = client.connect("device_id");
bool connected = client.connect("device_id", "user", "pass");
client.disconnect();

// Publishing
bool published = client.publish("topic", "payload");
bool published = client.publish("topic", (uint8_t*)data, len);

// Subscription
bool subscribed = client.subscribe("topic");
bool unsubscribed = client.unsubscribe("topic");

// Status
bool connected = client.connected();
int state = client.state();  // 0 = connected, -1 = disconnected

// Loop (for testing)
client.loop();

// Testing utilities
PubSubClient::reset();
bool published = PubSubClient::wasMessagePublished("topic");
std::string msg = PubSubClient::getPublishedMessage("topic");
bool subscribed = PubSubClient::wasTopicSubscribed("topic");
PubSubClient::clearMessages();
PubSubClient::clearSubscriptions();
```

**Usage:**
```cpp
void test_mqtt_publish() {
    PubSubClient client;
    client.setServer("mqtt.example.com", 1883);
    client.connect("device1");

    client.publish("device/led", "on");

    TEST_ASSERT_TRUE(
        PubSubClient::wasMessagePublished("device/led"));
    TEST_ASSERT_EQUAL_STRING("on",
        PubSubClient::getPublishedMessage("device/led").c_str());
}
```

### esp_random.h
**Purpose:** Mock ESP32 random number generation

**Features:**
- Deterministic pseudo-random numbers
- UUID-style random bytes
- Seed control for reproducibility

**API:**
```cpp
uint32_t rand_val = esp_random();
void esp_fill_random(void* buf, size_t len);

// Testing utilities
EspRandomMock::setSeed(seed_value);
EspRandomMock::next();
EspRandomMock::reset();  // Reset to default seed
```

**Usage:**
```cpp
void test_session_id_generation() {
    EspRandomMock::setSeed(12345);  // Reproducible

    uint8_t randomBytes[16];
    esp_fill_random(randomBytes, 16);

    // Should get same bytes next time with same seed
    EspRandomMock::setSeed(12345);
    uint8_t randomBytes2[16];
    esp_fill_random(randomBytes2, 16);

    // Compare for determinism
    TEST_ASSERT_EQUAL_MEMORY(randomBytes, randomBytes2, 16);
}
```

## Resetting State Between Tests

All mocks support reset for test isolation:

```cpp
void setUp(void) {
    // Clear all mock state
    ArduinoMock::reset();
    Preferences::reset();
    WiFiClass::reset();
    PubSubClient::reset();
    EspRandomMock::reset();
}
```

## Using Mocks in Tests

### Basic Pattern
```cpp
#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#include "../test_mocks/Preferences.h"
#else
#include <Arduino.h>
#include <Preferences.h>
#endif

void setUp(void) {
    // Reset mocks
    Preferences::reset();
    ArduinoMock::reset();
}

void test_example(void) {
    // Arrange
    Preferences prefs;
    prefs.begin("test", false);
    prefs.putString("key", "value");
    prefs.end();

    // Act
    prefs.begin("test", true);
    String result = prefs.getString("key", "");
    prefs.end();

    // Assert
    TEST_ASSERT_EQUAL_STRING("value", result.c_str());
}
```

### Testing Time-Dependent Code
```cpp
void test_timeout(void) {
    ArduinoMock::mockMillis = 0;

    unsigned long start = millis();
    TEST_ASSERT_EQUAL(0, start);

    // Advance time
    ArduinoMock::mockMillis = 5000;
    unsigned long elapsed = millis() - start;
    TEST_ASSERT_EQUAL(5000, elapsed);
}
```

### Testing WiFi and MQTT Together
```cpp
void test_mqtt_over_wifi(void) {
    // Setup WiFi
    WiFi.begin("TestSSID", "password");
    TEST_ASSERT_EQUAL(WiFiClass::WL_CONNECTED, WiFi.status());

    // Setup MQTT
    PubSubClient client;
    client.setServer("mqtt.local", 1883);
    bool connected = client.connect("device1");

    TEST_ASSERT_TRUE(connected);
    client.publish("topic", "message");
    TEST_ASSERT_TRUE(
        PubSubClient::wasMessagePublished("topic"));
}
```

## Mock Limitations

**Intentionally Limited Features:**
- No actual network communication
- No real TLS/SSL encryption
- No actual NVS wear-leveling
- No actual ESP32 hardware interaction

**These Limitations Are By Design:**
- Tests run without hardware
- Tests are fast (< 10 seconds total)
- Tests are deterministic and reproducible
- Tests don't depend on network availability

## Adding New Mocks

To mock a new dependency:

1. Create header file: `test/test_mocks/NewLibrary.h`
2. Implement class with same API as real library
3. Add static storage/state
4. Implement `reset()` method
5. Include conditional compilation guards:
   ```cpp
   #ifdef NATIVE_TEST
   #include "../test_mocks/NewLibrary.h"
   #else
   #include <NewLibrary.h>
   #endif
   ```

## Testing Strategies

### Isolation
Each test is independent. Use `setUp()` to reset all mocks.

### State Verification
Check internal mock state to verify behavior:
```cpp
// Verify WiFi scanned
int count = WiFi.scanNetworks();
TEST_ASSERT_GREATER_THAN(0, count);

// Verify MQTT published
TEST_ASSERT_TRUE(
    PubSubClient::wasMessagePublished("topic"));

// Verify settings saved
Preferences prefs;
prefs.begin("settings", true);
TEST_ASSERT_TRUE(prefs.isKey("device_name"));
prefs.end();
```

### Edge Cases
Mocks help test edge cases that are hard on real hardware:
- NVS full (reject writes)
- WiFi signal very weak (RSSI -100)
- MQTT disconnects during publish
- Random session ID generation
- Pin bounce simulation

## Debugging Mocks

Print mock state when tests fail:
```cpp
void test_debug(void) {
    ArduinoMock::mockMillis = 5000;
    printf("Current time: %lu\n", millis());

    Preferences prefs;
    prefs.begin("test", false);
    prefs.putString("key", "value");
    // Check storage directly
    printf("Storage has %zu namespaces\n",
        Preferences::storage.size());
    prefs.end();
}
```

## Performance

All mocks are optimized for test speed:
- Preferences uses in-memory map (no file I/O)
- WiFi scan returns instant mock data
- MQTT operations are O(1) to O(n)
- Random generation uses simple LCG

Total test execution: < 10 seconds for 106 tests

## Version Compatibility

Mocks are compatible with:
- Arduino SDK ≥ 2.0
- ESP32 core ≥ 2.0
- PlatformIO native platform
- Unity test framework

## Resources

- Mock examples: See `test/test_*/test_*.cpp`
- Unity assertions: http://www.throwtheswitch.org/unity
- PlatformIO testing: https://docs.platformio.org/en/latest/advanced/test-framework.html
