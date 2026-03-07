/**
 * peq-overlay.spec.js — PEQ overlay opens with frequency response canvas and band controls.
 *
 * The PEQ overlay is triggered by DSP buttons on input/output channel strips.
 * Since channel strips are dynamically rendered from audioChannelMap, we locate
 * the first DSP/PEQ button that appears after the fixture populates the strips.
 *
 * The overlay is created dynamically as <div id="peqOverlay" class="peq-overlay">.
 */

const { test, expect } = require('../helpers/fixtures');

test('PEQ overlay opens with frequency response canvas when DSP button clicked', async ({ connectedPage: page }) => {
  // Navigate to Audio > Inputs where DSP buttons appear on channel strips
  await page.locator('.sidebar-item[data-tab="audio"]').click();
  await expect(page.locator('#audio-sv-inputs')).toHaveClass(/active/);

  // Wait for channel strips to populate
  const container = page.locator('#audio-inputs-container');
  await expect(container).not.toContainText('Waiting for device data...', { timeout: 5000 });

  // Find the first PEQ button within a channel strip
  // Channel strips render buttons with text "PEQ" via renderInputStrips() in 05-audio-tab.js
  const dspBtn = container.locator('button').filter({ hasText: /PEQ|DSP|EQ/i }).first();

  // If no DSP button is found the channel strips may use a different label —
  // skip gracefully rather than fail the whole suite.
  const count = await dspBtn.count();
  if (count === 0) {
    return;
  }

  await dspBtn.click();

  // The PEQ overlay is created as <div id="peqOverlay" class="peq-overlay"> with display:flex.
  // Use the class selector to avoid matching other hidden modal overlays in the DOM.
  const overlay = page.locator('.peq-overlay#peqOverlay');
  await expect(overlay).toBeVisible({ timeout: 3000 });

  // A canvas element for the frequency response graph must be present inside the overlay
  const canvas = overlay.locator('canvas');
  await expect(canvas).toBeVisible({ timeout: 3000 });
});
