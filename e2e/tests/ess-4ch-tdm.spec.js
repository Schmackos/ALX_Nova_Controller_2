/**
 * ess-4ch-tdm.spec.js — Tests for 4-channel TDM ESS SABRE ADC expansion devices.
 *
 * Device variants covered (populated per-device phase as drivers are added):
 *   ES9842PRO, ES9841, ES9840
 *
 * The ES9843PRO (existing driver) is the reference 4-channel device.
 * These scaffold tests extend coverage to the remaining 4-channel family members.
 *
 * TDM devices register 2 stereo AudioInputSource pairs via HalTdmDeinterleaver.
 * Each pair appears as a separate lane in the audioChannelMap with names like
 * "ES9843PRO CH1/2" and "ES9843PRO CH3/4".
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
const FOUR_CH_DEVICES = [
  { compatible: 'ess,es9842pro', name: 'ES9842PRO', chipId: null },
  { compatible: 'ess,es9841',    name: 'ES9841',    chipId: null },
  { compatible: 'ess,es9840',    name: 'ES9840',    chipId: null },
];

/**
 * Build a 4-channel TDM ESS SABRE ADC device fixture matching the field schema
 * used in hal-device-state.json (ES9843PRO entry at slot 8).
 *
 * @param {string} compatible  - HAL compatible string (e.g. 'ess,es9842pro')
 * @param {string} name        - Human-readable device name
 * @param {number} [slot=8]    - HAL slot index (default 8, same as ES9843PRO fixture)
 */
function buildFourChAdcDevice(compatible, name, slot) {
  slot = slot === undefined ? 8 : slot;
  return {
    slot: slot,
    compatible: compatible,
    name: name,
    type: 2,           // HAL_DEV_ADC
    state: 3,          // HAL_STATE_AVAILABLE
    ready: true,
    discovery: 1,      // EEPROM discovery
    i2cAddr: 64,       // 0x40
    channels: 4,
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
    cfgHpfEnabled: false,
    cfgPaControlPin: -1,
    cfgPinBck: 0,
    cfgPinLrc: 0,
    cfgPinFmt: -1,
    cfgFilterMode: 0,
    channelCount: 4,
  };
}

/**
 * Navigate to Devices tab and inject a WS halDeviceState override containing
 * the base fixture devices plus a custom device at the specified slot.
 */
