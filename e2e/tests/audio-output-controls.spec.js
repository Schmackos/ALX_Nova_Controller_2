/**
 * audio-output-controls.spec.js — Audio output channel strip controls.
 *
 * Tests for output channel strip rendering, gain/volume/mute/phase/delay
 * controls, and WS command emission.
 */

const { test, expect } = require('../helpers/fixtures');
const { expectWsCommand, clearWsCapture } = require('../helpers/ws-assertions');

test.describe('@audio @ws Audio Output Controls', () => {
  test.beforeEach(async ({ connectedPage: page }) => {
    await page.locator('.sidebar-item[data-tab="audio"]').click();
    await page.locator('.audio-subnav-btn[data-view="outputs"]').click();
    await expect(page.locator('#audio-sv-outputs')).toHaveClass(/active/);
    await expect(page.locator('#audio-outputs-container')).not.toContainText('Waiting for device data...', { timeout: 5000 });
  });

  test('output strips render for each output', async ({ connectedPage: page }) => {
    // audioChannelMap fixture has 2 outputs (PCM5102A index 0, ES8311 index 1)
    const strips = page.locator('.channel-strip-output[data-sink]');
    await expect(strips).toHaveCount(2);
  });

  test('output gain slider sends WS command', async ({ connectedPage: page }) => {
    clearWsCapture(page);
    const slider = page.locator('#outputGain0');
    await slider.fill('-6');
    await slider.dispatchEvent('input');
    // Output 0 has firstChannel=0
    await expectWsCommand(page, 'setOutputGain', { channel: 0, db: -6 });
  });

  test('hardware volume slider sends WS command when available', async ({ connectedPage: page }) => {
    // ES8311 (index 1) has capabilities=199 which includes HAL_CAP_HW_VOLUME (bit 0)
    const hwVolSlider = page.locator('#outputHwVol1');
    const hwVolVisible = await hwVolSlider.count();
    if (hwVolVisible === 0) {
      test.skip(true, 'HW volume slider not rendered for this output configuration');
      return;
    }
    clearWsCapture(page);
    await hwVolSlider.fill('60');
    await hwVolSlider.dispatchEvent('input');
    // ES8311 has firstChannel=2
    await expectWsCommand(page, 'setOutputHwVolume', { channel: 2, volume: 60 });
  });

  test('output mute toggle sends WS command', async ({ connectedPage: page }) => {
    clearWsCapture(page);
    await page.locator('#outputMute0').click();
    // Output 0 has firstChannel=0
    await expectWsCommand(page, 'setOutputMute', { channel: 0, muted: true });
  });

  test('output phase invert sends WS command', async ({ connectedPage: page }) => {
    clearWsCapture(page);
    await page.locator('#outputPhase0').click();
    // Output 0 has firstChannel=0
    await expectWsCommand(page, 'setOutputPhase', { channel: 0, inverted: true });
  });

  test('output delay input sends WS command', async ({ connectedPage: page }) => {
    clearWsCapture(page);
    // Delay input keyed by firstChannel (output 0 has firstChannel=0)
    const delayInput = page.locator('#outputDelay0');
    await delayInput.fill('1.5');
    await delayInput.dispatchEvent('change');
    await expectWsCommand(page, 'setOutputDelay', { channel: 0, ms: 1.5 });
  });

  test('output strip shows sink name from audioChannelMap', async ({ connectedPage: page }) => {
    const strip0 = page.locator('.channel-strip-output[data-sink="0"]');
    await expect(strip0.locator('.channel-device-name')).toHaveText('PCM5102A');

    const strip1 = page.locator('.channel-strip-output[data-sink="1"]');
    await expect(strip1.locator('.channel-device-name')).toHaveText('ES8311');
  });

  test('default gain values from initial state', async ({ connectedPage: page }) => {
    await expect(page.locator('#outputGainVal0')).toHaveText('0.0 dB');
    await expect(page.locator('#outputGainVal1')).toHaveText('0.0 dB');
  });

  test('output strip shows channel range sub-header', async ({ connectedPage: page }) => {
    // Output 0: firstChannel=0, channels=2 -> "Ch 0-1"
    const strip0 = page.locator('.channel-strip-output[data-sink="0"]');
    await expect(strip0.locator('.channel-strip-sub')).toContainText('Ch 0-1');

    // Output 1: firstChannel=2, channels=2 -> "Ch 2-3"
    const strip1 = page.locator('.channel-strip-output[data-sink="1"]');
    await expect(strip1.locator('.channel-strip-sub')).toContainText('Ch 2-3');
  });

  test('multiple outputs independently controlled', async ({ connectedPage: page }) => {
    clearWsCapture(page);

    await test.step('set gain on output 0', async () => {
      const slider0 = page.locator('#outputGain0');
      await slider0.fill('-3');
      await slider0.dispatchEvent('input');
    });

    await test.step('set gain on output 1', async () => {
      const slider1 = page.locator('#outputGain1');
      await slider1.fill('-12');
      await slider1.dispatchEvent('input');
    });

    await test.step('verify both WS commands sent', async () => {
      await expectWsCommand(page, 'setOutputGain', { channel: 0, db: -3 });
      await expectWsCommand(page, 'setOutputGain', { channel: 2, db: -12 });
    });
  });
});
