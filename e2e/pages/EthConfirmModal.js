const { expect } = require('@playwright/test');
const BasePage = require('./BasePage');
const SELECTORS = require('../helpers/selectors');

class EthConfirmModal extends BasePage {
  constructor(page) {
    super(page);
  }

  /** Assert the Ethernet confirmation modal is visible. */
  async expectVisible() {
    await expect(this.page.locator(SELECTORS.ethConfirmModal)).toBeVisible();
  }

  /** Assert the Ethernet confirmation modal is hidden. */
  async expectHidden() {
    await expect(this.page.locator(SELECTORS.ethConfirmModal)).toBeHidden();
  }

  /**
   * Assert the countdown timer shows a specific value.
   * @param {number} seconds — expected countdown number
   */
  async expectCountdown(seconds) {
    await expect(this.page.locator(SELECTORS.ethConfirmCountdown)).toHaveText(String(seconds));
  }

  /** Click the Confirm Configuration button. */
  async confirm() {
    const modal = this.page.locator(SELECTORS.ethConfirmModal);
    await modal.locator('button', { hasText: 'Confirm Configuration' }).click();
  }

  /** Click the Revert to DHCP (cancel) button. */
  async cancel() {
    const modal = this.page.locator(SELECTORS.ethConfirmModal);
    await modal.locator('button', { hasText: 'Revert to DHCP' }).click();
  }
}

module.exports = EthConfirmModal;
