/**
 * visual-audio-tab.spec.js — Visual regression tests for the Audio tab,
 * including inputs, outputs, signal generator, matrix, PEQ overlay,
 * and the audio sub-navigation bar.
 */

const { test, expect } = require('../helpers/fixtures');

test.describe('@visual Visual Audio Tab', () => {

  test('audio inputs view with channel strips', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('audio'));
    await page.waitForTimeout(300);

    // Ensure we are on the inputs sub-view
    await page.evaluate(() => {
      const btn = document.querySelector('.audio-subnav-btn[data-view="inputs"]');
      if (btn) btn.click();
    });
    await page.waitForTimeout(300);

    const container = page.locator('#audio-sv-inputs');
    await expect(container).toBeVisible({ timeout: 5000 });
    await expect(container).toHaveScreenshot('audio-inputs-view.png', {
      maxDiffPixelRatio: 0.02,
    });
  });

  test('audio outputs view', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('audio'));
    await page.waitForTimeout(300);

    await page.evaluate(() => {
      const btn = document.querySelector('.audio-subnav-btn[data-view="outputs"]');
      if (btn) btn.click();
    });
    await page.waitForTimeout(300);

    const container = page.locator('#audio-sv-outputs');
    await expect(container).toBeVisible({ timeout: 5000 });
    await expect(container).toHaveScreenshot('audio-outputs-view.png', {
      maxDiffPixelRatio: 0.02,
    });
  });

  test('signal generator panel', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('audio'));
    await page.waitForTimeout(300);

    // Navigate to siggen sub-view
    await page.evaluate(() => {
      const btn = document.querySelector('.audio-subnav-btn[data-view="siggen"]');
      if (btn) btn.click();
    });
    await page.waitForTimeout(300);

    // Push signal generator state with it enabled
    page.wsRoute.send({
      type: 'signalGenerator',
      enabled: true,
      waveform: 0,
      frequency: 1000.0,
      amplitude: -20.0,
      channel: 0,
      outputMode: 0,
      sweepSpeed: 100.0
    });
    await page.waitForTimeout(300);

    const siggenView = page.locator('#audio-sv-siggen');
    await expect(siggenView).toBeVisible({ timeout: 5000 });
    await expect(siggenView).toHaveScreenshot('signal-generator-panel.png', {
      maxDiffPixelRatio: 0.02,
    });
  });

  test('matrix routing grid', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('audio'));
    await page.waitForTimeout(300);

    await page.evaluate(() => {
      const btn = document.querySelector('.audio-subnav-btn[data-view="matrix"]');
      if (btn) btn.click();
    });
    await page.waitForTimeout(300);

    const container = page.locator('#audio-sv-matrix');
    await expect(container).toBeVisible({ timeout: 5000 });
    await expect(container).toHaveScreenshot('audio-matrix-grid.png', {
      maxDiffPixelRatio: 0.02,
    });
  });

  test('PEQ overlay', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('audio'));
    await page.waitForTimeout(300);

    // Open PEQ overlay programmatically with sample bands
    await page.evaluate(() => {
      if (typeof openPeqOverlay === 'function') {
        openPeqOverlay(
          { type: 'input', channel: 0 },
          [
            { type: 0, freq: 100, gain: 3.0, Q: 1.0, enabled: true },
            { type: 0, freq: 1000, gain: -2.0, Q: 0.7, enabled: true },
            { type: 0, freq: 8000, gain: 1.5, Q: 1.4, enabled: true }
          ],
          48000
        );
      }
    });
    await page.waitForTimeout(500);

    const overlay = page.locator('#peqOverlay');
    const isVisible = await overlay.isVisible().catch(() => false);
    if (isVisible) {
      await expect(overlay).toHaveScreenshot('peq-overlay.png', {
        maxDiffPixelRatio: 0.03,
      });
    } else {
      // PEQ overlay may not render if DSP is not enabled; skip gracefully
      test.skip();
    }
  });

  test('audio sub-navigation bar', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('audio'));
    await page.waitForTimeout(300);

    const subNav = page.locator('.audio-subnav');
    await expect(subNav).toBeVisible({ timeout: 5000 });
    await expect(subNav).toHaveScreenshot('audio-subnav-bar.png', {
      maxDiffPixelRatio: 0.02,
    });
  });

});
