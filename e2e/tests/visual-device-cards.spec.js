/**
 * visual-device-cards.spec.js — Visual regression tests for HAL device cards
 * on the Devices tab.
 */

const { test, expect } = require('../helpers/fixtures');
const path = require('path');
const fs = require('fs');

const HAL_FIXTURE = JSON.parse(
  fs.readFileSync(path.join(__dirname, '..', 'fixtures', 'ws-messages', 'hal-device-state.json'), 'utf8')
);

/**
 * Navigate to the Devices tab and push a fresh halDeviceState WS message.
 */
async function openDevicesTab(page, fixture) {
  await page.evaluate(() => switchTab('devices'));
  await page.waitForTimeout(300);
  page.wsRoute.send(fixture || HAL_FIXTURE);
  const deviceList = page.locator('#hal-device-list');
  await expect(deviceList).not.toContainText('No HAL devices registered', { timeout: 5000 });
}

test.describe('@visual Visual Device Cards', () => {

  test('device list with multiple devices', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    const element = page.locator('#hal-device-list');
    await expect(element).toHaveScreenshot('device-list-multiple.png', {
      maxDiffPixelRatio: 0.02,
    });
  });

  test('expanded device card', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9822PRO' });
    await card.locator('.hal-device-header').click();
    await expect(card.locator('.hal-device-details')).toBeVisible({ timeout: 5000 });
    await expect(card).toHaveScreenshot('device-card-expanded.png', {
      maxDiffPixelRatio: 0.02,
    });
  });

  test('device card with ERROR state', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('devices'));
    await page.waitForTimeout(300);

    const modified = JSON.parse(JSON.stringify(HAL_FIXTURE));
    modified.devices[1].state = 5; // ES8311 -> Error
    modified.devices[1].errorReason = 'I2C timeout on bus 1';
    page.wsRoute.send(modified);

    const deviceList = page.locator('#hal-device-list');
    await expect(deviceList).toContainText('ES8311', { timeout: 5000 });

    const card = deviceList.locator('.hal-device-card').filter({ hasText: 'ES8311' });
    await expect(card.locator('.status-dot')).toHaveClass(/status-red/);
    await expect(card).toHaveScreenshot('device-card-error.png', {
      maxDiffPixelRatio: 0.02,
    });
  });

  test('device card with UNAVAILABLE state', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('devices'));
    await page.waitForTimeout(300);

    const modified = JSON.parse(JSON.stringify(HAL_FIXTURE));
    modified.devices[0].state = 4; // PCM5102A -> Unavailable
    modified.devices[0].ready = false;
    page.wsRoute.send(modified);

    const deviceList = page.locator('#hal-device-list');
    await expect(deviceList).toContainText('PCM5102A', { timeout: 5000 });

    const card = deviceList.locator('.hal-device-card').filter({ hasText: 'PCM5102A' });
    await expect(card).toHaveScreenshot('device-card-unavailable.png', {
      maxDiffPixelRatio: 0.02,
    });
  });

  test('capacity indicator at high usage', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('devices'));
    await page.waitForTimeout(300);

    const modified = JSON.parse(JSON.stringify(HAL_FIXTURE));
    modified.deviceCount = 22;
    modified.deviceMax = 24;
    page.wsRoute.send(modified);

    const deviceList = page.locator('#hal-device-list');
    await expect(deviceList).toContainText('PCM5102A', { timeout: 5000 });

    const indicator = page.locator('#hal-capacity-indicator');
    await expect(indicator).toContainText('22/24', { timeout: 3000 });
    await expect(indicator).toHaveScreenshot('capacity-high-usage.png', {
      maxDiffPixelRatio: 0.02,
    });
  });

  test('empty device list', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('devices'));
    await page.waitForTimeout(300);

    page.wsRoute.send({
      type: 'halDeviceState',
      scanning: false,
      deviceCount: 0,
      deviceMax: 24,
      driverCount: 0,
      driverMax: 24,
      devices: []
    });

    const deviceList = page.locator('#hal-device-list');
    await expect(deviceList).toContainText('No HAL devices registered', { timeout: 5000 });
    await expect(deviceList).toHaveScreenshot('device-list-empty.png', {
      maxDiffPixelRatio: 0.02,
    });
  });

});
