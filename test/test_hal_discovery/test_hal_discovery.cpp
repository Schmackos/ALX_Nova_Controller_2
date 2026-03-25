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
#define HAL_DB_MAX_ENTRIES 24
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
int hal_db_max() { return HAL_DB_MAX_ENTRIES; }

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

// ===== EEPROM v3 parsing for expansion discovery tests =====
// Include v3 parser directly (no platform dependencies)
#include "../../src/hal/hal_eeprom_v3.cpp"

// Inline dac_eeprom constants + parse/serialize (cannot include dac_eeprom.cpp
// because it pulls in debug_serial.h → Arduino.h → platform headers)
#define DAC_EEPROM_MAGIC      "ALXD"
#define DAC_EEPROM_MAGIC_LEN  4
#define DAC_EEPROM_VERSION    1
#define DAC_EEPROM_VERSION_V2  2
#define DAC_EEPROM_VERSION_V3  3
#define DAC_EEPROM_MAX_RATES  4
#define DAC_EEPROM_DATA_SIZE  0x5C
#define DAC_EEPROM_DATA_SIZE_V2  0x5E
#define DAC_EEPROM_DATA_SIZE_V3  0x80
#define DAC_EEPROM_ADDR_START 0x50
#define DAC_EEPROM_ADDR_END   0x57
#define DAC_DEVICE_TYPE_DAC    0
#define DAC_DEVICE_TYPE_ADC    1
#define DAC_DEVICE_TYPE_CODEC  2
#define DAC_FLAG_INDEPENDENT_CLOCK  0x01
#define DAC_FLAG_HW_VOLUME          0x02
#define DAC_FLAG_FILTERS            0x04

struct DacEepromData {
    bool valid;
    uint8_t formatVersion;
    uint16_t deviceId;
    uint8_t hwRevision;
    char deviceName[33];
    char manufacturer[33];
    uint8_t maxChannels;
    uint8_t dacI2cAddress;
    uint8_t flags;
    uint8_t numSampleRates;
    uint32_t sampleRates[DAC_EEPROM_MAX_RATES];
    uint8_t i2cAddress;
    uint8_t deviceType;
    uint8_t i2sPort;
};

static bool dac_eeprom_parse(const uint8_t* rawData, int len, DacEepromData* out) {
    if (!rawData || !out || len < 0x5C) { if (out) out->valid = false; return false; }
    memset(out, 0, sizeof(DacEepromData));
    if (memcmp(rawData, DAC_EEPROM_MAGIC, DAC_EEPROM_MAGIC_LEN) != 0) { out->valid = false; return false; }
    out->formatVersion = rawData[0x04];
    if (out->formatVersion != DAC_EEPROM_VERSION &&
        out->formatVersion != DAC_EEPROM_VERSION_V2 &&
        out->formatVersion != DAC_EEPROM_VERSION_V3) { out->valid = false; return false; }
    out->deviceId = (uint16_t)rawData[0x05] | ((uint16_t)rawData[0x06] << 8);
    out->hwRevision = rawData[0x07];
    memcpy(out->deviceName, &rawData[0x08], 32); out->deviceName[32] = '\0';
    memcpy(out->manufacturer, &rawData[0x28], 32); out->manufacturer[32] = '\0';
    out->maxChannels = rawData[0x48];
    out->dacI2cAddress = rawData[0x49];
    out->flags = rawData[0x4A];
    out->numSampleRates = rawData[0x4B];
    if (out->numSampleRates > DAC_EEPROM_MAX_RATES) out->numSampleRates = DAC_EEPROM_MAX_RATES;
    for (uint8_t i = 0; i < out->numSampleRates; i++) {
        int off = 0x4C + i * 4;
        out->sampleRates[i] = (uint32_t)rawData[off] | ((uint32_t)rawData[off+1]<<8) |
                              ((uint32_t)rawData[off+2]<<16) | ((uint32_t)rawData[off+3]<<24);
    }
    out->valid = true;
    if (out->formatVersion >= DAC_EEPROM_VERSION_V2 && len >= DAC_EEPROM_DATA_SIZE_V2) {
        out->deviceType = rawData[0x5C]; out->i2sPort = rawData[0x5D];
    } else { out->deviceType = DAC_DEVICE_TYPE_DAC; out->i2sPort = 0; }
    return true;
}

