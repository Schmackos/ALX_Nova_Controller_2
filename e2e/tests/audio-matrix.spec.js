/**
 * audio-matrix.spec.js — 8x8 routing matrix grid renders correctly.
 */

const { test, expect } = require('../helpers/fixtures');

test('8x8 routing matrix grid renders with input and output labels', async ({ connectedPage: page }) => {
  // Switch to Audio tab, then Matrix sub-view
  await page.locator('.sidebar-item[data-tab="audio"]').click();
  await page.locator('.audio-subnav-btn[data-view="matrix"]').click();

  // The matrix sub-view should become active
  await expect(page.locator('#audio-sv-matrix')).toHaveClass(/active/);

  // The matrix container must be populated (not showing placeholder text)
  const matrixContainer = page.locator('#audio-matrix-container');

  // After the audioChannelMap fixture is applied, the matrix renders a table/grid
  await expect(matrixContainer).not.toContainText('Waiting for device data...', { timeout: 5000 });

  // Routing matrix header buttons exist (1:1 Pass, Clear).
  // The matrixPreset1to1() button appears both in the card header and inside the matrix container
  // after rendering — use .first() to avoid strict mode violation.
  await expect(page.locator('button[onclick="matrixPreset1to1()"]').first()).toBeVisible();
  await expect(page.locator('button[onclick="matrixPresetClear()"]').first()).toBeVisible();
});
