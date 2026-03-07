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

router.get('/inputnames', (req, res) => {
  res.json({ success: true, names: ['ADC1 L','ADC1 R','ADC2 L','ADC2 R','SigGen L','SigGen R','USB L','USB R'], numAdcsDetected: 2 });
});

module.exports = router;
