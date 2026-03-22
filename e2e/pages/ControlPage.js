const { expect } = require('@playwright/test');
const BasePage = require('./BasePage');
const SELECTORS = require('../helpers/selectors');

class ControlPage extends BasePage {
  constructor(page) {
    super(page);
  }

  async open() {
    await this.page.evaluate(() => switchTab('control'));
    await this.page.locator(SELECTORS.panel('control')).waitFor({ state: 'visible' });
  }

  async setSensingMode(mode) {
    const radio = this.page.locator(SELECTORS.sensingModeRadio(mode));
    await radio.check({ force: true });
  }

  async toggleAmplifier() {
    await this.page.locator(SELECTORS.manualOnBtn).click();
  }

  async turnAmplifierOn() {
    await this.page.locator(SELECTORS.manualOnBtn).click();
  }

  async turnAmplifierOff() {
    await this.page.locator(SELECTORS.manualOffBtn).click();
  }

  async setTimerDuration(minutes) {
    const input = this.page.locator(SELECTORS.timerDurationInput);
    await input.fill(String(minutes));
    await input.dispatchEvent('change');
  }

  async setAudioThreshold(db) {
    const input = this.page.locator(SELECTORS.audioThresholdInput);
    await input.fill(String(db));
    await input.dispatchEvent('change');
  }

  async expectAmplifierState(on) {
    const expected = on ? 'ON' : 'OFF';
    await expect(this.page.locator(SELECTORS.amplifierStatus)).toHaveText(expected);
  }

  async expectSensingMode(mode) {
    const radio = this.page.locator(SELECTORS.sensingModeRadio(mode));
    await expect(radio).toBeChecked();
  }

  async expectSignalDetected(text) {
    await expect(this.page.locator(SELECTORS.signalDetected)).toHaveText(text);
  }

  async expectAudioLevel(text) {
    await expect(this.page.locator(SELECTORS.audioLevel)).toHaveText(text);
  }

  async expectTimerVisible(visible) {
    const locator = this.page.locator(SELECTORS.timerDisplay);
    if (visible) {
      await expect(locator).toBeVisible();
    } else {
      await expect(locator).toBeHidden();
    }
  }

  async expectSmartAutoSettingsVisible(visible) {
    const locator = this.page.locator(SELECTORS.smartAutoSettingsCard);
    if (visible) {
      await expect(locator).toBeVisible();
    } else {
      await expect(locator).toBeHidden();
    }
  }
}

module.exports = ControlPage;
