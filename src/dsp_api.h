#ifndef DSP_API_H
#define DSP_API_H

#ifdef DSP_ENABLED

// Register all DSP REST API endpoints on the global web server
void registerDspApiEndpoints();

// DSP settings persistence
void loadDspSettings();
void saveDspSettings();
void saveDspSettingsDebounced();

// Call from main loop to flush pending debounced saves
void dsp_check_debounced_save();

// DSP Preset management (4 slots)
bool dsp_preset_save(int slot, const char *name);
bool dsp_preset_load(int slot);
bool dsp_preset_delete(int slot);
bool dsp_preset_exists(int slot);

// Routing matrix access (for preset export/import)
struct DspRoutingMatrix;
DspRoutingMatrix* dsp_get_routing_matrix();

#endif // DSP_ENABLED
#endif // DSP_API_H
