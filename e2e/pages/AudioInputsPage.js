const { expect } = require('@playwright/test');
const BasePage = require('./BasePage');
const SELECTORS = require('../helpers/selectors');

class AudioInputsPage extends BasePage {
  constructor(page) {
    super(page);
  }

  /**
   * Returns a locator for the channel strip at the given input lane.
   * @param {number} lane
   * @returns {import('@playwright/test').Locator}
   */
  getChannelStrip(lane) {
    return this.page.locator(`.channel-strip[data-lane="${lane}"]`);
  }

  /**
   * Set the gain slider for an input lane.
   * @param {number} lane
   * @param {number} db — gain value in dB (-72 to +12)
   */
  async setGain(lane, db) {
    const slider = this.page.locator(`#inputGain${lane}`);
    await slider.fill(String(db));
    await slider.dispatchEvent('input');
  }

  /**
   * Click the mute button for an input lane.
   * @param {number} lane
   */
  async toggleMute(lane) {
    await this.page.locator(`#inputMute${lane}`).click();
  }

  /**
   * Click the phase invert button for an input lane.
   * @param {number} lane
   */
  async togglePhase(lane) {
    await this.page.locator(`#inputPhase${lane}`).click();
  }

  /**
   * Assert the displayed gain value for an input lane.
   * @param {number} lane
   * @param {string|number} db — expected value (will match text content)
   */
  async expectGainValue(lane, db) {
    const label = this.page.locator(`#inputGainVal${lane}`);
    await expect(label).toHaveText(`${parseFloat(db).toFixed(1)} dB`);
  }

  /**
   * Assert the mute state for an input lane.
   * @param {number} lane
   * @param {boolean} muted — true if the mute button should have the 'active' class
   */
  async expectMuted(lane, muted) {
    const btn = this.page.locator(`#inputMute${lane}`);
    if (muted) {
      await expect(btn).toHaveClass(/active/);
    } else {
      await expect(btn).not.toHaveClass(/active/);
    }
  }

  /**
   * Assert the ADC status badge text for an input lane.
   * @param {number} lane
   * @param {string} status — expected status text (e.g. 'OK', 'Offline')
   */
  async expectAdcStatus(lane, status) {
    const strip = this.getChannelStrip(lane);
    const badge = strip.locator('.channel-status');
    await expect(badge).toHaveText(status);
  }

  /**
   * Toggle the waveform display checkbox.
   */
  async toggleWaveform() {
    await this.page.locator(SELECTORS.waveformEnabledToggle).click({ force: true });
  }

  /**
   * Toggle the spectrum display checkbox.
   */
  async toggleSpectrum() {
    await this.page.locator(SELECTORS.spectrumEnabledToggle).click({ force: true });
  }

  /**
   * Toggle the VU meter display checkbox.
   */
  async toggleVuMeters() {
    await this.page.locator(SELECTORS.vuMeterEnabledToggle).click({ force: true });
  }

  /**
   * Select an FFT window type from the dropdown.
   * @param {string} type — option value (e.g. 'hann', 'hamming', 'blackman', 'flattop', 'rect')
   */
  async setFftWindow(type) {
    await this.page.locator('#fftWindowSelect').selectOption(type);
  }
}

module.exports = AudioInputsPage;
