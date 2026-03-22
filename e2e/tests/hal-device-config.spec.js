/**
 * hal-device-config.spec.js — HAL Device Config: edit form rendering,
 * field updates, config save, validation, and cancel behaviour.
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
  await expect(page.locator('#hal-device-list')).toContainText('ES9822PRO', { timeout: 5000 });
}

/**
 * Open the edit form for a device by clicking its Edit button.
 * The card must be in the expanded state for the edit form to render.
 */
async function openEditForm(page, deviceName) {
  await openDevicesTab(page);
  const card = page.locator('.hal-device-card').filter({ hasText: deviceName });
  await card.locator('button[title="Edit"]').click();
  await expect(card.locator('.hal-edit-form')).toBeVisible({ timeout: 5000 });
  return card;
}

test.describe('@hal @api HAL Device Config', () => {

  test('edit form shows current config values', async ({ connectedPage: page }) => {
    const card = await openEditForm(page, 'ES9822PRO');

    // Volume slider should show 80 (from fixture cfgVolume: 80)
    const volumeSlider = page.locator('#halCfgVolume');
    await expect(volumeSlider).toBeVisible();
    await expect(volumeSlider).toHaveValue('80');

    // I2S port should show port 2
    const i2sPort = page.locator('#halCfgI2sPort');
    await expect(i2sPort).toHaveValue('2');

    // Enabled checkbox should be checked
    const enabled = page.locator('#halCfgEnabled');
    await expect(enabled).toBeChecked();
  });

  test('I2S port selector updates', async ({ connectedPage: page }) => {
    await openEditForm(page, 'ES9822PRO');
    const i2sPort = page.locator('#halCfgI2sPort');
    await expect(i2sPort).toHaveValue('2');
    await i2sPort.selectOption('0');
    await expect(i2sPort).toHaveValue('0');
  });

  test('sample rate selector updates', async ({ connectedPage: page }) => {
    await openEditForm(page, 'ES9822PRO');
    const sampleRate = page.locator('#halCfgSampleRate');
    await expect(sampleRate).toBeVisible();
    // Fixture has cfgSampleRate: 48000
    await expect(sampleRate).toHaveValue('48000');
    await sampleRate.selectOption('96000');
    await expect(sampleRate).toHaveValue('96000');
  });

  test('I2C address field shows current value for I2C device', async ({ connectedPage: page }) => {
    await openEditForm(page, 'ES9822PRO');
    // ES9822PRO is I2C bus type (busType=1), so I2C address field should be visible
    const i2cAddr = page.locator('#halCfgI2cAddr');
    await expect(i2cAddr).toBeVisible();
    // i2cAddr=64 (0x40) in fixture
    await expect(i2cAddr).toHaveValue('0x40');
  });

  test('config save sends PUT /api/hal/devices with form data', async ({ connectedPage: page }) => {
    const card = await openEditForm(page, 'ES9822PRO');

    let putPayload = null;
    await page.route('/api/hal/devices', async (route) => {
      if (route.request().method() === 'PUT') {
        putPayload = JSON.parse(route.request().postData());
        await route.fulfill({
          status: 200,
          contentType: 'application/json',
          body: JSON.stringify({ status: 'ok' })
        });
      } else {
        await route.fulfill({ status: 200, contentType: 'application/json', body: '[]' });
      }
    });

    // Click Save
    await card.locator('button').filter({ hasText: 'Save' }).click();
    await page.waitForTimeout(500);

    expect(putPayload).not.toBeNull();
    expect(putPayload.slot).toBe(7);
  });

  test('config validation rejects invalid PGA gain via PUT', async ({ request }) => {
    const res = await request.put('http://localhost:3000/api/hal/devices', {
      data: { slot: 7, cfgPgaGain: 50 }
    });
    expect(res.status()).toBe(400);
    const body = await res.json();
    expect(body.error).toMatch(/cfgPgaGain/);
  });

  test('cancel edit restores original values (closes edit form)', async ({ connectedPage: page }) => {
    const card = await openEditForm(page, 'ES9822PRO');

    // Change volume value
    const volumeSlider = page.locator('#halCfgVolume');
    await volumeSlider.fill('50');
    await expect(volumeSlider).toHaveValue('50');

    // Click Cancel
    await card.locator('button').filter({ hasText: 'Cancel' }).click();

    // Edit form should be gone
    await expect(card.locator('.hal-edit-form')).not.toBeVisible({ timeout: 3000 });
  });

  test('form shows device capabilities — PGA and HPF for ADC', async ({ connectedPage: page }) => {
    await openEditForm(page, 'ES9822PRO');

    // ES9822PRO has PGA_CONTROL (bit 5) and HPF_CONTROL (bit 6)
    await expect(page.locator('#halCfgPgaGain')).toBeVisible();
    await expect(page.locator('#halCfgHpfEnabled')).toBeAttached();
    await expect(page.locator('#halCfgFilterPreset')).toBeVisible();
  });

});
