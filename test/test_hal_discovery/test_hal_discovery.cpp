#include <unity.h>
#include <cstring>
#include <cstdlib>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Inline HAL implementations for native testing =====
#include "../../src/hal/hal_types.h"
#include "../../src/hal/hal_device.h"
#include "../test_mocks/Preferences.h"
#include "../test_mocks/LittleFS.h"
#include "../../src/diag_journal.cpp"
#include "../../src/hal/hal_device_manager.cpp"
#include "../../src/hal/hal_driver_registry.cpp"

// Device DB needs to be inline too — but skip LittleFS parts
#define HAL_DB_MAX_ENTRIES 16
#define HAL_DB_FILE_PATH "/hal_devices.json"
#define HAL_CONFIG_FILE_PATH "/hal_config.json"

static HalDeviceDescriptor _testDb[HAL_DB_MAX_ENTRIES];
static int _testDbCount = 0;

void hal_db_reset() { _testDbCount = 0; memset(_testDb, 0, sizeof(_testDb)); }
void hal_db_init() { hal_db_reset(); }
int hal_db_count() { return _testDbCount; }
const HalDeviceDescriptor* hal_db_get(int idx) {
    return (idx >= 0 && idx < _testDbCount) ? &_testDb[idx] : nullptr;
}
bool hal_db_lookup(const char* c, HalDeviceDescriptor* out) {
    if (!c) return false;
    for (int i = 0; i < _testDbCount; i++) {
        if (strcmp(_testDb[i].compatible, c) == 0) {
            if (out) *out = _testDb[i];
            return true;
        }
    }
    return false;
}
bool hal_db_add(const HalDeviceDescriptor* d) {
    if (!d || d->compatible[0] == '\0' || _testDbCount >= HAL_DB_MAX_ENTRIES) return false;
    for (int i = 0; i < _testDbCount; i++) {
        if (strcmp(_testDb[i].compatible, d->compatible) == 0) {
            _testDb[i] = *d;
            return true;
        }
    }
    _testDb[_testDbCount++] = *d;
    return true;
}
bool hal_db_remove(const char* c) {
    if (!c) return false;
    for (int i = 0; i < _testDbCount; i++) {
        if (strcmp(_testDb[i].compatible, c) == 0) {
            for (int j = i; j < _testDbCount - 1; j++) _testDb[j] = _testDb[j + 1];
            _testDbCount--;
            return true;
        }
    }
    return false;
}
bool hal_db_save() { return true; }

// ===== YAML parser (inline from hal_online_fetch.cpp) =====
static bool yaml_get_value(const char* line, const char* key, char* out, int maxLen) {
    int keyLen = strlen(key);
    if (strncmp(line, key, keyLen) != 0) return false;
    const char* p = line + keyLen;
    if (*p != ':') return false;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '"') {
        p++;
        const char* end = strchr(p, '"');
        if (end) {
            int len = end - p;
            if (len >= maxLen) len = maxLen - 1;
            memcpy(out, p, len);
            out[len] = '\0';
            return true;
        }
    }
    int len = strlen(p);
    while (len > 0 && (p[len-1] == '\n' || p[len-1] == '\r' || p[len-1] == ' ')) len--;
    if (len >= maxLen) len = maxLen - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    return true;
}

