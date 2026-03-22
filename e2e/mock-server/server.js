/**
 * Express Mock Server for E2E Tests
 * Serves the real frontend from web_src/ and provides mock REST API endpoints.
 * WebSocket mocking is handled at browser level via Playwright's routeWebSocket().
 */

const express = require('express');
const cookieParser = require('cookie-parser');
const { assembleMainPage, assembleLoginPage } = require('./assembler');
const { resetState } = require('./ws-state');

const app = express();
app.use(express.json());
app.use(cookieParser());

// ===== Page Routes =====
app.get('/', (req, res) => {
  res.type('html').send(assembleMainPage());
});

app.get('/login', (req, res) => {
  res.type('html').send(assembleLoginPage());
});

// ===== Captive Portal / Misc =====
app.get('/favicon.ico', (req, res) => res.status(204).end());
app.get('/manifest.json', (req, res) => res.json({ name: 'ALX Audio Controller' }));
app.get('/robots.txt', (req, res) => res.type('text').send('User-agent: *\nDisallow: /'));
app.get('/generate_204', (req, res) => res.status(204).end());

// ===== Test Reset Endpoint =====
app.post('/api/__test__/reset', (req, res) => {
  resetState();
  res.json({ success: true });
});

// ===== REST API Routes =====
const auth = require('./routes/auth');
const hal = require('./routes/hal');
const wifi = require('./routes/wifi');
const mqtt = require('./routes/mqtt');
const settings = require('./routes/settings');
const ota = require('./routes/ota');
const pipeline = require('./routes/pipeline');
const dsp = require('./routes/dsp');
const sensing = require('./routes/sensing');
const siggen = require('./routes/siggen');
const diagnostics = require('./routes/diagnostics');
const system = require('./routes/system');
const ethernet = require('./routes/ethernet');
const i2sPorts = require('./routes/i2s-ports');

app.use('/api/auth', auth);

// WS token endpoint — returns one-time token for WS auth (mock always succeeds)
app.get('/api/ws-token', (req, res) => {
  const cookieId = req.cookies && req.cookies['sessionId'];
  if (!cookieId) {
    return res.status(401).json({ success: false, error: 'Unauthorized' });
  }
  res.json({ success: true, token: `ws-token-${Date.now()}` });
});

app.use('/api/hal', hal);
app.use('/api', wifi);
app.use('/api/mqtt', mqtt);
app.use('/api', settings);
app.use('/api', ota);
app.use('/api/pipeline', pipeline);
app.use('/api', dsp);
app.use('/api', sensing);
app.use('/api', siggen);
app.use('/api', diagnostics);
app.use('/api', system);
app.use('/api', ethernet);
app.use('/api/i2s', i2sPorts);

// ===== Start Server =====
const PORT = process.env.PORT || 3000;
const server = app.listen(PORT, () => {
  console.log(`[Mock Server] http://localhost:${PORT}`);
});

module.exports = { app, server };
