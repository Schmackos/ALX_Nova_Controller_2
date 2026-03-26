/**
 * dsp-drawer.spec.js — Phase 6: DSP Drawer + advanced overlays.
 *
 * Tests for:
 * - DSP summary line on input/output channel strips (collapsible drawer)
 * - Drawer toggle behavior (open/close)
 * - Input add-stage dropdown (Common + Advanced groups)
 * - New overlay types: Noise Gate, Linkwitz Transform, FIR upload, WAV IR, Multi-band Comp, Custom Biquad
 * - Input Compressor and Limiter overlays via drawer add-stage
 */

const { test, expect } = require('../helpers/fixtures');

async function goToInputs(page) {
  await page.locator('.sidebar-item[data-tab="audio"]').click();
  await expect(page.locator('#audio-sv-inputs')).toHaveClass(/active/);
  await expect(page.locator('#audio-inputs-container')).not.toContainText(
    'Waiting for device data...', { timeout: 5000 }
  );
}

async function goToOutputs(page) {
  await page.locator('.sidebar-item[data-tab="audio"]').click();
  await page.locator('.audio-subnav-btn[data-view="outputs"]').click();
  await expect(page.locator('#audio-sv-outputs')).toHaveClass(/active/);
  await expect(page.locator('#audio-outputs-container')).not.toContainText(
    'Waiting for device data...', { timeout: 5000 }
  );
}

