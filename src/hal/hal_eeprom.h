#pragma once
// hal_eeprom.h — Generic HAL EEPROM alias header
// Wraps legacy dac_eeprom.h so new HAL code can use hal_eeprom_* names.
// Old code that includes dac_eeprom.h directly continues to work unchanged.

#include "../dac_eeprom.h"

// Type alias: HalEepromData is the canonical name for new code;
// DacEepromData remains available for backward compatibility.
using HalEepromData = DacEepromData;
