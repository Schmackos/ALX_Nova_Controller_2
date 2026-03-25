/**
 * api-versioning.spec.js — REST API /api/v1/ prefix compatibility tests.
 *
 * Verifies that all REST endpoints are reachable under both:
 *   /api/<path>     — original path (backward compat)
 *   /api/v1/<path>  — versioned path (new in Phase 1+2 hardening)
 *
 * Both paths must return the same JSON structure and HTTP 200.
 * Tests run directly against the mock server (no WS auth required for
 * read-only GET endpoints that the server exposes without session checks,
 * or with session cookie for auth-guarded endpoints).
 *
 * @api @smoke
 */

const { test, expect } = require('@playwright/test');

const BASE_URL = 'http://localhost:3000';

// Helper: POST login and return session cookie value
async function loginAndGetCookie(request) {
  const resp = await request.post(`${BASE_URL}/api/auth/login`, {
    data: { password: 'testpass' },
  });
  const cookies = resp.headers()['set-cookie'] || '';
  const match = cookies.match(/sessionId=([^;]+)/);
  return match ? match[1] : null;
}

// Helper: fetch with optional session cookie
async function fetchJson(request, path, cookieValue) {
  const headers = cookieValue ? { Cookie: `sessionId=${cookieValue}` } : {};
  return request.get(`${BASE_URL}${path}`, { headers });
}

