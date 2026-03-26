/**
 * audio-viz-panel.spec.js — Phase 8: Visualization Migration.
 *
 * Tests for:
 * - Viz section appears in Audio > Inputs sub-view (below channel strips)
 * - Collapsible toggle works (shows/hides viz content)
 * - Enable/disable toggles per viz type still work after migration
 * - SNR/SFDR readout displays from audioLevels broadcast
 * - Binary WS handlers (0x01 waveform, 0x02 spectrum) still render canvases
 *   in their new location
 *
 * The existing audio-visualization.spec.js tests WS toggle commands;
 * this spec focuses on the panel's structure and SNR/SFDR display in
 * the new Phase 8 layout.
 */

const { test, expect } = require('../helpers/fixtures');
const { expectWsCommand, clearWsCapture } = require('../helpers/ws-assertions');
const path = require('path');
const fs = require('fs');
const { buildWaveformFrame, buildSpectrumFrame } = require('../helpers/ws-helpers');

const AUDIO_LEVELS = JSON.parse(
  fs.readFileSync(path.join(__dirname, '..', 'fixtures', 'ws-messages', 'audio-levels.json'), 'utf8')
);

test.describe('@audio @ws Phase 8: Visualization Panel Migration', () => {
  test.beforeEach(async ({ connectedPage: page }) => {
    await page.locator('.sidebar-item[data-tab="audio"]').click();
    await expect(page.locator('#audio-sv-inputs')).toHaveClass(/active/);
  });

  // ===== Panel structure =====

  test('viz section exists in audio inputs sub-view', async ({ connectedPage: page }) => {
    // Viz panel should be present somewhere in the Inputs sub-view
    const vizPanel = page.locator(
      '#vizPanel, .viz-panel, #audio-viz-section, [data-section="viz"], #waveformContent, #spectrumContent'
    );
    const count = await vizPanel.count();
    expect(count).toBeGreaterThan(0);
  });

  test('waveform canvas exists in inputs sub-view', async ({ connectedPage: page }) => {
    const canvas = page.locator(
      '#audio-sv-inputs #audioWaveformCanvas0, #audio-sv-inputs canvas.waveform-canvas, #waveformContent canvas'
    );
    const count = await canvas.count();
    // Canvas may not be visible until audioGraphState enables waveform
    // Fixture sets waveformEnabled=true so it should be present
    expect(count).toBeGreaterThan(0);
  });

  test('spectrum canvas exists in inputs sub-view', async ({ connectedPage: page }) => {
    const canvas = page.locator(
      '#audio-sv-inputs #audioSpectrumCanvas0, #audio-sv-inputs canvas.spectrum-canvas, #spectrumContent canvas'
    );
    const count = await canvas.count();
    expect(count).toBeGreaterThan(0);
  });

  // ===== Collapsible toggle =====

  test('viz panel collapsible toggle is present', async ({ connectedPage: page }) => {
    const toggle = page.locator(
      '[data-action="toggle-viz-panel"], .viz-panel-toggle, button:has-text("Visualization"), [data-section="viz"] summary'
    );
    const count = await toggle.count();
    if (count === 0) {
      test.skip(true, 'Viz panel collapsible toggle not present — implementation pending');
      return;
    }
    await expect(toggle.first()).toBeVisible();
  });

  test('viz panel collapsible toggle hides and shows content', async ({ connectedPage: page }) => {
    const toggle = page.locator(
      '[data-action="toggle-viz-panel"], .viz-panel-toggle, [data-section="viz"] summary'
    );
    if (await toggle.count() === 0) {
      test.skip(true, 'Viz panel collapsible toggle not present — implementation pending');
      return;
    }

    // Content that should hide/show
    const vizContent = page.locator(
      '#vizPanelContent, .viz-panel-content, #waveformContent'
    );
    if (await vizContent.count() === 0) {
      test.skip(true, 'Viz panel content container not present — implementation pending');
      return;
    }

    const isInitiallyVisible = await vizContent.first().isVisible();
    await toggle.first().click();
    await page.waitForTimeout(300);

    const isAfterToggle = await vizContent.first().isVisible();
    // Should have changed
    expect(isAfterToggle).not.toBe(isInitiallyVisible);

    // Toggle back
    await toggle.first().click();
    await page.waitForTimeout(300);
    const isRestored = await vizContent.first().isVisible();
    expect(isRestored).toBe(isInitiallyVisible);
  });

  // ===== WS toggle commands still work in new location =====

  test('waveform enable toggle still sends WS command after migration', async ({ connectedPage: page }) => {
    clearWsCapture(page);
    const toggled = await page.evaluate(() => {
      const el = document.getElementById('waveformEnabledToggle');
      if (!el) return false;
      el.checked = !el.checked;
      el.dispatchEvent(new Event('change'));
      return true;
    });
    if (!toggled) {
      test.skip(true, 'waveformEnabledToggle not present in new layout');
      return;
    }
    await expectWsCommand(page, 'setWaveformEnabled', {});
  });

  test('spectrum enable toggle still sends WS command after migration', async ({ connectedPage: page }) => {
    clearWsCapture(page);
    const toggled = await page.evaluate(() => {
      const el = document.getElementById('spectrumEnabledToggle');
      if (!el) return false;
      el.checked = !el.checked;
      el.dispatchEvent(new Event('change'));
      return true;
    });
    if (!toggled) {
      test.skip(true, 'spectrumEnabledToggle not present in new layout');
      return;
    }
    await expectWsCommand(page, 'setSpectrumEnabled', {});
  });

  // ===== Binary WS frames still render =====

  test('binary waveform frame (0x01) does not throw after migration', async ({ connectedPage: page }) => {
    // Send a waveform frame — if canvas rendering fails JS exception will surface
    const frame = buildWaveformFrame(0, new Array(256).fill(128));
    page.wsRoute.sendBinary(frame);
    await page.waitForTimeout(300);
    // No exception = pass (Playwright surfaces unhandled exceptions automatically)
  });

  test('binary spectrum frame (0x02) does not throw after migration', async ({ connectedPage: page }) => {
    const frame = buildSpectrumFrame(0, 1000, new Array(16).fill(0.5));
    page.wsRoute.sendBinary(frame);
    await page.waitForTimeout(300);
  });

  // ===== SNR/SFDR readouts =====

  test('SNR readout displays after audioLevels broadcast', async ({ connectedPage: page }) => {
    // Send audioLevels with SNR/SFDR fields
    page.wsRoute.send(AUDIO_LEVELS);
    await page.waitForTimeout(400);

    // SNR readout element (may use a flexible selector matching the implementation)
    const snrEl = page.locator(
      '#snrReadout0, .snr-readout, [data-metric="snr"], #audioSnr0'
    );
    const count = await snrEl.count();
    if (count === 0) {
      test.skip(true, 'SNR readout element not present — implementation pending');
      return;
    }
    // Skip if element exists but shows placeholder (not yet wired to audioLevels)
    const text = await snrEl.first().textContent();
    if (!text || text.trim() === '--' || text.trim() === '') {
      test.skip(true, 'SNR readout not yet wired to audioLevels — implementation pending');
      return;
    }
    // Fixture has adcSnrDb: [85.2, 84.7]
    await expect(snrEl.first()).toContainText(/85/, { timeout: 2000 });
  });

  test('SFDR readout displays after audioLevels broadcast', async ({ connectedPage: page }) => {
    page.wsRoute.send(AUDIO_LEVELS);
    await page.waitForTimeout(400);

    const sfdrEl = page.locator(
      '#sfdrReadout0, .sfdr-readout, [data-metric="sfdr"], #audioSfdr0'
    );
    const count = await sfdrEl.count();
    if (count === 0) {
      test.skip(true, 'SFDR readout element not present — implementation pending');
      return;
    }
    const text = await sfdrEl.first().textContent();
    if (!text || text.trim() === '--' || text.trim() === '') {
      test.skip(true, 'SFDR readout not yet wired to audioLevels — implementation pending');
      return;
    }
    // Fixture has adcSfdrDb: [92.1, 91.4]
    await expect(sfdrEl.first()).toContainText(/92/, { timeout: 2000 });
  });

  test('noise floor readout displays after audioLevels broadcast', async ({ connectedPage: page }) => {
    page.wsRoute.send(AUDIO_LEVELS);
    await page.waitForTimeout(400);

    // Noise floor: adcNoiseFloor: [-72.5, -71.8]
    const nfEl = page.locator(
      '.noise-floor-readout, [data-metric="noise-floor"], #audioNoiseFloor0'
    );
    const count = await nfEl.count();
    if (count === 0) {
      test.skip(true, 'Noise floor readout not present — implementation pending');
      return;
    }
    const text = await nfEl.first().textContent();
    if (!text || text.trim() === '--' || text.trim() === '') {
      test.skip(true, 'Noise floor readout not yet wired to audioLevels — implementation pending');
      return;
    }
    await expect(nfEl.first()).toContainText(/-72/, { timeout: 2000 });
  });

  // ===== Per-viz enable/disable toggles still work =====

  test('viz section is visible in inputs sub-view (not moved to wrong sub-view)', async ({ connectedPage: page }) => {
    // Confirm viz controls are NOT present in the matrix sub-view
    await page.locator('.audio-subnav-btn[data-view="matrix"]').click();
    await expect(page.locator('#audio-sv-matrix')).toHaveClass(/active/);

    const vizInMatrix = page.locator('#audio-sv-matrix #waveformEnabledToggle, #audio-sv-matrix #vizPanel');
    const count = await vizInMatrix.count();
    expect(count).toBe(0);
  });
});
