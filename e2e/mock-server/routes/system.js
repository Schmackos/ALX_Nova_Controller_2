/**
 * System routes — reboot, factory reset, version, uptime.
 * Mounted at /api in server.js.
 */
const express = require('express');
const { getState } = require('../ws-state');
const router = express.Router();

router.get('/version', (req, res) => {
  res.json({ version: getState().firmwareVersion });
});

router.post('/reboot', (req, res) => {
  res.json({ success: true });
});

router.post('/factoryreset', (req, res) => {
  res.json({ success: true });
});

router.get('/uptime', (req, res) => {
  res.json({ uptime: 120000 });
});

router.get('/psram/status', (req, res) => {
  res.json({
    total: 8388608,
    free: 7000000,
    usagePercent: 16.6,
    fallbackCount: 0,
    failedCount: 0,
    allocPsram: 155000,
    allocSram: 0,
    warning: false,
    critical: false,
    budget: [
      { label: 'pipe_lanes', bytes: 32768, psram: true },
      { label: 'dsp_states', bytes: 768, psram: true }
    ]
  });
});

router.get('/inputnames', (req, res) => {
  res.json({ success: true, names: ['ADC1 L','ADC1 R','ADC2 L','ADC2 R','SigGen L','SigGen R','USB L','USB R'], numAdcsDetected: 2 });
});

// GET /health — system health summary (mirrors health_check_api.cpp)
router.get('/health', (req, res) => {
  res.json({
    success: true,
    overall: 'pass',
    checks: {
      system: 'pass',
      i2c: 'pass',
      hal: 'pass',
      i2s: 'pass',
      network: 'pass',
      mqtt: 'pass',
      tasks: 'pass',
      storage: 'pass',
      audio: 'pass',
    },
  });
});

module.exports = router;