test.describe('@api @smoke API Version Prefix (/api/v1/)', () => {

  test('GET /api/pipeline/matrix and /api/v1/pipeline/matrix return identical responses', async ({ request }) => {
    const cookieVal = await loginAndGetCookie(request);

    const r1 = await fetchJson(request, '/api/pipeline/matrix', cookieVal);
    const r2 = await fetchJson(request, '/api/v1/pipeline/matrix', cookieVal);

    expect(r1.status()).toBe(200);
    expect(r2.status()).toBe(200);

    const body1 = await r1.json();
    const body2 = await r2.json();
    expect(body1.success).toBe(true);
    expect(body2.success).toBe(true);
    expect(body1.size).toEqual(body2.size);
    expect(body1.inputs).toEqual(body2.inputs);
    expect(body1.outputs).toEqual(body2.outputs);
  });

  test('GET /api/pipeline/sinks and /api/v1/pipeline/sinks return identical responses', async ({ request }) => {
    const cookieVal = await loginAndGetCookie(request);

    const r1 = await fetchJson(request, '/api/pipeline/sinks', cookieVal);
    const r2 = await fetchJson(request, '/api/v1/pipeline/sinks', cookieVal);

    expect(r1.status()).toBe(200);
    expect(r2.status()).toBe(200);
    const body1 = await r1.json();
    const body2 = await r2.json();
    expect(body1.success).toBe(true);
    expect(body2.success).toBe(true);
    expect(body1.sinks).toEqual(body2.sinks);
  });

  test('GET /api/pipeline/status and /api/v1/pipeline/status return format negotiation fields', async ({ request }) => {
    const cookieVal = await loginAndGetCookie(request);

    const r1 = await fetchJson(request, '/api/pipeline/status', cookieVal);
    const r2 = await fetchJson(request, '/api/v1/pipeline/status', cookieVal);

    expect(r1.status()).toBe(200);
    expect(r2.status()).toBe(200);
    const body1 = await r1.json();
    const body2 = await r2.json();

    // Format negotiation fields must be present
    expect(typeof body1.rateMismatch).toBe('boolean');
    expect(Array.isArray(body1.laneSampleRates)).toBe(true);
    expect(Array.isArray(body1.laneDsd)).toBe(true);
    expect(Array.isArray(body1.sinks)).toBe(true);

    // Both paths must return the same data
    expect(body1).toEqual(body2);
  });

  test('GET /api/hal/devices and /api/v1/hal/devices return identical responses', async ({ request }) => {
    const cookieVal = await loginAndGetCookie(request);

    const r1 = await fetchJson(request, '/api/hal/devices', cookieVal);
    const r2 = await fetchJson(request, '/api/v1/hal/devices', cookieVal);

    expect(r1.status()).toBe(200);
    expect(r2.status()).toBe(200);
    const body1 = await r1.json();
    const body2 = await r2.json();
    // Both paths return the same device list (array)
    expect(Array.isArray(body1)).toBe(true);
    expect(body1).toEqual(body2);
  });

  test('GET /api/i2s/ports and /api/v1/i2s/ports return identical responses', async ({ request }) => {
    const cookieVal = await loginAndGetCookie(request);

    const r1 = await fetchJson(request, '/api/i2s/ports', cookieVal);
    const r2 = await fetchJson(request, '/api/v1/i2s/ports', cookieVal);

    expect(r1.status()).toBe(200);
    expect(r2.status()).toBe(200);
    const body1 = await r1.json();
    const body2 = await r2.json();
    expect(body1).toEqual(body2);
  });

  test('GET /api/health and /api/v1/health return identical responses', async ({ request }) => {
    const cookieVal = await loginAndGetCookie(request);

    const r1 = await fetchJson(request, '/api/health', cookieVal);
    const r2 = await fetchJson(request, '/api/v1/health', cookieVal);

    expect(r1.status()).toBe(200);
    expect(r2.status()).toBe(200);
    const body1 = await r1.json();
    const body2 = await r2.json();
    expect(body1).toEqual(body2);
  });

  test('GET /api/diagnostics and /api/v1/diagnostics return identical responses', async ({ request }) => {
    const cookieVal = await loginAndGetCookie(request);

    const r1 = await fetchJson(request, '/api/diagnostics', cookieVal);
    const r2 = await fetchJson(request, '/api/v1/diagnostics', cookieVal);

    expect(r1.status()).toBe(200);
    expect(r2.status()).toBe(200);
    const body1 = await r1.json();
    const body2 = await r2.json();
    expect(body1).toEqual(body2);
  });

  test('GET /api/psram/status and /api/v1/psram/status return identical responses', async ({ request }) => {
    const cookieVal = await loginAndGetCookie(request);

    const r1 = await fetchJson(request, '/api/psram/status', cookieVal);
    const r2 = await fetchJson(request, '/api/v1/psram/status', cookieVal);

    expect(r1.status()).toBe(200);
    expect(r2.status()).toBe(200);
    const body1 = await r1.json();
    const body2 = await r2.json();
    expect(body1).toEqual(body2);
  });

  test('WS token endpoint available on both /api/ws-token and /api/v1/ws-token', async ({ request }) => {
    const cookieVal = await loginAndGetCookie(request);

    const r1 = await fetchJson(request, '/api/ws-token', cookieVal);
    const r2 = await fetchJson(request, '/api/v1/ws-token', cookieVal);

    expect(r1.status()).toBe(200);
    expect(r2.status()).toBe(200);
    const body1 = await r1.json();
    const body2 = await r2.json();
    // Both should return a token (values differ since they're one-time tokens)
    expect(body1.success).toBe(true);
    expect(body2.success).toBe(true);
    expect(typeof body1.token).toBe('string');
    expect(typeof body2.token).toBe('string');
  });

  test('apiFetch() rewrites /api/ to /api/v1/ automatically in the frontend', async ({ page, request }) => {
    // This test verifies the JS-level auto-rewrite by checking network requests
    // Use the page to observe whether requests go to /api/v1/ path
    const cookieVal = await loginAndGetCookie(request);
    await page.context().addCookies([
      { name: 'sessionId', value: cookieVal, domain: 'localhost', path: '/' },
    ]);

    // Track all outgoing API requests from the page
    const interceptedUrls = [];
    await page.route('**/api/**', async (route) => {
      interceptedUrls.push(route.request().url());
      await route.continue();
    });

    await page.goto(`${BASE_URL}/`);

    // Wait for initial page load requests
    await page.waitForTimeout(500);

    // Any requests that went through apiFetch() should use /api/v1/
    const v1Requests = interceptedUrls.filter(u => u.includes('/api/v1/'));
    const nonV1ApiRequests = interceptedUrls.filter(u =>
      u.includes('/api/') &&
      !u.includes('/api/v1/') &&
      !u.includes('/api/__test__/')
    );

    // All non-__test__ API requests from the page must go through /api/v1/
    expect(nonV1ApiRequests.length).toBe(0);
    expect(v1Requests.length).toBeGreaterThan(0);
  });

});
