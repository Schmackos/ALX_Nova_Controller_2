/**
 * Signal generator routes — mirrors /api/signalgenerator endpoints inlined in main.cpp.
 * Mounted at /api in server.js.
 *
 * GET  /signalgenerator — current signal generator parameters
 * POST /signalgenerator — update signal generator parameters
 */

const express = require('express');
const { getState } = require('../ws-state');

const router = express.Router();

// Waveform name → integer index mapping (matches AppState sigGenWaveform field)
const WAVEFORM_NAMES = ['sine', 'square', 'white_noise', 'sweep'];
const CHANNEL_NAMES  = ['left', 'right', 'both'];
const OUTPUT_MODES   = ['software', 'pwm'];

// GET /signalgenerator — return current signal generator config
router.get('/signalgenerator', (req, res) => {
  const state = getState();
  res.json({
    success: true,
    enabled: state.sigGenEnabled,
    waveform: WAVEFORM_NAMES[state.sigGenWaveform % 4],
    frequency: state.sigGenFrequency,
    amplitude: state.sigGenAmplitude,
    channel: CHANNEL_NAMES[0],   // default 'left' — not tracked separately in mock state
    outputMode: 'software',
    sweepSpeed: 1000,
  });
});

// POST /signalgenerator — update signal generator parameters
router.post('/signalgenerator', (req, res) => {
  const state = getState();
  const body = req.body || {};

  if (typeof body.enabled === 'boolean') {
    state.sigGenEnabled = body.enabled;
  }
  if (typeof body.waveform === 'string') {
    const idx = WAVEFORM_NAMES.indexOf(body.waveform);
    if (idx >= 0) state.sigGenWaveform = idx;
  }
  if (typeof body.frequency === 'number' && body.frequency >= 1 && body.frequency <= 22000) {
    state.sigGenFrequency = body.frequency;
  }
  if (typeof body.amplitude === 'number' && body.amplitude >= -96 && body.amplitude <= 0) {
    state.sigGenAmplitude = body.amplitude;
  }
  // channel and outputMode accepted but not reflected in mock state

  res.json({ success: true });
});

module.exports = router;
