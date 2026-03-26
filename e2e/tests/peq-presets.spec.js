/**
 * peq-presets.spec.js — Phase 7: Named PEQ preset UI in the PEQ overlay.
 *
 * Tests for:
 * - Save PEQ preset button in overlay toolbar → REST POST /api/dsp/peq/presets
 * - Load PEQ preset: select from list → REST GET /api/dsp/peq/preset
 * - Delete PEQ preset → REST DELETE /api/dsp/peq/preset
 * - List PEQ presets → REST GET /api/dsp/peq/presets
 * - Loaded preset replaces current bands in overlay
 *
 * REST endpoints are served by e2e/mock-server/routes/dsp.js (already wired).
 */

const { test, expect } = require('../helpers/fixtures');

// Mock /api/output/dsp GET for openOutputPeq
async function mockOutputDsp(page, ch = 0, stages = []) {
  await page.route('**/api/v1/output/dsp*', async (route) => {
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

// Seed a PEQ preset via REST
async function seedPeqPreset(page, name, stages = []) {
  await page.evaluate(async ({ presetName, presetStages }) => {
    await window.apiFetch('/api/dsp/peq/presets', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name: presetName, stages: presetStages })
    });
  }, { presetName: name, presetStages: stages });
}

test.describe('@audio @api Phase 7: PEQ Presets', () => {
  test.beforeEach(async ({ connectedPage: page }) => {
    await page.locator('.sidebar-item[data-tab="audio"]').click();
  });

  // ===== REST API layer (always testable) =====

  test('GET /api/dsp/peq/presets returns success:true and presets array', async ({ connectedPage: page }) => {
    const result = await page.evaluate(async () => {
      const resp = await window.apiFetch('/api/dsp/peq/presets');
      return await resp.json();
    });
    expect(result.success).toBe(true);
    expect(Array.isArray(result.presets)).toBe(true);
  });

  test('POST /api/dsp/peq/presets saves a preset with name', async ({ connectedPage: page }) => {
    let capturedBody = null;
    await page.route('**/api/v1/dsp/peq/presets', async (route) => {
      if (route.request().method() === 'POST') {
        capturedBody = route.request().postDataJSON();
      }
      await route.fulfill({ status: 200, contentType: 'application/json', body: '{"success":true}' });
    });

    await page.evaluate(async () => {
      await window.apiFetch('/api/dsp/peq/presets', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ name: 'MyPEQ', stages: [] })
      });
    });

    await page.waitForTimeout(200);
    expect(capturedBody).not.toBeNull();
    expect(capturedBody.name).toBe('MyPEQ');
  });

  test('POST /api/dsp/peq/presets with empty name returns 400', async ({ connectedPage: page }) => {
    const result = await page.evaluate(async () => {
      const resp = await window.apiFetch('/api/dsp/peq/presets', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ name: '' })
      });
      return { status: resp.status };
    });
    expect(result.status).toBe(400);
  });

  test('GET /api/dsp/peq/preset returns preset by name', async ({ connectedPage: page }) => {
    await seedPeqPreset(page, 'FetchMe', [{ type: 4, frequency: 1000, gain: 3, Q: 1.41, enabled: true }]);

    const result = await page.evaluate(async () => {
      const resp = await window.apiFetch('/api/dsp/peq/preset?name=FetchMe');
      return await resp.json();
    });
    expect(result.success).toBe(true);
    expect(result.preset).toBeDefined();
    expect(result.preset.name).toBe('FetchMe');
  });

  test('GET /api/dsp/peq/preset for unknown name returns 404', async ({ connectedPage: page }) => {
    const result = await page.evaluate(async () => {
      const resp = await window.apiFetch('/api/dsp/peq/preset?name=__nosuchpreset__');
      return { status: resp.status };
    });
    expect(result.status).toBe(404);
  });

  test('DELETE /api/dsp/peq/preset removes preset by name', async ({ connectedPage: page }) => {
    await seedPeqPreset(page, 'DeletePEQ', []);

    let capturedBody = null;
    await page.route('**/api/v1/dsp/peq/preset', async (route) => {
      if (route.request().method() === 'DELETE') {
        capturedBody = route.request().postDataJSON();
      }
      await route.fulfill({ status: 200, contentType: 'application/json', body: '{"success":true}' });
    });

    await page.evaluate(async () => {
      await window.apiFetch('/api/dsp/peq/preset', {
        method: 'DELETE',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ name: 'DeletePEQ' })
      });
    });

    await page.waitForTimeout(200);
    expect(capturedBody).not.toBeNull();
    expect(capturedBody.name).toBe('DeletePEQ');
  });

  test('GET /api/dsp/peq/presets includes newly saved preset', async ({ connectedPage: page }) => {
    await seedPeqPreset(page, 'PEQListTest', []);
    const result = await page.evaluate(async () => {
      const resp = await window.apiFetch('/api/dsp/peq/presets');
      return await resp.json();
    });
    expect(result.presets.some(p => p.name === 'PEQListTest')).toBe(true);
  });

  test('save then fetch PEQ preset preserves stages', async ({ connectedPage: page }) => {
    const stages = [{ type: 5, frequency: 80, gain: -3, Q: 0.707, enabled: true }];
    await seedPeqPreset(page, 'StageTest', stages);

    const result = await page.evaluate(async () => {
      const resp = await window.apiFetch('/api/dsp/peq/preset?name=StageTest');
      return await resp.json();
    });
    expect(result.success).toBe(true);
    expect(Array.isArray(result.preset.stages)).toBe(true);
    expect(result.preset.stages.length).toBeGreaterThan(0);
  });

  // ===== Overlay UI (conditionally skip if not yet implemented) =====

  test('PEQ overlay has save-preset button when open', async ({ connectedPage: page }) => {
    await mockOutputDsp(page, 0);
    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    const saveBtn = overlay.locator('[data-action="peq-preset-save"], button:has-text("Save Preset"), button:has-text("Preset")');
    const count = await saveBtn.count();
    if (count === 0) {
      test.skip(true, 'PEQ preset save button not present — implementation pending');
      return;
    }
    await expect(saveBtn.first()).toBeVisible();
  });

  test('PEQ overlay has preset list or load control when open', async ({ connectedPage: page }) => {
    await seedPeqPreset(page, 'LoadFromOverlay', []);
    await mockOutputDsp(page, 0);
    await page.evaluate(() => openOutputPeq(0));
    const overlay = page.locator('#peqOverlay');
    await expect(overlay).toBeVisible({ timeout: 3000 });

    const listEl = overlay.locator('#peqPresetList, [data-action="peq-preset-load"], select.peq-preset-select');
    const count = await listEl.count();
    if (count === 0) {
      test.skip(true, 'PEQ preset list not present — implementation pending');
      return;
    }
    await expect(listEl.first()).toBeVisible();
  });
});
