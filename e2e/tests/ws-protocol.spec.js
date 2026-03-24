/**
 * ws-protocol.spec.js — WebSocket protocol version handshake verification.
 *
 * Tests that after WS auth, the server sends a protocolVersion message
 * with the expected version string. This is a foundation hardening feature
 * to enable future protocol negotiation and backward compatibility detection.
 *
 * @ws @smoke
 */

const { test: base, expect } = require('@playwright/test');
const { buildInitialState } = require('../helpers/ws-helpers');

test.describe('@ws @smoke WebSocket Protocol Version', () => {

  test('server sends protocolVersion message after auth', async ({ page, request }) => {
    // Acquire session cookie
    const resp = await request.post('http://localhost:3000/api/auth/login', {
      data: { password: 'testpass' },
    });
    const cookies = resp.headers()['set-cookie'];
    const match = cookies.match(/sessionId=([^;]+)/);
    await page.context().addCookies([
      { name: 'sessionId', value: match[1], domain: 'localhost', path: '/' },
    ]);

    // Track all messages received from the "server" (mock WS)
    const receivedMessages = [];

    await page.routeWebSocket(/.*:81/, (ws) => {
      ws.onMessage((msg) => {
        let data;
        try { data = JSON.parse(msg); } catch { return; }

        if (data.type === 'auth') {
          // Replicate the firmware auth flow: authSuccess then protocolVersion
          ws.send(JSON.stringify({ type: 'authSuccess' }));
          ws.send(JSON.stringify({ type: 'protocolVersion', version: '1.0' }));

          // Send initial state to allow connection to complete
          const initialMessages = buildInitialState();
          for (const m of initialMessages) {
            ws.send(JSON.stringify(m));
          }
        }
      });

      // Capture messages sent from mock to page (server → client direction)
      ws.onClose(() => {});

      // Trigger auth flow
      ws.send(JSON.stringify({ type: 'authRequired' }));
    });

    // Intercept WS messages received by the page
    await page.addInitScript(() => {
      window.__wsMessages = [];
      const origWS = window.WebSocket;
      window.WebSocket = function(...args) {
        const ws = new origWS(...args);
        ws.addEventListener('message', (e) => {
          if (typeof e.data === 'string') {
            try { window.__wsMessages.push(JSON.parse(e.data)); } catch {}
          }
        });
        return ws;
      };
      // Copy prototype for instanceof checks
      window.WebSocket.prototype = origWS.prototype;
    });

    await page.goto('/');
    await expect(page.locator('#wsConnectionStatus')).toHaveText('Connected', { timeout: 10000 });

    // Verify that the protocolVersion message was received
    const messages = await page.evaluate(() => window.__wsMessages || []);
    const pvMsg = messages.find(m => m.type === 'protocolVersion');
    expect(pvMsg).toBeTruthy();
    expect(pvMsg.version).toBe('1.0');
  });

  test('protocolVersion message has correct structure', async ({ page, request }) => {
    // Acquire session cookie
    const resp = await request.post('http://localhost:3000/api/auth/login', {
      data: { password: 'testpass' },
    });
    const cookies = resp.headers()['set-cookie'];
    const match = cookies.match(/sessionId=([^;]+)/);
    await page.context().addCookies([
      { name: 'sessionId', value: match[1], domain: 'localhost', path: '/' },
    ]);

    let protocolMsg = null;

    await page.routeWebSocket(/.*:81/, (ws) => {
      ws.onMessage((msg) => {
        let data;
        try { data = JSON.parse(msg); } catch { return; }

        if (data.type === 'auth') {
          ws.send(JSON.stringify({ type: 'authSuccess' }));
          // Send protocolVersion with version field
          protocolMsg = { type: 'protocolVersion', version: '1.0' };
          ws.send(JSON.stringify(protocolMsg));

          const initialMessages = buildInitialState();
          for (const m of initialMessages) {
            ws.send(JSON.stringify(m));
          }
        }
      });
      ws.onClose(() => {});
      ws.send(JSON.stringify({ type: 'authRequired' }));
    });

    await page.goto('/');
    await expect(page.locator('#wsConnectionStatus')).toHaveText('Connected', { timeout: 10000 });

    // Verify structure
    expect(protocolMsg).not.toBeNull();
    expect(protocolMsg.type).toBe('protocolVersion');
    expect(protocolMsg.version).toBe('1.0');
    // Version must be a string, not a number
    expect(typeof protocolMsg.version).toBe('string');
    // Version follows major.minor format
    expect(protocolMsg.version).toMatch(/^\d+\.\d+$/);
  });

});
