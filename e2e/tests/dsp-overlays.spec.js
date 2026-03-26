/**
 * dsp-overlays.spec.js — Phase 6: DSP Drawer + stage overlays.
 *
 * Tests for:
 * - DSP button on input channel strips (open-input-peq action)
 * - DSP/PEQ button on output channel strips (open-output-peq action)
 * - Crossover overlay: opens, shows type/freq selects, Apply POSTs to /api/output/dsp/crossover
 * - Compressor overlay: opens, shows all 6 sliders (threshold/ratio/attack/release/knee/makeup)
 * - Limiter overlay: opens, shows 3 sliders (threshold/attack/release)
 * - All overlays share the same #peqOverlay container (reuse pattern)
 * - Each overlay close button returns to outputs view
 */

const { test, expect } = require('../helpers/fixtures');

// Navigate to Audio > Outputs and wait for content
async function goToOutputs(page) {
  await page.locator('.sidebar-item[data-tab="audio"]').click();
  await page.locator('.audio-subnav-btn[data-view="outputs"]').click();
  await expect(page.locator('#audio-sv-outputs')).toHaveClass(/active/);
  await expect(page.locator('#audio-outputs-container')).not.toContainText(
    'Waiting for device data...', { timeout: 5000 }
  );
}

// Open the first input DSP drawer so buttons inside are accessible
async function openFirstInputDrawer(page) {
  const toggle = page.locator('#audio-inputs-container [data-action="toggle-input-dsp-drawer"]').first();
  await toggle.click();
}

// Open the first output DSP drawer so buttons inside are accessible
async function openFirstOutputDrawer(page) {
  const toggle = page.locator('#audio-outputs-container [data-action="toggle-output-dsp-drawer"]').first();
  await toggle.click();
}

// Navigate to Audio > Inputs and wait for content
async function goToInputs(page) {
  await page.locator('.sidebar-item[data-tab="audio"]').click();
  await expect(page.locator('#audio-sv-inputs')).toHaveClass(/active/);
  await expect(page.locator('#audio-inputs-container')).not.toContainText(
    'Waiting for device data...', { timeout: 5000 }
  );
}

// Mock /api/output/dsp GET to return empty config
async function mockOutputDsp(page) {
  await page.route('**/api/v1/output/dsp*', async (route) => {
    if (route.request().method() === 'GET') {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ channel: 0, bypass: false, stages: [], sampleRate: 48000 })
      });
    } else {
      await route.fulfill({ status: 200, contentType: 'application/json', body: '{"success":true}' });
    }
  });
}

