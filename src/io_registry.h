#ifndef IO_REGISTRY_H
#define IO_REGISTRY_H

#ifdef DAC_ENABLED

#include <stdint.h>
#include <stdbool.h>

// Maximum slots
#define IO_MAX_OUTPUTS 4
#define IO_MAX_INPUTS  4

// Discovery method
enum IoDiscovery : uint8_t {
    IO_DISC_BUILTIN = 0,   // Hardcoded in firmware (PCM5102A, ES8311, ADCs)
    IO_DISC_EEPROM  = 1,   // Detected via EEPROM scan at boot
    IO_DISC_MANUAL  = 2    // User-added via REST API
};

// Output entry
struct IoOutputEntry {
    bool     active;                // Slot in use
    uint8_t  index;                 // Slot index (0-3)
    uint16_t deviceId;              // DAC_ID_PCM5102A, DAC_ID_ES8311, etc.
    char     name[33];              // Display name (32 chars + null)
    uint8_t  deviceType;            // DAC_DEVICE_TYPE_DAC / ADC / CODEC
    IoDiscovery discovery;          // How this entry was discovered
    uint8_t  i2sPort;               // I2S peripheral: 0, 1, or 2
    uint8_t  channelCount;          // Number of channels (usually 2)
    uint8_t  firstOutputChannel;    // First matrix output channel (slot*2)
    bool     ready;                 // Driver initialized and I2S TX active
};

// Input entry
struct IoInputEntry {
    bool     active;                // Slot in use
    uint8_t  index;                 // Slot index (0-3)
    char     name[33];              // Display name
    IoDiscovery discovery;          // How this entry was discovered
    uint8_t  i2sPort;               // I2S peripheral: 0, 1, or 2
    uint8_t  channelCount;          // Number of channels (usually 2)
    uint8_t  firstInputChannel;     // First matrix input channel (slot*2)
};

// ===== Public API =====

// Initialize registry: register builtins, load manual entries, check EEPROM cache
void io_registry_init();

// Get output/input arrays (IO_MAX_OUTPUTS / IO_MAX_INPUTS entries)
const IoOutputEntry* io_registry_get_outputs();
const IoInputEntry*  io_registry_get_inputs();

// Get active counts
int io_registry_output_count();
int io_registry_input_count();

// Add a manual output entry. Returns slot index (0-3) or -1 if full.
int io_registry_add_output(const char* name, uint8_t i2sPort, uint16_t deviceId, uint8_t channelCount);

// Remove an output entry by index. Returns false if builtin (cannot remove) or invalid.
bool io_registry_remove_output(uint8_t index);

// Persist manual entries to LittleFS /io_registry.json
void io_registry_save();

// Load manual entries from LittleFS /io_registry.json
void io_registry_load();

#endif // DAC_ENABLED
#endif // IO_REGISTRY_H
