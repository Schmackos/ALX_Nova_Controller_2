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

#endif // DSP_ENABLED
#endif // DSP_API_H
