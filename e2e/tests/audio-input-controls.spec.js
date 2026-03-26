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

test.describe('@audio @ws Phase 2 — Input Channel Label and Capability Controls', () => {
  test.beforeEach(async ({ connectedPage: page }) => {
    await page.locator('.sidebar-item[data-tab="audio"]').click();
    await expect(page.locator('#audio-sv-inputs')).toHaveClass(/active/);
    await expect(page.locator('#audio-inputs-container')).not.toContainText('Waiting for device data...', { timeout: 5000 });
  });

  test('channel label is rendered with data-action="edit-channel-label"', async ({ connectedPage: page }) => {
    // _buildInputStrip() always renders a .channel-label with data-action="edit-channel-label"
    const label = page.locator('.channel-label[data-action="edit-channel-label"][data-lane="0"]');
    await expect(label).toBeAttached();
  });

  test('channel label text comes from inputNames fixture', async ({ connectedPage: page }) => {
    // input-names.json has "ADC1 L" at index 0; lane 0 uses inputNames[lane*2] = inputNames[0]
    await expect(page.locator('.channel-label[data-lane="0"]')).toContainText('ADC1 L');
  });

  test('clicking channel label makes it contenteditable; Enter commits and sends setInputNames WS', async ({ connectedPage: page }) => {
    const label = page.locator('.channel-label[data-lane="0"]');

    clearWsCapture(page);

    // Single click triggers startChannelLabelEdit() via data-action="edit-channel-label" delegation
    await label.click();

    // Label becomes contenteditable='true'
    await expect(label).toHaveAttribute('contenteditable', 'true', { timeout: 2000 });

    // Select all existing text and replace
    await page.keyboard.press('Control+a');
    await page.keyboard.type('My Custom ADC');
    await page.keyboard.press('Enter');

    // startChannelLabelEdit commit() calls ws.send({ type: 'setInputNames', names: inputNames.slice() })
    const cmd = await expectWsCommand(page, 'setInputNames', {}, 3000);
    expect(Array.isArray(cmd.names), 'names should be an array').toBe(true);
    expect(cmd.names[0]).toBe('My Custom ADC');
  });

  test('PGA slider rendered when input capabilities include bit 5 (HAL_CAP_PGA_CONTROL=32)', async ({ connectedPage: page }) => {
    await page.wsRoute.send({
      type: 'audioChannelMap',
      inputs: [
        {
          lane: 0,
          name: 'ADC PGA',
          channels: 2,
          matrixCh: 0,
          deviceName: 'ES8388',
          deviceType: 2,
          compatible: 'everest-semi,es8388',
          manufacturer: 'Everest Semiconductor',
          capabilities: 32,
          ready: true
        }
      ],
      outputs: [],
      matrixInputs: 16,
      matrixOutputs: 16,
      matrixBypass: false,
      matrix: []
    });

    // Clear the render guard so re-render fires
    await expect(page.locator('#audio-inputs-container')).not.toContainText('Waiting for device data...', { timeout: 3000 });
    // _buildInputStrip() adds #inputPga{lane} slider when (capabilities & 32) is truthy
    await expect(page.locator('#inputPga0')).toBeAttached({ timeout: 3000 });
  });

  test('HPF button rendered when input capabilities include bit 6 (HAL_CAP_HPF_CONTROL=64)', async ({ connectedPage: page }) => {
    await page.wsRoute.send({
      type: 'audioChannelMap',
      inputs: [
        {
          lane: 0,
          name: 'ADC HPF',
          channels: 2,
          matrixCh: 0,
          deviceName: 'ES8388',
          deviceType: 2,
          compatible: 'everest-semi,es8388',
          manufacturer: 'Everest Semiconductor',
          capabilities: 64,
          ready: true
        }
      ],
      outputs: [],
      matrixInputs: 16,
      matrixOutputs: 16,
      matrixBypass: false,
      matrix: []
    });

    await expect(page.locator('#audio-inputs-container')).not.toContainText('Waiting for device data...', { timeout: 3000 });
    // _buildInputStrip() adds #inputHpf{lane} button when (capabilities & 64) is truthy
    await expect(page.locator('#inputHpf0')).toBeAttached({ timeout: 3000 });
  });

  test('PGA and HPF controls absent when capability bits not set', async ({ connectedPage: page }) => {
    // PCM1808 has capabilities=8 — neither bit 5 (32) nor bit 6 (64) set
    await expect(page.locator('#inputPga0')).not.toBeAttached();
    await expect(page.locator('#inputHpf0')).not.toBeAttached();
  });

  test('solo button toggles active class on click', async ({ connectedPage: page }) => {
    // toggleInputSolo() is UI-only — toggles .active on the button
    const soloBtn = page.locator('#inputSolo0');
    await expect(soloBtn).toBeAttached();
    await expect(soloBtn).not.toHaveClass(/active/);

    await soloBtn.click();
    await expect(soloBtn).toHaveClass(/active/);

    // Second click deactivates
    await soloBtn.click();
    await expect(soloBtn).not.toHaveClass(/active/);
  });
});
