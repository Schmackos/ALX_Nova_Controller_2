const { expect } = require('@playwright/test');
const BasePage = require('./BasePage');

class AudioOutputsPage extends BasePage {
  constructor(page) {
    super(page);
  }

  /**
   * Returns a locator for the output channel strip at the given sink index.
   * @param {number} index
   * @returns {import('@playwright/test').Locator}
   */
  getOutputStrip(index) {
    return this.page.locator(`.channel-strip-output[data-sink="${index}"]`);
  }

  /**
   * Set the output gain slider for a sink index.
   * @param {number} index
   * @param {number} db — gain value in dB (-72 to +12)
   */
  async setOutputGain(index, db) {
    const slider = this.page.locator(`#outputGain${index}`);
    await slider.fill(String(db));
    await slider.dispatchEvent('input');
  }

  /**
   * Set the hardware volume slider for a sink index (only available on devices with HAL_CAP_HW_VOLUME).
   * @param {number} index
   * @param {number} vol — volume percentage (0-100)
   */
  async setHwVolume(index, vol) {
    const slider = this.page.locator(`#outputHwVol${index}`);
    await slider.fill(String(vol));
    await slider.dispatchEvent('input');
  }

  /**
   * Click the mute button for an output sink.
   * @param {number} index
   */
  async toggleOutputMute(index) {
    await this.page.locator(`#outputMute${index}`).click();
  }

  /**
   * Click the phase invert button for an output sink.
   * @param {number} index
   */
  async toggleOutputPhase(index) {
    await this.page.locator(`#outputPhase${index}`).click();
  }

  /**
   * Set the delay value for an output channel.
   * The delay input is keyed by firstChannel, not sink index.
   * @param {number} firstChannel — the firstChannel value of the output
   * @param {number} ms — delay in milliseconds
   */
  async setOutputDelay(firstChannel, ms) {
    const input = this.page.locator(`#outputDelay${firstChannel}`);
    await input.fill(String(ms));
    await input.dispatchEvent('change');
  }

  /**
   * Assert the displayed gain value for an output sink.
   * @param {number} index
   * @param {string|number} db
   */
  async expectOutputGain(index, db) {
    const label = this.page.locator(`#outputGainVal${index}`);
    await expect(label).toHaveText(`${parseFloat(db).toFixed(1)} dB`);
  }

  /**
   * Open the output DSP PEQ overlay for a given output channel.
   * Invokes the frontend openOutputPeq() function with the firstChannel value.
   * @param {number} firstChannel — the firstChannel value of the output
   */
  async openOutputDsp(firstChannel) {
    await this.page.evaluate((ch) => openOutputPeq(ch), firstChannel);
    // Wait for overlay to appear
    await this.page.locator('#peqOverlay').waitFor({ state: 'visible' });
  }
}

module.exports = AudioOutputsPage;
