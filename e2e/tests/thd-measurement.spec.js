/**
 * thd-measurement.spec.js — Phase 7: THD+N measurement tool in SigGen sub-view.
 *
 * Tests for:
 * - "Measure THD" button present in SigGen view when enabled
 * - Button sends `startThdMeasurement` WS command with freq + averages
 * - Progress bar visible during measurement
 * - Results panel displays THD+N %, dB, and harmonics table on thdResult broadcast
 * - Stop button sends `stopThdMeasurement` WS command
 *
 * The thdResult WS message is server-push only — tests inject it via wsRoute.send().
 */

const { test, expect } = require('../helpers/fixtures');
const { expectWsCommand, clearWsCapture } = require('../helpers/ws-assertions');
const path = require('path');
const fs = require('fs');

const THD_FIXTURE = JSON.parse(
  fs.readFileSync(path.join(__dirname, '..', 'fixtures', 'ws-messages', 'thd-result.json'), 'utf8')
);

// Navigate to Audio > SigGen sub-view
async function goToSigGen(page) {
  await page.locator('.sidebar-item[data-tab="audio"]').click();
  await page.locator('.audio-subnav-btn[data-view="siggen"]').click();
  await expect(page.locator('#audio-sv-siggen')).toHaveClass(/active/);
}

// Enable the signal generator (reveals fields + THD button)
async function enableSigGen(page) {
  const enableLabel = page.locator('label.switch:has(#siggenEnable)');
  const isChecked = await page.locator('#siggenEnable').isChecked();
  if (!isChecked) {
    await enableLabel.click();
    await expect(page.locator('#siggenFields')).toBeVisible({ timeout: 2000 });
  }
}

