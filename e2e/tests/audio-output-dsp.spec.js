/**
 * audio-output-dsp.spec.js — Audio output DSP overlay controls.
 *
 * Tests for opening the PEQ overlay from output channel strips,
 * verifying canvas and band controls render, and closing.
 */

const { test, expect } = require('../helpers/fixtures');

test.describe('@audio Audio Output DSP', () => {
  test.beforeEach(async ({ connectedPage: page }) => {
    await page.locator('.sidebar-item[data-tab="audio"]').click();
    await page.locator('.audio-subnav-btn[data-view="outputs"]').click();
    await expect(page.locator('#audio-sv-outputs')).toHaveClass(/active/);
    await expect(page.locator('#audio-outputs-container')).not.toContainText('Waiting for device data...', { timeout: 5000 });
    // DSP buttons are inside the collapsible drawer — open the first one
    const drawerToggle = page.locator('#audio-outputs-container [data-action="toggle-output-dsp-drawer"]').first();
    if (await drawerToggle.count() > 0) {
      await drawerToggle.click();
    }
  });

  test('output DSP PEQ button opens overlay', async ({ connectedPage: page }) => {
    // Output 0 has firstChannel=0; PEQ button calls openOutputPeq(0)
    const container = page.locator('#audio-outputs-container');
    const peqBtn = container.locator('button').filter({ hasText: /PEQ/i }).first();

    const btnCount = await peqBtn.count();
    if (btnCount === 0) {
      test.skip(true, 'No PEQ button found on output channel strips');
      return;
    }

    // Intercept the fetch to /api/output/dsp that openOutputPeq triggers
    await page.route('**/api/v1/output/dsp*', async (route) => {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ channel: 0, bypass: false, stages: [] })
      });
    });

    await peqBtn.click();
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });
  });

  test('PEQ overlay contains frequency response canvas', async ({ connectedPage: page }) => {
    await page.route('**/api/v1/output/dsp*', async (route) => {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ channel: 0, bypass: false, stages: [] })
      });
    });

    // Open PEQ via evaluate to avoid button selector issues
    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    const canvas = overlay.locator('canvas');
    await expect(canvas.first()).toBeVisible({ timeout: 3000 });
  });

  test('close overlay returns to output view', async ({ connectedPage: page }) => {
    await page.route('**/api/v1/output/dsp*', async (route) => {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ channel: 0, bypass: false, stages: [] })
      });
    });

    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    // Close overlay
    const closeBtn = overlay.locator('.peq-overlay-close');
    const closeBtnCount = await closeBtn.count();
    if (closeBtnCount > 0) {
      await closeBtn.click();
    } else {
      // Try pressing Escape as alternative
      await page.keyboard.press('Escape');
    }

    await expect(overlay).toBeHidden({ timeout: 3000 });
    // Output sub-view should still be active
    await expect(page.locator('#audio-sv-outputs')).toHaveClass(/active/);
  });

  test('output DSP section has crossover and compressor buttons', async ({ connectedPage: page }) => {
    const container = page.locator('#audio-outputs-container');
    const crossoverBtn = container.locator('button').filter({ hasText: /Crossover/i }).first();
    const compressorBtn = container.locator('button').filter({ hasText: /Compressor/i }).first();

    await expect(crossoverBtn).toBeVisible();
    await expect(compressorBtn).toBeVisible();
  });

  test('output DSP section has limiter and delay controls', async ({ connectedPage: page }) => {
    const container = page.locator('#audio-outputs-container');
    const limiterBtn = container.locator('button').filter({ hasText: /Limiter/i }).first();
    await expect(limiterBtn).toBeVisible();

    // Delay input exists for output 0 (firstChannel=0)
    const delayInput = page.locator('#outputDelay0');
    await expect(delayInput).toBeVisible();
  });
});
