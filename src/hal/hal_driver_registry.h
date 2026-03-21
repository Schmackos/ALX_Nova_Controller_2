#pragma once
// HAL Driver Registry — maps compatible strings to device factories
// Phase 0: Purely additive — no existing files modified

#include "hal_types.h"
#include "hal_device.h"

// Factory function type — creates a new HalDevice on the heap
typedef HalDevice* (*HalDeviceFactory)();

// Registry entry
struct HalDriverEntry {
    char             compatible[32];  // "vendor,model" match key
    HalDeviceType    type;
    uint16_t         legacyId;        // DAC_ID_* for backward compat (0 = none)
    HalDeviceFactory factory;
};

// C-style API (matches codebase convention)
void                   hal_registry_init();
bool                   hal_registry_register(const HalDriverEntry& entry);
const HalDriverEntry*  hal_registry_find(const char* compatible);
// Discovery-only: used for EEPROM v1/v2 backward compatibility lookup.
// Do not call outside of hal_discovery.cpp.
const HalDriverEntry*  hal_registry_find_by_legacy_id(uint16_t legacyId);
int                    hal_registry_count();
int                    hal_registry_max();
void                   hal_registry_reset();  // For testing
