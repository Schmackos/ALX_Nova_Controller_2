/**
 * Diagnostics routes — mirrors /api/diagnostics endpoint.
 * Mounted at /api in server.js.
 */
const express = require('express');
const { getState } = require('../ws-state');
const router = express.Router();

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

module.exports = router;
