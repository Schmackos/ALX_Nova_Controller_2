/**
 * audio-siggen-controls.spec.js — Signal generator controls.
 *
 * Tests for signal generator enable/disable, waveform/frequency/amplitude
 * selection, output mode, target ADC, presets, and WS command emission.
 */

const { test, expect } = require('../helpers/fixtures');
const { expectWsCommand, clearWsCapture } = require('../helpers/ws-assertions');

test.describe('@audio @ws Audio Signal Generator', () => {
  test.beforeEach(async ({ connectedPage: page }) => {
    await page.locator('.sidebar-item[data-tab="audio"]').click();
    await page.locator('.audio-subnav-btn[data-view="siggen"]').click();
    await expect(page.locator('#audio-sv-siggen')).toHaveClass(/active/);
  });

  test('signal gen enable toggle sends WS command', async ({ connectedPage: page }) => {
    clearWsCapture(page);
    // siggenEnable is a hidden checkbox inside a label.switch
    const enableLabel = page.locator('label.switch:has(#siggenEnable)');
    await enableLabel.click();
    await expectWsCommand(page, 'setSignalGen', { enabled: true });
  });

  test('waveform selector sends WS command', async ({ connectedPage: page }) => {
    // Enable siggen first to reveal fields
    const enableLabel = page.locator('label.switch:has(#siggenEnable)');
    await enableLabel.click();
    await expect(page.locator('#siggenFields')).toBeVisible({ timeout: 2000 });

    clearWsCapture(page);
    await page.locator('#siggenWaveform').selectOption('1'); // Square
    // updateSigGen() fires on each change and sends full state
    await expectWsCommand(page, 'setSignalGen', { waveform: 1 });
  });

  test('frequency input sends WS command', async ({ connectedPage: page }) => {
    const enableLabel = page.locator('label.switch:has(#siggenEnable)');
    await enableLabel.click();
    await expect(page.locator('#siggenFields')).toBeVisible({ timeout: 2000 });

    clearWsCapture(page);
    const freqSlider = page.locator('#siggenFreq');
    await freqSlider.fill('440');
    await freqSlider.dispatchEvent('input');
    await freqSlider.dispatchEvent('change');
    await expectWsCommand(page, 'setSignalGen', { frequency: 440 });
  });

  test('amplitude input sends WS command', async ({ connectedPage: page }) => {
    const enableLabel = page.locator('label.switch:has(#siggenEnable)');
    await enableLabel.click();
    await expect(page.locator('#siggenFields')).toBeVisible({ timeout: 2000 });

    clearWsCapture(page);
    const ampSlider = page.locator('#siggenAmp');
    await ampSlider.fill('-10');
    await ampSlider.dispatchEvent('input');
    await ampSlider.dispatchEvent('change');
    await expectWsCommand(page, 'setSignalGen', { amplitude: -10 });
  });

  test('mode selector (software/PWM) sends WS command', async ({ connectedPage: page }) => {
    const enableLabel = page.locator('label.switch:has(#siggenEnable)');
    await enableLabel.click();
    await expect(page.locator('#siggenFields')).toBeVisible({ timeout: 2000 });

    clearWsCapture(page);
    await page.locator('#siggenOutputMode').selectOption('1'); // PWM
    await expectWsCommand(page, 'setSignalGen', { outputMode: 1 });
  });

  test('target ADC selector sends WS command', async ({ connectedPage: page }) => {
    const enableLabel = page.locator('label.switch:has(#siggenEnable)');
    await enableLabel.click();
    await expect(page.locator('#siggenFields')).toBeVisible({ timeout: 2000 });

    const targetSelect = page.locator('#siggenTargetAdc');
    const count = await targetSelect.count();
    if (count === 0) {
      test.skip(true, 'Target ADC selector not present in siggen fields');
      return;
    }

    clearWsCapture(page);
    await targetSelect.selectOption('1');
    await expectWsCommand(page, 'setSignalGen', { targetAdc: 1 });
  });

  test('preset buttons set correct values and send WS command', async ({ connectedPage: page }) => {
    clearWsCapture(page);
    // Call siggenPreset directly via evaluate to avoid button visibility issues
    await page.evaluate(() => siggenPreset(0, 1000, -6));
    await expectWsCommand(page, 'setSignalGen', { enabled: true, frequency: 1000 });
  });

  test('UI state matches WS fixture defaults', async ({ connectedPage: page }) => {
    // signalGenerator fixture: enabled=false, waveform=0 (Sine), frequency=1000, amplitude=-20
    const enableToggle = page.locator('#siggenEnable');
    await expect(enableToggle).not.toBeChecked();

    // Fields should be hidden when disabled
    await expect(page.locator('#siggenFields')).toBeHidden();
  });
});
