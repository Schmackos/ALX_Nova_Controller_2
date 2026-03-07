/**
 * WiFi management routes — mirrors /api/wifi* endpoints from wifi_manager.cpp.
 * Mounted at /api in server.js.
 *
 * GET  /wifistatus  — current connection status
 * GET  /wifiscan    — available networks scan
 * GET  /wifilist    — saved networks list
 * POST /wificonfig  — save network credentials
 * POST /wifisave    — save network to list
 * POST /wifiremove  — remove a saved network
 * POST /apconfig    — update AP mode config
 * POST /toggleap    — toggle AP mode
 */

const express = require('express');
const { getState } = require('../ws-state');

const router = express.Router();

// Static scan result — deterministic for E2E tests
const SCAN_RESULTS = [
  { ssid: 'TestNetwork',  rssi: -45, encryption: 'WPA2', channel: 6  },
  { ssid: 'GuestNet',     rssi: -72, encryption: 'WPA2', channel: 1  },
  { ssid: 'OpenNet',      rssi: -80, encryption: 'OPEN', channel: 11 },
];

// Saved network list (mutable during a test session)
let _savedNetworks = [
  { ssid: 'TestNetwork', password: '', priority: 1 },
];

// GET /wifistatus — returns current connection state
router.get('/wifistatus', (req, res) => {
  const state = getState();
  res.json({
    connected: state.wifiConnected,
    ssid: state.wifiSSID,
    ip: state.wifiIP,
    rssi: state.wifiRSSI,
    mode: state.wifiConnected ? 'STA' : 'AP',
    apSSID: 'ALX-AABBCCDDEEFF',
    apIP: '192.168.4.1',
    isAPMode: false,
    ethConnected: false,
    ethIP: '',
  });
});

// GET /wifiscan — returns available networks
router.get('/wifiscan', (req, res) => {
  res.json({ networks: SCAN_RESULTS });
});

// GET /wifilist — returns saved networks
router.get('/wifilist', (req, res) => {
  res.json({ networks: _savedNetworks });
});

// POST /wificonfig — save network credentials and attempt connection
router.post('/wificonfig', (req, res) => {
  const { ssid, password } = req.body || {};
  if (!ssid) {
    return res.status(400).json({ success: false, message: 'Missing SSID' });
  }
  // Update mock state to reflect new connection
  const state = getState();
  state.wifiSSID = ssid;
  state.wifiConnected = true;
  res.json({ success: true });
});

// POST /wifisave — add or update a saved network
router.post('/wifisave', (req, res) => {
  const { ssid, password, priority } = req.body || {};
  if (!ssid) {
    return res.status(400).json({ success: false, message: 'Missing SSID' });
  }
  const existing = _savedNetworks.find(n => n.ssid === ssid);
  if (existing) {
    if (password !== undefined) existing.password = password;
    if (priority !== undefined) existing.priority = priority;
  } else {
    _savedNetworks.push({ ssid, password: password || '', priority: priority || 99 });
  }
  res.json({ success: true });
});

// POST /wifiremove — remove a saved network by SSID
router.post('/wifiremove', (req, res) => {
  const { ssid } = req.body || {};
  if (!ssid) {
    return res.status(400).json({ success: false, message: 'Missing SSID' });
  }
  _savedNetworks = _savedNetworks.filter(n => n.ssid !== ssid);
  res.json({ success: true });
});

// POST /apconfig — update AP SSID/password
router.post('/apconfig', (req, res) => {
  // Accept and ignore in mock — no real AP to configure
  res.json({ success: true });
});

// POST /toggleap — toggle AP mode on/off
router.post('/toggleap', (req, res) => {
  res.json({ success: true });
});

module.exports = router;
