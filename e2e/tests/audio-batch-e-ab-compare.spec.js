/**
 * audio-batch-e-ab-compare.spec.js — A/B compare + Copy-from-channel in PEQ overlay.
 *
 * Tests for:
 *  - A/B toggle buttons present in PEQ overlay header
 *  - Selecting slot B switches state (peqAbSelect)
 *  - Copy-from-channel dropdown present
 *  - Copy-from-channel triggers correct REST call
 *  - Save Preset / Load Preset buttons present in PEQ actions bar
 */

const { test, expect } = require('../helpers/fixtures');

// Helper: open audio tab inputs and open PEQ overlay
async function openPeqOverlay(page) {
  await page.evaluate(() => { switchTab('audio'); switchAudioSubView('inputs'); });
  await expect(page.locator('#audio-sv-inputs')).toHaveClass(/active/, { timeout: 5000 });

  const container = page.locator('#audio-inputs-container');
  await expect(container).not.toContainText('Waiting for device data...', { timeout: 5000 });

  // Open DSP drawer
  const drawerToggle = container.locator('[data-action="toggle-input-dsp-drawer"]').first();
  if (await drawerToggle.count() > 0) {
    await drawerToggle.click();
  }

  // Click PEQ button
  const peqBtn = container.locator('button').filter({ hasText: /Edit PEQ|PEQ/i }).first();
  if (await peqBtn.count() === 0) return false;
  await peqBtn.click();

  await expect(page.locator('#peqOverlay')).toBeVisible({ timeout: 3000 });
  return true;
}

test.describe('@audio PEQ A/B Compare + Copy from Channel', () => {
  test('A/B toggle buttons are present in PEQ overlay', async ({ connectedPage: page }) => {
    const opened = await openPeqOverlay(page);
    if (!opened) {
      test.skip(true, 'PEQ overlay not available');
      return;
    }
    await expect(page.locator('#peqAbBtnA')).toBeVisible({ timeout: 2000 });
    await expect(page.locator('#peqAbBtnB')).toBeVisible({ timeout: 2000 });
  });

  test('A button is active by default', async ({ connectedPage: page }) => {
    const opened = await openPeqOverlay(page);
    if (!opened) {
      test.skip(true, 'PEQ overlay not available');
      return;
    }
    await expect(page.locator('#peqAbBtnA')).toHaveClass(/active/);
    await expect(page.locator('#peqAbBtnB')).not.toHaveClass(/active/);
  });

  test('Clicking B button makes it active', async ({ connectedPage: page }) => {
    const opened = await openPeqOverlay(page);
    if (!opened) {
      test.skip(true, 'PEQ overlay not available');
      return;
    }
    await page.locator('#peqAbBtnB').click();
    await expect(page.locator('#peqAbBtnB')).toHaveClass(/active/, { timeout: 2000 });
    await expect(page.locator('#peqAbBtnA')).not.toHaveClass(/active/);
  });

  test('Clicking A button after B switches back to A', async ({ connectedPage: page }) => {
    const opened = await openPeqOverlay(page);
    if (!opened) {
      test.skip(true, 'PEQ overlay not available');
      return;
    }
    await page.locator('#peqAbBtnB').click();
    await page.locator('#peqAbBtnA').click();
    await expect(page.locator('#peqAbBtnA')).toHaveClass(/active/, { timeout: 2000 });
  });

  test('Copy-from-channel dropdown is present', async ({ connectedPage: page }) => {
    const opened = await openPeqOverlay(page);
    if (!opened) {
      test.skip(true, 'PEQ overlay not available');
      return;
    }
    await expect(page.locator('#peqCopySelect')).toBeVisible({ timeout: 2000 });
  });

  test('Copy-from-channel fetches channel DSP config via REST', async ({ connectedPage: page }) => {
    const opened = await openPeqOverlay(page);
    if (!opened) {
      test.skip(true, 'PEQ overlay not available');
      return;
    }
    // Call peqCopyFromChannel directly — it fetches /api/dsp/channel?channel=0
    const reqPromise = page.waitForRequest(req => req.url().includes('/api') && req.url().includes('/dsp/channel'), { timeout: 5000 });
    await page.evaluate(() => peqCopyFromChannel(0));
    const req = await reqPromise;
    expect(req.url()).toContain('channel=0');
  });

  test('Save Preset button is present in PEQ overlay actions', async ({ connectedPage: page }) => {
    const opened = await openPeqOverlay(page);
    if (!opened) {
      test.skip(true, 'PEQ overlay not available');
      return;
    }
    const saveBtn = page.locator('[data-action="peq-preset-save"]');
    await expect(saveBtn).toBeVisible({ timeout: 2000 });
  });

  test('Load Preset button is present in PEQ overlay actions', async ({ connectedPage: page }) => {
    const opened = await openPeqOverlay(page);
    if (!opened) {
      test.skip(true, 'PEQ overlay not available');
      return;
    }
    const loadBtn = page.locator('[data-action="peq-preset-load"]');
    await expect(loadBtn).toBeVisible({ timeout: 2000 });
  });
});