bool hal_parse_device_yaml(const char* yamlText, int len, HalDeviceDescriptor* out) {
    if (!yamlText || !out || len <= 0) return false;
    memset(out, 0, sizeof(HalDeviceDescriptor));
    char buf[64];
    bool hasCompatible = false;
    const char* p = yamlText;
    const char* end = yamlText + len;
    while (p < end) {
        const char* eol = p;
        while (eol < end && *eol != '\n') eol++;
        int lineLen = eol - p;
        if (lineLen > 0 && lineLen < 256) {
            char line[256];
            memcpy(line, p, lineLen);
            line[lineLen] = '\0';
            if (line[0] != '#' && line[0] != '\0') {
                if (yaml_get_value(line, "compatible", buf, sizeof(buf))) {
                    strncpy(out->compatible, buf, 31); hasCompatible = true;
                }
                else if (yaml_get_value(line, "name", buf, sizeof(buf)))
                    strncpy(out->name, buf, 32);
                else if (yaml_get_value(line, "manufacturer", buf, sizeof(buf)))
                    strncpy(out->manufacturer, buf, 32);
                else if (yaml_get_value(line, "device_type", buf, sizeof(buf))) {
                    if (strcmp(buf, "DAC") == 0) out->type = HAL_DEV_DAC;
                    else if (strcmp(buf, "ADC") == 0) out->type = HAL_DEV_ADC;
                    else if (strcmp(buf, "CODEC") == 0) out->type = HAL_DEV_CODEC;
                    else if (strcmp(buf, "AMP") == 0) out->type = HAL_DEV_AMP;
                }
                else if (yaml_get_value(line, "i2c_default_address", buf, sizeof(buf)))
                    out->i2cAddr = (uint8_t)strtol(buf, nullptr, 0);
                else if (yaml_get_value(line, "channel_count", buf, sizeof(buf)))
                    out->channelCount = (uint8_t)atoi(buf);
                else if (yaml_get_value(line, "cap_hw_volume", buf, sizeof(buf))) {
                    if (strcmp(buf, "true") == 0) out->capabilities |= HAL_CAP_HW_VOLUME;
                }
                else if (yaml_get_value(line, "cap_hw_mute", buf, sizeof(buf))) {
                    if (strcmp(buf, "true") == 0) out->capabilities |= HAL_CAP_MUTE;
                }
            }
        }
        p = eol + 1;
    }
    return hasCompatible;
}

// ===== Wire Mock for I2C bus scan tests =====
#include "../test_mocks/Wire.h"

// --- Test-local I2C scan that mirrors the production hal_i2c_scan_bus() logic
//     but uses the WireMock instead of being #ifndef NATIVE_TEST guarded.
//     Scans standard address range 0x08-0x77 (skips reserved 0x00-0x07, 0x78-0x7F).
static uint8_t test_i2c_scan_bus(uint8_t busIndex) {
    uint8_t found = 0;
    WireClass *bus = nullptr;

    switch (busIndex) {
        case HAL_I2C_BUS_EXT:
            bus = &Wire1;
            bus->begin(48, 54, 100000);
            break;
        case HAL_I2C_BUS_ONBOARD:
            bus = &Wire;
            bus->begin(7, 8, 100000);
            break;
        case HAL_I2C_BUS_EXP:
            bus = &Wire1;
            bus->begin(28, 29, 100000);
            break;
        default:
            return 0;
    }

    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        bus->beginTransmission(addr);
        uint8_t err = bus->endTransmission();
        if (err == 0) {
            found++;
        }
    }
    return found;
}

// --- Test-local WiFi SDIO detection (mirrors hal_wifi_sdio_active() logic)
static bool _testWifiConnectSuccess = false;
static bool _testWifiConnecting = false;
static uint8_t _testActiveInterface = 0;  // 0=NET_NONE, 2=NET_WIFI

// Mirrors production hal_wifi_sdio_active() — returns true when WiFi SDIO
// pins (GPIO 48/54) are in use. Checks all three conditions.
static bool test_wifi_sdio_active() {
    return _testWifiConnectSuccess
        || _testWifiConnecting
        || _testActiveInterface == 2;  // NET_WIFI
}

// Legacy alias for backward compat with existing tests
static bool _testWifiActive = false;

static bool test_should_scan_bus(uint8_t busIndex) {
    // Use the new multi-flag check OR the legacy single flag
    bool sdioActive = test_wifi_sdio_active() || _testWifiActive;
    if (busIndex == HAL_I2C_BUS_EXT && sdioActive) return false;
    return true;  // ONBOARD and EXP always scanned
}

// ===== Test Fixtures =====
static HalDeviceManager* mgr;

void setUp() {
    mgr = &HalDeviceManager::instance();
    mgr->reset();
    hal_registry_reset();
    hal_db_reset();
    WireMock::reset();
    _testWifiActive = false;
    _testWifiConnectSuccess = false;
    _testWifiConnecting = false;
    _testActiveInterface = 0;
}

void tearDown() {}

