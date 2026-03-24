#ifndef HAL_EEPROM_API_H
#define HAL_EEPROM_API_H

#ifdef DAC_ENABLED

#include <WebServer.h>

// Register EEPROM REST API endpoints under /api/hal/eeprom
void registerHalEepromApiEndpoints();

#endif // DAC_ENABLED
#endif // HAL_EEPROM_API_H
