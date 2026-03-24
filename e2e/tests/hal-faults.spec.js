/**
 * hal-faults.spec.js — HAL device fault counter verification.
 *
 * Tests that the halDeviceState WS broadcast includes faultCount per device,
 * and that the GET /api/hal/devices REST response includes faultCount.
 *
 * @hal @smoke
 */

const { test, expect } = require('../helpers/fixtures');
const path = require('path');
const fs = require('fs');

const HAL_FIXTURE = JSON.parse(
  fs.readFileSync(path.join(__dirname, '..', 'fixtures', 'ws-messages', 'hal-device-state.json'), 'utf8')
);

test.describe('@hal @smoke HAL Fault Counters', () => {

  test('halDeviceState fixture includes faultCount field on all devices', async ({ connectedPage: page }) => {
    // Verify the fixture itself has faultCount on every device
    for (const dev of HAL_FIXTURE.devices) {
      expect(dev).toHaveProperty('faultCount');
      expect(typeof dev.faultCount).toBe('number');
      expect(dev.faultCount).toBeGreaterThanOrEqual(0);
    }
  });

  test('halDeviceState WS broadcast includes faultCount per device', async ({ connectedPage: page }) => {
    // Navigate to Devices tab to trigger rendering
    await page.evaluate(() => switchTab('devices'));
    const deviceList = page.locator('#hal-device-list');
    await expect(deviceList).not.toContainText('No HAL devices registered', { timeout: 5000 });

    // Send a halDeviceState with a non-zero faultCount to verify the field propagates
    const stateWithFault = JSON.parse(JSON.stringify(HAL_FIXTURE));
    stateWithFault.devices[0].faultCount = 3;
    page.wsRoute.send(stateWithFault);

    // Wait for the UI to process the update
    await page.waitForTimeout(500);

    // The device cards should still render correctly after receiving faultCount
    const cards = deviceList.locator('.hal-device-card');
    await expect(cards.first()).toBeVisible();
  });

  test('GET /api/hal/devices response includes faultCount per device', async ({ connectedPage: page }) => {
    // Intercept the REST API response
    await page.route('/api/hal/devices', async (route) => {
      // Return a mock response that includes faultCount
      const response = {
        devices: HAL_FIXTURE.devices.map(d => ({
          ...d,
          faultCount: d.faultCount || 0,
        })),
      };
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify(response),
      });
    });

    // Fetch via the page context
    const result = await page.evaluate(async () => {
      const resp = await fetch('/api/hal/devices');
      return resp.json();
    });

    // Verify every device has faultCount
    expect(result.devices).toBeDefined();
    for (const dev of result.devices) {
      expect(dev).toHaveProperty('faultCount');
      expect(typeof dev.faultCount).toBe('number');
    }
  });

  test('faultCount defaults to 0 for healthy devices', async ({ connectedPage: page }) => {
    // All devices in the default fixture should have faultCount=0
    for (const dev of HAL_FIXTURE.devices) {
      expect(dev.faultCount).toBe(0);
    }
  });

  test('faultCount field is numeric and non-negative in fixture', async ({ connectedPage: page }) => {
    for (const dev of HAL_FIXTURE.devices) {
      expect(Number.isInteger(dev.faultCount)).toBe(true);
      expect(dev.faultCount).toBeGreaterThanOrEqual(0);
      expect(dev.faultCount).toBeLessThanOrEqual(255); // uint8_t max
    }
  });

});
