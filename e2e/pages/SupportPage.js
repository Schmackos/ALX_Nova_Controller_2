const { expect } = require('@playwright/test');
const BasePage = require('./BasePage');
const SELECTORS = require('../helpers/selectors');

class SupportPage extends BasePage {
  constructor(page) {
    super(page);
  }

  async open() {
    await this.page.evaluate(() => switchTab('support'));
    await this.page.locator(SELECTORS.panel('support')).waitFor({ state: 'visible' });
  }

  async getFirmwareVersion() {
    return await this.page.locator(SELECTORS.currentVersion).textContent();
  }

  async expectManualLink() {
    const link = this.page.locator(SELECTORS.manualLink);
    await expect(link).toBeVisible();
    await expect(link).toHaveAttribute('href', /.+/);
  }

  async expectManualQrCode() {
    await expect(this.page.locator(SELECTORS.manualQrCode)).toBeVisible();
  }

  async expectManualRendered() {
    await expect(this.page.locator(SELECTORS.manualRendered)).toBeVisible();
  }

  async searchManual(query) {
    await this.page.locator(SELECTORS.manualSearchInput).fill(query);
  }
}

module.exports = SupportPage;
