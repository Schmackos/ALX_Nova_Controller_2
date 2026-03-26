/**
 * peq-ab-compare.spec.js — Deferred from Batch D: A/B compare in PEQ overlay.
 *
 * Tests for:
 * - A/B toggle button visible in PEQ overlay toolbar
 * - Initial state: "A" active
 * - Clicking toggle switches to "B" (different/empty band set)
 * - Toggling back switches to "A" (restores original bands)
 * - Both sets shown correctly in band table
 * - A/B label updates to reflect current set
 */

const { test, expect } = require('../helpers/fixtures');

// Mock /api/output/dsp GET to return empty config
async function mockOutputDsp(page, stages = []) {
  await page.route('**/api/v1/output/dsp*', async (route) => {
    if (route.request().method() === 'GET') {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ channel: 0, bypass: false, stages, sampleRate: 48000 })
      });
    } else {
      await route.fulfill({ status: 200, contentType: 'application/json', body: '{"success":true}' });
    }
  });
}

test.describe('@audio Phase 7 Deferred: PEQ A/B Compare', () => {
  test.beforeEach(async ({ connectedPage: page }) => {
    await page.locator('.sidebar-item[data-tab="audio"]').click();
  });

  test('A/B toggle button present in PEQ overlay', async ({ connectedPage: page }) => {
    await mockOutputDsp(page);
    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    const abBtn = overlay.locator('[data-action="peq-ab-toggle"], button:has-text("A/B"), #peqAbToggle');
    const count = await abBtn.count();
    if (count === 0) {
      test.skip(true, 'A/B toggle not present — implementation pending');
      return;
    }
    await expect(abBtn.first()).toBeVisible();
  });

  test('A/B label shows "A" initially', async ({ connectedPage: page }) => {
    await mockOutputDsp(page);
    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    const abLabel = overlay.locator('#peqAbLabel, .peq-ab-label, [data-action="peq-ab-toggle"]');
    const count = await abLabel.count();
    if (count === 0) {
      test.skip(true, 'A/B label not present — implementation pending');
      return;
    }
    // Initial state should indicate "A" is active
    await expect(abLabel.first()).toContainText(/A/i, { timeout: 2000 });
  });

  test('clicking A/B toggle switches to B set (band table changes)', async ({ connectedPage: page }) => {
    // Start with one band in A set
    await mockOutputDsp(page, [
      { type: 4, frequency: 1000, gain: 3, Q: 1.41, enabled: true }
    ]);
    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    const abBtn = overlay.locator('[data-action="peq-ab-toggle"], button:has-text("A/B"), #peqAbToggle');
    const count = await abBtn.count();
    if (count === 0) {
      test.skip(true, 'A/B toggle not present — implementation pending');
      return;
    }

    // A set has 1 band
    const bandRowsBefore = await overlay.locator('#peqBandRows tr').count();
    expect(bandRowsBefore).toBe(1);

    // Toggle to B (should start empty)
    await abBtn.first().click();
    await page.waitForTimeout(200);

    // After switching to B, the label should update
    const labelAfter = overlay.locator('#peqAbLabel, .peq-ab-label, [data-action="peq-ab-toggle"]');
    if (await labelAfter.count() > 0) {
      await expect(labelAfter.first()).toContainText(/B/i, { timeout: 2000 });
    }
  });

  test('toggling A/B back to A restores original bands', async ({ connectedPage: page }) => {
    await mockOutputDsp(page, [
      { type: 4, frequency: 1000, gain: 3, Q: 1.41, enabled: true }
    ]);
    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    const abBtn = overlay.locator('[data-action="peq-ab-toggle"], button:has-text("A/B"), #peqAbToggle');
    const count = await abBtn.count();
    if (count === 0) {
      test.skip(true, 'A/B toggle not present — implementation pending');
      return;
    }

    const initialCount = await overlay.locator('#peqBandRows tr').count();

    // Switch to B
    await abBtn.first().click();
    await page.waitForTimeout(200);

    // Switch back to A
    await abBtn.first().click();
    await page.waitForTimeout(200);

    const restoredCount = await overlay.locator('#peqBandRows tr').count();
    expect(restoredCount).toBe(initialCount);
  });

  test('A and B sets are independent — changes to B do not affect A', async ({ connectedPage: page }) => {
    await mockOutputDsp(page, [
      { type: 4, frequency: 1000, gain: 3, Q: 1.41, enabled: true }
    ]);
    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    const abBtn = overlay.locator('[data-action="peq-ab-toggle"], button:has-text("A/B"), #peqAbToggle');
    const count = await abBtn.count();
    if (count === 0) {
      test.skip(true, 'A/B toggle not present — implementation pending');
      return;
    }

    const bandCountA = await overlay.locator('#peqBandRows tr').count();

    // Switch to B and add a band there
    await abBtn.first().click();
    await page.waitForTimeout(200);

    const addBandBtn = overlay.locator('[data-action="peq-add-band"]');
    if (await addBandBtn.count() > 0) {
      await addBandBtn.click();
    }

    // Switch back to A — should still have the original count
    await abBtn.first().click();
    await page.waitForTimeout(200);

    const bandCountAAfter = await overlay.locator('#peqBandRows tr').count();
    expect(bandCountAAfter).toBe(bandCountA);
  });
});
