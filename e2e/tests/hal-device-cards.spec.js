/**
 * hal-device-cards.spec.js — HAL Device Cards: rendering, state indicators,
 * capacity display, sorting, and empty state.
 */

const { test, expect } = require('../helpers/fixtures');
const path = require('path');
const fs = require('fs');

const HAL_FIXTURE = JSON.parse(
  fs.readFileSync(path.join(__dirname, '..', 'fixtures', 'ws-messages', 'hal-device-state.json'), 'utf8')
);

/**
 * Navigate to the Devices tab and push a fresh halDeviceState WS message
 * so the cards render with numeric type/state/capabilities from the fixture.
 */
async function openDevicesTab(page) {
  await page.evaluate(() => switchTab('devices'));
  await page.waitForTimeout(300);
  page.wsRoute.send(HAL_FIXTURE);
  const deviceList = page.locator('#hal-device-list');
  await expect(deviceList).not.toContainText('No HAL devices registered', { timeout: 5000 });
}

test.describe('@hal @smoke HAL Device Cards', () => {

  test('device list renders with correct count', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    const cards = page.locator('.hal-device-card');
    // The fixture has 11 devices
    await expect(cards).toHaveCount(11, { timeout: 5000 });
  });

  test('each device shows name, compatible string badge, and state label', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    const deviceList = page.locator('#hal-device-list');

    // Verify known device names from fixture
    await expect(deviceList).toContainText('PCM5102A');
    await expect(deviceList).toContainText('ES8311');
    await expect(deviceList).toContainText('PCM1808');
    await expect(deviceList).toContainText('NS4150B');
    await expect(deviceList).toContainText('ES9822PRO');
    await expect(deviceList).toContainText('ES9843PRO');

    // Each card should show a name and state badge
    const firstCard = page.locator('.hal-device-card').first();
    await expect(firstCard.locator('.hal-device-name')).toBeVisible();
    await expect(firstCard.locator('.hal-device-info .badge').first()).toBeVisible();
  });

  test('status dots have correct color for AVAILABLE state (green)', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    // All fixture devices are state=3 (Available), which maps to green
    const card = page.locator('.hal-device-card').filter({ hasText: 'PCM5102A' });
    const dot = card.locator('.status-dot');
    await expect(dot).toBeVisible();
    await expect(dot).toHaveClass(/status-green/);
  });

  test('status dot uses amber for CONFIGURING and red for ERROR state', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('devices'));
    await page.waitForTimeout(300);

    // Push fixture with one device CONFIGURING (state=2) and one ERROR (state=5)
    const modified = JSON.parse(JSON.stringify(HAL_FIXTURE));
    modified.devices[0].state = 2; // PCM5102A → Configuring
    modified.devices[1].state = 5; // ES8311 → Error
    modified.devices[1].errorReason = 'I2C timeout';
    page.wsRoute.send(modified);

    const deviceList = page.locator('#hal-device-list');
    await expect(deviceList).toContainText('PCM5102A', { timeout: 5000 });

    // Check Configuring = amber
    const pcmCard = deviceList.locator('.hal-device-card').filter({ hasText: 'PCM5102A' });
    await expect(pcmCard.locator('.status-dot')).toHaveClass(/status-amber/);
    await expect(pcmCard.locator('.hal-device-info .badge').first()).toHaveText('Configuring');

    // Check Error = red
    const esCard = deviceList.locator('.hal-device-card').filter({ hasText: 'ES8311' });
    await expect(esCard.locator('.status-dot')).toHaveClass(/status-red/);
    await expect(esCard.locator('.hal-device-info .badge').first()).toHaveText('Error');
  });

  test('capacity indicator shows device/max count', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    const indicator = page.locator('#hal-capacity-indicator');
    await expect(indicator).toContainText('Devices: 16/24');
  });

  test('devices are sorted by slot (ascending order in DOM)', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    // Expand first and last cards to verify slot ordering
    const cards = page.locator('.hal-device-card');
    // First card should be slot 0 (PCM5102A DAC)
    const firstName = await cards.first().locator('.hal-device-name').textContent();
    expect(firstName).toContain('PCM5102A');
    // Last card should be slot 11 (ES9033Q)
    const lastName = await cards.last().locator('.hal-device-name').textContent();
    expect(lastName).toContain('ES9033Q');
  });

  test('disabled device shows unchecked enable toggle', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('devices'));
    await page.waitForTimeout(300);

    const modified = JSON.parse(JSON.stringify(HAL_FIXTURE));
    modified.devices[0].cfgEnabled = false; // PCM5102A disabled
    page.wsRoute.send(modified);

    const deviceList = page.locator('#hal-device-list');
    await expect(deviceList).toContainText('PCM5102A', { timeout: 5000 });

    const card = deviceList.locator('.hal-device-card').filter({ hasText: 'PCM5102A' });
    const toggle = card.locator('.hal-enable-toggle input[type="checkbox"]');
    await expect(toggle).not.toBeChecked();
  });

  test('device card expands on click to show details', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9822PRO' });

    // Click header to expand
    await card.locator('.hal-device-header').click();
    await expect(card.locator('.hal-device-details')).toBeVisible({ timeout: 5000 });

    // Should show compatible string and manufacturer in details
    await expect(card.locator('.hal-device-details')).toContainText('ess,es9822pro');
    await expect(card.locator('.hal-device-details')).toContainText('ESS Technology');
  });

  test('empty state when no devices', async ({ connectedPage: page }) => {
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
  });

});
