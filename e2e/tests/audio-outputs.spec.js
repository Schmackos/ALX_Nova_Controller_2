/**
 * audio-outputs.spec.js — Output channel strips populated from audioChannelMap.
 */

const { test, expect } = require('../helpers/fixtures');

test('output channel strips render with HAL device names after audioChannelMap', async ({ connectedPage: page }) => {
  await page.locator('.sidebar-item[data-tab="audio"]').click();
  await page.locator('.audio-subnav-btn[data-view="outputs"]').click();

  await expect(page.locator('#audio-sv-outputs')).toHaveClass(/active/);

  const container = page.locator('#audio-outputs-container');

  // Placeholder must be replaced after fixture is applied
  await expect(container).not.toContainText('Waiting for device data...', { timeout: 5000 });

  // Known output names from audioChannelMap.json fixture
  await expect(container).toContainText('PCM5102A', { timeout: 3000 });
});

test.describe('@audio Phase 3 — Device Grouping and Capability Badges (Outputs)', () => {
  test.beforeEach(async ({ connectedPage: page }) => {
    await page.locator('.sidebar-item[data-tab="audio"]').click();
    await page.locator('.audio-subnav-btn[data-view="outputs"]').click();
    await expect(page.locator('#audio-sv-outputs')).toHaveClass(/active/);
    await expect(page.locator('#audio-outputs-container')).not.toContainText('Waiting for device data...', { timeout: 5000 });
  });

  test('outputs are grouped by device in .device-group containers', async ({ connectedPage: page }) => {
    // Fixture has PCM5102A (index 0) and ES8311 (index 1) as separate output devices
    const groups = page.locator('#audio-outputs-container .device-group');
    await expect(groups).toHaveCount({ minimum: 1 });
  });

  test('empty state shown when audioChannelMap has empty outputs array', async ({ connectedPage: page }) => {
    await page.wsRoute.send({
      type: 'audioChannelMap',
      inputs: [],
      outputs: [],
      matrixInputs: 16,
      matrixOutputs: 16,
      matrixBypass: false,
      matrix: []
    });

    const container = page.locator('#audio-outputs-container');
    await expect(container.locator('.empty-state')).toBeVisible({ timeout: 3000 });
  });

  test('DSD badge visible when output capabilities include bit 11 (HAL_CAP_DSD=2048)', async ({ connectedPage: page }) => {
    // Push an audioChannelMap with a DSD-capable output (bit 11 = 2048)
    await page.wsRoute.send({
      type: 'audioChannelMap',
      inputs: [],
      outputs: [
        {
          index: 0,
          halSlot: 2,
          name: 'ESS DAC',
          firstChannel: 0,
          channels: 2,
          muted: false,
          compatible: 'ess,es9038q2m',
          manufacturer: 'ESS Technology',
          capabilities: 2048,
          ready: true,
          deviceType: 1,
          i2cAddr: 0
        }
      ],
      matrixInputs: 16,
      matrixOutputs: 16,
      matrixBypass: false,
      matrix: []
    });

    await expect(page.locator('#audio-outputs-container .badge-dsd')).toBeVisible({ timeout: 3000 });
  });

  test('DPLL badge visible when output capabilities include bit 15 (HAL_CAP_DPLL=32768)', async ({ connectedPage: page }) => {
    await page.wsRoute.send({
      type: 'audioChannelMap',
      inputs: [],
      outputs: [
        {
          index: 0,
          halSlot: 2,
          name: 'ESS DAC',
          firstChannel: 0,
          channels: 2,
          muted: false,
          compatible: 'ess,es9038q2m',
          manufacturer: 'ESS Technology',
          capabilities: 32768,
          ready: true,
          deviceType: 1,
          i2cAddr: 0
        }
      ],
      matrixInputs: 16,
      matrixOutputs: 16,
      matrixBypass: false,
      matrix: []
    });

    await expect(page.locator('#audio-outputs-container .badge-dpll')).toBeVisible({ timeout: 3000 });
  });

  test('HW Volume slider visible only when capabilities include bit 0 (HAL_CAP_HW_VOLUME=1)', async ({ connectedPage: page }) => {
    // ES8311 (index 1) has capabilities=199 (bit 0 set) — hwVol slider should exist
    const hwVolSlider = page.locator('#outputHwVol1');
    await expect(hwVolSlider).toBeAttached({ timeout: 3000 });

    // PCM5102A (index 0) has capabilities=16 (bit 0 NOT set) — hwVol slider should NOT exist
    const hwVolSlider0 = page.locator('#outputHwVol0');
    await expect(hwVolSlider0).not.toBeAttached();
  });

  test('HW Mute label visible when output capabilities include bit 2 (HAL_CAP_HW_MUTE=4)', async ({ connectedPage: page }) => {
    // ES8311 has capabilities=199 which includes bit 2 (4)
    const strip1 = page.locator('.channel-strip-output[data-sink="1"]');
    await expect(strip1.locator('.hw-mute-label, [class*="hw-mute"]')).toBeAttached({ timeout: 3000 });
  });

  test('capability badges render based on bitmask for combined flags', async ({ connectedPage: page }) => {
    // Push output with both DSD (2048) and DPLL (32768) flags set
    await page.wsRoute.send({
      type: 'audioChannelMap',
      inputs: [],
      outputs: [
        {
          index: 0,
          halSlot: 3,
          name: 'ESS Premium',
          firstChannel: 0,
          channels: 2,
          muted: false,
          compatible: 'ess,es9038pro',
          manufacturer: 'ESS Technology',
          capabilities: 2048 | 32768,
          ready: true,
          deviceType: 1,
          i2cAddr: 0
        }
      ],
      matrixInputs: 16,
      matrixOutputs: 16,
      matrixBypass: false,
      matrix: []
    });

    const container = page.locator('#audio-outputs-container');
    await expect(container.locator('.badge-dsd')).toBeVisible({ timeout: 3000 });
    await expect(container.locator('.badge-dpll')).toBeVisible({ timeout: 3000 });
  });
});
