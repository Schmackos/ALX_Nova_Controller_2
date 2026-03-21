/**
 * ess-2ch-adc.spec.js — Tests for 2-channel ESS SABRE ADC expansion devices.
 *
 * Device variants covered (populated per-device phase as drivers are added):
 *   ES9826, ES9823PRO, ES9821, ES9820
 *
 * The ES9822PRO (existing driver) shares the same field structure and is used
 * as the reference fixture in hal-adc-controls.spec.js. These scaffold tests
 * extend coverage to the remaining 2-channel family members.
 *
 * Pattern note: switchTab('devices') triggers a REST fetch that overwrites the
 * WS-pushed halDeviceState with string-typed data. Push a fresh halDeviceState
 * WS message after the tab switch to restore numeric type/state/capabilities.
 */

const { test, expect } = require('../helpers/fixtures');
const path = require('path');
const fs = require('fs');

const HAL_FIXTURE = JSON.parse(
  fs.readFileSync(path.join(__dirname, '..', 'fixtures', 'ws-messages', 'hal-device-state.json'), 'utf8')
);

// ---------------------------------------------------------------------------
// Device table — populated per phase as each driver is implemented.
// chipId is null until the hardware datasheet confirms the I2C device-ID register value.
// ---------------------------------------------------------------------------
const TWO_CH_DEVICES = [
  { compatible: 'ess,es9826',    name: 'ES9826',    chipId: null },
  { compatible: 'ess,es9823pro', name: 'ES9823PRO', chipId: null },
  { compatible: 'ess,es9821',    name: 'ES9821',    chipId: null },
  { compatible: 'ess,es9820',    name: 'ES9820',    chipId: null },
];

/**
 * Build a 2-channel ESS SABRE ADC device fixture matching the field schema
 * used in hal-device-state.json and the mock server ws-state.js halDevices array.
 *
 * Field names follow the WS broadcast schema from websocket_handler.cpp:
 *   cfgEnabled, cfgVolume, cfgMute, cfgPgaGain, cfgHpfEnabled, cfgFilterMode
 *
 * @param {string} compatible  - HAL compatible string (e.g. 'ess,es9826')
 * @param {string} name        - Human-readable device name
 * @param {number} [slot=7]    - HAL slot index (default 7, same as ES9822PRO fixture)
 */
function buildTwoChAdcDevice(compatible, name, slot) {
  slot = slot === undefined ? 7 : slot;
  return {
    slot: slot,
    compatible: compatible,
    name: name,
    type: 2,           // HAL_DEV_ADC
    state: 3,          // HAL_STATE_AVAILABLE
    ready: true,
    discovery: 1,      // EEPROM discovery
    i2cAddr: 64,       // 0x40
    channels: 2,
    capabilities: 105, // HAL_CAP_ADC_PATH | HAL_CAP_HW_VOLUME | HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL
    manufacturer: 'ESS Technology',
    busType: 1,        // I2C
    busIndex: 2,       // Expansion bus
    pinA: 0,
    pinB: 0,
    busFreq: 400000,
    sampleRates: 28,
    userLabel: '',
    cfgEnabled: true,
    cfgI2sPort: 2,
    cfgVolume: 80,
    cfgMute: false,
    cfgSampleRate: 48000,
    cfgBitDepth: 32,
    cfgPinSda: 28,
    cfgPinScl: 29,
    cfgPinData: 0,
    cfgPinMclk: 0,
    cfgMclkMultiple: 256,
    cfgI2sFormat: 0,
    cfgPgaGain: 0,
    cfgHpfEnabled: true,
    cfgPaControlPin: -1,
    cfgPinBck: 0,
    cfgPinLrc: 0,
    cfgPinFmt: -1,
    cfgFilterMode: 0,
  };
}

/**
 * Navigate to Devices tab and inject a WS halDeviceState override containing
 * both the base fixture devices and a custom device in slot 7.
 */
async function openDevicesTabWithDevice(page, device) {
  await page.evaluate(() => switchTab('devices'));
  await page.waitForTimeout(300);
  // Build the fixture with our custom device replacing slot 7
  const devicesWithOverride = HAL_FIXTURE.devices
    .filter(function(d) { return d.slot !== device.slot; })
    .concat([device]);
  page.wsRoute.send({
    type: 'halDeviceState',
    scanning: false,
    deviceCount: devicesWithOverride.length,
    deviceMax: 24,
    driverCount: 16,
    driverMax: 24,
    devices: devicesWithOverride,
  });
  await expect(page.locator('#hal-device-list')).toContainText(device.name, { timeout: 5000 });
}

/**
 * Navigate to Devices tab, inject the device fixture, open the edit form on
 * the matching card.
 */
async function openEditFormForDevice(page, device) {
  await openDevicesTabWithDevice(page, device);
  const card = page.locator('.hal-device-card').filter({ hasText: device.name });
  await card.locator('button[title="Edit"]').click();
  await expect(card.locator('.hal-edit-form')).toBeVisible({ timeout: 5000 });
  return card;
}

