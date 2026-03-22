const { expect } = require('@playwright/test');
const BasePage = require('./BasePage');
const SELECTORS = require('../helpers/selectors');

class SettingsPage extends BasePage {
  constructor(page) {
    super(page);
  }

  async open() {
    await this.page.evaluate(() => switchTab('settings'));
    await this.page.locator(SELECTORS.panel('settings')).waitFor({ state: 'visible' });
  }

  async toggleDarkMode() {
    const toggle = this.page.locator(SELECTORS.darkModeToggle);
    await toggle.click({ force: true });
  }

  async toggleDebugMode() {
    const toggle = this.page.locator(SELECTORS.debugModeToggle);
    await toggle.click({ force: true });
  }

  async setBrightness(level) {
    await this.page.locator(SELECTORS.brightnessSelect).selectOption(String(level));
  }

  async setDimTimeout(seconds) {
    await this.page.locator('#dimTimeoutSelect').selectOption(String(seconds));
  }

  async setScreenTimeout(seconds) {
    await this.page.locator(SELECTORS.screenTimeoutSelect).selectOption(String(seconds));
  }

  async toggleBuzzer() {
    const toggle = this.page.locator(SELECTORS.buzzerToggle);
    await toggle.click({ force: true });
  }

  async setBuzzerVolume(vol) {
    await this.page.locator(SELECTORS.buzzerVolumeSelect).selectOption(String(vol));
  }

  async toggleBacklight() {
    const toggle = this.page.locator(SELECTORS.backlightToggle);
    await toggle.click({ force: true });
  }

  async toggleDim() {
    const toggle = this.page.locator(SELECTORS.dimToggle);
    await toggle.click({ force: true });
  }

  async openPasswordModal() {
    await this.page.locator(SELECTORS.changePasswordBtn).click();
  }

  async expectDarkMode(enabled) {
    const toggle = this.page.locator(SELECTORS.darkModeToggle);
    if (enabled) {
      await expect(toggle).toBeChecked();
    } else {
      await expect(toggle).not.toBeChecked();
    }
  }

  async expectBuzzerEnabled(enabled) {
    const toggle = this.page.locator(SELECTORS.buzzerToggle);
    if (enabled) {
      await expect(toggle).toBeChecked();
    } else {
      await expect(toggle).not.toBeChecked();
    }
  }

  async expectBuzzerFieldsVisible(visible) {
    const locator = this.page.locator(SELECTORS.buzzerFields);
    if (visible) {
      await expect(locator).toBeVisible();
    } else {
      await expect(locator).toBeHidden();
    }
  }

  async expectCurrentVersion(version) {
    await expect(this.page.locator(SELECTORS.currentVersion)).toHaveText(version);
  }

  async expectLatestVersion(version) {
    await expect(this.page.locator(SELECTORS.latestVersion)).toHaveText(version);
  }

  async expectPasswordModalVisible(visible) {
    const modal = this.page.locator(SELECTORS.passwordChangeModal);
    if (visible) {
      await expect(modal).toBeVisible();
    } else {
      await expect(modal).toBeHidden();
    }
  }
}

module.exports = SettingsPage;
