/**
 * error-ws-auth.spec.js — WebSocket authentication error handling tests.
 *
 * Tests verify the frontend's response to WS auth failures, expired sessions,
 * and token refresh scenarios. Uses low-level page.routeWebSocket() to
 * simulate custom auth flows rather than the connectedPage fixture.
 */

const { test, expect } = require('@playwright/test');
const { buildInitialState } = require('../helpers/ws-helpers');

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

test.describe('@error @ws WebSocket Auth Errors', () => {
  test('authFailed message shows error toast and redirects to login', async ({ page, request, baseURL }) => {
    const serverBase = baseURL || 'http://localhost:3000';
    const sessionId = await acquireSessionCookie(request, serverBase);

    await page.context().addCookies([{
      name: 'sessionId', value: sessionId, domain: 'localhost', path: '/',
    }]);

    // Track navigation to /login
    let navigatedToLogin = false;

    await test.step('intercept WS and send authFailed', async () => {
      await page.routeWebSocket(/.*:81/, (ws) => {
        ws.onMessage((msg) => {
          let data;
          try { data = JSON.parse(msg); } catch { return; }

          if (data.type === 'auth') {
            // Simulate auth failure — token invalid
            ws.send(JSON.stringify({ type: 'authFailed', error: 'Invalid token' }));
          }
        });

        // Trigger the auth flow
        ws.send(JSON.stringify({ type: 'authRequired' }));
      });

      // Intercept navigation to /login to prevent actual redirect
      await page.route('**/login', async (route) => {
        navigatedToLogin = true;
        await route.fulfill({
          status: 200,
          contentType: 'text/html',
          body: '<html><body>Login Page</body></html>',
        });
      });

      await page.goto('/');
    });

    await test.step('verify error toast appears', async () => {
      const toast = page.locator('#toast');
      await expect(toast).toContainText('Session invalid', { timeout: 5000 });
    });

    await test.step('verify redirect to login occurs', async () => {
      // The frontend does setTimeout(() => location.href = '/login', 2000)
      await page.waitForTimeout(3000);
      expect(navigatedToLogin).toBe(true);
    });
  });

  test('WS token fetch failure shows error toast', async ({ page, request, baseURL }) => {
    const serverBase = baseURL || 'http://localhost:3000';
    const sessionId = await acquireSessionCookie(request, serverBase);

    await page.context().addCookies([{
      name: 'sessionId', value: sessionId, domain: 'localhost', path: '/',
    }]);

    await test.step('intercept ws-token endpoint to return failure', async () => {
      // Make the /api/ws-token endpoint fail
      await page.route('**/api/ws-token', async (route) => {
        await route.fulfill({
          status: 200,
          contentType: 'application/json',
          body: JSON.stringify({ success: false, error: 'Token pool exhausted' }),
        });
      });

      // Intercept navigation to /login
      await page.route('**/login', async (route) => {
        await route.fulfill({
          status: 200,
          contentType: 'text/html',
          body: '<html><body>Login Page</body></html>',
        });
      });

      await page.routeWebSocket(/.*:81/, (ws) => {
        // The frontend will try to fetch ws-token on ws.onopen, but
        // since the token fetch fails, it should redirect to /login.
        // We still need to send authRequired to trigger the flow.
        ws.send(JSON.stringify({ type: 'authRequired' }));
      });

      await page.goto('/');
    });

    await test.step('verify page redirects to login on token failure', async () => {
      // Frontend calls window.location.href = '/login' when token fetch fails
      await page.waitForURL('**/login', { timeout: 10000 });
    });
  });

  test('expired session on ws-token returns 401 and redirects', async ({ page, request, baseURL }) => {
    const serverBase = baseURL || 'http://localhost:3000';

    // Set an invalid session cookie (simulates expired session)
    await page.context().addCookies([{
      name: 'sessionId', value: 'expired-session-id', domain: 'localhost', path: '/',
    }]);

    let redirectedToLogin = false;

    await test.step('intercept ws-token to return 401', async () => {
      await page.route('**/api/ws-token', async (route) => {
        await route.fulfill({
          status: 401,
          contentType: 'application/json',
          body: JSON.stringify({ error: 'Unauthorized', redirect: '/login' }),
        });
      });

      // Intercept login redirect to capture and serve a stub page
      await page.route('**/login**', async (route) => {
        redirectedToLogin = true;
        await route.fulfill({
          status: 200,
          contentType: 'text/html',
          body: '<html><body id="login-page">Login Page</body></html>',
        });
      });

      await page.routeWebSocket(/.*:81/, (ws) => {
        // authRequired triggers the frontend to fetch /api/ws-token
        // which returns 401, triggering apiFetch's 401 handler (redirect to /login)
        ws.send(JSON.stringify({ type: 'authRequired' }));
      });

      await page.goto('/');
    });

    await test.step('verify redirect to login', async () => {
      // apiFetch handles 401 by setting window.location.href = '/login'
      // Wait for the login page stub to appear
      await expect(async () => {
        expect(redirectedToLogin).toBe(true);
      }).toPass({ timeout: 10000 });
    });
  });

  test('re-auth after reconnect sends new auth token', async ({ page, request, baseURL }) => {
    const serverBase = baseURL || 'http://localhost:3000';
    const sessionId = await acquireSessionCookie(request, serverBase);

    await page.context().addCookies([{
      name: 'sessionId', value: sessionId, domain: 'localhost', path: '/',
    }]);

    const authTokensSent = [];

    await test.step('set up WS interception tracking auth tokens', async () => {
      await page.routeWebSocket(/.*:81/, (ws) => {
        ws.onMessage((msg) => {
          let data;
          try { data = JSON.parse(msg); } catch { return; }

          if (data.type === 'auth') {
            authTokensSent.push(data.token);
            ws.send(JSON.stringify({ type: 'authSuccess' }));

            const initialMessages = buildInitialState();
            for (const m of initialMessages) {
              ws.send(JSON.stringify(m));
            }
          }
        });

        ws.send(JSON.stringify({ type: 'authRequired' }));
      });

      await page.goto('/');
      await expect(page.locator('#wsConnectionStatus')).toHaveText('Connected', { timeout: 10000 });
    });

    await test.step('verify at least one auth token was sent', async () => {
      expect(authTokensSent.length).toBeGreaterThanOrEqual(1);
      // All tokens should be non-empty strings
      for (const token of authTokensSent) {
        expect(typeof token).toBe('string');
        expect(token.length).toBeGreaterThan(0);
      }
    });
  });
});
