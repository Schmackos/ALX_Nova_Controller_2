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
  return _sessions.has(cookieId);
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
  const state = getState();
  res.json({
    authenticated: authed,
    username: 'admin',
    success: authed,
    isDefaultPassword: state.isDefaultPassword || false
  });
});

// POST /change — accepts {currentPassword, newPassword}
// currentPassword may be omitted when isDefaultPassword is true (first-boot exemption).
router.post('/change', (req, res) => {
  const { currentPassword, newPassword } = req.body || {};
  const state = getState();

  if (!newPassword || newPassword.length < 8) {
    return res.status(400).json({ success: false, error: 'Password must be at least 8 characters' });
  }

  // Require currentPassword unless device is on first-boot default password
  if (!state.isDefaultPassword && !currentPassword) {
    return res.status(400).json({ success: false, error: 'Current password is required' });
  }

  // Mock: reject a known "wrong" current password to enable error-path tests
  // Use 403 (not 401) — 401 triggers login redirect in apiFetch()
  if (!state.isDefaultPassword && currentPassword === 'wrongpassword') {
    return res.status(403).json({ success: false, error: 'Current password is incorrect' });
  }

  // Update state: no longer using default password after successful change
  state.isDefaultPassword = false;
  res.json({ success: true, message: 'Password changed successfully', isDefaultPassword: false });
});

module.exports = router;
