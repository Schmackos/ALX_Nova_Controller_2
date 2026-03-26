/**
 * DSP configuration routes — mirrors /api/dsp* endpoints from dsp_api.cpp.
 * Mounted at /api in server.js.
 *
 * Core per-lane DSP (input pipeline):
 * GET  /dsp                     — enabled flag + bypass state
 * PUT  /dsp                     — save DSP global config
 * POST /dsp/bypass              — toggle lane bypass
 * GET  /dsp/metrics             — CPU usage and stage counts
 * GET  /dsp/channel             — per-channel stage list
 * POST /dsp/channel/bypass      — toggle channel bypass
 * POST /dsp/stage               — add a DSP stage
 * PUT  /dsp/stage               — update a DSP stage
 * DELETE /dsp/stage             — remove a DSP stage
 * POST /dsp/stage/reorder       — reorder stages
 * POST /dsp/stage/enable        — enable/disable a stage
 * POST /dsp/crossover           — apply crossover preset
 * POST /dsp/bafflestep          — apply baffle step correction
 * GET  /dsp/peq/presets         — list PEQ presets
 * POST /dsp/peq/presets         — save PEQ preset
 * GET  /dsp/peq/preset          — load single preset
 * DELETE /dsp/peq/preset        — delete preset
 * GET  /dsp/presets             — list DSP presets
 * POST /dsp/presets/save        — save full DSP preset
 * POST /dsp/presets/load        — load full DSP preset
 * DELETE /dsp/presets           — delete DSP preset
 * POST /dsp/presets/rename      — rename DSP preset
 * POST /dsp/channel/stereolink  — link/unlink stereo pair
 * POST /dsp/import/apo          — import Equalizer APO file
 * POST /dsp/import/minidsp      — import miniDSP file
 * POST /dsp/import/fir          — import FIR coefficients
 * GET  /dsp/export/apo          — export Equalizer APO
 * GET  /dsp/export/minidsp      — export miniDSP format
 * GET  /dsp/export/json         — export raw JSON config
 *
 * Per-output DSP (post-matrix):
 * GET  /output/dsp              — per-output DSP config
 * PUT  /output/dsp              — save per-output DSP config
 * POST /output/dsp/stage        — add output DSP stage
 * DELETE /output/dsp/stage      — remove output DSP stage
 * POST /output/dsp/crossover    — apply crossover to output pair
 */

const express = require('express');

const router = express.Router();

// In-memory per-channel stage lists (4 input channels: 0=ADC1L, 1=ADC1R, 2=ADC2L, 3=ADC2R)
const _stages = { 0: [], 1: [], 2: [], 3: [] };
// Per-output DSP stage lists (8 outputs)
const _outputStages = { 0: [], 1: [], 2: [], 3: [], 4: [], 5: [], 6: [], 7: [] };
// PEQ presets store
const _peqPresets = [];
// DSP full presets store — slot-indexed object (slots 0-31)
const _dspPresets = {};
let _stageIdCounter = 1;

// ===== Input pipeline DSP =====

// GET /dsp — global DSP config
router.get('/dsp', (req, res) => {
  res.json({
    success: true,
    enabled: true,
    bypass: false,
    channels: 4,
  });
});

// PUT /dsp — save global DSP config
router.put('/dsp', (req, res) => {
  res.json({ success: true });
});

// POST /dsp/bypass — toggle lane bypass
router.post('/dsp/bypass', (req, res) => {
  const { channel, bypass } = req.body || {};
  res.json({ success: true, channel, bypass });
});

// GET /dsp/metrics — processing metrics
router.get('/dsp/metrics', (req, res) => {
  res.json({
    success: true,
    cpuUsage: 0.0,
    stageCounts: [_stages[0].length, _stages[1].length, _stages[2].length, _stages[3].length],
  });
});

// GET /dsp/channel — per-channel stage list
router.get('/dsp/channel', (req, res) => {
  const ch = parseInt(req.query.channel, 10);
  if (isNaN(ch) || ch < 0 || ch > 3) {
    return res.status(400).json({ success: false, message: 'Invalid channel' });
  }
  res.json({ success: true, channel: ch, stages: _stages[ch] || [] });
});

// POST /dsp/channel/bypass — toggle channel bypass
router.post('/dsp/channel/bypass', (req, res) => {
  res.json({ success: true });
});

// POST /dsp/stage — add a stage to a channel
router.post('/dsp/stage', (req, res) => {
  const { channel, type, params } = req.body || {};
  if (channel === undefined || !type) {
    return res.status(400).json({ success: false, message: 'Missing channel or type' });
  }
  const ch = parseInt(channel, 10);
  if (!_stages[ch]) {
    return res.status(400).json({ success: false, message: 'Invalid channel' });
  }
  const id = _stageIdCounter++;
  const stage = { id, type, enabled: true, params: params || {} };
  _stages[ch].push(stage);
  res.json({ success: true, id, channel: ch });
});

