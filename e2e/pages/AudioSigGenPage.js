const { expect } = require('@playwright/test');
const BasePage = require('./BasePage');
const SELECTORS = require('../helpers/selectors');

class AudioSigGenPage extends BasePage {
  constructor(page) {
    super(page);
  }

  /**
   * Navigate to the signal generator sub-view within the Audio tab.
   */
  async open() {
    await this.page.evaluate(() => switchTab('audio'));
    await this.page.locator(SELECTORS.audioPanel).waitFor({ state: 'visible' });
    await this.page.locator(SELECTORS.audioSubNavBtn('siggen')).click();
    await this.page.waitForTimeout(200);
  }

  /**
   * Toggle the signal generator enabled checkbox.
   */
  async toggleEnabled() {
    await this.page.locator(SELECTORS.siggenEnable).click({ force: true });
  }

  /**
   * Select a waveform type from the dropdown.
   * @param {'sine'|'square'|'noise'|'sweep'|string} type — waveform name or numeric value
   */
  async setWaveform(type) {
    const valueMap = { sine: '0', square: '1', noise: '2', sweep: '3' };
    const value = valueMap[type] || String(type);
    await this.page.locator(SELECTORS.siggenWaveform).selectOption(value);
  }

  /**
   * Set the frequency slider value.
   * @param {number} hz
   */
  async setFrequency(hz) {
    const slider = this.page.locator(SELECTORS.siggenFreq);
    await slider.fill(String(hz));
    await slider.dispatchEvent('input');
    await slider.dispatchEvent('change');
  }

  /**
   * Set the amplitude slider value.
   * @param {number} dbfs — amplitude in dBFS (-96 to 0)
   */
  async setAmplitude(dbfs) {
    const slider = this.page.locator(SELECTORS.siggenAmp);
    await slider.fill(String(dbfs));
    await slider.dispatchEvent('input');
    await slider.dispatchEvent('change');
  }

  /**
   * Select the output mode.
   * @param {'software'|'pwm'|string} mode — mode name or numeric value
   */
  async setMode(mode) {
    const valueMap = { software: '0', pwm: '1' };
    const value = valueMap[mode] || String(mode);
    await this.page.locator(SELECTORS.siggenOutputMode).selectOption(value);
  }

  /**
   * Select the target ADC from the dropdown.
   * @param {number} index — ADC index value
   */
  async setTargetAdc(index) {
    await this.page.locator(SELECTORS.siggenTargetAdc).selectOption(String(index));
  }

  /**
   * Click a signal generator preset button.
   * @param {'440Hz'|'1kHz'|'100Hz'|'10kHz'|'Noise'|'Sweep'} name
   */
  async applyPreset(name) {
    const siggenView = this.page.locator(SELECTORS.audioSubView('siggen'));
    const presetMap = {
      '440Hz': '440 Hz',
      '1kHz': '1 kHz',
      '100Hz': '100 Hz',
      '10kHz': '10 kHz',
      'Noise': 'Noise',
      'Sweep': 'Sweep',
    };
    const label = presetMap[name] || name;
    await siggenView.locator('.btn-outline', { hasText: label }).click();
  }

  /**
   * Assert whether the signal generator is enabled.
   * @param {boolean} enabled
   */
  async expectEnabled(enabled) {
    const checkbox = this.page.locator(SELECTORS.siggenEnable);
    if (enabled) {
      await expect(checkbox).toBeChecked();
    } else {
      await expect(checkbox).not.toBeChecked();
    }
  }

  /**
   * Assert the displayed frequency value.
   * @param {number} hz
   */
  async expectFrequency(hz) {
    await expect(this.page.locator(SELECTORS.siggenFreqVal)).toHaveText(String(hz));
  }
}

module.exports = AudioSigGenPage;
