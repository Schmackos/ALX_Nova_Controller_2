/**
 * audio-batch-e-presets.spec.js — DSP config presets + PEQ presets overlay.
 *
 * Tests for:
 *  - Presets button visible in audio subnav
 *  - DSP preset overlay opens / closes
 *  - Tab switching (DSP Configs / PEQ Presets)
 *  - Save / Load / Delete / Rename DSP config preset via REST mock
 *  - PEQ preset save from overlay via REST mock
 */

const { test, expect } = require('../helpers/fixtures');

// Helper: open audio tab and click Presets button
async function openPresetsOverlay(page) {
  await page.evaluate(() => switchTab('audio'));
  await expect(page.locator('#audio')).toBeVisible({ timeout: 5000 });
  await page.evaluate(() => openDspPresetOverlay());
  await expect(page.locator('#dspPresetOverlay')).toBeVisible({ timeout: 3000 });
}

test.describe('@audio DSP Config + PEQ Presets overlay', () => {
  test('Presets button is visible in audio subnav', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('audio'));
    await expect(page.locator('#audio')).toBeVisible({ timeout: 5000 });
    await expect(page.locator('#audioPresetsBtn')).toBeVisible({ timeout: 3000 });
  });

  test('Presets button opens overlay', async ({ connectedPage: page }) => {
    await page.evaluate(() => switchTab('audio'));
    await expect(page.locator('#audio')).toBeVisible({ timeout: 5000 });
    await page.locator('#audioPresetsBtn').click();
    await expect(page.locator('#dspPresetOverlay')).toBeVisible({ timeout: 3000 });
  });

  test('Overlay has two tabs: DSP Configs and PEQ Presets', async ({ connectedPage: page }) => {
    await openPresetsOverlay(page);
    const overlay = page.locator('#dspPresetOverlay');
    await expect(overlay.locator('#dspPresetTabDsp')).toBeVisible({ timeout: 2000 });
    await expect(overlay.locator('#dspPresetTabPeq')).toBeVisible({ timeout: 2000 });
  });

  test('Close button dismisses overlay', async ({ connectedPage: page }) => {
    await openPresetsOverlay(page);
    // Close button is in the footer
    await page.locator('#dspPresetOverlay .dsp-preset-footer button').click();
    await expect(page.locator('#dspPresetOverlay')).toBeHidden({ timeout: 2000 });
  });

  test('DSP tab shows Save Current Config button', async ({ connectedPage: page }) => {
    await openPresetsOverlay(page);
    await expect(page.locator('#dspPresetOverlay')).toContainText('Save Current Config', { timeout: 3000 });
  });

  test('Save DSP preset calls REST endpoint', async ({ connectedPage: page }) => {
    await openPresetsOverlay(page);
    // Intercept the save REST call
    const savePromise = page.waitForRequest(req => req.url().includes('/api/v1/dsp/presets/save'));
    // Trigger save via evaluate (prompts for name — bypass with direct API call)
    const resp = await page.evaluate(() => apiFetch('/api/dsp/presets/save?slot=0', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name: 'Test Preset' })
    }).then(r => r.json()));
    expect(resp.success).toBe(true);
  });

  test('Load DSP preset calls REST endpoint', async ({ connectedPage: page }) => {
    // First save a preset
    await page.evaluate(() => apiFetch('/api/dsp/presets/save?slot=0', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name: 'LoadTest' })
    }));
    // Then load it
    const resp = await page.evaluate(() => apiFetch('/api/dsp/presets/load?slot=0', {
      method: 'POST'
    }).then(r => r.json()));
    expect(resp.success).toBe(true);
  });

  test('Delete DSP preset calls REST endpoint', async ({ connectedPage: page }) => {
    // Save first
    await page.evaluate(() => apiFetch('/api/dsp/presets/save?slot=1', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name: 'DeleteMe' })
    }));
    // Delete
    const resp = await page.evaluate(() => apiFetch('/api/dsp/presets?slot=1', {
      method: 'DELETE'
    }).then(r => r.json()));
    expect(resp.success).toBe(true);
  });

  test('Rename DSP preset calls REST endpoint', async ({ connectedPage: page }) => {
    // Save first
    await page.evaluate(() => apiFetch('/api/dsp/presets/save?slot=2', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name: 'OldName' })
    }));
    const resp = await page.evaluate(() => apiFetch('/api/dsp/presets/rename', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ slot: 2, name: 'NewName' })
    }).then(r => r.json()));
    expect(resp.success).toBe(true);
  });

  test('GET dsp/presets returns slots array', async ({ connectedPage: page }) => {
    const data = await page.evaluate(() => apiFetch('/api/dsp/presets').then(r => r.json()));
    expect(data.success).toBe(true);
    expect(Array.isArray(data.slots)).toBe(true);
    expect(data.slots.length).toBe(32);
    expect(data.slots[0]).toHaveProperty('index', 0);
    expect(data.slots[0]).toHaveProperty('exists');
  });

  test('PEQ tab shows Save Current PEQ button', async ({ connectedPage: page }) => {
    await openPresetsOverlay(page);
    await page.evaluate(() => switchDspPresetTab('peq'));
    await expect(page.locator('#dspPresetOverlay')).toContainText('Save Current PEQ');
  });

  test('Save PEQ preset calls REST endpoint', async ({ connectedPage: page }) => {
    const resp = await page.evaluate(() => apiFetch('/api/dsp/peq/presets', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name: 'MyEQ', channel: 0, stages: [] })
    }).then(r => r.json()));
    expect(resp.success).toBe(true);
  });

  test('Load PEQ preset calls REST endpoint', async ({ connectedPage: page }) => {
    // Save first
    await page.evaluate(() => apiFetch('/api/dsp/peq/presets', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name: 'LoadEQ', channel: 0, stages: [] })
    }));
    const data = await page.evaluate(() => apiFetch('/api/dsp/peq/preset?name=LoadEQ').then(r => r.json()));
    expect(data.success).toBe(true);
    expect(data.preset.name).toBe('LoadEQ');
  });

  test('Delete PEQ preset calls REST endpoint', async ({ connectedPage: page }) => {
    await page.evaluate(() => apiFetch('/api/dsp/peq/presets', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name: 'DelEQ', channel: 0, stages: [] })
    }));
    const resp = await page.evaluate(() => apiFetch('/api/dsp/peq/preset', {
      method: 'DELETE',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name: 'DelEQ' })
    }).then(r => r.json()));
    expect(resp.success).toBe(true);
  });
});