// ===== DB Tests =====
void test_db_add_and_lookup() {
    HalDeviceDescriptor d;
    memset(&d, 0, sizeof(d));
    strncpy(d.compatible, "ti,pcm5102a", 31);
    strncpy(d.name, "PCM5102A", 32);
    d.type = HAL_DEV_DAC;
    d.channelCount = 2;

    TEST_ASSERT_TRUE(hal_db_add(&d));
    TEST_ASSERT_EQUAL(1, hal_db_count());

    HalDeviceDescriptor result;
    TEST_ASSERT_TRUE(hal_db_lookup("ti,pcm5102a", &result));
    TEST_ASSERT_EQUAL_STRING("PCM5102A", result.name);
    TEST_ASSERT_EQUAL(HAL_DEV_DAC, result.type);
}

void test_db_not_found() {
    TEST_ASSERT_FALSE(hal_db_lookup("nonexistent", nullptr));
}

void test_db_update_existing() {
    HalDeviceDescriptor d;
    memset(&d, 0, sizeof(d));
    strncpy(d.compatible, "ti,pcm5102a", 31);
    strncpy(d.name, "PCM5102A", 32);
    d.channelCount = 2;
    hal_db_add(&d);

    // Update
    d.channelCount = 4;
    strncpy(d.name, "PCM5102A-V2", 32);
    hal_db_add(&d);

    TEST_ASSERT_EQUAL(1, hal_db_count());
    HalDeviceDescriptor result;
    hal_db_lookup("ti,pcm5102a", &result);
    TEST_ASSERT_EQUAL(4, result.channelCount);
    TEST_ASSERT_EQUAL_STRING("PCM5102A-V2", result.name);
}

void test_db_remove() {
    HalDeviceDescriptor d;
    memset(&d, 0, sizeof(d));
    strncpy(d.compatible, "ti,pcm5102a", 31);
    hal_db_add(&d);

    TEST_ASSERT_TRUE(hal_db_remove("ti,pcm5102a"));
    TEST_ASSERT_EQUAL(0, hal_db_count());
    TEST_ASSERT_FALSE(hal_db_remove("ti,pcm5102a"));  // Already removed
}

void test_db_max_entries() {
    for (int i = 0; i < HAL_DB_MAX_ENTRIES; i++) {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        snprintf(d.compatible, 31, "test,device%d", i);
        TEST_ASSERT_TRUE(hal_db_add(&d));
    }
    TEST_ASSERT_EQUAL(HAL_DB_MAX_ENTRIES, hal_db_count());

    HalDeviceDescriptor overflow;
    memset(&overflow, 0, sizeof(overflow));
    strncpy(overflow.compatible, "test,overflow", 31);
    TEST_ASSERT_FALSE(hal_db_add(&overflow));
}

// ===== YAML Parser Tests =====
void test_yaml_parse_es8311() {
    const char* yaml =
        "hal_version: 1\n"
        "compatible: \"evergrande,es8311\"\n"
        "name: \"ES8311 Mono Audio Codec\"\n"
        "manufacturer: \"Evergrande Microelectronics\"\n"
        "device_type: CODEC\n"
        "i2c_default_address: 0x18\n"
        "channel_count: 1\n"
        "cap_hw_volume: true\n"
        "cap_hw_mute: true\n";

    HalDeviceDescriptor desc;
    TEST_ASSERT_TRUE(hal_parse_device_yaml(yaml, strlen(yaml), &desc));
    TEST_ASSERT_EQUAL_STRING("evergrande,es8311", desc.compatible);
    TEST_ASSERT_EQUAL_STRING("ES8311 Mono Audio Codec", desc.name);
    TEST_ASSERT_EQUAL(HAL_DEV_CODEC, desc.type);
    TEST_ASSERT_EQUAL(0x18, desc.i2cAddr);
    TEST_ASSERT_EQUAL(1, desc.channelCount);
    TEST_ASSERT_TRUE(desc.capabilities & HAL_CAP_HW_VOLUME);
    TEST_ASSERT_TRUE(desc.capabilities & HAL_CAP_MUTE);
}

void test_yaml_parse_pcm5102a() {
    const char* yaml =
        "compatible: \"ti,pcm5102a\"\n"
        "name: \"PCM5102A\"\n"
        "device_type: DAC\n"
        "i2c_default_address: 0x00\n"
        "channel_count: 2\n";

    HalDeviceDescriptor desc;
    TEST_ASSERT_TRUE(hal_parse_device_yaml(yaml, strlen(yaml), &desc));
    TEST_ASSERT_EQUAL_STRING("ti,pcm5102a", desc.compatible);
    TEST_ASSERT_EQUAL(HAL_DEV_DAC, desc.type);
    TEST_ASSERT_EQUAL(0, desc.i2cAddr);
    TEST_ASSERT_EQUAL(2, desc.channelCount);
}

