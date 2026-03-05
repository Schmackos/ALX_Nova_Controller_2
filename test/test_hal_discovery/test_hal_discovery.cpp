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

// ===== Test Fixtures =====
static HalDeviceManager* mgr;

void setUp() {
    mgr = &HalDeviceManager::instance();
    mgr->reset();
    hal_registry_reset();
    hal_db_reset();
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

    return UNITY_END();
}
