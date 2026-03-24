/**
 * hal-adc-controls.spec.js — HAL ADC device controls: ESS Sabre ADC cards,
 * PGA gain, HPF toggle, filter preset dropdown, capability badges.
 *
 * switchTab('devices') triggers a REST fetch that overwrites the WS-pushed
 * halDeviceState with string-typed data (missing numeric type/state/capabilities).
 * To test the full device card rendering, we push a fresh halDeviceState WS
 * message after navigating so the cards re-render with the complete fixture data.
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
  // Wait briefly for the REST fetch to settle, then push WS data to override
  await page.waitForTimeout(300);
  page.wsRoute.send(HAL_FIXTURE);
  // Wait for the device list to be populated with WS-pushed data
  const deviceList = page.locator('#hal-device-list');
  await expect(deviceList).toContainText('ES9822PRO', { timeout: 5000 });
}

/**
 * Navigate to Devices tab, push WS data, then click the Edit button on a
 * specific device card to open its configuration form.
 */
async function openEditForm(page, deviceName) {
  await openDevicesTab(page);
  const card = page.locator('.hal-device-card').filter({ hasText: deviceName });
  await card.locator('button[title="Edit"]').click();
  // Wait for the edit form to render
  await expect(card.locator('.hal-edit-form')).toBeVisible({ timeout: 5000 });
  return card;
}