void test_yaml_parse_missing_compatible() {
    const char* yaml =
        "name: \"NoCompat\"\n"
        "device_type: DAC\n";

    HalDeviceDescriptor desc;
    TEST_ASSERT_FALSE(hal_parse_device_yaml(yaml, strlen(yaml), &desc));
}

void test_yaml_parse_empty() {
    TEST_ASSERT_FALSE(hal_parse_device_yaml("", 0, nullptr));
    TEST_ASSERT_FALSE(hal_parse_device_yaml(nullptr, 0, nullptr));
}

void test_yaml_parse_with_comments() {
    const char* yaml =
        "# This is a comment\n"
        "compatible: \"ti,pcm1808\"\n"
        "# Another comment\n"
        "name: \"PCM1808\"\n"
        "device_type: ADC\n"
        "channel_count: 2\n";

    HalDeviceDescriptor desc;
    TEST_ASSERT_TRUE(hal_parse_device_yaml(yaml, strlen(yaml), &desc));
    TEST_ASSERT_EQUAL_STRING("ti,pcm1808", desc.compatible);
    TEST_ASSERT_EQUAL(HAL_DEV_ADC, desc.type);
}

// ===== Registry + DB Integration Tests =====
void test_registry_to_db_lookup() {
    // Register a driver
    HalDriverEntry entry;
    memset(&entry, 0, sizeof(entry));
    strncpy(entry.compatible, "ti,pcm5102a", 31);
    entry.type = HAL_DEV_DAC;
    entry.legacyId = 0x0001;
    hal_registry_register(entry);

    // Add to DB
    HalDeviceDescriptor d;
    memset(&d, 0, sizeof(d));
    strncpy(d.compatible, "ti,pcm5102a", 31);
    strncpy(d.name, "PCM5102A", 32);
    d.type = HAL_DEV_DAC;
    d.channelCount = 2;
    hal_db_add(&d);

    // Simulate discovery: find by legacy ID → lookup in DB
    const HalDriverEntry* found = hal_registry_find_by_legacy_id(0x0001);
    TEST_ASSERT_NOT_NULL(found);

    HalDeviceDescriptor result;
    TEST_ASSERT_TRUE(hal_db_lookup(found->compatible, &result));
    TEST_ASSERT_EQUAL_STRING("PCM5102A", result.name);
}

void test_discovery_without_eeprom_builtins_only() {
    // Register builtins in registry
    HalDriverEntry e;
    memset(&e, 0, sizeof(e));
    strncpy(e.compatible, "ti,pcm5102a", 31);
    e.type = HAL_DEV_DAC;
    hal_registry_register(e);

    // Without EEPROM, only builtins are available
    TEST_ASSERT_EQUAL(1, hal_registry_count());
    TEST_ASSERT_NOT_NULL(hal_registry_find("ti,pcm5102a"));
}

void test_db_get_by_index() {
    HalDeviceDescriptor d1, d2;
    memset(&d1, 0, sizeof(d1));
    memset(&d2, 0, sizeof(d2));
    strncpy(d1.compatible, "dev1", 31);
    strncpy(d2.compatible, "dev2", 31);
    hal_db_add(&d1);
    hal_db_add(&d2);

    const HalDeviceDescriptor* r0 = hal_db_get(0);
    const HalDeviceDescriptor* r1 = hal_db_get(1);
    TEST_ASSERT_NOT_NULL(r0);
    TEST_ASSERT_NOT_NULL(r1);
    TEST_ASSERT_EQUAL_STRING("dev1", r0->compatible);
    TEST_ASSERT_EQUAL_STRING("dev2", r1->compatible);
    TEST_ASSERT_NULL(hal_db_get(2));
    TEST_ASSERT_NULL(hal_db_get(-1));
}

// ===== I2C Bus Scan Tests =====

void test_bus_scan_returns_addresses_for_mocked_i2c_devices() {
    // Arrange: register two I2C devices on bus ONBOARD
    WireMock::registerDevice(0x18, 1);  // ES8311 on bus 1
    WireMock::registerDevice(0x50, 1);  // EEPROM on bus 1

    // Act
    uint8_t count = test_i2c_scan_bus(HAL_I2C_BUS_ONBOARD);

    // Assert: both devices found
    TEST_ASSERT_EQUAL(2, count);
}