static int dac_eeprom_serialize(const DacEepromData* data, uint8_t* outBuf, int bufLen) {
    if (!data || !outBuf || bufLen < DAC_EEPROM_DATA_SIZE_V2) return 0;
    memset(outBuf, 0, DAC_EEPROM_DATA_SIZE_V2);
    memcpy(&outBuf[0x00], DAC_EEPROM_MAGIC, DAC_EEPROM_MAGIC_LEN);
    outBuf[0x04] = DAC_EEPROM_VERSION_V2;
    outBuf[0x05] = (uint8_t)(data->deviceId & 0xFF);
    outBuf[0x06] = (uint8_t)((data->deviceId >> 8) & 0xFF);
    outBuf[0x07] = data->hwRevision;
    size_t nLen = strlen(data->deviceName); if (nLen > 32) nLen = 32;
    memcpy(&outBuf[0x08], data->deviceName, nLen);
    size_t mLen = strlen(data->manufacturer); if (mLen > 32) mLen = 32;
    memcpy(&outBuf[0x28], data->manufacturer, mLen);
    outBuf[0x48] = data->maxChannels;
    outBuf[0x49] = data->dacI2cAddress;
    outBuf[0x4A] = data->flags;
    uint8_t numRates = data->numSampleRates;
    if (numRates > DAC_EEPROM_MAX_RATES) numRates = DAC_EEPROM_MAX_RATES;
    outBuf[0x4B] = numRates;
    for (uint8_t i = 0; i < numRates; i++) {
        int off = 0x4C + i * 4;
        outBuf[off]   = (uint8_t)(data->sampleRates[i] & 0xFF);
        outBuf[off+1] = (uint8_t)((data->sampleRates[i] >> 8) & 0xFF);
        outBuf[off+2] = (uint8_t)((data->sampleRates[i] >> 16) & 0xFF);
        outBuf[off+3] = (uint8_t)((data->sampleRates[i] >> 24) & 0xFF);
    }
    outBuf[0x5C] = data->deviceType;
    outBuf[0x5D] = data->i2sPort;
    return DAC_EEPROM_DATA_SIZE_V2;
}

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

void test_wifi_sdio_clears_on_disconnect() {
    // Simulate: WiFi was connected (SDIO pins active, Bus 0 blocked)
    _testWifiConnectSuccess = true;
    _testActiveInterface = 2;  // NET_WIFI
    TEST_ASSERT_TRUE(test_wifi_sdio_active());
    TEST_ASSERT_FALSE(test_should_scan_bus(HAL_I2C_BUS_EXT));

    // Simulate: onWiFiEvent(ARDUINO_EVENT_WIFI_STA_DISCONNECTED) fires and clears flags
    // This is the fix — these flags are now cleared immediately in the event handler.
    _testWifiConnectSuccess = false;
    _testActiveInterface = 0;  // NET_NONE

    // Bus 0 should now be immediately scannable — no 20s timeout required
    TEST_ASSERT_FALSE(test_wifi_sdio_active());
    TEST_ASSERT_TRUE(test_should_scan_bus(HAL_I2C_BUS_EXT));
}

void test_wifi_sdio_connecting_remains_active_during_reconnect() {
    // Simulate: WiFi disconnected but reconnect is in progress (connecting=true)
    // Bus 0 must remain blocked during reconnect handshake
    _testWifiConnectSuccess = false;  // Cleared by disconnect event
    _testActiveInterface = 0;         // Cleared by disconnect event
    _testWifiConnecting = true;       // Reconnect attempt started

    TEST_ASSERT_TRUE(test_wifi_sdio_active());
    TEST_ASSERT_FALSE(test_should_scan_bus(HAL_I2C_BUS_EXT));
}

// ===== Capacity Accessor Tests =====

void test_db_max_returns_correct_value() {
    // hal_db_max() should return HAL_DB_MAX_ENTRIES (24)
    TEST_ASSERT_EQUAL(HAL_DB_MAX_ENTRIES, hal_db_max());
    TEST_ASSERT_EQUAL(24, hal_db_max());
}

void test_registry_max_returns_correct_value() {
    // hal_registry_max() should return HAL_MAX_DRIVERS (64)
    TEST_ASSERT_EQUAL(HAL_MAX_DRIVERS, hal_registry_max());
    TEST_ASSERT_EQUAL(64, hal_registry_max());
}

