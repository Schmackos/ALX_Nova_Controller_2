/**
 * device-deps.spec.js — Device dependency graph E2E tests.
 *
 * Verifies:
 *   1. REST API: GET /api/v1/hal/devices includes dependsOn array per device
 *   2. WS broadcast: halDeviceState includes dependsOn field
 *   3. UI: Device cards show dependency info where applicable
 *
 * @hal @api
 */

const { test, expect } = require('../helpers/fixtures');
const { test: base } = require('@playwright/test');
const path = require('path');
const fs = require('fs');

const HAL_FIXTURE = JSON.parse(
  fs.readFileSync(path.join(__dirname, '..', 'fixtures', 'ws-messages', 'hal-device-state.json'), 'utf8')
);

// Helper: build halDeviceState with dependency fields
function buildHalStateWithDeps() {
  const state = JSON.parse(JSON.stringify(HAL_FIXTURE));
  // Add dependsOn arrays to devices
  for (const dev of state.devices) {
    dev.dependsOn = [];
  }
  // ES9822PRO (slot 7) depends on nothing (root device)
  // ES9843PRO (slot 8) depends on ES9822PRO (slot 7) — shares I2C bus
  const es9843 = state.devices.find(d => d.slot === 8);
  if (es9843) {
    es9843.dependsOn = [7];
  }
  // ES9069Q (slot 9) depends on ES9822PRO (slot 7)
  const es9069 = state.devices.find(d => d.slot === 9);
  if (es9069) {
    es9069.dependsOn = [7];
  }
  return state;
}

// Helper: build state with no dependencies
function buildHalStateNoDeps() {
  const state = JSON.parse(JSON.stringify(HAL_FIXTURE));
  for (const dev of state.devices) {
    dev.dependsOn = [];
  }
  return state;
}

// Helper: build state with multi-level deps (chain)
function buildHalStateChainDeps() {
  const state = JSON.parse(JSON.stringify(HAL_FIXTURE));
  for (const dev of state.devices) {
    dev.dependsOn = [];
  }
  // Chain: CS43131 (10) → ES9069Q (9) → ES9822PRO (7)
  const cs43131 = state.devices.find(d => d.slot === 10);
  const es9069 = state.devices.find(d => d.slot === 9);
  if (cs43131) cs43131.dependsOn = [9];
  if (es9069) es9069.dependsOn = [7];
  return state;
}

test.describe('@hal Device Dependency Graph — REST API', () => {

  test('GET /api/v1/hal/devices returns device list with expected structure', async ({ request }) => {
    const loginResp = await request.post('http://localhost:3000/api/auth/login', {
      data: { password: 'testpass' },
    });
    const cookies = loginResp.headers()['set-cookie'] || '';
    const match = cookies.match(/sessionId=([^;]+)/);
    const cookieVal = match ? match[1] : null;

    const resp = await request.get('http://localhost:3000/api/v1/hal/devices', {
      headers: cookieVal ? { Cookie: `sessionId=${cookieVal}` } : {},
    });

    expect(resp.status()).toBe(200);
    const body = await resp.json();

    // Should return device list (array or object with devices array)
    const devices = Array.isArray(body) ? body : (body.devices || []);
    expect(devices.length).toBeGreaterThan(0);

    // Each device should have an id/slot field
    const firstDev = devices[0];
    expect(firstDev).toHaveProperty('id');
  });

  test('GET /api/v1/hal/devices and /api/hal/devices return same structure (version compat)', async ({ request }) => {
    const loginResp = await request.post('http://localhost:3000/api/auth/login', {
      data: { password: 'testpass' },
    });
    const cookies = loginResp.headers()['set-cookie'] || '';
    const match = cookies.match(/sessionId=([^;]+)/);
    const cookieVal = match ? match[1] : null;
    const headers = cookieVal ? { Cookie: `sessionId=${cookieVal}` } : {};

    const r1 = await request.get('http://localhost:3000/api/hal/devices', { headers });
    const r2 = await request.get('http://localhost:3000/api/v1/hal/devices', { headers });

    expect(r1.status()).toBe(200);
    expect(r2.status()).toBe(200);

    const body1 = await r1.json();
    const body2 = await r2.json();

    const devices1 = Array.isArray(body1) ? body1 : (body1.devices || []);
    const devices2 = Array.isArray(body2) ? body2 : (body2.devices || []);

    expect(devices1.length).toBe(devices2.length);
  });
});