// PUT /dsp/stage — update a stage
router.put('/dsp/stage', (req, res) => {
  const { channel, id, params } = req.body || {};
  const ch = parseInt(channel, 10);
  const stages = _stages[ch];
  if (!stages) {
    return res.status(400).json({ success: false, message: 'Invalid channel' });
  }
  const stage = stages.find(s => s.id === id);
  if (!stage) {
    return res.status(404).json({ success: false, message: 'Stage not found' });
  }
  if (params) Object.assign(stage.params, params);
  res.json({ success: true });
});

// DELETE /dsp/stage — remove a stage
router.delete('/dsp/stage', (req, res) => {
  const { channel, id } = req.body || {};
  const ch = parseInt(channel, 10);
  if (!_stages[ch]) {
    return res.status(400).json({ success: false, message: 'Invalid channel' });
  }
  _stages[ch] = _stages[ch].filter(s => s.id !== id);
  res.json({ success: true });
});

// POST /dsp/stage/reorder — reorder stages by id array
router.post('/dsp/stage/reorder', (req, res) => {
  const { channel, order } = req.body || {};
  const ch = parseInt(channel, 10);
  if (!_stages[ch] || !Array.isArray(order)) {
    return res.status(400).json({ success: false, message: 'Invalid request' });
  }
  const map = new Map(_stages[ch].map(s => [s.id, s]));
  _stages[ch] = order.map(id => map.get(id)).filter(Boolean);
  res.json({ success: true });
});

// POST /dsp/stage/enable — enable or disable a stage
router.post('/dsp/stage/enable', (req, res) => {
  const { channel, id, enabled } = req.body || {};
  const ch = parseInt(channel, 10);
  const stage = (_stages[ch] || []).find(s => s.id === id);
  if (!stage) {
    return res.status(404).json({ success: false, message: 'Stage not found' });
  }
  stage.enabled = Boolean(enabled);
  res.json({ success: true });
});

// POST /dsp/crossover — apply crossover preset to two channels
router.post('/dsp/crossover', (req, res) => {
  res.json({ success: true });
});

// POST /dsp/bafflestep — apply baffle step correction
router.post('/dsp/bafflestep', (req, res) => {
  res.json({ success: true });
});

// ===== PEQ Presets =====

// GET /dsp/peq/presets — list all saved PEQ presets
router.get('/dsp/peq/presets', (req, res) => {
  res.json({ success: true, presets: _peqPresets.map(p => ({ name: p.name, channel: p.channel })) });
});

// POST /dsp/peq/presets — save a PEQ preset
router.post('/dsp/peq/presets', (req, res) => {
  const { name, channel, stages } = req.body || {};
  if (!name) {
    return res.status(400).json({ success: false, message: 'Missing name' });
  }
  const existing = _peqPresets.findIndex(p => p.name === name);
  const entry = { name, channel, stages: stages || [] };
  if (existing >= 0) {
    _peqPresets[existing] = entry;
  } else {
    _peqPresets.push(entry);
  }
  res.json({ success: true });
});

// GET /dsp/peq/preset — load a single preset by name
router.get('/dsp/peq/preset', (req, res) => {
  const { name } = req.query;
  const preset = _peqPresets.find(p => p.name === name);
  if (!preset) {
    return res.status(404).json({ success: false, message: 'Not found' });
  }
  res.json({ success: true, preset });
});

// DELETE /dsp/peq/preset — delete a preset by name
router.delete('/dsp/peq/preset', (req, res) => {
  const { name } = req.body || req.query || {};
  const idx = _peqPresets.findIndex(p => p.name === name);
  if (idx >= 0) _peqPresets.splice(idx, 1);
  res.json({ success: true });
});

// ===== DSP Full Presets =====

// GET /dsp/presets — list all DSP preset slots (returns slots array)
router.get('/dsp/presets', (req, res) => {
  const slots = [];
  for (let i = 0; i < 32; i++) {
    const p = _dspPresets[i];
    slots.push({ index: i, exists: !!p, name: p ? p.name : '' });
  }
  res.json({ success: true, slots, activeIndex: -1 });
});

// POST /dsp/presets/save?slot=N — save full DSP config into a slot
router.post('/dsp/presets/save', (req, res) => {
  const slot = parseInt(req.query.slot);
  const { name } = req.body || {};
  if (isNaN(slot) || slot < 0 || slot > 31) {
    return res.status(400).json({ success: false, message: 'Invalid slot' });
  }
  _dspPresets[slot] = { name: name || ('Preset ' + slot), stages: JSON.parse(JSON.stringify(_stages)) };
  res.json({ success: true });
});

// POST /dsp/presets/load?slot=N — load DSP preset from slot
router.post('/dsp/presets/load', (req, res) => {
  const slot = parseInt(req.query.slot);
  const preset = _dspPresets[slot];
  if (!preset) {
    return res.status(404).json({ success: false, message: 'Preset not found' });
  }
  Object.assign(_stages, JSON.parse(JSON.stringify(preset.stages)));
  res.json({ success: true });
});