void test_db_overflow_at_new_limit() {
    // Fill DB to exactly HAL_DB_MAX_ENTRIES (24)
    for (int i = 0; i < HAL_DB_MAX_ENTRIES; i++) {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        snprintf(d.compatible, 31, "cap,device%d", i);
        snprintf(d.name, 32, "Device %d", i);
        d.type = HAL_DEV_DAC;
        TEST_ASSERT_TRUE_MESSAGE(hal_db_add(&d),
            "Should add up to HAL_DB_MAX_ENTRIES entries");
    }
    TEST_ASSERT_EQUAL(HAL_DB_MAX_ENTRIES, hal_db_count());

    // The 25th entry should be rejected
    HalDeviceDescriptor overflow;
    memset(&overflow, 0, sizeof(overflow));
    strncpy(overflow.compatible, "cap,overflow", 31);
    overflow.type = HAL_DEV_CODEC;
    TEST_ASSERT_FALSE(hal_db_add(&overflow));

    // Count unchanged
    TEST_ASSERT_EQUAL(HAL_DB_MAX_ENTRIES, hal_db_count());

    // Previously added entries still accessible
    HalDeviceDescriptor result;
    TEST_ASSERT_TRUE(hal_db_lookup("cap,device0", &result));
    TEST_ASSERT_TRUE(hal_db_lookup("cap,device23", &result));
    TEST_ASSERT_FALSE(hal_db_lookup("cap,overflow", nullptr));
}

void test_db_remove_then_add_after_full() {
    // Fill DB to max
    for (int i = 0; i < HAL_DB_MAX_ENTRIES; i++) {
        HalDeviceDescriptor d;
        memset(&d, 0, sizeof(d));
        snprintf(d.compatible, 31, "rmv,dev%d", i);
        d.type = HAL_DEV_ADC;
        hal_db_add(&d);
    }
    TEST_ASSERT_EQUAL(HAL_DB_MAX_ENTRIES, hal_db_count());

    // Overflow rejected
    HalDeviceDescriptor extra;
    memset(&extra, 0, sizeof(extra));
    strncpy(extra.compatible, "rmv,extra", 31);
    extra.type = HAL_DEV_CODEC;
    TEST_ASSERT_FALSE(hal_db_add(&extra));

    // Remove one entry
    TEST_ASSERT_TRUE(hal_db_remove("rmv,dev5"));
    TEST_ASSERT_EQUAL(HAL_DB_MAX_ENTRIES - 1, hal_db_count());

    // Now adding should succeed again
    TEST_ASSERT_TRUE(hal_db_add(&extra));
    TEST_ASSERT_EQUAL(HAL_DB_MAX_ENTRIES, hal_db_count());

    // Verify the new entry is findable
    TEST_ASSERT_TRUE(hal_db_lookup("rmv,extra", nullptr));
}

void test_registry_overflow_at_max_drivers() {
    // Fill registry to HAL_MAX_DRIVERS (24)
    for (int i = 0; i < HAL_MAX_DRIVERS; i++) {
        HalDriverEntry entry;
        memset(&entry, 0, sizeof(entry));
        snprintf(entry.compatible, 31, "reg,driver%d", i);
        entry.type = HAL_DEV_DAC;
        entry.legacyId = static_cast<uint16_t>(i + 100);
        TEST_ASSERT_TRUE_MESSAGE(hal_registry_register(entry),
            "Should register up to HAL_MAX_DRIVERS entries");
    }
    TEST_ASSERT_EQUAL(HAL_MAX_DRIVERS, hal_registry_count());

    // The 25th registration should fail
    HalDriverEntry overflow;
    memset(&overflow, 0, sizeof(overflow));
    strncpy(overflow.compatible, "reg,overflow", 31);
    overflow.type = HAL_DEV_CODEC;
    TEST_ASSERT_FALSE(hal_registry_register(overflow));

    // Count stays at max
    TEST_ASSERT_EQUAL(HAL_MAX_DRIVERS, hal_registry_count());

    // Previously registered entries still findable
    TEST_ASSERT_NOT_NULL(hal_registry_find("reg,driver0"));
    TEST_ASSERT_NOT_NULL(hal_registry_find("reg,driver23"));
    TEST_ASSERT_NULL(hal_registry_find("reg,overflow"));
}