test.describe('@audio Phase 6: DSP Drawer', () => {

  // ===== Input DSP summary line =====

  test('input channel strip has DSP summary line', async ({ connectedPage: page }) => {
    await goToInputs(page);
    const container = page.locator('#audio-inputs-container');
    const summary = container.locator('[data-action="toggle-input-dsp-drawer"]').first();
    await expect(summary).toBeVisible();
  });

  test('input DSP summary text is visible', async ({ connectedPage: page }) => {
    await goToInputs(page);
    const container = page.locator('#audio-inputs-container');
    const summaryText = container.locator('.dsp-summary-text').first();
    await expect(summaryText).toBeVisible();
  });

  test('input DSP drawer is collapsed by default', async ({ connectedPage: page }) => {
    await goToInputs(page);
    const container = page.locator('#audio-inputs-container');
    // The drawer div starts with display:none
    const drawerSection = container.locator('.channel-dsp-section').first();
    await expect(drawerSection).toBeVisible();
    // open-input-peq button inside drawer should not be visible initially
    const peqBtn = container.locator('[data-action="open-input-peq"]').first();
    await expect(peqBtn).toBeHidden();
  });

  test('clicking input DSP summary opens the drawer', async ({ connectedPage: page }) => {
    await goToInputs(page);
    const container = page.locator('#audio-inputs-container');
    const summary = container.locator('[data-action="toggle-input-dsp-drawer"]').first();
    await summary.click();

    // After click, the PEQ button inside the drawer should be visible
    const peqBtn = container.locator('[data-action="open-input-peq"]').first();
    await expect(peqBtn).toBeVisible({ timeout: 2000 });
  });

  test('clicking input DSP summary twice closes the drawer', async ({ connectedPage: page }) => {
    await goToInputs(page);
    const container = page.locator('#audio-inputs-container');
    const summary = container.locator('[data-action="toggle-input-dsp-drawer"]').first();
    await summary.click();
    await summary.click();

    const peqBtn = container.locator('[data-action="open-input-peq"]').first();
    await expect(peqBtn).toBeHidden({ timeout: 2000 });
  });

  // ===== Input add-stage dropdown =====

  test('input drawer has Add Stage button', async ({ connectedPage: page }) => {
    await goToInputs(page);
    const container = page.locator('#audio-inputs-container');
    await container.locator('[data-action="toggle-input-dsp-drawer"]').first().click();

    const addBtn = container.locator('[data-action="toggle-input-add-stage"]').first();
    await expect(addBtn).toBeVisible({ timeout: 2000 });
  });

  test('Add Stage dropdown shows Common and Advanced sections', async ({ connectedPage: page }) => {
    await goToInputs(page);
    const container = page.locator('#audio-inputs-container');
    await container.locator('[data-action="toggle-input-dsp-drawer"]').first().click();
    await container.locator('[data-action="toggle-input-add-stage"]').first().click();

    const menu = container.locator('.dsp-add-stage-menu').first();
    await expect(menu).toBeVisible({ timeout: 2000 });

    const groupLabels = menu.locator('.dsp-add-stage-group-label');
    await expect(groupLabels).toHaveCount(2);

    // Common section
    const noiseGateItem = menu.locator('[data-action="add-input-stage"][data-type="noise-gate"]');
    await expect(noiseGateItem).toBeVisible();

    // Advanced section
    const linkwitzItem = menu.locator('[data-action="add-input-stage"][data-type="linkwitz"]');
    await expect(linkwitzItem).toBeVisible();
  });

  test('Add Stage dropdown has all expected stage types', async ({ connectedPage: page }) => {
    await goToInputs(page);
    const container = page.locator('#audio-inputs-container');
    await container.locator('[data-action="toggle-input-dsp-drawer"]').first().click();
    await container.locator('[data-action="toggle-input-add-stage"]').first().click();

    const menu = container.locator('.dsp-add-stage-menu').first();
    await expect(menu.locator('[data-type="compressor"]')).toBeVisible();
    await expect(menu.locator('[data-type="limiter"]')).toBeVisible();
    await expect(menu.locator('[data-type="noise-gate"]')).toBeVisible();
    await expect(menu.locator('[data-type="fir"]')).toBeVisible();
    await expect(menu.locator('[data-type="wav-ir"]')).toBeVisible();
    await expect(menu.locator('[data-type="linkwitz"]')).toBeVisible();
    await expect(menu.locator('[data-type="multiband"]')).toBeVisible();
    await expect(menu.locator('[data-type="biquad"]')).toBeVisible();
  });

  // ===== Output DSP summary line =====

  test('output channel strip has DSP summary line', async ({ connectedPage: page }) => {
    await goToOutputs(page);
    const container = page.locator('#audio-outputs-container');
    const summary = container.locator('[data-action="toggle-output-dsp-drawer"]').first();
    await expect(summary).toBeVisible();
  });

  test('output DSP drawer is collapsed by default', async ({ connectedPage: page }) => {
    await goToOutputs(page);
    const container = page.locator('#audio-outputs-container');
    const peqBtn = container.locator('[data-action="open-output-peq"]').first();
    await expect(peqBtn).toBeHidden();
  });

  test('clicking output DSP summary opens the drawer', async ({ connectedPage: page }) => {
    await goToOutputs(page);
    const container = page.locator('#audio-outputs-container');
    await container.locator('[data-action="toggle-output-dsp-drawer"]').first().click();

    const peqBtn = container.locator('[data-action="open-output-peq"]').first();
    await expect(peqBtn).toBeVisible({ timeout: 2000 });
  });

  // ===== Noise Gate overlay =====

  test('Noise Gate overlay opens with 5 parameter sliders', async ({ connectedPage: page }) => {
    await goToInputs(page);
    const container = page.locator('#audio-inputs-container');
    await container.locator('[data-action="toggle-input-dsp-drawer"]').first().click();
    await container.locator('[data-action="toggle-input-add-stage"]').first().click();
    await container.locator('[data-type="noise-gate"]').first().click();

    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });
    await expect(overlay.locator('#ngThreshold')).toBeVisible();
    await expect(overlay.locator('#ngAttack')).toBeVisible();
    await expect(overlay.locator('#ngHold')).toBeVisible();
    await expect(overlay.locator('#ngRelease')).toBeVisible();
    await expect(overlay.locator('#ngRange')).toBeVisible();
    await expect(overlay.locator('[data-action="noise-gate-apply"]')).toBeVisible();
  });

  test('Noise Gate Apply sends POST to /api/dsp/stage with type NOISE_GATE', async ({ connectedPage: page }) => {
    await goToInputs(page);

    let capturedBody = null;
    await page.route('**/api/v1/dsp/stage', async (route) => {
      if (route.request().method() === 'POST') {
        capturedBody = route.request().postDataJSON();
      }
      await route.fulfill({ status: 200, contentType: 'application/json', body: '{"success":true,"id":10}' });
    });

    const container = page.locator('#audio-inputs-container');
    await container.locator('[data-action="toggle-input-dsp-drawer"]').first().click();
    await container.locator('[data-action="toggle-input-add-stage"]').first().click();
    await container.locator('[data-type="noise-gate"]').first().click();

    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });
    await overlay.locator('[data-action="noise-gate-apply"]').click();

    await page.waitForTimeout(400);
    expect(capturedBody).not.toBeNull();
    expect(capturedBody.type).toBe('NOISE_GATE');
    expect(capturedBody.params).toBeDefined();
    expect(capturedBody.params.thresholdDb).toBeDefined();
    expect(capturedBody.params.attackMs).toBeDefined();
    expect(capturedBody.params.releaseMs).toBeDefined();
  });

  // ===== Linkwitz Transform overlay =====

  test('Linkwitz Transform overlay opens with 4 parameter sliders', async ({ connectedPage: page }) => {
    await goToInputs(page);
    const container = page.locator('#audio-inputs-container');
    await container.locator('[data-action="toggle-input-dsp-drawer"]').first().click();
    await container.locator('[data-action="toggle-input-add-stage"]').first().click();
    await container.locator('[data-type="linkwitz"]').first().click();

    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });
    await expect(overlay.locator('#lkF0')).toBeVisible();
    await expect(overlay.locator('#lkQ0')).toBeVisible();
    await expect(overlay.locator('#lkFp')).toBeVisible();
    await expect(overlay.locator('#lkQp')).toBeVisible();
    await expect(overlay.locator('[data-action="linkwitz-apply"]')).toBeVisible();
  });

  test('Linkwitz Apply sends POST with type LINKWITZ_TRANSFORM', async ({ connectedPage: page }) => {
    await goToInputs(page);

    let capturedBody = null;
    await page.route('**/api/v1/dsp/stage', async (route) => {
      if (route.request().method() === 'POST') {
        capturedBody = route.request().postDataJSON();
      }
      await route.fulfill({ status: 200, contentType: 'application/json', body: '{"success":true,"id":11}' });
    });

    const container = page.locator('#audio-inputs-container');
    await container.locator('[data-action="toggle-input-dsp-drawer"]').first().click();
    await container.locator('[data-action="toggle-input-add-stage"]').first().click();
    await container.locator('[data-type="linkwitz"]').first().click();

    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });
    await overlay.locator('[data-action="linkwitz-apply"]').click();

    await page.waitForTimeout(400);
    expect(capturedBody).not.toBeNull();
    expect(capturedBody.type).toBe('LINKWITZ_TRANSFORM');
    expect(capturedBody.params.f0Hz).toBeDefined();
    expect(capturedBody.params.fpHz).toBeDefined();
  });

  // ===== Custom Biquad overlay =====

  test('Custom Biquad overlay opens with 5 coefficient inputs', async ({ connectedPage: page }) => {
    await goToInputs(page);
    const container = page.locator('#audio-inputs-container');
    await container.locator('[data-action="toggle-input-dsp-drawer"]').first().click();
    await container.locator('[data-action="toggle-input-add-stage"]').first().click();
    await container.locator('[data-type="biquad"]').first().click();

    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });
    await expect(overlay.locator('#bqB0')).toBeVisible();
    await expect(overlay.locator('#bqB1')).toBeVisible();
    await expect(overlay.locator('#bqB2')).toBeVisible();
    await expect(overlay.locator('#bqA1')).toBeVisible();
    await expect(overlay.locator('#bqA2')).toBeVisible();
    await expect(overlay.locator('[data-action="biquad-apply"]')).toBeVisible();
  });

  test('Custom Biquad Apply sends POST with type CUSTOM_BIQUAD and all 5 coefficients', async ({ connectedPage: page }) => {
    await goToInputs(page);

    let capturedBody = null;
    await page.route('**/api/v1/dsp/stage', async (route) => {
      if (route.request().method() === 'POST') {
        capturedBody = route.request().postDataJSON();
      }
      await route.fulfill({ status: 200, contentType: 'application/json', body: '{"success":true,"id":12}' });
    });

    const container = page.locator('#audio-inputs-container');
    await container.locator('[data-action="toggle-input-dsp-drawer"]').first().click();
    await container.locator('[data-action="toggle-input-add-stage"]').first().click();
    await container.locator('[data-type="biquad"]').first().click();

    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });
    await overlay.locator('[data-action="biquad-apply"]').click();

    await page.waitForTimeout(400);
    expect(capturedBody).not.toBeNull();
    expect(capturedBody.type).toBe('CUSTOM_BIQUAD');
    expect(capturedBody.params.b0).toBeDefined();
    expect(capturedBody.params.b1).toBeDefined();
    expect(capturedBody.params.b2).toBeDefined();
    expect(capturedBody.params.a1).toBeDefined();
    expect(capturedBody.params.a2).toBeDefined();
  });

  // ===== Multi-band Comp overlay =====

  test('Multi-band Comp overlay opens with band count selector', async ({ connectedPage: page }) => {
    await goToInputs(page);
    const container = page.locator('#audio-inputs-container');
    await container.locator('[data-action="toggle-input-dsp-drawer"]').first().click();
    await container.locator('[data-action="toggle-input-add-stage"]').first().click();
    await container.locator('[data-type="multiband"]').first().click();

    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });
    await expect(overlay.locator('#mbcNumBands')).toBeVisible();
    await expect(overlay.locator('[data-action="multiband-apply"]')).toBeVisible();
  });

  test('Multi-band Comp Apply sends POST with type MULTIBAND_COMP', async ({ connectedPage: page }) => {
    await goToInputs(page);

    let capturedBody = null;
    await page.route('**/api/v1/dsp/stage', async (route) => {
      if (route.request().method() === 'POST') {
        capturedBody = route.request().postDataJSON();
      }
      await route.fulfill({ status: 200, contentType: 'application/json', body: '{"success":true,"id":13}' });
    });

    const container = page.locator('#audio-inputs-container');
    await container.locator('[data-action="toggle-input-dsp-drawer"]').first().click();
    await container.locator('[data-action="toggle-input-add-stage"]').first().click();
    await container.locator('[data-type="multiband"]').first().click();

    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });
    await overlay.locator('[data-action="multiband-apply"]').click();

    await page.waitForTimeout(400);
    expect(capturedBody).not.toBeNull();
    expect(capturedBody.type).toBe('MULTIBAND_COMP');
    expect(capturedBody.params.numBands).toBeDefined();
  });

  // ===== FIR Upload overlay =====

  test('FIR Upload overlay opens with file picker', async ({ connectedPage: page }) => {
    await goToInputs(page);
    const container = page.locator('#audio-inputs-container');
    await container.locator('[data-action="toggle-input-dsp-drawer"]').first().click();
    await container.locator('[data-action="toggle-input-add-stage"]').first().click();
    await container.locator('[data-type="fir"]').first().click();

    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });
    // File input is hidden but the label button is visible
    await expect(overlay.locator('[data-action="fir-apply"]')).toBeVisible();
    await expect(overlay.locator('#firFileName')).toBeVisible();
  });

  // ===== WAV IR Upload overlay =====

  test('WAV IR Upload overlay opens with file picker', async ({ connectedPage: page }) => {
    await goToInputs(page);
    const container = page.locator('#audio-inputs-container');
    await container.locator('[data-action="toggle-input-dsp-drawer"]').first().click();
    await container.locator('[data-action="toggle-input-add-stage"]').first().click();
    await container.locator('[data-type="wav-ir"]').first().click();

    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });
    await expect(overlay.locator('[data-action="wav-ir-apply"]')).toBeVisible();
    await expect(overlay.locator('#wavIrFileName')).toBeVisible();
  });

  // ===== Input Compressor via drawer =====

  test('Input Compressor via add-stage dropdown opens overlay with 6 sliders', async ({ connectedPage: page }) => {
    await goToInputs(page);
    const container = page.locator('#audio-inputs-container');
    await container.locator('[data-action="toggle-input-dsp-drawer"]').first().click();
    await container.locator('[data-action="toggle-input-add-stage"]').first().click();
    await container.locator('[data-type="compressor"]').first().click();

    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });
    await expect(overlay.locator('#compThreshold')).toBeVisible();
    await expect(overlay.locator('#compRatio')).toBeVisible();
    await expect(overlay.locator('#compAttack')).toBeVisible();
    await expect(overlay.locator('#compRelease')).toBeVisible();
    await expect(overlay.locator('#compKnee')).toBeVisible();
    await expect(overlay.locator('#compMakeup')).toBeVisible();
  });

  // ===== Input Limiter via drawer =====

  test('Input Limiter via add-stage dropdown opens overlay with 3 sliders', async ({ connectedPage: page }) => {
    await goToInputs(page);
    const container = page.locator('#audio-inputs-container');
    await container.locator('[data-action="toggle-input-dsp-drawer"]').first().click();
    await container.locator('[data-action="toggle-input-add-stage"]').first().click();
    await container.locator('[data-type="limiter"]').first().click();

    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });
    await expect(overlay.locator('#limThreshold')).toBeVisible();
    await expect(overlay.locator('#limAttack')).toBeVisible();
    await expect(overlay.locator('#limRelease')).toBeVisible();
  });

  // ===== All overlays close =====

  test('all new overlays use the same #peqOverlay element', async ({ connectedPage: page }) => {
    await goToInputs(page);

    const container = page.locator('#audio-inputs-container');
    await container.locator('[data-action="toggle-input-dsp-drawer"]').first().click();

    async function openAndClose(type) {
      await container.locator('[data-action="toggle-input-add-stage"]').first().click();
      await container.locator('[data-type="' + type + '"]').first().click();
      const overlay = page.locator('#peqOverlay');
      await expect(overlay).toBeVisible({ timeout: 3000 });
      const count = await page.locator('#peqOverlay').count();
      expect(count).toBe(1);
      await overlay.locator('[data-action="peq-close"]').first().click();
      await expect(overlay).toBeHidden({ timeout: 2000 });
    }

    await openAndClose('noise-gate');
    await openAndClose('linkwitz');
    await openAndClose('biquad');
  });
});
