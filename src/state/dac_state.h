#ifndef STATE_DAC_STATE_H
#define STATE_DAC_STATE_H

#include "config.h"

// EEPROM diagnostic data (read from HAL EEPROM probes)
struct EepromDiag {
  bool scanned = false;           // Has a scan been performed?
  bool found = false;             // Was a valid ALXD EEPROM found?
  uint8_t eepromAddr = 0;         // I2C address where EEPROM was found
  uint8_t i2cDevicesMask = 0;     // Bitmask of 0x50-0x57 that ACK'd
  int i2cTotalDevices = 0;        // Total I2C devices found on bus
  uint32_t readErrors = 0;
  uint32_t writeErrors = 0;
  unsigned long lastScanMs = 0;
  // Parsed EEPROM fields (duplicated for WS/GUI access without re-reading)
  uint16_t deviceId = 0;
  uint8_t hwRevision = 0;
  char deviceName[33] = {};
  char manufacturer[33] = {};
  uint8_t maxChannels = 0;
  uint8_t dacI2cAddress = 0;
  uint8_t flags = 0;
  uint8_t numSampleRates = 0;
  uint32_t sampleRates[4] = {};
};

// DAC output state (guarded by DAC_ENABLED at the AppState level)
// Device-specific fields (enabled, volume, mute, etc.) live in HalDeviceConfig
// via the HAL device manager. DacState retains only DAC-specific concerns.
// Device toggle queue moved to HalCoordState (src/state/hal_coord_state.h).
struct DacState {
  uint32_t txUnderruns = 0;       // TX DMA full count (diagnostic counter)

  // EEPROM diagnostics
  EepromDiag eepromDiag;
};

#endif // STATE_DAC_STATE_H
