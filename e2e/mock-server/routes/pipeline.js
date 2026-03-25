/**
 * Audio pipeline routes — mirrors /api/pipeline/* endpoints from pipeline_api.cpp
 * and the inline matrix endpoints in main.cpp.
 * Mounted at /api/pipeline in server.js.
 *
 * GET  /matrix      — 8x8 gain matrix + input/output names
 * PUT  /matrix      — update matrix cell or full matrix
 * GET  /sinks       — output sink info array
 * GET  /inputnames  — input channel name array (alias of /api/inputnames)
 */

const express = require('express');
const { getState } = require('../ws-state');

const router = express.Router();

// Static input/output labels matching pipeline_api.cpp defaults
const INPUT_LABELS  = ['ADC1 L', 'ADC1 R', 'ADC2 L', 'ADC2 R', 'SigGen L', 'SigGen R', 'USB L', 'USB R'];
const OUTPUT_LABELS = ['Out 0 (L)', 'Out 1 (R)', 'Out 2', 'Out 3', 'Out 4', 'Out 5', 'Out 6', 'Out 7'];

// GET /matrix — returns the 8x8 gain matrix with labels
router.get('/matrix', (req, res) => {
  const state = getState();
  res.json({
    success: true,
    bypass: false,
    size: 8,
    matrix: state.matrix,
    inputs: INPUT_LABELS,
    outputs: OUTPUT_LABELS,
  });
});

// PUT /matrix — update a single cell or full matrix
router.put('/matrix', (req, res) => {
  const state = getState();
  const body = req.body || {};

  // bypass flag
  if (typeof body.bypass === 'boolean') {
    // No bypass field in mock state; accepted silently
  }

  // Single cell update: {cell: {out, in, gain}}
  if (body.cell) {
    const { out: o, in: i, gain } = body.cell;
    if (o >= 0 && o < 8 && i >= 0 && i < 8 && typeof gain === 'number') {
      state.matrix[o][i] = gain;
    }
  }

  // Single cell dB update: {cell_db: {out, in, gain_db}}
  if (body.cell_db) {
    const { out: o, in: i, gain_db } = body.cell_db;
    if (o >= 0 && o < 8 && i >= 0 && i < 8 && typeof gain_db === 'number') {
      state.matrix[o][i] = Math.pow(10, gain_db / 20);
    }
  }

  // Full matrix update: {matrix: [[...8 rows of 8 gains...]]}
  if (Array.isArray(body.matrix)) {
    for (let o = 0; o < 8 && o < body.matrix.length; o++) {
      if (Array.isArray(body.matrix[o])) {
        for (let i = 0; i < 8 && i < body.matrix[o].length; i++) {
          if (typeof body.matrix[o][i] === 'number') {
            state.matrix[o][i] = body.matrix[o][i];
          }
        }
      }
    }
  }

  res.json({ success: true });
});

// GET /sinks — output sink info (HAL-bound outputs)
router.get('/sinks', (req, res) => {
  const state = getState();
  const sinks = state.audioChannelMap.outputs.map(out => ({
    slot: out.id,
    name: out.name,
    type: out.type,
    halSlot: out.halSlot,
    ready: out.type !== 'NONE',
    muted: false,
  }));
  res.json({ success: true, sinks });
});

// GET /inputnames — input channel names (mirrors /api/inputnames)
router.get('/inputnames', (req, res) => {
  res.json({
    success: true,
    names: INPUT_LABELS,
    numAdcsDetected: 2,
  });
});

// GET /status — format negotiation status (new in Phase 1+2 hardening)
router.get('/status', (req, res) => {
  const state = getState();
  const laneSampleRates = Array(8).fill(0);
  laneSampleRates[0] = 48000;
  laneSampleRates[1] = 48000;
  res.json({
    success: true,
    rateMismatch: state.rateMismatch || false,
    laneSampleRates,
    laneDsd: Array(8).fill(false),
    sinks: (state.outputs || []).map((out, idx) => ({
      index: idx,
      name: out.name || '',
      sampleRate: 48000,
      sampleRatesMask: 0x1C,  // 44.1k|48k|96k
      bitDepth: 32,
      maxBitDepth: 32,
      supportsDsd: false,
    })),
  });
});

module.exports = router;
