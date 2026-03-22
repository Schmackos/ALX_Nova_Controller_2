/**
 * error-ws-disconnect.spec.js — WebSocket disconnect/reconnect error handling tests.
 *
 * Tests verify the frontend's behavior when the WebSocket connection is lost
 * and (if applicable) re-established. Uses custom routeWebSocket setup to
 * expose the raw ws handle for server-side close().
 */

const { test: base, expect } = require('@playwright/test');
const { buildInitialState, handleCommand } = require('../helpers/ws-helpers');
const StatusBar = require('../pages/StatusBar');

/**
 * Helper: acquire a valid session cookie from the mock server.
 */
async function acquireSessionCookie(request, baseURL) {
  const resp = await request.post(`${baseURL}/api/auth/login`, {
    data: { password: 'testpass' },
  });
  const cookies = resp.headers()['set-cookie'];
  const match = cookies.match(/sessionId=([^;]+)/);
  return match[1];
}

/**
 * Custom fixture that exposes wsHandle with a close() method.
 * The connectedPage fixture does not expose close(), so we build our own.
 */
const test = base.extend({
  wsPage: async ({ page, request, baseURL }, use) => {
    const burl = baseURL || 'http://localhost:3000';
    const sessionId = await acquireSessionCookie(request, burl);

    await page.context().addCookies([{
      name: 'sessionId', value: sessionId, domain: 'localhost', path: '/',
    }]);

    // Track all ws handles so we can close them
    let currentWs = null;
    page.wsCapture = [];

    await page.routeWebSocket(/.*:81/, (ws) => {
      currentWs = ws;

      ws.onMessage((msg) => {
        let data;
        try { data = JSON.parse(msg); } catch { return; }

        if (data.type === 'auth') {
          ws.send(JSON.stringify({ type: 'authSuccess' }));
          const initialMessages = buildInitialState();
          for (const m of initialMessages) {
            ws.send(JSON.stringify(m));
          }
          return;
        }

        page.wsCapture.push(data);
        const responses = handleCommand(data.type, data);
        for (const resp of responses) {
          ws.send(JSON.stringify(resp));
        }
      });

      ws.send(JSON.stringify({ type: 'authRequired' }));
    });

    await page.goto('/');
    await expect(page.locator('#wsConnectionStatus')).toHaveText('Connected', { timeout: 10000 });

    // Expose helper object
    page.wsHandle = {
      send: (obj) => currentWs && currentWs.send(JSON.stringify(obj)),
      close: () => currentWs && currentWs.close(),
    };

    await use(page);
  },
});

test.describe('@error @ws WebSocket Disconnect', () => {
  test('WS close shows Disconnected status indicator', async ({ wsPage }) => {
    const page = wsPage;

    await test.step('verify initial connected state', async () => {
      await expect(page.locator('#wsConnectionStatus')).toHaveText('Connected');
    });

    await test.step('close WebSocket from server side and verify disconnected state', async () => {
      page.wsHandle.close();
      await expect(page.locator('#wsConnectionStatus')).toHaveText('Disconnected', { timeout: 5000 });
    });
  });

  test('status bar WS indicator updates on disconnect', async ({ wsPage }) => {
    const page = wsPage;
    const statusBar = new StatusBar(page);

    await test.step('verify WS indicator is online', async () => {
      await statusBar.expectWsState('online');
    });

    await test.step('close WS and verify indicator goes offline', async () => {
      page.wsHandle.close();
      await statusBar.expectWsState('offline');
    });
  });

  test('page remains functional (no crash) after disconnect', async ({ wsPage }) => {
    const page = wsPage;

    await test.step('close WebSocket', async () => {
      page.wsHandle.close();
      await expect(page.locator('#wsConnectionStatus')).toHaveText('Disconnected', { timeout: 5000 });
    });

    await test.step('verify page is still interactive — switch tabs', async () => {
      await page.evaluate(() => switchTab('settings'));
      await expect(page.locator('#settings')).toHaveClass(/active/);
    });

    await test.step('switch back to control tab', async () => {
      await page.evaluate(() => switchTab('control'));
      await expect(page.locator('#control')).toHaveClass(/active/);
    });
  });

  test('frontend auto-reconnects after disconnect', async ({ wsPage }) => {
    const page = wsPage;

    await test.step('speed up reconnect delay and close WS', async () => {
      await page.evaluate(() => { wsReconnectDelay = 500; });
      page.wsHandle.close();
      await expect(page.locator('#wsConnectionStatus')).toHaveText('Disconnected', { timeout: 5000 });
    });

    await test.step('wait for auto-reconnect to restore Connected status', async () => {
      // The frontend's onclose handler calls setTimeout(initWebSocket, wsReconnectDelay).
      // routeWebSocket will intercept the new connection, handle auth, and broadcast state.
      await expect(page.locator('#wsConnectionStatus')).toHaveText('Connected', { timeout: 15000 });
    });
  });

  test('multiple disconnect/reconnect cycles', async ({ wsPage }) => {
    const page = wsPage;

    for (let cycle = 1; cycle <= 2; cycle++) {
      await test.step(`cycle ${cycle}: disconnect`, async () => {
        // Reset reconnect delay before each close to avoid exponential backoff
        await page.evaluate(() => { wsReconnectDelay = 500; });
        page.wsHandle.close();
        await expect(page.locator('#wsConnectionStatus')).toHaveText('Disconnected', { timeout: 5000 });
      });

      await test.step(`cycle ${cycle}: reconnect`, async () => {
        await expect(page.locator('#wsConnectionStatus')).toHaveText('Connected', { timeout: 15000 });
      });
    }
  });

  test('data display preserved during brief disconnect', async ({ wsPage }) => {
    const page = wsPage;

    await test.step('verify initial signal status is rendered', async () => {
      await page.evaluate(() => switchTab('control'));
      const signalEl = page.locator('#signalDetected');
      await expect(signalEl).toBeVisible();
      const signalText = await signalEl.textContent();
      expect(signalText.length).toBeGreaterThan(0);
    });

    await test.step('disconnect and verify previously rendered data persists', async () => {
      const textBefore = await page.locator('#signalDetected').textContent();

      await page.evaluate(() => { wsReconnectDelay = 500; });
      page.wsHandle.close();
      await expect(page.locator('#wsConnectionStatus')).toHaveText('Disconnected', { timeout: 5000 });

      // The signal detected text should still be the same (DOM not cleared on disconnect)
      const textAfter = await page.locator('#signalDetected').textContent();
      expect(textAfter).toBe(textBefore);
    });
  });
});
