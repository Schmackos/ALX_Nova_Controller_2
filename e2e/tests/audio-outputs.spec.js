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
