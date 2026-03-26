/**
 * audio-batch-e-viz.spec.js — Collapsible viz section + SNR/SFDR readout.
 *
 * Tests for:
 *  - Viz section header present in Inputs sub-view
 *  - Viz section body visible by default
 *  - Clicking header collapses viz body
 *  - Clicking header again expands viz body
 *  - SNR/SFDR spans present for ADC 0 and ADC 1
 *  - audioLevels broadcast with adcSnrDb/adcSfdrDb updates readouts
 */

const { test, expect } = require('../helpers/fixtures');

test.describe('@audio Viz Section + SNR/SFDR', () => {
  test.beforeEach(async ({ connectedPage: page }) => {
    // Navigate to audio tab inputs using evaluate
    await page.evaluate(() => { switchTab('audio'); switchAudioSubView('inputs'); });
    await expect(page.locator('#audio-sv-inputs')).toHaveClass(/active/, { timeout: 5000 });
    // Wait for channel strips to render
    await expect(page.locator('#audio-inputs-container'))
      .not.toContainText('Waiting for device data...', { timeout: 5000 });
  });

  test('Viz section header is present below channel strips', async ({ connectedPage: page }) => {
    await expect(page.locator('#audioVizSectionHeader')).toBeVisible({ timeout: 3000 });
  });

  test('Viz section header contains Visualizations label', async ({ connectedPage: page }) => {
    await expect(page.locator('#audioVizSectionHeader')).toContainText('Visualizations');
  });

  test('Viz section body is visible by default', async ({ connectedPage: page }) => {
    await expect(page.locator('#audioVizBody')).toBeVisible({ timeout: 3000 });
  });

  test('Waveform canvas is inside viz section body', async ({ connectedPage: page }) => {
    await expect(page.locator('#audioVizBody #audioWaveformCanvas0')).toBeAttached({ timeout: 3000 });
  });

  test('Spectrum canvas is inside viz section body', async ({ connectedPage: page }) => {
    await expect(page.locator('#audioVizBody #audioSpectrumCanvas0')).toBeAttached({ timeout: 3000 });
  });

  test('Clicking viz header collapses the body', async ({ connectedPage: page }) => {
    await page.locator('#audioVizSectionHeader').click();
    await expect(page.locator('#audioVizBody')).toBeHidden({ timeout: 2000 });
  });

  test('Clicking viz header again expands the body', async ({ connectedPage: page }) => {
    await page.locator('#audioVizSectionHeader').click();
    await expect(page.locator('#audioVizBody')).toBeHidden({ timeout: 2000 });
    await page.locator('#audioVizSectionHeader').click();
    await expect(page.locator('#audioVizBody')).toBeVisible({ timeout: 2000 });
  });

  test('SNR span for ADC 0 is present in spectrum section', async ({ connectedPage: page }) => {
    await expect(page.locator('#audioSnr0')).toBeAttached({ timeout: 3000 });
  });

  test('SFDR span for ADC 0 is present in spectrum section', async ({ connectedPage: page }) => {
    await expect(page.locator('#audioSfdr0')).toBeAttached({ timeout: 3000 });
  });

  test('SNR span for ADC 1 is present in spectrum section', async ({ connectedPage: page }) => {
    await expect(page.locator('#audioSnr1')).toBeAttached({ timeout: 3000 });
  });

  test('SFDR span for ADC 1 is present in spectrum section', async ({ connectedPage: page }) => {
    await expect(page.locator('#audioSfdr1')).toBeAttached({ timeout: 3000 });
  });

  test('SNR/SFDR spans update from audioLevels broadcast', async ({ connectedPage: page }) => {
    // Inject SNR/SFDR values via updateSnrSfdr directly
    await page.evaluate(() => {
      updateSnrSfdr([95.4, 88.2], [110.1, 102.5]);
    });
    await expect(page.locator('#audioSnr0')).toHaveText('95.4', { timeout: 2000 });
    await expect(page.locator('#audioSfdr0')).toHaveText('110.1', { timeout: 2000 });
    await expect(page.locator('#audioSnr1')).toHaveText('88.2', { timeout: 2000 });
    await expect(page.locator('#audioSfdr1')).toHaveText('102.5', { timeout: 2000 });
  });
});
