const { expect } = require('@playwright/test');
const BasePage = require('./BasePage');
const SELECTORS = require('../helpers/selectors');

class AudioPage extends BasePage {
  constructor(page) {
    super(page);
  }

  /**
   * Navigate to the Audio tab.
   */
  async open() {
    await this.page.evaluate(() => switchTab('audio'));
    await this.page.locator(SELECTORS.audioPanel).waitFor({ state: 'visible' });
  }

  /**
   * Switch to a sub-view within the Audio tab.
   * @param {'inputs'|'matrix'|'outputs'|'siggen'} name
   */
  async switchSubView(name) {
    await this.page.locator(SELECTORS.audioSubNavBtn(name)).click();
    await this.page.waitForTimeout(200);
  }

  /**
   * Assert that a given sub-view is currently active.
   * @param {'inputs'|'matrix'|'outputs'|'siggen'} name
   */
  async expectSubViewActive(name) {
    const btn = this.page.locator(SELECTORS.audioSubNavBtn(name));
    await expect(btn).toHaveClass(/active/);
    const panel = this.page.locator(SELECTORS.audioSubView(name));
    await expect(panel).toHaveClass(/active/);
  }

  /**
   * @returns {AudioInputsPage}
   */
  getInputsPage() {
    const AudioInputsPage = require('./AudioInputsPage');
    return new AudioInputsPage(this.page);
  }

  /**
   * @returns {AudioOutputsPage}
   */
  getOutputsPage() {
    const AudioOutputsPage = require('./AudioOutputsPage');
    return new AudioOutputsPage(this.page);
  }

  /**
   * @returns {AudioMatrixPage}
   */
  getMatrixPage() {
    const AudioMatrixPage = require('./AudioMatrixPage');
    return new AudioMatrixPage(this.page);
  }

  /**
   * @returns {AudioSigGenPage}
   */
  getSigGenPage() {
    const AudioSigGenPage = require('./AudioSigGenPage');
    return new AudioSigGenPage(this.page);
  }
}

module.exports = AudioPage;
