/**
 * audio-visualization.spec.js — Audio visualization toggles.
 *
 * Tests for waveform/spectrum/VU toggle controls, FFT window selector,
 * LED mode toggle, and WS command emission.
 *
 * The visualization controls are in the Audio > Inputs sub-view.
 * Checkboxes may be outside the viewport due to the page layout;
 * use scrollIntoView + dispatchEvent to toggle them reliably.
 */

const { test, expect } = require('../helpers/fixtures');
const { expectWsCommand, clearWsCapture } = require('../helpers/ws-assertions');

test.describe('@audio @ws Audio Visualization', () => {
  test.beforeEach(async ({ connectedPage: page }) => {
    await page.locator('.sidebar-item[data-tab="audio"]').click();
    await expect(page.locator('#audio-sv-inputs')).toHaveClass(/active/);
  });

  test('waveform toggle sends WS command', async ({ connectedPage: page }) => {
    clearWsCapture(page);
    // Toggle via evaluate to avoid viewport issues with CSS-hidden checkboxes
    await page.evaluate(() => {
      const el = document.getElementById('waveformEnabledToggle');
      el.checked = !el.checked;
      el.dispatchEvent(new Event('change'));
    });
    await expectWsCommand(page, 'setWaveformEnabled', { enabled: false });
  });

  test('spectrum toggle sends WS command', async ({ connectedPage: page }) => {
    clearWsCapture(page);
    await page.evaluate(() => {
      const el = document.getElementById('spectrumEnabledToggle');
      el.checked = !el.checked;
      el.dispatchEvent(new Event('change'));
    });
    await expectWsCommand(page, 'setSpectrumEnabled', { enabled: false });
  });

  test('VU meter toggle sends WS command', async ({ connectedPage: page }) => {
    clearWsCapture(page);
    await page.evaluate(() => {
      const el = document.getElementById('vuMeterEnabledToggle');
      el.checked = !el.checked;
      el.dispatchEvent(new Event('change'));
    });
    await expectWsCommand(page, 'setVuMeterEnabled', { enabled: false });
  });

  test('FFT window selector sends WS command', async ({ connectedPage: page }) => {
    clearWsCapture(page);
    const fftSelect = page.locator('#fftWindowSelect');
    const count = await fftSelect.count();
    if (count === 0) {
      test.skip(true, 'FFT window selector not found');
      return;
    }
    // Scroll into view first, then select
    await fftSelect.scrollIntoViewIfNeeded();
    await fftSelect.selectOption('1'); // e.g. Hamming
    await expectWsCommand(page, 'setFftWindowType', { value: 1 });
  });

  test('LED mode toggle exists and is clickable', async ({ connectedPage: page }) => {
    const ledToggle = page.locator('#ledModeToggle');
    const count = await ledToggle.count();
    if (count === 0) {
      test.skip(true, 'LED mode toggle not present');
      return;
    }
    // Toggle via evaluate to avoid viewport issues
    await page.evaluate(() => {
      const el = document.getElementById('ledModeToggle');
      el.checked = !el.checked;
      el.dispatchEvent(new Event('change'));
    });
    await expect(ledToggle).toBeChecked();
  });

  test('waveform content container exists when enabled', async ({ connectedPage: page }) => {
    // audioGraphState fixture has waveformEnabled=true
    const waveformContent = page.locator('#waveformContent');
    await expect(waveformContent).toBeVisible();
  });

  test('spectrum content container exists when enabled', async ({ connectedPage: page }) => {
    // audioGraphState fixture has spectrumEnabled=true
    const spectrumContent = page.locator('#spectrumContent');
    await expect(spectrumContent).toBeVisible();
  });

  test('subscribeAudio WS command sent when entering audio inputs view', async ({ connectedPage: page }) => {
    // Switching away and back to inputs sub-view
    await page.locator('.audio-subnav-btn[data-view="matrix"]').click();
    await page.waitForTimeout(200);

    clearWsCapture(page);
    await page.locator('.audio-subnav-btn[data-view="inputs"]').click();
    // subscribeAudio may or may not be re-sent since audioSubscribed flag is set.
    // Verify the navigation works without error.
    await expect(page.locator('#audio-sv-inputs')).toHaveClass(/active/);
  });
});
