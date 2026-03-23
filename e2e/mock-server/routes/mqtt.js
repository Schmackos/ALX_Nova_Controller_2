/**
 * MQTT config routes — mirrors /api/mqtt endpoints from mqtt_handler.cpp.
 * Mounted at /api/mqtt in server.js.
 *
 * GET  / — return current MQTT settings
 * POST / — save MQTT settings
 */

const express = require('express');
const { getState } = require('../ws-state');

const router = express.Router();

// GET / — return MQTT configuration and connection status
router.get('/', (req, res) => {
  const state = getState();
  res.json({
    enabled: state.mqttConnected,
    broker: state.mqttBroker,
    port: state.mqttPort,
    username: '',
    baseTopic: 'alx_nova',
    haDiscovery: true,
    useTls: state.mqttUseTls || false,
    verifyCert: state.mqttVerifyCert || false,
    connected: state.mqttConnected,
  });
});

// POST / — save MQTT configuration
router.post('/', (req, res) => {
  const state = getState();
  const { enabled, broker, port, username, password, baseTopic, haDiscovery, useTls, verifyCert } = req.body || {};
  if (broker !== undefined)     state.mqttBroker = broker;
  if (port !== undefined)       state.mqttPort = port;
  if (enabled !== undefined)    state.mqttConnected = enabled;
  if (useTls !== undefined)     state.mqttUseTls = useTls;
  if (verifyCert !== undefined) state.mqttVerifyCert = verifyCert;
  // username/password/baseTopic/haDiscovery accepted but not persisted in mock state
  res.json({ success: true });
});

module.exports = router;
