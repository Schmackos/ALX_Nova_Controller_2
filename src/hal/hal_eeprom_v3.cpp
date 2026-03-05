#ifdef DAC_ENABLED

#include "hal_eeprom_v3.h"
#include <string.h>

// CRC-16/CCITT — 0x1021 polynomial, 0xFFFF initial value
uint16_t hal_crc16_ccitt(const uint8_t* data, int len) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc = crc << 1;
        }
    }
    return crc;
}

bool hal_eeprom_parse_v3(const uint8_t* rawData, int len, char* compatible) {
    if (!rawData || !compatible || len < DAC_EEPROM_V3_DATA_SIZE) {
        if (compatible) compatible[0] = '\0';
        return false;
    }

    // Validate CRC over 0x00-0x7D (126 bytes)
    uint16_t stored_crc = (uint16_t)rawData[DAC_EEPROM_V3_CRC_OFF] |
                          ((uint16_t)rawData[DAC_EEPROM_V3_CRC_OFF + 1] << 8);
    uint16_t calc_crc = hal_crc16_ccitt(rawData, DAC_EEPROM_V3_CRC_OFF);

    if (stored_crc != calc_crc) {
        compatible[0] = '\0';
        return false;
    }

    // Extract compatible string (32 bytes at offset 0x5E)
    memcpy(compatible, &rawData[DAC_EEPROM_V3_COMPAT_OFF], DAC_EEPROM_V3_COMPAT_LEN);
    compatible[DAC_EEPROM_V3_COMPAT_LEN - 1] = '\0';  // Ensure null-terminated
    return true;
}

bool hal_eeprom_serialize_v3(uint8_t* rawData, int len, const char* compatible) {
    if (!rawData || !compatible || len < DAC_EEPROM_V3_DATA_SIZE) return false;

    // Write compatible string at offset 0x5E (zero-padded)
    memset(&rawData[DAC_EEPROM_V3_COMPAT_OFF], 0, DAC_EEPROM_V3_COMPAT_LEN);
    strncpy((char*)&rawData[DAC_EEPROM_V3_COMPAT_OFF], compatible, DAC_EEPROM_V3_COMPAT_LEN - 1);

    // Compute CRC-16 over 0x00-0x7D and write at 0x7E (little-endian)
    uint16_t crc = hal_crc16_ccitt(rawData, DAC_EEPROM_V3_CRC_OFF);
    rawData[DAC_EEPROM_V3_CRC_OFF] = crc & 0xFF;
    rawData[DAC_EEPROM_V3_CRC_OFF + 1] = (crc >> 8) & 0xFF;

    return true;
}

#endif // DAC_ENABLED
