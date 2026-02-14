#include <cstring>
#include <unity.h>

#ifdef NATIVE_TEST
#include "../test_mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ===== Inline re-implementation of EEPROM parser for native testing =====
// Tests don't compile src/ directly (test_build_src = no)

#define DAC_EEPROM_MAGIC      "ALXD"
#define DAC_EEPROM_MAGIC_LEN  4
#define DAC_EEPROM_VERSION    1
#define DAC_EEPROM_MAX_RATES  4

#define DAC_EEPROM_DATA_SIZE  0x5C
#define DAC_EEPROM_TOTAL_SIZE 256
#define DAC_EEPROM_PAGE_SIZE  8

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
};

static bool test_dac_eeprom_parse(const uint8_t* rawData, int len, DacEepromData* out) {
    if (!rawData || !out || len < 0x5C) {
        if (out) out->valid = false;
        return false;
    }
    memset(out, 0, sizeof(DacEepromData));

    if (memcmp(rawData, DAC_EEPROM_MAGIC, DAC_EEPROM_MAGIC_LEN) != 0) {
        out->valid = false;
        return false;
    }
    out->formatVersion = rawData[0x04];
    if (out->formatVersion != DAC_EEPROM_VERSION) {
        out->valid = false;
        return false;
    }
    out->deviceId = (uint16_t)rawData[0x05] | ((uint16_t)rawData[0x06] << 8);
    out->hwRevision = rawData[0x07];
    memcpy(out->deviceName, &rawData[0x08], 32);
    out->deviceName[32] = '\0';
    memcpy(out->manufacturer, &rawData[0x28], 32);
    out->manufacturer[32] = '\0';
    out->maxChannels = rawData[0x48];
    out->dacI2cAddress = rawData[0x49];
    out->flags = rawData[0x4A];
    out->numSampleRates = rawData[0x4B];
    if (out->numSampleRates > DAC_EEPROM_MAX_RATES) {
        out->numSampleRates = DAC_EEPROM_MAX_RATES;
    }
    for (uint8_t i = 0; i < out->numSampleRates; i++) {
        int offset = 0x4C + i * 4;
        out->sampleRates[i] = (uint32_t)rawData[offset]
                            | ((uint32_t)rawData[offset + 1] << 8)
                            | ((uint32_t)rawData[offset + 2] << 16)
                            | ((uint32_t)rawData[offset + 3] << 24);
    }
    out->valid = true;
    return true;
}

// ===== Inline re-implementation of serialize for native testing =====
static int test_dac_eeprom_serialize(const DacEepromData* data, uint8_t* outBuf, int bufLen) {
    if (!data || !outBuf || bufLen < DAC_EEPROM_DATA_SIZE) return 0;
    memset(outBuf, 0, DAC_EEPROM_DATA_SIZE);
    memcpy(&outBuf[0x00], DAC_EEPROM_MAGIC, DAC_EEPROM_MAGIC_LEN);
    outBuf[0x04] = DAC_EEPROM_VERSION;
    outBuf[0x05] = (uint8_t)(data->deviceId & 0xFF);
    outBuf[0x06] = (uint8_t)((data->deviceId >> 8) & 0xFF);
    outBuf[0x07] = data->hwRevision;
    size_t nameLen = strlen(data->deviceName);
    if (nameLen > 32) nameLen = 32;
    memcpy(&outBuf[0x08], data->deviceName, nameLen);
    size_t mfrLen = strlen(data->manufacturer);
    if (mfrLen > 32) mfrLen = 32;
    memcpy(&outBuf[0x28], data->manufacturer, mfrLen);
    outBuf[0x48] = data->maxChannels;
    outBuf[0x49] = data->dacI2cAddress;
    outBuf[0x4A] = data->flags;
    uint8_t numRates = data->numSampleRates;
    if (numRates > DAC_EEPROM_MAX_RATES) numRates = DAC_EEPROM_MAX_RATES;
    outBuf[0x4B] = numRates;
    for (uint8_t i = 0; i < numRates; i++) {
        int offset = 0x4C + i * 4;
        outBuf[offset]     = (uint8_t)(data->sampleRates[i] & 0xFF);
        outBuf[offset + 1] = (uint8_t)((data->sampleRates[i] >> 8) & 0xFF);
        outBuf[offset + 2] = (uint8_t)((data->sampleRates[i] >> 16) & 0xFF);
        outBuf[offset + 3] = (uint8_t)((data->sampleRates[i] >> 24) & 0xFF);
    }
    return DAC_EEPROM_DATA_SIZE;
}

