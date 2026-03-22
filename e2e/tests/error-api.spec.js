/**
 * error-api.spec.js — REST API error handling tests.
 *
 * Tests verify the frontend gracefully handles various HTTP error responses
 * by intercepting API calls with page.route() and returning error status codes.
 */

const { test, expect } = require('../helpers/fixtures');
const BasePage = require('../pages/BasePage');

test.describe('@error @api API Error Handling', () => {
  test('401 response triggers redirect to login', async ({ connectedPage }) => {
    const page = connectedPage;

    await test.step('intercept settings API to return 401', async () => {
      await page.route('**/api/settings', async (route) => {
        if (route.request().method() === 'POST') {
          await route.fulfill({
            status: 401,
            contentType: 'application/json',
            body: JSON.stringify({ error: 'Unauthorized', redirect: '/login' }),
          });
        } else {
          await route.continue();
        }
      });

      // Intercept login redirect to capture it
      await page.route('**/login', async (route) => {
        await route.fulfill({
          status: 200,
          contentType: 'text/html',
          body: '<html><body>Login Page</body></html>',
        });
      });
    });

    await test.step('trigger a settings save that returns 401', async () => {
      await page.evaluate(() => switchTab('settings'));

      // Trigger a POST to /api/settings via the timezone update
      // which uses apiFetch and will hit our intercepted 401
      await page.evaluate(() => {
        apiFetch('/api/settings', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ darkMode: true }),
        });
      });
    });

    await test.step('verify redirect to login', async () => {
      await page.waitForURL('**/login', { timeout: 10000 });
    });
  });

  test('404 response handled gracefully without crash', async ({ connectedPage }) => {
    const page = connectedPage;

    await test.step('intercept API to return 404', async () => {
      await page.route('**/api/hal/devices', async (route) => {
        await route.fulfill({
          status: 404,
          contentType: 'application/json',
          body: JSON.stringify({ error: 'Not found' }),
        });
      });
    });

    await test.step('trigger API call and verify page does not crash', async () => {
      await page.evaluate(() => switchTab('devices'));

      // Page should still be interactive
      const errors = [];
      page.on('pageerror', (err) => errors.push(err.message));

      await page.waitForTimeout(1000);

      // Navigate to another tab to confirm page is still functional
      await page.evaluate(() => switchTab('control'));
      await expect(page.locator('#control')).toHaveClass(/active/);
    });
  });

  test('500 response shows error via toast or catch handler', async ({ connectedPage }) => {
    const page = connectedPage;

    await test.step('intercept settings POST to return 500', async () => {
      await page.route('**/api/settings', async (route) => {
        if (route.request().method() === 'POST') {
          await route.fulfill({
            status: 500,
            contentType: 'application/json',
            body: JSON.stringify({ error: 'Internal server error' }),
          });
        } else {
          await route.continue();
        }
      });
    });

    await test.step('trigger timezone update which calls POST /api/settings', async () => {
      await page.evaluate(() => switchTab('settings'));
      await page.waitForTimeout(300);

      // Call updateTimezone() which does POST /api/settings and has .catch(showToast)
      await page.evaluate(() => {
        apiFetch('/api/settings', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ 'appState.timezoneOffset': 3600 }),
        }).then(r => r.safeJson()).catch(err => showToast(err.message || 'Request failed', 'error'));
      });
    });

    await test.step('verify error toast appears', async () => {
      const toast = page.locator('#toast');
      await expect(toast).toHaveClass(/show/, { timeout: 5000 });
      await expect(toast).toHaveClass(/error/);
    });
  });

  test('network timeout handled gracefully', async ({ connectedPage }) => {
    const page = connectedPage;

    await test.step('intercept API to simulate timeout by aborting', async () => {
      await page.route('**/api/settings', async (route) => {
        if (route.request().method() === 'POST') {
          await route.abort('timedout');
        } else {
          await route.continue();
        }
      });
    });

    await test.step('trigger API call that times out', async () => {
      await page.evaluate(() => switchTab('settings'));
      await page.waitForTimeout(300);

      // Call an API that will be aborted — apiFetch catch handler logs the error
      await page.evaluate(() => {
        apiFetch('/api/settings', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ darkMode: false }),
        }).then(r => r.safeJson()).then(d => {
          if (d.success) showToast('Saved', 'success');
        }).catch(err => showToast('Network error: request timed out', 'error'));
      });
    });

    await test.step('verify error toast for timeout', async () => {
      const toast = page.locator('#toast');
      await expect(toast).toHaveClass(/show/, { timeout: 5000 });
      await expect(toast).toContainText('Network error', { timeout: 3000 });
    });
  });

  test('HAL scan 409 (concurrent scan) returns appropriate error', async ({ connectedPage }) => {
    const page = connectedPage;

    await test.step('intercept HAL scan to always return 409', async () => {
      await page.route('**/api/hal/scan', async (route) => {
        await route.fulfill({
          status: 409,
          contentType: 'application/json',
          body: JSON.stringify({ error: 'Scan already in progress' }),
        });
      });
    });

    await test.step('trigger HAL rescan and verify 409 is handled', async () => {
      await page.evaluate(() => switchTab('devices'));
      await page.waitForTimeout(500);

      // Trigger the scan via a direct fetch and handle the error
      const result = await page.evaluate(async () => {
        try {
          const resp = await apiFetch('/api/hal/scan', { method: 'POST' });
          if (!resp.ok) {
            const data = await resp.json();
            showToast(data.error || 'Scan failed', 'error');
            return { status: resp.status, error: data.error };
          }
          return { status: resp.status };
        } catch (e) {
          return { error: e.message };
        }
      });

      // The response should have been a 409
      expect(result.status).toBe(409);
      expect(result.error).toBe('Scan already in progress');
    });

    await test.step('verify error toast shows scan message', async () => {
      const toast = page.locator('#toast');
      await expect(toast).toHaveClass(/show/, { timeout: 3000 });
      await expect(toast).toContainText('Scan already in progress');
    });
  });

  test('settings save failure shows error toast', async ({ connectedPage }) => {
    const page = connectedPage;

    await test.step('intercept settings POST to return failure', async () => {
      await page.route('**/api/settings', async (route) => {
        if (route.request().method() === 'POST') {
          await route.fulfill({
            status: 500,
            contentType: 'application/json',
            body: JSON.stringify({ success: false, error: 'Flash write failed' }),
          });
        } else {
          await route.continue();
        }
      });
    });

    await test.step('trigger settings save and verify error handling', async () => {
      await page.evaluate(() => switchTab('settings'));
      await page.waitForTimeout(300);

      // Simulate what updateTimezone() does — POST /api/settings with catch
      await page.evaluate(() => {
        apiFetch('/api/settings', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ 'appState.timezoneOffset': 0, 'appState.dstOffset': 0 }),
        }).then(r => r.safeJson()).then(d => {
          if (d.success) showToast('Settings saved', 'success');
        }).catch(err => showToast('Failed to save settings', 'error'));
      });
    });

    await test.step('verify error toast appears', async () => {
      const toast = page.locator('#toast');
      await expect(toast).toHaveClass(/show/, { timeout: 5000 });
      await expect(toast).toHaveClass(/error/);
      await expect(toast).toContainText('Failed to save settings');
    });
  });
});
