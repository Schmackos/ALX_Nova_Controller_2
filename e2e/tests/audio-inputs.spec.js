/**
 * audio-inputs.spec.js — Audio tab sub-navigation and input channel strips.
 */

const { test, expect } = require('../helpers/fixtures');

test('audio sub-nav tabs render with correct labels', async ({ connectedPage: page }) => {
  await page.locator('.sidebar-item[data-tab="audio"]').click();

  // Four sub-navigation buttons must be present
  const subNav = page.locator('.audio-subnav');
  await expect(subNav).toBeVisible();

  await expect(page.locator('.audio-subnav-btn[data-view="inputs"]')).toBeVisible();
  await expect(page.locator('.audio-subnav-btn[data-view="matrix"]')).toBeVisible();
  await expect(page.locator('.audio-subnav-btn[data-view="outputs"]')).toBeVisible();
  await expect(page.locator('.audio-subnav-btn[data-view="siggen"]')).toBeVisible();

  // Text labels
  await expect(page.locator('.audio-subnav-btn[data-view="inputs"]')).toContainText('Inputs');
  await expect(page.locator('.audio-subnav-btn[data-view="matrix"]')).toContainText('Matrix');
  await expect(page.locator('.audio-subnav-btn[data-view="outputs"]')).toContainText('Outputs');
  await expect(page.locator('.audio-subnav-btn[data-view="siggen"]')).toContainText('SigGen');
});

test('audioChannelMap WS message populates input channel strips', async ({ connectedPage: page }) => {
  await page.locator('.sidebar-item[data-tab="audio"]').click();

  // The Inputs sub-view should be active by default
  await expect(page.locator('#audio-sv-inputs')).toHaveClass(/active/);

  // The fixture audioChannelMap.json was broadcast on connect — container should
  // have been populated. Wait for at least one channel strip card element.
  const container = page.locator('#audio-inputs-container');

  // After switching to the audio tab, renderInputStrips() fires and replaces the
  // placeholder. The fixture has inputs with deviceName "PCM1808".
  await expect(container).not.toContainText('Waiting for device data...', { timeout: 5000 });

  // Verify known device names from the audio-channel-map.json fixture appear
  // (deviceName field rendered in each channel strip header)
  await expect(container).toContainText('PCM1808', { timeout: 3000 });
});

test.describe('@audio Phase 1 — Device Grouping (Inputs)', () => {
  test.beforeEach(async ({ connectedPage: page }) => {
    await page.locator('.sidebar-item[data-tab="audio"]').click();
    await expect(page.locator('#audio-sv-inputs')).toHaveClass(/active/);
    await expect(page.locator('#audio-inputs-container')).not.toContainText('Waiting for device data...', { timeout: 5000 });
  });

  test('inputs with same deviceName are grouped under one .device-group', async ({ connectedPage: page }) => {
    // The fixture has lanes 0+1 both with deviceName "PCM1808" — they should share a group
    const groups = page.locator('#audio-inputs-container .device-group');
    const count = await groups.count();
    expect(count).toBeGreaterThanOrEqual(1);

    // PCM1808 group should contain both lane 0 and lane 1 strips
    const pcmGroup = page.locator('#audio-inputs-container .device-group').filter({ hasText: 'PCM1808' });
    await expect(pcmGroup).toHaveCount(1);
    await expect(pcmGroup.locator('.channel-strip[data-lane="0"]')).toBeAttached();
    await expect(pcmGroup.locator('.channel-strip[data-lane="1"]')).toBeAttached();
  });

  test('device group header shows device name and manufacturer', async ({ connectedPage: page }) => {
    const pcmGroup = page.locator('#audio-inputs-container .device-group').filter({ hasText: 'PCM1808' });
    const header = pcmGroup.locator('.device-group-header');
    await expect(header).toContainText('PCM1808');
    await expect(header).toContainText('Texas Instruments');
  });

  test('empty state shown when audioChannelMap has empty inputs array', async ({ connectedPage: page }) => {
    await page.wsRoute.send({
      type: 'audioChannelMap',
      inputs: [],
      outputs: [],
      matrixInputs: 16,
      matrixOutputs: 16,
      matrixBypass: false,
      matrix: []
    });

    const container = page.locator('#audio-inputs-container');
    await expect(container.locator('.empty-state')).toBeVisible({ timeout: 3000 });
  });

  test('hot-plug toast appears when audioChannelMap device list changes', async ({ connectedPage: page }) => {
    // Push updated audioChannelMap with a different set of inputs — hash changes, toast fires
    await page.wsRoute.send({
      type: 'audioChannelMap',
      inputs: [
        {
          lane: 0,
          name: 'ADC1',
          channels: 2,
          matrixCh: 0,
          deviceName: 'PCM1808',
          deviceType: 2,
          compatible: 'ti,pcm1808',
          manufacturer: 'Texas Instruments',
          capabilities: 8,
          ready: false
        }
      ],
      outputs: [],
      matrixInputs: 16,
      matrixOutputs: 16,
      matrixBypass: false,
      matrix: []
    });

    // showToast() sets #toast to class "toast show info" with the message text
    await expect(page.locator('#toast.show')).toBeVisible({ timeout: 3000 });
    await expect(page.locator('#toast.show')).toContainText('Audio devices changed');
  });

  test('channel label shows inputNames value for lane 0', async ({ connectedPage: page }) => {
    // input-names.json fixture has "ADC1 L" at index 0; lane 0 uses inputNames[lane*2] = inputNames[0]
    const label0 = page.locator('.channel-label[data-lane="0"]');
    await expect(label0).toBeAttached();
    await expect(label0).toContainText('ADC1 L');
  });

  test('stereo link toggle present on every input strip', async ({ connectedPage: page }) => {
    // Every strip has a button with data-action="toggle-stereo-link" and matching data-lane
    await expect(page.locator('[data-action="toggle-stereo-link"][data-lane="0"]')).toBeAttached();
    await expect(page.locator('[data-action="toggle-stereo-link"][data-lane="1"]')).toBeAttached();
  });

  test('stereo link toggle activates and mirrors gain to paired lane', async ({ connectedPage: page }) => {
    // Click the stereo link toggle on lane 0 (pairs with lane 1)
    await page.locator('[data-action="toggle-stereo-link"][data-lane="0"]').click();

    // After re-render (renderInputStrips clears hash and re-renders), toggle should be active
    await expect(page.locator('.stereo-link-toggle[data-lane="0"]')).toHaveClass(/active/, { timeout: 2000 });

    // Setting gain on lane 0 should send WS commands for both lane 0 and lane 1
    const slider = page.locator('#inputGain0');
    await slider.fill('6');
    await slider.dispatchEvent('input');
    await page.waitForTimeout(300);

    const gainCmds = (page.wsCapture || []).filter(c => c.type === 'setInputGain');
    expect(gainCmds.find(c => c.lane === 0 && c.db === 6), 'lane 0 gain command').toBeDefined();
    expect(gainCmds.find(c => c.lane === 1 && c.db === 6), 'lane 1 mirrored gain command').toBeDefined();
  });
});