test.describe('@audio Phase 6: DSP Drawer and Overlays', () => {

  // ===== Input DSP button =====

  test('input channel strip has DSP button', async ({ connectedPage: page }) => {
    await goToInputs(page);
    await openFirstInputDrawer(page);
    const container = page.locator('#audio-inputs-container');
    const dspBtn = container.locator('[data-action="open-input-peq"]').first();
    await expect(dspBtn).toBeVisible();
  });

  test('input DSP button opens PEQ overlay', async ({ connectedPage: page }) => {
    await goToInputs(page);
    await openFirstInputDrawer(page);
    const container = page.locator('#audio-inputs-container');
    const dspBtn = container.locator('[data-action="open-input-peq"]').first();
    await dspBtn.click();

    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });
    await expect(overlay.locator('canvas')).toBeVisible({ timeout: 2000 });
  });

  // ===== Output PEQ =====

  test('output channel strip has PEQ button', async ({ connectedPage: page }) => {
    await goToOutputs(page);
    await openFirstOutputDrawer(page);
    const container = page.locator('#audio-outputs-container');
    const peqBtn = container.locator('[data-action="open-output-peq"]').first();
    await expect(peqBtn).toBeVisible();
  });

  test('output PEQ button opens overlay with canvas', async ({ connectedPage: page }) => {
    await goToOutputs(page);
    await mockOutputDsp(page);
    await openFirstOutputDrawer(page);

    const container = page.locator('#audio-outputs-container');
    const peqBtn = container.locator('[data-action="open-output-peq"]').first();
    await peqBtn.click();

    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });
    await expect(overlay.locator('canvas')).toBeVisible({ timeout: 2000 });
  });

  // ===== Crossover overlay =====

  test('output channel strip has Crossover button', async ({ connectedPage: page }) => {
    await goToOutputs(page);
    await openFirstOutputDrawer(page);
    const container = page.locator('#audio-outputs-container');
    const xoBtn = container.locator('[data-action="open-output-crossover"]').first();
    await expect(xoBtn).toBeVisible();
  });

  test('Crossover button opens overlay with type and frequency controls', async ({ connectedPage: page }) => {
    await goToOutputs(page);
    await openFirstOutputDrawer(page);

    const container = page.locator('#audio-outputs-container');
    const xoBtn = container.locator('[data-action="open-output-crossover"]').first();
    await xoBtn.click();

    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    // Type select should be visible
    await expect(overlay.locator('#xoverType')).toBeVisible();
    // Frequency input should be visible
    await expect(overlay.locator('#xoverFreq')).toBeVisible();
    // Apply button
    await expect(overlay.locator('[data-action="xover-apply"]')).toBeVisible();
  });

  test('Crossover Apply sends POST to /api/output/dsp/crossover with freq and order', async ({ connectedPage: page }) => {
    await goToOutputs(page);
    await openFirstOutputDrawer(page);

    let capturedBody = null;
    await page.route('**/api/v1/output/dsp/crossover', async (route) => {
      if (route.request().method() === 'POST') {
        capturedBody = route.request().postDataJSON();
      }
      await route.fulfill({ status: 200, contentType: 'application/json', body: '{"success":true}' });
    });

    const container = page.locator('#audio-outputs-container');
    const xoBtn = container.locator('[data-action="open-output-crossover"]').first();
    await xoBtn.click();

    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    // Set frequency to 120 Hz
    await overlay.locator('#xoverFreq').fill('120');
    // Apply
    await overlay.locator('[data-action="xover-apply"]').click();

    await page.waitForTimeout(400);
    expect(capturedBody).not.toBeNull();
    expect(capturedBody.freqHz).toBe(120);
    expect(typeof capturedBody.order).toBe('number');
    expect(capturedBody.order).toBeGreaterThan(0);
  });

  test('Crossover overlay close button hides overlay', async ({ connectedPage: page }) => {
    await goToOutputs(page);
    await openFirstOutputDrawer(page);

    const container = page.locator('#audio-outputs-container');
    const xoBtn = container.locator('[data-action="open-output-crossover"]').first();
    await xoBtn.click();

    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    await overlay.locator('[data-action="peq-close"]').first().click();
    await expect(overlay).toBeHidden({ timeout: 2000 });
  });

  // ===== Compressor overlay =====

  test('output channel strip has Compressor button', async ({ connectedPage: page }) => {
    await goToOutputs(page);
    await openFirstOutputDrawer(page);
    const container = page.locator('#audio-outputs-container');
    const compBtn = container.locator('[data-action="open-output-compressor"]').first();
    await expect(compBtn).toBeVisible();
  });

  test('Compressor button opens overlay with all 6 parameter sliders', async ({ connectedPage: page }) => {
    await goToOutputs(page);
    await openFirstOutputDrawer(page);

    const container = page.locator('#audio-outputs-container');
    const compBtn = container.locator('[data-action="open-output-compressor"]').first();
    await compBtn.click();

    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    // All 6 compressor sliders
    await expect(overlay.locator('#compThreshold')).toBeVisible();
    await expect(overlay.locator('#compRatio')).toBeVisible();
    await expect(overlay.locator('#compAttack')).toBeVisible();
    await expect(overlay.locator('#compRelease')).toBeVisible();
    await expect(overlay.locator('#compKnee')).toBeVisible();
    await expect(overlay.locator('#compMakeup')).toBeVisible();
    // Apply button
    await expect(overlay.locator('[data-action="compressor-apply"]')).toBeVisible();
  });

  test('Compressor Apply sends all 6 params to /api/output/dsp/stage', async ({ connectedPage: page }) => {
    await goToOutputs(page);
    await openFirstOutputDrawer(page);

    let capturedBody = null;
    await page.route('**/api/v1/output/dsp/stage', async (route) => {
      if (route.request().method() === 'POST') {
        capturedBody = route.request().postDataJSON();
      }
      await route.fulfill({ status: 200, contentType: 'application/json', body: '{"success":true,"id":1}' });
    });

    const container = page.locator('#audio-outputs-container');
    const compBtn = container.locator('[data-action="open-output-compressor"]').first();
    await compBtn.click();

    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    await overlay.locator('[data-action="compressor-apply"]').click();

    await page.waitForTimeout(400);
    expect(capturedBody).not.toBeNull();
    // Must include all 6 compressor params (regression: was type-only before Phase 6 fix)
    expect(capturedBody.thresholdDb).toBeDefined();
    expect(capturedBody.ratio).toBeDefined();
    expect(capturedBody.attackMs).toBeDefined();
    expect(capturedBody.releaseMs).toBeDefined();
    expect(capturedBody.kneeDb).toBeDefined();
    expect(capturedBody.makeupGainDb).toBeDefined();
  });

  test('Compressor Apply POST includes type field', async ({ connectedPage: page }) => {
    await goToOutputs(page);
    await openFirstOutputDrawer(page);

    let capturedBody = null;
    await page.route('**/api/v1/output/dsp/stage', async (route) => {
      if (route.request().method() === 'POST') {
        capturedBody = route.request().postDataJSON();
      }
      await route.fulfill({ status: 200, contentType: 'application/json', body: '{"success":true,"id":1}' });
    });

    const container = page.locator('#audio-outputs-container');
    const compBtn = container.locator('[data-action="open-output-compressor"]').first();
    await compBtn.click();

    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });
    await overlay.locator('[data-action="compressor-apply"]').click();

    await page.waitForTimeout(400);
    expect(capturedBody).not.toBeNull();
    expect(capturedBody.type).toBe('COMPRESSOR');
  });

  // ===== Limiter overlay =====

  test('output channel strip has Limiter button', async ({ connectedPage: page }) => {
    await goToOutputs(page);
    await openFirstOutputDrawer(page);
    const container = page.locator('#audio-outputs-container');
    const limBtn = container.locator('[data-action="open-output-limiter"]').first();
    await expect(limBtn).toBeVisible();
  });

  test('Limiter button opens overlay with 3 parameter sliders', async ({ connectedPage: page }) => {
    await goToOutputs(page);
    await openFirstOutputDrawer(page);

    const container = page.locator('#audio-outputs-container');
    const limBtn = container.locator('[data-action="open-output-limiter"]').first();
    await limBtn.click();

    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    await expect(overlay.locator('#limThreshold')).toBeVisible();
    await expect(overlay.locator('#limAttack')).toBeVisible();
    await expect(overlay.locator('#limRelease')).toBeVisible();
    await expect(overlay.locator('[data-action="limiter-apply"]')).toBeVisible();
  });

  test('Limiter Apply sends all 3 params to /api/output/dsp/stage', async ({ connectedPage: page }) => {
    await goToOutputs(page);
    await openFirstOutputDrawer(page);

    let capturedBody = null;
    await page.route('**/api/v1/output/dsp/stage', async (route) => {
      if (route.request().method() === 'POST') {
        capturedBody = route.request().postDataJSON();
      }
      await route.fulfill({ status: 200, contentType: 'application/json', body: '{"success":true,"id":2}' });
    });

    const container = page.locator('#audio-outputs-container');
    const limBtn = container.locator('[data-action="open-output-limiter"]').first();
    await limBtn.click();

    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });
    await overlay.locator('[data-action="limiter-apply"]').click();

    await page.waitForTimeout(400);
    expect(capturedBody).not.toBeNull();
    expect(capturedBody.type).toBe('LIMITER');
    expect(capturedBody.thresholdDb).toBeDefined();
    expect(capturedBody.attackMs).toBeDefined();
    expect(capturedBody.releaseMs).toBeDefined();
  });

  test('Limiter Apply POST default threshold is -3 dBFS', async ({ connectedPage: page }) => {
    await goToOutputs(page);
    await openFirstOutputDrawer(page);

    let capturedBody = null;
    await page.route('**/api/v1/output/dsp/stage', async (route) => {
      if (route.request().method() === 'POST') {
        capturedBody = route.request().postDataJSON();
      }
      await route.fulfill({ status: 200, contentType: 'application/json', body: '{"success":true,"id":2}' });
    });

    const container = page.locator('#audio-outputs-container');
    const limBtn = container.locator('[data-action="open-output-limiter"]').first();
    await limBtn.click();

    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });
    await overlay.locator('[data-action="limiter-apply"]').click();

    await page.waitForTimeout(400);
    expect(capturedBody).not.toBeNull();
    // Default threshold from peqControlRow is -3
    expect(capturedBody.thresholdDb).toBe(-3);
  });

  // ===== Shared overlay container =====

  test('all DSP overlays reuse the same #peqOverlay element', async ({ connectedPage: page }) => {
    await goToOutputs(page);
    await openFirstOutputDrawer(page);

    // Open crossover
    const container = page.locator('#audio-outputs-container');
    await container.locator('[data-action="open-output-crossover"]').first().click();
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });
    const overlayCount1 = await page.locator('#peqOverlay').count();
    await overlay.locator('[data-action="peq-close"]').first().click();
    await expect(overlay).toBeHidden({ timeout: 2000 });

    // Open compressor
    await container.locator('[data-action="open-output-compressor"]').first().click();
    await expect(overlay).toBeVisible({ timeout: 3000 });
    const overlayCount2 = await page.locator('#peqOverlay').count();
    await overlay.locator('[data-action="peq-close"]').first().click();
    await expect(overlay).toBeHidden({ timeout: 2000 });

    // Exactly one #peqOverlay element in DOM at all times
    expect(overlayCount1).toBe(1);
    expect(overlayCount2).toBe(1);
  });
});
