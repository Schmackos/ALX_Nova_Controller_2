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

  test('outputs are wrapped in .device-group containers', async ({ connectedPage: page }) => {
    // Each distinct output device name becomes its own .device-group
    const groups = page.locator('#audio-outputs-container .device-group');
    const count = await groups.count();
    expect(count).toBeGreaterThanOrEqual(1);
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

    // .badge-dsd is rendered inside .device-group-badges when hasDsd is truthy
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

  test('HW Volume slider present for ES8311 (capabilities=199 includes bit 0)', async ({ connectedPage: page }) => {
    // ES8311 is sink index 1 with capabilities=199 (bit 0 = HAL_CAP_HW_VOLUME)
    await expect(page.locator('#outputHwVol1')).toBeAttached({ timeout: 3000 });
  });

  test('HW Volume slider absent for PCM5102A (capabilities=16, bit 0 not set)', async ({ connectedPage: page }) => {
    // PCM5102A is sink index 0 with capabilities=16 — no HW volume slider
    await expect(page.locator('#outputHwVol0')).not.toBeAttached();
  });

  test('mute button shows "HW Mute" text when capabilities include bit 2 (HAL_CAP_MUTE=4)', async ({ connectedPage: page }) => {
    // ES8311 has capabilities=199 which includes bit 2 (4) — mute button label = "HW Mute"
    const muteBtn = page.locator('#outputMute1');
    await expect(muteBtn).toBeAttached({ timeout: 3000 });
    await expect(muteBtn).toHaveText('HW Mute');
  });

  test('mute button shows plain "Mute" when HAL_CAP_MUTE not set', async ({ connectedPage: page }) => {
    // PCM5102A capabilities=16, bit 2 not set — plain "Mute"
    const muteBtn = page.locator('#outputMute0');
    await expect(muteBtn).toBeAttached();
    await expect(muteBtn).toHaveText('Mute');
  });

  test('DSD and DPLL badges both visible when both capability bits set', async ({ connectedPage: page }) => {
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