void test_bus_ext_skipped_when_wifi_connected() {
    // Arrange: WiFi is active — bus EXT should be skipped
    _testWifiActive = true;
    WireMock::registerDevice(0x18, 0);  // Device on bus EXT

    // Act
    bool shouldScan = test_should_scan_bus(HAL_I2C_BUS_EXT);

    // Assert: bus EXT not scanned when WiFi active (SDIO conflict)
    TEST_ASSERT_FALSE(shouldScan);
}

void test_bus_ext_scanned_when_wifi_not_connected() {
    // Arrange: WiFi is NOT active — bus EXT should be scanned
    _testWifiActive = false;
    WireMock::registerDevice(0x20, 0);  // Device on bus EXT

    // Act
    bool shouldScan = test_should_scan_bus(HAL_I2C_BUS_EXT);

    // Assert: bus EXT is scanned when WiFi is inactive
    TEST_ASSERT_TRUE(shouldScan);

    // Verify the scan actually finds the device
    uint8_t count = test_i2c_scan_bus(HAL_I2C_BUS_EXT);
    TEST_ASSERT_EQUAL(1, count);
}

void test_bus_onboard_always_scanned() {
    // Arrange: WiFi active — should NOT affect bus ONBOARD
    _testWifiActive = true;
    WireMock::registerDevice(0x18, 1);  // ES8311 on bus ONBOARD

    // Act
    bool shouldScanOnboard = test_should_scan_bus(HAL_I2C_BUS_ONBOARD);

    // Assert: bus ONBOARD is always scanned regardless of WiFi state
    TEST_ASSERT_TRUE(shouldScanOnboard);

    // Also verify WiFi=false still works
    _testWifiActive = false;
    TEST_ASSERT_TRUE(test_should_scan_bus(HAL_I2C_BUS_ONBOARD));

    // And the actual scan returns the device
    uint8_t count = test_i2c_scan_bus(HAL_I2C_BUS_ONBOARD);
    TEST_ASSERT_EQUAL(1, count);
}

void test_empty_bus_returns_zero_devices() {
    // Arrange: no devices registered on any bus
    // WireMock::reset() already called in setUp()

    // Act
    uint8_t countOnboard = test_i2c_scan_bus(HAL_I2C_BUS_ONBOARD);
    uint8_t countExt     = test_i2c_scan_bus(HAL_I2C_BUS_EXT);
    uint8_t countExp     = test_i2c_scan_bus(HAL_I2C_BUS_EXP);

    // Assert: all buses return 0
    TEST_ASSERT_EQUAL(0, countOnboard);
    TEST_ASSERT_EQUAL(0, countExt);
    TEST_ASSERT_EQUAL(0, countExp);
}

void test_address_range_0x08_to_0x77() {
    // Arrange: register devices at boundary addresses
    //   0x07 is in the reserved low range  (0x00-0x07) — must NOT be found
    //   0x08 is the first valid address    — must be found
    //   0x77 is the last valid address     — must be found
    //   0x78 is in the reserved high range (0x78-0x7F) — must NOT be found
    WireMock::registerDevice(0x07, 1);  // Reserved low — should be skipped
    WireMock::registerDevice(0x08, 1);  // First valid address
    WireMock::registerDevice(0x3C, 1);  // Mid-range (e.g. OLED display)
    WireMock::registerDevice(0x77, 1);  // Last valid address
    WireMock::registerDevice(0x78, 1);  // Reserved high — should be skipped

    // Act
    uint8_t count = test_i2c_scan_bus(HAL_I2C_BUS_ONBOARD);

    // Assert: only the 3 addresses in 0x08-0x77 range should be found
    TEST_ASSERT_EQUAL(3, count);
}

// ===== WiFi SDIO Guard Tests =====

void test_wifi_sdio_active_when_connectSuccess() {
    // WiFi connected — SDIO pins are in use, Bus 0 must be skipped
    _testWifiConnectSuccess = true;
    TEST_ASSERT_TRUE(test_wifi_sdio_active());
    TEST_ASSERT_FALSE(test_should_scan_bus(HAL_I2C_BUS_EXT));
    // Other buses unaffected
    TEST_ASSERT_TRUE(test_should_scan_bus(HAL_I2C_BUS_ONBOARD));
    TEST_ASSERT_TRUE(test_should_scan_bus(HAL_I2C_BUS_EXP));
}

