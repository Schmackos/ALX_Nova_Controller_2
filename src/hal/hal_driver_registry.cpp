#include "hal_driver_registry.h"
#include <string.h>

// Static registry storage
static HalDriverEntry _entries[HAL_MAX_DRIVERS];
static int _entryCount = 0;

void hal_registry_init() {
    memset(_entries, 0, sizeof(_entries));
    _entryCount = 0;
}

bool hal_registry_register(const HalDriverEntry& entry) {
    if (_entryCount >= HAL_MAX_DRIVERS) return false;
    if (entry.compatible[0] == '\0') return false;

    // Reject duplicates
    for (int i = 0; i < _entryCount; i++) {
        if (strcmp(_entries[i].compatible, entry.compatible) == 0) return false;
    }

    _entries[_entryCount] = entry;
    _entryCount++;
    return true;
}

const HalDriverEntry* hal_registry_find(const char* compatible) {
    if (!compatible) return nullptr;
    for (int i = 0; i < _entryCount; i++) {
        if (strcmp(_entries[i].compatible, compatible) == 0) {
            return &_entries[i];
        }
    }
    return nullptr;
}

const HalDriverEntry* hal_registry_find_by_legacy_id(uint16_t legacyId) {
    if (legacyId == 0) return nullptr;
    for (int i = 0; i < _entryCount; i++) {
        if (_entries[i].legacyId == legacyId) {
            return &_entries[i];
        }
    }
    return nullptr;
}

int hal_registry_count() {
    return _entryCount;
}

void hal_registry_reset() {
    hal_registry_init();
}
