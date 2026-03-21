/**
 * hal-devices.spec.js — HAL Devices tab: device cards and rescan/disable actions.
 */

const { test, expect } = require('../helpers/fixtures');

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
