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

// Generic device toggle request — device-independent, works with any HAL device
struct PendingDeviceToggle {
  uint8_t halSlot = 0xFF;  // 0xFF = none/invalid
  int8_t action = 0;       // 1=enable, -1=disable, 0=none
};

// DAC output state (guarded by DAC_ENABLED at the AppState level)
// Device-specific fields (enabled, volume, mute, etc.) live in HalDeviceConfig
// via the HAL device manager. DacState retains only cross-cutting concerns.
struct DacState {
  uint32_t txUnderruns = 0;       // TX DMA full count (diagnostic counter)

  // Generic deferred toggle — main loop executes actual activation/deactivation
  volatile PendingDeviceToggle pendingToggle = {};

  // Generic validated setter — routes through HAL slot, zero device-type knowledge
  // Direct dev->deinit() is unsafe (audio task race), so all toggle requests use this
  void requestDeviceToggle(uint8_t halSlot, int8_t action) {
    if (halSlot < 0xFF && action >= -1 && action <= 1) {
      pendingToggle.halSlot = halSlot;
      pendingToggle.action = action;
    }
  }

  // EEPROM diagnostics
  EepromDiag eepromDiag;
};

#endif // STATE_DAC_STATE_H
