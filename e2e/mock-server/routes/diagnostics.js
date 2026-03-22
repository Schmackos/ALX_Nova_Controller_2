/**
 * Diagnostics routes — mirrors /api/diagnostics, /api/diagnostics/journal,
 * and /api/diag/snapshot endpoints from diag_api.cpp.
 * Mounted at /api in server.js.
 */
const express = require('express');
const path = require('path');
const fs = require('fs');
const { getState } = require('../ws-state');
const router = express.Router();

const FIXTURE_DIR = path.join(__dirname, '..', '..', 'fixtures', 'api-responses');

function loadFixture(name) {
  return JSON.parse(fs.readFileSync(path.join(FIXTURE_DIR, `${name}.json`), 'utf8'));
}

// GET /diagnostics — full diagnostics summary
router.get('/diagnostics', (req, res) => {
  const state = getState();
  const payload = {
    firmware: state.firmwareVersion,
    wifi: { ssid: state.wifiSSID, ip: state.wifiIP, rssi: state.wifiRSSI },
    mqtt: { connected: state.mqttConnected },
    hal: { devices: state.halDevices.length },
  };
  res.json(payload);
});

// GET /diagnostics/journal — diagnostic event journal
router.get('/diagnostics/journal', (req, res) => {
  res.json(loadFixture('diag-journal'));
});

// DELETE /diagnostics/journal — clear journal
router.delete('/diagnostics/journal', (req, res) => {
  res.json({ success: true });
});

// GET /diag/snapshot — compact diagnostic snapshot
router.get('/diag/snapshot', (req, res) => {
  res.json(loadFixture('diag-snapshot'));
});

module.exports = router;
