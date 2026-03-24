/**
 * HAL device management routes — mirrors /api/hal/* endpoints from hal_api.cpp.
 * Mounted at /api/hal in server.js.
 *
 * GET  /devices         — list all registered HAL devices
 * POST /scan            — trigger rescan (409 if already scanning)
 * PUT  /devices         — update device config
 * DELETE /devices       — remove a device
 * POST /devices/reinit  — re-initialise a device
 * GET  /db/presets      — list device database presets
 * GET  /settings        — HAL auto-discovery toggle
 * PUT  /settings        — save HAL auto-discovery toggle
 * GET  /devices/custom  — list custom device schemas
 * POST /devices/custom  — upload a custom device schema
 * DELETE /devices/custom — remove a custom device schema
 */

const express = require('express');
const { getState } = require('../ws-state');

const router = express.Router();

// Preset device database entries (mirrors hal_device_db builtin list)
const DB_PRESETS = [
  { compatible: 'analog,pcm5102a', name: 'PCM5102A', type: 'DAC' },
  { compatible: 'ti,es8311',       name: 'ES8311',   type: 'CODEC' },
  { compatible: 'ti,pcm1808',      name: 'PCM1808',  type: 'ADC' },
  { compatible: 'ns,ns4150b',      name: 'NS4150B',  type: 'AMP' },
  { compatible: 'espressif,temp-sensor', name: 'Chip Temp', type: 'SENSOR' },
];

// GET /devices — list all registered HAL devices
router.get('/devices', (req, res) => {
  const state = getState();
  res.json(state.halDevices);
});

// POST /scan — trigger device rescan with 409 guard
router.post('/scan', (req, res) => {
  const state = getState();
  if (state.scanning) {
    return res.status(409).json({ error: 'Scan already in progress' });
  }
  // Simulate a quick scan returning no new devices
  state.scanning = true;
  setTimeout(() => { state.scanning = false; }, 500);
  res.json({ status: 'ok', devicesFound: state.halDevices.length, partialScan: false });
});

// Valid ADC config ranges (mirrors firmware hal_api.cpp validation)
const VALID_PGA_GAIN_STEPS = [0, 6, 12, 18, 24, 30, 36, 42];

// PUT /devices — update device config by slot
router.put('/devices', (req, res) => {
  const state = getState();
  const { slot } = req.body || {};
  if (slot === undefined || slot === null) {
    return res.status(400).json({ error: 'Invalid slot' });
  }
  const device = state.halDevices.find(d => d.id === slot);
  if (!device) {
    return res.status(404).json({ error: 'No device in slot' });
  }

  // Validate ADC-specific config fields before applying any changes
  if (req.body.cfgPgaGain !== undefined) {
    if (!VALID_PGA_GAIN_STEPS.includes(Number(req.body.cfgPgaGain))) {
      return res.status(400).json({ error: `Invalid cfgPgaGain: must be one of ${VALID_PGA_GAIN_STEPS.join(',')}` });
    }
  }
  if (req.body.cfgFilterMode !== undefined) {
    const fm = Number(req.body.cfgFilterMode);
    if (isNaN(fm) || fm < 0 || fm > 7) {
      return res.status(400).json({ error: 'Invalid cfgFilterMode: must be 0-7' });
    }
  }
  if (req.body.cfgVolume !== undefined) {
    const vol = Number(req.body.cfgVolume);
    if (isNaN(vol) || vol < 0 || vol > 100) {
      return res.status(400).json({ error: 'Invalid cfgVolume: must be 0-100' });
    }
  }

  // Apply supported config fields from request body
  const fields = ['enabled', 'label', 'volume', 'mute', 'filterMode'];
  for (const field of fields) {
    if (req.body[field] !== undefined) {
      device[field] = req.body[field];
    }
  }
  // ADC-specific config fields
  if (req.body.cfgPgaGain !== undefined) device.cfgPgaGain = req.body.cfgPgaGain;
  if (req.body.cfgHpfEnabled !== undefined) device.cfgHpfEnabled = req.body.cfgHpfEnabled;
  if (req.body.filterMode !== undefined) device.cfgFilterMode = req.body.filterMode;
  res.json({ status: 'ok' });
});

// DELETE /devices — remove a device by slot
router.delete('/devices', (req, res) => {
  const state = getState();
  const { slot } = req.body || {};
  if (slot === undefined || slot === null) {
    return res.status(400).json({ error: 'Invalid slot' });
  }
  const idx = state.halDevices.findIndex(d => d.id === slot);
  if (idx === -1) {
    return res.status(404).json({ error: 'No device in slot' });
  }
  state.halDevices.splice(idx, 1);
  res.json({ status: 'ok' });
});

// POST /devices/reinit — re-initialise a device by slot
router.post('/devices/reinit', (req, res) => {
  const state = getState();
  const { slot } = req.body || {};
  if (slot === undefined || slot === null) {
    return res.status(400).json({ error: 'Invalid slot' });
  }
  const device = state.halDevices.find(d => d.id === slot);
  if (!device) {
    return res.status(404).json({ error: 'No device in slot' });
  }
  device.state = 'AVAILABLE';
  res.json({ status: 'ok', state: 'AVAILABLE' });
});

// GET /db/presets — list available device presets from the database
router.get('/db/presets', (req, res) => {
  res.json(DB_PRESETS);
});

// GET /settings — HAL auto-discovery toggle
router.get('/settings', (req, res) => {
  res.json({ halAutoDiscovery: true });
});

// PUT /settings — save HAL auto-discovery toggle
router.put('/settings', (req, res) => {
  // Accept and ignore — mock has no persistence
  res.json({ status: 'ok' });
});

// GET /devices/custom — list custom device schemas
router.get('/devices/custom', (req, res) => {
  res.json({ schemas: [] });
});

// POST /devices/custom — upload a custom device schema
router.post('/devices/custom', (req, res) => {
  if (!req.body || !req.body.compatible) {
    return res.status(400).json({ error: 'Invalid schema' });
  }
  res.json({ ok: true });
});

// DELETE /devices/custom — remove a schema by name query param
router.delete('/devices/custom', (req, res) => {
  if (!req.query.name) {
    return res.status(400).json({ error: 'Missing name parameter' });
  }
  res.json({ ok: true });
});

// GET /eeprom — read EEPROM state (moved from /api/dac/eeprom)
router.get('/eeprom', (req, res) => {
  res.json({
    scanned: false,
    found: false,
    i2cMask: 0,
    i2cDevices: 0,
    readErrors: 0,
    writeErrors: 0,
  });
});

// POST /eeprom/scan — trigger EEPROM rescan (moved from /api/dac/eeprom/scan)
router.post('/eeprom/scan', (req, res) => {
  res.json({ success: true, scanned: true });
});

// GET /eeprom/presets — list EEPROM presets (moved from /api/dac/eeprom/presets)
router.get('/eeprom/presets', (req, res) => {
  res.json({ presets: DB_PRESETS });
});

// POST /eeprom — program EEPROM (moved from /api/dac/eeprom POST)
router.post('/eeprom', (req, res) => {
  if (!req.body || !req.body.deviceId) {
    return res.status(400).json({ error: 'Missing deviceId' });
  }
  res.json({ success: true });
});

module.exports = router;