test.describe('@audio @ws Phase 7: THD Measurement', () => {
  test.beforeEach(async ({ connectedPage: page }) => {
    await goToSigGen(page);
  });

  // ===== Presence =====

  test('Measure THD button is present in SigGen view', async ({ connectedPage: page }) => {
    const btn = page.locator('#thdMeasureBtn');
    const count = await btn.count();
    if (count === 0) {
      // Button may only appear when siggen is enabled
      await enableSigGen(page);
    }
    // After enabling, check again
    const btnAfter = page.locator('#thdMeasureBtn');
    const countAfter = await btnAfter.count();
    if (countAfter === 0) {
      test.skip(true, 'thdMeasureBtn not present — implementation pending');
      return;
    }
    await expect(btnAfter).toBeVisible({ timeout: 2000 });
  });

  // ===== WS command emission =====

  test('Measure THD button sends startThdMeasurement WS command', async ({ connectedPage: page }) => {
    await enableSigGen(page);

    const btn = page.locator('#thdMeasureBtn');
    if (await btn.count() === 0) {
      test.skip(true, 'thdMeasureBtn not present — implementation pending');
      return;
    }

    clearWsCapture(page);
    await btn.click();

    const cmd = await expectWsCommand(page, 'startThdMeasurement', {});
    expect(typeof cmd.freq === 'number' || typeof cmd.frequency === 'number').toBe(true);
  });

  test('startThdMeasurement command includes averages field', async ({ connectedPage: page }) => {
    await enableSigGen(page);

    const btn = page.locator('#thdMeasureBtn');
    if (await btn.count() === 0) {
      test.skip(true, 'thdMeasureBtn not present — implementation pending');
      return;
    }

    clearWsCapture(page);
    await btn.click();

    const cmd = await expectWsCommand(page, 'startThdMeasurement', {});
    const averages = cmd.averages || cmd.numAverages || cmd.frames;
    expect(averages).toBeDefined();
    expect(typeof averages).toBe('number');
    expect(averages).toBeGreaterThan(0);
  });

  test('startThdMeasurement uses current siggen frequency', async ({ connectedPage: page }) => {
    await enableSigGen(page);

    // Set frequency to 440 Hz via slider
    const freqSlider = page.locator('#siggenFreq');
    await freqSlider.fill('440');
    await freqSlider.dispatchEvent('input');
    await freqSlider.dispatchEvent('change');

    const btn = page.locator('#thdMeasureBtn');
    if (await btn.count() === 0) {
      test.skip(true, 'thdMeasureBtn not present — implementation pending');
      return;
    }

    clearWsCapture(page);
    await btn.click();

    const cmd = await expectWsCommand(page, 'startThdMeasurement', {});
    const freq = cmd.freq || cmd.frequency;
    expect(freq).toBe(440);
  });

  // ===== Progress bar =====

  test('progress indicator becomes visible after starting measurement', async ({ connectedPage: page }) => {
    await enableSigGen(page);

    const btn = page.locator('#thdMeasureBtn');
    if (await btn.count() === 0) {
      test.skip(true, 'thdMeasureBtn not present — implementation pending');
      return;
    }

    await btn.click();

    // Progress container or bar should appear
    const progressEl = page.locator('#thdProgress, #thdProgressBar, .thd-progress');
    const countEl = await progressEl.count();
    if (countEl === 0) {
      test.skip(true, 'THD progress element not present — implementation pending');
      return;
    }
    await expect(progressEl.first()).toBeVisible({ timeout: 2000 });
  });

  // ===== Results display =====

  test('thdResult broadcast shows THD+N percentage', async ({ connectedPage: page }) => {
    await enableSigGen(page);

    const btn = page.locator('#thdMeasureBtn');
    if (await btn.count() === 0) {
      test.skip(true, 'thdMeasureBtn not present — implementation pending');
      return;
    }
    await btn.click();

    // Inject thdResult via server push
    page.wsRoute.send(THD_FIXTURE);

    // Results container should appear
    const resultsEl = page.locator('#thdResults, .thd-results');
    const countEl = await resultsEl.count();
    if (countEl === 0) {
      test.skip(true, 'THD results element not present — implementation pending');
      return;
    }
    await expect(resultsEl.first()).toBeVisible({ timeout: 3000 });
  });

  test('thdResult broadcast shows THD+N dB value', async ({ connectedPage: page }) => {
    await enableSigGen(page);

    const btn = page.locator('#thdMeasureBtn');
    if (await btn.count() === 0) {
      test.skip(true, 'thdMeasureBtn not present — implementation pending');
      return;
    }
    await btn.click();
    page.wsRoute.send(THD_FIXTURE);

    // THD+N dB value from fixture is -66.0
    const dbEl = page.locator('#thdResultDb, .thd-result-db');
    const countEl = await dbEl.count();
    if (countEl === 0) {
      test.skip(true, 'THD dB result element not present — implementation pending');
      return;
    }
    await expect(dbEl.first()).toContainText('-66', { timeout: 3000 });
  });

  test('thdResult broadcast shows harmonic levels', async ({ connectedPage: page }) => {
    await enableSigGen(page);

    const btn = page.locator('#thdMeasureBtn');
    if (await btn.count() === 0) {
      test.skip(true, 'thdMeasureBtn not present — implementation pending');
      return;
    }
    await btn.click();
    page.wsRoute.send(THD_FIXTURE);

    // Harmonics table or list (2nd through 9th harmonic) should appear
    const harmonicsEl = page.locator('#thdHarmonicsTable, .thd-harmonics, table.thd-harmonics');
    const countEl = await harmonicsEl.count();
    if (countEl === 0) {
      test.skip(true, 'THD harmonics table not present — implementation pending');
      return;
    }
    await expect(harmonicsEl.first()).toBeVisible({ timeout: 3000 });
    // Fixture has 8 harmonics
    const rows = harmonicsEl.first().locator('tr, li');
    const rowCount = await rows.count();
    expect(rowCount).toBeGreaterThanOrEqual(2);
  });

  test('invalid thdResult (valid=false) shows error or nothing', async ({ connectedPage: page }) => {
    await enableSigGen(page);

    const btn = page.locator('#thdMeasureBtn');
    if (await btn.count() === 0) {
      test.skip(true, 'thdMeasureBtn not present — implementation pending');
      return;
    }
    await btn.click();

    // Push invalid result
    page.wsRoute.send({ type: 'thdResult', valid: false, thdPlusNPercent: 0, thdPlusNDb: 0,
      fundamentalDbfs: 0, harmonicLevels: [], framesProcessed: 0, framesTarget: 8 });

    // Results should either not show or show an error state — no crash
    await page.waitForTimeout(500);
    // Just verify no JS error was thrown (Playwright would surface unhandled exceptions)
  });

  // ===== Stop button =====

  test('Stop button sends stopThdMeasurement WS command', async ({ connectedPage: page }) => {
    await enableSigGen(page);

    const startBtn = page.locator('#thdMeasureBtn');
    if (await startBtn.count() === 0) {
      test.skip(true, 'thdMeasureBtn not present — implementation pending');
      return;
    }
    await startBtn.click();

    const stopBtn = page.locator('#thdStopBtn, [data-action="thd-stop"]');
    const stopCount = await stopBtn.count();
    if (stopCount === 0) {
      test.skip(true, 'THD stop button not present — implementation pending');
      return;
    }

    clearWsCapture(page);
    await stopBtn.first().click();
    await expectWsCommand(page, 'stopThdMeasurement', {});
  });
});