// ===== Expansion EEPROM V3 Discovery Tests =====

// Helper: build a v3 EEPROM image and register it on Wire2 (Bus 2)
static void mock_expansion_eeprom_v3(uint8_t eepromAddr, const char* compatible,
                                      const char* deviceName, uint16_t deviceId,
                                      uint8_t devType = 0) {
    uint8_t buf[DAC_EEPROM_DATA_SIZE_V3];
    memset(buf, 0, sizeof(buf));

    // v2 base data
    DacEepromData data;
    memset(&data, 0, sizeof(data));
    data.formatVersion = DAC_EEPROM_VERSION_V3;
    data.deviceId = deviceId;
    data.hwRevision = 1;
    strncpy(data.deviceName, deviceName, 32);
    strncpy(data.manufacturer, "ESS Technology", 32);
    data.maxChannels = 2;
    data.dacI2cAddress = 0x48;
    data.flags = DAC_FLAG_HW_VOLUME | DAC_FLAG_FILTERS;
    data.numSampleRates = 2;
    data.sampleRates[0] = 48000;
    data.sampleRates[1] = 96000;
    data.deviceType = devType;
    data.i2sPort = 2;

    // Serialize v2 base
    dac_eeprom_serialize(&data, buf, DAC_EEPROM_DATA_SIZE_V2);
    // Override version to 3 (serialize writes v2)
    buf[0x04] = DAC_EEPROM_VERSION_V3;

    // Add v3 compatible string + CRC
    hal_eeprom_serialize_v3(buf, DAC_EEPROM_DATA_SIZE_V3, compatible);

    // Register EEPROM on Bus 2
    WireMock::registerDevice(eepromAddr, 2, buf, DAC_EEPROM_DATA_SIZE_V3);
}

void test_expansion_eeprom_v3_discovers_device() {
    // Register driver and DB entry for es9038q2m
    HalDriverEntry entry;
    memset(&entry, 0, sizeof(entry));
    strncpy(entry.compatible, "ess,es9038q2m", 31);
    entry.type = HAL_DEV_DAC;
    entry.legacyId = 0;  // No legacy ID — v3 compatible string only
    hal_registry_register(entry);

    HalDeviceDescriptor desc;
    memset(&desc, 0, sizeof(desc));
    strncpy(desc.compatible, "ess,es9038q2m", 31);
    strncpy(desc.name, "ES9038Q2M", 32);
    desc.type = HAL_DEV_DAC;
    hal_db_add(&desc);

    // Mock EEPROM at 0x50 on Bus 2 with v3 data
    mock_expansion_eeprom_v3(0x50, "ess,es9038q2m", "ES9038Q2M", 0);

    // Read raw data from mock and verify v3 parsing
    uint8_t rawData[DAC_EEPROM_DATA_SIZE_V3];
    for (int i = 0; i < DAC_EEPROM_DATA_SIZE_V3; i++) {
        auto& regmap = WireMock::registerMap[0x50];
        auto it = regmap.find((uint8_t)i);
        rawData[i] = (it != regmap.end()) ? it->second : 0;
    }

    // Verify v3 compatible string extraction
    char compat[32];
    TEST_ASSERT_TRUE(hal_eeprom_parse_v3(rawData, DAC_EEPROM_DATA_SIZE_V3, compat));
    TEST_ASSERT_EQUAL_STRING("ess,es9038q2m", compat);

    // Verify registry lookup by compatible string succeeds
    const HalDriverEntry* found = hal_registry_find(compat);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_STRING("ess,es9038q2m", found->compatible);
}

void test_expansion_eeprom_v3_no_driver_returns_no_match() {
    // EEPROM with v3 compatible string but no registered driver
    mock_expansion_eeprom_v3(0x51, "unknown,device123", "UnknownDevice", 0);

    uint8_t rawData[DAC_EEPROM_DATA_SIZE_V3];
    for (int i = 0; i < DAC_EEPROM_DATA_SIZE_V3; i++) {
        auto& regmap = WireMock::registerMap[0x51];
        auto it = regmap.find((uint8_t)i);
        rawData[i] = (it != regmap.end()) ? it->second : 0;
    }

    char compat[32];
    TEST_ASSERT_TRUE(hal_eeprom_parse_v3(rawData, DAC_EEPROM_DATA_SIZE_V3, compat));
    TEST_ASSERT_EQUAL_STRING("unknown,device123", compat);

    // No driver registered for this compatible string
    TEST_ASSERT_NULL(hal_registry_find(compat));
}