// ===== Test Data Builder =====
static uint8_t testEeprom[256];

static void build_valid_eeprom() {
    memset(testEeprom, 0, sizeof(testEeprom));
    // Magic
    memcpy(&testEeprom[0x00], "ALXD", 4);
    // Version
    testEeprom[0x04] = 1;
    // Device ID = 0x0001 (PCM5102A), little-endian
    testEeprom[0x05] = 0x01;
    testEeprom[0x06] = 0x00;
    // Hardware revision
    testEeprom[0x07] = 2;
    // Device name
    const char* name = "PCM5102A DAC Board";
    memcpy(&testEeprom[0x08], name, strlen(name));
    // Manufacturer
    const char* mfr = "ALX Audio";
    memcpy(&testEeprom[0x28], mfr, strlen(mfr));
    // Max channels
    testEeprom[0x48] = 2;
    // DAC I2C address (0 = no I2C)
    testEeprom[0x49] = 0x00;
    // Flags: no HW volume, no independent clock, no filters
    testEeprom[0x4A] = 0x00;
    // Num sample rates = 3
    testEeprom[0x4B] = 3;
    // Rate 0: 44100 = 0x0000AC44
    testEeprom[0x4C] = 0x44; testEeprom[0x4D] = 0xAC;
    testEeprom[0x4E] = 0x00; testEeprom[0x4F] = 0x00;
    // Rate 1: 48000 = 0x0000BB80
    testEeprom[0x50] = 0x80; testEeprom[0x51] = 0xBB;
    testEeprom[0x52] = 0x00; testEeprom[0x53] = 0x00;
    // Rate 2: 96000 = 0x00017700
    testEeprom[0x54] = 0x00; testEeprom[0x55] = 0x77;
    testEeprom[0x56] = 0x01; testEeprom[0x57] = 0x00;
}

void setUp(void) {
    build_valid_eeprom();
}

void tearDown(void) {}

// ===== EEPROM Parse Tests =====

void test_eeprom_valid_magic(void) {
    DacEepromData data;
    TEST_ASSERT_TRUE(test_dac_eeprom_parse(testEeprom, 0x5C, &data));
    TEST_ASSERT_TRUE(data.valid);
}

void test_eeprom_invalid_magic(void) {
    testEeprom[0] = 'X'; // Corrupt magic
    DacEepromData data;
    TEST_ASSERT_FALSE(test_dac_eeprom_parse(testEeprom, 0x5C, &data));
    TEST_ASSERT_FALSE(data.valid);
}

void test_eeprom_wrong_version(void) {
    testEeprom[0x04] = 99; // Wrong version
    DacEepromData data;
    TEST_ASSERT_FALSE(test_dac_eeprom_parse(testEeprom, 0x5C, &data));
    TEST_ASSERT_FALSE(data.valid);
}

void test_eeprom_device_id_little_endian(void) {
    // Set device ID to 0x0302 (LE: 02 03)
    testEeprom[0x05] = 0x02;
    testEeprom[0x06] = 0x03;
    DacEepromData data;
    TEST_ASSERT_TRUE(test_dac_eeprom_parse(testEeprom, 0x5C, &data));
    TEST_ASSERT_EQUAL_UINT16(0x0302, data.deviceId);
}

void test_eeprom_device_name(void) {
    DacEepromData data;
    test_dac_eeprom_parse(testEeprom, 0x5C, &data);
    TEST_ASSERT_EQUAL_STRING("PCM5102A DAC Board", data.deviceName);
}

void test_eeprom_manufacturer(void) {
    DacEepromData data;
    test_dac_eeprom_parse(testEeprom, 0x5C, &data);
    TEST_ASSERT_EQUAL_STRING("ALX Audio", data.manufacturer);
}

void test_eeprom_null_terminated_strings(void) {
    // Fill device name field with non-null bytes
    memset(&testEeprom[0x08], 'A', 32);
    DacEepromData data;
    test_dac_eeprom_parse(testEeprom, 0x5C, &data);
    // Parser must null-terminate at position 32
    TEST_ASSERT_EQUAL_UINT8('\0', data.deviceName[32]);
    TEST_ASSERT_EQUAL(32, (int)strlen(data.deviceName));
}

void test_eeprom_flags_hw_volume(void) {
    testEeprom[0x4A] = DAC_FLAG_HW_VOLUME;
    DacEepromData data;
    test_dac_eeprom_parse(testEeprom, 0x5C, &data);
    TEST_ASSERT_TRUE(data.flags & DAC_FLAG_HW_VOLUME);
    TEST_ASSERT_FALSE(data.flags & DAC_FLAG_INDEPENDENT_CLOCK);
    TEST_ASSERT_FALSE(data.flags & DAC_FLAG_FILTERS);
}

