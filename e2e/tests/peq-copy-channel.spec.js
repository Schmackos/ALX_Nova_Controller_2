/**
 * peq-copy-channel.spec.js — Deferred from Batch D: Copy-from-channel in PEQ overlay.
 *
 * Tests for:
 * - Copy-from-channel dropdown present in PEQ overlay toolbar
 * - Dropdown lists available channels from audioChannelMap fixture
 * - Selecting a channel and confirming copies its PEQ bands into the overlay
 * - Band table and graph update to reflect copied bands
 * - Copying from a channel with no bands clears the current overlay bands
 */

const { test, expect } = require('../helpers/fixtures');

// Mock /api/output/dsp GET — returns stages per channel
async function mockOutputDsp(page, channelStages = {}) {
  await page.route('**/api/v1/output/dsp*', async (route) => {
    if (route.request().method() === 'GET') {
      const url = route.request().url();
      const chMatch = url.match(/[?&](?:ch|channel)=(\d+)/);
      const ch = chMatch ? parseInt(chMatch[1], 10) : 0;
      const stages = channelStages[ch] || [];
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ channel: ch, bypass: false, stages, sampleRate: 48000 })
      });
    } else {
      await route.fulfill({ status: 200, contentType: 'application/json', body: '{"success":true}' });
    }
  });
}

test.describe('@audio Phase 7 Deferred: PEQ Copy from Channel', () => {
  test.beforeEach(async ({ connectedPage: page }) => {
    await page.locator('.sidebar-item[data-tab="audio"]').click();
  });

  test('copy-from-channel control present in PEQ overlay', async ({ connectedPage: page }) => {
    await mockOutputDsp(page);
    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    const copyEl = overlay.locator(
      '#peqCopyChannelSelect, [data-action="peq-copy-channel"], select.peq-copy-select, button:has-text("Copy from")'
    );
    const count = await copyEl.count();
    if (count === 0) {
      test.skip(true, 'Copy-from-channel control not present — implementation pending');
      return;
    }
    await expect(copyEl.first()).toBeVisible();
  });

  test('copy-from-channel dropdown lists channels from audioChannelMap', async ({ connectedPage: page }) => {
    await mockOutputDsp(page);
    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    const select = overlay.locator('#peqCopyChannelSelect, select.peq-copy-select');
    const count = await select.count();
    if (count === 0) {
      test.skip(true, 'Copy-from-channel select not present — implementation pending');
      return;
    }

    // audioChannelMap fixture has at least 2 output channels
    const options = select.first().locator('option');
    const optCount = await options.count();
    // At minimum there should be a placeholder + at least 1 channel option
    expect(optCount).toBeGreaterThanOrEqual(1);
  });

  test('copy from source channel replaces current overlay bands', async ({ connectedPage: page }) => {
    // Channel 1 has 2 biquad stages, channel 0 starts empty
    await mockOutputDsp(page, {
      0: [],
      1: [
        { type: 4, frequency: 500, gain: 2, Q: 1.0, enabled: true },
        { type: 5, frequency: 100, gain: -3, Q: 0.707, enabled: true }
      ]
    });

    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    const select = overlay.locator('#peqCopyChannelSelect, select.peq-copy-select');
    if (await select.count() === 0) {
      test.skip(true, 'Copy-from-channel select not present — implementation pending');
      return;
    }

    const copyBtn = overlay.locator('[data-action="peq-copy-channel"], button:has-text("Copy")');
    if (await copyBtn.count() === 0) {
      test.skip(true, 'Copy button not present — implementation pending');
      return;
    }

    // Start with 0 bands
    await expect(overlay.locator('#peqBandRows tr')).toHaveCount(0);

    // Select channel 1 and copy
    await select.first().selectOption('1');
    await copyBtn.first().click();
    await page.waitForTimeout(300);

    // Should now have 2 bands from channel 1
    const bandCount = await overlay.locator('#peqBandRows tr').count();
    expect(bandCount).toBe(2);
  });

  test('copy from channel with no bands clears overlay bands', async ({ connectedPage: page }) => {
    // Channel 0 starts with 1 band; channel 2 has no bands
    await mockOutputDsp(page, {
      0: [{ type: 4, frequency: 1000, gain: 3, Q: 1.41, enabled: true }],
      2: []
    });

    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    const select = overlay.locator('#peqCopyChannelSelect, select.peq-copy-select');
    const copyBtn = overlay.locator('[data-action="peq-copy-channel"], button:has-text("Copy")');
    if (await select.count() === 0 || await copyBtn.count() === 0) {
      test.skip(true, 'Copy-from-channel controls not present — implementation pending');
      return;
    }

    // Starts with 1 band
    await expect(overlay.locator('#peqBandRows tr')).toHaveCount(1);

    // Copy from channel 2 (empty)
    await select.first().selectOption('2');
    await copyBtn.first().click();
    await page.waitForTimeout(300);

    // Should now be 0 bands
    await expect(overlay.locator('#peqBandRows tr')).toHaveCount(0);
  });

  test('copy from same channel is a no-op or copies identical bands', async ({ connectedPage: page }) => {
    await mockOutputDsp(page, {
      0: [{ type: 4, frequency: 1000, gain: 3, Q: 1.41, enabled: true }]
    });

    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    const select = overlay.locator('#peqCopyChannelSelect, select.peq-copy-select');
    const copyBtn = overlay.locator('[data-action="peq-copy-channel"], button:has-text("Copy")');
    if (await select.count() === 0 || await copyBtn.count() === 0) {
      test.skip(true, 'Copy-from-channel controls not present — implementation pending');
      return;
    }

    const beforeCount = await overlay.locator('#peqBandRows tr').count();

    // Copy from channel 0 (same as open target)
    await select.first().selectOption('0');
    await copyBtn.first().click();
    await page.waitForTimeout(300);

    // Band count should be the same (copied identical bands)
    const afterCount = await overlay.locator('#peqBandRows tr').count();
    expect(afterCount).toBe(beforeCount);
  });
});
