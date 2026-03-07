/**
 * Smart sensing routes — mirrors /api/smartsensing endpoints from smart_sensing.cpp.
 * Mounted at /api in server.js.
 *
 * GET  /smartsensing — current sensing config and state
 * POST /smartsensing — update sensing config
 */

const express = require('express');
const { getState } = require('../ws-state');

const router = express.Router();

// GET /smartsensing — current sensing configuration and runtime state
router.get('/smartsensing', (req, res) => {
  const state = getState();
  res.json({
    success: true,
    mode: state.sensingMode,
    threshold: state.sensingThreshold,
    autoOffTimer: state.autoOffTimer,
    ampOn: state.ampOn,
    // Additional fields broadcast by sendSmartSensingState()
    fsmState: state.ampOn ? 1 : 0,   // 0=IDLE, 1=SIGNAL_DETECTED
    signalLevel: 0.0,
    autoOffRemaining: 0,
  });
});

// POST /smartsensing — update sensing configuration
router.post('/smartsensing', (req, res) => {
  const state = getState();
  const body = req.body || {};
  if (body.mode !== undefined)          state.sensingMode = body.mode;
  if (body.threshold !== undefined)     state.sensingThreshold = body.threshold;
  if (body.autoOffTimer !== undefined)  state.autoOffTimer = body.autoOffTimer;
  res.json({ success: true });
});

module.exports = router;