void test_eeprom_flags_all(void) {
    testEeprom[0x4A] = DAC_FLAG_INDEPENDENT_CLOCK | DAC_FLAG_HW_VOLUME | DAC_FLAG_FILTERS;
    DacEepromData data;
    test_dac_eeprom_parse(testEeprom, 0x5C, &data);
    TEST_ASSERT_TRUE(data.flags & DAC_FLAG_INDEPENDENT_CLOCK);
    TEST_ASSERT_TRUE(data.flags & DAC_FLAG_HW_VOLUME);
    TEST_ASSERT_TRUE(data.flags & DAC_FLAG_FILTERS);
}

void test_eeprom_sample_rates(void) {
    DacEepromData data;
    test_dac_eeprom_parse(testEeprom, 0x5C, &data);
    TEST_ASSERT_EQUAL_UINT8(3, data.numSampleRates);
    TEST_ASSERT_EQUAL_UINT32(44100, data.sampleRates[0]);
    TEST_ASSERT_EQUAL_UINT32(48000, data.sampleRates[1]);
    TEST_ASSERT_EQUAL_UINT32(96000, data.sampleRates[2]);
}

void test_eeprom_too_short(void) {
    DacEepromData data;
    TEST_ASSERT_FALSE(test_dac_eeprom_parse(testEeprom, 10, &data));
    TEST_ASSERT_FALSE(data.valid);
}

void test_eeprom_null_input(void) {
    DacEepromData data;
    TEST_ASSERT_FALSE(test_dac_eeprom_parse(nullptr, 0x5C, &data));
    TEST_ASSERT_FALSE(data.valid);
}

void test_eeprom_null_output(void) {
    TEST_ASSERT_FALSE(test_dac_eeprom_parse(testEeprom, 0x5C, nullptr));
}

void test_eeprom_max_channels(void) {
    testEeprom[0x48] = 8;
    DacEepromData data;
    test_dac_eeprom_parse(testEeprom, 0x5C, &data);
    TEST_ASSERT_EQUAL_UINT8(8, data.maxChannels);
}

void test_eeprom_i2c_address(void) {
    testEeprom[0x49] = 0x48;
    DacEepromData data;
    test_dac_eeprom_parse(testEeprom, 0x5C, &data);
    TEST_ASSERT_EQUAL_UINT8(0x48, data.dacI2cAddress);
}

void test_eeprom_hw_revision(void) {
    DacEepromData data;
    test_dac_eeprom_parse(testEeprom, 0x5C, &data);
    TEST_ASSERT_EQUAL_UINT8(2, data.hwRevision);
}

void test_eeprom_rate_count_clamped(void) {
    testEeprom[0x4B] = 10; // More than DAC_EEPROM_MAX_RATES
    DacEepromData data;
    test_dac_eeprom_parse(testEeprom, 0x5C, &data);
    TEST_ASSERT_EQUAL_UINT8(DAC_EEPROM_MAX_RATES, data.numSampleRates);
}

// ===== Serialize Tests =====

