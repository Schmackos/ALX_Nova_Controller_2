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