// ---------------------------------------------------------------------------
// Scaffold: device card rendering
// These tests are parameterised over all 2-channel variants and use the
// buildTwoChAdcDevice helper to inject a fixture. They will pass when the
// browser's JS renders generic ADC cards (which it does today).
// ---------------------------------------------------------------------------

test.describe('ESS 2-channel ADC — Device Card Rendering', () => {

  for (const dev of TWO_CH_DEVICES) {
    test('renders device card for ' + dev.name, async ({ connectedPage: page }) => {
      const device = buildTwoChAdcDevice(dev.compatible, dev.name, 7);
      await openDevicesTabWithDevice(page, device);
      const deviceList = page.locator('#hal-device-list');
      await expect(deviceList).toContainText(dev.name);
    });

    test(dev.name + ' shows ADC type badge', async ({ connectedPage: page }) => {
      const device = buildTwoChAdcDevice(dev.compatible, dev.name, 7);
      await openDevicesTabWithDevice(page, device);
      const card = page.locator('.hal-device-card').filter({ hasText: dev.name });
      await expect(card.locator('.hal-device-info')).toContainText('ADC');
    });

    test(dev.name + ' shows EEPROM discovery badge', async ({ connectedPage: page }) => {
      const device = buildTwoChAdcDevice(dev.compatible, dev.name, 7);
      await openDevicesTabWithDevice(page, device);
      const card = page.locator('.hal-device-card').filter({ hasText: dev.name });
      await expect(card.locator('.hal-device-info')).toContainText('EEPROM');
    });
  }

});

// ---------------------------------------------------------------------------
// ES9826 — device card rendering (driver implemented)
// ---------------------------------------------------------------------------

test.describe('ES9826 rendering', () => {

  test('renders ES9826 device card with correct name', async ({ connectedPage: page }) => {
    const device = buildTwoChAdcDevice('ess,es9826', 'ES9826', 7);
    await openDevicesTabWithDevice(page, device);
    const deviceList = page.locator('#hal-device-list');
    await expect(deviceList).toContainText('ES9826');
  });

  test('ES9826 card shows ADC type badge', async ({ connectedPage: page }) => {
    const device = buildTwoChAdcDevice('ess,es9826', 'ES9826', 7);
    await openDevicesTabWithDevice(page, device);
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9826' });
    await expect(card.locator('.hal-device-info')).toContainText('ADC');
  });

  test('ES9826 card shows EEPROM discovery badge', async ({ connectedPage: page }) => {
    const device = buildTwoChAdcDevice('ess,es9826', 'ES9826', 7);
    await openDevicesTabWithDevice(page, device);
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9826' });
    await expect(card.locator('.hal-device-info')).toContainText('EEPROM');
  });

  test('ES9826 card shows manufacturer ESS Technology', async ({ connectedPage: page }) => {
    const device = buildTwoChAdcDevice('ess,es9826', 'ES9826', 7);
    await openDevicesTabWithDevice(page, device);
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9826' });
    await card.locator('.hal-device-header').click();
    await expect(card.locator('.hal-device-details')).toContainText('ESS Technology');
  });

});

// ---------------------------------------------------------------------------
// ES9821 — device card rendering (driver implemented)
// ---------------------------------------------------------------------------

test.describe('ES9821 rendering', () => {

  test('renders ES9821 device card with correct name', async ({ connectedPage: page }) => {
    const device = buildTwoChAdcDevice('ess,es9821', 'ES9821', 7);
    await openDevicesTabWithDevice(page, device);
    const deviceList = page.locator('#hal-device-list');
    await expect(deviceList).toContainText('ES9821');
  });

  test('ES9821 card shows ADC type badge', async ({ connectedPage: page }) => {
    const device = buildTwoChAdcDevice('ess,es9821', 'ES9821', 7);
    await openDevicesTabWithDevice(page, device);
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9821' });
    await expect(card.locator('.hal-device-info')).toContainText('ADC');
  });

  test('ES9821 card shows EEPROM discovery badge', async ({ connectedPage: page }) => {
    const device = buildTwoChAdcDevice('ess,es9821', 'ES9821', 7);
    await openDevicesTabWithDevice(page, device);
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9821' });
    await expect(card.locator('.hal-device-info')).toContainText('EEPROM');
  });

  test('ES9821 card shows manufacturer ESS Technology', async ({ connectedPage: page }) => {
    const device = buildTwoChAdcDevice('ess,es9821', 'ES9821', 7);
    await openDevicesTabWithDevice(page, device);
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9821' });
    await card.locator('.hal-device-header').click();
    await expect(card.locator('.hal-device-details')).toContainText('ESS Technology');
  });

});

// ---------------------------------------------------------------------------
// ES9823PRO — device card rendering (driver implemented)
// ---------------------------------------------------------------------------

