#ifdef DAC_ENABLED

#include "dac_registry.h"
#include "drivers/dac_pcm5102.h"
#include <string.h>

// ===== Compile-time DAC Registry =====
// Add new drivers here. Factory functions must be declared in the driver header.
static const DacRegistryEntry DAC_REGISTRY[] = {
    { DAC_ID_PCM5102A,  "PCM5102A",  createDacPcm5102 },
    // Future: { DAC_ID_ES9038Q2M, "ES9038Q2M", createDacEs9038 },
    // Future: { DAC_ID_ES9842,    "ES9842",    createDacEs9842 },
};

static const int DAC_REGISTRY_COUNT = sizeof(DAC_REGISTRY) / sizeof(DAC_REGISTRY[0]);

const DacRegistryEntry* dac_registry_get_entries() {
    return DAC_REGISTRY;
}

int dac_registry_get_count() {
    return DAC_REGISTRY_COUNT;
}

const DacRegistryEntry* dac_registry_find_by_id(uint16_t deviceId) {
    for (int i = 0; i < DAC_REGISTRY_COUNT; i++) {
        if (DAC_REGISTRY[i].deviceId == deviceId) {
            return &DAC_REGISTRY[i];
        }
    }
    return nullptr;
}

const DacRegistryEntry* dac_registry_find_by_name(const char* name) {
    if (!name) return nullptr;
    for (int i = 0; i < DAC_REGISTRY_COUNT; i++) {
        if (strcmp(DAC_REGISTRY[i].name, name) == 0) {
            return &DAC_REGISTRY[i];
        }
    }
    return nullptr;
}

#endif // DAC_ENABLED
