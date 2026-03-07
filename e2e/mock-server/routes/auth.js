/**
 * Auth routes — mirrors /api/auth/* endpoints from auth_handler.cpp.
 * POST /login, POST /logout, GET /status, POST /change
 */

const express = require('express');
const { getState } = require('../ws-state');

const router = express.Router();

// Simple session store: sessionId -> true
const _sessions = new Set();
let _sessionCounter = 1;

function generateSessionId() {
  return `mock-session-${_sessionCounter++}-${Date.now()}`;
}

function isAuthenticated(req) {
  const cookieId = req.cookies && req.cookies['sessionId'];
  const headerId = req.headers['x-session-id'];
  return _sessions.has(cookieId) || _sessions.has(headerId);
}

// POST /login — accepts {password}, returns session cookie
router.post('/login', (req, res) => {
  const { password } = req.body || {};
  // Mock: any non-empty password succeeds; real firmware checks NVS hash
  if (!password || typeof password !== 'string' || password.length === 0) {
    return res.status(401).json({ success: false, message: 'Invalid password' });
  }

  const sessionId = generateSessionId();
  _sessions.add(sessionId);
  getState().authenticated = true;

  res.cookie('sessionId', sessionId, { httpOnly: true, sameSite: 'Strict' });
  res.json({ success: true, isDefaultPassword: false });
});

// POST /logout — clears session cookie
router.post('/logout', (req, res) => {
  const cookieId = req.cookies && req.cookies['sessionId'];
  if (cookieId) {
    _sessions.delete(cookieId);
  }
  getState().authenticated = false;
  res.clearCookie('sessionId');
  res.json({ success: true });
});

// GET /status — returns current auth state
router.get('/status', (req, res) => {
  const authed = isAuthenticated(req);
  res.json({ authenticated: authed, username: 'admin' });
});

// POST /change — accepts {currentPassword, newPassword}
router.post('/change', (req, res) => {
  const { currentPassword, newPassword } = req.body || {};
  if (!currentPassword || !newPassword) {
    return res.status(400).json({ success: false, message: 'Missing fields' });
  }
  res.json({ success: true });
});

module.exports = router;
