#ifndef DAC_REGISTRY_H
#define DAC_REGISTRY_H

#ifdef DAC_ENABLED

#include "dac_hal.h"

// Factory function type — creates a new driver instance on the heap
typedef DacDriver* (*DacFactoryFn)();

// Registry entry
struct DacRegistryEntry {
    uint16_t deviceId;
    const char* name;
    DacFactoryFn factory;
};

// Get the compile-time registry array and its size
const DacRegistryEntry* dac_registry_get_entries();
int dac_registry_get_count();

// Lookup helpers
const DacRegistryEntry* dac_registry_find_by_id(uint16_t deviceId);
const DacRegistryEntry* dac_registry_find_by_name(const char* name);

#endif // DAC_ENABLED
#endif // DAC_REGISTRY_H
