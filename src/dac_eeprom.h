#ifndef DAC_EEPROM_H
#define DAC_EEPROM_H

#ifdef DAC_ENABLED

#include <stdint.h>

// ===== EEPROM Format (AT24C02, 256 bytes, I2C 0x50-0x57) =====
#define DAC_EEPROM_MAGIC      "ALXD"
#define DAC_EEPROM_MAGIC_LEN  4
#define DAC_EEPROM_VERSION    1
#define DAC_EEPROM_ADDR_START 0x50
#define DAC_EEPROM_ADDR_END   0x57
#define DAC_EEPROM_MAX_RATES  4

// EEPROM flags byte (offset 0x4A)
#define DAC_FLAG_INDEPENDENT_CLOCK  0x01
#define DAC_FLAG_HW_VOLUME          0x02
#define DAC_FLAG_FILTERS            0x04

// Parsed EEPROM data
struct DacEepromData {
    bool valid;                   // Magic + version OK
    uint8_t formatVersion;        // Format version (must be 1)
    uint16_t deviceId;            // Device ID (uint16_t LE)
    uint8_t hwRevision;           // Hardware revision
    char deviceName[33];          // Null-terminated (32 chars + null)
    char manufacturer[33];        // Null-terminated (32 chars + null)
    uint8_t maxChannels;          // Max output channels
    uint8_t dacI2cAddress;        // DAC I2C address (0 = none)
    uint8_t flags;                // Bit flags: independent clock, HW volume, filters
    uint8_t numSampleRates;       // Number of supported sample rates (max 4)
    uint32_t sampleRates[DAC_EEPROM_MAX_RATES]; // Supported sample rates
    uint8_t i2cAddress;           // EEPROM address where found
};

// EEPROM data size (92 bytes = 0x5C)
#define DAC_EEPROM_DATA_SIZE  0x5C
// AT24C02 total size and page size
#define DAC_EEPROM_TOTAL_SIZE 256
#define DAC_EEPROM_PAGE_SIZE  8

// Parse raw EEPROM bytes into DacEepromData
// rawData must be at least 92 bytes (0x5C)
bool dac_eeprom_parse(const uint8_t* rawData, int len, DacEepromData* out);

// Serialize DacEepromData into raw EEPROM bytes (inverse of parse)
// outBuf must be at least DAC_EEPROM_DATA_SIZE (92) bytes
// Returns number of bytes written (92) on success, 0 on failure
int dac_eeprom_serialize(const DacEepromData* data, uint8_t* outBuf, int bufLen);

// Scan I2C bus for EEPROM with ALXD magic (requires Wire to be initialized)
// Returns true if a valid EEPROM is found, fills out with parsed data
bool dac_eeprom_scan(DacEepromData* out);

// Read raw bytes from EEPROM (public wrapper for hex dump diagnostics)
bool dac_eeprom_read_raw(uint8_t i2cAddr, uint8_t memAddr, uint8_t* buf, int len);

// Write data to AT24C02 EEPROM with page-aware writes + read-back verification
// Returns true on success (all bytes verified)
bool dac_eeprom_write(uint8_t i2cAddr, const uint8_t* data, int len);

// Erase EEPROM (fill all 256 bytes with 0xFF)
bool dac_eeprom_erase(uint8_t i2cAddr);

// Full I2C bus scan (0x08-0x77). Returns count of devices found.
// eepromMask: bitmask of which 0x50-0x57 addresses ACK'd
int dac_i2c_scan(uint8_t* eepromMask);

#endif // DAC_ENABLED
#endif // DAC_EEPROM_H
