#ifndef STATE_GENERAL_STATE_H
#define STATE_GENERAL_STATE_H

#include "config.h"

// General settings that don't belong to any specific subsystem
struct GeneralState {
  int timezoneOffset = 0;
  int dstOffset = 0;
  bool darkMode = false;
  bool enableCertValidation = true;
  String deviceSerialNumber;
  bool factoryResetInProgress = false;
};

#endif // STATE_GENERAL_STATE_H