void test_expansion_eeprom_legacy_fallback() {
    // v1 EEPROM on Bus 2 with legacy ID (no compatible string)
    uint8_t buf[DAC_EEPROM_DATA_SIZE_V3];
    memset(buf, 0, sizeof(buf));
    buf[0] = 'A'; buf[1] = 'L'; buf[2] = 'X'; buf[3] = 'D';
    buf[4] = 1;  // v1 format
    buf[5] = 0x01; buf[6] = 0x00;  // deviceId = 0x0001
    strncpy((char*)&buf[0x08], "PCM5102A", 32);
    strncpy((char*)&buf[0x28], "Texas Instruments", 32);
    buf[0x48] = 2;  // channels

    WireMock::registerDevice(0x52, 2, buf, DAC_EEPROM_DATA_SIZE_V3);

    // Register driver with legacy ID
    HalDriverEntry entry;
    memset(&entry, 0, sizeof(entry));
    strncpy(entry.compatible, "ti,pcm5102a", 31);
    entry.type = HAL_DEV_DAC;
    entry.legacyId = 0x0001;
    hal_registry_register(entry);

    // v3 parse should fail (v1 format, no CRC)
    char compat[32];
    TEST_ASSERT_FALSE(hal_eeprom_parse_v3(buf, DAC_EEPROM_DATA_SIZE_V3, compat));

    // But v1 parse + legacy ID lookup should succeed
    DacEepromData eepromData;
    TEST_ASSERT_TRUE(dac_eeprom_parse(buf, DAC_EEPROM_DATA_SIZE, &eepromData));
    TEST_ASSERT_EQUAL(0x0001, eepromData.deviceId);

    const HalDriverEntry* found = hal_registry_find_by_legacy_id(eepromData.deviceId);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_STRING("ti,pcm5102a", found->compatible);
}

void test_dac_eeprom_parse_accepts_v3() {
    uint8_t buf[DAC_EEPROM_DATA_SIZE_V3];
    memset(buf, 0, sizeof(buf));
    buf[0] = 'A'; buf[1] = 'L'; buf[2] = 'X'; buf[3] = 'D';
    buf[4] = DAC_EEPROM_VERSION_V3;
    buf[5] = 0x38; buf[6] = 0x90;  // deviceId = 0x9038
    buf[7] = 1;
    strncpy((char*)&buf[0x08], "TestV3Device", 32);
    strncpy((char*)&buf[0x28], "TestMfg", 32);
    buf[0x48] = 8;  // channels
    buf[0x5C] = 0;  // DAC
    buf[0x5D] = 2;  // i2sPort

    DacEepromData out;
    TEST_ASSERT_TRUE(dac_eeprom_parse(buf, DAC_EEPROM_DATA_SIZE_V3, &out));
    TEST_ASSERT_EQUAL(DAC_EEPROM_VERSION_V3, out.formatVersion);
    TEST_ASSERT_EQUAL_STRING("TestV3Device", out.deviceName);
    TEST_ASSERT_EQUAL(0x9038, out.deviceId);
    TEST_ASSERT_EQUAL(8, out.maxChannels);
    TEST_ASSERT_EQUAL(0, out.deviceType);  // DAC
    TEST_ASSERT_EQUAL(2, out.i2sPort);
}

