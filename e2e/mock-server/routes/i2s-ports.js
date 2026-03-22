/**
 * I2S port routes — mirrors /api/i2s/ports endpoint from i2s_port_api.cpp.
 * Mounted at /api/i2s in server.js.
 */
const express = require('express');
const path = require('path');
const fs = require('fs');
const router = express.Router();

const FIXTURE_DIR = path.join(__dirname, '..', '..', 'fixtures', 'api-responses');

function loadFixture(name) {
  return JSON.parse(fs.readFileSync(path.join(FIXTURE_DIR, `${name}.json`), 'utf8'));
}

// GET /ports — all I2S port status, or single port with ?id=N
router.get('/ports', (req, res) => {
  const fixture = loadFixture('i2s-ports');

  // Single port query
  if (req.query.id !== undefined) {
    const id = parseInt(req.query.id, 10);
    const port = fixture.ports.find(p => p.id === id);
    if (!port) {
      return res.status(400).json({ error: 'Invalid port id' });
    }
    return res.json(port);
  }

  // All ports
  res.json(fixture);
});

module.exports = router;
