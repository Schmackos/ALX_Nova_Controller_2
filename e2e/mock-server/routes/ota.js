/**
 * OTA update routes — mirrors /api/checkupdate, /api/startupdate, etc. from ota_updater.cpp.
 * Mounted at /api in server.js.
 *
 * GET  /checkupdate    — poll for a new firmware release
 * GET  /updatestatus   — current OTA state machine status
 * GET  /releasenotes   — release notes for available update
 * GET  /releases       — list of GitHub releases
 * POST /startupdate    — begin OTA download for latest release
 * POST /installrelease — begin OTA download for a specific release tag
 */

const express = require('express');
const { getState } = require('../ws-state');

const router = express.Router();

// Static release list — deterministic for E2E tests
const RELEASES = [
  {
    tag: 'v1.12.0',
    name: 'v1.12.0 — Unified Audio Tab',
    publishedAt: '2026-03-07T00:00:00Z',
    size: 1572864,
    notes: 'Unified Audio Tab with HAL-driven channel strips and 8x8 matrix routing.',
    prerelease: false,
  },
  {
    tag: 'v1.11.0',
    name: 'v1.11.0 — HAL Framework',
    publishedAt: '2026-03-02T00:00:00Z',
    size: 1507328,
    notes: 'HAL device framework with 3-tier discovery and EEPROM v3.',
    prerelease: false,
  },
];

// GET /checkupdate — poll GitHub for a newer release
router.get('/checkupdate', (req, res) => {
  const state = getState();
  res.json({
    available: false,
    currentVersion: state.firmwareVersion,
    latestVersion: state.firmwareVersion,
    autoUpdateEnabled: false,
  });
});

// GET /updatestatus — OTA state machine status
router.get('/updatestatus', (req, res) => {
  const state = getState();
  res.json({
    status: 'idle',
    progress: 0,
    message: '',
    otaInProgress: false,
    currentVersion: state.firmwareVersion,
    updateAvailable: false,
  });
});

// GET /releasenotes — release notes for available update (empty when none available)
router.get('/releasenotes', (req, res) => {
  res.json({
    notes: '',
    tag: '',
  });
});

// GET /releases — list all known GitHub releases
router.get('/releases', (req, res) => {
  res.json(RELEASES);
});

// POST /startupdate — begin OTA download for latest release
router.post('/startupdate', (req, res) => {
  res.json({ success: true });
});

// POST /installrelease — begin OTA download for a specific tag
router.post('/installrelease', (req, res) => {
  const { tag } = req.body || {};
  if (!tag) {
    return res.status(400).json({ success: false, message: 'Missing tag' });
  }
  const found = RELEASES.find(r => r.tag === tag);
  if (!found) {
    return res.status(404).json({ success: false, message: 'Release not found' });
  }
  res.json({ success: true, tag });
});

module.exports = router;