void test_dual_mezzanine_discovery() {
    // Two EEPROMs on Bus 2: ADC at 0x50, DAC at 0x51
    mock_expansion_eeprom_v3(0x50, "ess,es9822pro", "ES9822PRO", 0, DAC_DEVICE_TYPE_ADC);
    mock_expansion_eeprom_v3(0x51, "ess,es9038q2m", "ES9038Q2M", 0, DAC_DEVICE_TYPE_DAC);

    // Register both drivers
    HalDriverEntry e1, e2;
    memset(&e1, 0, sizeof(e1));
    memset(&e2, 0, sizeof(e2));
    strncpy(e1.compatible, "ess,es9822pro", 31);
    e1.type = HAL_DEV_ADC;
    strncpy(e2.compatible, "ess,es9038q2m", 31);
    e2.type = HAL_DEV_DAC;
    hal_registry_register(e1);
    hal_registry_register(e2);

    // Verify both can be looked up by compatible string
    TEST_ASSERT_NOT_NULL(hal_registry_find("ess,es9822pro"));
    TEST_ASSERT_NOT_NULL(hal_registry_find("ess,es9038q2m"));

    // Verify both EEPROMs have valid v3 data
    for (uint8_t addr = 0x50; addr <= 0x51; addr++) {
        uint8_t rawData[DAC_EEPROM_DATA_SIZE_V3];
        for (int i = 0; i < DAC_EEPROM_DATA_SIZE_V3; i++) {
            auto& regmap = WireMock::registerMap[addr];
            auto it = regmap.find((uint8_t)i);
            rawData[i] = (it != regmap.end()) ? it->second : 0;
        }
        char compat[32];
        TEST_ASSERT_TRUE(hal_eeprom_parse_v3(rawData, DAC_EEPROM_DATA_SIZE_V3, compat));
    }
}

void test_eeprom_v3_crc_mismatch_rejects() {
    // Build valid v3 EEPROM, then corrupt one byte to invalidate CRC
    mock_expansion_eeprom_v3(0x53, "ess,es9039q2m", "ES9039Q2M", 0);

    uint8_t rawData[DAC_EEPROM_DATA_SIZE_V3];
    for (int i = 0; i < DAC_EEPROM_DATA_SIZE_V3; i++) {
        auto& regmap = WireMock::registerMap[0x53];
        auto it = regmap.find((uint8_t)i);
        rawData[i] = (it != regmap.end()) ? it->second : 0;
    }

    // Verify it's valid first
    char compat[32];
    TEST_ASSERT_TRUE(hal_eeprom_parse_v3(rawData, DAC_EEPROM_DATA_SIZE_V3, compat));

    // Corrupt the compatible string area
    rawData[0x60] ^= 0xFF;

    // CRC should now fail
    TEST_ASSERT_FALSE(hal_eeprom_parse_v3(rawData, DAC_EEPROM_DATA_SIZE_V3, compat));
}

void test_wire_mock_eeprom_read_with_register_address() {
    // Verify the Wire mock correctly handles EEPROM-style reads:
    // write(memAddr) + endTransmission(false) + requestFrom(addr, len)
    uint8_t testData[8] = { 0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44 };
    WireMock::registerDevice(0x50, 2, testData, 8);

    Wire2.begin(28, 29, 100000);

    // Read from offset 4 (should get 0x11, 0x22, 0x33, 0x44)
    Wire2.beginTransmission(0x50);
    Wire2.write((uint8_t)4);
    Wire2.endTransmission(false);  // Repeated start — sets register pointer

    uint8_t received = Wire2.requestFrom((uint8_t)0x50, (uint8_t)4);
    TEST_ASSERT_EQUAL(4, received);
    TEST_ASSERT_EQUAL(0x11, Wire2.read());
    TEST_ASSERT_EQUAL(0x22, Wire2.read());
    TEST_ASSERT_EQUAL(0x33, Wire2.read());
    TEST_ASSERT_EQUAL(0x44, Wire2.read());
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
    RUN_TEST(test_wifi_sdio_clears_on_disconnect);
    RUN_TEST(test_wifi_sdio_connecting_remains_active_during_reconnect);

    // Capacity accessor and overflow tests
    RUN_TEST(test_db_max_returns_correct_value);
    RUN_TEST(test_registry_max_returns_correct_value);
    RUN_TEST(test_db_overflow_at_new_limit);
    RUN_TEST(test_db_remove_then_add_after_full);
    RUN_TEST(test_registry_overflow_at_max_drivers);

    // Expansion EEPROM v3 discovery tests
    RUN_TEST(test_expansion_eeprom_v3_discovers_device);
    RUN_TEST(test_expansion_eeprom_v3_no_driver_returns_no_match);
    RUN_TEST(test_expansion_eeprom_legacy_fallback);
    RUN_TEST(test_dac_eeprom_parse_accepts_v3);
    RUN_TEST(test_dual_mezzanine_discovery);
    RUN_TEST(test_eeprom_v3_crc_mismatch_rejects);
    RUN_TEST(test_wire_mock_eeprom_read_with_register_address);

    return UNITY_END();
}
