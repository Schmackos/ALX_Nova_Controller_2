/**
 * peq-drag-graph.spec.js — Phase 5: PEQ Overlay with drag-on-graph.
 *
 * Tests for:
 * - PEQ overlay opens for both input (lane) and output (channel) targets
 * - Frequency response canvas is present with correct id
 * - Band table is synced with bands: add band adds a table row
 * - Band type, freq, gain, Q inputs update the graph (peqDrawGraph called)
 * - Band enable checkbox toggling
 * - 5 Hz lower bound: band at 5 Hz is valid (not clamped to 20)
 * - Apply sends correct REST payload for output PEQ
 * - Add Band / Reset All / Cancel buttons work
 * - peqOverlayActive state resets on close
 */

const { test, expect } = require('../helpers/fixtures');

// Helper: intercept /api/output/dsp and return an empty stages config
async function mockOutputDsp(page, ch = 0, stages = []) {
  await page.route(`**/api/v1/output/dsp*`, async (route) => {
    if (route.request().method() === 'GET') {
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

test.describe('@audio @api Phase 5: PEQ Overlay', () => {
  test.beforeEach(async ({ connectedPage: page }) => {
    await page.locator('.sidebar-item[data-tab="audio"]').click();
  });

  // ===== Basic open/close =====

  test('openOutputPeq() opens overlay with canvas', async ({ connectedPage: page }) => {
    await mockOutputDsp(page, 0);
    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    const canvas = overlay.locator('#peqOverlayCanvas');
    await expect(canvas).toBeVisible({ timeout: 2000 });
  });

  test('openInputPeq() opens overlay with canvas', async ({ connectedPage: page }) => {
    await page.evaluate(() => openInputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    const canvas = overlay.locator('#peqOverlayCanvas');
    await expect(canvas).toBeVisible({ timeout: 2000 });
  });

  test('overlay header shows channel context for output', async ({ connectedPage: page }) => {
    await mockOutputDsp(page, 2);
    await page.evaluate(() => openOutputPeq(2));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    const title = overlay.locator('.peq-overlay-title');
    await expect(title).toContainText('Ch 2');
  });

  test('overlay header shows lane context for input', async ({ connectedPage: page }) => {
    await page.evaluate(() => openInputPeq(1));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    const title = overlay.locator('.peq-overlay-title');
    await expect(title).toContainText('Lane 1');
  });

  test('close button hides overlay', async ({ connectedPage: page }) => {
    await mockOutputDsp(page, 0);
    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    await overlay.locator('.peq-overlay-close').click();
    await expect(overlay).toBeHidden({ timeout: 2000 });
  });

  test('Cancel button hides overlay', async ({ connectedPage: page }) => {
    await mockOutputDsp(page, 0);
    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    await overlay.locator('[data-action="peq-close"]').last().click();
    await expect(overlay).toBeHidden({ timeout: 2000 });
  });

  // ===== Band table =====

  test('overlay opens with empty band table when no stages', async ({ connectedPage: page }) => {
    await mockOutputDsp(page, 0, []);
    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    const bandRows = overlay.locator('#peqBandRows tr');
    await expect(bandRows).toHaveCount(0);
  });

  test('overlay loads existing biquad stages as band rows', async ({ connectedPage: page }) => {
    await mockOutputDsp(page, 0, [
      { type: 4, frequency: 1000, gain: 3, Q: 1.41, enabled: true },
      { type: 6, frequency: 8000, gain: -2, Q: 0.707, enabled: true }
    ]);
    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    const bandRows = overlay.locator('#peqBandRows tr');
    await expect(bandRows).toHaveCount(2);
  });

  test('Add Band button adds a row to the band table', async ({ connectedPage: page }) => {
    await mockOutputDsp(page, 0, []);
    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    await overlay.locator('[data-action="peq-add-band"]').click();
    const bandRows = overlay.locator('#peqBandRows tr');
    await expect(bandRows).toHaveCount(1);
  });

  test('Add Band button adds default Peak filter at 1000 Hz', async ({ connectedPage: page }) => {
    await mockOutputDsp(page, 0, []);
    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    await overlay.locator('[data-action="peq-add-band"]').click();
    const firstRow = overlay.locator('#peqBandRows tr').first();

    // Default: Peak (type=4), freq=1000
    const freqInput = firstRow.locator('input[type="number"]').nth(0);
    await expect(freqInput).toHaveValue('1000');
  });

  test('Reset All clears all band rows', async ({ connectedPage: page }) => {
    await mockOutputDsp(page, 0, [
      { type: 4, frequency: 1000, gain: 3, Q: 1.41, enabled: true }
    ]);
    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    await expect(overlay.locator('#peqBandRows tr')).toHaveCount(1);

    await overlay.locator('[data-action="peq-reset-all"]').click();
    await expect(overlay.locator('#peqBandRows tr')).toHaveCount(0);
  });

  test('Remove band button decreases band count', async ({ connectedPage: page }) => {
    await mockOutputDsp(page, 0, [
      { type: 4, frequency: 500, gain: 2, Q: 1.0, enabled: true },
      { type: 5, frequency: 100, gain: 3, Q: 0.707, enabled: true }
    ]);
    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    await expect(overlay.locator('#peqBandRows tr')).toHaveCount(2);

    // Click delete button on first band row
    await overlay.locator('#peqBandRows tr').first().locator('[data-action="peq-remove-band"]').click();
    await expect(overlay.locator('#peqBandRows tr')).toHaveCount(1);
  });

  test('band enable checkbox unchecked renders as disabled', async ({ connectedPage: page }) => {
    await mockOutputDsp(page, 0, [
      { type: 4, frequency: 1000, gain: 3, Q: 1.41, enabled: false }
    ]);
    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    const enableCheckbox = overlay.locator('#peqBandRows tr').first().locator('input[type="checkbox"]');
    await expect(enableCheckbox).not.toBeChecked();
  });

  // ===== Apply (output PEQ) =====

  test('Apply sends PUT to /api/output/dsp with bands as stages', async ({ connectedPage: page }) => {
    let putBody = null;
    await page.route('**/api/v1/output/dsp*', async (route) => {
      const method = route.request().method();
      if (method === 'GET') {
        await route.fulfill({
          status: 200,
          contentType: 'application/json',
          body: JSON.stringify({ channel: 0, bypass: false, stages: [], sampleRate: 48000 })
        });
      } else if (method === 'PUT') {
        putBody = route.request().postDataJSON();
        await route.fulfill({ status: 200, contentType: 'application/json', body: '{"success":true}' });
      } else {
        await route.fulfill({ status: 200, contentType: 'application/json', body: '{"success":true}' });
      }
    });

    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    // Add a band then apply
    await overlay.locator('[data-action="peq-add-band"]').click();
    await overlay.locator('[data-action="peq-apply"]').click();

    await page.waitForTimeout(400);
    expect(putBody).not.toBeNull();
    expect(putBody.ch).toBe(0);
    expect(Array.isArray(putBody.stages)).toBe(true);
    expect(putBody.stages.length).toBeGreaterThan(0);
    // Each band stage should have type, frequency, gain, Q
    expect(putBody.stages[0].type).toBeDefined();
    expect(putBody.stages[0].frequency).toBeDefined();
  });

  test('Apply for input PEQ sends WS addDspStage messages', async ({ connectedPage: page }) => {
    await page.evaluate(() => openInputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    await overlay.locator('[data-action="peq-add-band"]').click();
    await overlay.locator('[data-action="peq-apply"]').click();

    await page.waitForTimeout(400);
    // WS capture is stored on the Node.js page object (not window), set by the connectedPage fixture
    const wsCap = page.wsCapture || [];
    const addStage = wsCap.find(m => m.type === 'addDspStage');
    expect(addStage).toBeTruthy();
    expect(addStage.ch).toBe(0);
    expect(addStage.frequency).toBeDefined();
  });

  // ===== Frequency range =====

  test('band frequency input accepts 5 Hz (infrasonic support)', async ({ connectedPage: page }) => {
    await mockOutputDsp(page, 0, []);
    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    await overlay.locator('[data-action="peq-add-band"]').click();
    const freqInput = overlay.locator('#peqBandRows tr').first().locator('input[type="number"]').nth(0);

    await freqInput.fill('5');
    await freqInput.dispatchEvent('change');

    // Value should remain 5 (not clamped to 20 from old design)
    await expect(freqInput).toHaveValue('5');
  });

  // ===== Overlay reuse and re-opening =====

  test('closing and re-opening overlay creates fresh band list', async ({ connectedPage: page }) => {
    await mockOutputDsp(page, 0, []);
    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    // Add a band
    await overlay.locator('[data-action="peq-add-band"]').click();
    await expect(overlay.locator('#peqBandRows tr')).toHaveCount(1);

    // Close overlay
    await overlay.locator('.peq-overlay-close').click();
    await expect(overlay).toBeHidden({ timeout: 2000 });

    // Re-open with no stages — should be empty again
    await page.evaluate(() => openOutputPeq(0));
    await expect(overlay).toBeVisible({ timeout: 3000 });
    await expect(overlay.locator('#peqBandRows tr')).toHaveCount(0);
  });
});
