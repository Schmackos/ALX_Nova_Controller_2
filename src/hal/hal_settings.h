#pragma once
// HAL Settings — persist per-device configuration across reboots
// Phase 4: Deferred save, export/import for backup/restore
// Note: hal_load_device_configs() and hal_save_device_config() are in hal_device_db.h

#include <stdint.h>
#include <stddef.h>
#include "hal_types.h"

#ifdef DAC_ENABLED

// Save a specific device slot's config (debounced)
void hal_save_device_config_deferred(uint8_t slot);

// Check and flush pending saves (call from main loop)
void hal_check_deferred_save();

// Save all device configs immediately
void hal_save_all_configs();

// Export all configs as a single JSON blob (for backup)
// Returns true if successful, writes to provided buffer
bool hal_export_configs(char* buf, size_t bufSize);

// Import configs from JSON blob (for restore)
bool hal_import_configs(const char* json, size_t len);

// Returns config for the first registered device of the given type, or nullptr if none/invalid
HalDeviceConfig* hal_get_config_for_type(HalDeviceType type);

// Apply saved HalDeviceConfig to hardware immediately (hot reconfigure).
void hal_apply_config(uint8_t slot);

#endif // DAC_ENABLED