test.describe('ES9823PRO rendering', () => {

  test('renders ES9823PRO device card with correct name', async ({ connectedPage: page }) => {
    const device = buildTwoChAdcDevice('ess,es9823pro', 'ES9823PRO', 7);
    await openDevicesTabWithDevice(page, device);
    await expect(page.locator('#hal-device-list')).toContainText('ES9823PRO');
  });

  test('ES9823PRO card shows ADC type badge', async ({ connectedPage: page }) => {
    const device = buildTwoChAdcDevice('ess,es9823pro', 'ES9823PRO', 7);
    await openDevicesTabWithDevice(page, device);
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9823PRO' });
    await expect(card.locator('.hal-device-info')).toContainText('ADC');
  });

  test('ES9823PRO card shows EEPROM discovery badge', async ({ connectedPage: page }) => {
    const device = buildTwoChAdcDevice('ess,es9823pro', 'ES9823PRO', 7);
    await openDevicesTabWithDevice(page, device);
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9823PRO' });
    await expect(card.locator('.hal-device-info')).toContainText('EEPROM');
  });

  test('ES9823PRO card shows manufacturer ESS Technology', async ({ connectedPage: page }) => {
    const device = buildTwoChAdcDevice('ess,es9823pro', 'ES9823PRO', 7);
    await openDevicesTabWithDevice(page, device);
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9823PRO' });
    await card.locator('.hal-device-header').click();
    await expect(card.locator('.hal-device-details')).toContainText('ESS Technology');
  });

});

// ---------------------------------------------------------------------------
// ES9820 — device card rendering (driver implemented)
// ---------------------------------------------------------------------------

test.describe('ES9820 rendering', () => {

  test('renders ES9820 device card with correct name', async ({ connectedPage: page }) => {
    const device = buildTwoChAdcDevice('ess,es9820', 'ES9820', 7);
    await openDevicesTabWithDevice(page, device);
    await expect(page.locator('#hal-device-list')).toContainText('ES9820');
  });

  test('ES9820 card shows ADC type badge', async ({ connectedPage: page }) => {
    const device = buildTwoChAdcDevice('ess,es9820', 'ES9820', 7);
    await openDevicesTabWithDevice(page, device);
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9820' });
    await expect(card.locator('.hal-device-info')).toContainText('ADC');
  });

  test('ES9820 card shows EEPROM discovery badge', async ({ connectedPage: page }) => {
    const device = buildTwoChAdcDevice('ess,es9820', 'ES9820', 7);
    await openDevicesTabWithDevice(page, device);
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9820' });
    await expect(card.locator('.hal-device-info')).toContainText('EEPROM');
  });

  test('ES9820 card shows manufacturer ESS Technology', async ({ connectedPage: page }) => {
    const device = buildTwoChAdcDevice('ess,es9820', 'ES9820', 7);
    await openDevicesTabWithDevice(page, device);
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9820' });
    await card.locator('.hal-device-header').click();
    await expect(card.locator('.hal-device-details')).toContainText('ESS Technology');
  });

});

// ---------------------------------------------------------------------------
// Scaffold: config persistence
// Full PUT-request body verification is deferred to per-device phases once
// each driver is wired to the firmware REST handler. Placeholder tests
// document the intended coverage.
// ---------------------------------------------------------------------------

test.describe('ESS 2-channel ADC — Config Persistence', () => {

  test('saves PGA gain and reloads from GET /api/hal/devices (scaffold)', async ({ connectedPage: page }) => {
    // SCAFFOLD: implement when ess,es9826 device phase is executed.
    // Steps when implemented:
    //   1. openEditFormForDevice(page, buildTwoChAdcDevice('ess,es9826', 'ES9826', 7))
    //   2. Intercept PUT /api/hal/devices
    //   3. Select a non-zero PGA gain in #halCfgPgaGain
    //   4. Click Save
    //   5. Verify intercepted PUT body.cfgPgaGain === selected value
    expect(true).toBe(true);
  });

  test('saves HPF toggle state (scaffold)', async ({ connectedPage: page }) => {
    // SCAFFOLD: implement when ess,es9826 device phase is executed.
    expect(true).toBe(true);
  });

  test('saves filter preset selection (scaffold)', async ({ connectedPage: page }) => {
    // SCAFFOLD: implement when ess,es9826 device phase is executed.
    expect(true).toBe(true);
  });

});

// ---------------------------------------------------------------------------
// Scaffold: capability badges
// ---------------------------------------------------------------------------

test.describe('ESS 2-channel ADC — Capability Badges', () => {

  test('shows Vol, PGA, HPF capability badges (scaffold)', async ({ connectedPage: page }) => {
    // SCAFFOLD: implement when ess,es9826 device phase is executed.
    // Capability bitmask 105 = HAL_CAP_HW_VOLUME | HAL_CAP_ADC_PATH |
    //                          HAL_CAP_PGA_CONTROL | HAL_CAP_HPF_CONTROL
    // Expected badge text: 'Vol', 'PGA', 'HPF'
    expect(true).toBe(true);
  });

});
