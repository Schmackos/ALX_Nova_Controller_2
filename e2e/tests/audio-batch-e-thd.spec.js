/**
 * audio-batch-e-thd.spec.js — THD+N measurement tool in SigGen sub-view.
 *
 * Tests for:
 *  - THD measurement card present in SigGen view
 *  - Measure THD button sends WS startThdMeasurement command
 *  - Stop button sends WS stopThdMeasurement command
 *  - thdResult broadcast updates results display
 */

const { test, expect } = require('../helpers/fixtures');
const { expectWsCommand, clearWsCapture } = require('../helpers/ws-assertions');

test.describe('@audio @ws THD Measurement', () => {
  test.beforeEach(async ({ connectedPage: page }) => {
    // Navigate to audio tab and siggen sub-view using evaluate to bypass visibility constraints
    await page.evaluate(() => { switchTab('audio'); switchAudioSubView('siggen'); });
    await expect(page.locator('#audio-sv-siggen')).toHaveClass(/active/, { timeout: 5000 });
  });

  test('THD measurement card is visible in SigGen view', async ({ connectedPage: page }) => {
    await expect(page.locator('.card-title').filter({ hasText: 'THD+N Measurement' })).toBeVisible({ timeout: 3000 });
  });

  test('Measure THD button is visible', async ({ connectedPage: page }) => {
    await expect(page.locator('#thdMeasureBtn')).toBeVisible({ timeout: 3000 });
  });

  test('Stop THD button is initially disabled', async ({ connectedPage: page }) => {
    const stopBtn = page.locator('#thdStopBtn');
    const count = await stopBtn.count();
    if (count === 0) {
      test.skip(true, 'thdStopBtn not present');
      return;
    }
    await expect(stopBtn).toBeDisabled();
  });

  test('Averages selector is visible', async ({ connectedPage: page }) => {
    await expect(page.locator('#thdAverages')).toBeVisible({ timeout: 3000 });
  });

  test('Measure THD button sends WS startThdMeasurement command', async ({ connectedPage: page }) => {
    clearWsCapture(page);
    await page.locator('#thdMeasureBtn').click();
    await expectWsCommand(page, 'startThdMeasurement', {});
  });

  test('Stop THD button sends WS stopThdMeasurement command', async ({ connectedPage: page }) => {
    // Start first so stop is enabled
    await page.locator('#thdMeasureBtn').click();
    await expect(page.locator('#thdStopBtn')).toBeEnabled({ timeout: 2000 });

    clearWsCapture(page);
    await page.locator('#thdStopBtn').click();
    await expectWsCommand(page, 'stopThdMeasurement', {});
  });

  test('thdResult broadcast with valid data shows results', async ({ connectedPage: page }) => {
    // Trigger measurement to set _thdMeasuring = true
    await page.locator('#thdMeasureBtn').click();

    // Inject a valid thdResult broadcast via evaluate
    await page.evaluate(() => {
      handleThdResult({
        valid: true,
        thdPlusNPercent: 0.0023,
        thdPlusNDb: -52.8,
        fundamentalDbfs: -6.0,
        harmonicLevels: [-60, -70, -75, -80, -82, -84, -86, -88],
        framesProcessed: 8,
        framesTarget: 8
      });
    });

    await expect(page.locator('#thdResults')).toBeVisible({ timeout: 2000 });
    await expect(page.locator('#thdResults')).toContainText('THD+N');
  });

  test('thdResult broadcast updates progress bar', async ({ connectedPage: page }) => {
    await page.locator('#thdMeasureBtn').click();
    await page.evaluate(() => {
      handleThdResult({
        valid: false,
        framesProcessed: 4,
        framesTarget: 8
      });
    });
    const label = page.locator('#thdProgressLabel');
    await expect(label).toContainText('4 / 8', { timeout: 2000 });
  });

  test('averages select options are present', async ({ connectedPage: page }) => {
    const select = page.locator('#thdAverages');
    await expect(select.locator('option[value="4"]')).toHaveCount(1);
    await expect(select.locator('option[value="8"]')).toHaveCount(1);
    await expect(select.locator('option[value="16"]')).toHaveCount(1);
  });

  test('startThdMeasurement sends averages value from select', async ({ connectedPage: page }) => {
    await page.locator('#thdAverages').selectOption('16');
    clearWsCapture(page);
    await page.locator('#thdMeasureBtn').click();
    await expectWsCommand(page, 'startThdMeasurement', { averages: 16 });
  });
});
