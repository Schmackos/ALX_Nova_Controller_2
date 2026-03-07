/**
 * General settings routes — mirrors /api/settings endpoints from settings_manager.cpp.
 * Mounted at /api in server.js.
 *
 * GET  /settings         — bulk settings object
 * POST /settings         — save settings
 * GET  /settings/export  — full settings JSON export
 * POST /settings/import  — import settings JSON
 * GET  /inputnames       — input channel names
 * POST /inputnames       — save input channel names
 */

const express = require('express');
const { getState } = require('../ws-state');

const router = express.Router();

/**
 * Build the full settings payload that the firmware's handleSettingsGet() sends.
 * Key names use the AppState field names (with dot notation where relevant).
 */
function buildSettingsPayload(state) {
  return {
    firmwareVersion: state.firmwareVersion,
    deviceSerialNumber: 'ALX-AABBCCDDEEFF',
    // Display / buzzer
    buzzerEnabled: state.buzzerEnabled,
    buzzerVolume: state.buzzerVolume,
    backlightOn: state.backlightOn,
    screenTimeout: state.screenTimeout,
    backlightBrightness: state.backlightBrightness,
    dimEnabled: state.dimEnabled,
    dimTimeout: state.dimTimeout,
    dimBrightness: state.dimBrightness,
    darkMode: state.darkMode,
    // Audio
    audioUpdateRate: 100,
    hardwareStatsInterval: 2000,
    vuMeterEnabled: state.vuMeterEnabled,
    waveformEnabled: state.waveformEnabled,
    spectrumEnabled: state.spectrumEnabled,
    fftWindowType: state.fftWindowType,
    // Debug
    debugMode: state.debugLevel > 0,
    debugSerialLevel: state.debugLevel,
    debugTaskMonitor: state.debugTaskMonitor,
    // OTA
    autoUpdateEnabled: false,
    // HAL
    halAutoDiscovery: true,
    // USB Audio
    usbAudioEnabled: false,
    // DSP
    dspEnabled: true,
    dspBypass: false,
  };
}

// GET /settings — return bulk settings object
router.get('/settings', (req, res) => {
  const state = getState();
  res.json(buildSettingsPayload(state));
});

// POST /settings — save settings (partial update accepted)
router.post('/settings', (req, res) => {
  const state = getState();
  const body = req.body || {};
  // Apply known mutable fields
  if (body.buzzerEnabled !== undefined)    state.buzzerEnabled = body.buzzerEnabled;
  if (body.buzzerVolume !== undefined)     state.buzzerVolume = body.buzzerVolume;
  if (body.backlightOn !== undefined)      state.backlightOn = body.backlightOn;
  if (body.screenTimeout !== undefined)    state.screenTimeout = body.screenTimeout;
  if (body.backlightBrightness !== undefined) state.backlightBrightness = body.backlightBrightness;
  if (body.dimEnabled !== undefined)       state.dimEnabled = body.dimEnabled;
  if (body.dimTimeout !== undefined)       state.dimTimeout = body.dimTimeout;
  if (body.dimBrightness !== undefined)    state.dimBrightness = body.dimBrightness;
  if (body.darkMode !== undefined)         state.darkMode = body.darkMode;
  if (body.debugSerialLevel !== undefined) state.debugLevel = body.debugSerialLevel;
  if (body.debugTaskMonitor !== undefined) state.debugTaskMonitor = body.debugTaskMonitor;
  if (body.vuMeterEnabled !== undefined)   state.vuMeterEnabled = body.vuMeterEnabled;
  if (body.waveformEnabled !== undefined)  state.waveformEnabled = body.waveformEnabled;
  if (body.spectrumEnabled !== undefined)  state.spectrumEnabled = body.spectrumEnabled;
  if (body.fftWindowType !== undefined)    state.fftWindowType = body.fftWindowType;
  res.json({ success: true });
});

// GET /settings/export — full settings JSON dump
router.get('/settings/export', (req, res) => {
  const state = getState();
  const payload = buildSettingsPayload(state);
  res.setHeader('Content-Disposition', 'attachment; filename="alx_settings.json"');
  res.json(payload);
});

// POST /settings/import — import a settings JSON blob
router.post('/settings/import', (req, res) => {
  // Accept any valid JSON body and return success — mock does not persist
  if (!req.body || typeof req.body !== 'object') {
    return res.status(400).json({ success: false, message: 'Invalid JSON' });
  }
  res.json({ success: true });
});

// GET /inputnames — input channel name array
router.get('/inputnames', (req, res) => {
  res.json({
    success: true,
    names: ['ADC1 L', 'ADC1 R', 'ADC2 L', 'ADC2 R', 'SigGen L', 'SigGen R', 'USB L', 'USB R'],
    numAdcsDetected: 2,
  });
});

// POST /inputnames — save input channel names
router.post('/inputnames', (req, res) => {
  // Accept and ignore in mock — no persistent state for names
  res.json({ success: true });
});

module.exports = router;
