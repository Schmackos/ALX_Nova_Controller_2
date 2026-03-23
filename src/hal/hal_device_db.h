#pragma once
// HAL Device Database — local JSON device database on LittleFS
// Phase 2: Maps compatible strings to device descriptors

#ifdef DAC_ENABLED

#include "hal_types.h"

#define HAL_DB_MAX_ENTRIES 48
#define HAL_DB_FILE_PATH "/hal_devices.json"
#define HAL_CONFIG_FILE_PATH "/hal_config.json"
#define HAL_CONFIG_TMP_PATH  "/hal_config.json.tmp"

// Initialize the device DB (loads from LittleFS if available)
void hal_db_init();

// Lookup a device descriptor by compatible string
// Returns true if found, fills out descriptor
bool hal_db_lookup(const char* compatible, HalDeviceDescriptor* out);

// Add or update a device entry
bool hal_db_add(const HalDeviceDescriptor* desc);

// Remove a device entry by compatible string
bool hal_db_remove(const char* compatible);

// Save the database to LittleFS
bool hal_db_save();

// Get the number of entries in the database
int hal_db_count();

// Get the maximum number of entries
int hal_db_max();

// Get entry by index (for iteration)
const HalDeviceDescriptor* hal_db_get(int index);

// Load per-device runtime configs from /hal_config.json
void hal_load_device_configs();

// Save per-device runtime config
bool hal_save_device_config(uint8_t slot);

// Reset database (for testing)
void hal_db_reset();

// Write /hal_auto_devices.json with board defaults if the file does not yet exist (first boot)
void hal_provision_defaults();

// Read /hal_auto_devices.json and register each entry via the HAL factory + driver registry
void hal_load_auto_devices();

#endif // DAC_ENABLED