test.describe('HAL ADC Device Controls', () => {

  test('ESS ADC devices render in device list', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    const deviceList = page.locator('#hal-device-list');
    await expect(deviceList).toContainText('ES9822PRO');
    await expect(deviceList).toContainText('ES9843PRO');
  });

  test('ES9822PRO shows ADC type badge', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9822PRO' });
    await expect(card.locator('.hal-device-info')).toContainText('ADC');
  });

  test('ES9843PRO shows 4 channels in expanded details', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    // Click header to expand the card
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9843PRO' });
    await card.locator('.hal-device-header').click();
    // Channels are shown in the expanded detail rows
    await expect(card.locator('.hal-device-details')).toContainText('4');
  });

  test('ADC device edit form shows PGA gain dropdown', async ({ connectedPage: page }) => {
    await openEditForm(page, 'ES9822PRO');
    await expect(page.locator('#halCfgPgaGain')).toBeVisible();
  });

  test('PGA gain dropdown has 8 options (0-42dB in 6dB steps)', async ({ connectedPage: page }) => {
    await openEditForm(page, 'ES9822PRO');
    const options = page.locator('#halCfgPgaGain option');
    await expect(options).toHaveCount(8);
    // Verify first and last option values
    await expect(options.first()).toContainText('0 dB');
    await expect(options.last()).toContainText('42 dB');
  });

  test('PGA gain shows correct initial selection from device config', async ({ connectedPage: page }) => {
    await openEditForm(page, 'ES9822PRO');
    // ES9822PRO fixture has cfgPgaGain: 6
    const pgaSelect = page.locator('#halCfgPgaGain');
    await expect(pgaSelect).toHaveValue('6');
  });

  test('HPF toggle is visible for ADC devices with HPF capability', async ({ connectedPage: page }) => {
    await openEditForm(page, 'ES9822PRO');
    // HPF checkbox — may be styled hidden, use toBeAttached
    const hpf = page.locator('#halCfgHpfEnabled');
    await expect(hpf).toBeAttached();
  });

  test('HPF toggle reflects initial state from device config', async ({ connectedPage: page }) => {
    await openEditForm(page, 'ES9822PRO');
    // ES9822PRO fixture has cfgHpfEnabled: true
    const hpf = page.locator('#halCfgHpfEnabled');
    await expect(hpf).toBeChecked();
  });

  test('HPF toggle is unchecked when device has hpfEnabled false', async ({ connectedPage: page }) => {
    await openEditForm(page, 'ES9843PRO');
    // ES9843PRO fixture has cfgHpfEnabled: false
    const hpf = page.locator('#halCfgHpfEnabled');
    await expect(hpf).not.toBeChecked();
  });

  test('filter preset dropdown visible for ADC devices with 8 presets', async ({ connectedPage: page }) => {
    await openEditForm(page, 'ES9822PRO');
    const filterSelect = page.locator('#halCfgFilterPreset');
    await expect(filterSelect).toBeVisible();
    const options = filterSelect.locator('option');
    await expect(options).toHaveCount(8);
    // Verify first and last preset names
    await expect(options.first()).toContainText('Minimum Phase');
    await expect(options.last()).toContainText('Minimum Slow Low Dispersion');
  });

  test('filter preset shows correct initial selection from device config', async ({ connectedPage: page }) => {
    await openEditForm(page, 'ES9822PRO');
    // ES9822PRO fixture has cfgFilterMode: 2 (Linear Fast)
    const filterSelect = page.locator('#halCfgFilterPreset');
    await expect(filterSelect).toHaveValue('2');
  });

  test('filter preset defaults to index 0 when cfgFilterMode is 0', async ({ connectedPage: page }) => {
    await openEditForm(page, 'ES9843PRO');
    // ES9843PRO fixture has cfgFilterMode: 0 (Minimum Phase)
    const filterSelect = page.locator('#halCfgFilterPreset');
    await expect(filterSelect).toHaveValue('0');
  });

  test('capability badges show Vol, PGA, HPF for ESS ADC device', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9822PRO' });
    // Capability badges are rendered in .hal-device-info from capabilities bitmask
    const info = card.locator('.hal-device-info');
    await expect(info).toContainText('Vol');
    await expect(info).toContainText('PGA');
    await expect(info).toContainText('HPF');
  });

  test('ESS ADC devices show EEPROM discovery badge', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    // Both ESS devices have discovery: 1 (EEPROM)
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9822PRO' });
    await expect(card.locator('.hal-device-info')).toContainText('EEPROM');
  });

  test('ESS ADC device shows manufacturer in expanded details', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9822PRO' });
    await card.locator('.hal-device-header').click();
    await expect(card.locator('.hal-device-details')).toContainText('ESS Technology');
  });

  test('ESS ADC device shows I2C bus info in expanded details', async ({ connectedPage: page }) => {
    await openDevicesTab(page);
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9822PRO' });
    await card.locator('.hal-device-header').click();
    const details = card.locator('.hal-device-details');
    await expect(details).toContainText('I2C');
    await expect(details).toContainText('400 kHz');
  });

  test('edit form shows volume slider for ADC with HW_VOLUME capability', async ({ connectedPage: page }) => {
    await openEditForm(page, 'ES9822PRO');
    // ES9822PRO has capabilities & HAL_CAP_HW_VOLUME (bit 0)
    const volumeSlider = page.locator('#halCfgVolume');
    await expect(volumeSlider).toBeVisible();
    // ES9822PRO fixture has cfgVolume: 80
    await expect(volumeSlider).toHaveValue('80');
  });

  test('edit form shows Save and Cancel buttons', async ({ connectedPage: page }) => {
    const card = await openEditForm(page, 'ES9822PRO');
    await expect(card.locator('button').filter({ hasText: 'Save' })).toBeVisible();
    await expect(card.locator('button').filter({ hasText: 'Cancel' })).toBeVisible();
  });

});

// ---------------------------------------------------------------------------
// Gap 2+3: Invalid PGA gain and filter mode rejected by mock server PUT
// Gap 4: Volume out of range rejected by mock server PUT
// Gap acceptance: valid values return HTTP 200
//
// These tests exercise the validation added to PUT /api/hal/devices.
// They use the Playwright `request` fixture (direct HTTP) rather than the
// browser page so they run without a WS session.
//
// The mock server halDevices state contains a slot-7 ES9822PRO entry (id: 7)
// so the device-lookup succeeds and validation runs.
// ---------------------------------------------------------------------------

