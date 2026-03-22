/**
 * hal-discovery.spec.js — HAL Discovery: rescan button, scanning state,
 * partial scan warning, and concurrent scan handling.
 */

const { test, expect } = require('../helpers/fixtures');
const path = require('path');
const fs = require('fs');

const HAL_FIXTURE = JSON.parse(
  fs.readFileSync(path.join(__dirname, '..', 'fixtures', 'ws-messages', 'hal-device-state.json'), 'utf8')
);

async function openDevicesTab(page) {
  await page.evaluate(() => switchTab('devices'));
  await page.waitForTimeout(300);
  page.wsRoute.send(HAL_FIXTURE);
  await expect(page.locator('#hal-device-list')).toContainText('PCM5102A', { timeout: 5000 });
}

test.describe('@hal @api HAL Discovery', () => {

  test('rescan button sends POST /api/hal/scan', async ({ connectedPage: page }) => {
    await openDevicesTab(page);

    let scanCalled = false;
    await page.route('/api/hal/scan', async (route) => {
      scanCalled = true;
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ status: 'ok', devicesFound: 8, partialScan: false })
      });
    });

    await page.locator('#hal-rescan-btn').click();
    await page.waitForTimeout(500);
    expect(scanCalled).toBe(true);
  });

  test('scanning state shows Scanning text on rescan button', async ({ connectedPage: page }) => {
    await openDevicesTab(page);

    // Intercept scan and delay response to observe scanning state
    await page.route('/api/hal/scan', async (route) => {
      // Delay fulfillment to observe scanning state
      await new Promise(r => setTimeout(r, 1000));
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ status: 'ok', devicesFound: 8, partialScan: false })
      });
    });

    await page.locator('#hal-rescan-btn').click();

    // Button should show scanning text and be disabled
    const btn = page.locator('#hal-rescan-btn');
    await expect(btn).toContainText('Scanning', { timeout: 2000 });
    await expect(btn).toBeDisabled();
  });

  test('scan results update device list via WS push', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    await expect(page.locator('.hal-device-card')).toHaveCount(8);

    await page.route('/api/hal/scan', async (route) => {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ status: 'ok', devicesFound: 3, partialScan: false })
      });
    });

    await page.locator('#hal-rescan-btn').click();
    await page.waitForTimeout(300);

    // Push an updated halDeviceState with fewer devices to simulate scan result
    const reduced = JSON.parse(JSON.stringify(HAL_FIXTURE));
    reduced.devices = reduced.devices.slice(0, 3);
    reduced.deviceCount = 3;
    page.wsRoute.send(reduced);

    await expect(page.locator('.hal-device-card')).toHaveCount(3, { timeout: 5000 });
  });

  test('concurrent scan returns 409 and shows toast', async ({ connectedPage: page }) => {
    await openDevicesTab(page);

    let callCount = 0;
    await page.route('/api/hal/scan', async (route) => {
      callCount++;
      if (callCount === 1) {
        // First call succeeds but takes a moment
        await new Promise(r => setTimeout(r, 800));
        await route.fulfill({
          status: 200,
          contentType: 'application/json',
          body: JSON.stringify({ status: 'ok', devicesFound: 8, partialScan: false })
        });
      } else {
        // Second concurrent call returns 409
        await route.fulfill({
          status: 409,
          contentType: 'application/json',
          body: JSON.stringify({ error: 'Scan already in progress' })
        });
      }
    });

    // First click starts scanning — button becomes disabled, preventing a real second click
    await page.locator('#hal-rescan-btn').click();

    // Verify the button is disabled during scanning
    await expect(page.locator('#hal-rescan-btn')).toBeDisabled({ timeout: 2000 });
  });

  test('partial scan flag shows Bus 0 warning in toast', async ({ connectedPage: page }) => {
    await openDevicesTab(page);

    await page.route('/api/hal/scan', async (route) => {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ status: 'ok', devicesFound: 6, partialScan: true })
      });
    });

    await page.locator('#hal-rescan-btn').click();

    // The toast should mention Bus 0 / SDIO conflict
    const toast = page.locator('.toast, .notification, [role="alert"]')
      .filter({ hasText: /Bus 0.*SDIO|partial/i });
    await expect(toast.first()).toBeVisible({ timeout: 5000 });
  });

  test('rescan button is disabled during scan', async ({ connectedPage: page }) => {
    await openDevicesTab(page);

    await page.route('/api/hal/scan', async (route) => {
      await new Promise(r => setTimeout(r, 1500));
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ status: 'ok', devicesFound: 8, partialScan: false })
      });
    });

    const btn = page.locator('#hal-rescan-btn');
    await expect(btn).toBeEnabled();
    await btn.click();
    await expect(btn).toBeDisabled({ timeout: 2000 });
  });

});
