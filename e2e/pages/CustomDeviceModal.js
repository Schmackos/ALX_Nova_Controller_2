const { expect } = require('@playwright/test');
const BasePage = require('./BasePage');

class CustomDeviceModal extends BasePage {
  constructor(page) {
    super(page);
  }

  /**
   * Open the Create Custom Device modal by clicking the Create Device button
   * on the Devices tab.
   */
  async open() {
    await this.page.locator('button', { hasText: 'Create Device' }).click();
    await this.page.locator('#halCustomCreateModal').waitFor({ state: 'visible' });
  }

  /** Assert the custom device modal is visible. */
  async expectVisible() {
    await expect(this.page.locator('#halCustomCreateModal')).toBeVisible();
  }

  /**
   * Set the device name in the create form.
   * @param {string} name
   */
  async setDeviceName(name) {
    await this.page.locator('#halCcName').fill(name);
  }

  /**
   * Read the auto-generated compatible string displayed in the modal.
   * @returns {Promise<string>}
   */
  async getCompatible() {
    return this.page.locator('#halCcCompatDisplay').textContent();
  }

  /**
   * Set the device type via the dropdown.
   * @param {string} value — '1' (DAC), '2' (ADC), '3' (Codec), '4' (Amp), '9' (GPIO)
   */
  async setDeviceType(value) {
    await this.page.locator('#halCcType').selectOption(value);
  }

  /**
   * Set the bus type via the dropdown.
   * @param {string} value — '1' (I2C), '2' (I2S), '4' (GPIO)
   */
  async setBusType(value) {
    await this.page.locator('#halCcBus').selectOption(value);
  }

  /**
   * Set the I2C address in the create form.
   * @param {string} addr — e.g. '0x48'
   */
  async setI2cAddress(addr) {
    await this.page.locator('#halCcI2cAddr').fill(addr);
  }

  /**
   * Set the I2C bus index.
   * @param {string} value — '0' (External), '1' (Onboard), '2' (Expansion)
   */
  async setI2cBus(value) {
    await this.page.locator('#halCcI2cBus').selectOption(value);
  }

  /**
   * Set the I2S port.
   * @param {string} value — '0', '1', or '2'
   */
  async setI2sPort(value) {
    await this.page.locator('#halCcI2sPort').selectOption(value);
  }

  /**
   * Set the channel count.
   * @param {string} value — '2', '4', or '8'
   */
  async setChannels(value) {
    await this.page.locator('#halCcChannels').selectOption(value);
  }

  /** Submit the custom device create form (Create & Test button). */
  async submit() {
    const modal = this.page.locator('#halCustomCreateModal');
    await modal.locator('button', { hasText: 'Create & Test' }).click();
  }

  /** Cancel and close the custom device modal. */
  async cancel() {
    const modal = this.page.locator('#halCustomCreateModal');
    await modal.locator('.hal-cc-close').click();
  }

  /**
   * Assert an error is displayed. Custom device errors typically appear as toasts.
   * @param {string} msg — expected error text (substring match)
   */
  async expectError(msg) {
    await this.expectToast(msg);
  }
}

module.exports = CustomDeviceModal;
