const { expect } = require('@playwright/test');
const BasePage = require('./BasePage');
const SELECTORS = require('../helpers/selectors');

class PasswordModal extends BasePage {
  constructor(page) {
    super(page);
  }

  /**
   * Open the password change modal by clicking the Change Password button
   * in the Settings tab.
   */
  async open() {
    await this.page.locator(SELECTORS.changePasswordBtn).click();
    await this.page.locator(SELECTORS.passwordChangeModal).waitFor({ state: 'visible' });
  }

  /**
   * Fill the new password input field.
   * @param {string} pwd
   */
  async fillNewPassword(pwd) {
    await this.page.locator(SELECTORS.newPasswordInput).fill(pwd);
  }

  /**
   * Fill the confirm password input field.
   * @param {string} pwd
   */
  async fillConfirmPassword(pwd) {
    await this.page.locator(SELECTORS.confirmPasswordInput).fill(pwd);
  }

  /**
   * Submit the password change form by clicking the submit button.
   */
  async submit() {
    const modal = this.page.locator(SELECTORS.passwordChangeModal);
    await modal.locator('button[type="submit"]').click();
  }

  /**
   * Cancel the password change by clicking the Cancel button.
   */
  async cancel() {
    const modal = this.page.locator(SELECTORS.passwordChangeModal);
    await modal.locator('button', { hasText: 'Cancel' }).click();
  }

  /** Assert the password change modal is visible. */
  async expectVisible() {
    await expect(this.page.locator(SELECTORS.passwordChangeModal)).toBeVisible();
  }

  /** Assert the password change modal is hidden / not in DOM. */
  async expectHidden() {
    await expect(this.page.locator(SELECTORS.passwordChangeModal)).toHaveCount(0);
  }

  /**
   * Assert that an error message is displayed in the modal.
   * @param {string} message — expected error text (substring match)
   */
  async expectError(message) {
    const errorEl = this.page.locator(SELECTORS.passwordError);
    await expect(errorEl).toBeVisible();
    await expect(errorEl).toContainText(message);
  }

  /** Assert that the modal was closed (indicating success). */
  async expectSuccess() {
    // On success the modal is removed from the DOM and a toast appears
    await expect(this.page.locator(SELECTORS.passwordChangeModal)).toHaveCount(0);
  }
}

module.exports = PasswordModal;