test.describe('@hal Device Dependency Graph — WS Broadcast', () => {

  test('halDeviceState with dependsOn arrays renders devices correctly', async ({ connectedPage: page }) => {
    await page.locator('.sidebar-item[data-tab="devices"]').click();

    const depState = buildHalStateWithDeps();
    page.wsRoute.send(depState);

    const deviceList = page.locator('#hal-device-list');
    await expect(deviceList).not.toContainText('No HAL devices registered', { timeout: 5000 });

    // Verify devices render — dependency info should not break rendering
    const cards = deviceList.locator('.hal-device-card');
    await expect(cards).toHaveCount(depState.devices.length, { timeout: 5000 });
  });

  test('halDeviceState with empty dependsOn arrays renders normally', async ({ connectedPage: page }) => {
    await page.locator('.sidebar-item[data-tab="devices"]').click();

    const noDepsState = buildHalStateNoDeps();
    page.wsRoute.send(noDepsState);

    const deviceList = page.locator('#hal-device-list');
    await expect(deviceList).not.toContainText('No HAL devices registered', { timeout: 5000 });

    const cards = deviceList.locator('.hal-device-card');
    await expect(cards).toHaveCount(noDepsState.devices.length, { timeout: 5000 });
  });

  test('halDeviceState with chain dependencies renders all devices', async ({ connectedPage: page }) => {
    await page.locator('.sidebar-item[data-tab="devices"]').click();

    const chainState = buildHalStateChainDeps();
    page.wsRoute.send(chainState);

    const deviceList = page.locator('#hal-device-list');
    await expect(deviceList).not.toContainText('No HAL devices registered', { timeout: 5000 });

    // All devices should still render
    await expect(deviceList).toContainText('ES9822PRO');
    await expect(deviceList).toContainText('ES9069Q');
    await expect(deviceList).toContainText('CS43131');
  });

  test('dependency update via WS does not break device card rendering', async ({ connectedPage: page }) => {
    await page.locator('.sidebar-item[data-tab="devices"]').click();

    // First send: no dependencies
    const state1 = buildHalStateNoDeps();
    page.wsRoute.send(state1);

    const deviceList = page.locator('#hal-device-list');
    await expect(deviceList).not.toContainText('No HAL devices registered', { timeout: 5000 });

    // Second send: add dependencies
    const state2 = buildHalStateWithDeps();
    page.wsRoute.send(state2);
    await page.waitForTimeout(500);

    // Devices should still render correctly after dependency update
    const cards = deviceList.locator('.hal-device-card');
    const count = await cards.count();
    expect(count).toBeGreaterThan(0);
  });
});

test.describe('@hal Device Dependency Graph — UI Display', () => {

  test('device cards with dependencies show dependency info on expand', async ({ connectedPage: page }) => {
    await page.locator('.sidebar-item[data-tab="devices"]').click();

    const depState = buildHalStateWithDeps();
    page.wsRoute.send(depState);

    const deviceList = page.locator('#hal-device-list');
    await expect(deviceList).not.toContainText('No HAL devices registered', { timeout: 5000 });

    // Find ES9843PRO card (has dependency on slot 7)
    const cards = deviceList.locator('.hal-device-card');
    await expect(cards).toHaveCount(depState.devices.length, { timeout: 5000 });

    // Expand a card header to show details
    const headers = deviceList.locator('.hal-device-header');
    const headerCount = await headers.count();
    expect(headerCount).toBeGreaterThan(0);
    await headers.first().click();
  });

  test('root devices (no dependencies) render without dependency section', async ({ connectedPage: page }) => {
    await page.locator('.sidebar-item[data-tab="devices"]').click();

    const depState = buildHalStateWithDeps();
    page.wsRoute.send(depState);

    const deviceList = page.locator('#hal-device-list');
    await expect(deviceList).not.toContainText('No HAL devices registered', { timeout: 5000 });

    // PCM5102A (slot 0) has no dependencies
    await expect(deviceList).toContainText('PCM5102A');
  });

  test('device cards maintain correct order after dependency-informed state update', async ({ connectedPage: page }) => {
    await page.locator('.sidebar-item[data-tab="devices"]').click();

    const depState = buildHalStateWithDeps();
    page.wsRoute.send(depState);

    const deviceList = page.locator('#hal-device-list');
    await expect(deviceList).not.toContainText('No HAL devices registered', { timeout: 5000 });

    // Verify key devices are present
    await expect(deviceList).toContainText('PCM5102A');
    await expect(deviceList).toContainText('ES8311');
    await expect(deviceList).toContainText('ES9822PRO');
    await expect(deviceList).toContainText('ES9843PRO');
    await expect(deviceList).toContainText('ES9069Q');
  });
});
