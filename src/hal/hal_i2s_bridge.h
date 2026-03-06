#pragma once
// Shared I2S channel initialisation helpers — used by platform classes
// Phase 1-3: New file — no existing files modified

#ifdef DAC_ENABLED
#ifndef NATIVE_TEST
#include <driver/i2s_std.h>
#endif
#include "hal_types.h"

// Initialise an I2S TX channel using HalDeviceConfig pin/rate settings.
// Falls back to config.h constants when config values are 0/-1.
bool hal_i2s_tx_init(void** txHandle, const HalDeviceConfig* cfg);

// Initialise an I2S RX channel.
bool hal_i2s_rx_init(void** rxHandle, const HalDeviceConfig* cfg);

// Reconfigure sample rate + bit depth on an active channel.
bool hal_i2s_reconfigure(void* handle, uint32_t rate, uint8_t bits);

#endif // DAC_ENABLED