void test_serialize_round_trip(void) {
    // Parse test EEPROM, then serialize, then parse again — should match
    DacEepromData parsed;
    TEST_ASSERT_TRUE(test_dac_eeprom_parse(testEeprom, 0x5C, &parsed));

    uint8_t buf[DAC_EEPROM_DATA_SIZE];
    int written = test_dac_eeprom_serialize(&parsed, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(DAC_EEPROM_DATA_SIZE, written);

    DacEepromData reparsed;
    TEST_ASSERT_TRUE(test_dac_eeprom_parse(buf, DAC_EEPROM_DATA_SIZE, &reparsed));
    TEST_ASSERT_TRUE(reparsed.valid);
    TEST_ASSERT_EQUAL_UINT16(parsed.deviceId, reparsed.deviceId);
    TEST_ASSERT_EQUAL_UINT8(parsed.hwRevision, reparsed.hwRevision);
    TEST_ASSERT_EQUAL_STRING(parsed.deviceName, reparsed.deviceName);
    TEST_ASSERT_EQUAL_STRING(parsed.manufacturer, reparsed.manufacturer);
    TEST_ASSERT_EQUAL_UINT8(parsed.maxChannels, reparsed.maxChannels);
    TEST_ASSERT_EQUAL_UINT8(parsed.dacI2cAddress, reparsed.dacI2cAddress);
    TEST_ASSERT_EQUAL_UINT8(parsed.flags, reparsed.flags);
    TEST_ASSERT_EQUAL_UINT8(parsed.numSampleRates, reparsed.numSampleRates);
    for (int i = 0; i < parsed.numSampleRates; i++) {
        TEST_ASSERT_EQUAL_UINT32(parsed.sampleRates[i], reparsed.sampleRates[i]);
    }
}

void test_serialize_endianness(void) {
    DacEepromData data;
    memset(&data, 0, sizeof(data));
    data.deviceId = 0xBEEF;
    strcpy(data.deviceName, "Test");
    strcpy(data.manufacturer, "Mfr");
    data.numSampleRates = 1;
    data.sampleRates[0] = 0x12345678;

    uint8_t buf[DAC_EEPROM_DATA_SIZE];
    test_dac_eeprom_serialize(&data, buf, sizeof(buf));

    // Device ID LE: 0xEF, 0xBE
    TEST_ASSERT_EQUAL_UINT8(0xEF, buf[0x05]);
    TEST_ASSERT_EQUAL_UINT8(0xBE, buf[0x06]);
    // Sample rate LE: 0x78, 0x56, 0x34, 0x12
    TEST_ASSERT_EQUAL_UINT8(0x78, buf[0x4C]);
    TEST_ASSERT_EQUAL_UINT8(0x56, buf[0x4D]);
    TEST_ASSERT_EQUAL_UINT8(0x34, buf[0x4E]);
    TEST_ASSERT_EQUAL_UINT8(0x12, buf[0x4F]);
}

void test_serialize_null_data(void) {
    uint8_t buf[DAC_EEPROM_DATA_SIZE];
    TEST_ASSERT_EQUAL(0, test_dac_eeprom_serialize(nullptr, buf, sizeof(buf)));
}

void test_serialize_null_buffer(void) {
    DacEepromData data;
    memset(&data, 0, sizeof(data));
    TEST_ASSERT_EQUAL(0, test_dac_eeprom_serialize(&data, nullptr, DAC_EEPROM_DATA_SIZE));
}

void test_serialize_short_buffer(void) {
    DacEepromData data;
    memset(&data, 0, sizeof(data));
    uint8_t buf[10];
    TEST_ASSERT_EQUAL(0, test_dac_eeprom_serialize(&data, buf, sizeof(buf)));
}

void test_serialize_flags_all(void) {
    DacEepromData data;
    memset(&data, 0, sizeof(data));
    data.flags = DAC_FLAG_INDEPENDENT_CLOCK | DAC_FLAG_HW_VOLUME | DAC_FLAG_FILTERS;
    strcpy(data.deviceName, "X");
    strcpy(data.manufacturer, "Y");

    uint8_t buf[DAC_EEPROM_DATA_SIZE];
    test_dac_eeprom_serialize(&data, buf, sizeof(buf));

    TEST_ASSERT_EQUAL_UINT8(0x07, buf[0x4A]);

    // Parse back to verify
    DacEepromData reparsed;
    test_dac_eeprom_parse(buf, DAC_EEPROM_DATA_SIZE, &reparsed);
    TEST_ASSERT_TRUE(reparsed.flags & DAC_FLAG_INDEPENDENT_CLOCK);
    TEST_ASSERT_TRUE(reparsed.flags & DAC_FLAG_HW_VOLUME);
    TEST_ASSERT_TRUE(reparsed.flags & DAC_FLAG_FILTERS);
}

void test_serialize_max_sample_rates(void) {
    DacEepromData data;
    memset(&data, 0, sizeof(data));
    strcpy(data.deviceName, "Test");
    strcpy(data.manufacturer, "Mfr");
    data.numSampleRates = 4;
    data.sampleRates[0] = 44100;
    data.sampleRates[1] = 48000;
    data.sampleRates[2] = 96000;
    data.sampleRates[3] = 192000;

    uint8_t buf[DAC_EEPROM_DATA_SIZE];
    int written = test_dac_eeprom_serialize(&data, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(DAC_EEPROM_DATA_SIZE, written);

    DacEepromData reparsed;
    test_dac_eeprom_parse(buf, DAC_EEPROM_DATA_SIZE, &reparsed);
    TEST_ASSERT_EQUAL_UINT8(4, reparsed.numSampleRates);
    TEST_ASSERT_EQUAL_UINT32(192000, reparsed.sampleRates[3]);
}

void test_serialize_name_truncation(void) {
    DacEepromData data;
    memset(&data, 0, sizeof(data));
    // Fill with 40 chars — should be truncated to 32
    memset(data.deviceName, 'Z', 32);
    data.deviceName[32] = '\0';
    strcpy(data.manufacturer, "Short");

    uint8_t buf[DAC_EEPROM_DATA_SIZE];
    test_dac_eeprom_serialize(&data, buf, sizeof(buf));

    // Verify all 32 bytes are 'Z'
    for (int i = 0; i < 32; i++) {
        TEST_ASSERT_EQUAL_UINT8('Z', buf[0x08 + i]);
    }

    // Parse back — should be 32 chars
    DacEepromData reparsed;
    test_dac_eeprom_parse(buf, DAC_EEPROM_DATA_SIZE, &reparsed);
    TEST_ASSERT_EQUAL(32, (int)strlen(reparsed.deviceName));
}

void test_serialize_magic_and_version(void) {
    DacEepromData data;
    memset(&data, 0, sizeof(data));
    strcpy(data.deviceName, "Test");
    strcpy(data.manufacturer, "Mfr");

    uint8_t buf[DAC_EEPROM_DATA_SIZE];
    test_dac_eeprom_serialize(&data, buf, sizeof(buf));

    // Verify magic
    TEST_ASSERT_EQUAL_UINT8('A', buf[0]);
    TEST_ASSERT_EQUAL_UINT8('L', buf[1]);
    TEST_ASSERT_EQUAL_UINT8('X', buf[2]);
    TEST_ASSERT_EQUAL_UINT8('D', buf[3]);
    // Verify version
    TEST_ASSERT_EQUAL_UINT8(1, buf[4]);
}

void test_serialize_rate_count_clamped(void) {
    DacEepromData data;
    memset(&data, 0, sizeof(data));
    strcpy(data.deviceName, "Test");
    strcpy(data.manufacturer, "Mfr");
    data.numSampleRates = 10; // More than max

    uint8_t buf[DAC_EEPROM_DATA_SIZE];
    test_dac_eeprom_serialize(&data, buf, sizeof(buf));

    // Should be clamped to 4
    TEST_ASSERT_EQUAL_UINT8(DAC_EEPROM_MAX_RATES, buf[0x4B]);
}

void test_serialize_dac_i2c_address(void) {
    DacEepromData data;
    memset(&data, 0, sizeof(data));
    strcpy(data.deviceName, "Test");
    strcpy(data.manufacturer, "Mfr");
    data.dacI2cAddress = 0x48;
    data.maxChannels = 6;

    uint8_t buf[DAC_EEPROM_DATA_SIZE];
    test_dac_eeprom_serialize(&data, buf, sizeof(buf));

    DacEepromData reparsed;
    test_dac_eeprom_parse(buf, DAC_EEPROM_DATA_SIZE, &reparsed);
    TEST_ASSERT_EQUAL_UINT8(0x48, reparsed.dacI2cAddress);
    TEST_ASSERT_EQUAL_UINT8(6, reparsed.maxChannels);
}

// ===== Main =====
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    // Parse tests
    RUN_TEST(test_eeprom_valid_magic);
    RUN_TEST(test_eeprom_invalid_magic);
    RUN_TEST(test_eeprom_wrong_version);
    RUN_TEST(test_eeprom_device_id_little_endian);
    RUN_TEST(test_eeprom_device_name);
    RUN_TEST(test_eeprom_manufacturer);
    RUN_TEST(test_eeprom_null_terminated_strings);
    RUN_TEST(test_eeprom_flags_hw_volume);
    RUN_TEST(test_eeprom_flags_all);
    RUN_TEST(test_eeprom_sample_rates);
    RUN_TEST(test_eeprom_too_short);
    RUN_TEST(test_eeprom_null_input);
    RUN_TEST(test_eeprom_null_output);
    RUN_TEST(test_eeprom_max_channels);
    RUN_TEST(test_eeprom_i2c_address);
    RUN_TEST(test_eeprom_hw_revision);
    RUN_TEST(test_eeprom_rate_count_clamped);

    // Serialize tests
    RUN_TEST(test_serialize_round_trip);
    RUN_TEST(test_serialize_endianness);
    RUN_TEST(test_serialize_null_data);
    RUN_TEST(test_serialize_null_buffer);
    RUN_TEST(test_serialize_short_buffer);
    RUN_TEST(test_serialize_flags_all);
    RUN_TEST(test_serialize_max_sample_rates);
    RUN_TEST(test_serialize_name_truncation);
    RUN_TEST(test_serialize_magic_and_version);
    RUN_TEST(test_serialize_rate_count_clamped);
    RUN_TEST(test_serialize_dac_i2c_address);

    return UNITY_END();
}
