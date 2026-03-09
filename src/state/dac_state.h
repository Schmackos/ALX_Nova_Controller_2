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
struct DacState {
  // Primary DAC
  bool enabled = false;
  uint8_t volume = 80;            // 0-100 percent
  bool mute = false;
  uint16_t deviceId = 0x0001;     // DAC_ID_PCM5102A default
  char modelName[33] = "PCM5102A";
  uint8_t outputChannels = 2;     // From driver capabilities
  bool detected = false;          // EEPROM or manual selection made
  bool ready = false;             // Driver init + I2S TX active
  uint8_t filterMode = 0;         // Digital filter mode (DAC-specific)
  uint32_t txUnderruns = 0;       // TX DMA full count

  // ES8311 secondary DAC (P4 onboard codec + NS4150B speaker amp)
  bool es8311Enabled = false;
  uint8_t es8311Volume = 80;      // 0-100 (hardware volume via I2C)
  bool es8311Mute = false;
  bool es8311Ready = false;

  // Deferred toggle flags — main loop executes actual init/deinit
  volatile int8_t pendingEs8311Toggle = 0;  // 0=none, 1=init, -1=deinit
  volatile int8_t pendingDacToggle = 0;     // 0=none, 1=init, -1=deinit

  // Validated setters — direct dev->deinit() is unsafe (audio task race),
  // so all toggle requests go through deferred flags consumed by main loop
  void requestDacToggle(int8_t action) {
    if (action >= -1 && action <= 1) pendingDacToggle = action;
  }
  void requestEs8311Toggle(int8_t action) {
    if (action >= -1 && action <= 1) pendingEs8311Toggle = action;
  }

  // EEPROM diagnostics
  EepromDiag eepromDiag;
};

#endif // STATE_DAC_STATE_H