// DELETE /dsp/presets?slot=N — delete DSP preset from slot
router.delete('/dsp/presets', (req, res) => {
  const slot = parseInt(req.query.slot);
  if (!isNaN(slot) && _dspPresets[slot]) delete _dspPresets[slot];
  res.json({ success: true });
});

// POST /dsp/presets/rename — rename a DSP preset
router.post('/dsp/presets/rename', (req, res) => {
  const { slot, name } = req.body || {};
  const s = parseInt(slot);
  if (!isNaN(s) && _dspPresets[s]) {
    _dspPresets[s].name = name;
    return res.json({ success: true });
  }
  res.status(404).json({ success: false, message: 'Preset not found' });
});

// POST /dsp/channel/stereolink — link/unlink a stereo channel pair
router.post('/dsp/channel/stereolink', (req, res) => {
  res.json({ success: true });
});

// ===== Import / Export =====

// POST /dsp/import/apo — import Equalizer APO text
router.post('/dsp/import/apo', (req, res) => {
  res.json({ success: true, stagesAdded: 0 });
});

// POST /dsp/import/minidsp — import miniDSP format
router.post('/dsp/import/minidsp', (req, res) => {
  res.json({ success: true, stagesAdded: 0 });
});

// POST /dsp/import/fir — import FIR coefficients
router.post('/dsp/import/fir', (req, res) => {
  res.json({ success: true });
});

// POST /dsp/convolution/upload?ch=N — upload WAV IR for convolution stage
// Accepts raw binary body (WAV file). Mock validates ch query param only.
router.post('/dsp/convolution/upload', (req, res) => {
  const ch = parseInt(req.query.ch, 10);
  if (isNaN(ch) || ch < 0 || ch > 3) {
    return res.status(400).json({ success: false, message: 'Invalid channel' });
  }
  res.json({ success: true, channel: ch, tapsLoaded: 128 });
});

// GET /dsp/export/apo — export Equalizer APO format
router.get('/dsp/export/apo', (req, res) => {
  res.type('text/plain').send('# ALX Nova DSP export (APO format)\n');
});

// GET /dsp/export/minidsp — export miniDSP format
router.get('/dsp/export/minidsp', (req, res) => {
  res.type('text/plain').send('# ALX Nova DSP export (miniDSP format)\n');
});

// GET /dsp/export/json — export raw JSON config
router.get('/dsp/export/json', (req, res) => {
  res.json({ stages: _stages });
});

// ===== Per-output DSP (pipeline_api.cpp) =====

// GET /output/dsp — per-output DSP config
router.get('/output/dsp', (req, res) => {
  const ch = parseInt(req.query.channel || '0', 10);
  res.json({
    success: true,
    channel: ch,
    stages: _outputStages[ch] || [],
  });
});

// PUT /output/dsp — save per-output DSP config
router.put('/output/dsp', (req, res) => {
  res.json({ success: true });
});

// POST /output/dsp/stage — add an output DSP stage
// Accepts both 'ch' and 'channel' for the channel field.
// Flat DSP params (thresholdDb, ratio, attackMs, etc.) are stored under params.
router.post('/output/dsp/stage', (req, res) => {
  const body = req.body || {};
  // Frontend sends 'ch'; fallback to 'channel' for API compatibility
  const ch = parseInt(body.ch !== undefined ? body.ch : body.channel, 10);
  const { type, params } = body;
  if (!_outputStages[ch] || ch === undefined || isNaN(ch)) {
    return res.status(400).json({ success: false, message: 'Invalid channel' });
  }
  // Collect flat DSP params from body (compressor/limiter send them flat)
  const flatParams = Object.assign({}, params || {});
  const paramKeys = ['thresholdDb', 'ratio', 'attackMs', 'releaseMs', 'kneeDb', 'makeupGainDb'];
  paramKeys.forEach(k => { if (body[k] !== undefined) flatParams[k] = body[k]; });
  const id = _stageIdCounter++;
  _outputStages[ch].push({ id, type, enabled: true, params: flatParams });
  res.json({ success: true, id, channel: ch });
});

// DELETE /output/dsp/stage — remove an output DSP stage
router.delete('/output/dsp/stage', (req, res) => {
  const { channel, id } = req.body || {};
  const ch = parseInt(channel, 10);
  if (!_outputStages[ch]) {
    return res.status(400).json({ success: false, message: 'Invalid channel' });
  }
  _outputStages[ch] = _outputStages[ch].filter(s => s.id !== id);
  res.json({ success: true });
});

// POST /output/dsp/crossover — apply crossover to an output channel pair
router.post('/output/dsp/crossover', (req, res) => {
  res.json({ success: true });
});

module.exports = router;
