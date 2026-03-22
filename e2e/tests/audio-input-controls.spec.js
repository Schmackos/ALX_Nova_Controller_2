/**
 * audio-input-controls.spec.js — Audio input channel strip controls.
 *
 * Tests for input channel strip rendering, gain/mute/phase controls,
 * ADC status badges, and WS command emission.
 */

const { test, expect } = require('../helpers/fixtures');
const { expectWsCommand, clearWsCapture } = require('../helpers/ws-assertions');

test.describe('@audio @ws Audio Input Controls', () => {
  test.beforeEach(async ({ connectedPage: page }) => {
    await page.locator('.sidebar-item[data-tab="audio"]').click();
    await expect(page.locator('#audio-sv-inputs')).toHaveClass(/active/);
    // Wait for channel strips to populate from audioChannelMap fixture
    await expect(page.locator('#audio-inputs-container')).not.toContainText('Waiting for device data...', { timeout: 5000 });
  });

  test('input channel strips render for each input lane', async ({ connectedPage: page }) => {
    // The audioChannelMap fixture has 4 inputs (lanes 0-3)
    const strips = page.locator('.channel-strip[data-lane]');
    await expect(strips).toHaveCount(4);
  });

  test('gain slider changes value and sends WS command', async ({ connectedPage: page }) => {
    await test.step('set gain on lane 0', async () => {
      clearWsCapture(page);
      const slider = page.locator('#inputGain0');
      await slider.fill('3');
      await slider.dispatchEvent('input');
    });

    await test.step('verify WS command sent', async () => {
      await expectWsCommand(page, 'setInputGain', { lane: 0, db: 3 });
    });
  });

  test('mute toggle sends WS command with correct lane', async ({ connectedPage: page }) => {
    clearWsCapture(page);
    await page.locator('#inputMute0').click();
    await expectWsCommand(page, 'setInputMute', { lane: 0, muted: true });
  });

  test('phase invert toggle sends WS command', async ({ connectedPage: page }) => {
    clearWsCapture(page);
    await page.locator('#inputPhase0').click();
    await expectWsCommand(page, 'setInputPhase', { lane: 0, inverted: true });
  });

  test('ADC status badge shows correct state for ready and offline lanes', async ({ connectedPage: page }) => {
    // Lane 0 (PCM1808) is ready=true, lane 3 (USB Audio) is ready=false
    const strip0 = page.locator('.channel-strip[data-lane="0"]');
    await expect(strip0.locator('.channel-status')).toHaveText('OK');

    const strip3 = page.locator('.channel-strip[data-lane="3"]');
    await expect(strip3.locator('.channel-status')).toHaveText('Offline');
  });

  test('channel strip shows device name from audioChannelMap', async ({ connectedPage: page }) => {
    // Lane 0 device name is PCM1808
    const strip0 = page.locator('.channel-strip[data-lane="0"]');
    await expect(strip0.locator('.channel-device-name')).toHaveText('PCM1808');

    // Lane 2 device name is Signal Generator
    const strip2 = page.locator('.channel-strip[data-lane="2"]');
    await expect(strip2.locator('.channel-device-name')).toHaveText('Signal Generator');
  });

  test('gain value display updates on slider change', async ({ connectedPage: page }) => {
    const slider = page.locator('#inputGain0');
    await slider.fill('6');
    await slider.dispatchEvent('input');
    await expect(page.locator('#inputGainVal0')).toHaveText('6.0 dB');
  });

  test('multiple lanes can be independently controlled', async ({ connectedPage: page }) => {
    clearWsCapture(page);

    await test.step('mute lane 0', async () => {
      await page.locator('#inputMute0').click();
    });

    await test.step('mute lane 1', async () => {
      await page.locator('#inputMute1').click();
    });

    await test.step('verify both WS commands sent with different lanes', async () => {
      await expectWsCommand(page, 'setInputMute', { lane: 0, muted: true });
      await expectWsCommand(page, 'setInputMute', { lane: 1, muted: true });
    });
  });

  test('default gain values from initial state', async ({ connectedPage: page }) => {
    // Default gain value is 0.0 dB for all lanes
    await expect(page.locator('#inputGainVal0')).toHaveText('0.0 dB');
    await expect(page.locator('#inputGainVal1')).toHaveText('0.0 dB');
  });

  test('channel strip shows lane name in header', async ({ connectedPage: page }) => {
    // USB Audio lane (lane 3) should show "USB Audio" as device name
    const strip3 = page.locator('.channel-strip[data-lane="3"]');
    await expect(strip3.locator('.channel-device-name')).toHaveText('USB Audio');
  });
});
