/**
 * Custom Playwright fixture: connectedPage
 *
 * Provides a browser page that has:
 *  1. A valid session cookie set (sessionId=test-session) via /api/auth/login.
 *  2. The WebSocket connection to port 81 intercepted at browser level using
 *     page.routeWebSocket(/.*:81\//) so no real device is required.
 *  3. WS auth handshake completed automatically:
 *       client → fetches /api/ws-token (cookie sent automatically)
 *       client → {type:'auth', token:'...'}
 *       server → {type:'authSuccess'}
 *  4. All initial-state fixture messages broadcast after authSuccess.
 *  5. Waits until #wsConnectionStatus reads "Connected" before resolving.
 *
 * Usage in tests:
 *   const { test, expect } = require('../helpers/fixtures');
 *   test('my test', async ({ connectedPage }) => { ... });
 *
 * The fixture also exposes connectedPage.wsRoute (the MockedWebSocket handle)
 * so individual tests can push additional WS messages.
 */

const { test: base, expect } = require('@playwright/test');
const { buildInitialState, handleCommand } = require('./ws-helpers');

/**
 * Obtain a valid session cookie from the mock server's login endpoint.
 * Returns the full Set-Cookie string value (e.g. "sessionId=mock-session-1-...").
 */
async function acquireSessionCookie(request, baseURL) {
  const resp = await request.post(`${baseURL}/api/auth/login`, {
    data: { password: 'testpass' },
  });
  const cookies = resp.headers()['set-cookie'];
  if (!cookies) throw new Error('No set-cookie header from /api/auth/login');
  // extract the sessionId=... value (first cookie segment before ";")
  const match = cookies.match(/sessionId=([^;]+)/);
  if (!match) throw new Error(`No sessionId in set-cookie: ${cookies}`);
  return match[1];
}

const test = base.extend({
  /**
   * connectedPage — a Page with a live mock WebSocket connection.
   * Exposes connectedPage.wsRoute for sending extra WS messages in tests.
   */
  connectedPage: async ({ page, request, baseURL }, use) => {
    // 1. Acquire a real session cookie from the mock server
    const sessionId = await acquireSessionCookie(request, baseURL || 'http://localhost:3000');

    // 2. Set the cookie in the browser context before navigation
    await page.context().addCookies([
      {
        name: 'sessionId',
        value: sessionId,
        domain: 'localhost',
        path: '/',
      },
    ]);

    // 3. Intercept all WebSocket connections to *:81
    //    The frontend dials: ws://<hostname>:81
    let wsRoute = null;

    // Capture array for WS messages sent by the frontend (excludes auth)
    page.wsCapture = [];

    await page.routeWebSocket(/.*:81/, (ws) => {
      wsRoute = ws;

      // Mirror close events from server side to the frontend
      ws.onClose(() => {});

      ws.onMessage((msg) => {
        // All messages from the frontend are text JSON
        let data;
        try {
          data = JSON.parse(msg);
        } catch {
          return;
        }

        const { type } = data;

        if (type === 'auth') {
          // Complete the auth handshake
          ws.send(JSON.stringify({ type: 'authSuccess' }));

          // Broadcast all initial state messages in sequence
          const initialMessages = buildInitialState();
          for (const msg of initialMessages) {
            ws.send(JSON.stringify(msg));
          }
          return;
        }

        // Capture non-auth messages for test assertions
        page.wsCapture.push(data);

        // Route any other inbound commands and send responses
        const responses = handleCommand(type, data);
        for (const resp of responses) {
          ws.send(JSON.stringify(resp));
        }
      });

      // Immediately fire authRequired to trigger the frontend auth flow
      ws.send(JSON.stringify({ type: 'authRequired' }));
    });

    // 4. Navigate to the main page
    await page.goto('/');

    // 5. Wait until the WS connection status shows "Connected"
    await expect(page.locator('#wsConnectionStatus')).toHaveText('Connected', { timeout: 10000 });

    // Attach wsRoute helper so tests can inject additional messages
    page.wsRoute = {
      send: (obj) => wsRoute && wsRoute.send(JSON.stringify(obj)),
      sendBinary: (buf) => wsRoute && wsRoute.send(buf),
    };

    await use(page);
  },
});

module.exports = { test, expect };