void test_wifi_sdio_active_when_connecting() {
    // WiFi is connecting — SDIO handshake in progress, pins active
    _testWifiConnecting = true;
    TEST_ASSERT_TRUE(test_wifi_sdio_active());
    TEST_ASSERT_FALSE(test_should_scan_bus(HAL_I2C_BUS_EXT));
}

void test_wifi_sdio_active_when_activeInterface_wifi() {
    // Ethernet failover path — activeInterface set to NET_WIFI
    _testActiveInterface = 2;  // NET_WIFI
    TEST_ASSERT_TRUE(test_wifi_sdio_active());
    TEST_ASSERT_FALSE(test_should_scan_bus(HAL_I2C_BUS_EXT));
}

void test_wifi_sdio_inactive_when_fully_disconnected() {
    // All flags false — WiFi fully disconnected, Bus 0 safe to scan
    TEST_ASSERT_FALSE(test_wifi_sdio_active());
    TEST_ASSERT_TRUE(test_should_scan_bus(HAL_I2C_BUS_EXT));
    TEST_ASSERT_TRUE(test_should_scan_bus(HAL_I2C_BUS_ONBOARD));
    TEST_ASSERT_TRUE(test_should_scan_bus(HAL_I2C_BUS_EXP));
}

void test_wifi_sdio_active_combinations() {
    // Truth table: any single flag being true should activate the guard
    struct { bool cs; bool cn; uint8_t ai; bool expected; } cases[] = {
        { false, false, 0, false },  // All off → safe
        { true,  false, 0, true  },  // Connected only
        { false, true,  0, true  },  // Connecting only
        { false, false, 2, true  },  // activeInterface only
        { true,  true,  0, true  },  // Connected + connecting
        { true,  false, 2, true  },  // Connected + activeInterface
        { false, true,  2, true  },  // Connecting + activeInterface
        { true,  true,  2, true  },  // All on
    };

    for (int i = 0; i < 8; i++) {
        _testWifiConnectSuccess = cases[i].cs;
        _testWifiConnecting = cases[i].cn;
        _testActiveInterface = cases[i].ai;
        TEST_ASSERT_EQUAL_MESSAGE(cases[i].expected, test_wifi_sdio_active(),
            cases[i].expected ? "Expected SDIO active" : "Expected SDIO inactive");
    }
}

// ===== Test Runner =====
int main(int argc, char** argv) {
    UNITY_BEGIN();

    // DB tests
    RUN_TEST(test_db_add_and_lookup);
    RUN_TEST(test_db_not_found);
    RUN_TEST(test_db_update_existing);
    RUN_TEST(test_db_remove);
    RUN_TEST(test_db_max_entries);
    RUN_TEST(test_db_get_by_index);

    // YAML parser tests
    RUN_TEST(test_yaml_parse_es8311);
    RUN_TEST(test_yaml_parse_pcm5102a);
    RUN_TEST(test_yaml_parse_missing_compatible);
    RUN_TEST(test_yaml_parse_empty);
    RUN_TEST(test_yaml_parse_with_comments);

    // Integration tests
    RUN_TEST(test_registry_to_db_lookup);
    RUN_TEST(test_discovery_without_eeprom_builtins_only);

    // I2C bus scan tests
    RUN_TEST(test_bus_scan_returns_addresses_for_mocked_i2c_devices);
    RUN_TEST(test_bus_ext_skipped_when_wifi_connected);
    RUN_TEST(test_bus_ext_scanned_when_wifi_not_connected);
    RUN_TEST(test_bus_onboard_always_scanned);
    RUN_TEST(test_empty_bus_returns_zero_devices);
    RUN_TEST(test_address_range_0x08_to_0x77);

    // WiFi SDIO guard tests
    RUN_TEST(test_wifi_sdio_active_when_connectSuccess);
    RUN_TEST(test_wifi_sdio_active_when_connecting);
    RUN_TEST(test_wifi_sdio_active_when_activeInterface_wifi);
    RUN_TEST(test_wifi_sdio_inactive_when_fully_disconnected);
    RUN_TEST(test_wifi_sdio_active_combinations);

    return UNITY_END();
}