test.describe('ADC config validation — PUT /api/hal/devices', () => {

  test('rejects invalid PGA gain value (50 is not a valid step)', async ({ request }) => {
    const res = await request.put('http://localhost:3000/api/hal/devices', {
      headers: { Cookie: 'sessionId=test-session' },
      data: { slot: 7, cfgPgaGain: 50 },
    });
    expect(res.status()).toBe(400);
    const body = await res.json();
    expect(body.error).toMatch(/cfgPgaGain/);
  });

  test('rejects non-numeric PGA gain value', async ({ request }) => {
    const res = await request.put('http://localhost:3000/api/hal/devices', {
      headers: { Cookie: 'sessionId=test-session' },
      data: { slot: 7, cfgPgaGain: 'bad' },
    });
    expect(res.status()).toBe(400);
  });

  test('accepts valid PGA gain value (12 dB)', async ({ request }) => {
    const res = await request.put('http://localhost:3000/api/hal/devices', {
      headers: { Cookie: 'sessionId=test-session' },
      data: { slot: 7, cfgPgaGain: 12 },
    });
    expect(res.status()).toBe(200);
  });

  test('accepts all valid PGA gain steps (0-42 in 6dB increments)', async ({ request }) => {
    const validSteps = [0, 6, 12, 18, 24, 30, 36, 42];
    for (const gain of validSteps) {
      const res = await request.put('http://localhost:3000/api/hal/devices', {
        headers: { Cookie: 'sessionId=test-session' },
        data: { slot: 7, cfgPgaGain: gain },
      });
      expect(res.status()).toBe(200);
    }
  });

  test('rejects out-of-range filter mode (8 exceeds max of 7)', async ({ request }) => {
    const res = await request.put('http://localhost:3000/api/hal/devices', {
      headers: { Cookie: 'sessionId=test-session' },
      data: { slot: 7, cfgFilterMode: 8 },
    });
    expect(res.status()).toBe(400);
    const body = await res.json();
    expect(body.error).toMatch(/cfgFilterMode/);
  });

  test('rejects negative filter mode (-1)', async ({ request }) => {
    const res = await request.put('http://localhost:3000/api/hal/devices', {
      headers: { Cookie: 'sessionId=test-session' },
      data: { slot: 7, cfgFilterMode: -1 },
    });
    expect(res.status()).toBe(400);
  });

  test('accepts valid filter mode at boundary values (0 and 7)', async ({ request }) => {
    for (const mode of [0, 7]) {
      const res = await request.put('http://localhost:3000/api/hal/devices', {
        headers: { Cookie: 'sessionId=test-session' },
        data: { slot: 7, cfgFilterMode: mode },
      });
      expect(res.status()).toBe(200);
    }
  });

  test('rejects volume above 100', async ({ request }) => {
    const res = await request.put('http://localhost:3000/api/hal/devices', {
      headers: { Cookie: 'sessionId=test-session' },
      data: { slot: 7, cfgVolume: 150 },
    });
    expect(res.status()).toBe(400);
    const body = await res.json();
    expect(body.error).toMatch(/cfgVolume/);
  });

  test('rejects negative volume (-1)', async ({ request }) => {
    const res = await request.put('http://localhost:3000/api/hal/devices', {
      headers: { Cookie: 'sessionId=test-session' },
      data: { slot: 7, cfgVolume: -1 },
    });
    expect(res.status()).toBe(400);
  });

  test('accepts volume at boundary values (0 and 100)', async ({ request }) => {
    for (const vol of [0, 100]) {
      const res = await request.put('http://localhost:3000/api/hal/devices', {
        headers: { Cookie: 'sessionId=test-session' },
        data: { slot: 7, cfgVolume: vol },
      });
      expect(res.status()).toBe(200);
    }
  });

});
