#pragma once
// HAL Builtin Devices — registers onboard hardware with HAL
// Phase 1: Registers PCM5102A, ES8311, PCM1808 ADCs as HAL devices

#ifdef DAC_ENABLED

// Register all builtin driver entries in the HAL registry
void hal_register_builtins();

#endif // DAC_ENABLED
