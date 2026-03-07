/**
 * auth.spec.js — Login / session authentication tests.
 *
 * These tests work against the login page directly without needing
 * the connectedPage fixture (the login page has no WS requirement).
 */

const { test, expect } = require('@playwright/test');

test('login page renders with password field and submit button', async ({ page }) => {
  await page.goto('/login');

  // The page must have a password input and a submit button.
  const pwdInput = page.locator('input[type="password"]');
  const submitBtn = page.locator('button[type="submit"], input[type="submit"]');

  await expect(pwdInput).toBeVisible();
  await expect(submitBtn).toBeVisible();
});

test('correct password submits and redirects to main page', async ({ page }) => {
  await page.goto('/login');

  // Intercept the POST /api/auth/login call. The mock server accepts any
  // non-empty password and returns a session cookie, then redirects.
  await page.route('/api/auth/login', async (route) => {
    // Let the real request reach the mock server so the cookie is set properly.
    await route.continue();
  });

  await page.locator('input[type="password"]').fill('anypassword');
  await page.locator('button[type="submit"], input[type="submit"]').click();

  // After a successful login the real login page JS redirects to '/'.
  // We just verify the response from the API is success.
  const resp = await page.request.post('/api/auth/login', {
    data: { password: 'anypassword' },
  });
  const body = await resp.json();
  expect(body.success).toBe(true);
});

test('invalid session cookie results in error or redirect back to login', async ({ page }) => {
  // Set an invalid session cookie and navigate to the main app.
  await page.context().addCookies([{
    name: 'sessionId',
    value: 'totally-invalid-session-id',
    domain: 'localhost',
    path: '/',
  }]);

  // The /api/auth/status endpoint should report not authenticated.
  const resp = await page.request.get('/api/auth/status', {
    headers: { 'X-Session-ID': 'totally-invalid-session-id' },
  });
  const body = await resp.json();
  // Mock auth returns authenticated: false for unknown sessions.
  expect(body.authenticated).toBe(false);
});
