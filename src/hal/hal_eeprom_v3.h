#pragma once
// HAL EEPROM v3 — extends EEPROM format with compatible string + CRC-16
// Phase 2: Backward compatible with v1/v2

#ifdef DAC_ENABLED

#include <stdint.h>

// v3 EEPROM layout extension (offsets within existing 256-byte EEPROM):
// 0x00-0x5D  v1/v2 data (unchanged)
// 0x5E  32B  Compatible string (null-terminated, e.g. "evergrande,es8311")
// 0x7E  2B   CRC-16/CCITT over bytes 0x00-0x7D
// 0x80-0xFF  Reserved (driver-specific data, user notes, etc.)

#define DAC_EEPROM_VERSION_V3      3
#define DAC_EEPROM_V3_COMPAT_OFF   0x5E
#define DAC_EEPROM_V3_COMPAT_LEN   32
#define DAC_EEPROM_V3_CRC_OFF      0x7E
#define DAC_EEPROM_V3_DATA_SIZE    0x80   // 128 bytes

// CRC-16/CCITT (0x1021 polynomial, 0xFFFF init)
uint16_t hal_crc16_ccitt(const uint8_t* data, int len);

// Parse v3 extension fields from raw EEPROM data
// rawData must be at least 128 bytes (0x80)
// Fills compatible[32] and validates CRC. Returns true if CRC matches.
bool hal_eeprom_parse_v3(const uint8_t* rawData, int len, char* compatible);

// Serialize v3 fields into raw EEPROM data
// Writes compatible string at offset 0x5E and CRC-16 at 0x7E
// rawData must already contain v1/v2 data at 0x00-0x5D, with version set to 3
// rawData buffer must be at least 128 bytes
bool hal_eeprom_serialize_v3(uint8_t* rawData, int len, const char* compatible);

#endif // DAC_ENABLED
