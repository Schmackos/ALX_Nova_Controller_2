const { expect } = require('@playwright/test');
const BasePage = require('./BasePage');

class ApConfigModal extends BasePage {
  constructor(page) {
    super(page);
  }

  /**
   * Open the AP config modal by clicking the Configure AP button.
   */
  async open() {
    await this.page.locator('button', { hasText: 'Configure AP' }).click();
    await this.page.locator('#apConfigModal').waitFor({ state: 'visible' });
  }

  /** Assert the AP config modal is visible. */
  async expectVisible() {
    await expect(this.page.locator('#apConfigModal')).toBeVisible();
  }

  /**
   * Set the AP SSID input value.
   * @param {string} ssid
   */
  async setApSsid(ssid) {
    // The input has id="appState.apSSID" — need to escape the dot for CSS
    await this.page.locator('#appState\\.apSSID').fill(ssid);
  }

  /**
   * Set the AP password input value.
   * @param {string} pwd
   */
  async setApPassword(pwd) {
    await this.page.locator('#appState\\.apPassword').fill(pwd);
  }

  /** Submit the AP config form by clicking Save AP Settings. */
  async submit() {
    const modal = this.page.locator('#apConfigModal');
    await modal.locator('button[type="submit"]', { hasText: 'Save AP Settings' }).click();
  }

  /** Cancel and close the AP config modal. */
  async cancel() {
    const modal = this.page.locator('#apConfigModal');
    await modal.locator('button', { hasText: 'Cancel' }).click();
  }
}

module.exports = ApConfigModal;
