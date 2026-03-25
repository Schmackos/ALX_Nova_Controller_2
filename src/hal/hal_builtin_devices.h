#pragma once
// HAL Builtin Devices — registers onboard hardware with HAL
// Phase 1: Registers PCM5102A, ES8311, PCM1808 ADCs as HAL devices

#ifdef DAC_ENABLED

// Register all builtin driver entries in the HAL registry
void hal_register_builtins();

// Wire known static dependencies between onboard devices.
// Call after all onboard devices have been registered with the HalDeviceManager.
// Sets _dependsOn bitmasks for REST/WS API and initAll() topological sort.
void hal_wire_builtin_dependencies();

#endif // DAC_ENABLED
