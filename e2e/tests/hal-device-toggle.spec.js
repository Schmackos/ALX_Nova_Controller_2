/**
 * hal-device-toggle.spec.js — HAL Device Toggle: enable/disable via toggle checkbox,
 * state transitions, and visual feedback.
 *
 * The enable toggle uses a PUT /api/hal/devices request (not WS command),
 * so we intercept the REST call and verify the payload.
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
  const deviceList = page.locator('#hal-device-list');
  await expect(deviceList).toContainText('ES9822PRO', { timeout: 5000 });
}

test.describe('@hal @ws HAL Device Toggle', () => {

  test('disable toggle sends PUT /api/hal/devices with enabled=false', async ({ connectedPage: page }) => {
    await openDevicesTab(page);

    // Intercept the PUT request
    let putPayload = null;
    await page.route('/api/hal/devices', async (route) => {
      if (route.request().method() === 'PUT') {
        putPayload = JSON.parse(route.request().postData());
        await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ status: 'ok' }) });
      } else {
        await route.fulfill({ status: 200, contentType: 'application/json', body: '[]' });
      }
    });

    // ES9822PRO is enabled — uncheck the toggle to disable
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9822PRO' });
    const toggle = card.locator('.hal-enable-toggle input[type="checkbox"]');
    await expect(toggle).toBeChecked();
    // CSS-hidden checkbox — click via the parent label element
    await card.locator('.hal-enable-toggle').click();

    await page.waitForTimeout(500);
    expect(putPayload).not.toBeNull();
    expect(putPayload.slot).toBe(7);
    expect(putPayload.enabled).toBe(false);
  });

  test('enable toggle sends PUT /api/hal/devices with enabled=true', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('devices'));
    await page.waitForTimeout(300);

    // Push fixture with ES9822PRO disabled
    const modified = JSON.parse(JSON.stringify(HAL_FIXTURE));
    modified.devices[6].cfgEnabled = false; // ES9822PRO
    page.wsRoute.send(modified);
    await expect(page.locator('#hal-device-list')).toContainText('ES9822PRO', { timeout: 5000 });

    let putPayload = null;
    await page.route('/api/hal/devices', async (route) => {
      if (route.request().method() === 'PUT') {
        putPayload = JSON.parse(route.request().postData());
        await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ status: 'ok' }) });
      } else {
        await route.fulfill({ status: 200, contentType: 'application/json', body: '[]' });
      }
    });

    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9822PRO' });
    const toggle = card.locator('.hal-enable-toggle input[type="checkbox"]');
    await expect(toggle).not.toBeChecked();
    // CSS-hidden checkbox — click via the parent label element
    await card.locator('.hal-enable-toggle').click();

    await page.waitForTimeout(500);
    expect(putPayload).not.toBeNull();
    expect(putPayload.slot).toBe(7);
    expect(putPayload.enabled).toBe(true);
  });

  test('state transitions to UNAVAILABLE after WS update', async ({ connectedPage: page }) => {
    await openDevicesTab(page);

    // Push updated state with ES9822PRO UNAVAILABLE
    const modified = JSON.parse(JSON.stringify(HAL_FIXTURE));
    modified.devices[6].state = 4; // UNAVAILABLE
    modified.devices[6].ready = false;
    page.wsRoute.send(modified);

    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9822PRO' });
    await expect(card.locator('.hal-device-info .badge').first()).toHaveText('Unavailable', { timeout: 5000 });
    await expect(card.locator('.status-dot')).toHaveClass(/status-red/);
  });

  test('state transitions to AVAILABLE after WS update', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('devices'));
    await page.waitForTimeout(300);

    // Start with UNAVAILABLE
    const modified = JSON.parse(JSON.stringify(HAL_FIXTURE));
    modified.devices[6].state = 4;
    modified.devices[6].ready = false;
    page.wsRoute.send(modified);

    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9822PRO' });
    await expect(card.locator('.hal-device-info .badge').first()).toHaveText('Unavailable', { timeout: 5000 });

    // Now push AVAILABLE
    const available = JSON.parse(JSON.stringify(HAL_FIXTURE));
    page.wsRoute.send(available);
    await expect(card.locator('.hal-device-info .badge').first()).toHaveText('Available', { timeout: 5000 });
    await expect(card.locator('.status-dot')).toHaveClass(/status-green/);
  });

  test('toggle checkbox reflects enabled state from fixture', async ({ connectedPage: page }) => {
    await openDevicesTab(page);

    // All devices in fixture have cfgEnabled=true
    const card = page.locator('.hal-device-card').filter({ hasText: 'PCM5102A' });
    const toggle = card.locator('.hal-enable-toggle input[type="checkbox"]');
    await expect(toggle).toBeChecked();
  });

  test('disabled device card has unchecked toggle after state push', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('devices'));
    await page.waitForTimeout(300);

    const modified = JSON.parse(JSON.stringify(HAL_FIXTURE));
    modified.devices[0].cfgEnabled = false;
    modified.devices[0].state = 4;
    modified.devices[0].ready = false;
    page.wsRoute.send(modified);

    const deviceList = page.locator('#hal-device-list');
    await expect(deviceList).toContainText('PCM5102A', { timeout: 5000 });

    const card = deviceList.locator('.hal-device-card').filter({ hasText: 'PCM5102A' });
    const toggle = card.locator('.hal-enable-toggle input[type="checkbox"]');
    await expect(toggle).not.toBeChecked();
    await expect(card.locator('.hal-device-info .badge').first()).toHaveText('Unavailable');
  });

});