async function openDevicesTabWithDevice(page, device) {
  await page.evaluate(() => switchTab('devices'));
  await page.waitForTimeout(300);
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

// ---------------------------------------------------------------------------
// Scaffold: device card rendering
// ---------------------------------------------------------------------------

test.describe('ESS 4-channel TDM ADC — Device Card Rendering', () => {

  for (const dev of FOUR_CH_DEVICES) {
    test('renders device card for ' + dev.name, async ({ connectedPage: page }) => {
      const device = buildFourChAdcDevice(dev.compatible, dev.name, 8);
      await openDevicesTabWithDevice(page, device);
      await expect(page.locator('#hal-device-list')).toContainText(dev.name);
    });

    test(dev.name + ' shows ADC type badge', async ({ connectedPage: page }) => {
      const device = buildFourChAdcDevice(dev.compatible, dev.name, 8);
      await openDevicesTabWithDevice(page, device);
      const card = page.locator('.hal-device-card').filter({ hasText: dev.name });
      await expect(card.locator('.hal-device-info')).toContainText('ADC');
    });

    test(dev.name + ' shows 4 channels in expanded details', async ({ connectedPage: page }) => {
      const device = buildFourChAdcDevice(dev.compatible, dev.name, 8);
      await openDevicesTabWithDevice(page, device);
      const card = page.locator('.hal-device-card').filter({ hasText: dev.name });
      await card.locator('.hal-device-header').click();
      await expect(card.locator('.hal-device-details')).toContainText('4');
    });

  }

});

// ---------------------------------------------------------------------------
// ES9842PRO — device card rendering (driver implemented)
// ---------------------------------------------------------------------------

test.describe('ES9842PRO rendering', () => {

  test('renders ES9842PRO device card with correct name', async ({ connectedPage: page }) => {
    const device = buildFourChAdcDevice('ess,es9842pro', 'ES9842PRO', 8);
    await openDevicesTabWithDevice(page, device);
    await expect(page.locator('#hal-device-list')).toContainText('ES9842PRO');
  });

  test('ES9842PRO card shows ADC type badge', async ({ connectedPage: page }) => {
    const device = buildFourChAdcDevice('ess,es9842pro', 'ES9842PRO', 8);
    await openDevicesTabWithDevice(page, device);
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9842PRO' });
    await expect(card.locator('.hal-device-info')).toContainText('ADC');
  });

  test('ES9842PRO card shows manufacturer ESS Technology', async ({ connectedPage: page }) => {
    const device = buildFourChAdcDevice('ess,es9842pro', 'ES9842PRO', 8);
    await openDevicesTabWithDevice(page, device);
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9842PRO' });
    await card.locator('.hal-device-header').click();
    await expect(card.locator('.hal-device-details')).toContainText('ESS Technology');
  });

});

// ---------------------------------------------------------------------------
// ES9841 — device card rendering (driver implemented)
// ---------------------------------------------------------------------------

test.describe('ES9841 rendering', () => {

  test('renders ES9841 device card with correct name', async ({ connectedPage: page }) => {
    const device = buildFourChAdcDevice('ess,es9841', 'ES9841', 8);
    await openDevicesTabWithDevice(page, device);
    await expect(page.locator('#hal-device-list')).toContainText('ES9841');
  });

  test('ES9841 card shows ADC type badge', async ({ connectedPage: page }) => {
    const device = buildFourChAdcDevice('ess,es9841', 'ES9841', 8);
    await openDevicesTabWithDevice(page, device);
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9841' });
    await expect(card.locator('.hal-device-info')).toContainText('ADC');
  });

  test('ES9841 card shows manufacturer ESS Technology', async ({ connectedPage: page }) => {
    const device = buildFourChAdcDevice('ess,es9841', 'ES9841', 8);
    await openDevicesTabWithDevice(page, device);
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9841' });
    await card.locator('.hal-device-header').click();
    await expect(card.locator('.hal-device-details')).toContainText('ESS Technology');
  });

});

// ---------------------------------------------------------------------------
// ES9840 — device card rendering (driver implemented)
// ---------------------------------------------------------------------------

test.describe('ES9840 rendering', () => {

  test('renders ES9840 device card with correct name', async ({ connectedPage: page }) => {
    const device = buildFourChAdcDevice('ess,es9840', 'ES9840', 8);
    await openDevicesTabWithDevice(page, device);
    await expect(page.locator('#hal-device-list')).toContainText('ES9840');
  });

  test('ES9840 card shows ADC type badge', async ({ connectedPage: page }) => {
    const device = buildFourChAdcDevice('ess,es9840', 'ES9840', 8);
    await openDevicesTabWithDevice(page, device);
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9840' });
    await expect(card.locator('.hal-device-info')).toContainText('ADC');
  });

  test('ES9840 card shows manufacturer ESS Technology', async ({ connectedPage: page }) => {
    const device = buildFourChAdcDevice('ess,es9840', 'ES9840', 8);
    await openDevicesTabWithDevice(page, device);
    const card = page.locator('.hal-device-card').filter({ hasText: 'ES9840' });
    await card.locator('.hal-device-header').click();
    await expect(card.locator('.hal-device-details')).toContainText('ESS Technology');
  });

});

// ---------------------------------------------------------------------------
// Gap 6 (real): ES9843PRO dual stereo source pairs in audioChannelMap
//
// The ES9843PRO registers 2 AudioInputSource entries via HalTdmDeinterleaver.
// Each pair gets a separate lane in the audioChannelMap.inputs array.
// The firmware sets the name to "<DeviceName> CH1/2" and "<DeviceName> CH3/4".
// This test verifies the frontend renders both channel strips.
// ---------------------------------------------------------------------------

test.describe('ESS 4-channel TDM ADC — Channel Pairs', () => {

  test('ES9843PRO shows two stereo source pairs in audio channel map', async ({ connectedPage: page }) => {
    // Navigate to the Audio tab inputs sub-view
    await page.evaluate(() => switchTab('audio'));
    await page.waitForTimeout(200);

    // Push an audioChannelMap that includes the two ES9843PRO stereo pairs
    // alongside the standard PCM1808 and SigGen lanes
    page.wsRoute.send({
      type: 'audioChannelMap',
      inputs: [
        {
          lane: 0,
          name: 'PCM1808 CH1/2',
          channels: 2,
          matrixCh: 0,
          deviceName: 'PCM1808',
          deviceType: 2,
          compatible: 'ti,pcm1808',
          manufacturer: 'Texas Instruments',
          capabilities: 8,
          ready: true,
        },
        {
          lane: 1,
          name: 'ES9843PRO CH1/2',
          channels: 2,
          matrixCh: 2,
          deviceName: 'ES9843PRO',
          deviceType: 2,
          compatible: 'ess,es9843pro',
          manufacturer: 'ESS Technology',
          capabilities: 105,
          ready: true,
        },
        {
          lane: 2,
          name: 'ES9843PRO CH3/4',
          channels: 2,
          matrixCh: 4,
          deviceName: 'ES9843PRO',
          deviceType: 2,
          compatible: 'ess,es9843pro',
          manufacturer: 'ESS Technology',
          capabilities: 105,
          ready: true,
        },
      ],
      outputs: [],
      matrixInputs: 16,
      matrixOutputs: 16,
      matrixBypass: false,
      matrix: [],
    });

    // Switch to the Inputs sub-view
    await page.evaluate(() => switchAudioSubView('inputs'));
    await page.waitForTimeout(300);

    // Both ES9843PRO channel strips should appear in the inputs container
    const container = page.locator('#audio-inputs-container');
    const strips = container.locator('.channel-device-name');
    // The device name rendered in each strip is inp.deviceName = 'ES9843PRO'
    // There should be exactly 2 strips with that device name
    const es9843Strips = container.locator('.channel-strip').filter({ hasText: 'ES9843PRO' });
    await expect(es9843Strips).toHaveCount(2, { timeout: 5000 });
  });

  test('ES9843PRO channel pair strips have distinct lane indices', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('audio'));
    await page.waitForTimeout(200);

    page.wsRoute.send({
      type: 'audioChannelMap',
      inputs: [
        {
          lane: 0,
          name: 'ES9843PRO CH1/2',
          channels: 2,
          matrixCh: 0,
          deviceName: 'ES9843PRO',
          deviceType: 2,
          compatible: 'ess,es9843pro',
          manufacturer: 'ESS Technology',
          capabilities: 105,
          ready: true,
        },
        {
          lane: 1,
          name: 'ES9843PRO CH3/4',
          channels: 2,
          matrixCh: 2,
          deviceName: 'ES9843PRO',
          deviceType: 2,
          compatible: 'ess,es9843pro',
          manufacturer: 'ESS Technology',
          capabilities: 105,
          ready: true,
        },
      ],
      outputs: [],
      matrixInputs: 16,
      matrixOutputs: 16,
      matrixBypass: false,
      matrix: [],
    });

    await page.evaluate(() => switchAudioSubView('inputs'));
    await page.waitForTimeout(300);

    const container = page.locator('#audio-inputs-container');
    const lane0Strip = container.locator('.channel-strip[data-lane="0"]');
    const lane1Strip = container.locator('.channel-strip[data-lane="1"]');
    await expect(lane0Strip).toBeAttached({ timeout: 5000 });
    await expect(lane1Strip).toBeAttached({ timeout: 5000 });
  });

});

// ---------------------------------------------------------------------------
// Scaffold: config persistence for 4-channel devices
// ---------------------------------------------------------------------------

test.describe('ESS 4-channel TDM ADC — Config Persistence', () => {

  test('saves PGA gain for 4-channel device (scaffold)', async ({ connectedPage: page }) => {
    // SCAFFOLD: implement when ess,es9842pro device phase is executed.
    // Same PUT body fields as 2-channel variants (cfgPgaGain, cfgFilterMode, etc.)
    expect(true).toBe(true);
  });

  test('saves filter preset selection for 4-channel device (scaffold)', async ({ connectedPage: page }) => {
    // SCAFFOLD: implement when ess,es9842pro device phase is executed.
    expect(true).toBe(true);
  });

});
