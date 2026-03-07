/**
 * audio-siggen.spec.js — Signal generator controls on the SigGen sub-view.
 *
 * Note: #siggenEnable is a <input type="checkbox"> inside <label class="switch">.
 * CSS hides the raw input (opacity:0; width:0; height:0). Click the parent label to
 * toggle, and assert state with toBeChecked().
 */

const { test, expect } = require('../helpers/fixtures');

test('signal generator controls render and enable toggle reveals parameters', async ({ connectedPage: page }) => {
  await page.locator('.sidebar-item[data-tab="audio"]').click();
  await page.locator('.audio-subnav-btn[data-view="siggen"]').click();

  await expect(page.locator('#audio-sv-siggen')).toHaveClass(/active/);

  // The label wrapping the enable toggle is the clickable element
  const enableLabel = page.locator('label.switch:has(#siggenEnable)');
  const enableToggle = page.locator('#siggenEnable');
  await expect(enableLabel).toBeVisible();

  // Enable toggle is unchecked (sigGen disabled in fixture)
  await expect(enableToggle).not.toBeChecked();

  // The siggen fields div is hidden while disabled
  await expect(page.locator('#siggenFields')).toBeHidden();

  // Enabling the toggle (click label) reveals the parameter fields
  await enableLabel.click();
  await expect(page.locator('#siggenFields')).toBeVisible({ timeout: 2000 });

  // Waveform select, frequency slider, amplitude slider must be visible
  await expect(page.locator('#siggenWaveform')).toBeVisible();
  await expect(page.locator('#siggenFreq')).toBeVisible();
  await expect(page.locator('#siggenAmp')).toBeVisible();

  // Waveform options include Sine, Square, White Noise, Frequency Sweep
  const options = page.locator('#siggenWaveform option');
  await expect(options).toHaveCount(4);
  await expect(options.nth(0)).toHaveText('Sine');
  await expect(options.nth(1)).toHaveText('Square');
});
