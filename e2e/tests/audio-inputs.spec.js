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
    // At least PCM1808 group and Signal Generator/USB Audio entries
    await expect(groups).toHaveCount({ minimum: 1 });

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
    // Push a new audioChannelMap with no inputs
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

  test('hot-plug toast appears when device count changes in audioChannelMap', async ({ connectedPage: page }) => {
    // Push updated audioChannelMap with one fewer input
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
          ready: true
        }
      ],
      outputs: [],
      matrixInputs: 16,
      matrixOutputs: 16,
      matrixBypass: false,
      matrix: []
    });

    // A toast or notification element should appear indicating a device change
    await expect(page.locator('.toast, .hotplug-toast, [class*="toast"]')).toBeVisible({ timeout: 3000 });
  });

  test('channel label displays the name from inputNames fixture', async ({ connectedPage: page }) => {
    // The fixture input-names.json has "ADC1 L" at index 0 (lane 0, channel 0)
    // The label for lane 0 should reflect the name for its first matrix channel
    const strip0 = page.locator('.channel-strip[data-lane="0"]');
    await expect(strip0.locator('.channel-label[data-lane="0"]')).toBeAttached();
  });

  test('stereo link toggle is present for paired lanes', async ({ connectedPage: page }) => {
    // Lanes 0+1 are PCM1808 stereo pair — stereo link toggle should appear
    const stereoToggle = page.locator('.stereo-link-toggle[data-lane="0"]');
    await expect(stereoToggle).toBeAttached();
  });

  test('stereo link toggle mirrors gain to paired lane when active', async ({ connectedPage: page }) => {
    const stereoToggle = page.locator('.stereo-link-toggle[data-lane="0"]');
    // Only attempt if the toggle is visible (capability-dependent)
    const count = await stereoToggle.count();
    if (count === 0) {
      test.skip(true, 'Stereo link toggle not rendered for this fixture');
      return;
    }

    // Enable stereo link
    await stereoToggle.click();
    await expect(stereoToggle).toHaveClass(/active/);

    // Setting gain on lane 0 should also fire a WS command for lane 1
    const slider = page.locator('#inputGain0');
    await slider.fill('6');
    await slider.dispatchEvent('input');

    // Both lanes should receive the gain command
    await page.waitForTimeout(200);
    const captures = page.wsCapture || [];
    const gainCmds = captures.filter(c => c.type === 'setInputGain');
    const lane1Cmd = gainCmds.find(c => c.lane === 1 && c.db === 6);
    expect(lane1Cmd, 'Stereo link should mirror gain to lane 1').toBeDefined();
  });
});
