/**
 * Ethernet management routes — mirrors /api/ethconfig* and /api/ethstatus endpoints.
 * Mounted at /api in server.js.
 *
 * GET  /ethstatus         — current Ethernet connection status
 * POST /ethconfig         — apply Ethernet configuration (hostname, static IP)
 * POST /ethconfig/confirm — confirm pending static IP configuration
 */

const express = require('express');
const { getState } = require('../ws-state');

const router = express.Router();

// GET /ethstatus — returns current Ethernet connection state
router.get('/ethstatus', (req, res) => {
  const state = getState();
  res.json({
    linkUp: state.ethLinkUp || false,
    connected: state.ethConnected || false,
    ip: state.ethIP || '',
    mac: state.ethMAC || 'AA:BB:CC:DD:EE:FF',
    speed: state.ethSpeed || 0,
    fullDuplex: state.ethFullDuplex || false,
    gateway: state.ethGateway || '',
    subnet: state.ethSubnet || '',
    dns1: state.ethDns1 || '',
    dns2: state.ethDns2 || '',
    hostname: state.ethHostname || 'alx-nova',
    useStaticIP: state.ethUseStaticIP || false,
    activeInterface: state.activeInterface || 'wifi',
  });
});

// POST /ethconfig — save Ethernet configuration (hostname and/or static IP)
router.post('/ethconfig', (req, res) => {
  const body = req.body || {};
  const state = getState();

  if (body.hostname !== undefined) {
    state.ethHostname = body.hostname;
  }

  if (body.useStaticIP !== undefined) {
    state.ethUseStaticIP = body.useStaticIP;
    state.ethStaticIP = body.staticIP || '';
    state.ethSubnet = body.subnet || '255.255.255.0';
    state.ethGateway = body.gateway || '';
    state.ethDns1 = body.dns1 || '';
    state.ethDns2 = body.dns2 || '';
  }

  const response = { success: true };
  if (body.useStaticIP) {
    response.pendingConfirm = true;
  }
  res.json(response);
});

// POST /ethconfig/confirm — confirm pending static IP configuration
router.post('/ethconfig/confirm', (req, res) => {
  const state = getState();
  state.ethPendingConfirm = false;
  res.json({ success: true });
});

module.exports = router;
