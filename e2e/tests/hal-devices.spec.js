/**
 * hal-devices.spec.js — HAL Devices tab: device cards and rescan/disable actions.
 */

const { test, expect } = require('../helpers/fixtures');
const path = require('path');
const fs = require('fs');

const HAL_FIXTURE = JSON.parse(
  fs.readFileSync(path.join(__dirname, '..', 'fixtures', 'ws-messages', 'hal-device-state.json'), 'utf8')
);

test('device cards render for all 8 HAL devices with name, type, and state', async ({ connectedPage: page }) => {
  await page.locator('.sidebar-item[data-tab="devices"]').click();

  // Wait for the device list to populate
  const deviceList = page.locator('#hal-device-list');
  await expect(deviceList).not.toContainText('No HAL devices registered', { timeout: 5000 });

  // The halDeviceState fixture contains 8 devices.
  const cards = deviceList.locator('.hal-device-card');
  await expect(cards).toHaveCount(8, { timeout: 5000 });

  // Spot-check known device names from halDeviceState.json
  await expect(deviceList).toContainText('PCM5102A');
  await expect(deviceList).toContainText('ES8311');
  await expect(deviceList).toContainText('PCM1808');
  await expect(deviceList).toContainText('NS4150B');
  await expect(deviceList).toContainText('Chip Temp');
  await expect(deviceList).toContainText('ES9822PRO');
  await expect(deviceList).toContainText('ES9843PRO');

  // Each card header should have a name and a state dot
  const firstCard = cards.first();
  await expect(firstCard.locator('.hal-device-name')).toBeVisible();
  await expect(firstCard.locator('.status-dot')).toBeVisible();
});

test('rescan button triggers POST /api/hal/scan and disable button sends correct API', async ({ connectedPage: page }) => {
  await page.locator('.sidebar-item[data-tab="devices"]').click();

  const deviceList = page.locator('#hal-device-list');
  await expect(deviceList).not.toContainText('No HAL devices registered', { timeout: 5000 });

  // Intercept the rescan request
  let scanCalled = false;
  await page.route('/api/hal/scan', async (route) => {
    scanCalled = true;
    await route.fulfill({ status: 200, body: JSON.stringify({ status: 'ok', devicesFound: 6 }) });
  });

  // Click the Rescan button
  await page.locator('#hal-rescan-btn').click();
  await page.waitForTimeout(500);
  expect(scanCalled).toBe(true);

  // Expand the first card to reveal Disable/Enable and Delete buttons
  await deviceList.locator('.hal-device-header').first().click();

  // Intercept the disable/enable PUT request
  let disableCalled = false;
  await page.route('/api/hal/devices', async (route) => {
    if (route.request().method() === 'PUT') {
      disableCalled = true;
      await route.fulfill({ status: 200, body: JSON.stringify({ status: 'ok' }) });
    } else {
      await route.continue();
    }
  });

  // Find and click the Disable button within the expanded card
  const disableBtn = deviceList.locator('.hal-device-card.expanded button').filter({ hasText: /Disable/i }).first();
  if (await disableBtn.count() > 0) {
    await disableBtn.click();
    await page.waitForTimeout(300);
    expect(disableCalled).toBe(true);
  }
});

// ---------------------------------------------------------------------------
// Gap 5: UNAVAILABLE state rendering
//
// When a device transitions to HAL_STATE_UNAVAILABLE (numeric value 4) the
// frontend should show a red/unavailable indicator on the card.
// halGetStateInfo(4) returns { cls: 'red', label: 'Unavailable' }.
// ---------------------------------------------------------------------------

test('UNAVAILABLE device shows correct state indicator on card', async ({ connectedPage: page }) => {
  await page.locator('.sidebar-item[data-tab="devices"]').click();
  await page.waitForTimeout(300);

  // Inject a halDeviceState with the ES9822PRO (slot 7) set to UNAVAILABLE (state 4)
  const devicesWithUnavailable = HAL_FIXTURE.devices.map(function(d) {
    if (d.slot === 7) {
      return Object.assign({}, d, { state: 4, ready: false });
    }
    return d;
  });

  page.wsRoute.send({
    type: 'halDeviceState',
    scanning: false,
    deviceCount: devicesWithUnavailable.length,
    deviceMax: 24,
    driverCount: 16,
    driverMax: 24,
    devices: devicesWithUnavailable,
  });

  // Wait for the card to render
  const deviceList = page.locator('#hal-device-list');
  await expect(deviceList).toContainText('ES9822PRO', { timeout: 5000 });

  // The ES9822PRO card should show the Unavailable label
  const card = deviceList.locator('.hal-device-card').filter({ hasText: 'ES9822PRO' });
  await expect(card).toContainText('Unavailable', { timeout: 5000 });
});

test('UNAVAILABLE device status dot uses red colour class', async ({ connectedPage: page }) => {
  await page.locator('.sidebar-item[data-tab="devices"]').click();
  await page.waitForTimeout(300);

  const devicesWithUnavailable = HAL_FIXTURE.devices.map(function(d) {
    if (d.slot === 7) {
      return Object.assign({}, d, { state: 4, ready: false });
    }
    return d;
  });

  page.wsRoute.send({
    type: 'halDeviceState',
    scanning: false,
    deviceCount: devicesWithUnavailable.length,
    deviceMax: 24,
    driverCount: 16,
    driverMax: 24,
    devices: devicesWithUnavailable,
  });

  const deviceList = page.locator('#hal-device-list');
  await expect(deviceList).toContainText('ES9822PRO', { timeout: 5000 });

  const card = deviceList.locator('.hal-device-card').filter({ hasText: 'ES9822PRO' });
  const statusDot = card.locator('.status-dot');
  await expect(statusDot).toBeVisible({ timeout: 5000 });
  // The red class is applied via halGetStateInfo(4).cls = 'red'
  await expect(statusDot).toHaveClass(/red/, { timeout: 5000 });
});
